#include <stdint.h>
#include <string.h>
#include <SDL/SDL.h>

#include "main.h"
#include "mem.h"
#include "cpu.h"

bool emu_running = 1;

int main() {
    // SDL stuff
    //SDL_Init(SDL_INIT_VIDEO);
    //SDL_Surface *screen = SDL_SetVideoMode(256, 200, 8, 0);

    /*uint8_t *pixels = (uint8_t *)screen->pixels;
    for (int y = 0; y < screen->h; y++) {
        for (int x = 0; x < screen->w; x++) {
            pixels[y * screen->w + x] = SDL_MapRGB(screen->format, x, y, 255);
        }
    }*/

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
    //SDL_Event e;
    uint8_t t = 0; // Time to next instruction
    while (emu_running) {
        // Execute instruction/fetch next
        if (t == 0) {
            cpu_writeback();
            cpu_execute();
        } else { // Do IO mapped stuff here
            t = 0;
        }
    }

    //screen->pixels = mem_vram();
    //SDL_UpdateRect(screen, 0, 0, 0, 0);

    /*bool exit = 0;
    while (!exit) {
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT || e.type == SDL_KEYDOWN) {
                exit = 1;
            }
        }
    }

    SDL_Quit();*/
    return 0;
}

void emu_halt() {
    printf("Emulation halting, execution beyond this point invalid\n");
    emu_running = 0;
}
