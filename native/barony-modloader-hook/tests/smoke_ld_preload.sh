#!/usr/bin/env sh
set -eu

HOOK_LIBRARY=${1:?usage: smoke_ld_preload.sh /absolute/path/to/libbarony_bml.so /absolute/path/to/libfake_barony_symbols.so}
FAKE_SYMBOL_PROVIDER=${2:?usage: smoke_ld_preload.sh /absolute/path/to/libbarony_bml.so /absolute/path/to/libfake_barony_symbols.so}
ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
PROFILE_DIR=$(mktemp -d "${TMPDIR:-/tmp}/bml-hook-smoke.XXXXXX")
RUNTIME_MANIFEST="$PROFILE_DIR/BaronyModLoader/runtime-manifest.json"
HOOK_MANIFEST="$PROFILE_DIR/hook-manifest.json"
REPORT_DIR="$PROFILE_DIR/BaronyModLoader/reports"
REPORT="$REPORT_DIR/runtime-load-report.json"
SYMBOL_REPORT="$REPORT_DIR/symbol-probe-report.json"
STASH_REPORT="$REPORT_DIR/stash-hook-report.json"
FAIL_PROFILE_DIR="$PROFILE_DIR/failure-profile"
FAIL_REPORT_DIR="$FAIL_PROFILE_DIR/BaronyModLoader/reports"
FAIL_REPORT="$FAIL_REPORT_DIR/runtime-load-report.json"
FAIL_SYMBOL_REPORT="$FAIL_REPORT_DIR/symbol-probe-report.json"
FAIL_STASH_REPORT="$FAIL_REPORT_DIR/stash-hook-report.json"
MISSING_HOOK_MANIFEST="$PROFILE_DIR/missing-hook-manifest.json"

cleanup() {
  rm -rf "$PROFILE_DIR"
}
trap cleanup EXIT INT TERM

mkdir -p "$PROFILE_DIR/BaronyModLoader" "$(dirname -- "$HOOK_MANIFEST")"
cat > "$RUNTIME_MANIFEST" <<'JSON'
{
  "contract": {
    "id": "bml-runtime-contract",
    "version": "0.1.0"
  },
  "launch": {
    "profileId": "steam-default",
    "runtimeStrategy": "installed-binary-hook",
    "gameVersionString": "v5.0.2",
    "runtime": {
      "runtimeId": "barony-bml-runtime-stash",
      "runtimeVersion": "0.1.0"
    }
  },
  "mods": [
    {
      "id": "jml.stash",
      "version": "0.1.0",
      "packagePath": "/tmp/jml.stash/bml-package.json",
      "capabilities": [
        { "id": "persistent_storage", "version": "0.1.0", "required": true },
        { "id": "persistent_inventory", "version": "0.1.0", "required": true },
        { "id": "void_chest_binding", "version": "0.1.0", "required": true },
        { "id": "placement_lobby", "version": "0.1.0", "required": true },
        { "id": "placement_shop", "version": "0.1.0", "required": true },
        { "id": "multiplayer_version_metadata", "version": "0.1.0", "required": true }
      ],
      "modules": {
        "persistentStorage": {},
        "persistentInventories": {},
        "voidChestBindings": {},
        "placements": {},
        "multiplayer": {}
      }
    }
  ]
}
JSON

cp "$ROOT_DIR/manifests/steam-371970-22630456-linux.json" "$HOOK_MANIFEST"

BML_PROFILE_DIR="$PROFILE_DIR" \
BML_RUNTIME_MANIFEST="$RUNTIME_MANIFEST" \
BML_HOOK_MANIFEST="$HOOK_MANIFEST" \
BML_HOOK_LIBRARY="$HOOK_LIBRARY" \
LD_PRELOAD="$FAKE_SYMBOL_PROVIDER:$HOOK_LIBRARY" \
/usr/bin/true

python - "$REPORT" "$SYMBOL_REPORT" "$STASH_REPORT" "$HOOK_LIBRARY" <<'PY'
import json
import pathlib
import sys

report_path = pathlib.Path(sys.argv[1])
symbol_report_path = pathlib.Path(sys.argv[2])
stash_report_path = pathlib.Path(sys.argv[3])
hook_library = sys.argv[4]
for path in (report_path, symbol_report_path, stash_report_path):
    if not path.is_file():
        raise SystemExit(f"missing report: {path}")

report = json.loads(report_path.read_text(encoding="utf-8"))
symbol_report = json.loads(symbol_report_path.read_text(encoding="utf-8"))
stash_report = json.loads(stash_report_path.read_text(encoding="utf-8"))

assert report["contract"] == {"id": "bml-runtime-contract", "version": "0.1.0"}
assert report["runtime"]["id"] == "barony-bml-runtime-stash"
assert report["runtime"]["version"] == "0.1.0"
assert report["runtime"]["strategy"] == "installed-binary-hook"
assert report["runtime"]["executable"] == hook_library
assert report["profileId"] == "steam-default"
assert report["status"] == "failed"
assert report["loadedMods"] == []
runtime_codes = {error["code"] for error in report["errors"]}
assert "BML_STASH_HOOKS_NOT_INSTALLED" in runtime_codes, report["errors"]
assert "BML_HOOK_SYMBOL_MISSING" not in runtime_codes, report["errors"]
assert isinstance(report["warnings"], list)

assert symbol_report["schemaVersion"] == "0.1.0"
assert symbol_report["runtime"]["id"] == "barony-bml-runtime-stash"
assert symbol_report["profileId"] == "steam-default"
assert symbol_report["status"] == "loaded"
assert symbol_report["summary"]["required"] >= 20
assert symbol_report["summary"]["resolved"] == symbol_report["summary"]["required"]
assert symbol_report["summary"]["missing"] == 0
assert symbol_report["errors"] == []
for symbol in symbol_report["symbols"]:
    assert symbol["required"] is True
    assert symbol["status"] == "resolved", symbol
    assert isinstance(symbol["address"], str) and symbol["address"].startswith("0x"), symbol

assert stash_report["schemaVersion"] == "0.1.0"
assert stash_report["runtime"]["id"] == "barony-bml-runtime-stash"
assert stash_report["profileId"] == "steam-default"
assert stash_report["mod"] == {"id": "jml.stash", "version": "0.1.0", "manifestDetected": True}
assert stash_report["status"] == "failed"
assert stash_report["summary"]["failClosed"] is True
assert stash_report["summary"]["required"] == 6
assert stash_report["summary"]["installed"] == 0
assert stash_report["summary"]["notInstalled"] == 6
hook_ids = {hook["id"] for hook in stash_report["hooks"]}
assert hook_ids == {
    "persistent_inventory",
    "void_chest_binding",
    "close_save_flush",
    "placement_lobby",
    "placement_shop",
    "multiplayer_metadata",
}
for hook in stash_report["hooks"]:
    assert hook["required"] is True
    assert hook["status"] == "not-installed", hook
assert any(error["code"] == "BML_STASH_HOOKS_NOT_INSTALLED" and error["severity"] == "fatal" for error in stash_report["errors"]), stash_report["errors"]
print(f"smoke symbol resolved and stash fail-closed ok: {report_path}")
PY

BML_PROFILE_DIR="$FAIL_PROFILE_DIR" \
BML_RUNTIME_MANIFEST="$RUNTIME_MANIFEST" \
BML_HOOK_MANIFEST="$MISSING_HOOK_MANIFEST" \
BML_HOOK_LIBRARY="$HOOK_LIBRARY" \
LD_PRELOAD="$FAKE_SYMBOL_PROVIDER:$HOOK_LIBRARY" \
/usr/bin/true

python - "$FAIL_REPORT" "$FAIL_SYMBOL_REPORT" "$FAIL_STASH_REPORT" <<'PY'
import json
import pathlib
import sys

runtime_report_path = pathlib.Path(sys.argv[1])
symbol_report_path = pathlib.Path(sys.argv[2])
stash_report_path = pathlib.Path(sys.argv[3])
for path in (runtime_report_path, symbol_report_path, stash_report_path):
    if not path.is_file():
        raise SystemExit(f"missing failure report: {path}")

runtime_report = json.loads(runtime_report_path.read_text(encoding="utf-8"))
symbol_report = json.loads(symbol_report_path.read_text(encoding="utf-8"))
stash_report = json.loads(stash_report_path.read_text(encoding="utf-8"))

assert runtime_report["status"] == "failed"
assert runtime_report["loadedMods"] == []
assert runtime_report["profileId"] == "steam-default"
runtime_errors = runtime_report["errors"]
assert any(error["code"] == "BML_HOOK_MANIFEST_UNREADABLE" and error["severity"] == "fatal" for error in runtime_errors), runtime_errors
assert any(error["code"] == "BML_STASH_HOOKS_NOT_INSTALLED" and error["severity"] == "fatal" for error in runtime_errors), runtime_errors
assert not any(error["code"] == "BML_HOOK_SYMBOL_MISSING" for error in runtime_errors), runtime_errors

assert symbol_report["status"] == "loaded"
assert symbol_report["summary"]["missing"] == 0
assert symbol_report["errors"] == []

assert stash_report["status"] == "failed"
assert stash_report["summary"]["failClosed"] is True
assert any(error["code"] == "BML_STASH_HOOKS_NOT_INSTALLED" and error["severity"] == "fatal" for error in stash_report["errors"]), stash_report["errors"]
print(f"smoke missing hook manifest failure ok: {runtime_report_path}")
PY
