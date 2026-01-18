#include <stdint.h>
#include <stdio.h>

#include "joypad.h"
#include "mem.h"

#define JOYPAD_SELECT_DPAD          0b00010000
#define JOYPAD_SELECT_BUTTONS       0b00100000

gb_joypad_t joypad = {
    .buttons = 0x0F,
    .dpad = 0x0F,
    .select = 0
};

void joypad_down(uint8_t mask) {
    if ((mask & JOYPAD_SELECT_BUTTONS) == 0) {
        joypad.buttons &= (mask & 0x0F);
    } else if ((mask & JOYPAD_SELECT_DPAD) == 0) {
        joypad.dpad &= (mask & 0x0F);
    }

    mem.iflag |= INT_JOYPAD;
}

void joypad_up(uint8_t mask) {
    if ((mask & JOYPAD_SELECT_BUTTONS) == 0) {
        joypad.buttons |= (~mask & 0x0F);
    } else if ((mask & JOYPAD_SELECT_DPAD) == 0) {
        joypad.dpad |= (~mask & 0x0F);
    }
}

uint8_t joypad_io_read(uint8_t addr) {
    switch (addr) {
        case 0x00:
            uint8_t state = 0x0F;

            if ((joypad.select & JOYPAD_SELECT_BUTTONS) == 0) {
                state &= joypad.buttons;
            }
            if ((joypad.select & JOYPAD_SELECT_DPAD) == 0) {
                state &= joypad.dpad;
            }

            return 0b11000000 | joypad.select | state;
            break;
        default:
            printf("Bad joypad IO read");
            return 0;
            break;
    }
}

void joypad_io_write(uint8_t addr, uint8_t data) {
    switch (addr) {
        case 0x00:
            joypad.select = data & (JOYPAD_SELECT_BUTTONS | JOYPAD_SELECT_DPAD);
            break;
        default:
            printf("Bad joypad IO write");
            break;
    }
}
