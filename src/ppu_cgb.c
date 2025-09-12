#ifdef CGB

#include <stdint.h>

#include "ppu.h"
#include "mem.h"
#include "log.h"
#include "cgb.h"

#define PPU_MODE_HBLANK     0
#define PPU_MODE_VBLANK     1
#define PPU_MODE_OAM        2
#define PPU_MODE_DRAWING    3

#define LCDC_BG_WIN_ENABLE      0b00000001
#define LCDC_OBJ_ENABLE         0b00000010
#define LCDC_OBJ_SIZE           0b00000100
#define LCDC_BG_TILEMAP         0b00001000
#define LCDC_BG_WIN_TILE_DATA   0b00010000
#define LCDC_WIN_ENABLE         0b00100000
#define LCDC_WIN_TILEMAP        0b01000000
#define LCDC_PPU_ENABLE         0b10000000

#define STAT_PPU_MODE       0b00000011
#define STAT_LYC_LY         0b00000100
#define STAT_HBLANK_INT     0b00001000
#define STAT_VBLANK_INT     0b00010000
#define STAT_OAM_INT        0b00100000
#define STAT_LYC_INT        0b01000000
#define STAT_UNUSED         0b10000000

#define OBJ_CGB_PALLETE 0b00000111
#define OBJ_BANK        0b00001000
#define OBJ_DMG_PALETTE 0b00010000
#define OBJ_X_FLIP      0b00100000
#define OBJ_Y_FLIP      0b01000000
#define OBJ_PRIORITY    0b10000000

#define BG_ATTR_PALETTE     0b00000111
#define BG_ATTR_BANK        0b00001000
#define BG_ATTR_UNUSED      0b00010000
#define BG_ATTR_X_FLIP      0b00100000
#define BG_ATTR_Y_FLIP      0b01000000
#define BG_ATTR_PRIORITY    0b10000000

#define PI_ADDRESS    0b00111111
#define PI_UNUSED     0b01000000
#define PI_INCREMENT  0b10000000

#define RGB555_RED      0
#define RGB555_GREEN    5
#define RGB555_BLUE     10

ppu_t ppu = {};

uint8_t tile_pixel(uint8_t x, uint8_t y, uint8_t tile_id, bool bg_win, bool bank) {
    // Get tile data address
    uint16_t tile_address;
    if (!bg_win || (ppu.lcdc & LCDC_BG_WIN_TILE_DATA)) {
        tile_address = (tile_id * 16);
    } else {
        tile_address = 0x1000 + ((int8_t)tile_id * 16);
    }

    if (bank) {
        tile_address += 0x2000;
    }

    // Get the high/low bytes
    uint8_t byte_l = ppu.vram[tile_address + (y * 2)];
    uint8_t byte_h = ppu.vram[tile_address + 1 + (y * 2)];

    // Get each bit
    bool h = (byte_h >> (7 - x)) & 1;
    bool l = (byte_l >> (7 - x)) & 1;

    // Combine
    return (h << 1) + l;
}

uint16_t map_palette_bg(uint8_t palette, uint8_t index) {
    // Get the palette address
    uint8_t address = (palette * 8) + (index * 2);
    // Combine the high and low bytes at the address
    uint16_t color = ppu.bgpd[address] | (ppu.bgpd[address+1] << 8);
    // Return the value
    return color;
}

uint16_t map_palette_obj(uint8_t palette, uint8_t index) {
    // Get the palette address
    uint8_t address = (palette * 8) + (index * 2);
    // Combine the high and low bytes at the address
    uint16_t color = ppu.obpd[address] | (ppu.obpd[address+1] << 8);
    // Return the value
    return color;
}

void draw() {
    uint16_t pixel_color = 0;
    uint8_t pixel_index_bg_win = 0;

    // Background
    if (ppu.lcdc & LCDC_BG_WIN_ENABLE) {
        // Get scrolled x/y values
        int x = (ppu.lx + ppu.scx) % 256;
        int y = (ppu.ly + ppu.scy) % 256;

        // Get tile map area
        uint16_t tile_map_addr = (ppu.lcdc & LCDC_BG_TILEMAP) ? 0x1C00 : 0x1800;

        // Obtain tile index
        uint16_t tile_address = ((y/8) * 32 + (x/8)) + tile_map_addr;
        uint8_t tile_id = ppu.vram[tile_address];

        // Get BG map attributes
        uint8_t attributes = ppu.vram[tile_address + 0x2000];
        bool tile_bank = attributes & BG_ATTR_BANK;

        if (attributes & BG_ATTR_X_FLIP) {
            x = 7 - (x % 8);
        }

        if (attributes & BG_ATTR_Y_FLIP) {
            y = 7 - (y % 8);
        }

        pixel_index_bg_win = tile_pixel(x % 8, y % 8, tile_id, 1, tile_bank);
        pixel_color = map_palette_bg(attributes & BG_ATTR_PALETTE, pixel_index_bg_win);
    }

    // Window
    if ((ppu.lcdc & LCDC_BG_WIN_ENABLE) && (ppu.lcdc & LCDC_WIN_ENABLE)) {
        if (ppu.lx >= (ppu.wx - 7) && ppu.ly >= ppu.wy) {
            // Get window x/y values
            int x = ppu.lx - (ppu.wx - 7);
            int y = ppu.ly - ppu.wy;

            // Get tile map area
            uint16_t tile_map_addr = (ppu.lcdc & LCDC_WIN_TILEMAP) ? 0x1C00 : 0x1800;

            // Obtain tile index
            uint16_t tile_address = ((y/8) * 32 + (x/8)) + tile_map_addr;
            uint8_t tile_id = ppu.vram[tile_address];

            // Get BG map attributes
            uint8_t attributes = ppu.vram[tile_address + 0x2000];
            bool tile_bank = attributes & BG_ATTR_BANK;

            if (attributes & BG_ATTR_X_FLIP) {
                x = 7 - (x % 8);
            }

            if (attributes & BG_ATTR_Y_FLIP) {
                y = 7 - (y % 8);
            }

            pixel_index_bg_win = tile_pixel(x % 8, y % 8, tile_id, 1, tile_bank);
            pixel_color = map_palette_bg(attributes & BG_ATTR_PALETTE, pixel_index_bg_win);
        }
    }

    // Objects
    if (ppu.lcdc & LCDC_OBJ_ENABLE) {
        int x = ppu.lx + 8;
        int y = ppu.ly + 16;
        int obj_y_size = (ppu.lcdc & LCDC_OBJ_SIZE) ? 16 : 8;
        for (int i = 0; i < 0xA0; i += 4) {
            int obj_y = mem.oam[i];
            int obj_x = mem.oam[i+1];
            int obj_tile = mem.oam[i+2];
            int obj_flags = mem.oam[i+3];

            // Bit 0 of tile index is ignored for 8x16 objects
            if (obj_y_size == 16) {
                obj_tile &= 0b11111110;
            }

            // Object pixel is not drawn if priority is enabled and BG/WIN index is 1-3
            bool priority_disable = (obj_flags & OBJ_PRIORITY) && (pixel_index_bg_win > 0);

            bool object_in_pos = y >= obj_y && y < obj_y+obj_y_size && x >= obj_x && x < obj_x+8;

            if (!priority_disable && object_in_pos) {
                uint8_t tile_x = x - obj_x;
                uint8_t tile_y = y - obj_y;

                if (obj_flags & OBJ_X_FLIP) {
                    tile_x = 7 - tile_x;
                }

                if (obj_flags & OBJ_Y_FLIP) {
                    tile_y = (obj_y_size - 1) - tile_y;
                }

                uint8_t pixel_index = tile_pixel(tile_x, tile_y, obj_tile, 0, obj_flags & OBJ_BANK);

                uint8_t palette = (obj_flags & OBJ_DMG_PALETTE) ? ppu.obp1 : ppu.obp0;

                // Pixel index 0 == transparent
                if (pixel_index != 0) {
                    pixel_color = map_palette_obj(obj_flags & OBJ_CGB_PALLETE, pixel_index);
                }
            }
        }
    }

    ppu.fb[(ppu.ly * 160) + ppu.lx] = pixel_color;
}

bool ppu_execute(uint8_t t) {
    // Do one dot per t
    bool new_frame = false;

    if (cgb_speed() == CGB_SPEED_DOUBLE) {
        t /= 2;
    }

    if (ppu.lcdc & LCDC_PPU_ENABLE) {
        for (int i = 0; i < t; i++) {
            // Helper variables
            int dot_x = ppu.dot % 456;
            int dot_y = ppu.dot / 456;

            // Set registers
            ppu.ly = dot_y;
            ppu.stat = (ppu.stat & ~STAT_PPU_MODE) | (ppu.mode & STAT_PPU_MODE); // PPU mode
            ppu.stat = (ppu.stat & ~STAT_LYC_LY) | ((ppu.lyc == ppu.ly) ? STAT_LYC_LY : 0); // LYC == LY

            // STAT line is common between all sources and only triggers on high transition
            // Defer final value until all sources are calculated
            bool stat_int_trans = (ppu.lyc == ppu.ly) && (ppu.stat & STAT_LYC_INT);

            // State transitions/timing
            if (dot_y < 144) {
                if (dot_x == 0) {
                    ppu.mode = PPU_MODE_OAM;
                } else if (dot_x == 80) {
                    ppu.mode = PPU_MODE_DRAWING;
                } else if (dot_x == 252+80) {
                    ppu.mode = PPU_MODE_HBLANK;
                }
            } else if (dot_y == 144 && dot_x == 0) {
                ppu.mode = PPU_MODE_VBLANK;
                mem.iflag |= INT_VBLANK;
            }

            // PPU mode
            switch (ppu.mode) {
                case PPU_MODE_HBLANK:
                    stat_int_trans |= ppu.stat & STAT_HBLANK_INT;
                    break;
                case PPU_MODE_VBLANK:
                    stat_int_trans |= ppu.stat & STAT_VBLANK_INT;
                    break;
                case PPU_MODE_OAM:
                    stat_int_trans |= ppu.stat & STAT_OAM_INT;
                    break;
                case PPU_MODE_DRAWING:
                    ppu.lx = dot_x - 80;
                    if (ppu.lx < 160) {
                        draw();
                    }
                    break;
            }

            if (!ppu.stat_int && stat_int_trans) {
                mem.iflag |= INT_STAT;
            }
            ppu.stat_int = stat_int_trans;

            ppu.dot++;
            if (ppu.dot >= 70224) {
                ppu.dot = 0;
                new_frame = true;
            }
        }
    } else {
        ppu.dot = 0;
        ppu.mode = 0;
        ppu.ly = 0;
        ppu.stat &= ~(STAT_PPU_MODE | STAT_LYC_LY);
    }

    return new_frame;
}

bool ppu_enabled() {
    return ppu.lcdc & LCDC_PPU_ENABLE;
}

void oam_dma(uint8_t data) {
    DEBUG_PRINTF_PPU("OAM DMA:0x%X00\n", data);
    for (uint16_t i = 0; i <= 0x9F; i++) {
        uint16_t data_addr = ((uint16_t)data << 8) + i;
        mem.oam[i] = mem_read(data_addr);
    }
}

uint8_t ppu_io_read(uint8_t addr) {
    switch (addr) {
        case 0x40: return ppu.lcdc; break;
        case 0x41: return ppu.stat | STAT_UNUSED; break;
        case 0x42: return ppu.scy; break;
        case 0x43: return ppu.scx; break;
        case 0x44: return ppu.ly; break;
        case 0x45: return ppu.lyc; break;
        case 0x46: return ppu.dma; break;
        case 0x47: return ppu.bgp; break;
        case 0x48: return ppu.obp0; break;
        case 0x49: return ppu.obp1; break;
        case 0x4A: return ppu.wy; break;
        case 0x4B: return ppu.wx; break;
        case 0x4F: return ppu.vram_bank; break;
        case 0x68: return ppu.bgpi | PI_UNUSED; break;
        case 0x69: return ppu.bgpd[ppu.bgpi & PI_ADDRESS]; break;
        case 0x6A: return ppu.obpi | PI_UNUSED; break;
        case 0x6B: return ppu.obpd[ppu.obpi & PI_ADDRESS]; break;
        default: return 0; break;
    }
}

void ppu_io_write(uint8_t addr, uint8_t data) {
    switch (addr) {
        case 0x40: ppu.lcdc = data; break;
        case 0x41: ppu.stat = data; break;
        case 0x42: ppu.scy = data; break;
        case 0x43: ppu.scx = data; break;
        case 0x44: ppu.ly = data; break;
        case 0x45: ppu.lyc = data; break;
        case 0x46: oam_dma(data); break;
        case 0x47: ppu.bgp = data; break;
        case 0x48: ppu.obp0 = data; break;
        case 0x49: ppu.obp1 = data; break;
        case 0x4A: ppu.wy = data; break;
        case 0x4B: ppu.wx = data; break;
        case 0x4F: ppu.vram_bank = data & 1; break;
        case 0x68: ppu.bgpi = data; break;
        case 0x69:
            ppu.bgpd[ppu.bgpi & PI_ADDRESS] = data;
            if (ppu.bgpi & PI_INCREMENT) {
                ppu.bgpi = PI_INCREMENT | ((ppu.bgpi & PI_ADDRESS) + 1);
            }
            break;
        case 0x6A: ppu.obpi = data; break;
        case 0x6B:
            ppu.obpd[ppu.obpi & PI_ADDRESS] = data;
            if (ppu.obpi & PI_INCREMENT) {
                ppu.obpi = PI_INCREMENT | ((ppu.obpi & PI_ADDRESS) + 1);
            }
            break;
    }
}

uint8_t ppu_vram_read(uint16_t addr) {
    addr -= 0x8000 - (ppu.vram_bank * 0x2000);
    return ppu.vram[addr];
}

void ppu_vram_write(uint16_t addr, uint8_t data) {
    addr -= 0x8000 - (ppu.vram_bank * 0x2000);
    ppu.vram[addr] = data;
}

#endif
