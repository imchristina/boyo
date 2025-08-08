#include <stdint.h>
#include <string.h>
#include <signal.h>
#include <SDL2/SDL.h>

#include "emu.h"
#include "mem.h"
#include "cpu.h"
#include "joypad.h"
#include "cartridge.h"
#include "apu.h"
#include "log.h"

SDL_Window *win;
SDL_Surface *surface;
SDL_AudioSpec audiospec_want;
SDL_AudioSpec audiospec_have;
SDL_AudioDeviceID audio_dev;

uint32_t sdl_col[4];

uint64_t perf_count_freq = 0;
uint64_t perf_count_start = 0;
const float target_frametime = 16.666f;

void emu_halt(int sig) {
    printf("Emulation halting: %i\n", sig);
    emu.running = 0;
}

void process_events() {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        switch (e.type) {
            case SDL_QUIT: emu.running = 0; break;
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
                    case SDLK_ESCAPE: emu.running = 0;
                    default: break;
                }
                break;
            default: break;
        }
    }
}

void frame_callback(uint8_t *buffer) {
    DEBUG_PRINTF("NEW FRAME\n");

    static uint32_t sdl_fb[160*144*4];

    // Convert GB color to SDL 8-bit
    for (int i = 0; i < 160*144; i++) {
        sdl_fb[i] = sdl_col[buffer[i]];
    }

    memcpy(surface->pixels, sdl_fb, 160*144*4);

    SDL_UpdateWindowSurface(win);

    // Limit framerate
    uint64_t perf_count_end = SDL_GetPerformanceCounter();
    float elapsed = (float)((perf_count_end - perf_count_start) * 1000) / perf_count_freq;

    if (elapsed < target_frametime) {
        SDL_Delay((uint32_t)(target_frametime - elapsed));
    }

    perf_count_start = SDL_GetPerformanceCounter();

    process_events();
}

void audio_callback(int16_t *buffer, int len) {
    bool underrun = SDL_GetQueuedAudioSize(audio_dev) < (len * sizeof(int16_t));
    bool overrun = SDL_GetQueuedAudioSize(audio_dev) > (len * sizeof(int16_t) * 2);

    if (underrun) { printf("UNDERRUN %d\n", SDL_GetQueuedAudioSize(audio_dev)); }
    if (overrun) { printf("OVERRUN %d\n", SDL_GetQueuedAudioSize(audio_dev)); }

    if (!overrun) {
        if (SDL_QueueAudio(audio_dev, buffer, len * sizeof(int16_t)) < 0) {
            printf("SDL_QueueAudio error: %s\n", SDL_GetError());
        }
    }

    if (underrun) {
        emu_run_to(EMU_EVENT_AUDIO);
    }
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s rom.gb\n", argv[0]);
        return 1;
    }

    // Initialize SDL/Window/Surface
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);
    win = SDL_CreateWindow("Boyo", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 160, 144, 0);
    surface = SDL_GetWindowSurface(win);

    // Initialize SDL audio
    audiospec_want.freq = APU_SAMPLE_RATE;
    audiospec_want.format = AUDIO_S16SYS;
    audiospec_want.channels = 2;
    audiospec_want.samples = APU_BUFFER_SIZE;
    audiospec_want.callback = NULL;
    audio_dev = SDL_OpenAudioDevice(NULL, 0, &audiospec_want, &audiospec_have, 0);
    if (!audio_dev) {
        fprintf(stderr, "Failed to open audio: %s\n", SDL_GetError());
        return 1;
    }
    SDL_PauseAudioDevice(audio_dev, 0);

    // Attach termination signals to handler
    signal(SIGINT, emu_halt);
    signal(SIGTERM, emu_halt);

    // Initialize the SDL color palette
    sdl_col[0] = SDL_MapRGB(surface->format, 255, 255, 255);
    sdl_col[1] = SDL_MapRGB(surface->format, 170, 170, 170);
    sdl_col[2] = SDL_MapRGB(surface->format, 85, 85, 85);
    sdl_col[3] = SDL_MapRGB(surface->format, 0, 0, 0);

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

    // Initialize the framerate limiter
    perf_count_freq = SDL_GetPerformanceFrequency();
    perf_count_start = SDL_GetPerformanceCounter();

    // Attach callbacks
    emu.frame_callback = frame_callback;
    emu.audio_callback = audio_callback;

    emu.running = true;

    // Main loop
    while (emu.running) {
        emu_run_to(EMU_EVENT_FRAME);
    }

    // Append .sav to rom path and save cartridge ram
    printf("Saving cartridge ram\n");
    cartridge_save_ram(save_path);

    SDL_DestroyWindow(win);
    SDL_CloseAudioDevice(audio_dev);
    SDL_Quit();
    return 0;
}
