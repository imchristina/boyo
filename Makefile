# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -std=c23 -g $(shell pkg-config --cflags --libs sdl2)

# Target executable and source files
TARGET = gbemu
SRCS = main.c mem.c cpu.c ppu.c timer.c joypad.c serial.c cartridge.c apu.c

# Default target
all:
	$(CC) $(CFLAGS) -o $(TARGET) $(SRCS)

# Clean up
clean:
	rm -f $(TARGET)

# Phony targets
.PHONY: all clean
