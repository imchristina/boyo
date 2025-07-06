#include "timer.h"
#include "mem.h"

#include <stdint.h>
#include <stdlib.h>

#define TAC_CLOCK   0b00000011
#define TAC_ENABLE  0b00000100

gb_timer_t timer = {
    .div = &mem.io_reg[0x04],
    .tima = &mem.io_reg[0x05],
    .tma = &mem.io_reg[0x06],
    .tac = &mem.io_reg[0x07]
};

void timer_execute(uint8_t t) {
    // Loop through M-cycles
    for (int i = 0; i < (t/4); i++) {
        timer.counter += 1;

        // DIV
        if (*timer.div != timer.div_last) {
            timer.counter = 0;
        }
        *timer.div = ((timer.counter >> 8) & 0xFF);
        timer.div_last = *timer.div;

        // TIMA
        if (*timer.tac & TAC_ENABLE) {
            // Select the clock bit
            uint8_t bit;
            switch (*timer.tac & TAC_CLOCK) {
                case 0: bit = 7; break;
                case 1: bit = 1; break;
                case 2: bit = 3; break;
                case 3: bit = 5; break;
            }

            bool clock = (timer.counter >> bit) & 1;

            // Execute on falling edge
            if (!clock && timer.clock_last) {
                *timer.tima += 1;
                if (*timer.tima == 0) {
                    *timer.tima = *timer.tma;
                    mem.io_reg[0x0F] |= INT_TIMER;
                }
            }

            timer.clock_last = clock;
        }
    }

    if (t == 0) {
        // The clock stopped, reset counter
        timer.counter = 0;
        timer.div_last = 0;
    }
}

