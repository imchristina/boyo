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
#define APU_PAN_RIGHT_CH2   0b00000010
#define APU_PAN_LEFT_CH1    0b00010000
#define APU_PAN_LEFT_CH2    0b00100000

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

void pulse_execute(gb_apu_pulse_t *ch, int ch_num) {
    uint8_t ch_control = (ch_num == 1) ? APU_CONTROL_CH1 : APU_CONTROL_CH2;

    // Trigger
    if (ch->control & APU_CH_CONTROL_TRIGGER) {
        // Set length timer if expired
        if (ch->length_timer >= 64) {
            ch->length_timer = ch->length_duty & APU_CH_LD_LENGTH;
        }

        ch->period_timer = ch->period;
        ch->envelope_timer = (ch->envelope & APU_CH_ENVELOPE_PACE);
        ch->volume = (ch->envelope & APU_CH_ENVELOPE_VOL) >> APU_CH_ENVELOPE_VOL_SHIFT;

        ch->control &= ~APU_CH_CONTROL_TRIGGER;

        apu.control |= ch_control;

        printf("CH%d ON!\n", ch_num);
    }

    // Execute
    if (apu.control & ch_control) {
        ch->period_timer++;
        // If period timer overflows, execute
        if (ch->period_timer > 0b11111111111) {
            // Get the duty sample byte and extract the current bit
            uint8_t wave_duty = (ch->length_duty & APU_CH_LD_DUTY) >> APU_CH_LD_DUTY_SHIFT;
            uint8_t pulse_samples = APU_PULSE_SAMPLES[wave_duty];
            ch->sample = (pulse_samples >> ch->pulse_index) & 1;
            ch->pulse_index = (ch->pulse_index + 1) % 8;

            // Volume
            ch->sample *= ch->volume * (APU_SAMPLE_HIGH / 15);

            ch->period_timer = ch->period;
        }

        // Envelope
        if ((ch->envelope & APU_CH_ENVELOPE_PACE) && apu.envelope_clock) {
            ch->envelope_timer -= 1;
            if (ch->envelope_timer <= 0) {
                if (ch->envelope & APU_CH_ENVELOPE_DIR) {
                    if (ch->volume < 15) { ch->volume += 1; }
                } else {
                    if (ch->volume > 0) { ch->volume -= 1; }
                }

                if (ch->volume <= 0) {
                    apu.control &= ~ch_control;
                    printf("CH%d ENVELOPE OFF!\n", ch_num);
                } else {
                    ch->envelope_timer = (ch->envelope & APU_CH_ENVELOPE_PACE);
                }
            }
        }

        // Length
        ch->length_timer += apu.length_clock && (ch->control & APU_CH_CONTROL_LENGTH);
        if (ch->length_timer >= 64) {
            apu.control &= ~ch_control;
            printf("CH%d TIMER OFF!\n", ch_num);
        }
        //printf("CH%d IS ON! %d\n", ch_num, ch->envelope & APU_CH_ENVELOPE_PACE);
    } else {
        ch->sample = 0;
    }
}

bool apu_execute(uint8_t t) {
    bool new_buffer = false;

    // Step on M cycles
    for (int m = 0; m < t/4; m++) {
        if (apu.control & APU_CONTROL_AUDIO) {
            // Clocks

            // DIV_APU is updated on DIV bit 4 going low
            apu.div_clock = (timer.div >> 4) & 1;
            apu.div_apu += !apu.div_clock && apu.div_clock_last;
            apu.div_clock_last = apu.div_clock;

            apu.length_clock = (!(apu.div_apu & 1)) && apu.length_clock_last;
            apu.length_clock_last = apu.div_apu & 1;

            apu.sweep_clock = (!(apu.div_apu & 0b10)) && apu.sweep_clock_last;
            apu.sweep_clock_last = apu.div_apu & 0b10;

            apu.envelope_clock = (!(apu.div_apu & 0b100)) && apu.envelope_clock_last;
            apu.envelope_clock_last = apu.div_apu & 0b100;

            // CH1/CH2 execute
            pulse_execute(&apu.ch1, 1);
            pulse_execute(&apu.ch2, 2);

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
                *left += (apu.panning & APU_PAN_LEFT_CH2) ? apu.ch2.sample : 0;
                *right += (apu.panning & APU_PAN_RIGHT_CH2) ? apu.ch2.sample : 0;

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

bool apu_enabled() {
    return apu.control | APU_CONTROL_AUDIO;
}

uint8_t apu_io_read(uint16_t addr) {
    switch (addr) {
        case 0x10: return apu.ch1.sweep | APU_CH1_SWEEP_UNUSED; break;
        case 0x11: return apu.ch1.length_duty | APU_CH_LD_LENGTH; break;
        case 0x12: return apu.ch1.envelope; break;
        case 0x14: return (apu.ch1.control & APU_CH_CONTROL_LENGTH) | ~APU_CH_CONTROL_LENGTH; break;
        case 0x16: return apu.ch2.length_duty | APU_CH_LD_LENGTH; break;
        case 0x17: return apu.ch2.envelope; break;
        case 0x19: return (apu.ch2.control & APU_CH_CONTROL_LENGTH) | ~APU_CH_CONTROL_LENGTH; break;
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
        case 0x16: apu.ch2.length_duty = data; break;
        case 0x17: apu.ch2.envelope = data; break;
        case 0x18: apu.ch2.period = (apu.ch2.period & APU_CH_PERIOD_HIGH) | data; break;
        case 0x19:
            period_high = ((data & APU_CH_CONTROL_PERIOD) << APU_CH_PERIOD_HIGH_SHIFT);
            apu.ch2.period = (apu.ch2.period & APU_CH_PERIOD_LOW) | period_high;
            apu.ch2.control = data;
            break;
        case 0x24: apu.volume_vin = data; break;
        case 0x25: apu.panning = data; break;
        case 0x26: apu.control = (apu.control & ~APU_CONTROL_AUDIO) | (data & APU_CONTROL_AUDIO); break;
        default: break;
    }
}
