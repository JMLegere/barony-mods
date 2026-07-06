#!/usr/bin/env sh
set -eu

HOOK_LIBRARY=${1:?usage: run_elixir_production_validation.sh /absolute/path/to/libbarony_bml.so}
ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
REPO_ROOT=$(CDPATH= cd -- "$ROOT_DIR/../.." && pwd)
PYTHON=${PYTHON:-python}
TIMEOUT_DURATION=${BML_ELIXIR_PRODUCTION_VALIDATION_TIMEOUT:-25s}
ASSERT_HELPER="$ROOT_DIR/tests/assert_elixir_production_validation.py"
STATIC_HOOK_MANIFEST="$ROOT_DIR/manifests/steam-371970-22630456-linux.json"
RUNES_PACKAGE_PATH="$REPO_ROOT/mods/runebound-elixirs/bml-package.json"

case "$HOOK_LIBRARY" in
  /*) ;;
  *) HOOK_LIBRARY=$(CDPATH= cd -- "$(dirname -- "$HOOK_LIBRARY")" && pwd)/$(basename -- "$HOOK_LIBRARY") ;;
esac

fail() {
  echo "error: $*" >&2
  exit 1
}

skip() {
  echo "skip: $*" >&2
  exit 77
}

resolve_configured_barony_executable() {
  "$PYTHON" - "$REPO_ROOT/barony-mods.toml" <<'PY'
import os
import pathlib
import re
import sys

config_path = pathlib.Path(sys.argv[1])
install_paths = []
if config_path.is_file():
    text = config_path.read_text(encoding="utf-8")
    match = re.search(r"install_paths\s*=\s*\[(.*?)\]", text, re.S)
    if match:
        install_paths.extend(re.findall(r'"([^"]+)"', match.group(1)))

for fallback in (
    "~/.local/share/Steam/steamapps/common/Barony",
    "~/.steam/steam/steamapps/common/Barony",
):
    if fallback not in install_paths:
        install_paths.append(fallback)

for raw in install_paths:
    executable = pathlib.Path(raw).expanduser() / "barony.x86_64"
    if executable.is_file() and os.access(executable, os.X_OK):
        print(executable.resolve())
        raise SystemExit(0)
raise SystemExit(1)
PY
}

requested_barony_executable=${BML_BARONY_EXECUTABLE:-${BARONY_EXECUTABLE:-}}
if [ -n "$requested_barony_executable" ]; then
  case "$requested_barony_executable" in
    */barony.x86_64) candidate_barony_executable=$requested_barony_executable ;;
    *) candidate_barony_executable=$requested_barony_executable/barony.x86_64 ;;
  esac
  if [ -f "$candidate_barony_executable" ] && [ -x "$candidate_barony_executable" ]; then
    candidate_barony_dir=$(CDPATH= cd -- "$(dirname -- "$candidate_barony_executable")" && pwd)
    BARONY_EXECUTABLE=$candidate_barony_dir/$(basename -- "$candidate_barony_executable")
  else
    fail "configured Barony executable is missing or not executable: $candidate_barony_executable"
  fi
elif BARONY_EXECUTABLE=$(resolve_configured_barony_executable); then
  :
else
  skip "real Steam Barony executable was not found in barony-mods.toml install_paths; set BML_BARONY_EXECUTABLE=/path/to/barony.x86_64 to run production validation"
fi

[ -f "$HOOK_LIBRARY" ] || fail "hook library is missing: $HOOK_LIBRARY"
[ -f "$STATIC_HOOK_MANIFEST" ] || fail "hook manifest is missing: $STATIC_HOOK_MANIFEST"
[ -f "$RUNES_PACKAGE_PATH" ] || fail "Runebound: Elixirs package is missing: $RUNES_PACKAGE_PATH"
command -v timeout >/dev/null 2>&1 || fail "GNU coreutils timeout is required for bounded production validation runs"
[ -f "$ASSERT_HELPER" ] || fail "production validation assertion helper is missing: $ASSERT_HELPER"
BARONY_DIR=$(CDPATH= cd -- "$(dirname -- "$BARONY_EXECUTABLE")" && pwd)

PROFILE_DIR=$(mktemp -d "${TMPDIR:-/tmp}/bml-elixir-production.XXXXXX")
cleanup() {
  if [ "${BML_KEEP_PRODUCTION_VALIDATION_PROFILE:-0}" != "1" ]; then
    rm -rf "$PROFILE_DIR"
  else
    echo "kept production validation profile: $PROFILE_DIR" >&2
  fi
}
trap cleanup EXIT INT TERM

BML_DIR="$PROFILE_DIR/BaronyModLoader"
REPORT_DIR="$BML_DIR/reports"
RUNTIME_MANIFEST="$BML_DIR/runtime-manifest.json"
HOOK_MANIFEST="$PROFILE_DIR/hook-manifest.json"
RUNTIME_REPORT="$REPORT_DIR/runtime-load-report.json"
SYMBOL_REPORT="$REPORT_DIR/symbol-probe-report.json"
LIVE_INSTALL_REPORT="$REPORT_DIR/runebound-elixir-live-install-report.json"
PRODUCTION_VALIDATION_REPORT="$REPORT_DIR/runebound-elixir-production-validation-report.json"

mkdir -p "$BML_DIR" "$REPORT_DIR"
cp "$STATIC_HOOK_MANIFEST" "$HOOK_MANIFEST"

cat > "$RUNTIME_MANIFEST" <<JSON
{
  "contract": {
    "id": "bml-runtime-contract",
    "version": "0.1.0"
  },
  "launch": {
    "profileId": "steam-runebound-elixirs-production-validation",
    "runtimeStrategy": "installed-binary-hook",
    "gameVersionString": "v5.0.2",
    "runtime": {
      "runtimeId": "barony-bml-runtime-runebound-elixirs-production-validation",
      "runtimeVersion": "0.1.0"
    }
  },
  "mods": [
    {
      "id": "jml.runebound-elixirs",
      "version": "0.1.0",
      "packagePath": "$RUNES_PACKAGE_PATH",
      "capabilities": [
        { "id": "elixir_item_metadata", "version": "0.1.0", "required": true },
        { "id": "elixir_drop_generation", "version": "0.1.0", "required": true },
        { "id": "elixir_consumption", "version": "0.1.0", "required": true },
        { "id": "active_elixir_effect_state", "version": "0.1.0", "required": true },
        { "id": "active_elixir_effect_application", "version": "0.1.0", "required": true },
        { "id": "item_name_tooltip_rendering", "version": "0.1.0", "required": true },
        { "id": "multiplayer_version_metadata", "version": "0.1.0", "required": true }
      ],
      "modules": {
        "runeboundElixirs": {
          "namespace": "runebound_elixirs",
          "schemaVersion": "0.1.0",
          "authority": "host",
          "carrierItemType": "POTION_STRENGTH",
          "dataFiles": [
            "content/data/bml/elixir-catalog.json",
            "content/data/bml/elixir-drop-tables.json"
          ],
          "dropPolicy": {
            "eligibleClasses": "present_party_classes",
            "soloClassPolicy": "local_player_only",
            "partySizeEligibility": "generation_time_only",
            "rngAuthority": "host"
          },
          "activeEffects": {
            "stateScope": "profile_save_sidecar",
            "stateFile": "BaronyModLoader/state/jml.runebound-elixirs/active-elixir-effects.json",
            "savePolicy": "on_safe_save_boundary",
            "duplicatePolicy": "onePerElixirIdPerPlayer",
            "failurePolicy": "fail-closed"
          },
          "display": {
            "nameRendering": "elixir_display_name",
            "tooltipRendering": "upside_downside_lifecycle_duplicate_policy",
            "consumeMessages": "explicit_bargain_result",
            "reminderPolicy": "runtime_report_and_tooltip"
          },
          "multiplayer": {
            "versionPolicy": "matching_package_and_runtime_metadata_required",
            "stateAuthority": "host",
            "clientCompatibility": "reject_mismatched_elixir_contract",
            "failurePolicy": "fail-closed"
          },
          "failurePolicy": "fail-closed"
        }
      }
    }
  ]
}
JSON

echo "running Runebound: Elixirs production validation against: $BARONY_EXECUTABLE" >&2
echo "validation profile: $PROFILE_DIR" >&2

set +e
(
  cd "$BARONY_DIR" && \
  timeout --kill-after=5s "$TIMEOUT_DURATION" \
    env \
      -u BML_HOOK_FAKE_PROVIDER \
      -u BML_HOOK_ALLOW_NON_BARONY \
      BML_PROFILE_DIR="$PROFILE_DIR" \
      BML_RUNTIME_MANIFEST="$RUNTIME_MANIFEST" \
      BML_HOOK_MANIFEST="$HOOK_MANIFEST" \
      BML_HOOK_LIBRARY="$HOOK_LIBRARY" \
      BML_RUNEBOUND_ELIXIRS_PRODUCTION_VALIDATION=1 \
      BML_RUNEBOUND_ELIXIRS_PRODUCTION_VALIDATION_REPORT="$PRODUCTION_VALIDATION_REPORT" \
      BML_RUNEBOUND_ELIXIRS_LIVE_INSTALL_REPORT="$LIVE_INSTALL_REPORT" \
      LD_PRELOAD="$HOOK_LIBRARY" \
      "$BARONY_EXECUTABLE"
)
barony_status=$?
set -e

case "$barony_status" in
  0|124|137|143)
    ;;
  *)
    if [ -s "$PRODUCTION_VALIDATION_REPORT" ]; then
      echo "warning: Barony exited with status $barony_status after writing production validation artifacts; deferring to JSON assertions" >&2
    else
      fail "Barony exited with status $barony_status before writing $PRODUCTION_VALIDATION_REPORT"
    fi
    ;;
esac

if "$PYTHON" "$ASSERT_HELPER" --help 2>/dev/null | grep -q -- "--expected-barony-executable"; then
  "$PYTHON" "$ASSERT_HELPER" \
    --runtime-report "$RUNTIME_REPORT" \
    --symbol-report "$SYMBOL_REPORT" \
    --live-install-report "$LIVE_INSTALL_REPORT" \
    --production-validation-report "$PRODUCTION_VALIDATION_REPORT" \
    --expected-barony-executable "$BARONY_EXECUTABLE"
else
  "$PYTHON" "$ASSERT_HELPER" \
    --runtime-report "$RUNTIME_REPORT" \
    --symbol-report "$SYMBOL_REPORT" \
    --live-install-report "$LIVE_INSTALL_REPORT" \
    --production-validation-report "$PRODUCTION_VALIDATION_REPORT"
fi
