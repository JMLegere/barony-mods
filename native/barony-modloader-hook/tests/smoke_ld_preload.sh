#!/usr/bin/env sh
set -eu

HOOK_LIBRARY=${1:?usage: smoke_ld_preload.sh /absolute/path/to/libbarony_bml.so}
ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
PROFILE_DIR=$(mktemp -d "${TMPDIR:-/tmp}/bml-hook-smoke.XXXXXX")
RUNTIME_MANIFEST="$PROFILE_DIR/BaronyModLoader/runtime-manifest.json"
HOOK_MANIFEST="$PROFILE_DIR/hook-manifest.json"
REPORT="$PROFILE_DIR/BaronyModLoader/reports/runtime-load-report.json"
FAIL_PROFILE_DIR="$PROFILE_DIR/failure-profile"
FAIL_REPORT="$FAIL_PROFILE_DIR/BaronyModLoader/reports/runtime-load-report.json"
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
LD_PRELOAD="$HOOK_LIBRARY" \
/usr/bin/true

python - "$REPORT" "$HOOK_LIBRARY" <<'PY'
import json
import pathlib
import sys

report_path = pathlib.Path(sys.argv[1])
hook_library = sys.argv[2]
if not report_path.is_file():
    raise SystemExit(f"missing report: {report_path}")
report = json.loads(report_path.read_text(encoding="utf-8"))

assert report["contract"] == {"id": "bml-runtime-contract", "version": "0.1.0"}
assert report["runtime"]["id"] == "barony-bml-runtime-stash"
assert report["runtime"]["version"] == "0.1.0"
assert report["runtime"]["strategy"] == "installed-binary-hook"
assert report["runtime"]["executable"] == hook_library
assert report["profileId"] == "steam-default"
assert report["status"] == "loaded"
assert report["errors"] == []
assert isinstance(report["warnings"], list)
loaded_mods = report["loadedMods"]
assert len(loaded_mods) == 1, loaded_mods
stash = loaded_mods[0]
assert stash["id"] == "jml.stash"
assert stash["version"] == "0.1.0"
assert stash["status"] == "loaded"
for capability in (
    "persistent_storage",
    "persistent_inventory",
    "void_chest_binding",
    "placement_lobby",
    "placement_shop",
    "multiplayer_version_metadata",
):
    assert capability in stash["capabilities"], capability
print(f"smoke loaded ok: {report_path}")
PY

BML_PROFILE_DIR="$FAIL_PROFILE_DIR" \
BML_RUNTIME_MANIFEST="$RUNTIME_MANIFEST" \
BML_HOOK_MANIFEST="$MISSING_HOOK_MANIFEST" \
BML_HOOK_LIBRARY="$HOOK_LIBRARY" \
LD_PRELOAD="$HOOK_LIBRARY" \
/usr/bin/true

python - "$FAIL_REPORT" <<'PY'
import json
import pathlib
import sys

report_path = pathlib.Path(sys.argv[1])
if not report_path.is_file():
    raise SystemExit(f"missing failure report: {report_path}")
report = json.loads(report_path.read_text(encoding="utf-8"))
assert report["status"] == "failed"
assert report["loadedMods"] == []
assert report["profileId"] == "steam-default"
assert any(error["code"] == "BML_HOOK_MANIFEST_UNREADABLE" and error["severity"] == "fatal" for error in report["errors"]), report["errors"]
print(f"smoke failure ok: {report_path}")
PY
