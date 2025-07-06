#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>

typedef struct {
    uint8_t *div;
    uint8_t *tima;
    uint8_t *tma;
    uint8_t *tac;

    // Internal state
    //int8_t div_timer; // Clock speed / 256
    uint8_t div_last; // Detect writes to div
    //uint8_t tima_timer;
    uint16_t counter;
    bool clock_last;
} gb_timer_t;

void timer_execute(uint8_t t);

#endif
