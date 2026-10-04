#include "mdio_frame.h"
#include <stddef.h>
#include <string.h>
static void put(uint32_t w[4], unsigned bit, bool value, bool output) {
    w[bit / 16] |= ((uint32_t)value | ((uint32_t)output << 1)) << ((bit % 16) * 2);
}
bool mdio_frame_encode(uint32_t words[4], bool read, unsigned phy, unsigned reg, uint16_t data) {
    if (!words || phy > 31 || reg > 31) return false;
    memset(words, 0, 4 * sizeof(*words));
    /* ST 01, OP 10(read)/01(write), PHY[4:0], REG[4:0], TA Z0/10, DATA. */
    uint32_t tail = (1u << 30) | ((read ? 2u : 1u) << 28) |
                    (phy << 23) | (reg << 18) | (read ? 0u : (2u << 16) | data);
    for (unsigned b = 0; b < 32; ++b) put(words, b, true, true);
    for (unsigned b = 0; b < 32; ++b)
        put(words, b + 32, (tail >> (31 - b)) & 1u, !read || b < 14);
    return true;
}
bool mdio_frame_decode(uint32_t samples, uint16_t *data) {
    if (!data || (samples & (1u << 16))) return false;
    *data = (uint16_t)samples;
    return true;
}
