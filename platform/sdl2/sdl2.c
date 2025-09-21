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
uint64_t perf_count_target = 0;
const double target_frametime = 1000.0/60.0;

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
            case SDL_CONTROLLERBUTTONDOWN:
                switch (e.cbutton.button) {
                    case SDL_CONTROLLER_BUTTON_A: joypad_down(JOYPAD_BUTTON_A); break;
                    case SDL_CONTROLLER_BUTTON_B: joypad_down(JOYPAD_BUTTON_B); break;
                    case SDL_CONTROLLER_BUTTON_START: joypad_down(JOYPAD_BUTTON_START); break;
                    case SDL_CONTROLLER_BUTTON_BACK: joypad_down(JOYPAD_BUTTON_SELECT); break;
                    case SDL_CONTROLLER_BUTTON_DPAD_UP: joypad_down(JOYPAD_DPAD_UP); break;
                    case SDL_CONTROLLER_BUTTON_DPAD_DOWN: joypad_down(JOYPAD_DPAD_DOWN); break;
                    case SDL_CONTROLLER_BUTTON_DPAD_LEFT: joypad_down(JOYPAD_DPAD_LEFT); break;
                    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: joypad_down(JOYPAD_DPAD_RIGHT); break;
                }
                break;
            case SDL_CONTROLLERBUTTONUP:
                switch (e.cbutton.button) {
                    case SDL_CONTROLLER_BUTTON_A: joypad_up(JOYPAD_BUTTON_A); break;
                    case SDL_CONTROLLER_BUTTON_B: joypad_up(JOYPAD_BUTTON_B); break;
                    case SDL_CONTROLLER_BUTTON_START: joypad_up(JOYPAD_BUTTON_START); break;
                    case SDL_CONTROLLER_BUTTON_BACK: joypad_up(JOYPAD_BUTTON_SELECT); break;
                    case SDL_CONTROLLER_BUTTON_DPAD_UP: joypad_up(JOYPAD_DPAD_UP); break;
                    case SDL_CONTROLLER_BUTTON_DPAD_DOWN: joypad_up(JOYPAD_DPAD_DOWN); break;
                    case SDL_CONTROLLER_BUTTON_DPAD_LEFT: joypad_up(JOYPAD_DPAD_LEFT); break;
                    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: joypad_up(JOYPAD_DPAD_RIGHT); break;
                }
            default: break;
        }
    }
}

bool skip_frame = false;

void limit_framerate(double target_frametime) {
    perf_count_target += (uint64_t)(target_frametime * perf_count_freq / 1000.0);

    uint64_t perf_count_end = SDL_GetPerformanceCounter();

    if (perf_count_end < perf_count_target) {
        double ms_to_wait = (perf_count_target - perf_count_end) * 1000.0 / perf_count_freq;
        SDL_Delay((uint32_t)ms_to_wait);
    } else {
        perf_count_target = perf_count_end;
    }

    perf_count_start = SDL_GetPerformanceCounter();
}

#ifdef CGB
void frame_callback(uint16_t *buffer) {
    DEBUG_PRINTF("NEW FRAME\n");

    // Convert RGB555 color to SDL 32-bit
    SDL_ConvertPixels(160, 144,
                      SDL_PIXELFORMAT_BGR555, buffer, 160 * 2,
                      surface->format->format, surface->pixels, surface->pitch);

    if (!skip_frame) {
        SDL_UpdateWindowSurface(win);
        limit_framerate(target_frametime);
    } else {
        skip_frame = false;
    }
}
#else
void frame_callback(uint8_t *buffer) {
    DEBUG_PRINTF("NEW FRAME\n");

    static uint32_t sdl_fb[160*144];

    // Convert GB color to SDL 32-bit
    for (int i = 0; i < 160*144; i++) {
        sdl_fb[i] = sdl_col[buffer[i]];
    }

    memcpy(surface->pixels, sdl_fb, 160*144*4);

    if (!skip_frame) {
        SDL_UpdateWindowSurface(win);
        limit_framerate(target_frametime);
    } else {
        skip_frame = false;
    }
}
#endif

void audio_callback(int16_t *buffer, int len) {
    bool underrun = SDL_GetQueuedAudioSize(audio_dev) < (len * sizeof(int16_t));
    bool overrun = SDL_GetQueuedAudioSize(audio_dev) > (len * sizeof(int16_t) * 8);

    if (underrun) { DEBUG_PRINTF("AUDIO UNDERRUN %d\n", SDL_GetQueuedAudioSize(audio_dev)); }
    if (overrun) { DEBUG_PRINTF("AUDIO OVERRUN %d\n", SDL_GetQueuedAudioSize(audio_dev)); }

    if (!overrun) {
        if (SDL_QueueAudio(audio_dev, buffer, len * sizeof(int16_t)) < 0) {
            printf("SDL_QueueAudio error: %s\n", SDL_GetError());
        }
    }

    if (underrun) {
        skip_frame = true;
    }

    if (!underrun && !emu.ppu_enabled) {
        limit_framerate(1000 * (len/2.0)/(double)audiospec_have.freq);
    }
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s rom.gb\n", argv[0]);
        return 1;
    }

    // Initialize SDL/Window/Surface
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER);
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

    // Initialize SDL controller
    SDL_GameController *controller = NULL;
    if (SDL_IsGameController(0)) { // 0 = first joystick index
        controller = SDL_GameControllerOpen(0);
    }

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
#ifdef CGB
    mem_open_bootrom("cgb_boot.bin");
#else
    mem_open_bootrom("dmg_boot.bin");
#endif
    cartridge_open_rom(argv[1]);
    cartridge_open_ram(save_path);

    // Get the game title out of the cartridge header
    printf("%s %s\n","Cartridge Header Title:", cartridge.title);

    // Initialize the framerate limiter
    perf_count_freq = SDL_GetPerformanceFrequency();
    perf_count_start = SDL_GetPerformanceCounter();
    perf_count_target = SDL_GetPerformanceCounter();

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

        process_events();
    }

    // Append .sav to rom path and save cartridge ram
    printf("Saving cartridge ram\n");
    cartridge_save_ram(save_path);

    SDL_DestroyWindow(win);
    SDL_CloseAudioDevice(audio_dev);
    if (controller) SDL_GameControllerClose(controller);
    SDL_Quit();
    return 0;
}
