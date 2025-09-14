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

    fread(cartridge.rom, 1, ROM_SIZE, rom);
    fclose(rom);

    // Decode cartridge header
    for (int i = 0x134; i <= 0x143; i++) {
        cartridge.title[i-0x134] = cartridge.rom[i];
    }
    cartridge.type = cartridge.rom[0x147];
    cartridge.rom_size = cartridge.rom[0x148];
    cartridge.ram_size = cartridge.rom[0x149];
}

int get_sav_size() {
    int sav_size = 0;
    switch (cartridge.type) {
        case 0x03: // MBC1+RAM+BATTERY
        case 0x10: // MBC3+TIMER+RAM+BATTERY
        case 0x13: // MBC3+RAM+BATTERY
        case 0x1B: // MBC5+RAM+BATTERY
        case 0x1E: // MBC5+RUMBLE+RAM+BATTERY
            switch (cartridge.ram_size) {
                case 0x02: sav_size = 0x2000; break;
                case 0x03: sav_size = 0x8000; break;
                case 0x04: sav_size = 0x20000; break;
                case 0x05: sav_size = 0x10000; break;
            }
            break;
        case 0x06: sav_size = 0x200; break; // MBC2+BATTERY
    }

    return sav_size;
}

void cartridge_open_ram(char *path) {
    int sav_size = get_sav_size();
    if (sav_size > 0) {
        FILE *save = fopen(path, "rb");
        if (save) {
            fread(cartridge.ram, 1, sav_size, save);
            fclose(save);
        }
    }
}

void cartridge_save_ram(char *path) {
    int sav_size = get_sav_size();
    if (sav_size > 0) {
        FILE *save = fopen(path, "wb");
        if (!save) {
            printf("Could not open cartridge save %s\n", path);
            exit(1);
        }

        fwrite(cartridge.ram, 1, sav_size, save);
        fclose(save);
    }
}

uint8_t cartridge_read(uint16_t addr) {
    switch (cartridge.type) {
        case 0x00: // ROM ONLY
            return cartridge.rom[addr];
            break;
        case 0x01: // MBC1
        case 0x02: // MBC1+RAM
        case 0x03: // MBC1+RAM+BATTERY
            if (addr <= 0x3FFF) { // ROM Bank 0
                return cartridge.rom[addr];
            } else if (addr <= 0x7FFF) { // ROM bank 1+
                uint8_t bank = cartridge.rom_bank;
                if (bank == 0) { bank = 1; }

                int bank_addr = 0x4000 * (bank - 1);
                return cartridge.rom[addr+bank_addr];
            } else if (addr >= 0xA000 && addr <= 0xBFFF) { // RAM
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
        case 0x05: // MBC2
        case 0x06: // MBC2+BATTERY
            if (addr <= 0x3FFF) { // ROM Bank 0
                return cartridge.rom[addr];
            } else if (addr <= 0x7FFF) { // ROM bank 1+
                uint8_t bank = cartridge.rom_bank;
                if (bank == 0) { bank = 1; }

                int bank_addr = 0x4000 * (bank - 1);
                return cartridge.rom[addr+bank_addr];
            } else if (addr >= 0xA000 && addr <= 0xBFFF) { // RAM
                if (cartridge.ram_enable) {
                    uint16_t ram_addr = addr - 0xA000;
                    return cartridge.ram[ram_addr % 0x200] | 0xF0;
                }
            }
            break;
        case 0x10: // MBC3+TIMER+RAM+BATTERY
        case 0x11: // MBC3
        case 0x12: // MBC3+RAM
        case 0x13: // MBC3+RAM+BATTERY
            if (addr <= 0x3FFF) { // ROM Bank 0
                return cartridge.rom[addr];
            } else if (addr <= 0x7FFF) { // ROM bank 1+
                uint8_t bank = cartridge.rom_bank;
                if (bank == 0) { bank = 1; }

                int bank_addr = 0x4000 * (bank - 1);
                return cartridge.rom[addr+bank_addr];
            } else if (addr >= 0xA000 && addr <= 0xBFFF) { // RAM
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
        case 0x19: // MBC5
        case 0x1A: // MBC5+RAM
        case 0x1B: // MBC5+RAM+BATTERY
        case 0x1C: // MBC5+RUMBLE
        case 0x1D: // MBC5+RUMBLE+RAM
        case 0x1E: // MBC5+RUMBLE+RAM+BATTERY
            if (addr <= 0x3FFF) { // ROM Bank 0
                return cartridge.rom[addr];
            } else if (addr <= 0x7FFF) { // ROM bank *
                uint8_t bank = cartridge.rom_bank;
                int bank_addr = 0x4000 * (bank - 1);
                return cartridge.rom[addr+bank_addr];
            } else if (addr >= 0xA000 && addr <= 0xBFFF) { // RAM
                if (cartridge.ram_enable) {
                    uint16_t ram_addr = addr - 0xA000;
                    switch (cartridge.ram_size) {
                        case 0x02: // 8KB Unbanked
                            return cartridge.ram[ram_addr];
                            break;
                        case 0x03: // 32KB Banked
                        case 0x04: // 128KB Banked
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
        case 0x05: // MBC2
        case 0x06: // MBC2+BATTERY
            if (addr <= 0x3FFF) { // RAM Enable/ROM Bank Number
                bool rom_mode = addr & 0b100000000;
                if (rom_mode) {
                    cartridge.rom_bank = data & 0b00001111;
                } else {
                    cartridge.ram_enable = (data & 0b00001111) == 0xA;
                }
            } else if (addr >= 0xA000 && addr <= 0xBFFF) { // RAM
                if (cartridge.ram_enable) {
                    uint16_t ram_addr = addr - 0xA000;
                    cartridge.ram[ram_addr % 0x200] = data;
                }
            }
            break;
        case 0x10: // MBC3+TIMER+RAM+BATTERY
        case 0x11: // MBC3
        case 0x12: // MBC3+RAM
        case 0x13: // MBC3+RAM+BATTERY
            if (addr <= 0x1FFF) { // RAM Enable
                cartridge.ram_enable = (data & 0b00001111) == 0xA;
            } else if (addr <= 0x3FFF) { // ROM Bank Number
                cartridge.rom_bank = data & 0b01111111;
            } else if (addr <= 0x5FFF) { // RAM Bank Number
                cartridge.ram_bank = data & 0b00000011;
            } else if (addr <= 0x7FFF) { // Bank Mode Select
                cartridge.bank_mode = data & 0b00000001;
            } else if (addr >= 0xA000 && addr <= 0xBFFF) { // RAM/RTC Register
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
        case 0x19: // MBC5
        case 0x1A: // MBC5+RAM
        case 0x1B: // MBC5+RAM+BATTERY
        case 0x1C: // MBC5+RUMBLE
        case 0x1D: // MBC5+RUMBLE+RAM
        case 0x1E: // MBC5+RUMBLE+RAM+BATTERY
            if (addr <= 0x1FFF) { // RAM Enable
                cartridge.ram_enable = (data & 0b00001111) == 0xA;
            } else if (addr <= 0x2FFF) { // ROM Bank Number (8 LSB)
                cartridge.rom_bank &= 0b100000000;
                cartridge.rom_bank |= data;
            } else if (addr <= 0x3FFF) { // ROM Bank Number (MSB)
                cartridge.rom_bank &= 0b011111111;
                cartridge.rom_bank |= (data & 1) << 8;
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
                        case 0x04: // 128KB Banked
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
