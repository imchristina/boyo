#include <stdint.h>

#include "cgb.h"
#include "mem.h"
#include "cpu.h"
#include "log.h"

#define CGB_SPEED_ARMED     0b00000001
#define CGB_SPEED_UNUSED    0b01111110
#define CGB_SPEED_CURRENT   0b10000000

gb_cgb_t cgb = {};

void cgb_execute(uint8_t t) {
    // If the CPU is stopped (t == 0) and speed change is armed,
    // wipe the speed register and set the inverse of the previous speed
    if ((t == 0) && (cgb.speed & CGB_SPEED_ARMED)) {
        bool speed_current = cgb.speed & CGB_SPEED_CURRENT;
        cgb.speed = CGB_SPEED_CURRENT * !speed_current;
        cpu_continue();
        DEBUG_PRINTF("CGB SPEED CHANGED: %s\n", speed_current ? "NORMAL" : "DOUBLE");
    }
}

uint8_t cgb_io_read(uint16_t addr) {
    switch (addr) {
        case 0x4D: return cgb.speed | CGB_SPEED_UNUSED; break;
        default: return 0xFF; break;
    }
}

void cgb_io_write(uint16_t addr, uint8_t data) {
    switch (addr) {
        case 0x4D: cgb.speed = data; break;
    }
}

bool cgb_speed() {
    return cgb.speed & CGB_SPEED_CURRENT;
}
