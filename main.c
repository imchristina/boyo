#include <stdint.h>
#include <string.h>
#include <signal.h>
#include <SDL/SDL.h>

#include "main.h"
#include "mem.h"
#include "cpu.h"
#include "ppu.h"
#include "timer.h"

bool emu_running = 1;

int main() {
    // SDL stuff
    SDL_Init(SDL_INIT_VIDEO);
    signal(SIGINT, SIG_DFL); // Return ctrl-c to normal
    SDL_Surface *screen = SDL_SetVideoMode(160, 144, 8, 0);

    // Open boot/game roms
    mem_open_bootrom("dmg_boot.bin");
    mem_open_rom("rom.gb");

    // Get the game title out of the cartridge header
    char header_title[13];
    for (int i = 0x134; i < 0x13E; i++) {
        header_title[i-0x134] = mem_read(i);
    }

    printf("%s %s\n","Cartridge Header Title:", header_title);

    cpu_reset();

    // Main loop
    SDL_Event e;
    uint8_t t = 0; // Time to next instruction
    uint8_t sdl_fb[160*144];
    while (emu_running) {
        // Execute instruction/fetch next
        if (t == 0) {
            cpu_writeback();
            t = cpu_execute();
        } else { // Do IO mapped stuff here
            bool new_frame = ppu_execute(t);
            timer_execute(t);

            if (new_frame) {
                printf("NEW FRAME\n");

                // Convert GB color to SDL 8-bit
                for (int i = 0; i < 160*144; i++) {
                    switch (ppu.fb[i]) {
                        case 0: // White
                            sdl_fb[i] = SDL_MapRGB(screen->format, 255, 255, 255);
                            break;
                        case 1: // Light grey
                            sdl_fb[i] = SDL_MapRGB(screen->format, 170, 170, 170);
                            break;
                        case 2: // Dark grey
                            sdl_fb[i] = SDL_MapRGB(screen->format, 85, 85, 85);
                            break;
                        case 3: // Black
                            sdl_fb[i] = SDL_MapRGB(screen->format, 0, 0, 0);
                            break;
                    }
                }

                memcpy(screen->pixels, sdl_fb, 160*144);
                SDL_UpdateRect(screen, 0, 0, 0, 0);
            }
            t = 0;
        }

        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                emu_running = 0;
            }
        }
    }

    SDL_Quit();
    return 0;
}

void emu_halt() {
    printf("Emulation halting, execution beyond this point invalid\n");
    emu_running = 0;
}
