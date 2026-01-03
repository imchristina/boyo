#ifndef CARTRIDGE_H
#define CARTRIDGE_H

#include <stddef.h>

typedef struct {
    uint8_t *rom;
    uint8_t *ram;

    uint8_t type;
    uint8_t rom_size;
    uint8_t ram_size;

    bool ram_enable;
    uint16_t rom_bank;
    uint16_t ram_bank;
    bool bank_mode;
} gb_cartridge_t;

extern gb_cartridge_t cartridge;

void cartridge_load_rom(uint8_t *data, size_t size);
void cartridge_load_ram(uint8_t *data, size_t size);
size_t cartridge_get_ram_size();
size_t cartridge_get_title(char *title);
uint8_t cartridge_read(uint16_t addr);
void cartridge_write(uint16_t addr, uint8_t data);

#endif
