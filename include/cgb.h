#ifndef CGB_H
#define CGB_H

#include <stdint.h>

#define CGB_SPEED_NORMAL 0
#define CGB_SPEED_DOUBLE 1

typedef struct {
    uint8_t speed;
} gb_cgb_t;

void cgb_execute(uint8_t t);
uint8_t cgb_io_read(uint16_t addr);
void cgb_io_write(uint16_t addr, uint8_t data);
bool cgb_speed();

#endif
