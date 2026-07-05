CC ?= cc
CFLAGS ?= -std=c11 -fPIC -Wall -Wextra -Wpedantic -O2
LDFLAGS ?=
LDLIBS ?= -ldl

BUILD_DIR := build
SRC_DIR := src
TEST_DIR := tests
BML_PLATFORM ?= linux-x86_64

# Platform boundary:
# - linux-x86_64 is the only buildable native target in this repo state.
# - windows-x86_64 is scaffold-only until bml-win-launcher.exe, barony_bml.dll,
#   a Windows build manifest, and live Windows runtime evidence exist.
LINUX_TARGET := $(BUILD_DIR)/libbarony_bml.so
WINDOWS_TARGET := $(BUILD_DIR)/barony_bml.dll
WINDOWS_LAUNCHER := $(BUILD_DIR)/bml-win-launcher.exe
WINDOWS_ADAPTER_STUB := $(SRC_DIR)/bml_windows_adapter_stub.c

ifeq ($(BML_PLATFORM),linux-x86_64)
TARGET := $(LINUX_TARGET)
SOURCES := $(SRC_DIR)/bml_hook.c
else ifeq ($(BML_PLATFORM),windows-x86_64)
$(error windows-x86_64 native adapter is scaffold-only: requires bml-win-launcher.exe + barony_bml.dll artifacts and live Windows verification before enabling)
else
$(error unsupported BML_PLATFORM '$(BML_PLATFORM)'; supported build target is linux-x86_64; windows-x86_64 is scaffold-only)
endif
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
