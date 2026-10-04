#ifndef PHY_CONFIG_H
#define PHY_CONFIG_H

#include <stdint.h>

#define DP83867_CFG4_MIRROR_ENABLE 0x0001u
#define DP83867_RGMII_DELAY_ENABLE 0x0003u
#define DP83867_RGMII_DELAY_2NS 0x0077u
#define DP83TG720_RGMII_DELAY_ENABLE 0x0003u

uint16_t dp83867_cfg4_without_mirror(uint16_t value);
uint16_t dp83867_rgmii_control_with_delays(uint16_t value);
uint16_t dp83867_rgmii_delay_2ns(uint16_t value);
uint16_t dp83tg720_rgmii_control_without_delays(uint16_t value);

#endif
