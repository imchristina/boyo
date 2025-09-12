#ifdef CGB

#include <stdint.h>

#include "vdma.h"
#include "ppu.h"
#include "mem.h"

#define CONTROL_MODE 0b10000000

gb_vdma_t vdma = {};

void vdma_execute(uint8_t t) {
    if (vdma.start) {
        uint16_t length = ((vdma.control & ~CONTROL_MODE) + 1) * 0x10;
        vdma.source &= 0xFFF0;
        vdma.destination &= 0x0FF0;

        for (int i = 0; i < length; i++) {
            uint16_t vram_address = vdma.destination + i + (ppu.vram_bank * 0x2000);
            ppu.vram[vram_address] = mem_read(vdma.source + i);
        }

        printf("VDMA MODE %d COMPLETE!\n", vdma.control & CONTROL_MODE);
        printf("VDMA LENGTH:0x%X\n", length);
        printf("VDMA SOURCE:0x%X\n", vdma.source);
        printf("VDMA DESTINATION:0x%X\n", vdma.destination);

        vdma.start = 0;
    }
}

uint8_t vdma_io_read(uint8_t addr) {
    switch (addr) {
        case 0x55: return 0xFF; break;
        default: return 0xFF; break;
    }
}

void vdma_io_write(uint8_t addr, uint8_t data) {
    switch (addr) {
        case 0x51: vdma.source = (vdma.source & 0x00FF) | (data << 8); break;
        case 0x52: vdma.source = (vdma.source & 0xFF00) | data; break;
        case 0x53: vdma.destination = (vdma.destination & 0x00FF) | (data << 8); break;
        case 0x54: vdma.destination = (vdma.destination & 0xFF00) | data; break;
        case 0x55: vdma.control = data; vdma.start = 1; break;
    }
}

#endif
