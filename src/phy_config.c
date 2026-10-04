#include "phy_config.h"

uint16_t dp83867_cfg4_without_mirror(uint16_t value) {
    return (uint16_t)(value & ~DP83867_CFG4_MIRROR_ENABLE);
}

uint16_t dp83867_rgmii_control_with_delays(uint16_t value) {
    return (uint16_t)(value | DP83867_RGMII_DELAY_ENABLE);
}

uint16_t dp83867_rgmii_delay_2ns(uint16_t value) {
    return (uint16_t)((value & 0xff00u) | DP83867_RGMII_DELAY_2NS);
}

uint16_t dp83tg720_rgmii_control_without_delays(uint16_t value) {
    return (uint16_t)(value & ~DP83TG720_RGMII_DELAY_ENABLE);
}
