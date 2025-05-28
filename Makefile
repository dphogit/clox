CC = gcc

# TODO: Define a .h file to conditionally include depending on debug rule
DEBUG_MACROS = DEBUG_TRACE_EXEC DEBUG_PRINT_CODE
DEBUG_ARGS = $(patsubst %,-D%,$(DEBUG_MACROS))

SRC_DIR = src
BUILD_DIR = build
OBJ_DIR = $(BUILD_DIR)/objs
INCLUDE_DIR = include

TARGET = $(BUILD_DIR)/clox

# src/main.c, src/chunk.c, ... -> build/objs/main.o, build/objs/chunk.o ...
SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRCS))

.PHONY: all clean release debug

BUILD_TYPE ?= debug

ifeq ($(BUILD_TYPE),debug)
	CFLAGS = -Wall -Wextra -I$(INCLUDE_DIR) -g $(DEBUG_ARGS)
	BUILD_LABEL = Debug
else ifeq ($(BUILD_TYPE),release)
	CFLAGS = -Wall -Wextra -I$(INCLUDE_DIR) -O
	BUILD_LABEL = Release
else
	$(error Unknown BUILD_TYPE '$(BUILD_TYPE)'. Use 'debug' or 'release')
endif

all: $(TARGET)
	@echo "$(BUILD_LABEL) build completed: $(TARGET)"

# Link final binary
$(TARGET): $(OBJS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $^

# Compile .c files to .o files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

clean:
	rm -rf $(BUILD_DIR)

debug:
	$(MAKE) BUILD_TYPE=debug

release:
	$(MAKE) BUILD_TYPE=release
