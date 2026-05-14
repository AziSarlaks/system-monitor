CC ?= gcc
PKG_CONFIG ?= pkg-config

APP_NAME = system-monitor-app
BUILD_DIR = build
SRC_DIR = src
BACKEND_DIR = ../backend/src

CFLAGS = -Wall -Wextra -O2 -g -std=c11 -D_GNU_SOURCE \
         -I$(BACKEND_DIR) \
         $(shell $(PKG_CONFIG) --cflags gtk+-3.0)
LDFLAGS = $(shell $(PKG_CONFIG) --libs gtk+-3.0) -lm

OBJECTS = \
	$(BUILD_DIR)/$(SRC_DIR)/main.o \
	$(BUILD_DIR)/backend/proc_parser.o \
	$(BUILD_DIR)/backend/history.o
TARGET = $(BUILD_DIR)/$(APP_NAME)

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $@ $(LDFLAGS)

$(BUILD_DIR)/$(SRC_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/backend/%.o: $(BACKEND_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

run: $(TARGET)
	./$(TARGET)

clean:
	rm -rf $(BUILD_DIR)

check-deps:
	@$(PKG_CONFIG) --exists gtk+-3.0 || \
		(echo "GTK3 development files are missing. Install: sudo apt install libgtk-3-dev" && exit 1)
	@echo "GTK3 is available: $$($(PKG_CONFIG) --modversion gtk+-3.0)"

.PHONY: all run clean check-deps
