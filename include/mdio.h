#ifndef CONVERTER_MDIO_H
#define CONVERTER_MDIO_H
#include <stdbool.h>
#include <stdint.h>
typedef enum { MDIO_OK, MDIO_BAD_ARG, MDIO_TIMEOUT, MDIO_NO_ACK, MDIO_BUS_LOW } mdio_result;
/* Single-core API: indirect MMD transactions must not be interleaved. */
mdio_result mdio_init(void);
mdio_result mdio_read(unsigned phy, unsigned reg, uint16_t *value);
mdio_result mdio_write(unsigned phy, unsigned reg, uint16_t value);
mdio_result mdio_mmd_read(unsigned phy, unsigned dev, unsigned reg, uint16_t *value);
mdio_result mdio_mmd_write(unsigned phy, unsigned dev, unsigned reg, uint16_t value);
const char *mdio_error(mdio_result result);
#endif
