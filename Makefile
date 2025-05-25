CC = gcc
CFLAGS = -g -Wall -Werror

BUILD_DIR = build

TARGET = $(BUILD_DIR)/clox

all: src/main.c
	mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $(TARGET) src/main.c

.PHONY: clean
clean:
	rm $(TARGET)
