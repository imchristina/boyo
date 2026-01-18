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
    #define WRAM_SIZE 0x8000
#else
    #define WRAM_SIZE 0x2000
#endif

typedef struct {
    uint8_t *bootrom;
    uint8_t wram[WRAM_SIZE];
    uint8_t oam[0xA0];
    uint8_t iflag;
    uint8_t hram[0x7F];
    uint8_t ie;

#ifdef CGB
    uint8_t wram_bank;
#endif

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
void mem_load_bootrom(uint8_t *data, size_t size);
void mem_write_next(uint16_t addr, uint8_t data);
void mem_write_next16(uint16_t addr, uint16_t data);
void mem_writeback();

extern mem_t mem;

#endif
