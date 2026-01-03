#ifndef EMU_H
#define EMU_H

#include <stddef.h>

#define EMU_ROM_SIZE_MIN    32768
#define EMU_ROM_SIZE_MAX    8388608
#define EMU_SAV_SIZE_MAX    0x20000
#define EMU_TITLE_SIZE_MAX  16

#define EMU_EVENT_NONE  0b00
#define EMU_EVENT_FRAME 0b01
#define EMU_EVENT_AUDIO 0b10
#define EMU_EVENT_ANY   0b11

#ifdef CGB
typedef void (*emu_frame_callback_t)(uint16_t *buffer);
#else
typedef void (*emu_frame_callback_t)(uint8_t *buffer);
#endif

typedef void (*emu_audio_callback_t)(int16_t *buffer, int len);

typedef struct {
    emu_frame_callback_t frame_callback;
    emu_audio_callback_t audio_callback;

    bool running;
    bool ppu_enabled;
    bool apu_enabled;
} gb_emu_t;

extern gb_emu_t emu;

int emu_run_to(int mask);

void emu_load_rom(uint8_t *data, size_t size);
void emu_load_sav(uint8_t *data, size_t size);
size_t emu_get_sav_size();
size_t emu_get_title(char *title);

#endif
