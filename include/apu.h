#ifndef APU_H
#define APU_H

#define APU_BUFFER_SIZE 256
#define APU_SAMPLE_RATE 44100

typedef struct {
    uint8_t sweep;
    uint8_t length_duty;
    uint8_t envelope;
    int period;
    uint8_t control;

    uint8_t length_timer;
    int period_timer;
    uint8_t envelope_timer;
    uint8_t volume;
    uint8_t pulse_index;
    int16_t sample;
} gb_apu_ch1_t;

typedef struct {
    uint8_t length_duty;
    uint8_t envelope;
    int period;
    uint8_t control;

    uint8_t length_timer;
    int period_timer;
    uint8_t envelope_timer;
    uint8_t volume;
    uint8_t pulse_index;
    int16_t sample;
} gb_apu_ch2_t;

typedef struct {
    uint8_t enable;
    uint8_t length;
    uint8_t level;
    int period;
    uint8_t control;
} gb_apu_ch3_t;

typedef struct {
    uint8_t length;
    uint8_t envelope;
    uint8_t freq_rand;
    uint8_t control;
} gb_apu_ch4_t;

typedef struct {
    gb_apu_ch1_t ch1;
    gb_apu_ch2_t ch2;
    gb_apu_ch3_t ch3;
    gb_apu_ch4_t ch4;

    uint8_t volume_vin;
    uint8_t panning;
    uint8_t control;

    int16_t buffer[APU_BUFFER_SIZE*2];
    int buffer_index;
    float buffer_index_timer;

    int div_apu;
    bool div_clock_last;
    bool length_clock_last;
} gb_apu_t;

extern gb_apu_t apu;

bool apu_execute(uint8_t t);
uint8_t apu_io_read(uint16_t addr);
void apu_io_write(uint16_t addr, uint8_t data);

#endif
