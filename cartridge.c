#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "cartridge.h"

gb_cartridge_t cartridge = {};

void cartridge_open_rom(char *path) {
    FILE *rom = fopen(path, "rb");
    if (!rom) {
        printf("Could not open cartridge rom %s\n", path);
        exit(1);
    }

    fread(cartridge.rom, 1, 1000000, rom);
    fclose(rom);

    // Decode cartridge header
    for (int i = 0x134; i <= 0x143; i++) {
        cartridge.title[i-0x134] = cartridge.rom[i];
    }
    cartridge.type = cartridge.rom[0x147];
    cartridge.rom_size = cartridge.rom[0x148];
    cartridge.ram_size = cartridge.rom[0x149];
}

uint8_t cartridge_read(uint16_t addr) {
    switch (cartridge.type) {
        case 0x00: // ROM ONLY
            return cartridge.rom[addr];
            break;
        case 0x01: // MBC1
        case 0x02: // MBC1+RAM
        case 0x03: // MBC1+RAM+BATTERY
            if (addr <= 0x3FFF) {
                return cartridge.rom[addr];
            } else if (addr <= 0x7FFF) {
                uint16_t bank = cartridge.rom_bank;
                if (bank == 0) { bank = 1; }

                uint16_t bank_addr = 0x4000 * (bank - 1);
                return cartridge.rom[addr+bank_addr];
            } else if (addr >= 0xA000 && addr <= 0xBFFF) {
                if (cartridge.ram_enable) {
                    uint16_t ram_addr = addr - 0xA000;
                    switch (cartridge.ram_size) {
                        case 0x02: // 8KB Unbanked
                            return cartridge.ram[ram_addr];
                            break;
                        case 0x03: // 32KB Banked
                            uint16_t bank_addr = 0x2000 * cartridge.ram_bank;
                            return cartridge.ram[ram_addr+bank_addr];
                            break;
                    }
                }
            }
            break;
        default:
            printf("Unknown cartridge type on read: 0x%X\n", cartridge.type);
            exit(1);
            break;
    }

    return 0xFF;
}

void cartridge_write(uint16_t addr, uint8_t data) {
    switch (cartridge.type) {
        case 0x00: // ROM ONLY
            break; // Do nothing
        case 0x01: // MBC1
        case 0x02: // MBC1+RAM
        case 0x03: // MBC1+RAM+BATTERY
            if (addr <= 0x1FFF) { // RAM Enable
                cartridge.ram_enable = (data & 0b00001111) == 0xA;
            } else if (addr <= 0x3FFF) { // ROM Bank Number
                cartridge.rom_bank = data & 0b00011111;
            } else if (addr <= 0x5FFF) { // RAM Bank Number
                cartridge.ram_bank = data & 0b00000011;
            } else if (addr <= 0x7FFF) { // Bank Mode Select
                cartridge.bank_mode = data & 0b00000001;
            } else if (addr >= 0xA000 && addr <= 0xBFFF) { // RAM
                if (cartridge.ram_enable) {
                    uint16_t ram_addr = addr - 0xA000;
                    switch (cartridge.ram_size) {
                        case 0x02: // 8KB Unbanked
                            cartridge.ram[ram_addr] = data;
                            break;
                        case 0x03: // 32KB Banked
                            uint16_t bank_addr = 0x2000 * cartridge.ram_bank;
                            cartridge.ram[ram_addr+bank_addr] = data;
                            break;
                    }
                }
            }
            break;
        default:
            printf("Unknown cartridge type on write: 0x%X\n", cartridge.type);
            exit(1);
            break;
    }
}
