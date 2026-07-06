"use strict";

const { After, Given, When, Then } = require("@cucumber/cucumber");
const { execFileSync } = require("child_process");
const fs = require("fs");
const os = require("os");
const path = require("path");

// paths: <repo>/framework/BaronyModLoader/features/steps/profile_steps.js
//          ../../../../ from here = <repo>
const REPO_ROOT = path.resolve(__dirname, "../../../../");
const BML_APP = path.join(REPO_ROOT, "framework/BaronyModLoader/app/barony_mod_loader.py");
const ELIXIRS_PKG = path.join(REPO_ROOT, "mods/runebound-elixirs");
const MARKER = "__BML_PROFILE_JSON__";

function runProfilePython(script, world) {
  const tmp = path.join(os.tmpdir(), `bml-profile-contract-${Date.now()}-${Math.random().toString(36).slice(2)}.py`);
  fs.writeFileSync(tmp, script, "utf-8");
  try {
    const stdout = execFileSync("/usr/bin/python3", [tmp], {
      cwd: REPO_ROOT,
      encoding: "utf-8",
      timeout: 30000,
      stdio: ["ignore", "pipe", "pipe"],
    });
    const line = stdout.split(/\r?\n/).find(entry => entry.startsWith(MARKER));
    if (!line) {
      throw new Error(`Profile contract script did not emit ${MARKER}. Output:\n${stdout}`);
    }
    world.profileContract = JSON.parse(line.slice(MARKER.length));
  } catch (err) {
    const stdout = err.stdout ? String(err.stdout) : "";
    const stderr = err.stderr ? String(err.stderr) : "";
    throw new Error(`Profile contract script failed (exit ${err.status ?? 1}):\nSTDOUT:\n${stdout}\nSTDERR:\n${stderr}`);
  } finally {
    try { fs.unlinkSync(tmp); } catch (_) {}
  }
}

function profileScript(world, scenario, profileId) {
  return `
import argparse
import contextlib
import dataclasses
import importlib.util
import io
import json
from pathlib import Path
import sys

REPO_ROOT = Path(${JSON.stringify(REPO_ROOT)})
BML_APP = Path(${JSON.stringify(BML_APP)})
ELIXIRS_PKG = Path(${JSON.stringify(ELIXIRS_PKG)})
STAGING_DIR = Path(${JSON.stringify(world.profileBddWorkspace)})
FAKE_BARONY = Path(${JSON.stringify(world.profileBddFakeBarony)})
SCENARIO = ${JSON.stringify(scenario)}
PROFILE_ID = ${JSON.stringify(profileId || "profile-bdd")}
MARKER = ${JSON.stringify(MARKER)}

spec = importlib.util.spec_from_file_location("bml_profile_app", BML_APP)
mod = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = mod
spec.loader.exec_module(mod)

PROFILE_STATE_CANDIDATES = (
    "build_profile_state",
    "load_profile_state",
    "project_profile_state",
    "inspect_profile_state",
    "get_profile_state",
    "read_profile_state",
)
ENABLE_CANDIDATES = (
    "enable_profile_mod",
    "enable_profile_package",
    "set_profile_mod_enabled",
    "profile_enable_mod",
    "update_profile_active_mod",
)
DISABLE_CANDIDATES = (
    "disable_profile_mod",
    "disable_profile_package",
    "set_profile_mod_disabled",
    "profile_disable_mod",
    "update_profile_active_mod",
)


def emit(payload):
    print(MARKER + json.dumps(payload, sort_keys=True))


def to_plain(value):
    if dataclasses.is_dataclass(value):
        return {field.name: to_plain(getattr(value, field.name)) for field in dataclasses.fields(value)}
    if isinstance(value, Path):
        return str(value)
    if isinstance(value, dict):
        return {str(key): to_plain(val) for key, val in value.items()}
    if isinstance(value, (list, tuple, set)):
        return [to_plain(item) for item in value]
    return value


def fail(contract, message, **details):
    emit({
        "ok": False,
        "scenario": SCENARIO,
        "contract": contract,
        "message": message,
        "details": to_plain(details),
    })


def public_callable(names):
    for name in names:
        value = getattr(mod, name, None)
        if callable(value):
            return name, value
    return None, None


def profile_dir_for(profile_id=PROFILE_ID):
    return STAGING_DIR / profile_id


def profile_json_path(profile_dir):
    return mod.profile_json_path(profile_dir)


def create_profile(profile_id=PROFILE_ID):
    profile_dir = profile_dir_for(profile_id)
    FAKE_BARONY.parent.mkdir(parents=True, exist_ok=True)
    if not FAKE_BARONY.exists():
        FAKE_BARONY.write_text("#!/bin/sh\\necho fake barony\\n", encoding="utf-8")
        FAKE_BARONY.chmod(0o755)
    args = argparse.Namespace(
        profile_dir=str(profile_dir),
        profile_id=profile_id,
        barony_executable=str(FAKE_BARONY),
        steam=False,
        steam_manifest=None,
        steam_install=None,
        runtime_info=None,
    )
    stdout = io.StringIO()
    stderr = io.StringIO()
    with contextlib.redirect_stdout(stdout), contextlib.redirect_stderr(stderr):
        exit_code = mod.command_profile_create(args)
    if exit_code != 0:
        fail(
            "Profile setup",
            "Existing profile create command failed while preparing hermetic state.",
            exitCode=exit_code,
            stdout=stdout.getvalue(),
            stderr=stderr.getvalue(),
            profileDir=profile_dir,
        )
        return None
    return profile_dir


def cli_enable(profile_dir, package_path=ELIXIRS_PKG):
    args = argparse.Namespace(profile_dir=str(profile_dir), package=str(package_path))
    stdout = io.StringIO()
    stderr = io.StringIO()
    with contextlib.redirect_stdout(stdout), contextlib.redirect_stderr(stderr):
        exit_code = mod.command_profile_enable(args)
    if exit_code != 0:
        fail(
            "Profile setup",
            "Existing profile enable command failed while preparing active-mod state.",
            exitCode=exit_code,
            stdout=stdout.getvalue(),
            stderr=stderr.getvalue(),
            profileDir=profile_dir,
            packagePath=package_path,
        )
        return False
    return True


def load_package_manifest(package_path=ELIXIRS_PKG):
    package, result = mod.load_package(str(package_path))
    if package is None or not result.ok:
        fail(
            "Profile setup",
            "Could not load the package fixture for profile BDD setup.",
            problems=[to_plain(problem) for problem in result.problems],
            packagePath=package_path,
        )
        return None
    return package.manifest


def call_state_api(profile_dir, *, package_path=None, context=None):
    api_name, api = public_callable(PROFILE_STATE_CANDIDATES)
    if api is None:
        fail(
            "Profile semantic state API",
            "Missing semantic profile state API. Expected one of: " + ", ".join(PROFILE_STATE_CANDIDATES),
            candidateNames=list(PROFILE_STATE_CANDIDATES),
            candidatePurpose="Return a profile DTO/state object with profile id, profile-local paths, active mod projection, warnings, and product path metadata.",
            profileDir=profile_dir,
            packagePath=package_path,
        )
        return None

    raw_profile = None
    try:
        raw_profile, _, _ = mod.load_profile(str(profile_dir))
    except Exception:
        raw_profile = None

    variants = []
    if package_path is not None:
        variants.extend([
            ("profile-package-positional", lambda: api(profile_dir, package_path)),
            ("profile-package-string-positional", lambda: api(str(profile_dir), str(package_path))),
            ("profile-package-root-keyword", lambda: api(profile_dir=profile_dir, package_root=package_path)),
            ("profile-package-keyword", lambda: api(profile_dir=profile_dir, package_path=package_path)),
            ("profile-package-string-keyword", lambda: api(profile_dir=str(profile_dir), package_path=str(package_path))),
            ("profile-package-arg-keyword", lambda: api(profile_dir=profile_dir, package=str(package_path))),
            ("profile-dict-package-keyword", lambda: api(profile=raw_profile, profile_dir=profile_dir, package_path=package_path)),
        ])
    variants.extend([
        ("path-positional", lambda: api(profile_dir)),
        ("string-path-positional", lambda: api(str(profile_dir))),
        ("profile-dir-keyword", lambda: api(profile_dir=profile_dir)),
        ("profile-dir-string-keyword", lambda: api(profile_dir=str(profile_dir))),
        ("profile-root-keyword", lambda: api(profile_root=profile_dir)),
        ("profile-path-keyword", lambda: api(profile_path=profile_json_path(profile_dir))),
        ("profile-dict-keyword", lambda: api(profile=raw_profile, profile_dir=profile_dir)),
    ])
    if context is not None:
        variants.extend([
            ("context-keyword", lambda: api(profile_dir=profile_dir, context=context)),
            ("fixture-keyword", lambda: api(profile_fixture=context)),
        ])

    errors = []
    for label, thunk in variants:
        try:
            response = thunk()
            return {"apiName": api_name, "callVariant": label, "response": to_plain(response)}
        except TypeError as exc:
            errors.append(f"{label}: {exc}")
        except Exception as exc:
            errors.append(f"{label}: {type(exc).__name__}: {exc}")
    fail(
        "Profile semantic state API",
        f"{api_name} exists but has no compatible Cucumber contract call signature for Profile BDD.",
        apiName=api_name,
        callErrors=errors,
        profileDir=profile_dir,
        packagePath=package_path,
    )
    return None


def call_mutation_api(names, operation, profile_dir, *, package_path=None, mod_id=None):
    api_name, api = public_callable(names)
    if api is None:
        fail(
            "Profile semantic mutation API",
            f"Missing semantic profile {operation} API. Expected one of: " + ", ".join(names),
            candidateNames=list(names),
            candidatePurpose="Return mutation DTOs that distinguish changed transitions from already-applied no-ops without relying on raw CLI stdout.",
            profileDir=profile_dir,
            packagePath=package_path,
            modId=mod_id,
        )
        return None

    variants = []
    if package_path is not None:
        variants.extend([
            ("profile-package-positional", lambda: api(profile_dir, package_path)),
            ("profile-package-keyword", lambda: api(profile_dir=profile_dir, package_path=package_path)),
            ("profile-package-string-keyword", lambda: api(profile_dir=str(profile_dir), package_path=str(package_path))),
            ("profile-package-arg-keyword", lambda: api(profile_dir=profile_dir, package=str(package_path))),
        ])
    if mod_id is not None:
        variants.extend([
            ("profile-mod-positional", lambda: api(profile_dir, mod_id)),
            ("profile-mod-keyword", lambda: api(profile_dir=profile_dir, mod_id=mod_id)),
            ("profile-mod-string-keyword", lambda: api(profile_dir=str(profile_dir), mod_id=mod_id)),
        ])
    errors = []
    for label, thunk in variants:
        try:
            response = thunk()
            return {"apiName": api_name, "callVariant": label, "response": to_plain(response)}
        except TypeError as exc:
            errors.append(f"{label}: {exc}")
        except Exception as exc:
            errors.append(f"{label}: {type(exc).__name__}: {exc}")
    fail(
        "Profile semantic mutation API",
        f"{api_name} exists but has no compatible Cucumber contract call signature for semantic profile {operation}.",
        apiName=api_name,
        callErrors=errors,
        profileDir=profile_dir,
        packagePath=package_path,
        modId=mod_id,
    )
    return None


def flatten_strings(value):
    strings = []
    if isinstance(value, str):
        strings.append(value)
    elif isinstance(value, dict):
        for item in value.values():
            strings.extend(flatten_strings(item))
    elif isinstance(value, list):
        for item in value:
            strings.extend(flatten_strings(item))
    return strings


def flatten_keyed_values(value, path=()):
    items = []
    if isinstance(value, dict):
        for key, item in value.items():
            items.extend(flatten_keyed_values(item, (*path, str(key))))
    elif isinstance(value, list):
        for index, item in enumerate(value):
            items.extend(flatten_keyed_values(item, (*path, str(index))))
    else:
        items.append((path, value))
    return items


def collect_objects(value):
    objects = []
    if isinstance(value, dict):
        objects.append(value)
        for item in value.values():
            objects.extend(collect_objects(item))
    elif isinstance(value, list):
        for item in value:
            objects.extend(collect_objects(item))
    return objects


def norm_key(key_path):
    return "".join(key_path).replace("_", "").replace("-", "").lower()


def find_numeric_projection(response, expected):
    accepted = {"activemodcount", "enabledmodcount", "activecount", "modcount", "enabledcount"}
    for key_path, value in flatten_keyed_values(response):
        if norm_key(key_path) in accepted and isinstance(value, int):
            return value == expected, {"key": ".".join(key_path), "value": value}
    return False, None


def bool_values(response):
    return {".".join(key): value for key, value in flatten_keyed_values(response) if isinstance(value, bool)}


def response_text(response):
    return "\\n".join(flatten_strings(response)).lower()


def is_noop_response(response):
    bools = bool_values(response)
    if any(key.lower().endswith(("changed", "created", "updated", "removed")) and value is False for key, value in bools.items()):
        return True
    text = response_text(response)
    return any(token in text for token in ("noop", "no-op", "unchanged", "already enabled", "already disabled", "already inactive"))


def is_changed_response(response):
    bools = bool_values(response)
    if any(key.lower().endswith(("changed", "created", "updated", "removed")) and value is True for key, value in bools.items()):
        return True
    text = response_text(response)
    return any(token in text for token in ("enabled", "disabled", "changed", "updated")) and not is_noop_response(response)


def profile_path_strings(response):
    paths = []
    for key_path, value in flatten_keyed_values(response):
        key = norm_key(key_path)
        if isinstance(value, str) and any(token in key for token in ("path", "root", "dir", "directory", "executable")):
            paths.append(value)
    return paths


def warning_entries(value):
    entries = []
    if isinstance(value, dict):
        for key, item in value.items():
            key_text = str(key).replace("_", "").replace("-", "").lower()
            if "warning" in key_text:
                if isinstance(item, list):
                    entries.extend(item)
                else:
                    entries.append(item)
            else:
                entries.extend(warning_entries(item))
        severity = str(value.get("severity") or value.get("level") or "").lower()
        if severity == "warning":
            entries.append(value)
    elif isinstance(value, list):
        for item in value:
            entries.extend(warning_entries(item))
    return entries


def stale_warning_objects(response):
    candidates = []
    for entry in warning_entries(response):
        text = json.dumps(entry, sort_keys=True).lower() if not isinstance(entry, str) else entry.lower()
        severity = "warning" if isinstance(entry, str) else str(entry.get("severity") or entry.get("level") or "warning").lower()
        if "stale" in text or "outdated" in text or "checksum" in text:
            candidates.append({"object": entry, "severity": severity, "text": text})
    return candidates


def run_stable_creation():
    profile_dir = create_profile(PROFILE_ID)
    if profile_dir is None:
        return
    state = call_state_api(profile_dir)
    if state is None:
        return
    response = state["response"]
    text = response_text(response)
    paths = profile_path_strings(response)
    if PROFILE_ID.lower() not in text:
        fail(
            "Stable profile creation",
            "Semantic profile state did not expose the created profile id.",
            expectedProfileId=PROFILE_ID,
            api=state,
        )
        return
    if not any(str(profile_dir) in path for path in paths):
        fail(
            "Stable profile creation",
            "Semantic profile state did not expose profile-local paths under the hermetic profile directory.",
            expectedProfileDir=profile_dir,
            observedPaths=paths,
            api=state,
        )
        return
    emit({"ok": True, "scenario": SCENARIO, "assertion": "stable profile creation", "observations": {"profileDir": str(profile_dir), "api": state["apiName"], "callVariant": state["callVariant"], "pathCount": len(paths)}})


def run_reload_without_rewrite():
    profile_dir = create_profile(PROFILE_ID)
    if profile_dir is None:
        return
    path = profile_json_path(profile_dir)
    before_bytes = path.read_bytes()
    before_mtime = path.stat().st_mtime_ns
    first = call_state_api(profile_dir)
    if first is None:
        return
    second = call_state_api(profile_dir)
    if second is None:
        return
    after_bytes = path.read_bytes()
    after_mtime = path.stat().st_mtime_ns
    if before_bytes != after_bytes or before_mtime != after_mtime:
        fail(
            "Profile reload without rewrite",
            "Reloading semantic profile state rewrote profile.json even though no state transition was requested.",
            profilePath=path,
            beforeMtimeNs=before_mtime,
            afterMtimeNs=after_mtime,
            bytesChanged=before_bytes != after_bytes,
            first=first,
            second=second,
        )
        return
    if PROFILE_ID.lower() not in response_text(second["response"]):
        fail("Profile reload without rewrite", "Reloaded semantic state did not preserve the profile id.", expectedProfileId=PROFILE_ID, api=second)
        return
    emit({"ok": True, "scenario": SCENARIO, "assertion": "reload did not rewrite", "observations": {"profilePath": str(path), "mtimeNs": after_mtime, "api": second["apiName"]}})


def run_active_count():
    profile_dir = create_profile(PROFILE_ID)
    if profile_dir is None:
        return
    if not cli_enable(profile_dir):
        return
    state = call_state_api(profile_dir, package_path=ELIXIRS_PKG)
    if state is None:
        return
    ok, projection = find_numeric_projection(state["response"], 1)
    if not ok:
        fail(
            "Active mod count projection",
            "Semantic profile state did not project activeModCount=1 after one enabled package.",
            expectedActiveModCount=1,
            observedProjection=projection,
            candidateFields=["activeModCount", "enabledModCount", "activeCount", "modCount"],
            api=state,
        )
        return
    emit({"ok": True, "scenario": SCENARIO, "assertion": "active mod count projection", "observations": {"projection": projection, "api": state["apiName"]}})


def run_stale_warning():
    profile_dir = create_profile(PROFILE_ID)
    if profile_dir is None:
        return
    manifest = load_package_manifest()
    if manifest is None:
        return
    stale_entry = {
        "id": manifest.get("id"),
        "version": manifest.get("version"),
        "packagePath": str(ELIXIRS_PKG),
        "checksumSet": "sha256:stale-profile-digest",
        "enabledAt": "2026-01-01T00:00:00Z",
    }
    profile, _, result = mod.load_profile(str(profile_dir))
    if profile is None or not result.ok:
        fail("Profile setup", "Could not reload profile before writing stale active mod fixture.", problems=[to_plain(problem) for problem in result.problems])
        return
    mod.write_profile_active_mods(profile_dir, profile, [stale_entry], "2026-01-01T00:00:00Z")
    state = call_state_api(profile_dir, package_path=ELIXIRS_PKG, context={"expectedPackage": to_plain(manifest), "staleEntry": stale_entry})
    if state is None:
        return
    stale = stale_warning_objects(state["response"])
    if not stale:
        fail(
            "Stale active mods warning",
            "Semantic profile state did not expose a stale active-mod warning for an enabled id with an outdated version.",
            expectedWarning={"id": manifest.get("id"), "activeVersion": stale_entry["version"], "packageVersion": manifest.get("version")},
            api=state,
        )
        return
    if any(item["severity"] in ("fatal", "error", "blocked") for item in stale):
        fail(
            "Stale active mods warning",
            "Stale active mods must be warning-level semantic diagnostics, not fatal launch/readiness blockers at the profile projection layer.",
            staleObjects=stale,
            api=state,
        )
        return
    emit({"ok": True, "scenario": SCENARIO, "assertion": "stale active mods warning", "observations": {"warningCount": len(stale), "api": state["apiName"]}})


def run_idempotence():
    profile_dir = create_profile(PROFILE_ID)
    if profile_dir is None:
        return
    manifest = load_package_manifest()
    if manifest is None:
        return
    first_enable = call_mutation_api(ENABLE_CANDIDATES, "enable", profile_dir, package_path=ELIXIRS_PKG)
    if first_enable is None:
        return
    second_enable = call_mutation_api(ENABLE_CANDIDATES, "enable", profile_dir, package_path=ELIXIRS_PKG)
    if second_enable is None:
        return
    first_disable = call_mutation_api(DISABLE_CANDIDATES, "disable", profile_dir, mod_id=manifest.get("id"))
    if first_disable is None:
        return
    second_disable = call_mutation_api(DISABLE_CANDIDATES, "disable", profile_dir, mod_id=manifest.get("id"))
    if second_disable is None:
        return
    checks = {
        "firstEnableChanged": is_changed_response(first_enable["response"]),
        "secondEnableNoop": is_noop_response(second_enable["response"]),
        "firstDisableChanged": is_changed_response(first_disable["response"]),
        "secondDisableNoop": is_noop_response(second_disable["response"]),
    }
    if not all(checks.values()):
        fail(
            "Enable/disable idempotence",
            "Semantic profile mutation DTOs did not distinguish first state transitions from repeated idempotent no-ops.",
            checks=checks,
            firstEnable=first_enable,
            secondEnable=second_enable,
            firstDisable=first_disable,
            secondDisable=second_disable,
        )
        return
    emit({"ok": True, "scenario": SCENARIO, "assertion": "enable disable idempotence", "observations": checks})


def run_no_tmp_paths():
    profile_dir = create_profile(PROFILE_ID)
    if profile_dir is None:
        return
    state = call_state_api(profile_dir)
    if state is None:
        return
    paths = profile_path_strings(state["response"])
    if not paths:
        fail(
            "No .tmp product path",
            "Semantic profile state did not expose product/profile path fields to audit for .tmp leakage.",
            candidatePathFields=["profilePath", "profileRoot", "bmlRoot", "logsDir", "reportsDir", "stateDir", "runtimeManifestPath"],
            api=state,
        )
        return
    tmp_paths = [value for value in paths if ".tmp" in value]
    if tmp_paths:
        fail(
            "No .tmp product path",
            "Semantic profile state exposed .tmp in a product/profile path.",
            tmpPaths=tmp_paths,
            allPaths=paths,
            api=state,
        )
        return
    emit({"ok": True, "scenario": SCENARIO, "assertion": "no .tmp product path", "observations": {"pathCount": len(paths), "api": state["apiName"]}})


SCENARIOS = {
    "stable-creation": run_stable_creation,
    "reload-without-rewrite": run_reload_without_rewrite,
    "active-count": run_active_count,
    "stale-warning": run_stale_warning,
    "enable-disable-idempotence": run_idempotence,
    "no-tmp-path": run_no_tmp_paths,
}

handler = SCENARIOS.get(SCENARIO)
if handler is None:
    fail("Profile BDD harness", f"Unknown Profile BDD scenario: {SCENARIO}", knownScenarios=sorted(SCENARIOS))
else:
    handler()
`;
}

function requireProfileContract(world, expectedAssertion) {
  const result = world.profileContract;
  if (!result) throw new Error("No Profile BDD contract result was recorded.");
  if (!result.ok) {
    const details = result.details ? `\n${JSON.stringify(result.details, null, 2)}` : "";
    throw new Error(`${result.contract || "Profile BDD contract"} failed: ${result.message || "unsatisfied"}${details}`);
  }
  if (expectedAssertion && result.assertion !== expectedAssertion) {
    throw new Error(`Expected Profile BDD assertion '${expectedAssertion}', got '${result.assertion}'.\n${JSON.stringify(result, null, 2)}`);
  }
  return result;
}

Given("a hermetic Profile BDD workspace with a fake Barony executable", function () {
  this.profileBddWorkspace = fs.mkdtempSync(path.join(os.tmpdir(), "bml-profile-bdd-"));
  this.profileBddFakeBarony = path.join(this.profileBddWorkspace, "fake-barony", "barony.x86_64");
  fs.mkdirSync(path.dirname(this.profileBddFakeBarony), { recursive: true });
  fs.writeFileSync(this.profileBddFakeBarony, "#!/bin/sh\necho fake barony\n", "utf-8");
  fs.chmodSync(this.profileBddFakeBarony, 0o755);
});

After(function () {
  if (this.profileBddWorkspace) {
    fs.rmSync(this.profileBddWorkspace, { recursive: true, force: true });
  }
});

Given("the Profile BDD contract imports the BaronyModLoader Python app module", function () {
  this.profileBddAppModule = BML_APP;
});

When("I create a Profile BDD profile named {string}", function (profileId) {
  runProfilePython(profileScript(this, "stable-creation", profileId), this);
});

Then("the Profile BDD state exposes a stable profile id and profile-local paths", function () {
  requireProfileContract(this, "stable profile creation");
});

When("I reload a Profile BDD profile without changing it", function () {
  runProfilePython(profileScript(this, "reload-without-rewrite", "reload-profile"), this);
});

Then("the Profile BDD reload reports unchanged state and no profile rewrite", function () {
  requireProfileContract(this, "reload did not rewrite");
});

When("I enable the Runebound package for a Profile BDD profile", function () {
  runProfilePython(profileScript(this, "active-count", "active-count-profile"), this);
});

Then("the Profile BDD state projects an active mod count of {int}", function (expected) {
  const result = requireProfileContract(this, "active mod count projection");
  const observed = result.observations && result.observations.projection && result.observations.projection.value;
  if (observed !== expected) {
    throw new Error(`Expected active mod count ${expected}, got ${observed}.\n${JSON.stringify(result, null, 2)}`);
  }
});

When("I load a Profile BDD profile with a stale active Runebound entry", function () {
  runProfilePython(profileScript(this, "stale-warning", "stale-profile"), this);
});

Then("the Profile BDD state warns about stale active mods semantically", function () {
  requireProfileContract(this, "stale active mods warning");
});

When("I enable and disable the same Profile BDD mod repeatedly", function () {
  runProfilePython(profileScript(this, "enable-disable-idempotence", "idempotence-profile"), this);
});

Then("the Profile BDD mutation results are idempotent semantic state transitions", function () {
  requireProfileContract(this, "enable disable idempotence");
});

When("I inspect the Profile BDD product paths for a created profile", function () {
  runProfilePython(profileScript(this, "no-tmp-path", "no-tmp-profile"), this);
});

Then("no Profile BDD product path uses a .tmp location", function () {
  requireProfileContract(this, "no .tmp product path");
});
