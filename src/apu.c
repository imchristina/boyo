#include <stdint.h>
#include <stdio.h>
#include <limits.h>

#include "apu.h"
#include "timer.h"
#include "log.h"
#include "emu.h"

#ifdef CGB
#include "cgb.h"
#endif

#define APU_SAMPLE_HIGH INT16_MAX / 2

#define APU_CONTROL_CH1     0b00000001
#define APU_CONTROL_CH2     0b00000010
#define APU_CONTROL_CH3     0b00000100
#define APU_CONTROL_CH4     0b00001000
#define APU_CONTROL_AUDIO   0b10000000
#define APU_CONTROL_UNUSED  0b01110000

#define APU_PAN_RIGHT_CH1   0b00000001
#define APU_PAN_RIGHT_CH2   0b00000010
#define APU_PAN_RIGHT_CH3   0b00000100
#define APU_PAN_RIGHT_CH4   0b00001000
#define APU_PAN_LEFT_CH1    0b00010000
#define APU_PAN_LEFT_CH2    0b00100000
#define APU_PAN_LEFT_CH3    0b01000000
#define APU_PAN_LEFT_CH4    0b10000000

#define APU_VOLUME_RIGHT        0b00000111
#define APU_VOLUME_VIN_RIGHT    0b00001000
#define APU_VOLUME_LEFT         0b01110000
#define APU_VOLUME_LEFT_SHIFT   4
#define APU_VOLUME_VIN_LEFT     0b10000000

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

#define APU_CH1_SWEEP_STEP          0b00000111
#define APU_CH1_SWEEP_DIRECTION     0b00001000
#define APU_CH1_SWEEP_PACE          0b01110000
#define APU_CH1_SWEEP_PACE_SHIFT    4
#define APU_CH1_SWEEP_UNUSED        0b10000000

#define APU_CH3_DAC_ENABLE          0b10000000
#define APU_CH3_DAC_UNUSED          0b01111111

#define APU_CH3_LEVEL_OUTPUT        0b01100000
#define APU_CH3_LEVEL_OUTPUT_SHIFT  5
#define APU_CH3_LEVEL_UNUSED        0b10011111

#define APU_CH4_RAND_CLK_DIV        0b00000111
#define APU_CH4_RAND_LFSR_WIDTH     0b00001000
#define APU_CH4_RAND_CLK_SEL        0b11110000
#define APU_CH4_RAND_CLK_SEL_SHIFT  4

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
        // Set length timer (always, not just if expired PAN DOCS)
        ch->length_timer = 64 - (ch->length_duty & APU_CH_LD_LENGTH);

        ch->sweep_timer = (ch->sweep & APU_CH1_SWEEP_PACE) >> APU_CH1_SWEEP_PACE_SHIFT;
        ch->period_timer = ch->period;
        ch->envelope_pace = (ch->envelope & APU_CH_ENVELOPE_PACE);
        ch->envelope_dir = (ch->envelope & APU_CH_ENVELOPE_DIR);
        ch->envelope_timer = ch->envelope_pace;
        ch->volume = (ch->envelope & APU_CH_ENVELOPE_VOL) >> APU_CH_ENVELOPE_VOL_SHIFT;

        ch->control &= ~APU_CH_CONTROL_TRIGGER;

        apu.control |= ch_control;

        DEBUG_PRINTF_APU("CH%d ON!\n", ch_num);
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
            ch->sample *= ch->volume;

            ch->period_timer = ch->period;
        }

        // Sweep (CH1)
        if (ch_num == 1) {
            if (ch->sweep & APU_CH1_SWEEP_PACE && apu.sweep_clock) {
                if (ch->sweep_timer <= 0) {
                    uint8_t step = ch->sweep & APU_CH1_SWEEP_STEP;
                    bool direction = ch->sweep & APU_CH1_SWEEP_DIRECTION;
                    uint8_t pace = (ch->sweep & APU_CH1_SWEEP_PACE) >> APU_CH1_SWEEP_PACE_SHIFT;

                    // pow(2, step)
                    int step_pow = 1;
                    for (int i = 0; i < step; i++) {
                        step_pow *= 2;
                    }

                    int period_delta = ch->period / step_pow;

                    if (direction) {
                        if (ch->period > 0) { ch->period -= period_delta; }
                    } else {
                        ch->period += period_delta;
                    }

                    ch->sweep_timer = pace;
                }
                ch->sweep_timer--;
            }

            // Always turn of channel if period overflows
            if (!(ch->sweep & APU_CH1_SWEEP_DIRECTION) && (ch->period > 0b11111111111)) {
                apu.control &= ~ch_control;
                DEBUG_PRINTF_APU("CH%d SWEEP OFF!\n", ch_num);
            }
        }

        // Envelope
        if (ch->envelope_pace && apu.envelope_clock) {
            ch->envelope_timer -= 1;
            if (ch->envelope_timer <= 0) {
                if (ch->envelope_dir) {
                    if (ch->volume < 15) { ch->volume += 1; }
                } else {
                    if (ch->volume > 0) { ch->volume -= 1; }
                }

                ch->envelope_timer = ch->envelope_pace;
            }
        }

        // Envelope DAC disable
        if (!(ch->envelope & (APU_CH_ENVELOPE_VOL | APU_CH_ENVELOPE_DIR))) {
            apu.control &= ~ch_control;
            DEBUG_PRINTF_APU("CH%d DAC OFF!\n", ch_num);
        }

        // Length
        bool length_enable = ch->control & APU_CH_CONTROL_LENGTH;
        ch->length_timer -= apu.length_clock && length_enable;
        if (ch->length_timer == 0 && length_enable) {
            apu.control &= ~ch_control;
            DEBUG_PRINTF_APU("CH%d TIMER OFF!\n", ch_num);
        }
    } else {
        ch->sample = ch->volume / 2;
    }
}

void wave_execute(gb_apu_wave_t *ch) {
    // Trigger
    if (ch->control & APU_CH_CONTROL_TRIGGER) {
        ch->length_timer = 255 - ch->length;

        ch->period_timer = ch->period;

        ch->wave_index = 1;

        ch->control &= ~APU_CH_CONTROL_TRIGGER;

        apu.control |= APU_CONTROL_CH3;

        DEBUG_PRINTF_APU("CH3 ON!\n");
    }

    // Execute
    if (apu.control & APU_CONTROL_CH3) {
        ch->period_timer += 2;
        // If period timer overflows, execute
        if (ch->period_timer > 0b11111111111) {
            // Get the duty sample byte and extract the current bit
            uint8_t wave_samples = ch->wave[ch->wave_index/2];
            int wave_shift = (ch->wave_index & 1) * 4;
            ch->sample = (wave_samples >> wave_shift) & 0b00001111;
            ch->wave_index = (ch->wave_index + 1) % 32;

            // Volume/level
            uint8_t volume = (ch->level & APU_CH3_LEVEL_OUTPUT) >> APU_CH3_LEVEL_OUTPUT_SHIFT;
            if (volume) {
                ch->volume = 15 >> (volume-1);
                ch->sample = ch->sample >> (volume-1);
            } else {
                ch->volume = 0;
                ch->sample = 0;
            }

            ch->period_timer = ch->period;
        }

        // Length
        bool length_enable = ch->control & APU_CH_CONTROL_LENGTH;
        ch->length_timer -= apu.length_clock && length_enable;
        if (ch->length_timer == 0 && length_enable) {
            apu.control &= ~APU_CONTROL_CH3;
            DEBUG_PRINTF_APU("CH3 TIMER OFF!\n");
        }

        // DAC enable
        if (!ch->dac) {
            apu.control &= ~APU_CONTROL_CH3;
            DEBUG_PRINTF_APU("CH3 DAC OFF!\n");
        }
    } else {
        ch->sample = ch->volume / 2;
    }
}

void noise_execute(gb_apu_noise_t *ch) {
    // Trigger
    if (ch->control & APU_CH_CONTROL_TRIGGER) {
        ch->length_timer = 63 - (ch->length & APU_CH_LD_LENGTH);

        ch->envelope_pace = (ch->envelope & APU_CH_ENVELOPE_PACE);
        ch->envelope_dir = (ch->envelope & APU_CH1_SWEEP_DIRECTION);
        ch->envelope_timer = ch->envelope_pace;
        ch->volume = (ch->envelope & APU_CH_ENVELOPE_VOL) >> APU_CH_ENVELOPE_VOL_SHIFT;
        ch->lfsr = 0;

        ch->control &= ~APU_CH_CONTROL_TRIGGER;
        apu.control |= APU_CONTROL_CH4;

        DEBUG_PRINTF_APU("CH4 ON!\n");
    }

    // Execute
    if (apu.control & APU_CONTROL_CH4) {
        // LFSR clock
        uint8_t clock_div = ch->rand & APU_CH4_RAND_CLK_DIV;
        uint8_t clock_shift = (ch->rand & APU_CH4_RAND_CLK_SEL) >> APU_CH4_RAND_CLK_SEL_SHIFT;

        int timer_target;
        if (clock_div == 0) {
            timer_target = 2 << clock_shift;
        } else {
            timer_target = (4 * clock_div) << clock_shift;
        }

        bool lfsr_clock = false;

        if (ch->lfsr_timer >= timer_target) {
            ch->lfsr_timer = 0;
            lfsr_clock = true;
        }

        ch->lfsr_timer++;

        // LFSR
        if (lfsr_clock) {
            bool bit0 = ch->lfsr & 1;
            bool bit1 = (ch->lfsr & 0b10) >> 1;
            bool result = bit0 == bit1;

            ch->lfsr &= ~(1 << 15); // Clear bit
            ch->lfsr |= (1 << 15) * result; // Set bit
            if (ch->rand & APU_CH4_RAND_LFSR_WIDTH) { // Short mode
                ch->lfsr &= ~(1 << 7); // Clear bit
                ch->lfsr |= (1 << 7) * result; // Set bit
            }

            ch->sample = (ch->lfsr & 1) * ch->volume;

            ch->lfsr = ch->lfsr >> 1;
        }

        // Envelope
        if (ch->envelope_pace && apu.envelope_clock) {
            ch->envelope_timer -= 1;
            if (ch->envelope_timer <= 0) {
                if (ch->envelope_dir) {
                    if (ch->volume < 15) { ch->volume += 1; }
                } else {
                    if (ch->volume > 0) { ch->volume -= 1; }
                }

                ch->envelope_timer = ch->envelope_pace;
            }
        }

        // Envelope DAC disable
        if (!(ch->envelope & (APU_CH_ENVELOPE_VOL | APU_CH_ENVELOPE_DIR))) {
            apu.control &= ~APU_CONTROL_CH4;
            DEBUG_PRINTF_APU("CH4 DAC OFF!\n");
        }

        // Length
        bool length_enable = ch->control & APU_CH_CONTROL_LENGTH;
        ch->length_timer -= apu.length_clock && length_enable;
        if (ch->length_timer == 0 && length_enable) {
            apu.control &= ~APU_CONTROL_CH4;
            DEBUG_PRINTF_APU("CH4 TIMER OFF!\n");
        }
    } else {
        ch->sample = ch->volume / 2;
    }
}

bool apu_execute(uint8_t t) {
    bool new_buffer = false;

    // Step on M cycles
    for (int m = 0; m < t/4; m++) {
#ifdef CGB
        static bool half_timer = false;
        if (cgb_speed() == CGB_SPEED_DOUBLE) {
            half_timer = !half_timer;
        } else {
            half_timer = true;
        }
        if (half_timer && (apu.control & APU_CONTROL_AUDIO)) {
#else
        if (apu.control & APU_CONTROL_AUDIO) {
#endif
            // Clocks

            // DIV_APU is updated on DIV bit 4 going low
            int div_shift = 4;
#ifdef CGB
            if (cgb_speed() == CGB_SPEED_DOUBLE) {
                div_shift = 5;
            }
#endif
            apu.div_clock = (timer.div >> div_shift) & 1;
            apu.div_apu += !apu.div_clock && apu.div_clock_last;
            apu.div_clock_last = apu.div_clock;

            apu.length_clock = (!(apu.div_apu & 1)) && apu.length_clock_last;
            apu.length_clock_last = apu.div_apu & 1;

            apu.sweep_clock = (!(apu.div_apu & 0b10)) && apu.sweep_clock_last;
            apu.sweep_clock_last = apu.div_apu & 0b10;

            apu.envelope_clock = (!(apu.div_apu & 0b100)) && apu.envelope_clock_last;
            apu.envelope_clock_last = apu.div_apu & 0b100;

            // Channel execute
            pulse_execute(&apu.ch1, 1);
            pulse_execute(&apu.ch2, 2);
            wave_execute(&apu.ch3);
            noise_execute(&apu.ch4);

            // Mix
            float timer_target = (float)1048576 / (float)EMU_AUDIO_SAMPLE_RATE; // (M cycles)/(audio samples) per second
            timer_target *= 0.963; // WHAT IS THIS MAGIC NUMBER???
            if (apu.buffer_index_timer >= timer_target) {
                // Get samples and convert to output format
                int16_t sample_unit = (APU_SAMPLE_HIGH / 15);
                int16_t ch1_sample = (apu.ch1.sample - (apu.ch1.volume / 2)) * sample_unit;
                int16_t ch2_sample = (apu.ch2.sample - (apu.ch2.volume / 2)) * sample_unit;
                int16_t ch3_sample = (apu.ch3.sample - (apu.ch3.volume / 2)) * sample_unit;
                int16_t ch4_sample = (apu.ch4.sample - (apu.ch4.volume / 2)) * sample_unit;

                // Write out samples and pan
                int16_t *left = &apu.buffer[apu.buffer_index];
                int16_t *right = &apu.buffer[apu.buffer_index+1];
                *left = 0;
                *right = 0;

                *left += (apu.panning & APU_PAN_LEFT_CH1) ? ch1_sample : 0;
                *right += (apu.panning & APU_PAN_RIGHT_CH1) ? ch1_sample : 0;
                *left += (apu.panning & APU_PAN_LEFT_CH2) ? ch2_sample : 0;
                *right += (apu.panning & APU_PAN_RIGHT_CH2) ? ch2_sample : 0;
                *left += (apu.panning & APU_PAN_LEFT_CH3) ? ch3_sample : 0;
                *right += (apu.panning & APU_PAN_RIGHT_CH3) ? ch3_sample : 0;
                *left += (apu.panning & APU_PAN_LEFT_CH4) ? ch4_sample : 0;
                *right += (apu.panning & APU_PAN_RIGHT_CH4) ? ch4_sample : 0;

                // Master volume
                uint8_t volume_left = (apu.volume_vin & APU_VOLUME_LEFT) >> APU_VOLUME_LEFT_SHIFT;
                uint8_t volume_right = apu.volume_vin & APU_VOLUME_RIGHT;
                *left /= 16 - ((volume_left * 2) + 1);
                *right /= 16 - ((volume_right * 2) + 1);

                apu.buffer_index_timer -= timer_target;
                apu.buffer_index += 2;
                if (apu.buffer_index >= EMU_AUDIO_BUFFER_SIZE*2) {
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
        case 0x1A: return apu.ch3.dac | APU_CH3_DAC_UNUSED; break;
        case 0x1C: return apu.ch3.level; break;
        case 0x1E: return (apu.ch3.control & APU_CH_CONTROL_LENGTH) | ~APU_CH_CONTROL_LENGTH; break;
        case 0x21: return apu.ch4.envelope; break;
        case 0x22: return apu.ch4.rand; break;
        case 0x23: return apu.ch4.control | APU_CH_CONTROL_PERIOD | APU_CH_CONTROL_UNUSED; break;
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
        case 0x1A: apu.ch3.dac = data; break;
        case 0x1B: apu.ch3.length = data; break;
        case 0x1C: apu.ch3.level = data; break;
        case 0x1D: apu.ch3.period = (apu.ch3.period & APU_CH_PERIOD_HIGH) | data; break;
        case 0x1E:
            period_high = ((data & APU_CH_CONTROL_PERIOD) << APU_CH_PERIOD_HIGH_SHIFT);
            apu.ch3.period = (apu.ch3.period & APU_CH_PERIOD_LOW) | period_high;
            apu.ch3.control = data;
            break;
        case 0x20: apu.ch4.length = data; break;
        case 0x21: apu.ch4.envelope = data; break;
        case 0x22: apu.ch4.rand = data; break;
        case 0x23: apu.ch4.control = data; break;
        case 0x24: apu.volume_vin = data; break;
        case 0x25: apu.panning = data; break;
        case 0x26: apu.control = (apu.control & ~APU_CONTROL_AUDIO) | (data & APU_CONTROL_AUDIO); break;
        default: break;
    }
}

uint8_t apu_wave_read(uint16_t addr) {
    return apu.ch3.wave[addr-0x30];
}

void apu_wave_write(uint16_t addr, uint8_t data) {
    apu.ch3.wave[addr-0x30] = data;
}
