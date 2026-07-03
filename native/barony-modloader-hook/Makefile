CC ?= cc
CFLAGS ?= -std=c11 -fPIC -Wall -Wextra -Wpedantic -O2
LDFLAGS ?=
LDLIBS ?= -ldl

BUILD_DIR := build
SRC_DIR := src
TEST_DIR := tests
TARGET := $(BUILD_DIR)/libbarony_bml.so
SOURCES := $(SRC_DIR)/bml_hook.c
TEST_SYMBOL_PROVIDER := $(BUILD_DIR)/libfake_barony_symbols.so
TEST_SYMBOL_PROVIDER_SOURCES := $(TEST_DIR)/fake_barony_symbols.c
TOOLS_DIR := tools
STATIC_READINESS_MANIFEST := manifests/steam-371970-22630456-linux.json
STATIC_READINESS_EXECUTABLE ?= /home/jerry/.local/share/Steam/steamapps/common/Barony/barony.x86_64

.PHONY: all clean test
.PHONY: static-readiness

all: $(TARGET)

$(TARGET): $(SOURCES)
	mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -shared $(LDFLAGS) -o $@ $^ $(LDLIBS)

$(TEST_SYMBOL_PROVIDER): $(TEST_SYMBOL_PROVIDER_SOURCES)
	mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -shared $(LDFLAGS) -o $@ $^

clean:
	rm -rf $(BUILD_DIR)

test: all $(TEST_SYMBOL_PROVIDER)
	sh $(TEST_DIR)/smoke_ld_preload.sh $(abspath $(TARGET)) $(abspath $(TEST_SYMBOL_PROVIDER))

static-readiness:
	python $(TOOLS_DIR)/analyze_stash_targets.py --manifest $(STATIC_READINESS_MANIFEST) --executable $(STATIC_READINESS_EXECUTABLE)
