"use strict";

const { After, Given, When, Then } = require("@cucumber/cucumber");
const { execFileSync } = require("child_process");
const fs = require("fs");
const os = require("os");
const path = require("path");

// paths: <repo>/framework/BaronyModLoader/features/steps/core_service_contract_steps.js
//          ../../../../ from here = <repo>
const REPO_ROOT = path.resolve(__dirname, "../../../../");
const BML_APP = path.join(REPO_ROOT, "framework/BaronyModLoader/app/barony_mod_loader.py");
const MODS_ROOT = path.join(REPO_ROOT, "mods");
const ELIXIRS_PKG = path.join(REPO_ROOT, "mods/runebound-elixirs");
const RUNTIME_INFO = path.join(REPO_ROOT, "framework/BaronyModLoader/fixtures/runtime-info.installed-hook.stash.json");
const RUNTIME_REPORT_LOADED = path.join(REPO_ROOT, "framework/BaronyModLoader/fixtures/runtime-load-report.loaded.json");

function runContractPython(script, world) {
  const tmp = path.join(os.tmpdir(), `bml-core-contract-${Date.now()}-${Math.random().toString(36).slice(2)}.py`);
  fs.writeFileSync(tmp, script, "utf-8");
  try {
    const stdout = execFileSync("/usr/bin/python3", [tmp], {
      cwd: REPO_ROOT,
      encoding: "utf-8",
      timeout: 30000,
      stdio: ["ignore", "pipe", "pipe"],
    });
    const marker = "__BML_CONTRACT_JSON__";
    const line = stdout.split(/\r?\n/).find(entry => entry.startsWith(marker));
    if (!line) {
      throw new Error(`Contract script did not emit JSON marker. Output:\n${stdout}`);
    }
    world.coreContract = JSON.parse(line.slice(marker.length));
  } catch (err) {
    const stdout = err.stdout ? String(err.stdout) : "";
    const stderr = err.stderr ? String(err.stderr) : "";
    throw new Error(`Contract script failed (exit ${err.status ?? 1}):\nSTDOUT:\n${stdout}\nSTDERR:\n${stderr}`);
  } finally {
    try { fs.unlinkSync(tmp); } catch (_) {}
  }
}

function pyPrelude(extra = "") {
  return `
import argparse
import dataclasses
import hashlib
import importlib.util
import inspect
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

REPO_ROOT = Path(${JSON.stringify(REPO_ROOT)})
BML_APP = Path(${JSON.stringify(BML_APP)})
MODS_ROOT = Path(${JSON.stringify(MODS_ROOT)})
ELIXIRS_PKG = Path(${JSON.stringify(ELIXIRS_PKG)})
RUNTIME_INFO = Path(${JSON.stringify(RUNTIME_INFO)})
RUNTIME_REPORT_LOADED = Path(${JSON.stringify(RUNTIME_REPORT_LOADED)})
STAGING_DIR = Path(${JSON.stringify(extra)}) if ${JSON.stringify(extra)} else Path(tempfile.mkdtemp(prefix="bml-core-contract-py-"))

spec = importlib.util.spec_from_file_location("bml_app", BML_APP)
mod = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = mod
spec.loader.exec_module(mod)

def emit(payload):
    print("__BML_CONTRACT_JSON__" + json.dumps(payload, sort_keys=True))

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
        for key in ("packages", "summaries", "items", "entries", "rows", "diagnostics"):
            if isinstance(plain.get(key), list):
                return plain[key]
    if isinstance(plain, list):
        return plain
    return []
`;
}

function assertContractOk(world) {
  const result = world.coreContract;
  if (!result) throw new Error("No contract result was recorded.");
  if (!result.ok) {
    const details = result.details ? `\n${JSON.stringify(result.details, null, 2)}` : "";
    throw new Error(`${result.contract || "Core service contract"} failed: ${result.message || "unsatisfied"}${details}`);
  }
  return result;
}

function flattenValues(value, out = []) {
  if (value == null) return out;
  if (typeof value === "string") out.push(value);
  else if (Array.isArray(value)) value.forEach(v => flattenValues(v, out));
  else if (typeof value === "object") Object.values(value).forEach(v => flattenValues(v, out));
  return out;
}

Given("a clean Core Service Contract staging directory", function () {
  this.coreStagingDir = fs.mkdtempSync(path.join(os.tmpdir(), "bml-core-contract-"));
});

After(function () {
  if (this.coreStagingDir) {
    fs.rmSync(this.coreStagingDir, { recursive: true, force: true });
  }
});

When("I exercise the profile store contract with one enabled Runebound package", function () {
  runContractPython(pyPrelude(this.coreStagingDir) + `
try:
    profile_dir = STAGING_DIR / "stable-profile"
    fake_exe = STAGING_DIR / "game" / "barony.x86_64"
    fake_exe.parent.mkdir(parents=True, exist_ok=True)
    fake_exe.write_text("#!/bin/sh\\n", encoding="utf-8")
    create_args = argparse.Namespace(
        profile_dir=str(profile_dir),
        profile_id="core-service-contract-profile",
        barony_executable=str(fake_exe),
        steam=False,
        steam_install=None,
        steam_manifest=None,
        runtime_info=None,
    )
    create_code = int(mod.command_profile_create(create_args))
    profile, loaded_dir, load_result = mod.load_profile(str(profile_dir))
    package, package_result = mod.load_package(str(ELIXIRS_PKG))
    enable_args = argparse.Namespace(profile_dir=str(profile_dir), package=str(ELIXIRS_PKG))
    enable_code = int(mod.command_profile_enable(enable_args))
    reloaded, reloaded_dir, reloaded_result = mod.load_profile(str(profile_dir))
    active_mods = mod.profile_authoritative_mods(reloaded, reloaded_dir)
    profile_path = mod.profile_json_path(reloaded_dir)
    payload = {
        "ok": create_code == 0 and enable_code == 0 and load_result.ok and reloaded_result.ok,
        "contract": "profile store stable load/projection",
        "message": "profile create/load/enable must succeed through the public app-core profile surface",
        "profilePath": str(profile_path),
        "profileRoot": str(profile_dir.resolve()),
        "paths": to_plain(reloaded.get("paths", {})),
        "activeMods": to_plain(active_mods),
        "loadProblems": [to_plain(problem) for problem in load_result.problems + reloaded_result.problems + package_result.problems],
    }
    emit(payload)
except Exception as exc:
    emit({"ok": False, "contract": "profile store stable load/projection", "message": str(exc)})
`, this);
});

Then("the profile store returns stable profile paths without a product .tmp path", function () {
  const result = assertContractOk(this);
  const expectedProfilePath = path.join(result.profileRoot, "BaronyModLoader", "profile.json");
  if (path.resolve(result.profilePath) !== path.resolve(expectedProfilePath)) {
    throw new Error(`Profile JSON path is not stable. Expected ${expectedProfilePath}, got ${result.profilePath}`);
  }
  const pathValues = flattenValues(result.paths).filter(v => v.includes("BaronyModLoader") || v.includes(result.profileRoot));
  const tmpProductPaths = pathValues.filter(v => /(^|[/\\])\.tmp([/\\]|$)|\.tmp/.test(v));
  if (tmpProductPaths.length) {
    throw new Error(`Profile projection exposed product path(s) containing .tmp:\n${tmpProductPaths.join("\n")}`);
  }
});

Then("the active mods projection contains enabled package DTOs", function () {
  const result = assertContractOk(this);
  const mods = result.activeMods || [];
  const runebound = mods.find(m => m && m.id === "jml.runebound-elixirs");
  if (!runebound) throw new Error(`Runebound package was not present in active mods: ${JSON.stringify(mods, null, 2)}`);
  for (const field of ["id", "version", "packagePath", "enabledAt"]) {
    if (typeof runebound[field] !== "string" || !runebound[field]) {
      throw new Error(`Active mod DTO missing non-empty ${field}: ${JSON.stringify(runebound, null, 2)}`);
    }
  }
});

When("I ask the package catalog service to scan local mods with a malformed fixture", function () {
  runContractPython(pyPrelude(this.coreStagingDir) + `
try:
    invalid_root = STAGING_DIR / "invalid-mods"
    invalid_pkg = invalid_root / "broken-package"
    invalid_pkg.mkdir(parents=True, exist_ok=True)
    (invalid_pkg / "bml-package.json").write_text("{ definitely-not-json", encoding="utf-8")
    name, fn = public_callable([
        "scan_package_catalog",
        "package_catalog_scan",
        "build_package_catalog",
        "scan_local_package_catalog",
        "package_catalog_summaries",
    ])
    if fn is None:
        emit({
            "ok": False,
            "contract": "package catalog scan service",
            "message": "No public package catalog scan function found.",
            "details": {"expectedAnyOf": ["scan_package_catalog", "package_catalog_scan", "build_package_catalog", "scan_local_package_catalog", "package_catalog_summaries"]},
        })
    else:
        label, value = call_first_success(fn, [
            ("roots-list", lambda: fn([MODS_ROOT, invalid_root])),
            ("roots-kw", lambda: fn(roots=[MODS_ROOT, invalid_root])),
            ("mods-root-and-invalid-root", lambda: fn(MODS_ROOT, invalid_root)),
            ("mods-root-kw", lambda: fn(mods_root=MODS_ROOT, extra_roots=[invalid_root])),
            ("mods-root-only", lambda: fn(MODS_ROOT)),
        ])
        emit({"ok": True, "contract": "package catalog scan service", "function": name, "call": label, "catalog": to_plain(value), "items": as_items(value)})
except Exception as exc:
    emit({"ok": False, "contract": "package catalog scan service", "message": str(exc)})
`, this);
});

Then("the catalog contains semantic summaries for valid and invalid packages", function () {
  const result = assertContractOk(this);
  const items = result.items || [];
  if (!items.length) throw new Error(`Catalog returned no package summaries: ${JSON.stringify(result.catalog, null, 2)}`);
  const statuses = items.map(item => String(item.validationStatus ?? item.status ?? item.valid ?? "").toLowerCase());
  const hasValid = statuses.some(s => ["valid", "ok", "true", "passed"].includes(s));
  const hasInvalid = statuses.some(s => ["invalid", "error", "errors", "false", "failed", "malformed"].includes(s));
  if (!hasValid || !hasInvalid) {
    throw new Error(`Catalog must include both valid and invalid summaries. Statuses: ${statuses.join(", ")}\n${JSON.stringify(items, null, 2)}`);
  }
});

Then("the catalog includes the Runebound package validation status without raw stdout parsing", function () {
  const result = assertContractOk(this);
  const items = result.items || [];
  const runebound = items.find(item => item.id === "jml.runebound-elixirs" || item.packageId === "jml.runebound-elixirs" || String(item.name || "").includes("Runebound"));
  if (!runebound) throw new Error(`Runebound package summary missing from catalog: ${JSON.stringify(items, null, 2)}`);
  const serialized = JSON.stringify(runebound);
  if (/^Package validation:/m.test(serialized) || /\bFAILED:\s/.test(serialized)) {
    throw new Error(`Runebound summary appears to embed raw CLI/stdout text: ${serialized}`);
  }
  const status = String(runebound.validationStatus ?? runebound.status ?? runebound.valid ?? "").toLowerCase();
  if (!status) throw new Error(`Runebound summary lacks validation status: ${JSON.stringify(runebound, null, 2)}`);
});

When("I plan a runtime manifest with an intentionally stale package digest", function () {
  runContractPython(pyPrelude(this.coreStagingDir) + `
try:
    name, fn = public_callable([
        "plan_runtime_manifest",
        "build_runtime_manifest_plan",
        "runtime_manifest_plan",
        "plan_launch_manifest",
    ])
    if fn is None:
        emit({
            "ok": False,
            "contract": "pure runtime manifest planner",
            "message": "No public pure runtime manifest planner function found.",
            "details": {"expectedAnyOf": ["plan_runtime_manifest", "build_runtime_manifest_plan", "runtime_manifest_plan", "plan_launch_manifest"]},
        })
    else:
        package, package_result = mod.load_package(str(ELIXIRS_PKG))
        runtime_info, runtime_path, runtime_result = mod.load_runtime_info(str(RUNTIME_INFO))
        profile_dir = STAGING_DIR / "planner-profile"
        profile_dir.mkdir(parents=True, exist_ok=True)
        profile = {
            "profile": {"id": "planner-profile"},
            "runtime": {"gameSource": "manual", "baronyExecutable": str(STAGING_DIR / "game" / "barony.x86_64")},
            "activeMods": [{"id": "jml.runebound-elixirs", "version": "0.0.0", "checksumSet": "sha256:stale"}],
            "paths": {"profileRoot": str(profile_dir), "bmlRoot": str(profile_dir / "BaronyModLoader")},
        }
        out_path = profile_dir / "BaronyModLoader" / "manifests" / "runtime-manifest.json"
        original_run = subprocess.run
        def forbidden_run(*args, **kwargs):
            raise AssertionError("Runtime manifest planner attempted to launch a process")
        subprocess.run = forbidden_run
        try:
            label, value = call_first_success(fn, [
                ("keywords", lambda: fn(profile=profile, profile_dir=profile_dir, package=package, runtime_info=runtime_info, out_path=out_path, previous_digest="sha256:stale")),
                ("positional", lambda: fn(profile, profile_dir, package, runtime_info, out_path)),
                ("dto-keywords", lambda: fn(profile=profile, package=package, runtime_info=runtime_info, output_path=out_path)),
            ])
        finally:
            subprocess.run = original_run
        plain = to_plain(value)
        emit({"ok": True, "contract": "pure runtime manifest planner", "function": name, "call": label, "plan": plain, "manifestExists": out_path.exists()})
except Exception as exc:
    emit({"ok": False, "contract": "pure runtime manifest planner", "message": str(exc)})
`, this);
});

Then("the manifest planner returns a DTO with output path and manifest payload", function () {
  const result = assertContractOk(this);
  const plan = result.plan || {};
  const manifestPath = plan.runtimeManifestPath || plan.manifestPath || plan.outputPath || plan.outPath;
  if (typeof manifestPath !== "string" || !manifestPath.endsWith("runtime-manifest.json")) {
    throw new Error(`Planner DTO lacks runtime manifest output path: ${JSON.stringify(plan, null, 2)}`);
  }
  const manifest = plan.manifest || plan.runtimeManifest || plan.payload;
  if (!manifest || !manifest.launch || !Array.isArray(manifest.mods)) {
    throw new Error(`Planner DTO lacks manifest payload with launch and mods: ${JSON.stringify(plan, null, 2)}`);
  }
  if (result.manifestExists) {
    throw new Error("Pure planner wrote the runtime manifest to disk; this contract requires planning without side effects.");
  }
});

Then("the planner projects stale digest blockers without launching a process", function () {
  const result = assertContractOk(this);
  const plan = result.plan || {};
  const blockers = flattenValues(plan.blockers || plan.disabledReasons || plan.problems || []);
  if (!blockers.some(value => /stale|digest|checksum/i.test(value))) {
    throw new Error(`Planner did not project stale digest/checksum blocker: ${JSON.stringify(plan, null, 2)}`);
  }
  const launched = plan.processLaunched === true || plan.launched === true || plan.sideEffects?.processLaunch === true;
  if (launched) throw new Error(`Planner reports a process launch side effect: ${JSON.stringify(plan, null, 2)}`);
});

When("I build readiness matrices for missing inputs and unverified Windows runtime", function () {
  runContractPython(pyPrelude(this.coreStagingDir) + `
try:
    name, fn = public_callable([
        "build_readiness_matrix",
        "readiness_matrix",
        "compute_readiness_matrix",
        "plan_readiness_matrix",
    ])
    if fn is None:
        emit({
            "ok": False,
            "contract": "pure readiness matrix",
            "message": "No public pure readiness matrix function found.",
            "details": {"expectedAnyOf": ["build_readiness_matrix", "readiness_matrix", "compute_readiness_matrix", "plan_readiness_matrix"]},
        })
    else:
        missing_case = {"install": None, "profile": None, "package": None, "runtime": None, "platform": "linux-x86_64"}
        windows_case = {
            "install": {"status": "found", "platform": "windows-x86_64"},
            "profile": {"status": "selected", "id": "win-profile"},
            "package": {"status": "valid", "id": "jml.runebound-elixirs"},
            "runtime": {"platform": "windows-x86_64", "verificationStatus": "scaffold_buildable_fail_closed_self_test_only_windows_runtime_unverified"},
            "platform": "windows-x86_64",
        }
        _, missing = call_first_success(fn, [
            ("missing-keywords", lambda: fn(**missing_case)),
            ("missing-dto", lambda: fn(missing_case)),
        ])
        _, windows = call_first_success(fn, [
            ("windows-keywords", lambda: fn(**windows_case)),
            ("windows-dto", lambda: fn(windows_case)),
        ])
        emit({"ok": True, "contract": "pure readiness matrix", "function": name, "missing": to_plain(missing), "windows": to_plain(windows)})
except Exception as exc:
    emit({"ok": False, "contract": "pure readiness matrix", "message": str(exc)})
`, this);
});

Then("the readiness matrix exposes rows, blockers, and disabled reasons", function () {
  const result = assertContractOk(this);
  for (const [label, matrix] of [["missing", result.missing], ["windows", result.windows]]) {
    const rows = matrix?.rows || matrix?.checks || matrix?.items;
    if (!Array.isArray(rows) || !rows.length) throw new Error(`${label} readiness matrix has no rows: ${JSON.stringify(matrix, null, 2)}`);
    const disabled = matrix?.disabledReasons || matrix?.disabled_reasons || matrix?.blockers;
    if (!Array.isArray(disabled) || !disabled.length) throw new Error(`${label} readiness matrix has no blockers/disabled reasons: ${JSON.stringify(matrix, null, 2)}`);
  }
});

Then("missing install, profile, package, and Windows fail-closed blockers are explicit", function () {
  const result = assertContractOk(this);
  const missing = flattenValues(result.missing);
  for (const word of ["install", "profile", "package"]) {
    if (!missing.some(value => new RegExp(word, "i").test(value) && /missing|select|required|not found/i.test(value))) {
      throw new Error(`Missing-input matrix lacks explicit ${word} blocker: ${JSON.stringify(result.missing, null, 2)}`);
    }
  }
  const windows = flattenValues(result.windows);
  if (!windows.some(value => /windows/i.test(value) && /fail.?closed|unverified|blocked/i.test(value))) {
    throw new Error(`Windows matrix lacks fail-closed unverified-runtime blocker: ${JSON.stringify(result.windows, null, 2)}`);
  }
});

When("I load diagnostics for missing, malformed, fake, and production evidence reports", function () {
  runContractPython(pyPrelude(this.coreStagingDir) + `
try:
    name, fn = public_callable([
        "load_diagnostics_repository",
        "diagnostics_repository_load",
        "read_diagnostics_repository",
        "build_diagnostics_repository",
    ])
    if fn is None:
        emit({
            "ok": False,
            "contract": "diagnostics repository",
            "message": "No public diagnostics repository function found.",
            "details": {"expectedAnyOf": ["load_diagnostics_repository", "diagnostics_repository_load", "read_diagnostics_repository", "build_diagnostics_repository"]},
        })
    else:
        report_dir = STAGING_DIR / "reports"
        report_dir.mkdir(parents=True, exist_ok=True)
        missing_path = report_dir / "missing-runtime-load-report.json"
        malformed_path = report_dir / "malformed-runtime-load-report.json"
        malformed_path.write_text("{ nope", encoding="utf-8")
        fake_path = report_dir / "fake-runtime-load-report.json"
        fake_payload = json.loads(RUNTIME_REPORT_LOADED.read_text(encoding="utf-8"))
        fake_path.write_text(json.dumps(fake_payload, indent=2), encoding="utf-8")
        production_path = report_dir / "linux-production-runtime-load-report.json"
        production_payload = json.loads(json.dumps(fake_payload))
        production_payload["runtime"]["evidenceKind"] = "steam-linux-production-runtime"
        production_payload["runtime"]["hostOs"] = "linux"
        production_payload["runtime"]["gameExecutableName"] = "barony.x86_64"
        for loaded_mod in production_payload.get("loadedMods", []):
            if loaded_mod.get("id") == "jml.runebound-elixirs":
                loaded_mod.pop("evidenceScope", None)
                loaded_mod["liveHookBehaviorClaimed"] = True
                loaded_mod["productionEvidence"] = {"kind": "steam-linux-live-gameplay", "artifact": "real-runtime-load-report", "hostOs": "linux"}
        production_path.write_text(json.dumps(production_payload, indent=2), encoding="utf-8")
        paths = [missing_path, malformed_path, fake_path, production_path]
        label, value = call_first_success(fn, [
            ("report-dir", lambda: fn(report_dir)),
            ("paths-list", lambda: fn(paths)),
            ("paths-kw", lambda: fn(report_paths=paths)),
            ("dir-kw", lambda: fn(reports_dir=report_dir)),
        ])
        emit({"ok": True, "contract": "diagnostics repository", "function": name, "call": label, "repository": to_plain(value), "items": as_items(value)})
except Exception as exc:
    emit({"ok": False, "contract": "diagnostics repository", "message": str(exc)})
`, this);
});

Then("the diagnostics repository reports missing and malformed runtime reports", function () {
  const result = assertContractOk(this);
  const text = JSON.stringify(result.repository || result.items || []);
  if (!/missing/i.test(text) || !/malformed|parse|invalid json|json/i.test(text)) {
    throw new Error(`Diagnostics repository lacks missing/malformed report diagnostics: ${JSON.stringify(result.repository, null, 2)}`);
  }
});

Then("fake-provider evidence is classified separately from real Linux production evidence", function () {
  const result = assertContractOk(this);
  const text = JSON.stringify(result.repository || result.items || []);
  if (!/fake/i.test(text)) throw new Error(`Diagnostics repository did not classify fake-provider evidence: ${JSON.stringify(result.repository, null, 2)}`);
  if (!/production|steam-linux-live-gameplay|real-runtime-load-report/i.test(text)) {
    throw new Error(`Diagnostics repository did not classify real Linux production evidence: ${JSON.stringify(result.repository, null, 2)}`);
  }
});

When("I request the core facade dashboard for a dry-run app journey", function () {
  runContractPython(pyPrelude(this.coreStagingDir) + `
try:
    name, fn = public_callable([
        "build_core_dashboard_state",
        "core_facade_dashboard",
        "build_semantic_dashboard",
        "core_dashboard",
        "build_dashboard_state",
    ])
    if fn is None:
        emit({
            "ok": False,
            "contract": "core facade dashboard journey",
            "message": "No public core facade dashboard function found.",
            "details": {"expectedAnyOf": ["build_core_dashboard_state", "core_facade_dashboard", "build_semantic_dashboard", "core_dashboard", "build_dashboard_state"]},
        })
    else:
        profile_dir = STAGING_DIR / "facade-profile"
        profile = {"profile": {"id": "facade-profile"}, "activeMods": [], "paths": {"profileRoot": str(profile_dir)}}
        context = {
            "install": {"status": "missing"},
            "profile": profile,
            "packageRoot": str(ELIXIRS_PKG),
            "runtimeReport": str(RUNTIME_REPORT_LOADED),
            "workshop": {"mode": "dry-run", "publishEnabled": False},
            "platform": "linux-x86_64",
        }
        label, value = call_first_success(fn, [
            ("context-dict", lambda: fn(context)),
            ("keywords", lambda: fn(install=context["install"], profile=profile, package_root=ELIXIRS_PKG, runtime_report=RUNTIME_REPORT_LOADED, workshop=context["workshop"])),
            ("no-args", lambda: fn()),
        ])
        emit({"ok": True, "contract": "core facade dashboard journey", "function": name, "call": label, "dashboard": to_plain(value)})
except Exception as exc:
    emit({"ok": False, "contract": "core facade dashboard journey", "message": str(exc)})
`, this);
});

Then("the facade returns one semantic dashboard object with all core sections", function () {
  const result = assertContractOk(this);
  const dashboard = result.dashboard;
  if (!dashboard || typeof dashboard !== "object" || Array.isArray(dashboard)) {
    throw new Error(`Facade did not return a semantic dashboard object: ${JSON.stringify(dashboard, null, 2)}`);
  }
  for (const section of ["install", "profile", "package", "readiness", "diagnostics", "workshop"]) {
    if (!dashboard[section] || typeof dashboard[section] !== "object" || Array.isArray(dashboard[section])) {
      throw new Error(`Dashboard missing semantic section '${section}': ${JSON.stringify(dashboard, null, 2)}`);
    }
  }
});

Then("the facade dashboard preserves workshop stub state and disabled reasons without raw stdout", function () {
  const result = assertContractOk(this);
  const dashboard = result.dashboard;
  const disabled = dashboard.disabledReasons || dashboard.disabled_reasons || dashboard.readiness?.disabledReasons || dashboard.readiness?.disabled_reasons;
  if (!Array.isArray(disabled)) throw new Error(`Dashboard lacks disabled reasons list: ${JSON.stringify(dashboard, null, 2)}`);
  const workshopText = JSON.stringify(dashboard.workshop || {});
  if (!/dry|stub|disabled/i.test(workshopText)) {
    throw new Error(`Workshop section does not expose dry-run/stub/disabled state: ${workshopText}`);
  }
  const allText = JSON.stringify(dashboard);
  if (/^Package validation:/m.test(allText) || /Runtime load report:/m.test(allText) || /\bFAILED:\s/m.test(allText)) {
    throw new Error(`Dashboard embeds raw command stdout instead of semantic state: ${allText}`);
  }
});
