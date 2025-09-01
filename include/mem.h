#ifndef MEM_H
#define MEM_H

#include <stdint.h>
#include <stdio.h>

// Interrupt masks
#define INT_VBLANK  0b00000001
#define INT_STAT    0b00000010
#define INT_TIMER   0b00000100
#define INT_SERIAL  0b00001000
#define INT_JOYPAD  0b00010000

#ifdef CGB
    #define BOOTROM_SIZE 2048
#else
    #define BOOTROM_SIZE 256
#endif

typedef struct {
    uint8_t bootrom[BOOTROM_SIZE];
    uint8_t vram[0x2000];
    uint8_t wram[0x2000];
    uint8_t oam[0xA0];
    uint8_t iflag;
    uint8_t hram[0x7F];
    uint8_t ie;

    bool bootrom_disable;
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
void mem_write_next16(uint16_t addr, uint16_t data);
void mem_writeback();

extern mem_t mem;

#endif
