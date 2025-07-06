#include "ppu.h"
#include "mem.h"

#include <stdint.h>

#define PPU_MODE_HBLANK     0
#define PPU_MODE_VBLANK     1
#define PPU_MODE_OAM        2
#define PPU_MODE_DRAWING    3

#define LCDC_BG_WIN_ENABLE      0b00000001
#define LCDC_BG_TILEMAP         0b00001000
#define LCDC_BG_WIN_TILE_DATA   0b00010000
#define LCDC_WIN_ENABLE         0b00100000
#define LCDC_WIN_TILEMAP        0b01000000
#define LCDC_PPU_ENABLE         0b10000000

#define STAT_PPU_MODE   0b00000011
#define STAT_LYC_LY     0b00000100
#define STAT_HBLANK_INT  0b00001000
#define STAT_VBLANK_INT  0b00010000
#define STAT_OAM_INT  0b00100000
#define STAT_LYC_INT    0b01000000

ppu_t ppu = {
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

static uint8_t vram_read(uint16_t addr) {
    return mem.vram[addr - 0x8000];
}

static uint8_t tile_pixel(uint8_t x, uint8_t y, uint8_t tile_id, bool bg_win) {
    // Get tile data address
    uint16_t tile_address;
    if (!bg_win || *ppu.lcdc & LCDC_BG_WIN_TILE_DATA) {
        tile_address = 0x8000 + (tile_id * 16);
    } else {
        if (tile_id <= 127) {
            tile_address = 0x9000 + (tile_id * 16);
        } else {
            tile_address = 0x8800 + (tile_id * 16);
        }
    }

    // Get the high/low bytes
    uint8_t byte_h = vram_read(tile_address + (y * 2));
    uint8_t byte_l = vram_read(tile_address + 1 + (y * 2));

    // Get each bit
    bool h = (byte_h >> (7 - x)) & 1;
    bool l = (byte_l >> (7 - x)) & 1;

    // Combine
    return (h << 1) + l;
}

static void draw() {
    uint8_t pixel_color = 0;

    // Background
    if (*ppu.lcdc & LCDC_BG_WIN_ENABLE) {
        // Get scrolled x/y values
        int x = (ppu.lx + *ppu.scx) % 256;
        int y = (*ppu.ly + *ppu.scy) % 256;

        // Get tile map area
        uint16_t tile_map_addr;
        if (*ppu.lcdc & LCDC_BG_TILEMAP) {
            tile_map_addr = 0x9C00;
        } else {
            tile_map_addr = 0x9800;
        }

        // Obtain tile index
        uint8_t tile_id = vram_read(((y/8) * 32 + (x/8)) + tile_map_addr);

        pixel_color = tile_pixel(x % 8, y % 8, tile_id, 1);
    }

    // Window
    if (false) {//((*ppu.lcdc & LCDC_BG_WIN_ENABLE) && (*ppu.lcdc & LCDC_WIN_ENABLE)) {
        if (ppu.lx >= *ppu.wx && *ppu.ly >= (*ppu.wy - 7)) {
            // Get window x/y values
            int x = ppu.lx - (*ppu.wx - 7);
            int y = *ppu.ly - *ppu.wy;

            // Get tile map area
            uint16_t tile_map_addr;
            if (*ppu.lcdc & LCDC_WIN_TILEMAP) {
                tile_map_addr = 0x9C00;
            } else {
                tile_map_addr = 0x9800;
            }

            // Obtain tile index
            uint8_t tile_id = vram_read(((y/8) * 32 + (x/8)) + tile_map_addr);

            pixel_color = tile_pixel(x % 8, y % 8, tile_id, 1);
        }
    }

    // Objects

    ppu.fb[(*ppu.ly * 160) + ppu.lx] = pixel_color;
}

bool ppu_execute(uint8_t t) {
    // Do one dot per t
    bool new_frame = false;

    if (*ppu.lcdc & LCDC_PPU_ENABLE) {
        for (int i = 0; i < t; i++) {
            // Helper variables
            int dot_x = ppu.dot % 456;
            int dot_y = ppu.dot / 456;

            // Set registers
            *ppu.ly = dot_y;
            *ppu.stat = (*ppu.stat & ~STAT_PPU_MODE) | (ppu.mode & 0x03); // PPU mode
            *ppu.stat = (*ppu.stat & ~STAT_LYC_LY) | ((uint8_t)(*ppu.lyc == *ppu.ly) << 3); // LYC == LY

            // STAT line is common between all sources and only triggers on high transition
            // Defer final value until all sources are calculated
            bool stat_int_trans = (*ppu.lyc == *ppu.ly) & *ppu.stat & STAT_LYC_INT;

            // PPU mode/timing
            switch (ppu.mode) {
                case PPU_MODE_HBLANK:
                    if ((dot_x == 0) && (dot_y < 144)) {
                        ppu.mode = PPU_MODE_OAM;
                        stat_int_trans |= *ppu.stat & STAT_OAM_INT;
                    } else if (dot_y >= 144) {
                        mem.io_reg[0x0F] &= INT_VBLANK; // VBLANK interrupt
                        ppu.mode = PPU_MODE_VBLANK;
                        stat_int_trans |= *ppu.stat & STAT_VBLANK_INT;
                    }
                    break;
                case PPU_MODE_VBLANK:
                    if (ppu.dot == 0) {
                        ppu.mode = PPU_MODE_OAM;
                        stat_int_trans |= *ppu.stat & STAT_OAM_INT;
                    }
                    break;
                case PPU_MODE_OAM:
                    if (dot_x >= 80) {
                        ppu.mode = PPU_MODE_DRAWING;
                    }
                    break;
                case PPU_MODE_DRAWING:
                    if (dot_x >= 80+172) { // TODO variable timing
                        ppu.lx = 0;
                        ppu.mode = PPU_MODE_HBLANK;
                        stat_int_trans |= *ppu.stat & STAT_HBLANK_INT;
                    } else if (ppu.lx < 160) {
                        draw();
                        ppu.lx++;
                    }
                    break;
            }

            if (!ppu.stat_int && stat_int_trans) {
                mem.io_reg[0x0F] &= INT_LCD;
            }
            ppu.stat_int = stat_int_trans;

            ppu.dot++;
            if (ppu.dot >= 70224) {
                ppu.dot = 0;
                new_frame = true;
            }
        }
    }

    return new_frame;
}
