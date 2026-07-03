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
DETOUR_REPORT="$REPORT_DIR/detour-self-test-report.json"
STASH_DETOUR_REPORT="$REPORT_DIR/stash-detour-self-test-report.json"
FAIL_PROFILE_DIR="$PROFILE_DIR/failure-profile"
FAIL_REPORT_DIR="$FAIL_PROFILE_DIR/BaronyModLoader/reports"
FAIL_REPORT="$FAIL_REPORT_DIR/runtime-load-report.json"
FAIL_SYMBOL_REPORT="$FAIL_REPORT_DIR/symbol-probe-report.json"
FAIL_STASH_REPORT="$FAIL_REPORT_DIR/stash-hook-report.json"
MISSING_HOOK_MANIFEST="$PROFILE_DIR/missing-hook-manifest.json"
NO_STASH_PROFILE_DIR="$PROFILE_DIR/no-stash-profile"
NO_STASH_RUNTIME_MANIFEST="$NO_STASH_PROFILE_DIR/BaronyModLoader/runtime-manifest.json"
NO_STASH_REPORT_DIR="$NO_STASH_PROFILE_DIR/BaronyModLoader/reports"
NO_STASH_REPORT="$NO_STASH_REPORT_DIR/runtime-load-report.json"
NO_STASH_SYMBOL_REPORT="$NO_STASH_REPORT_DIR/symbol-probe-report.json"
NO_STASH_STASH_REPORT="$NO_STASH_REPORT_DIR/stash-hook-report.json"
INSTALL_PROFILE_DIR="$PROFILE_DIR/install-profile"
INSTALL_REPORT_DIR="$INSTALL_PROFILE_DIR/BaronyModLoader/reports"
INSTALL_REPORT="$INSTALL_REPORT_DIR/runtime-load-report.json"
INSTALL_SYMBOL_REPORT="$INSTALL_REPORT_DIR/symbol-probe-report.json"
INSTALL_STASH_REPORT="$INSTALL_REPORT_DIR/stash-hook-report.json"
STASH_DETOUR_INSTALL_REPORT="$INSTALL_REPORT_DIR/stash-detour-install-report.json"
CORE_INSTALL_PROFILE_DIR="$PROFILE_DIR/core-install-profile"
CORE_INSTALL_REPORT_DIR="$CORE_INSTALL_PROFILE_DIR/BaronyModLoader/reports"
CORE_INSTALL_REPORT="$CORE_INSTALL_REPORT_DIR/runtime-load-report.json"
CORE_INSTALL_SYMBOL_REPORT="$CORE_INSTALL_REPORT_DIR/symbol-probe-report.json"
CORE_INSTALL_STASH_REPORT="$CORE_INSTALL_REPORT_DIR/stash-hook-report.json"
STASH_CORE_DETOUR_INSTALL_REPORT="$CORE_INSTALL_REPORT_DIR/stash-core-detour-install-report.json"
BEHAVIOR_PROFILE_DIR="$PROFILE_DIR/behavior-profile"
BEHAVIOR_REPORT_DIR="$BEHAVIOR_PROFILE_DIR/BaronyModLoader/reports"
BEHAVIOR_REPORT="$BEHAVIOR_REPORT_DIR/runtime-load-report.json"
BEHAVIOR_SYMBOL_REPORT="$BEHAVIOR_REPORT_DIR/symbol-probe-report.json"
BEHAVIOR_STASH_REPORT="$BEHAVIOR_REPORT_DIR/stash-hook-report.json"
STASH_CORE_BEHAVIOR_REPORT="$BEHAVIOR_REPORT_DIR/stash-core-behavior-report.json"


cleanup() {
  rm -rf "$PROFILE_DIR"
}
trap cleanup EXIT INT TERM

mkdir -p "$PROFILE_DIR/BaronyModLoader" "$NO_STASH_PROFILE_DIR/BaronyModLoader" "$INSTALL_PROFILE_DIR/BaronyModLoader" "$CORE_INSTALL_PROFILE_DIR/BaronyModLoader" "$BEHAVIOR_PROFILE_DIR/BaronyModLoader" "$(dirname -- "$HOOK_MANIFEST")"
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
cat > "$NO_STASH_RUNTIME_MANIFEST" <<'JSON'
{
  "contract": {
    "id": "bml-runtime-contract",
    "version": "0.1.0"
  },
  "launch": {
    "profileId": "steam-no-stash",
    "runtimeStrategy": "installed-binary-hook",
    "gameVersionString": "v5.0.2",
    "runtime": {
      "runtimeId": "barony-bml-runtime-base",
      "runtimeVersion": "0.1.0"
    }
  },
  "mods": []
}
JSON


cp "$ROOT_DIR/manifests/steam-371970-22630456-linux.json" "$HOOK_MANIFEST"

BML_DETOUR_SELF_TEST=1 \
BML_STASH_DETOUR_SELF_TEST=1 \
BML_PROFILE_DIR="$PROFILE_DIR" \
BML_RUNTIME_MANIFEST="$RUNTIME_MANIFEST" \
BML_HOOK_MANIFEST="$HOOK_MANIFEST" \
BML_HOOK_LIBRARY="$HOOK_LIBRARY" \
LD_PRELOAD="$FAKE_SYMBOL_PROVIDER:$HOOK_LIBRARY" \
/usr/bin/true

python - "$REPORT" "$SYMBOL_REPORT" "$STASH_REPORT" "$DETOUR_REPORT" "$STASH_DETOUR_REPORT" "$HOOK_LIBRARY" <<'PY'
import json
import pathlib
import sys

report_path = pathlib.Path(sys.argv[1])
symbol_report_path = pathlib.Path(sys.argv[2])
stash_report_path = pathlib.Path(sys.argv[3])
detour_report_path = pathlib.Path(sys.argv[4])
stash_detour_report_path = pathlib.Path(sys.argv[5])
hook_library = sys.argv[6]
for path in (report_path, symbol_report_path, stash_report_path, detour_report_path, stash_detour_report_path):
    if not path.is_file():
        raise SystemExit(f"missing report: {path}")

report = json.loads(report_path.read_text(encoding="utf-8"))
symbol_report = json.loads(symbol_report_path.read_text(encoding="utf-8"))
stash_report = json.loads(stash_report_path.read_text(encoding="utf-8"))
detour_report = json.loads(detour_report_path.read_text(encoding="utf-8"))
stash_detour_report = json.loads(stash_detour_report_path.read_text(encoding="utf-8"))

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

assert detour_report["schemaVersion"] == "0.1.0"
assert detour_report["test"] == "linux-x86_64-absolute-jump-detour-self-test"
assert detour_report["status"] == "loaded", detour_report
assert detour_report["backend"] == {
    "patchStyle": "rip-relative-indirect-jmp-absolute-slot",
    "patchBytes": 14,
    "decoder": "fixture-safe-subset",
}
assert detour_report["targetSymbol"] == "bml_fake_detour_target"
assert isinstance(detour_report["targetAddress"], str) and detour_report["targetAddress"].startswith("0x"), detour_report
assert isinstance(detour_report["replacementAddress"], str) and detour_report["replacementAddress"].startswith("0x"), detour_report
assert isinstance(detour_report["trampolineAddress"], str) and detour_report["trampolineAddress"].startswith("0x"), detour_report
assert detour_report["patchSize"] >= 14, detour_report
assert detour_report["replacementInvoked"] is True, detour_report
assert detour_report["originalCallThroughInvoked"] is True, detour_report
assert detour_report["replacementCalls"] == 1, detour_report
assert detour_report["fakeCounterAfter"] == detour_report["fakeCounterBefore"] + 1, detour_report
assert detour_report["originalResult"] == 41, detour_report
assert detour_report["directResult"] == 1041, detour_report
assert detour_report["error"] is None, detour_report

assert stash_detour_report["schemaVersion"] == "0.1.0"
assert stash_detour_report["test"] == "stash-add-item-detour-self-test"
assert stash_detour_report["status"] == "loaded", stash_detour_report
assert stash_detour_report["backend"] == {
    "patchStyle": "rip-relative-indirect-jmp-absolute-slot",
    "patchBytes": 14,
    "decoder": "fixture-safe-subset",
}
assert stash_detour_report["targetSymbol"] == "_ZN6Entity24addItemToVoidChestServerEiP4ItembS1_"
assert stash_detour_report["targetName"] == "Entity::addItemToVoidChestServer"
assert isinstance(stash_detour_report["targetAddress"], str) and stash_detour_report["targetAddress"].startswith("0x"), stash_detour_report
assert isinstance(stash_detour_report["replacementAddress"], str) and stash_detour_report["replacementAddress"].startswith("0x"), stash_detour_report
assert isinstance(stash_detour_report["trampolineAddress"], str) and stash_detour_report["trampolineAddress"].startswith("0x"), stash_detour_report
assert stash_detour_report["patchSize"] >= 14, stash_detour_report
assert stash_detour_report["replacementInvoked"] is True, stash_detour_report
assert stash_detour_report["originalCallThroughInvoked"] is True, stash_detour_report
assert stash_detour_report["replacementCalls"] == 1, stash_detour_report
assert stash_detour_report["originalResult"] == "0x2a", stash_detour_report
assert stash_detour_report["directResult"] == "0x2a", stash_detour_report
assert stash_detour_report["error"] is None, stash_detour_report

assert stash_report["schemaVersion"] == "0.1.0"
assert stash_report["runtime"]["id"] == "barony-bml-runtime-stash"
assert stash_report["profileId"] == "steam-default"
assert stash_report["mod"] == {"id": "jml.stash", "version": "0.1.0", "manifestDetected": True}
assert stash_report["backend"]["id"] == "linux-x86_64-direct-stash-detour"
assert stash_report["backend"]["mode"] == "analyze-only"
assert stash_report["backend"]["strategy"] == "abstract-direct-detour-backend"
assert stash_report["backend"]["patchBytes"] == 14
assert stash_report["status"] == "failed"
summary = stash_report["summary"]
assert summary["failClosed"] is True
assert summary["required"] == 5
assert summary["installed"] == 0
assert summary["ready"] == 2
assert summary["blocked"] == 3
assert summary["notInstalled"] == 5
expected_hook_ids = {
    "stash_void_chest_binding",
    "stash_inventory_persistence",
    "stash_lobby_placement",
    "stash_shop_placement",
    "stash_multiplayer_metadata_gate",
}
assert len(stash_report["hooks"]) == 5
hook_ids = {hook["id"] for hook in stash_report["hooks"]}
assert hook_ids == expected_hook_ids
all_targets = []
for hook in stash_report["hooks"]:
    assert hook["required"] is True
    assert hook["capability"], hook
    assert hook["status"] in {"ready", "blocked"}, hook
    assert hook["status"] != "installed", hook
    assert isinstance(hook["targets"], list) and hook["targets"], hook
    assert hook["readyTargets"] + hook["blockedTargets"] + hook["missingTargets"] == len(hook["targets"]), hook
    all_targets.extend(hook["targets"])
function_targets = [target for target in all_targets if target["kind"] == "function"]
data_targets = [target for target in all_targets if target["kind"] == "data"]
assert function_targets, all_targets
assert data_targets, all_targets
ready_function_targets = [target for target in function_targets if target["status"] == "ready"]
blocked_function_targets = [target for target in function_targets if target["status"] == "blocked"]
assert ready_function_targets, function_targets
assert blocked_function_targets, function_targets
ready_function_names = {target["name"] for target in ready_function_targets}
assert "Entity::addItemToVoidChestServer" in ready_function_names, ready_function_targets
assert "newEntity" in ready_function_names, ready_function_targets
assert "actChestLid" in ready_function_names, ready_function_targets
assert all(target["patchWindowBytes"] >= 14 for target in ready_function_targets), ready_function_targets
assert all("blockerCode" not in target for target in ready_function_targets), ready_function_targets
assert all("blockerCode" in target for target in blocked_function_targets), blocked_function_targets
assert all(target["status"] == "ready" for target in data_targets), data_targets
assert all("blockerCode" not in target for target in data_targets), data_targets
assert any(error["code"] == "BML_STASH_HOOKS_NOT_INSTALLED" and error["severity"] == "fatal" for error in stash_report["errors"]), stash_report["errors"]
print(f"smoke symbol resolved and stash fail-closed ok: {report_path}")
PY

BML_STASH_INSTALL_ADD_ITEM_PASSTHROUGH=1 \
BML_PROFILE_DIR="$INSTALL_PROFILE_DIR" \
BML_RUNTIME_MANIFEST="$RUNTIME_MANIFEST" \
BML_HOOK_MANIFEST="$HOOK_MANIFEST" \
BML_HOOK_LIBRARY="$HOOK_LIBRARY" \
LD_PRELOAD="$FAKE_SYMBOL_PROVIDER:$HOOK_LIBRARY" \
/usr/bin/true

python - "$INSTALL_REPORT" "$INSTALL_SYMBOL_REPORT" "$INSTALL_STASH_REPORT" "$STASH_DETOUR_INSTALL_REPORT" <<'PY'
import json
import pathlib
import sys

report_path = pathlib.Path(sys.argv[1])
symbol_report_path = pathlib.Path(sys.argv[2])
stash_report_path = pathlib.Path(sys.argv[3])
install_report_path = pathlib.Path(sys.argv[4])
for path in (report_path, symbol_report_path, stash_report_path, install_report_path):
    if not path.is_file():
        raise SystemExit(f"missing opt-in install report: {path}")

report = json.loads(report_path.read_text(encoding="utf-8"))
symbol_report = json.loads(symbol_report_path.read_text(encoding="utf-8"))
stash_report = json.loads(stash_report_path.read_text(encoding="utf-8"))
install_report = json.loads(install_report_path.read_text(encoding="utf-8"))

assert report["status"] == "failed", report
runtime_codes = {error["code"] for error in report["errors"]}
assert "BML_STASH_HOOKS_NOT_INSTALLED" in runtime_codes, report["errors"]
assert "BML_STASH_ADD_ITEM_INSTALL_FAILED" not in runtime_codes, report["errors"]
assert symbol_report["summary"]["missing"] == 0, symbol_report
assert stash_report["backend"]["mode"] == "analyze-only", stash_report
assert stash_report["status"] == "failed", stash_report
assert stash_report["summary"]["failClosed"] is True, stash_report
assert stash_report["summary"]["installed"] == 0, stash_report
assert stash_report["summary"]["notInstalled"] == stash_report["summary"]["required"], stash_report

assert install_report["schemaVersion"] == "0.1.0"
assert install_report["test"] == "stash-add-item-passthrough-install"
assert install_report["status"] == "installed", install_report
assert install_report["targetSymbol"] == "_ZN6Entity24addItemToVoidChestServerEiP4ItembS1_"
assert install_report["targetName"] == "Entity::addItemToVoidChestServer"
assert isinstance(install_report["targetAddress"], str) and install_report["targetAddress"].startswith("0x"), install_report
assert isinstance(install_report["replacementAddress"], str) and install_report["replacementAddress"].startswith("0x"), install_report
assert isinstance(install_report["trampolineAddress"], str) and install_report["trampolineAddress"].startswith("0x"), install_report
assert install_report["patchSize"] >= 14, install_report
assert install_report["replacementInvoked"] is False, install_report
assert install_report["originalCallThroughInvoked"] is False, install_report
assert install_report["replacementCalls"] == 0, install_report
assert install_report["originalResult"] is None, install_report
assert install_report["directResult"] is None, install_report
assert install_report["error"] is None, install_report
print(f"opt-in stash add-item pass-through install remains fail-closed ok: {install_report_path}")
PY

BML_STASH_INSTALL_CORE_PASSTHROUGH=1 \
BML_PROFILE_DIR="$CORE_INSTALL_PROFILE_DIR" \
BML_RUNTIME_MANIFEST="$RUNTIME_MANIFEST" \
BML_HOOK_MANIFEST="$HOOK_MANIFEST" \
BML_HOOK_LIBRARY="$HOOK_LIBRARY" \
LD_PRELOAD="$FAKE_SYMBOL_PROVIDER:$HOOK_LIBRARY" \
/usr/bin/true

python - "$CORE_INSTALL_REPORT" "$CORE_INSTALL_SYMBOL_REPORT" "$CORE_INSTALL_STASH_REPORT" "$STASH_CORE_DETOUR_INSTALL_REPORT" <<'PY'
import json
import pathlib
import sys

report_path = pathlib.Path(sys.argv[1])
symbol_report_path = pathlib.Path(sys.argv[2])
stash_report_path = pathlib.Path(sys.argv[3])
core_install_report_path = pathlib.Path(sys.argv[4])
for path in (report_path, symbol_report_path, stash_report_path, core_install_report_path):
    if not path.is_file():
        raise SystemExit(f"missing opt-in core install report: {path}")

report = json.loads(report_path.read_text(encoding="utf-8"))
symbol_report = json.loads(symbol_report_path.read_text(encoding="utf-8"))
stash_report = json.loads(stash_report_path.read_text(encoding="utf-8"))
core_install_report = json.loads(core_install_report_path.read_text(encoding="utf-8"))

assert report["status"] == "failed", report
runtime_codes = {error["code"] for error in report["errors"]}
assert "BML_STASH_HOOKS_NOT_INSTALLED" in runtime_codes, report["errors"]
assert "BML_STASH_CORE_INSTALL_FAILED" not in runtime_codes, report["errors"]
assert symbol_report["summary"]["missing"] == 0, symbol_report
assert stash_report["backend"]["mode"] == "analyze-only", stash_report
assert stash_report["status"] == "failed", stash_report
assert stash_report["summary"]["failClosed"] is True, stash_report
assert stash_report["summary"]["installed"] == 0, stash_report
assert stash_report["summary"]["notInstalled"] == stash_report["summary"]["required"], stash_report

assert core_install_report["schemaVersion"] == "0.1.0"
assert core_install_report["test"] == "stash-core-passthrough-install"
assert core_install_report["status"] == "installed", core_install_report
assert core_install_report["backend"] == {
    "patchStyle": "rip-relative-indirect-jmp-absolute-slot",
    "patchBytes": 14,
    "decoder": "fixture-safe-subset",
}
assert core_install_report["summary"] == {
    "requested": 5,
    "installed": 5,
    "failed": 0,
    "failClosed": True,
}, core_install_report
expected_targets = {
    "Entity::getChestInventoryList",
    "Entity::addItemToVoidChestServer",
    "Entity::removeItemFromVoidChestServer",
    "Entity::closeChest",
    "Entity::closeChestServer",
}
targets = core_install_report["targets"]
assert {target["targetName"] for target in targets} == expected_targets, targets
for target in targets:
    assert target["status"] == "installed", target
    assert isinstance(target["targetAddress"], str) and target["targetAddress"].startswith("0x"), target
    assert isinstance(target["replacementAddress"], str) and target["replacementAddress"].startswith("0x"), target
    assert isinstance(target["trampolineAddress"], str) and target["trampolineAddress"].startswith("0x"), target
    assert target["patchSize"] >= 14, target
    assert target["replacementInvoked"] is False, target
    assert target["replacementCalls"] == 0, target
    assert target["error"] is None, target
print(f"opt-in stash core pass-through install remains fail-closed ok: {core_install_report_path}")
PY

BML_STASH_ENABLE_EXPERIMENTAL_CORE_BEHAVIOR=1 \
BML_STASH_CORE_BEHAVIOR_SELF_TEST=1 \
BML_PROFILE_DIR="$BEHAVIOR_PROFILE_DIR" \
BML_RUNTIME_MANIFEST="$RUNTIME_MANIFEST" \
BML_HOOK_MANIFEST="$HOOK_MANIFEST" \
BML_HOOK_LIBRARY="$HOOK_LIBRARY" \
LD_PRELOAD="$FAKE_SYMBOL_PROVIDER:$HOOK_LIBRARY" \
/usr/bin/true

python - "$BEHAVIOR_REPORT" "$BEHAVIOR_SYMBOL_REPORT" "$BEHAVIOR_STASH_REPORT" "$STASH_CORE_BEHAVIOR_REPORT" <<'PY'
import json
import pathlib
import sys

report_path = pathlib.Path(sys.argv[1])
symbol_report_path = pathlib.Path(sys.argv[2])
stash_report_path = pathlib.Path(sys.argv[3])
behavior_report_path = pathlib.Path(sys.argv[4])
for path in (report_path, symbol_report_path, stash_report_path, behavior_report_path):
    if not path.is_file():
        raise SystemExit(f"missing experimental behavior report: {path}")

report = json.loads(report_path.read_text(encoding="utf-8"))
symbol_report = json.loads(symbol_report_path.read_text(encoding="utf-8"))
stash_report = json.loads(stash_report_path.read_text(encoding="utf-8"))
behavior_report = json.loads(behavior_report_path.read_text(encoding="utf-8"))

assert report["status"] == "failed", report
runtime_codes = {error["code"] for error in report["errors"]}
assert "BML_STASH_HOOKS_NOT_INSTALLED" in runtime_codes, report["errors"]
assert "BML_STASH_CORE_BEHAVIOR_FAILED" not in runtime_codes, report["errors"]
assert symbol_report["summary"]["missing"] == 0, symbol_report
assert stash_report["backend"]["mode"] == "analyze-only", stash_report
assert stash_report["summary"]["failClosed"] is True, stash_report

assert behavior_report["schemaVersion"] == "0.1.0", behavior_report
assert behavior_report["test"] == "stash-core-experimental-behavior", behavior_report
assert behavior_report["status"] == "installed", behavior_report
assert behavior_report["experimental"] is True, behavior_report
assert behavior_report["claimBoundary"] == "fake-provider-state-backed-core-lifecycle-only", behavior_report
assert behavior_report["selfTest"] == {
    "requested": True,
    "loadedCount": 1,
    "savedRows": 2,
}, behavior_report
state = behavior_report["state"]
assert state["loaded"] is True, state
assert state["dirty"] is False, state
assert state["loadCount"] == 1, state
assert state["saveCount"] == 1, state
assert state["dirtyMarks"] >= 1, state
assert state["boundInventoryCount"] == 2, state
assert state["savedRows"] == 2, state
state_path = pathlib.Path(state["path"])
assert state_path.is_file(), state_path
rows = [line for line in state_path.read_text(encoding="utf-8").splitlines() if line and not line.startswith("#")]
assert rows == ["1 2 -1 3 12345 1", "2 3 0 4 67890 1"], rows
expected_targets = {
    "Entity::getChestInventoryList": 1,
    "Entity::addItemToVoidChestServer": 1,
    "Entity::removeItemFromVoidChestServer": 0,
    "Entity::closeChest": 1,
    "Entity::closeChestServer": 0,
}
assert {target["targetName"] for target in behavior_report["targets"]} == set(expected_targets), behavior_report["targets"]
for target in behavior_report["targets"]:
    assert target["status"] == "installed", target
    assert target["replacementCalls"] == expected_targets[target["targetName"]], target
    assert target["error"] is None, target
print(f"experimental stash core behavior self-test remains fail-closed ok: {behavior_report_path}")
PY

BML_PROFILE_DIR="$NO_STASH_PROFILE_DIR" \
BML_RUNTIME_MANIFEST="$NO_STASH_RUNTIME_MANIFEST" \
BML_HOOK_MANIFEST="$HOOK_MANIFEST" \
BML_HOOK_LIBRARY="$HOOK_LIBRARY" \
LD_PRELOAD="$FAKE_SYMBOL_PROVIDER:$HOOK_LIBRARY" \
/usr/bin/true

python - "$NO_STASH_REPORT" "$NO_STASH_SYMBOL_REPORT" "$NO_STASH_STASH_REPORT" <<'PY'
import json
import pathlib
import sys

runtime_report_path = pathlib.Path(sys.argv[1])
symbol_report_path = pathlib.Path(sys.argv[2])
stash_report_path = pathlib.Path(sys.argv[3])
for path in (runtime_report_path, symbol_report_path, stash_report_path):
    if not path.is_file():
        raise SystemExit(f"missing no-stash report: {path}")

runtime_report = json.loads(runtime_report_path.read_text(encoding="utf-8"))
symbol_report = json.loads(symbol_report_path.read_text(encoding="utf-8"))
stash_report = json.loads(stash_report_path.read_text(encoding="utf-8"))

assert runtime_report["status"] == "loaded"
assert runtime_report["loadedMods"] == []
assert runtime_report["profileId"] == "steam-no-stash"
assert runtime_report["errors"] == []

assert symbol_report["status"] == "loaded"
assert symbol_report["profileId"] == "steam-no-stash"
assert symbol_report["summary"]["missing"] == 0
assert symbol_report["errors"] == []

assert stash_report["status"] == "not_applicable"
assert stash_report["profileId"] == "steam-no-stash"
assert stash_report["mod"] == {"id": "jml.stash", "version": "0.1.0", "manifestDetected": False}
assert stash_report["summary"] == {
    "required": 0,
    "installed": 0,
    "ready": 0,
    "blocked": 0,
    "notInstalled": 0,
    "failClosed": False,
}
assert stash_report["hooks"] == []
assert stash_report["errors"] == []
print(f"smoke no-stash not-applicable ok: {runtime_report_path}")
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
assert stash_report["backend"]["id"] == "linux-x86_64-direct-stash-detour"
assert stash_report["summary"]["failClosed"] is True
assert stash_report["summary"]["required"] == 5
assert stash_report["summary"]["installed"] == 0
assert stash_report["summary"]["notInstalled"] == 5
assert any(error["code"] == "BML_STASH_HOOKS_NOT_INSTALLED" and error["severity"] == "fatal" for error in stash_report["errors"]), stash_report["errors"]
print(f"smoke missing hook manifest failure ok: {runtime_report_path}")
PY
