#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "mem.h"
#include "ansi_color.h"

#define MEM_WRITE_NEXT_LEN 4

mem_t mem = {};

uint8_t mem_read(uint16_t addr) {
    if ((addr < 255) && !mem.bootrom_disable) {
        return mem.bootrom[addr];
    } else if (addr <= 0x7FFF) {
        return mem.rom[addr];
    } else if (addr <= 0x9FFF) {
        return mem.vram[addr-0x8000];
    } else if (addr <= 0xBFFF) {
        return mem.eram[addr-0xA000];
    } else if (addr <= 0xDFFF) {
        return mem.wram[addr-0xC000];
    } else if (addr <= 0xFDFF) {
        return mem.wram[addr-0xE000];
    } else if (addr <= 0xFE9F) {
        return mem.oam[addr-0xFE00];
    } else if (addr <= 0xFEFF) {
        return 0;
    } else if (addr <= 0xFF7F) {
        printf("%sIO READ:0x%X%s ", ANSI_YELLOW, addr, ANSI_CLEAR);
        return mem.io_reg[addr-0xFF00];
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
        // Writes to rom ignored
    } else if (addr <= 0x9FFF) {
        mem.vram[addr-0x8000] = data;
    } else if (addr <= 0xBFFF) {
        mem.eram[addr-0xA000] = data;
    } else if (addr <= 0xDFFF) {
        mem.wram[addr-0xC000] = data;
    } else if (addr <= 0xFDFF) {
        mem.wram[addr-0xE000] = data;
    } else if (addr <= 0xFE9F) {
        mem.eram[addr-0xFE00] = data;
    } else if (addr <= 0xFEFF) {
        // Do nothing
    } else if (addr <= 0xFF7F) {
        if (addr == 0xFF50 && data) {
            printf("%sBOOTROM DISABLED%s\n", ANSI_YELLOW, ANSI_CLEAR);
            mem.bootrom_disable = 1;
        } else {
            printf("%sIO WRITE:0x%X 0x%X%s\n", ANSI_YELLOW, addr, data, ANSI_CLEAR);
            mem.io_reg[addr-0xFF00] = data;
        }
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

void mem_open_rom(char *path) {
    FILE *rom = fopen(path, "rb");
    if (!rom) {
        printf("Could not open %s\n", path);
        exit(1);
    }

    fread(mem.rom, 1, 0x8000, rom);
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
