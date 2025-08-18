#ifndef CARTRIDGE_H
#define CARTRIDGE_H

typedef struct {
    uint8_t rom[8000000];
    uint8_t ram[0x8000];

    char title[16];
    uint8_t type;
    uint8_t rom_size;
    uint8_t ram_size;

    bool ram_enable;
    uint8_t rom_bank;
    uint8_t ram_bank;
    bool bank_mode;
} gb_cartridge_t;

extern gb_cartridge_t cartridge;

void cartridge_open_rom(char *path);
void cartridge_open_ram(char *path);
void cartridge_save_ram(char *path);
uint8_t cartridge_read(uint16_t addr);
void cartridge_write(uint16_t addr, uint8_t data);

#endif
