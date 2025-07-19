#include <stdint.h>
#include <string.h>
#include <signal.h>
#include <SDL3/SDL.h>

#include "main.h"
#include "mem.h"
#include "cpu.h"
#include "ppu.h"
#include "timer.h"
#include "joypad.h"
#include "log.h"

bool emu_running = 1;

int main() {
    // SDL stuff
    SDL_Init(SDL_INIT_VIDEO);
    signal(SIGINT, SIG_DFL); // Return ctrl-c to normal
    SDL_Window *win = SDL_CreateWindow("Boyo", 160, 144, 0);
    SDL_Surface *screen = SDL_GetWindowSurface(win);

    // Get colors here because MapRGB is slow
    uint32_t sdl_col[4];
    sdl_col[0] = SDL_MapSurfaceRGB(screen, 255, 255, 255);
    sdl_col[1] = SDL_MapSurfaceRGB(screen, 170, 170, 170);
    sdl_col[2] = SDL_MapSurfaceRGB(screen, 85, 85, 85);
    sdl_col[3] = SDL_MapSurfaceRGB(screen, 0, 0, 0);

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

    uint64_t perf_count_freq = SDL_GetPerformanceFrequency();
    uint64_t perf_count_start = SDL_GetPerformanceCounter();
    float target_frametime = 16.666f;

    // Main loop
    SDL_Event e;
    uint32_t sdl_fb[160*144*4];
    while (emu_running) {
        uint8_t t = 0; // Time to next instruction
        bool new_frame = false;

        // Execute instruction/fetch next
        t = cpu_execute();
        cpu_writeback();

        new_frame = ppu_execute(t);
        timer_execute(t);

        if (new_frame) {
            DEBUG_PRINTF("NEW FRAME\n");

            // Convert GB color to SDL 8-bit
            for (int i = 0; i < 160*144; i++) {
                sdl_fb[i] = sdl_col[ppu.fb[i]];
            }

            memcpy(screen->pixels, sdl_fb, 160*144*4);

            SDL_UpdateWindowSurface(win);

            // Limit framerate
            uint64_t perf_count_end = SDL_GetPerformanceCounter();
            float elapsed = (float)((perf_count_end - perf_count_start) * 1000) / perf_count_freq;

            if (elapsed < target_frametime) {
                SDL_Delay((uint32_t)(target_frametime - elapsed));
            }

            perf_count_start = SDL_GetPerformanceCounter();

            while (SDL_PollEvent(&e)) {
                switch (e.type) {
                    case SDL_EVENT_QUIT: emu_running = 0; break;
                    case SDL_EVENT_KEY_DOWN:
                        switch (e.key.key) {
                            case SDLK_L: joypad_down(JOYPAD_BUTTON_A); break;
                            case SDLK_K: joypad_down(JOYPAD_BUTTON_B); break;
                            case SDLK_H: joypad_down(JOYPAD_BUTTON_START); break;
                            case SDLK_G: joypad_down(JOYPAD_BUTTON_SELECT); break;
                            case SDLK_W: joypad_down(JOYPAD_DPAD_UP); break;
                            case SDLK_S: joypad_down(JOYPAD_DPAD_DOWN); break;
                            case SDLK_A: joypad_down(JOYPAD_DPAD_LEFT); break;
                            case SDLK_D: joypad_down(JOYPAD_DPAD_RIGHT); break;
                            default: break;
                        }
                        break;
                    case SDL_EVENT_KEY_UP:
                        switch (e.key.key) {
                            case SDLK_L: joypad_up(JOYPAD_BUTTON_A); break;
                            case SDLK_K: joypad_up(JOYPAD_BUTTON_B); break;
                            case SDLK_H: joypad_up(JOYPAD_BUTTON_START); break;
                            case SDLK_G: joypad_up(JOYPAD_BUTTON_SELECT); break;
                            case SDLK_W: joypad_up(JOYPAD_DPAD_UP); break;
                            case SDLK_S: joypad_up(JOYPAD_DPAD_DOWN); break;
                            case SDLK_A: joypad_up(JOYPAD_DPAD_LEFT); break;
                            case SDLK_D: joypad_up(JOYPAD_DPAD_RIGHT); break;
                            default: break;
                        }
                        break;
                    default: break;
                }
            }
        }


    }

    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}

void emu_halt() {
    printf("Emulation halting, execution beyond this point invalid\n");
    emu_running = 0;
}
