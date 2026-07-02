CC ?= cc
CFLAGS ?= -std=c11 -fPIC -Wall -Wextra -Wpedantic -O2
LDFLAGS ?=

BUILD_DIR := build
SRC_DIR := src
TEST_DIR := tests
TARGET := $(BUILD_DIR)/libbarony_bml.so
SOURCES := $(SRC_DIR)/bml_hook.c

.PHONY: all clean test

all: $(TARGET)

$(TARGET): $(SOURCES)
	mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -shared $(LDFLAGS) -o $@ $^

clean:
	rm -rf $(BUILD_DIR)

test: all
	sh $(TEST_DIR)/smoke_ld_preload.sh $(abspath $(TARGET))
