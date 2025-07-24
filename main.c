#include <stdint.h>
#include <string.h>
#include <signal.h>
#include <SDL2/SDL.h>

#include "main.h"
#include "mem.h"
#include "cpu.h"
#include "ppu.h"
#include "timer.h"
#include "joypad.h"
#include "serial.h"
#include "cartridge.h"
#include "log.h"

bool emu_running = 1;

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s rom.gb\n", argv[0]);
        return 1;
    }

    // SDL stuff
    SDL_Init(SDL_INIT_VIDEO);
    signal(SIGINT, SIG_DFL); // Return ctrl-c to normal
    SDL_Window *win = SDL_CreateWindow("Boyo", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 160, 144, 0);
    SDL_Surface *screen = SDL_GetWindowSurface(win);

    // Get colors here because MapRGB is slow
    uint32_t sdl_col[4];
    sdl_col[0] = SDL_MapRGB(screen->format, 255, 255, 255);
    sdl_col[1] = SDL_MapRGB(screen->format, 170, 170, 170);
    sdl_col[2] = SDL_MapRGB(screen->format, 85, 85, 85);
    sdl_col[3] = SDL_MapRGB(screen->format, 0, 0, 0);

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
        serial_execute(t);

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
                    case SDL_QUIT: emu_running = 0; break;
                    case SDL_KEYDOWN:
                        switch (e.key.keysym.sym) {
                            case SDLK_l: joypad_down(JOYPAD_BUTTON_A); break;
                            case SDLK_k: joypad_down(JOYPAD_BUTTON_B); break;
                            case SDLK_h: joypad_down(JOYPAD_BUTTON_START); break;
                            case SDLK_g: joypad_down(JOYPAD_BUTTON_SELECT); break;
                            case SDLK_w: joypad_down(JOYPAD_DPAD_UP); break;
                            case SDLK_s: joypad_down(JOYPAD_DPAD_DOWN); break;
                            case SDLK_a: joypad_down(JOYPAD_DPAD_LEFT); break;
                            case SDLK_d: joypad_down(JOYPAD_DPAD_RIGHT); break;
                            default: break;
                        }
                        break;
                    case SDL_KEYUP:
                        switch (e.key.keysym.sym) {
                            case SDLK_l: joypad_up(JOYPAD_BUTTON_A); break;
                            case SDLK_k: joypad_up(JOYPAD_BUTTON_B); break;
                            case SDLK_h: joypad_up(JOYPAD_BUTTON_START); break;
                            case SDLK_g: joypad_up(JOYPAD_BUTTON_SELECT); break;
                            case SDLK_w: joypad_up(JOYPAD_DPAD_UP); break;
                            case SDLK_s: joypad_up(JOYPAD_DPAD_DOWN); break;
                            case SDLK_a: joypad_up(JOYPAD_DPAD_LEFT); break;
                            case SDLK_d: joypad_up(JOYPAD_DPAD_RIGHT); break;
                            case SDLK_ESCAPE: emu_running = 0;
                            default: break;
                        }
                        break;
                    default: break;
                }
            }
        }
    }

    // Append .sav to rom path and save cartridge ram
    printf("Saving cartridge ram\n");
    cartridge_save_ram(save_path);

    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}

void emu_halt() {
    printf("Emulation halting, execution beyond this point invalid\n");
    emu_running = 0;
}
