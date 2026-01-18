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

#endif
