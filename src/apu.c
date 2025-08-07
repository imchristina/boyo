#include <stdint.h>
#include <stdio.h>
#include <limits.h>

#include "apu.h"
#include "timer.h"

#define APU_SAMPLE_HIGH INT16_MAX / 4

#define APU_CONTROL_CH1     0b00000001
#define APU_CONTROL_CH2     0b00000010
#define APU_CONTROL_CH3     0b00000100
#define APU_CONTROL_CH4     0b00001000
#define APU_CONTROL_AUDIO   0b10000000
#define APU_CONTROL_UNUSED  0b01110000

#define APU_PAN_RIGHT_CH1   0b00000001
#define APU_PAN_LEFT_CH1    0b00010000

#define APU_CH1_SWEEP_STEP          0b00000111
#define APU_CH1_SWEEP_DIRECTION     0b00001000
#define APU_CH1_SWEEP_PACE          0b01110000
#define APU_CH1_SWEEP_PACE_SHIFT    4
#define APU_CH1_SWEEP_UNUSED        0b10000000

#define APU_CH_LD_LENGTH       0b00111111
#define APU_CH_LD_DUTY         0b11000000
#define APU_CH_LD_DUTY_SHIFT   6

#define APU_CH_ENVELOPE_PACE        0b00000111
#define APU_CH_ENVELOPE_DIR         0b00001000
#define APU_CH_ENVELOPE_VOL         0b11110000
#define APU_CH_ENVELOPE_VOL_SHIFT   4

#define APU_CH_PERIOD_LOW           0b00011111111
#define APU_CH_PERIOD_HIGH          0b11100000000
#define APU_CH_PERIOD_HIGH_SHIFT    8

#define APU_CH_CONTROL_PERIOD       0b00000111
#define APU_CH_CONTROL_LENGTH       0b01000000
#define APU_CH_CONTROL_TRIGGER      0b10000000
#define APU_CH_CONTROL_UNUSED       0b00111000

const uint8_t APU_PULSE_SAMPLES[] = {
    0b11111110,
    0b01111110,
    0b01111000,
    0b10000001
};

gb_apu_t apu = {};

bool apu_execute(uint8_t t) {
    bool new_buffer = false;

    // Step on M cycles
    for (int m = 0; m < t/4; m++) {
        if (apu.control & APU_CONTROL_AUDIO) {
            // Clocks

            // DIV_APU is updated on DIV bit 4 going low
            bool div_clock = (timer.div >> 4) & 1;
            apu.div_apu += !div_clock && apu.div_clock_last;
            apu.div_clock_last = div_clock;

            bool length_clock = (!(apu.div_apu & 1)) && apu.length_clock_last;
            apu.length_clock_last = apu.div_apu & 1;

            // CH1 trigger
            if (apu.ch1.control & APU_CH_CONTROL_TRIGGER) {
                apu.control |= APU_CONTROL_CH1;

                // Set length timer if expired
                if (apu.ch1.length_timer >= 64) {
                    apu.ch1.length_timer = apu.ch1.length_duty & APU_CH_LD_LENGTH;
                }

                apu.ch1.period_timer = apu.ch1.period;
                uint8_t volume = (apu.ch1.envelope & APU_CH_ENVELOPE_VOL) >> APU_CH_ENVELOPE_VOL_SHIFT;
                apu.ch1.envelope_timer = volume;
                apu.ch1.volume = volume;

                apu.ch1.control &= ~APU_CH_CONTROL_TRIGGER;
            }

            // CH1 execute
            if (apu.control & APU_CONTROL_CH1) {
                apu.ch1.period_timer++;
                // If period timer overflows, execute
                if (apu.ch1.period_timer > 0b11111111111) {
                    // Get the duty sample byte and extract the current bit
                    uint8_t wave_duty = (apu.ch1.length_duty & APU_CH_LD_DUTY) >> APU_CH_LD_DUTY_SHIFT;
                    uint8_t pulse_samples = APU_PULSE_SAMPLES[wave_duty];
                    apu.ch1.sample = (pulse_samples >> apu.ch1.pulse_index) & 1;
                    apu.ch1.sample *= APU_SAMPLE_HIGH;
                    apu.ch1.pulse_index = (apu.ch1.pulse_index + 1) % 8;

                    // Volume
                    apu.ch1.sample /= 16 - apu.ch1.volume;

                    apu.ch1.period_timer = apu.ch1.period;
                }

                // Length
                apu.ch1.length_timer += length_clock && (apu.ch1.control & APU_CH_CONTROL_LENGTH);
                if (apu.ch1.length_timer >= 64) {
                    apu.control &= ~APU_CONTROL_CH1;
                    printf("CH1 OFF!\n");
                }
            }

            // Mix
            float timer_target = (float)1048576 / (float)APU_SAMPLE_RATE;
            if (apu.buffer_index_timer > timer_target) {
                int16_t *left = &apu.buffer[apu.buffer_index];
                int16_t *right = &apu.buffer[apu.buffer_index+1];
                *left = 0;
                *right = 0;

                // Write out samples and pan
                *left += (apu.panning & APU_PAN_LEFT_CH1) ? apu.ch1.sample : 0;
                *right += (apu.panning & APU_PAN_RIGHT_CH1) ? apu.ch1.sample : 0;

                apu.buffer_index_timer -= timer_target;
                apu.buffer_index += 2;
                if (apu.buffer_index >= APU_BUFFER_SIZE*2) {
                    apu.buffer_index = 0;
                    new_buffer = true;
                }
            } else {
                apu.buffer_index_timer++;
            }
        }
    }

    return new_buffer;
}

uint8_t apu_io_read(uint16_t addr) {
    switch (addr) {
        case 0x10: return apu.ch1.sweep | APU_CH1_SWEEP_UNUSED; break;
        case 0x11: return apu.ch1.length_duty | APU_CH_LD_LENGTH; break;
        case 0x12: return apu.ch1.envelope; break;
        case 0x14: return (apu.ch1.control & APU_CH_CONTROL_LENGTH) | ~APU_CH_CONTROL_LENGTH; break;
        case 0x24: return apu.volume_vin; break;
        case 0x25: return apu.panning; break;
        case 0x26: return apu.control | APU_CONTROL_UNUSED; break;
        default:
            return 0xFF;
            break;
    }
}

void apu_io_write(uint16_t addr, uint8_t data) {
    switch (addr) {
        case 0x10: apu.ch1.sweep = data; break;
        case 0x11: apu.ch1.length_duty = data; break;
        case 0x12: apu.ch1.envelope = data; break;
        case 0x13: apu.ch1.period = (apu.ch1.period & APU_CH_PERIOD_HIGH) | data; break;
        case 0x14:
            uint16_t period_high = ((data & APU_CH_CONTROL_PERIOD) << APU_CH_PERIOD_HIGH_SHIFT);
            apu.ch1.period = (apu.ch1.period & APU_CH_PERIOD_LOW) | period_high;
            apu.ch1.control = data;
            break;
        case 0x24: apu.volume_vin = data; break;
        case 0x25: apu.panning = data; break;
        case 0x26: apu.control = (apu.control & ~APU_CONTROL_AUDIO) | (data & APU_CONTROL_AUDIO); break;
        default: break;
    }
}
