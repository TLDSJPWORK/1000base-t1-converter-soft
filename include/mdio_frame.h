#ifndef CONVERTER_MDIO_FRAME_H
#define CONVERTER_MDIO_FRAME_H
#include <stdbool.h>
#include <stdint.h>
/* 64 wire bits, two PIO output bits per wire bit: value then direction. */
bool mdio_frame_encode(uint32_t words[4], bool read, unsigned phy, unsigned reg, uint16_t data);
bool mdio_frame_decode(uint32_t samples, uint16_t *data);
#endif
