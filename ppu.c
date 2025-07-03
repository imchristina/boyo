#include "ppu.h"
#include "mem.h"

#include <stdint.h>

static ppu_t ppu = {
    .lcdc = &mem.io_reg[0x40],
    .stat = &mem.io_reg[0x41],
    .scy = &mem.io_reg[0x42],
    .scx = &mem.io_reg[0x43],
    .ly = &mem.io_reg[0x44],
    .lyc = &mem.io_reg[0x45],
    .dma = &mem.io_reg[0x46],
    .bgp = &mem.io_reg[0x47],
    .obp0 = &mem.io_reg[0x48],
    .obp1 = &mem.io_reg[0x49],
    .wy = &mem.io_reg[0x4A],
    .wx = &mem.io_reg[0x4B]
};

void ppu_execute(uint8_t t) {
    *ppu.ly = 0x90;
}
