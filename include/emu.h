#ifndef EMU_H
#define EMU_H

#define EMU_EVENT_NONE  0b00
#define EMU_EVENT_FRAME 0b01
#define EMU_EVENT_AUDIO 0b10

typedef void (*emu_frame_callback_t)();
typedef void (*emu_audio_callback_t)(int16_t *buffer, int len);

typedef struct {
    emu_frame_callback_t frame_callback;
    emu_audio_callback_t audio_callback;
} gb_emu_t;

extern gb_emu_t emu;

int emu_execute();
int emu_run_to(int mask);

#endif
