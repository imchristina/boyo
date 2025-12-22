# User-selectable frontend (default = sdl2)
FRONTENDS = null sdl2
DEFAULT_FRONTEND = sdl2

# Directories
SRC_DIR = src
INCLUDE_DIR = include
FRONTEND_DIR = frontend
BUILD_DIR = build
LIB = libboyo.a
BIN = boyo

# Compiler and flags
CC = gcc
AR = ar
MAKE = make
CFLAGS = -Wall -Wextra -I$(INCLUDE_DIR) -std=c2x -g -O3

ifdef DEBUG_PRINT
	CFLAGS += -DDEBUG_PRINT=$(DEBUG_PRINT)
endif

ifeq ($(CGB), 1)
	CFLAGS += -DCGB
endif
