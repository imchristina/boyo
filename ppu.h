#ifndef PPU_H
#define PPU_H

#include <stdint.h>

typedef struct {
    // MMIO Registers
    uint8_t *lcdc; // LCD control
    uint8_t *stat; // LCD status
    uint8_t *scy; // Viewport Y position
    uint8_t *scx; // Viewport X position
    uint8_t *ly; // LCD Y coordinate
    uint8_t *lyc; // LY compare
    uint8_t *dma; // OAM DMA source address & start
    uint8_t *bgp; // BG palette data
    uint8_t *obp0; // OBJ pallete 0 data
    uint8_t *obp1; // OBJ pallete 1 data
    uint8_t *wy; // Window Y position
    uint8_t *wx; // Window X position plus 7

    int dot; // Current dot in frame
    uint8_t mode;
    uint8_t lx; // Virtual lx for decoupling with dot while rendering
    bool stat_int;
    uint8_t fb[160*144];
} ppu_t;

extern ppu_t ppu;

bool ppu_execute(uint8_t t);
uint8_t ppu_io_read(uint8_t addr);
void ppu_io_write(uint8_t addr, uint8_t data);

#endif
