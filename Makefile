# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -g $(shell pkg-config --cflags --libs sdl3)

# Target executable and source files
TARGET = gbemu
SRCS = main.c mem.c cpu.c ppu.c timer.c joypad.c

# Default target
all:
	$(CC) $(CFLAGS) -o $(TARGET) $(SRCS)

# Clean up
clean:
	rm -f $(TARGET)

# Phony targets
.PHONY: all clean
