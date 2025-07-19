#include <stdint.h>
#include <stdio.h>

#include "serial.h"
#include "mem.h"

#define SC_CLOCK_SELECT     0b00000001
#define SC_TRANSFER_ENABLE  0b10000000
#define SC_UNUSED           0b01111110

gb_serial_t serial = {
    .sb = 0xFF
};

void serial_execute(uint8_t t) {
    for (int i = 0; i < t; i++) {
        // Only do anything if the master/internal clock is enabled
        if ((serial.sc & SC_TRANSFER_ENABLE) && (serial.sc & SC_CLOCK_SELECT)) {
            serial.counter += 1;
            if (serial.counter >= 512) { // 8192 Hz
                mem.io_reg[0x0F] |= INT_SERIAL;
                serial.sc &= ~SC_TRANSFER_ENABLE;
                serial.counter = 0;
            }
        }
    }
}

uint8_t serial_io_read(uint8_t addr) {
    switch (addr) {
        case 0x01: return serial.sb; break;
        case 0x02: return serial.sc | SC_UNUSED; break;
        default:
            printf("Bad serial IO read");
            return 0;
            break;
    }
}

void serial_io_write(uint8_t addr, uint8_t data) {
    switch (addr) {
        case 0x01: /* Ignore SB writes */ break;
        case 0x02:
            serial.sc = data;
            serial.counter = 0;
            break;
        default:
            printf("Bad serial IO write");
            break;
    }
}
