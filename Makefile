# User-selectable platform (default = sdl2)
PLATFORM ?= sdl2

# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -Iinclude -std=c23 -g -O3

ifeq ($(DEBUG_PRINT), 1)
	CFLAGS += -DDEBUG_PRINT=$(DEBUG_PRINT)
endif

# Platform-specific flags
ifeq ($(PLATFORM), null)
	CFLAGS_PLATFORM = $(CFLAGS)
	LDFLAGS_PLATFORM = $(LDFLAGS)
else ifeq ($(PLATFORM), sdl2)
	CFLAGS_PLATFORM = $(CFLAGS) $(shell pkg-config --cflags sdl2)
	LDFLAGS_PLATFORM = $(shell pkg-config --libs sdl2)
else
$(error Unknown PLATFORM '$(PLATFORM)'. Supported: sdl2)
endif

# Directories
SRC_DIR = src
OBJ_DIR = obj
BIN = gbemu

# Platform directories
SRC_DIR_PLATFORM = platform/$(PLATFORM)
OBJ_DIR_PLATFORM = $(OBJ_DIR)/$(PLATFORM)

# Source files
SRCS = $(wildcard $(SRC_DIR)/*.c)
SRCS_PLATFORM = $(wildcard $(SRC_DIR_PLATFORM)/*.c)

OBJS = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRCS))
OBJS_PLATFORM = $(patsubst $(SRC_DIR_PLATFORM)/%.c, $(OBJ_DIR_PLATFORM)/%.o, $(SRCS_PLATFORM))

# Default target
all: $(BIN)

# Link the executable
$(BIN): $(OBJS) $(OBJS_PLATFORM)
	$(CC) -o $@ $^ $(LDFLAGS_PLATFORM)

# Compile main source files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Compile platform source files
$(OBJ_DIR_PLATFORM)/%.o: $(SRC_DIR_PLATFORM)/%.c | $(OBJ_DIR_PLATFORM)
	$(CC) $(CFLAGS_PLATFORM) -c $< -o $@

# Create output directories
$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(OBJ_DIR_PLATFORM):
	mkdir -p $(OBJ_DIR_PLATFORM)

# Clean target
clean:
	rm -rf $(OBJ_DIR) $(BIN)

# Phony targets
.PHONY: all clean
