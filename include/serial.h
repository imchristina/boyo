#ifndef SERIAL_H
#define SERIAL_H

typedef struct {
    uint8_t sb;
    uint8_t sc;

    int counter;
} gb_serial_t;

void serial_execute(uint8_t t);
uint8_t serial_io_read(uint8_t addr);
void serial_io_write(uint8_t addr, uint8_t data);

#endif
