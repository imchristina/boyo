#ifndef CPU_H
#define CPU_H

#include <stdint.h>

typedef struct {
    // Registers
    uint8_t a;
    uint8_t f;
    uint8_t b;
    uint8_t c;
    uint8_t d;
    uint8_t e;
    uint8_t h;
    uint8_t l;
    uint16_t sp;
    uint16_t pc;
    bool ime; // Interrupt handling enable

    // Internal State
    bool halt;
    bool stop;
    bool ime_pending; // EI state mutation delayed by one instruction
    uint8_t op;
} cpu_t;

void cpu_reset();
uint8_t cpu_execute();
void cpu_writeback();
void cpu_continue();

#endif
