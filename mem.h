#ifndef MEM_H
#define MEM_H

#include <stdint.h>
#include <stdio.h>

typedef struct {
    uint8_t bootrom[256];
    uint8_t rom[0x8000];
    uint8_t vram[0x2000];
    uint8_t eram[0x2000];
    uint8_t wram[0x2000];
    uint8_t oam[0xA0];
    uint8_t io_reg[0x80];
    uint8_t hram[0x7F];
    uint8_t ie;
} mem_t;

typedef struct {
    uint16_t addr;
    uint8_t data;
} mem_write_t;

uint8_t mem_read(uint16_t addr);
void mem_write(uint16_t addr, uint8_t data);
uint16_t mem_read16(uint16_t addr);
void mem_write16(uint16_t addr, uint16_t data);
void mem_open_bootrom(char *path);
void mem_open_rom(char *path);
void mem_write_next(uint16_t addr, uint8_t data);
void mem_writeback();

extern mem_t mem;

#endif
