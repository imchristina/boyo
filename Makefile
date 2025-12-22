include common.mk

# Source files
SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS = $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o, $(SRCS))

# Default target
all: lib $(DEFAULT_FRONTEND)

lib: $(BUILD_DIR)/$(LIB)

$(FRONTENDS): lib
	$(MAKE) -C $(FRONTEND_DIR)/$@

# Link the library
$(BUILD_DIR)/$(LIB): $(OBJS)
	$(AR) cr $@ $^

# Compile main source files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Create output directories
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Default clean target
clean:
	rm -rf $(BUILD_DIR)

# Phony targets
.PHONY: all lib clean
