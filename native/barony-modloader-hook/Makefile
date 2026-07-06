CC ?= cc
CFLAGS ?= -std=c11 -fPIC -Wall -Wextra -Wpedantic -O2
LDFLAGS ?=
LDLIBS ?= -ldl
WINEGCC ?= winegcc
WINE ?= wine
PYTHON ?= python
BML_WINDOWS_CFLAGS ?= -std=c11 -D_DEFAULT_SOURCE -Wall -Wextra -O2

BUILD_DIR := build
SRC_DIR := src
TEST_DIR := tests
BML_PLATFORM ?= linux-x86_64

# Platform boundary:
# - linux-x86_64 remains the verified production native target.
# - windows-x86_64 builds fail-closed adapter artifacts only; the launcher and
#   DLL self-test report unsupported status until live Windows Barony verification
#   exists.
LINUX_TARGET := $(BUILD_DIR)/libbarony_bml.so
WINDOWS_TARGET := $(BUILD_DIR)/barony_bml.dll
WINDOWS_LAUNCHER := $(BUILD_DIR)/bml-win-launcher.exe
WINDOWS_ADAPTER_STUB := $(SRC_DIR)/bml_windows_adapter_stub.c
WINDOWS_LAUNCHER_SOURCE := $(SRC_DIR)/bml_win_launcher.c
WINDOWS_DLL_SPEC := $(BUILD_DIR)/barony_bml.spec

ifeq ($(BML_PLATFORM),linux-x86_64)
TARGETS := $(LINUX_TARGET)
SOURCES := $(SRC_DIR)/bml_hook.c
else ifeq ($(BML_PLATFORM),windows-x86_64)
TARGETS := $(WINDOWS_TARGET) $(WINDOWS_LAUNCHER)
else
$(error unsupported BML_PLATFORM '$(BML_PLATFORM)'; supported targets: linux-x86_64 windows-x86_64)
endif
TEST_SYMBOL_PROVIDER := $(BUILD_DIR)/libfake_barony_symbols.so
TEST_SYMBOL_PROVIDER_SOURCES := $(TEST_DIR)/fake_barony_symbols.c
TOOLS_DIR := tools
STATIC_READINESS_MANIFEST := manifests/steam-371970-22630456-linux.json
STATIC_READINESS_EXECUTABLE ?= /home/jerry/.local/share/Steam/steamapps/common/Barony/barony.x86_64
WINDOWS_SELF_TEST_REPORT := $(BUILD_DIR)/windows-adapter-self-test-report.json

.PHONY: all clean test
.PHONY: static-readiness
.PHONY: check-winegcc check-wine windows-smoke elixir-production-validation

all: $(TARGETS)

$(LINUX_TARGET): $(SRC_DIR)/bml_hook.c
	mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -shared $(LDFLAGS) -o $@ $^ $(LDLIBS)

check-winegcc:
	@command -v $(WINEGCC) >/dev/null 2>&1 || { echo "error: BML_PLATFORM=windows-x86_64 requires winegcc. Install Wine development tools or set WINEGCC=/path/to/winegcc." >&2; exit 127; }

check-wine:
	@command -v $(WINE) >/dev/null 2>&1 || { echo "error: windows-smoke requires wine. Install Wine or set WINE=/path/to/wine." >&2; exit 127; }

$(WINDOWS_DLL_SPEC):
	mkdir -p $(BUILD_DIR)
	printf '%s\n' '@ cdecl bml_windows_adapter_status()' '@ cdecl bml_windows_hook_self_test()' > $@

$(WINDOWS_TARGET): $(WINDOWS_ADAPTER_STUB) $(WINDOWS_DLL_SPEC) | check-winegcc
	mkdir -p $(BUILD_DIR)
	$(WINEGCC) $(BML_WINDOWS_CFLAGS) -shared -mwindows -o $@ $^
	test -f $@ || cp $@.so $@

$(WINDOWS_LAUNCHER): $(WINDOWS_LAUNCHER_SOURCE) | check-winegcc
	mkdir -p $(BUILD_DIR)
	$(WINEGCC) $(BML_WINDOWS_CFLAGS) -mconsole -o $@ $^

$(TEST_SYMBOL_PROVIDER): $(TEST_SYMBOL_PROVIDER_SOURCES)
	mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -shared $(LDFLAGS) -o $@ $^

clean:
	rm -rf $(BUILD_DIR)

test: all $(TEST_SYMBOL_PROVIDER)
	$(PYTHON) $(TEST_DIR)/test_stash_backend_parity.py
	sh $(TEST_DIR)/smoke_ld_preload.sh $(abspath $(LINUX_TARGET)) $(abspath $(TEST_SYMBOL_PROVIDER))

# Opt-in real Steam executable proof for Runebound: Elixirs; intentionally not part of `test`.
elixir-production-validation: $(LINUX_TARGET)
	PYTHON=$(PYTHON) sh $(TEST_DIR)/run_elixir_production_validation.sh $(abspath $(LINUX_TARGET))

windows-smoke: BML_PLATFORM=windows-x86_64
windows-smoke: $(WINDOWS_TARGET) $(WINDOWS_LAUNCHER) | check-wine
	rm -f $(WINDOWS_SELF_TEST_REPORT)
	WINEDEBUG=-all BML_WINDOWS_SELF_TEST_REPORT=$(WINDOWS_SELF_TEST_REPORT) ./$(WINDOWS_LAUNCHER) --self-test
	test -s $(WINDOWS_SELF_TEST_REPORT)

static-readiness:
	$(PYTHON) $(TOOLS_DIR)/analyze_stash_targets.py --manifest $(STATIC_READINESS_MANIFEST) --executable $(STATIC_READINESS_EXECUTABLE)
