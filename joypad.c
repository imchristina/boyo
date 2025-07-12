#include <stdint.h>
#include <stdio.h>

#include "joypad.h"

gb_joypad_t joypad = {
    .buttons = 0x0F,
    .dpad = 0x0F,
    .select = JOYPAD_SELECT_BUTTONS | JOYPAD_SELECT_DPAD
};

void joypad_down(uint8_t mask) {
    if ((mask & JOYPAD_SELECT_BUTTONS) == 0) {
        joypad.buttons &= (mask & 0x0F);
    } else if ((mask & JOYPAD_SELECT_DPAD) == 0) {
        joypad.dpad &= (mask & 0x0F);
    }
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
            if ((joypad.select & JOYPAD_SELECT_BUTTONS) == 0) {
                return joypad.select | joypad.buttons;
            } else if ((joypad.select & JOYPAD_SELECT_DPAD) == 0) {
                return joypad.select | joypad.dpad;
            } else {
                return joypad.select | 0x0F;
            }
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
