#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>

typedef struct {
    uint8_t div;
    uint8_t tima;
    uint8_t tma;
    uint8_t tac;

    // Internal state
    uint16_t counter;
    bool clock_last;
} gb_timer_t;

extern gb_timer_t timer;

void timer_execute(uint8_t t);
uint8_t timer_io_read(uint8_t addr);
void timer_io_write(uint8_t addr, uint8_t data);

#endif
