#include <stdint.h>
#include <string.h>

#include "emu.h"
#include "mem.h"
#include "cpu.h"
#include "joypad.h"
#include "cartridge.h"
#include "apu.h"
#include "log.h"

void frame_callback(uint8_t *buffer) {
    DEBUG_PRINTF("NEW FRAME\n");
}

void audio_callback(int16_t *buffer, int len) {

}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s rom.gb\n", argv[0]);
        return 1;
    }

    // Get save path
    char save_path[256];
    int i = 0;
    while (argv[1][i] && i < 252) {
        save_path[i] = argv[1][i];
        i++;
    }
    save_path[i++] = '.';
    save_path[i++] = 's';
    save_path[i++] = 'a';
    save_path[i++] = 'v';
    save_path[i++] = 0;

    // Open boot/game roms
    mem_open_bootrom("dmg_boot.bin");
    cartridge_open_rom(argv[1]);
    cartridge_open_ram(save_path);

    // Get the game title out of the cartridge header
    printf("%s %s\n","Cartridge Header Title:", cartridge.title);

    // Attach callbacks
    emu.frame_callback = frame_callback;
    emu.audio_callback = audio_callback;

    emu.running = true;

    // Main loop
    while (emu.running) {
        if (emu.ppu_enabled) {
            emu_run_to(EMU_EVENT_FRAME);
        } else if (emu.apu_enabled) {
            emu_run_to(EMU_EVENT_AUDIO);
        } else {
            emu_run_to(EMU_EVENT_ANY);
        }
    }

    // Append .sav to rom path and save cartridge ram
    printf("Saving cartridge ram\n");
    cartridge_save_ram(save_path);

    return 0;
}
