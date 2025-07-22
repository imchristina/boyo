#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "mem.h"
#include "ppu.h"
#include "timer.h"
#include "joypad.h"
#include "serial.h"
#include "cartridge.h"
#include "log.h"

#define MEM_WRITE_NEXT_LEN 4

mem_t mem = {};

uint8_t mem_io_read(uint8_t addr) {
    DEBUG_PRINTF_MEM("IO READ:0x%X ", addr);

    if (addr == 0x00) {
        return joypad_io_read(addr);
    } else if (addr <= 0x02) {
        return serial_io_read(addr);
    } else if ((addr >= 0x04) && (addr <= 0x07)) {
        return timer_io_read(addr);
    } else if (addr == 0x0F) {
        return mem.iflag;
    } else if ((addr >= 0x40) && (addr <= 0x4B)) {
        return ppu_io_read(addr);
    } else {
        return 0;
    }
}

void mem_io_write(uint8_t addr, uint8_t data) {
    DEBUG_PRINTF_MEM("IO WRITE:0x%X 0x%X\n", addr, data);

    if (addr == 0x00) {
      joypad_io_write(addr, data);
    } else if (addr <= 0x02) {
        serial_io_write(addr, data);
    } else if ((addr >= 0x04) && (addr <= 0x07)) {
        timer_io_write(addr, data);
    } else if (addr == 0x0F) {
        mem.iflag = data;
    } else if ((addr >= 0x40) && (addr <= 0x4B)) {
        ppu_io_write(addr, data);
    } else if ((addr == 0x50) && data) {
        DEBUG_PRINTF_MEM("BOOTROM DISABLED\n");
        mem.bootrom_disable = 1;
    }
}

uint8_t mem_read(uint16_t addr) {
    if ((addr < 256) && !mem.bootrom_disable) {
        return mem.bootrom[addr];
    } else if (addr <= 0x7FFF) {
        return cartridge_read(addr);
    } else if (addr <= 0x9FFF) {
        return mem.vram[addr-0x8000];
    } else if (addr <= 0xBFFF) {
        return cartridge_read(addr);
    } else if (addr <= 0xDFFF) {
        return mem.wram[addr-0xC000];
    } else if (addr <= 0xFDFF) {
        return mem.wram[addr-0xE000];
    } else if (addr <= 0xFE9F) {
        return mem.oam[addr-0xFE00];
    } else if (addr <= 0xFEFF) {
        return 0;
    } else if (addr <= 0xFF7F) {
        return mem_io_read(addr & 0x00FF);
    } else if (addr <= 0xFFFE) {
        return mem.hram[addr-0xFF80];
    } else if (addr == 0xFFFF) {
        return mem.ie;
    } else {
        printf("Unknown read address 0x%X\n", addr);
        exit(1);
        return 0;
    }
}

void mem_write(uint16_t addr, uint8_t data) {
    if (addr <= 0x7FFF) {
        cartridge_write(addr, data);
    } else if (addr <= 0x9FFF) {
        mem.vram[addr-0x8000] = data;
    } else if (addr <= 0xBFFF) {
        cartridge_write(addr, data);
    } else if (addr <= 0xDFFF) {
        mem.wram[addr-0xC000] = data;
    } else if (addr <= 0xFDFF) {
        mem.wram[addr-0xE000] = data;
    } else if (addr <= 0xFE9F) {
        mem.oam[addr-0xFE00] = data;
    } else if (addr <= 0xFEFF) {
        // Do nothing
    } else if (addr <= 0xFF7F) {
        mem_io_write(addr & 0x00FF, data);
    } else if (addr <= 0xFFFE) {
        mem.hram[addr-0xFF80] = data;
    } else if (addr == 0xFFFF) {
        mem.ie = data;
    } else {
        printf("Unknown write address 0x%X, data 0x%X\n", addr, data);
        exit(1);
    }
}

uint16_t mem_read16(uint16_t addr) {
    return ((uint16_t)mem_read(addr+1) << 8) + (uint16_t)mem_read(addr);
}

void mem_write16(uint16_t addr, uint16_t data) {
    mem_write(addr, data & 0x00FF);
    mem_write(addr + 1, (data & 0xFF00) >> 8);
}

void mem_open_bootrom(char *path) {
    FILE *rom = fopen(path, "rb");
    if (!rom) {
        printf("Could not open %s\n", path);
        exit(1);
    }

    fread(mem.bootrom, 1, 256, rom);
    fclose(rom);
}

// Memory write log, to be commited at t = 0
static mem_write_t mem_writes[MEM_WRITE_NEXT_LEN] = {};
int mem_writes_i = 0;

void mem_write_next(uint16_t addr, uint8_t data) {
    if (mem_writes_i >= MEM_WRITE_NEXT_LEN) {
        printf("Memory write next length exceeded!");
        exit(1);
    }
    mem_write_t write = {
        .addr = addr,
        .data = data
    };
    mem_writes[mem_writes_i] = write;
    mem_writes_i++;
}

void mem_write_next16(uint16_t addr, uint16_t data) {
    mem_write_next(addr, data & 0x00FF);
    mem_write_next(addr + 1, (data & 0xFF00) >> 8);
}

void mem_writeback() {
    for (int i = 0; i < mem_writes_i; i++) {
            mem_write(mem_writes[i].addr, mem_writes[i].data);
    }
    mem_writes_i = 0;
}
