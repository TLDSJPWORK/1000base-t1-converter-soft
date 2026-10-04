#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/structs/sio.h"

#define MDIO_PIN 4u
#define MDC_PIN 5u
#define MDIO_MASK (1u << MDIO_PIN)
#define MDC_MASK  (1u << MDC_PIN)

static bool slow_diagnostic_mode;

// Slow mode is intentionally below the DP83TG720 MDC specification. It is an
// electrical/debug aid that can work with the RP2040's weak pull-up and is easy
// to inspect. Fast mode is in the low-single-digit MHz range at 125 MHz sys_clk;
// confirm its exact frequency with an oscilloscope when one is available.
static inline void mdio_delay(void) {
    if (slow_diagnostic_mode) sleep_us(10); // approximately 50 kHz MDC
    else busy_wait_at_least_cycles(12);     // estimated low-single-digit MHz
}
static inline void mdio_drive(bool v) {
    // Set the output latch before enabling the driver to avoid a glitch.
    if (v) sio_hw->gpio_set = MDIO_MASK;
    else   sio_hw->gpio_clr = MDIO_MASK;
    sio_hw->gpio_oe_set = MDIO_MASK;
}
static inline void mdio_release(void) { sio_hw->gpio_oe_clr = MDIO_MASK; }

static void mdio_write_bit(bool v) {
    sio_hw->gpio_clr = MDC_MASK;
    mdio_drive(v);
    mdio_delay();                 // MDIO setup time before rising MDC
    sio_hw->gpio_set = MDC_MASK;
    mdio_delay();                 // Hold through the high phase
}
static bool mdio_read_bit(void) {
    sio_hw->gpio_clr = MDC_MASK;
    mdio_release();
    mdio_delay();                 // Release MDIO for the complete setup interval
    sio_hw->gpio_set = MDC_MASK;
    mdio_delay();
    return (sio_hw->gpio_in & MDIO_MASK) != 0; // End of high phase
}
static void mdio_write_bits(uint32_t value, unsigned count) {
    for (int bit = (int)count - 1; bit >= 0; --bit)
        mdio_write_bit((value >> bit) & 1u);
}
static void mdio_preamble(void) { mdio_write_bits(0xffffffffu, 32); }

typedef struct {
    uint16_t data;
    bool ta_zero_low;
} mdio_read_result_t;

static mdio_read_result_t mdio_read_c22(uint8_t phy, uint8_t reg) {
    mdio_preamble();
    mdio_write_bits(0x1u, 2); // ST=01
    mdio_write_bits(0x2u, 2); // OP=10 read
    mdio_write_bits(phy & 0x1fu, 5);
    mdio_write_bits(reg & 0x1fu, 5);
    // Release MDIO before the read turnaround. On this bit-banged interface the
    // following sampled clock is the PHY-driven turnaround zero; adding a
    // separate sampled "Z" clock shifts the returned data left by one bit.
    bool ta_zero = mdio_read_bit();
    uint16_t value = 0;
    for (unsigned i = 0; i < 16; ++i)
        value = (uint16_t)((value << 1) | mdio_read_bit());
    sio_hw->gpio_clr = MDC_MASK;
    mdio_delay();
    mdio_release();
    mdio_read_result_t result = {
        .data = value,
        .ta_zero_low = !ta_zero,
    };
    return result;
}

static void print_expected_phy(uint8_t phy, const char *name) {
    printf("\nExpected PHY: %s at address %u\n", name, phy);
    puts("reg  name    TA-0  data");
    static const char *const names[] = {"BMCR ", "BMSR ", "PHYID1", "PHYID2"};
    for (uint8_t reg = 0; reg <= 3; ++reg) {
        mdio_read_result_t r = mdio_read_c22(phy, reg);
        printf(" %u   %-6s  %s   %04x\n", reg, names[reg],
               r.ta_zero_low ? "LOW " : "HIGH", r.data);
    }
}

static void diagnostic_pass(bool slow) {
    slow_diagnostic_mode = slow;
    puts("\n============================================================");
    puts("MDIO Clause-22 diagnostic (GPIO4=MDIO, GPIO5=MDC)");
    if (slow)
        puts("MDC mode: SLOW ~50 kHz (diagnostic only; below PHY specification)");
    else
        puts("MDC mode: FAST low-single-digit MHz estimate (verify by scope)");
    printf("MDIO_idle=%u (weak RP2040 pull-up enabled)\n", gpio_get(MDIO_PIN));

    print_expected_phy(0, "DP83867");
    print_expected_phy(8, "DP83TG720");

    unsigned ta_responders = 0;
    puts("\nAll-address TA scan using register 2 (PHYID1):");
    puts("addr  TA-0  data");
    for (uint8_t phy = 0; phy < 32; ++phy) {
        mdio_read_result_t r = mdio_read_c22(phy, 2);
        if (r.ta_zero_low) {
            ++ta_responders;
            printf("%2u    LOW   %04x\n", phy, r.data);
        }
    }
    if (!ta_responders)
        puts("No address drove TA-0 low. Raw reads for addresses 0 and 8 are shown above.");
    else
        printf("Addresses that drove TA-0 low: %u (expected addresses: 0 and 8)\n",
               ta_responders);
}

int main(void) {
    stdio_init_all();
    gpio_init(MDC_PIN);
    gpio_set_dir(MDC_PIN, GPIO_OUT);
    gpio_put(MDC_PIN, false);
    gpio_init(MDIO_PIN);
    // Diagnostic aid only. A 2.2k external pull-up is still required by the
    // PHY datasheet; the RP2040 pull-up is not a compliant replacement.
    gpio_pull_up(MDIO_PIN);
    mdio_release();
    sleep_ms(3000);
    while (true) {
        diagnostic_pass(true);
        diagnostic_pass(false);
        puts("\nNext pair of diagnostic passes in 5 seconds...");
        sleep_ms(5000);
    }
}
