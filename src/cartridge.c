#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#include "cartridge.h"
#include "emu.h"

#define HEADER_TITLE_OFFSET     0x134
#define HEADER_TITLE_SIZE       0x10
#define HEADER_TYPE_OFFSET      0x147
#define HEADER_ROM_SIZE_OFFSET  0x148
#define HEADER_RAM_SIZE_OFFSET  0x149

gb_cartridge_t cartridge = {};

void cartridge_load_rom(uint8_t *data, size_t size) {
    if (size < EMU_ROM_SIZE_MIN) {
        printf("CARTRIDGE: Loaded ROM size smaller than minimum!\n");
        exit(1);
    }

    cartridge.rom = data;
    cartridge.type = cartridge.rom[HEADER_TYPE_OFFSET];
    cartridge.rom_size = cartridge.rom[HEADER_ROM_SIZE_OFFSET];
    cartridge.ram_size = cartridge.rom[HEADER_RAM_SIZE_OFFSET];
}

size_t cartridge_get_ram_size() {
    size_t size = 0;
    switch (cartridge.type) {
        case 0x03: // MBC1+RAM+BATTERY
        case 0x10: // MBC3+TIMER+RAM+BATTERY
        case 0x13: // MBC3+RAM+BATTERY
        case 0x1B: // MBC5+RAM+BATTERY
        case 0x1E: // MBC5+RUMBLE+RAM+BATTERY
            switch (cartridge.ram_size) {
                case 0x02: size = 0x2000; break;
                case 0x03: size = 0x8000; break;
                case 0x04: size = 0x20000; break;
                case 0x05: size = 0x10000; break;
            }
            break;
        case 0x06: size = 0x200; break; // MBC2+BATTERY
    }

    return size;
}

void cartridge_load_ram(uint8_t *data, size_t size) {
    cartridge.ram = data;

    size_t sav_size = cartridge_get_ram_size();
    if (size < sav_size) {
        printf("CARTRIDGE: Loaded RAM size smaller than cartridge spec, continuing anyways\n");
    }
}

size_t cartridge_get_title(char *title) {
    size_t i = 0;

    do {
        title[i] = cartridge.rom[i+HEADER_TITLE_OFFSET];
        i++;
    } while (title[i-1] != 0 && i < EMU_TITLE_SIZE_MAX);

    return i;
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
                uint16_t bank = cartridge.rom_bank;
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
                cartridge.ram_bank = data & 0b00001111;
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
