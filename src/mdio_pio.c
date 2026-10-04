#include "mdio.h"
#include "mdio_frame.h"
#include "board_config.h"
#include "mdio.pio.h"
#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "hardware/pio.h"
#include "pico/time.h"
static PIO const pio = pio0;
static int sm = -1;
static unsigned offset;
static pio_sm_config config;
static void stop(void) {
    pio_sm_set_enabled(pio, (unsigned)sm, false);
    pio_sm_set_pins_with_mask(pio, (unsigned)sm, 0, 1u << BOARD_MDC_PIN);
    pio_sm_set_pindirs_with_mask(pio, (unsigned)sm, 0, 1u << BOARD_MDIO_PIN);
}
mdio_result mdio_init(void) {
    if (sm >= 0) return MDIO_OK;
    if (!pio_can_add_program(pio, &converter_mdio_program)) return MDIO_TIMEOUT;
    sm = pio_claim_unused_sm(pio, false);
    if (sm < 0) return MDIO_TIMEOUT;
    offset = pio_add_program(pio, &converter_mdio_program);
    config = converter_mdio_program_get_default_config(offset);
    sm_config_set_out_pins(&config, BOARD_MDIO_PIN, 1);
    sm_config_set_in_pins(&config, BOARD_MDIO_PIN);
    sm_config_set_sideset_pins(&config, BOARD_MDC_PIN);
    sm_config_set_out_shift(&config, true, true, 32);
    sm_config_set_in_shift(&config, false, true, 32);
    sm_config_set_clkdiv(&config, (float)clock_get_hz(clk_sys) / (8u * BOARD_MDC_HZ));
    gpio_init(BOARD_MDIO_PIN);
    gpio_pull_up(BOARD_MDIO_PIN);
    gpio_init(BOARD_MDC_PIN);
    gpio_disable_pulls(BOARD_MDC_PIN);
    pio_sm_init(pio, (unsigned)sm, offset, &config);
    pio_sm_set_pins_with_mask(pio, (unsigned)sm, 0, (1u << BOARD_MDIO_PIN) | (1u << BOARD_MDC_PIN));
    pio_sm_set_consecutive_pindirs(pio, (unsigned)sm, BOARD_MDIO_PIN, 1, false);
    pio_sm_set_consecutive_pindirs(pio, (unsigned)sm, BOARD_MDC_PIN, 1, true);
    pio_gpio_init(pio, BOARD_MDIO_PIN);
    pio_gpio_init(pio, BOARD_MDC_PIN);
    busy_wait_us_32(BOARD_MDIO_IDLE_US);
    return MDIO_OK;
}
static mdio_result transfer(bool read, unsigned phy, unsigned reg, uint16_t *value) {
    uint32_t tx[4];
    if (!value || !mdio_frame_encode(tx, read, phy, reg, *value)) return MDIO_BAD_ARG;
    if (sm < 0) return MDIO_TIMEOUT;
    /* Allow the weak internal pull-up to settle between diagnostic frames. */
    if (!gpio_get(BOARD_MDIO_PIN)) return MDIO_BUS_LOW;
    pio_sm_init(pio, (unsigned)sm, offset, &config);
    for (unsigned i = 0; i < 4; ++i) pio_sm_put(pio, (unsigned)sm, tx[i]);
    const uint32_t stalled = 1u << (PIO_FDEBUG_TXSTALL_LSB + (unsigned)sm);
    pio->fdebug = stalled;
    pio_sm_set_enabled(pio, (unsigned)sm, true);
    absolute_time_t until = make_timeout_time_ms(10);
    unsigned received = 0;
    uint32_t tail = 0;
    while (received < 2 || !(pio->fdebug & stalled)) {
        if (!pio_sm_is_rx_fifo_empty(pio, (unsigned)sm)) {
            tail = pio_sm_get(pio, (unsigned)sm);
            ++received;
        }
        if (time_reached(until)) { stop(); return MDIO_TIMEOUT; }
    }
    /* TXSTALL at the next OUT means the final high phase has completed. */
    stop();
    busy_wait_us_32(BOARD_MDIO_IDLE_US);
    if (read && !mdio_frame_decode(tail, value)) return MDIO_NO_ACK;
    return MDIO_OK;
}
mdio_result mdio_read(unsigned phy, unsigned reg, uint16_t *value) {
    uint16_t data = 0;
    if (!value) return MDIO_BAD_ARG;
    mdio_result r = transfer(true, phy, reg, &data);
    if (r == MDIO_OK) *value = data;
    return r;
}
mdio_result mdio_write(unsigned phy, unsigned reg, uint16_t value) {
    return transfer(false, phy, reg, &value);
}
