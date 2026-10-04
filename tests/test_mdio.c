#include "mdio.h"
#include "mdio_frame.h"
#include "phy_config.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned count, fail_at = 99;
static struct { unsigned phy, reg, data; bool read; } calls[4];
mdio_result mdio_write(unsigned p, unsigned r, uint16_t d) {
    unsigned i = count++; calls[i].phy=p; calls[i].reg=r; calls[i].data=d; calls[i].read=false;
    return i == fail_at ? MDIO_TIMEOUT : MDIO_OK;
}
mdio_result mdio_read(unsigned p, unsigned r, uint16_t *d) {
    unsigned i = count++; calls[i].phy=p; calls[i].reg=r; calls[i].read=true;
    if (i == fail_at) return MDIO_NO_ACK;
    *d=0x5aa5; return MDIO_OK;
}
static unsigned symbol(const uint32_t *w, unsigned i) { return (w[i/16] >> (2*(i%16))) & 3u; }
int main(void) {
    assert(dp83867_cfg4_without_mirror(0x10b1u) == 0x10b0u);
    assert(dp83867_cfg4_without_mirror(0x10b0u) == 0x10b0u);
    assert(dp83867_rgmii_control_with_delays(0x50d0u) == 0x50d3u);
    assert(dp83867_rgmii_control_with_delays(0x40d3u) == 0x40d3u);
    assert(dp83867_rgmii_delay_2ns(0x0017u) == 0x0077u);
    assert(dp83867_rgmii_delay_2ns(0xab17u) == 0xab77u);
    assert(dp83tg720_rgmii_control_without_delays(0x0003u) == 0x0000u);
    assert(dp83tg720_rgmii_control_without_delays(0xa5a0u) == 0xa5a0u);
    uint32_t w[4]; uint16_t value;
    /* Decode actual encoded wire fields, independently of encoder packing. */
    for (unsigned p=0; p<32; ++p) for (unsigned r=0; r<32; ++r) {
        for (unsigned read=0; read<2; ++read) {
            assert(mdio_frame_encode(w, read, p, r, 0xa55a));
            for (unsigned b=0; b<32; ++b) assert(symbol(w,b)==3);
            unsigned header=0;
            for (unsigned b=32; b<46; ++b) { assert(symbol(w,b)&2); header=(header<<1)|(symbol(w,b)&1); }
            assert((header>>12)==1); assert(((header>>10)&3)==(read?2:1));
            assert(((header>>5)&31)==p); assert((header&31)==r);
            if (read) for (unsigned b=46; b<64; ++b) assert(!(symbol(w,b)&2));
            else {
                assert(symbol(w,46)==3 && symbol(w,47)==2);
                unsigned data=0;
                for (unsigned b=48; b<64; ++b) { assert(symbol(w,b)&2); data=(data<<1)|(symbol(w,b)&1); }
                assert(data==0xa55a);
            }
        }
    }
    assert(!mdio_frame_encode(w,true,32,0,0)); assert(!mdio_frame_encode(w,true,0,32,0));
    for (unsigned v=0; v<=65535; ++v) {
        assert(mdio_frame_decode(v,&value) && value==v);
        value=0x1234; assert(!mdio_frame_decode(v|(1u<<16),&value) && value==0x1234);
    }
    count=0; assert(mdio_mmd_read(8,31,0x45d,&value)==MDIO_OK && value==0x5aa5);
    assert(count==4 && calls[0].reg==13 && calls[0].data==31);
    assert(calls[1].reg==14 && calls[1].data==0x45d);
    assert(calls[2].reg==13 && calls[2].data==0x401f);
    assert(calls[3].read && calls[3].reg==14);
    for(unsigned i=0;i<4;++i) assert(calls[i].phy==8);
    count=0; assert(mdio_mmd_write(3,1,0x834,0x4000)==MDIO_OK);
    assert(count==4 && calls[2].data==0x4001 && calls[3].data==0x4000 && !calls[3].read);
    for(unsigned f=0; f<4; ++f) {
        count=0; fail_at=f; value=0x1234;
        assert(mdio_mmd_read(8,31,0x45d,&value)!=MDIO_OK);
        assert(count==f+1 && value==0x1234);
    }
    count=0; assert(mdio_mmd_write(32,0,0,0)==MDIO_BAD_ARG);
    assert(mdio_mmd_write(0,32,0,0)==MDIO_BAD_ARG);
    assert(mdio_mmd_write(0,0,65536,0)==MDIO_BAD_ARG);
    assert(mdio_mmd_read(0,0,0,NULL)==MDIO_BAD_ARG && count==0);
    puts("MDIO frames, turnaround, all 16-bit data values, MMD access and error propagation passed");
}
