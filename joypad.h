#ifndef JOYPAD_H
#define JOYPAD_H

typedef struct {
    uint8_t buttons;
    uint8_t dpad;
    uint8_t select;
} gb_joypad_t;

void joypad_down(uint8_t mask);
void joypad_up(uint8_t mask);
uint8_t joypad_io_read(uint8_t addr);
void joypad_io_write(uint8_t addr, uint8_t data);

#define JOYPAD_BUTTON_A             0b11011110
#define JOYPAD_BUTTON_B             0b11011101
#define JOYPAD_BUTTON_SELECT        0b11011011
#define JOYPAD_BUTTON_START         0b11010111
#define JOYPAD_DPAD_RIGHT           0b11101110
#define JOYPAD_DPAD_LEFT            0b11101101
#define JOYPAD_DPAD_UP              0b11101011
#define JOYPAD_DPAD_DOWN            0b11100111
#define JOYPAD_SELECT_DPAD          0b00010000
#define JOYPAD_SELECT_BUTTONS       0b00100000

#endif
