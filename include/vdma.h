#ifndef VDMA_H
#define VDMA_H

typedef struct {
    uint16_t source;
    uint16_t destination;
    uint8_t control;

    bool start;
} gb_vdma_t;

void vdma_execute(uint8_t t);
uint8_t vdma_io_read(uint8_t addr);
void vdma_io_write(uint8_t addr, uint8_t data);

#endif
