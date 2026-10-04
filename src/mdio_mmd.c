#include "mdio.h"
static mdio_result address(unsigned phy, unsigned dev, unsigned reg) {
    if (phy > 31 || dev > 31 || reg > 65535) return MDIO_BAD_ARG;
    mdio_result r = mdio_write(phy, 13, (uint16_t)dev);
    if (r != MDIO_OK) return r;
    r = mdio_write(phy, 14, (uint16_t)reg);
    if (r != MDIO_OK) return r;
    return mdio_write(phy, 13, (uint16_t)(0x4000u | dev));
}
mdio_result mdio_mmd_read(unsigned phy, unsigned dev, unsigned reg, uint16_t *value) {
    if (!value) return MDIO_BAD_ARG;
    mdio_result r = address(phy, dev, reg);
    return r == MDIO_OK ? mdio_read(phy, 14, value) : r;
}
mdio_result mdio_mmd_write(unsigned phy, unsigned dev, unsigned reg, uint16_t value) {
    mdio_result r = address(phy, dev, reg);
    return r == MDIO_OK ? mdio_write(phy, 14, value) : r;
}
const char *mdio_error(mdio_result r) {
    switch (r) {
    case MDIO_OK: return "OK";
    case MDIO_BAD_ARG: return "invalid address/argument";
    case MDIO_TIMEOUT: return "PIO unavailable or timed out";
    case MDIO_NO_ACK: return "no PHY turnaround acknowledgement";
    case MDIO_BUS_LOW: return "MDIO idle low: check pull-up settling/power/wiring";
    }
    return "unknown MDIO error";
}
