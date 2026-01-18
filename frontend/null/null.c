#include <stdint.h>
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "emu.h"
#include "mem.h"
#include "cartridge.h"

#ifdef CGB
void frame_callback(uint16_t *buffer) {
#else
void frame_callback(uint8_t *buffer) {
#endif
    //printf("NEW FRAME\n");
}

void audio_callback(int16_t *buffer, int len) {
    //printf("NEW AUDIO\n");
}

bool open_file(const char *path, uint8_t *destination, size_t size) {
    if (size > 0) {
        FILE *file = fopen(path, "rb");
        if (!file) {
            return false;
        }
        fread(destination, 1, size, file);
        fclose(file);
    }

    return true;
}

bool save_file(const char *path, uint8_t *destination, size_t size) {
    if (size > 0) {
        FILE *file = fopen(path, "wb");
        if (!file) {
            return false;
        }
        fwrite(destination, 1, size, file);
        fclose(file);
    }

    return true;
}

uint8_t bootrom[EMU_BOOTROM_SIZE_MAX];
uint8_t rom[EMU_ROM_SIZE_MAX];
uint8_t sav[EMU_SAV_SIZE_MAX];
char title[EMU_TITLE_SIZE_MAX];

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
    #ifdef CGB
    char *bootrom_path = "cgb_boot.bin";
    #else
    char *bootrom_path = "dmg_boot.bin";
    #endif

    // Open BOOTROM
    if (!open_file(bootrom_path, bootrom, EMU_BOOTROM_SIZE_MAX)) {
        printf("Could not open boot rom %s\n", argv[1]);
    }

    // Open ROM
    if (!open_file(argv[1], rom, EMU_ROM_SIZE_MAX)) {
        printf("Could not open cartridge rom %s\n", argv[1]);
    }

    // Open SAV (if it exists)
    open_file(save_path, sav, EMU_SAV_SIZE_MAX);

    emu_load_bootrom(bootrom, EMU_BOOTROM_SIZE_MAX);
    emu_load_rom(rom, EMU_ROM_SIZE_MAX);
    emu_load_sav(sav, EMU_SAV_SIZE_MAX);

    // Get the game title out of the cartridge header
    emu_get_title(title);
    printf("%s %s\n","Cartridge Header Title:", title);

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

    // Save cartridge ram
    printf("Saving cartridge ram\n");
    if (!save_file(save_path, sav, emu_get_sav_size())) {
        printf("Could not open cartridge save %s\n", save_path);
    };

    return 0;
}
