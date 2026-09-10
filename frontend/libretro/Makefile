PROJECT_ROOT = ../..

include $(PROJECT_ROOT)/common.mk

# Directories
FRONTEND_SRC_DIR = .
FRONTEND_BUILD_DIR = $(PROJECT_ROOT)/$(BUILD_DIR)/libretro
FRONTEND_BIN = $(PROJECT_ROOT)/$(BUILD_DIR)/$(BIN)_libretro.so

# Compiler and flags
FRONTEND_CFLAGS = -shared -fPIC -I$(PROJECT_ROOT)/$(INCLUDE_DIR)
FRONTEND_LDFLAGS = -shared -fPIC

# Source files
FRONTEND_SRCS = $(wildcard $(FRONTEND_SRC_DIR)/*.c)
FRONTEND_OBJS = $(patsubst $(FRONTEND_SRC_DIR)/%.c, $(FRONTEND_BUILD_DIR)/%.o, $(FRONTEND_SRCS))

# Default target
all: $(FRONTEND_BIN)

# Link the executable
$(FRONTEND_BIN): $(FRONTEND_OBJS)
	$(CC) -o $@ $^ $(PROJECT_ROOT)/$(BUILD_DIR)/$(LIB) $(FRONTEND_LDFLAGS)

# Compile platform source files
$(FRONTEND_BUILD_DIR)/%.o: $(FRONTEND_SRC_DIR)/%.c | $(FRONTEND_BUILD_DIR)
	$(CC) $(FRONTEND_CFLAGS) -c $< -o $@

# Create output directories
$(FRONTEND_BUILD_DIR):
	mkdir -p $(FRONTEND_BUILD_DIR)

# Phony targets
.PHONY: all
