#include <stdint.h>

#include "emu.h"
#include "mem.h"
#include "cpu.h"
#include "ppu.h"
#include "timer.h"
#include "joypad.h"
#include "serial.h"
#include "cartridge.h"
#include "apu.h"
#include "log.h"

#ifdef CGB
#include "cgb.h"
#endif

gb_emu_t emu = {};

int emu_execute() {
    uint8_t t = cpu_execute();

    // CPU writeback SHOULD be done on the last T cycle, but that breaks a lot of timings.
    // In the mean time, we do it immediately after CPU fetch/execute
    // TODO Figure out why this is
    cpu_writeback();

    bool new_frame = ppu_execute(t);
    bool new_audio = apu_execute(t);
    timer_execute(t);
    serial_execute(t);

#ifdef CGB
    cgb_execute(t);
#endif

    int result = EMU_EVENT_NONE;

    if (new_frame) {
        if (emu.frame_callback != 0) { emu.frame_callback(ppu.fb); }
        result |= EMU_EVENT_FRAME;
    }

    if (new_audio) {
        if (emu.audio_callback != 0) { emu.audio_callback(apu.buffer, APU_BUFFER_SIZE*2); }
        result |= EMU_EVENT_AUDIO;
    }

    emu.ppu_enabled = ppu_enabled();
    emu.apu_enabled = apu_enabled();

    return result;
}

// Run until an event is triggered
// Any event will cause execution to stop
// The event bits flipped during execution are returned
int emu_run_to(int mask) {
    int result = EMU_EVENT_NONE;

    while (!(result & mask) && emu.running) {
        result |= emu_execute();
    }

    return result;
}
