#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/stdio_usb.h"
#include "board_config.h"
#include "mdio.h"
#include "phy_config.h"
static uint32_t detected;
static bool watch;
static int last_link[32];
static mdio_result boot_role_result = MDIO_OK;
static uint16_t boot_role_value;
static mdio_result media_config_result = MDIO_OK;
static uint16_t copper_cfg4_value;
static uint16_t copper_rgmii_control_value;
static uint16_t copper_rgmii_delay_value;
static uint16_t t1_rgmii_control_value;

#define T1_PHY_ADDRESS 8u
#define T1_PMA_DEVICE 1u
#define T1_PMA_CONTROL 0x0834u
#define T1_MASTER_BIT 0x4000u
#define COPPER_PHY_ADDRESS 0u
#define COPPER_VENDOR_DEVICE 31u
#define COPPER_CFG4 0x0031u
#define COPPER_RGMII_CONTROL 0x0032u
#define COPPER_RGMII_DELAY 0x0086u
#define T1_VENDOR_DEVICE 31u
#define T1_RGMII_DELAY_CONTROL 0x0602u
#define PHY_SOFT_RESTART_REGISTER 0x1fu
#define PHY_SOFT_RESTART 0x4000u

#if defined(CONVERTER_T1_ROLE_MASTER)
static const char default_role[] = "master";
#elif defined(CONVERTER_T1_ROLE_SLAVE)
static const char default_role[] = "slave";
#else
static const char default_role[] = "diagnostic (no automatic role change)";
#endif

static void report(mdio_result r) { printf("%s\n", mdio_error(r)); }
static void help(void) {
    printf("1000base-t1-converter-soft 0.3.0\n"
         "help | scan | status | watch on|off\n"
         "read PHY REG | write PHY REG VALUE\n"
         "mread PHY DEV REG | mwrite PHY DEV REG VALUE\n"
         "Numbers: decimal or 0xHEX; PHY/DEV/C22 REG 0..31; MMD REG/VALUE 0..65535.\n"
         "Writes are explicit and volatile. C22 write has no acknowledgement.\n"
         "MMD reads also write the address selector registers 13/14.\n"
         "T1 boot role: %s. No GPIO reset. MDIO: 50 kHz, internal pull-up (diagnostic).\n",
           default_role);
}

static mdio_result apply_media_converter_config(void) {
    uint16_t current;
    mdio_result r = mdio_mmd_read(COPPER_PHY_ADDRESS, COPPER_VENDOR_DEVICE,
                                  COPPER_CFG4, &current);
    if (r != MDIO_OK) return r;

    uint16_t desired = dp83867_cfg4_without_mirror(current);
    if (desired != current) {
        r = mdio_mmd_write(COPPER_PHY_ADDRESS, COPPER_VENDOR_DEVICE,
                           COPPER_CFG4, desired);
        if (r != MDIO_OK) return r;
        r = mdio_write(COPPER_PHY_ADDRESS, PHY_SOFT_RESTART_REGISTER,
                       PHY_SOFT_RESTART);
        if (r != MDIO_OK) return r;
        sleep_ms(2);
    }
    r = mdio_mmd_read(COPPER_PHY_ADDRESS, COPPER_VENDOR_DEVICE,
                      COPPER_CFG4, &copper_cfg4_value);
    if (r != MDIO_OK) return r;
    if (copper_cfg4_value & DP83867_CFG4_MIRROR_ENABLE) return MDIO_NO_ACK;

    r = mdio_mmd_read(COPPER_PHY_ADDRESS, COPPER_VENDOR_DEVICE,
                      COPPER_RGMII_CONTROL, &current);
    if (r != MDIO_OK) return r;
    desired = dp83867_rgmii_control_with_delays(current);
    if (desired != current) {
        r = mdio_mmd_write(COPPER_PHY_ADDRESS, COPPER_VENDOR_DEVICE,
                           COPPER_RGMII_CONTROL, desired);
        if (r != MDIO_OK) return r;
    }
    r = mdio_mmd_read(COPPER_PHY_ADDRESS, COPPER_VENDOR_DEVICE,
                      COPPER_RGMII_CONTROL, &copper_rgmii_control_value);
    if (r != MDIO_OK) return r;
    if ((copper_rgmii_control_value & DP83867_RGMII_DELAY_ENABLE) !=
        DP83867_RGMII_DELAY_ENABLE) return MDIO_NO_ACK;

    r = mdio_mmd_read(COPPER_PHY_ADDRESS, COPPER_VENDOR_DEVICE,
                      COPPER_RGMII_DELAY, &current);
    if (r != MDIO_OK) return r;
    desired = dp83867_rgmii_delay_2ns(current);
    if (desired != current) {
        r = mdio_mmd_write(COPPER_PHY_ADDRESS, COPPER_VENDOR_DEVICE,
                           COPPER_RGMII_DELAY, desired);
        if (r != MDIO_OK) return r;
    }
    r = mdio_mmd_read(COPPER_PHY_ADDRESS, COPPER_VENDOR_DEVICE,
                      COPPER_RGMII_DELAY, &copper_rgmii_delay_value);
    if (r != MDIO_OK) return r;
    if ((copper_rgmii_delay_value & 0x00ffu) != DP83867_RGMII_DELAY_2NS)
        return MDIO_NO_ACK;

    r = mdio_mmd_read(T1_PHY_ADDRESS, T1_VENDOR_DEVICE,
                      T1_RGMII_DELAY_CONTROL, &current);
    if (r != MDIO_OK) return r;
    desired = dp83tg720_rgmii_control_without_delays(current);
    if (desired != current) {
        r = mdio_mmd_write(T1_PHY_ADDRESS, T1_VENDOR_DEVICE,
                           T1_RGMII_DELAY_CONTROL, desired);
        if (r != MDIO_OK) return r;
    }
    r = mdio_mmd_read(T1_PHY_ADDRESS, T1_VENDOR_DEVICE,
                      T1_RGMII_DELAY_CONTROL, &t1_rgmii_control_value);
    if (r != MDIO_OK) return r;
    return (t1_rgmii_control_value & DP83TG720_RGMII_DELAY_ENABLE) == 0
               ? MDIO_OK : MDIO_NO_ACK;
}

static mdio_result apply_boot_role(void) {
#if defined(CONVERTER_T1_ROLE_DIAGNOSTIC)
    return mdio_mmd_read(T1_PHY_ADDRESS, T1_PMA_DEVICE, T1_PMA_CONTROL,
                         &boot_role_value);
#else
    uint16_t current;
    mdio_result r = mdio_mmd_read(T1_PHY_ADDRESS, T1_PMA_DEVICE,
                                  T1_PMA_CONTROL, &current);
    if (r != MDIO_OK) return r;
#if defined(CONVERTER_T1_ROLE_MASTER)
    uint16_t desired = (uint16_t)(current | T1_MASTER_BIT);
#else
    uint16_t desired = (uint16_t)(current & ~T1_MASTER_BIT);
#endif
    if (desired != current) {
        r = mdio_mmd_write(T1_PHY_ADDRESS, T1_PMA_DEVICE,
                           T1_PMA_CONTROL, desired);
        if (r != MDIO_OK) return r;
        r = mdio_write(T1_PHY_ADDRESS, 0x1fu, 0x4000u);
        if (r != MDIO_OK) return r;
        sleep_ms(2);
    }
    r = mdio_mmd_read(T1_PHY_ADDRESS, T1_PMA_DEVICE,
                      T1_PMA_CONTROL, &boot_role_value);
    if (r != MDIO_OK) return r;
    return (boot_role_value & T1_MASTER_BIT) == (desired & T1_MASTER_BIT)
               ? MDIO_OK : MDIO_NO_ACK;
#endif
}
static bool number(const char *s, unsigned max, unsigned *out) {
    if (!s || !*s || *s == '-' || *s == '+') return false;
    errno = 0;
    char *end;
    int base = s[0] == '0' && (s[1] == 'x' || s[1] == 'X') ? 16 : 10;
    unsigned long v = strtoul(s, &end, base);
    if (errno || *end || end == s || v > max) return false;
    *out = (unsigned)v;
    return true;
}
static void scan(void) {
    detected = 0;
    for (unsigned p = 0; p < 32; ++p) {
        uint16_t id1, id2;
        last_link[p] = -1;
        mdio_result r = mdio_read(p, 2, &id1);
        if (r == MDIO_OK) r = mdio_read(p, 3, &id2);
        if (r == MDIO_BUS_LOW || r == MDIO_TIMEOUT) { report(r); break; }
        if (r != MDIO_OK) continue;
        uint32_t id = ((uint32_t)id1 << 16) | id2;
        if (!id || id == UINT32_MAX) {
            printf("PHY %u: invalid ID %08lx; check bus\n", p, (unsigned long)id);
            continue;
        }
        detected |= 1u << p;
        printf("PHY %u ID=%08lx\n", p, (unsigned long)id);
    }
    printf("Detected mask: %08lx\n", (unsigned long)detected);
}
static void status(bool changes_only) {
    if (!detected && !changes_only) { puts("No detected PHY. Run scan."); return; }
    for (unsigned p = 0; p < 32; ++p) {
        if (!(detected & (1u << p))) continue;
        uint16_t old_status, current, control;
        /* BMSR link status is latch-low: report the second read as current. */
        mdio_result r = mdio_read(p, 1, &old_status);
        if (r == MDIO_OK) r = mdio_read(p, 1, &current);
        if (r == MDIO_OK) r = mdio_read(p, 0, &control);
        if (r != MDIO_OK) {
            if (!changes_only || last_link[p] != -2) {
                printf("PHY %u: ", p); report(r);
            }
            last_link[p] = -2;
            continue;
        }
        int link = (current & 4u) != 0;
        if (!changes_only || last_link[p] != link)
            printf("PHY %u link=%s BMCR=%04x BMSR=%04x latched=%04x\n",
                   p, link ? "up" : "down", control, current, old_status);
        last_link[p] = link;
    }
}
static void command(char *line) {
    char *args[6]; unsigned n = 0;
    for (char *s = strtok(line, " \t"); s; s = strtok(NULL, " \t")) {
        if (n == 6) { puts("Too many arguments"); return; }
        args[n++] = s;
    }
    if (!n) return;
    if (n == 1 && !strcmp(args[0], "help")) { help(); return; }
    if (n == 1 && !strcmp(args[0], "scan")) { scan(); return; }
    if (n == 1 && !strcmp(args[0], "status")) { status(false); return; }
    if (n == 2 && !strcmp(args[0], "watch") &&
        (!strcmp(args[1], "on") || !strcmp(args[1], "off"))) {
        watch = !strcmp(args[1], "on"); puts(watch ? "Watch on (1 s)" : "Watch off"); return;
    }
    bool rd = !strcmp(args[0], "read"), wr = !strcmp(args[0], "write");
    bool mr = !strcmp(args[0], "mread"), mw = !strcmp(args[0], "mwrite");
    bool mmd = mr || mw;
    unsigned p, d = 0, reg, val = 0;
    unsigned count = (mmd ? 4u : 3u) + ((wr || mw) ? 1u : 0u);
    if (!(rd || wr || mr || mw) || n != count || !number(args[1], 31, &p) ||
        (mmd && !number(args[2], 31, &d)) ||
        !number(args[mmd ? 3 : 2], mmd ? 65535 : 31, &reg) ||
        ((wr || mw) && !number(args[n - 1], 65535, &val))) {
        puts("Invalid command/argument. Type help."); return;
    }
    uint16_t result = 0;
    mdio_result r;
    if (rd) r = mdio_read(p, reg, &result);
    else if (mr) r = mdio_mmd_read(p, d, reg, &result);
    else {
        /* Verify a target responds before sending a requested write. */
        r = mdio_read(p, 2, &result);
        if (r == MDIO_OK)
            r = mw ? mdio_mmd_write(p, d, reg, (uint16_t)val) : mdio_write(p, reg, (uint16_t)val);
    }
    if (r != MDIO_OK) report(r);
    else if (rd || mr) printf("0x%04x\n", result);
    else puts("Write frame sent; use a read to verify writable non-self-clearing bits.");
}
int main(void) {
    stdio_init_all();
    sleep_ms(BOARD_PHY_STARTUP_MS);
    mdio_result init = mdio_init();
    if (init == MDIO_OK) {
        boot_role_result = apply_boot_role();
        media_config_result = apply_media_converter_config();
    } else {
        boot_role_result = init;
        media_config_result = init;
    }
    char line[128]; size_t used = 0; bool overflow = false, connected = false;
    absolute_time_t next = make_timeout_time_ms(1000);
    for (;;) {
        bool now = stdio_usb_connected();
        if (now && !connected) {
            used = 0; overflow = false; help(); report(init);
            printf("Boot role result: %s, PMA 1:0834=0x%04x\n",
                   mdio_error(boot_role_result), boot_role_value);
            printf("Media config result: %s, CFG4=0x%04x, RGMIICTL=0x%04x, "
                   "RGMIIDCTL=0x%04x, T1_RGMIICTL=0x%04x\n",
                   mdio_error(media_config_result), copper_cfg4_value,
                   copper_rgmii_control_value, copper_rgmii_delay_value,
                   t1_rgmii_control_value);
            puts("> ");
        }
        if (!now && connected) { used = 0; overflow = false; watch = false; }
        connected = now;
        int ch = getchar_timeout_us(1000);
        if (ch >= 0) {
            if (ch == '\r' || ch == '\n') {
                if (overflow) puts("Line too long; discarded.");
                else if (used) { line[used] = 0; command(line); }
                used = 0; overflow = false;
            } else if (ch == '\b' || ch == 127) {
                if (!overflow && used) --used;
            } else if (isprint((unsigned char)ch) || ch == '\t') {
                if (used + 1 < sizeof(line) && !overflow) line[used++] = (char)ch;
                else overflow = true;
            }
        }
        if (watch && connected && time_reached(next)) {
            status(true); next = make_timeout_time_ms(1000);
        }
    }
}
