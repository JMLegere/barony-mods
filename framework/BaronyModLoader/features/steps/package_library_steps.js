"use strict";

const { After, Given, When, Then } = require("@cucumber/cucumber");
const { execFileSync } = require("child_process");
const fs = require("fs");
const os = require("os");
const path = require("path");

// paths: <repo>/framework/BaronyModLoader/features/steps/package_library_steps.js
//          ../../../../ from here = <repo>
const REPO_ROOT = path.resolve(__dirname, "../../../../");
const BML_APP = path.join(REPO_ROOT, "framework/BaronyModLoader/app/barony_mod_loader.py");
const ELIXIRS_PKG = path.join(REPO_ROOT, "mods/runebound-elixirs");
const STASH_PKG = path.join(REPO_ROOT, "mods/stash");

function runPackageLibraryPython(script, world) {
  const tmp = path.join(os.tmpdir(), `bml-package-library-${Date.now()}-${Math.random().toString(36).slice(2)}.py`);
  fs.writeFileSync(tmp, script, "utf-8");
  try {
    const stdout = execFileSync("/usr/bin/python3", [tmp], {
      cwd: REPO_ROOT,
      encoding: "utf-8",
      timeout: 30000,
      stdio: ["ignore", "pipe", "pipe"],
    });
    const marker = "__BML_PACKAGE_LIBRARY_JSON__";
    const line = stdout.split(/\r?\n/).find(entry => entry.startsWith(marker));
    if (!line) {
      throw new Error(`Package Library script did not emit JSON marker. Output:\n${stdout}`);
    }
    world.packageLibraryContract = JSON.parse(line.slice(marker.length));
  } catch (err) {
    const stdout = err.stdout ? String(err.stdout) : "";
    const stderr = err.stderr ? String(err.stderr) : "";
    throw new Error(`Package Library contract script failed (exit ${err.status ?? 1}):\nSTDOUT:\n${stdout}\nSTDERR:\n${stderr}`);
  } finally {
    try { fs.unlinkSync(tmp); } catch (_) {}
  }
}

function pyPrelude(stagingDir) {
  return `
import argparse
import dataclasses
import importlib.util
import json
from pathlib import Path
import shutil
import stat
import sys

REPO_ROOT = Path(${JSON.stringify(REPO_ROOT)})
BML_APP = Path(${JSON.stringify(BML_APP)})
ELIXIRS_PKG = Path(${JSON.stringify(ELIXIRS_PKG)})
STASH_PKG = Path(${JSON.stringify(STASH_PKG)})
STAGING_DIR = Path(${JSON.stringify(stagingDir)})

spec = importlib.util.spec_from_file_location("bml_app", BML_APP)
mod = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = mod
spec.loader.exec_module(mod)

def emit(payload):
    print("__BML_PACKAGE_LIBRARY_JSON__" + json.dumps(payload, sort_keys=True))

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

def public_callable(names):
    for name in names:
        value = getattr(mod, name, None)
        if callable(value):
            return name, value
    return None, None

def call_first_success(fn, variants):
    errors = []
    for label, thunk in variants:
        try:
            return label, thunk()
        except TypeError as exc:
            errors.append(f"{label}: {exc}")
    raise AssertionError("No compatible call signature. Tried: " + " | ".join(errors))

def as_items(value):
    plain = to_plain(value)
    if isinstance(plain, dict):
        for key in ("packages", "summaries", "items", "entries", "rows", "cards"):
            if isinstance(plain.get(key), list):
                return plain[key]
    if isinstance(plain, list):
        return plain
    return []

def semantic_gap(contract, message, candidates, extra=None):
    details = {"candidateNames": list(candidates)}
    if extra:
        details.update(extra)
    emit({"ok": False, "contract": contract, "message": message, "details": details})

def clone_package(source, destination, *, version=None, package_id=None):
    if destination.exists():
        shutil.rmtree(destination)
    shutil.copytree(source, destination)
    manifest_path = destination / "bml-package.json"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    if version is not None:
        manifest["version"] = version
    if package_id is not None:
        manifest["id"] = package_id
    manifest_path.write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\\n", encoding="utf-8")
    return destination

def write_fake_executable(path):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("#!/bin/sh\\nexit 0\\n", encoding="utf-8")
    path.chmod(path.stat().st_mode | stat.S_IXUSR)
    return path

def create_profile(profile_dir):
    fake_exe = write_fake_executable(STAGING_DIR / "fake-game" / "barony.x86_64")
    args = argparse.Namespace(
        profile_dir=str(profile_dir),
        profile_id="package-library-profile",
        barony_executable=str(fake_exe),
        steam=False,
        steam_install=None,
        steam_manifest=None,
        runtime_info=None,
    )
    code = int(mod.command_profile_create(args))
    profile, loaded_dir, result = mod.load_profile(str(profile_dir))
    if code != 0 or profile is None or not result.ok:
        raise AssertionError({"profileCreateCode": code, "profileProblems": [to_plain(problem) for problem in result.problems]})
    return profile, loaded_dir

def scan_catalog_or_emit(*roots):
    candidates = (
        "scan_package_catalog",
        "package_library_scan",
        "scan_package_library",
        "package_catalog_scan",
        "build_package_catalog",
        "scan_local_package_catalog",
        "package_catalog_summaries",
    )
    name, fn = public_callable(candidates)
    if fn is None:
        semantic_gap("Package Library local scan API", "Missing semantic Package Library scan API.", candidates)
        return None
    label, value = call_first_success(fn, [
        ("roots-list", lambda: fn(list(roots))),
        ("roots-positional", lambda: fn(*roots)),
        ("roots-keyword", lambda: fn(roots=list(roots))),
        ("mods-root-keyword", lambda: fn(mods_root=roots[0], extra_roots=list(roots[1:]))),
    ])
    return {"apiName": name, "callVariant": label, "catalog": to_plain(value), "items": as_items(value)}

def enable_package_or_gap(profile_dir, package_path):
    candidates = (
        "enable_package_for_profile",
        "package_library_enable_package",
        "enable_package_in_profile",
        "set_profile_package_enabled",
        "activate_package_for_profile",
        "command_profile_enable",
    )
    name, fn = public_callable(candidates)
    if fn is None:
        semantic_gap("Package Library profile enable API", "Missing semantic Package Library enable API.", candidates)
        return None
    if name == "command_profile_enable":
        label, value = "command-profile-enable", fn(argparse.Namespace(profile_dir=str(profile_dir), package=str(package_path)))
    else:
        label, value = call_first_success(fn, [
            ("path-objects", lambda: fn(profile_dir, package_path)),
            ("path-strings", lambda: fn(str(profile_dir), str(package_path))),
            ("keywords", lambda: fn(profile_dir=str(profile_dir), package=str(package_path))),
            ("profile-package-keywords", lambda: fn(profile=str(profile_dir), package_path=str(package_path))),
        ])
    return {"apiName": name, "callVariant": label, "response": to_plain(value)}

def disable_package_or_gap(profile_dir, package_id):
    candidates = (
        "disable_package_for_profile",
        "package_library_disable_package",
        "disable_package_in_profile",
        "set_profile_package_disabled",
        "deactivate_package_for_profile",
        "command_profile_disable",
    )
    name, fn = public_callable(candidates)
    if fn is None:
        semantic_gap("Package Library profile disable API", "Missing semantic Package Library disable API.", candidates)
        return None
    if name == "command_profile_disable":
        label, value = "command-profile-disable", fn(argparse.Namespace(profile_dir=str(profile_dir), mod_id=package_id))
    else:
        label, value = call_first_success(fn, [
            ("path-id", lambda: fn(profile_dir, package_id)),
            ("path-id-strings", lambda: fn(str(profile_dir), package_id)),
            ("keywords", lambda: fn(profile_dir=str(profile_dir), package_id=package_id)),
            ("profile-mod-keywords", lambda: fn(profile=str(profile_dir), mod_id=package_id)),
        ])
    return {"apiName": name, "callVariant": label, "response": to_plain(value)}

def active_mods_for(profile_dir):
    profile, loaded_dir, result = mod.load_profile(str(profile_dir))
    return profile, loaded_dir, result, mod.profile_authoritative_mods(profile or {}, loaded_dir)
`;
}

function assertPackageLibraryOk(world) {
  const result = world.packageLibraryContract;
  if (!result) throw new Error("No Package Library contract result was recorded.");
  if (!result.ok) {
    const details = result.details ? `\n${JSON.stringify(result.details, null, 2)}` : "";
    throw new Error(`${result.contract || "Package Library contract"} failed: ${result.message || "unsatisfied"}${details}`);
  }
  return result;
}

function itemId(item) {
  return item && (item.id || item.packageId || item.modId);
}

function itemStatus(item) {
  return String(item?.validationStatus ?? item?.status ?? item?.valid ?? "").toLowerCase();
}

function itemVersion(item) {
  return item && String(item.version || item.packageVersion || "");
}

function itemPath(item) {
  return item && String(item.path || item.packagePath || item.root || item.manifestPath || "");
}

function isRunebound(item) {
  return itemId(item) === "jml.runebound-elixirs" || String(item?.name || "").includes("Runebound");
}

function collectStrings(value) {
  const strings = [];
  const visit = (node) => {
    if (node == null) return;
    if (typeof node === "string") {
      strings.push(node);
      return;
    }
    if (typeof node === "number" || typeof node === "boolean") {
      strings.push(String(node));
      return;
    }
    if (Array.isArray(node)) {
      node.forEach(visit);
      return;
    }
    if (typeof node === "object") {
      Object.values(node).forEach(visit);
    }
  };
  visit(value);
  return strings;
}

function collectPackageIds(value) {
  const ids = new Set();
  const visit = (node) => {
    if (node == null) return;
    if (typeof node === "string") {
      if (/^[a-z0-9_.-]+$/i.test(node)) ids.add(node);
      return;
    }
    if (Array.isArray(node)) {
      node.forEach(visit);
      return;
    }
    if (typeof node === "object") {
      for (const key of ["id", "packageId", "modId"]) {
        if (typeof node[key] === "string") ids.add(node[key]);
      }
      Object.values(node).forEach(visit);
    }
  };
  visit(value);
  return [...ids];
}

function profileActiveModsFromState(state) {
  return (
    state?.activeMods ||
    state?.active_mods ||
    state?.profile?.activeMods ||
    state?.profile?.active_mods ||
    []
  );
}

function modlistPlanFromState(state) {
  return state?.modlistPlan || state?.modlist_plan || state?.compatibilityPlan || state?.compatibility_plan;
}

Given("a clean Package Library staging directory", function () {
  this.packageLibraryStagingDir = fs.mkdtempSync(path.join(os.tmpdir(), "bml-package-library-"));
});

Given("a Package Library temp profile with a fake Barony executable", function () {
  if (!this.packageLibraryStagingDir) {
    this.packageLibraryStagingDir = fs.mkdtempSync(path.join(os.tmpdir(), "bml-package-library-"));
  }
  runPackageLibraryPython(pyPrelude(this.packageLibraryStagingDir) + `
try:
    profile_dir = STAGING_DIR / "profile"
    profile, loaded_dir = create_profile(profile_dir)
    emit({
        "ok": True,
        "contract": "Package Library temp profile setup",
        "profileDir": str(profile_dir),
        "profile": to_plain(profile),
    })
except Exception as exc:
    emit({"ok": False, "contract": "Package Library temp profile setup", "message": str(exc)})
`, this);
  const result = assertPackageLibraryOk(this);
  this.packageLibraryProfileDir = result.profileDir;
});

Given("Runebound Elixirs is installed and enabled from a Package Library store", function () {
  if (!this.packageLibraryProfileDir) throw new Error("Package Library profile was not created before installing Runebound Elixirs.");
  runPackageLibraryPython(pyPrelude(this.packageLibraryStagingDir) + `
try:
    profile_dir = Path(${JSON.stringify(this.packageLibraryProfileDir)})
    store_dir = STAGING_DIR / "package-store"
    package, load_result = mod.load_package(str(ELIXIRS_PKG))
    if package is None or not load_result.ok:
        raise AssertionError({"loadProblems": [to_plain(problem) for problem in load_result.problems]})
    installed_path, copied = mod.install_loaded_package(package, store_dir)
    enable = enable_package_or_gap(profile_dir, installed_path)
    if enable is None:
        raise SystemExit(0)
    profile, loaded_dir, result, active_mods = active_mods_for(profile_dir)
    emit({
        "ok": result.ok,
        "contract": "Package Library install and enable setup",
        "message": "Runebound must install and enable before disable-preserves-files can be exercised.",
        "enable": enable,
        "profileProblems": [to_plain(problem) for problem in result.problems],
        "activeMods": to_plain(active_mods),
        "installedPath": str(installed_path),
        "copiedFiles": copied,
    })
except Exception as exc:
    emit({"ok": False, "contract": "Package Library install and enable setup", "message": str(exc)})
`, this);
  const result = assertPackageLibraryOk(this);
  this.packageLibraryInstalledPath = result.installedPath;
});

After(function () {
  if (this.packageLibraryStagingDir) {
    fs.rmSync(this.packageLibraryStagingDir, { recursive: true, force: true });
  }
});

When("I ask the Package Library API to scan a local Runebound package root", function () {
  runPackageLibraryPython(pyPrelude(this.packageLibraryStagingDir) + `
try:
    local_root = STAGING_DIR / "local-mods"
    clone_package(ELIXIRS_PKG, local_root / "runebound-elixirs")
    result = scan_catalog_or_emit(local_root)
    if result is not None:
        result.update({"ok": True, "contract": "Package Library local mod package scan", "root": str(local_root)})
        emit(result)
except Exception as exc:
    emit({"ok": False, "contract": "Package Library local mod package scan", "message": str(exc)})
`, this);
});

When("I ask the Package Library API to scan a malformed package fixture", function () {
  runPackageLibraryPython(pyPrelude(this.packageLibraryStagingDir) + `
try:
    malformed_root = STAGING_DIR / "malformed-mods"
    malformed_pkg = malformed_root / "broken-package"
    malformed_pkg.mkdir(parents=True, exist_ok=True)
    (malformed_pkg / "bml-package.json").write_text("{ definitely-not-json", encoding="utf-8")
    result = scan_catalog_or_emit(malformed_root)
    if result is not None:
        result.update({"ok": True, "contract": "Package Library validation failure card", "root": str(malformed_root)})
        emit(result)
except Exception as exc:
    emit({"ok": False, "contract": "Package Library validation failure card", "message": str(exc)})
`, this);
});

When("I ask the Package Library API to enable Runebound Elixirs for that profile", function () {
  if (!this.packageLibraryProfileDir) throw new Error("Package Library profile was not created before enabling Runebound Elixirs.");
  runPackageLibraryPython(pyPrelude(this.packageLibraryStagingDir) + `
try:
    profile_dir = Path(${JSON.stringify(this.packageLibraryProfileDir)})
    package_path = clone_package(ELIXIRS_PKG, STAGING_DIR / "enable-source" / "runebound-elixirs")
    enable = enable_package_or_gap(profile_dir, package_path)
    if enable is None:
        raise SystemExit(0)
    profile, loaded_dir, result, active_mods = active_mods_for(profile_dir)
    emit({
        "ok": result.ok,
        "contract": "Package Library profile enable",
        "message": "Profile enable must produce semantic active package state.",
        "enable": enable,
        "profileProblems": [to_plain(problem) for problem in result.problems],
        "activeMods": to_plain(active_mods),
        "profileDir": str(profile_dir),
    })
except Exception as exc:
    emit({"ok": False, "contract": "Package Library profile enable", "message": str(exc)})
`, this);
});

When("I ask the Package Library API to disable Runebound Elixirs for that profile", function () {
  if (!this.packageLibraryProfileDir) throw new Error("Package Library profile was not created before disabling Runebound Elixirs.");
  if (!this.packageLibraryInstalledPath) throw new Error("Runebound Elixirs was not installed before disabling it.");
  runPackageLibraryPython(pyPrelude(this.packageLibraryStagingDir) + `
try:
    profile_dir = Path(${JSON.stringify(this.packageLibraryProfileDir)})
    installed_path = Path(${JSON.stringify(this.packageLibraryInstalledPath)})
    disable = disable_package_or_gap(profile_dir, "jml.runebound-elixirs")
    if disable is None:
        raise SystemExit(0)
    profile, loaded_dir, result, active_mods = active_mods_for(profile_dir)
    remaining_files = {
        "manifest": (installed_path / "bml-package.json").is_file(),
        "catalog": (installed_path / "content/data/bml/elixir-catalog.json").is_file(),
        "dropTables": (installed_path / "content/data/bml/elixir-drop-tables.json").is_file(),
    }
    emit({
        "ok": result.ok,
        "contract": "Package Library profile disable preserves installed files",
        "message": "Disabling a package must remove active state without deleting installed package files.",
        "disable": disable,
        "profileProblems": [to_plain(problem) for problem in result.problems],
        "activeMods": to_plain(active_mods),
        "installedPath": str(installed_path),
        "remainingFiles": remaining_files,
    })
except Exception as exc:
    emit({"ok": False, "contract": "Package Library profile disable preserves installed files", "message": str(exc)})
`, this);
});

When("I ask the Package Library API to scan two Runebound Elixirs versions", function () {
  runPackageLibraryPython(pyPrelude(this.packageLibraryStagingDir) + `
try:
    duplicates_root = STAGING_DIR / "duplicate-versions"
    clone_package(ELIXIRS_PKG, duplicates_root / "runebound-elixirs-0.1.0", version="0.1.0")
    clone_package(ELIXIRS_PKG, duplicates_root / "runebound-elixirs-0.1.1", version="0.1.1")
    result = scan_catalog_or_emit(duplicates_root)
    if result is not None:
        result.update({"ok": True, "contract": "Package Library duplicate package versions", "root": str(duplicates_root)})
        emit(result)
except Exception as exc:
    emit({"ok": False, "contract": "Package Library duplicate package versions", "message": str(exc)})
`, this);
});

Given("a Package Library temp profile with multiple active packages", function () {
  if (!this.packageLibraryStagingDir) {
    this.packageLibraryStagingDir = fs.mkdtempSync(path.join(os.tmpdir(), "bml-package-library-"));
  }
  runPackageLibraryPython(pyPrelude(this.packageLibraryStagingDir) + `
try:
    profile_dir = STAGING_DIR / "multi-active-profile"
    profile, loaded_dir = create_profile(profile_dir)
    now = mod.utc_now()
    active_mods = [
        {"id": "jml.runebound-elixirs", "version": "0.1.0", "packagePath": str(ELIXIRS_PKG), "enabledAt": now},
        {"id": "jml.stash", "version": "0.1.0", "packagePath": str(STASH_PKG), "enabledAt": now},
    ]
    mod.write_profile_active_mods(loaded_dir, profile, active_mods, now)
    reloaded, reloaded_dir, result, mods = active_mods_for(profile_dir)
    emit({
        "ok": result.ok,
        "contract": "Package Library multi-active profile setup",
        "profileDir": str(profile_dir),
        "activeMods": to_plain(mods),
        "profileProblems": [to_plain(problem) for problem in result.problems],
    })
except Exception as exc:
    emit({"ok": False, "contract": "Package Library multi-active profile setup", "message": str(exc)})
`, this);
  const result = assertPackageLibraryOk(this);
  this.packageLibraryProfileDir = result.profileDir;
});

When("I ask the Package Library API to evaluate the compatible multi-active modlist state", function () {
  if (!this.packageLibraryProfileDir) throw new Error("Package Library profile was not created before evaluating compatible multi-active modlist state.");
  runPackageLibraryPython(pyPrelude(this.packageLibraryStagingDir) + `
try:
    profile_dir = Path(${JSON.stringify(this.packageLibraryProfileDir)})
    profile, loaded_dir, result, active_mods = active_mods_for(profile_dir)
    if not result.ok:
        emit({
            "ok": False,
            "contract": "Package Library compatible multi-active modlist state",
            "message": "Profile setup did not load cleanly before state evaluation.",
            "details": {"profileProblems": [to_plain(problem) for problem in result.problems]},
        })
    else:
        state = mod.build_package_library_state(ELIXIRS_PKG, STASH_PKG, profile_dir=profile_dir, selected_package=ELIXIRS_PKG)
        emit({
            "ok": True,
            "contract": "Package Library compatible multi-active modlist state",
            "state": to_plain(state),
            "activeMods": to_plain(active_mods),
        })
except Exception as exc:
    emit({"ok": False, "contract": "Package Library compatible multi-active modlist state", "message": str(exc)})
`, this);
});

Then("the Package Library scan returns a semantic card for Runebound Elixirs", function () {
  const result = assertPackageLibraryOk(this);
  const cards = result.items || [];
  const runebound = cards.find(isRunebound);
  if (!runebound) throw new Error(`Runebound package card missing: ${JSON.stringify(cards, null, 2)}`);
  const status = itemStatus(runebound);
  if (!["valid", "ok", "true", "passed"].includes(status)) {
    throw new Error(`Runebound package should be semantically valid, got status ${status}: ${JSON.stringify(runebound, null, 2)}`);
  }
  for (const field of ["id", "version"]) {
    const value = runebound[field] || (field === "id" ? runebound.packageId : undefined);
    if (typeof value !== "string" || !value) {
      throw new Error(`Runebound card missing ${field}: ${JSON.stringify(runebound, null, 2)}`);
    }
  }
  const serialized = JSON.stringify(runebound);
  if (/^Package validation:/m.test(serialized) || /\bFAILED:\s/.test(serialized)) {
    throw new Error(`Runebound card appears to embed raw CLI/stdout text: ${serialized}`);
  }
});

Then("the Package Library returns a validation failure card with structured problems", function () {
  const result = assertPackageLibraryOk(this);
  const cards = result.items || [];
  const failure = cards.find(card => ["invalid", "error", "errors", "false", "failed", "malformed"].includes(itemStatus(card)) || card.valid === false);
  if (!failure) throw new Error(`No invalid package card found: ${JSON.stringify(cards, null, 2)}`);
  if (!Array.isArray(failure.problems) || failure.problems.length === 0) {
    throw new Error(`Invalid package card must expose structured problems: ${JSON.stringify(failure, null, 2)}`);
  }
  for (const problem of failure.problems) {
    if (typeof problem.code !== "string" || !problem.code || typeof problem.message !== "string" || !problem.message) {
      throw new Error(`Validation problem must include semantic code and message: ${JSON.stringify(problem, null, 2)}`);
    }
  }
  const serialized = JSON.stringify(failure);
  if (/^Package validation:/m.test(serialized) || /\bFAILED:\s/.test(serialized)) {
    throw new Error(`Failure card appears to embed raw CLI/stdout text: ${serialized}`);
  }
});

Then("the profile semantic active package state contains Runebound Elixirs as enabled", function () {
  const result = assertPackageLibraryOk(this);
  const mods = result.activeMods || [];
  const runebound = mods.find(mod => mod && mod.id === "jml.runebound-elixirs");
  if (!runebound) throw new Error(`Runebound package was not active: ${JSON.stringify(mods, null, 2)}`);
  if (runebound.version !== "0.1.0") {
    throw new Error(`Runebound active state preserved wrong version: ${JSON.stringify(runebound, null, 2)}`);
  }
  if (typeof runebound.packagePath !== "string" || !runebound.packagePath) {
    throw new Error(`Runebound active state missing packagePath: ${JSON.stringify(runebound, null, 2)}`);
  }
  if (typeof runebound.enabledAt !== "string" || !runebound.enabledAt) {
    throw new Error(`Runebound active state missing enabledAt: ${JSON.stringify(runebound, null, 2)}`);
  }
});

Then("Runebound Elixirs is inactive and its installed package files remain on disk", function () {
  const result = assertPackageLibraryOk(this);
  const mods = result.activeMods || [];
  const stillActive = mods.find(mod => mod && mod.id === "jml.runebound-elixirs");
  if (stillActive) throw new Error(`Runebound package remained active after disable: ${JSON.stringify(mods, null, 2)}`);
  const remaining = result.remainingFiles || {};
  const missing = Object.entries(remaining).filter(([, present]) => present !== true).map(([name]) => name);
  if (missing.length) {
    throw new Error(`Disable removed installed package files (${missing.join(", ")}): ${JSON.stringify(result, null, 2)}`);
  }
});

Then("the Package Library returns one semantic card per Runebound version without clobbering paths", function () {
  const result = assertPackageLibraryOk(this);
  const cards = (result.items || []).filter(isRunebound);
  const byVersion = new Map(cards.map(card => [itemVersion(card), card]));
  for (const version of ["0.1.0", "0.1.1"]) {
    if (!byVersion.has(version)) {
      throw new Error(`Runebound version ${version} missing from duplicate-version scan: ${JSON.stringify(cards, null, 2)}`);
    }
  }
  const paths = [itemPath(byVersion.get("0.1.0")), itemPath(byVersion.get("0.1.1"))].filter(Boolean);
  if (paths.length !== 2 || paths[0] === paths[1]) {
    throw new Error(`Duplicate versions must keep distinct package paths: ${JSON.stringify(cards, null, 2)}`);
  }
  for (const card of byVersion.values()) {
    const status = itemStatus(card);
    if (!["valid", "ok", "true", "passed"].includes(status)) {
      throw new Error(`Duplicate version card should remain valid, got ${status}: ${JSON.stringify(card, null, 2)}`);
    }
  }
});

Then("the Package Library represents multiple active packages as launchable modlist state", function () {
  const result = assertPackageLibraryOk(this);
  const state = result.state || {};
  const activeIds = collectPackageIds(profileActiveModsFromState(state));
  for (const packageId of ["jml.runebound-elixirs", "jml.stash"]) {
    if (!activeIds.includes(packageId)) {
      throw new Error(`Package Library state does not expose active package id ${packageId}: ${JSON.stringify(state, null, 2)}`);
    }
  }

  const disabledText = collectStrings(state.disabledReasons || state.disabled_reasons || []).join("\n");
  if (/multiple active|more than one active|one active package|one package at a time|disable all but one/i.test(disabledText)) {
    throw new Error(`Package Library state still exposes the retired one-active-package disabled reason: ${disabledText}`);
  }

  const plan = modlistPlanFromState(state);
  if (!plan || typeof plan !== "object") {
    throw new Error(`Package Library state must expose modlistPlan for multi-active profiles: ${JSON.stringify(state, null, 2)}`);
  }
  if (plan.launchable !== true) {
    throw new Error(`Compatible multi-active package state must be launchable: ${JSON.stringify(plan, null, 2)}`);
  }
  const planIds = collectPackageIds(plan.enabledMods || plan.enabled_mods || plan.activeMods || plan.active_mods || plan.loadOrder || plan.load_order || plan.packages || plan);
  for (const packageId of ["jml.runebound-elixirs", "jml.stash"]) {
    if (!planIds.includes(packageId)) {
      throw new Error(`modlistPlan does not represent active package id ${packageId}: ${JSON.stringify(plan, null, 2)}`);
    }
  }
});
