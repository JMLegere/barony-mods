"use strict";

const { After, Given, When, Then } = require("@cucumber/cucumber");
const { execFileSync } = require("child_process");
const fs = require("fs");
const os = require("os");
const path = require("path");

// paths: <repo>/framework/BaronyModLoader/features/steps/launch_readiness_steps.js
//          ../../../../ from here = <repo>
const REPO_ROOT = path.resolve(__dirname, "../../../../");
const BML_APP = path.join(REPO_ROOT, "framework/BaronyModLoader/app/barony_mod_loader.py");
const ELIXIRS_PKG = path.join(REPO_ROOT, "mods/runebound-elixirs");
const RUNTIME_INFO = path.join(REPO_ROOT, "framework/BaronyModLoader/fixtures/runtime-info.installed-hook.stash.json");

function runLaunchReadinessPython(script, world) {
  const tmp = path.join(os.tmpdir(), `bml-launch-readiness-${Date.now()}-${Math.random().toString(36).slice(2)}.py`);
  fs.writeFileSync(tmp, script, "utf-8");
  try {
    const stdout = execFileSync("/usr/bin/python3", [tmp], {
      cwd: REPO_ROOT,
      encoding: "utf-8",
      timeout: 30000,
      stdio: ["ignore", "pipe", "pipe"],
    });
    const marker = "__BML_LAUNCH_READINESS_JSON__";
    const line = stdout.split(/\r?\n/).find(entry => entry.startsWith(marker));
    if (!line) {
      throw new Error(`Launch readiness script did not emit JSON marker. Output:\n${stdout}`);
    }
    world.launchReadinessContract = JSON.parse(line.slice(marker.length));
  } catch (err) {
    const stdout = err.stdout ? String(err.stdout) : "";
    const stderr = err.stderr ? String(err.stderr) : "";
    throw new Error(`Launch readiness contract script failed (exit ${err.status ?? 1}):\nSTDOUT:\n${stdout}\nSTDERR:\n${stderr}`);
  } finally {
    try { fs.unlinkSync(tmp); } catch (_) {}
  }
}

function pyPrelude(stagingDir) {
  return `
import argparse
import contextlib
import dataclasses
import hashlib
import importlib.util
import io
import json
import os
from pathlib import Path
import shutil
import stat
import subprocess
import sys

REPO_ROOT = Path(${JSON.stringify(REPO_ROOT)})
BML_APP = Path(${JSON.stringify(BML_APP)})
SOURCE_PACKAGE = Path(${JSON.stringify(ELIXIRS_PKG)})
SOURCE_RUNTIME_INFO = Path(${JSON.stringify(RUNTIME_INFO)})
STAGING_DIR = Path(${JSON.stringify(stagingDir)})

spec = importlib.util.spec_from_file_location("bml_app", BML_APP)
mod = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = mod
spec.loader.exec_module(mod)

CANDIDATES = {
    "readiness": ["build_readiness_matrix", "readiness_matrix", "compute_readiness_matrix", "plan_readiness_matrix"],
    "launch_plan": ["command_launch_plan", "create_launch_plan_manifest", "write_launch_plan_manifest", "create_runtime_manifest_artifacts"],
    "stale_plan": ["plan_runtime_manifest", "build_runtime_manifest_plan", "runtime_manifest_plan", "plan_launch_manifest"],
    "dry_run": ["command_launch", "plan_dry_run_launch", "dry_run_launch", "build_launch_dry_run"],
    "playable": ["build_launch_playability", "evaluate_playable_claim", "assess_playable_claim_boundary", "build_readiness_matrix"],
}

def emit(payload):
    print("__BML_LAUNCH_READINESS_JSON__" + json.dumps(payload, sort_keys=True))

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

def sha256(path):
    digest = hashlib.sha256()
    with Path(path).open("rb") as handle:
        for chunk in iter(lambda: handle.read(65536), b""):
            digest.update(chunk)
    return digest.hexdigest()

def write_file(path, content, executable=False):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content, encoding="utf-8")
    if executable:
        path.chmod(path.stat().st_mode | stat.S_IXUSR)
    return path

def copy_package_fixture():
    package_root = STAGING_DIR / "packages" / "runebound-elixirs"
    if package_root.exists():
        shutil.rmtree(package_root)
    shutil.copytree(SOURCE_PACKAGE, package_root)
    return package_root

def copy_runtime_info_fixture():
    runtime_path = STAGING_DIR / "runtime" / "runtime-info.json"
    runtime_path.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(SOURCE_RUNTIME_INFO, runtime_path)
    return runtime_path

def setup_profile_package_runtime():
    package_root = copy_package_fixture()
    runtime_path = copy_runtime_info_fixture()
    profile_dir = STAGING_DIR / "profiles" / "launch-profile"
    fake_game = STAGING_DIR / "game" / "barony.x86_64"
    write_file(fake_game, "#!/bin/sh\\necho should-not-run\\n", executable=True)

    create_stdout = io.StringIO()
    with contextlib.redirect_stdout(create_stdout):
        create_code = int(mod.command_profile_create(argparse.Namespace(
            profile_dir=str(profile_dir),
            profile_id="launch-readiness-profile",
            barony_executable=str(fake_game),
            steam=False,
            steam_install=None,
            steam_manifest=None,
            runtime_info=None,
        )))
    enable_stdout = io.StringIO()
    with contextlib.redirect_stdout(enable_stdout):
        enable_code = int(mod.command_profile_enable(argparse.Namespace(
            profile_dir=str(profile_dir),
            package=str(package_root),
        )))
    profile, loaded_profile_dir, profile_result = mod.load_profile(str(profile_dir))
    package, package_result = mod.load_package(str(package_root))
    runtime_info, loaded_runtime_path, runtime_result = mod.load_runtime_info(str(runtime_path))
    if create_code != 0 or enable_code != 0 or profile is None or package is None or runtime_info is None:
        raise AssertionError(json.dumps({
            "message": "Hermetic launch fixture setup failed.",
            "createCode": create_code,
            "createStdout": create_stdout.getvalue(),
            "enableCode": enable_code,
            "enableStdout": enable_stdout.getvalue(),
            "profileProblems": [to_plain(problem) for problem in profile_result.problems],
            "packageProblems": [to_plain(problem) for problem in package_result.problems],
            "runtimeProblems": [to_plain(problem) for problem in runtime_result.problems],
        }, indent=2, sort_keys=True))
    return {
        "profile_dir": loaded_profile_dir,
        "profile": profile,
        "package_root": package_root,
        "package": package,
        "runtime_info_path": loaded_runtime_path,
        "runtime_info": runtime_info,
        "fake_game": fake_game,
    }

def write_runtime_registry(fixtures):
    target = mod.current_platform_target()
    platform_id = mod.current_platform_id()
    runtime_dir = STAGING_DIR / "registered-runtime"
    steam_executable = write_file(runtime_dir / target.executable_name, "#!/bin/sh\\necho dry-run-only\\n", executable=True)
    hook_library_name = "libbarony_bml.so" if target.hook_artifact_extension == ".so" else ("barony_bml.dll" if target.hook_artifact_extension == ".dll" else "libbarony_bml.dylib")
    hook_library = write_file(runtime_dir / hook_library_name, "fake hook library for dry-run contract\\n")
    hook_manifest = write_file(runtime_dir / "hook-manifest.json", json.dumps({"contract": "dry-run-only", "platform": platform_id}, indent=2))
    registry_path = STAGING_DIR / "runtime-registry.json"
    runtime = {
        "id": "hermetic-dry-run-runtime",
        "platform": platform_id,
        "platformTarget": platform_id,
        "runtimeStrategy": mod.RUNTIME_STRATEGY_INSTALLED_HOOK,
        "launchAdapter": target.launch_adapter,
        "hookArtifactExtension": target.hook_artifact_extension,
        "steamExecutable": str(steam_executable),
        "steamExecutableSha256": sha256(steam_executable),
        "hookLibrary": str(hook_library),
        "hookLibrarySha256": sha256(hook_library),
        "hookManifest": str(hook_manifest),
        "hookManifestSha256": sha256(hook_manifest),
        "runtimeInfo": str(fixtures["runtime_info_path"]),
        "storefront": "manual",
    }
    registry_path.write_text(json.dumps({
        "schemaVersion": mod.SCHEMA_VERSION,
        "app": {"id": mod.APP_ID, "version": mod.APP_VERSION},
        "runtimes": [runtime],
    }, indent=2), encoding="utf-8")
    return registry_path, runtime
`;
}

function assertContractOk(world) {
  const result = world.launchReadinessContract;
  if (!result) throw new Error("No launch readiness contract result was recorded.");
  if (!result.ok) {
    const details = result.details ? `\n${JSON.stringify(result.details, null, 2)}` : "";
    throw new Error(`${result.contract || "Launch readiness contract"} failed: ${result.message || "unsatisfied"}${details}`);
  }
  return result;
}

function flattenValues(value, out = []) {
  if (value == null) return out;
  if (["string", "number", "boolean"].includes(typeof value)) out.push(String(value));
  else if (Array.isArray(value)) value.forEach(item => flattenValues(item, out));
  else if (typeof value === "object") Object.values(value).forEach(item => flattenValues(item, out));
  return out;
}

Given("a clean Launch Readiness staging directory", function () {
  this.launchStagingDir = fs.mkdtempSync(path.join(os.tmpdir(), "bml-launch-readiness-"));
});

After(function () {
  if (this.launchStagingDir) {
    fs.rmSync(this.launchStagingDir, { recursive: true, force: true });
  }
});

When("I build the launch readiness blocked matrix", function () {
  runLaunchReadinessPython(pyPrelude(this.launchStagingDir) + `
try:
    name, fn = public_callable(CANDIDATES["readiness"])
    if fn is None:
        emit({
            "ok": False,
            "contract": "launch readiness blocked matrix",
            "message": "Missing semantic readiness matrix API. Expected one of: " + ", ".join(CANDIDATES["readiness"]),
            "details": {"candidateNames": CANDIDATES["readiness"]},
        })
    else:
        label, matrix = call_first_success(fn, [
            ("keywords", lambda: fn(install=None, profile=None, package=None, runtime=None, platform="linux-x86_64")),
            ("dto", lambda: fn({"install": None, "profile": None, "package": None, "runtime": None, "platform": "linux-x86_64"})),
        ])
        emit({"ok": True, "contract": "launch readiness blocked matrix", "function": name, "call": label, "matrix": to_plain(matrix), "candidateNames": CANDIDATES["readiness"]})
except Exception as exc:
    emit({"ok": False, "contract": "launch readiness blocked matrix", "message": str(exc), "details": {"candidateNames": CANDIDATES["readiness"]}})
`, this);
});

Then("launch readiness reports install, profile, package, and runtime blockers", function () {
  const result = assertContractOk(this);
  const matrix = result.matrix || {};
  const rows = matrix.rows || matrix.checks || matrix.items;
  if (!Array.isArray(rows) || rows.length < 4) {
    throw new Error(`Readiness matrix must expose one row per launch prerequisite: ${JSON.stringify(matrix, null, 2)}`);
  }
  const disabled = matrix.disabledReasons || matrix.disabled_reasons || matrix.blockers;
  if (!Array.isArray(disabled) || disabled.length < 4) {
    throw new Error(`Readiness matrix must expose disabled reasons for blocked launch actions: ${JSON.stringify(matrix, null, 2)}`);
  }
  const text = flattenValues(matrix);
  for (const word of ["install", "profile", "package", "runtime"]) {
    if (!text.some(value => new RegExp(word, "i").test(value) && /missing|select|required|not registered|blocked/i.test(value))) {
      throw new Error(`Readiness matrix lacks explicit ${word} launch blocker. Function candidates: ${result.candidateNames.join(", ")}\n${JSON.stringify(matrix, null, 2)}`);
    }
  }
  if (!/blocked/i.test(String(matrix.status || ""))) {
    throw new Error(`Readiness matrix status must fail closed as blocked: ${JSON.stringify(matrix, null, 2)}`);
  }
});

When("I create a launch plan manifest from hermetic fixtures", function () {
  runLaunchReadinessPython(pyPrelude(this.launchStagingDir) + `
try:
    name, fn = public_callable(CANDIDATES["launch_plan"])
    if fn is None:
        emit({
            "ok": False,
            "contract": "launch plan manifest creation",
            "message": "Missing semantic launch plan creation API. Expected one of: " + ", ".join(CANDIDATES["launch_plan"]),
            "details": {"candidateNames": CANDIDATES["launch_plan"]},
        })
    else:
        fixtures = setup_profile_package_runtime()
        out_path = fixtures["profile_dir"] / "BaronyModLoader" / "manifests" / "runtime-manifest.json"
        run_count = {"value": 0}
        original_run = subprocess.run
        def forbidden_run(*args, **kwargs):
            run_count["value"] += 1
            raise AssertionError("Launch plan creation attempted to start a process")
        subprocess.run = forbidden_run
        stdout = io.StringIO()
        try:
            if name == "command_launch_plan":
                with contextlib.redirect_stdout(stdout):
                    code = int(fn(argparse.Namespace(
                        profile_dir=str(fixtures["profile_dir"]),
                        package=str(fixtures["package_root"]),
                        runtime_info=str(fixtures["runtime_info_path"]),
                        out=str(out_path),
                    )))
            else:
                label, value = call_first_success(fn, [
                    ("keywords", lambda: fn(profile=fixtures["profile"], profile_dir=fixtures["profile_dir"], package=fixtures["package"], runtime_info=fixtures["runtime_info"], out_path=out_path)),
                    ("positional", lambda: fn(fixtures["profile"], fixtures["profile_dir"], fixtures["package"], fixtures["runtime_info"], out_path)),
                ])
                code = 0
        finally:
            subprocess.run = original_run
        active_mods_path = mod.active_mods_json_path(fixtures["profile_dir"])
        manifest = json.loads(out_path.read_text(encoding="utf-8")) if out_path.exists() else None
        active_mods = json.loads(active_mods_path.read_text(encoding="utf-8")) if active_mods_path.exists() else None
        emit({
            "ok": code == 0,
            "contract": "launch plan manifest creation",
            "function": name,
            "exitCode": code,
            "stdout": stdout.getvalue(),
            "manifestPath": str(out_path),
            "manifestExists": out_path.exists(),
            "manifest": to_plain(manifest),
            "activeModsPath": str(active_mods_path),
            "activeModsExists": active_mods_path.exists(),
            "activeMods": to_plain(active_mods),
            "processStartCount": run_count["value"],
            "candidateNames": CANDIDATES["launch_plan"],
        })
except Exception as exc:
    emit({"ok": False, "contract": "launch plan manifest creation", "message": str(exc), "details": {"candidateNames": CANDIDATES["launch_plan"]}})
`, this);
});

Then("the launch plan manifest and active mods artifacts are created", function () {
  const result = assertContractOk(this);
  if (!result.manifestExists) throw new Error(`Launch plan did not create runtime manifest at ${result.manifestPath}`);
  if (!result.activeModsExists) throw new Error(`Launch plan did not create active mods at ${result.activeModsPath}`);
  const manifest = result.manifest || {};
  if (manifest.contract?.id !== "bml-runtime-contract") {
    throw new Error(`Runtime manifest contract id is not semantic BML runtime contract: ${JSON.stringify(manifest, null, 2)}`);
  }
  if (!manifest.launch || manifest.launch.profileId !== "launch-readiness-profile") {
    throw new Error(`Runtime manifest lacks launch profile payload: ${JSON.stringify(manifest, null, 2)}`);
  }
  const modEntry = (manifest.mods || []).find(entry => entry.id === "jml.runebound-elixirs");
  if (!modEntry || typeof modEntry.checksumSet !== "string" || !modEntry.checksumSet.startsWith("sha256:")) {
    throw new Error(`Runtime manifest lacks Runebound mod entry with checksum: ${JSON.stringify(manifest.mods, null, 2)}`);
  }
  const activeMods = result.activeMods || {};
  if (!Array.isArray(activeMods.mods) || !activeMods.mods.some(entry => entry.id === "jml.runebound-elixirs")) {
    throw new Error(`Active mods artifact lacks Runebound enabled mod: ${JSON.stringify(activeMods, null, 2)}`);
  }
});

Then("launch plan creation does not start a process", function () {
  const result = assertContractOk(this);
  if (result.processStartCount !== 0) {
    throw new Error(`Launch plan creation started a process ${result.processStartCount} time(s); expected pure manifest creation.`);
  }
});

When("I plan launch readiness with stale manifest package digests", function () {
  runLaunchReadinessPython(pyPrelude(this.launchStagingDir) + `
try:
    name, fn = public_callable(CANDIDATES["stale_plan"])
    if fn is None:
        emit({
            "ok": False,
            "contract": "stale manifest blocking",
            "message": "Missing semantic stale-manifest planning API. Expected one of: " + ", ".join(CANDIDATES["stale_plan"]),
            "details": {"candidateNames": CANDIDATES["stale_plan"]},
        })
    else:
        fixtures = setup_profile_package_runtime()
        profile = fixtures["profile"]
        profile["activeMods"] = [{"id": "jml.runebound-elixirs", "version": "0.0.0", "checksumSet": "sha256:stale-active-mod"}]
        out_path = fixtures["profile_dir"] / "BaronyModLoader" / "manifests" / "runtime-manifest.json"
        run_count = {"value": 0}
        original_run = subprocess.run
        def forbidden_run(*args, **kwargs):
            run_count["value"] += 1
            raise AssertionError("Stale manifest planning attempted to start a process")
        subprocess.run = forbidden_run
        try:
            label, plan = call_first_success(fn, [
                ("keywords", lambda: fn(profile=profile, profile_dir=fixtures["profile_dir"], package=fixtures["package"], runtime_info=fixtures["runtime_info"], out_path=out_path, previous_digest="sha256:stale-runtime-manifest")),
                ("output-path", lambda: fn(profile=profile, profile_dir=fixtures["profile_dir"], package=fixtures["package"], runtime_info=fixtures["runtime_info"], output_path=out_path, previous_digest="sha256:stale-runtime-manifest")),
                ("positional", lambda: fn(profile, fixtures["profile_dir"], fixtures["package"], fixtures["runtime_info"], out_path)),
            ])
        finally:
            subprocess.run = original_run
        emit({
            "ok": True,
            "contract": "stale manifest blocking",
            "function": name,
            "call": label,
            "plan": to_plain(plan),
            "manifestExists": out_path.exists(),
            "processStartCount": run_count["value"],
            "candidateNames": CANDIDATES["stale_plan"],
        })
except Exception as exc:
    emit({"ok": False, "contract": "stale manifest blocking", "message": str(exc), "details": {"candidateNames": CANDIDATES["stale_plan"]}})
`, this);
});

Then("stale manifest digest blockers are explicit", function () {
  const result = assertContractOk(this);
  const plan = result.plan || {};
  const blockers = flattenValues(plan.blockers || plan.disabledReasons || plan.disabled_reasons || plan.problems || []);
  const staleMentions = blockers.filter(value => /stale|digest|checksum/i.test(value));
  if (staleMentions.length < 1) {
    throw new Error(`Stale manifest planning did not expose stale digest/checksum blockers. Function candidates: ${result.candidateNames.join(", ")}\n${JSON.stringify(plan, null, 2)}`);
  }
});

Then("launch planning remains pure", function () {
  const result = assertContractOk(this);
  if (result.processStartCount !== 0) throw new Error(`Pure launch planning started a process ${result.processStartCount} time(s).`);
  if (result.manifestExists) throw new Error("Pure launch planning wrote a runtime manifest; stale planning must report blockers without filesystem side effects.");
  const plan = result.plan || {};
  if (plan.sideEffects?.processLaunch === true || plan.processLaunched === true || plan.launched === true) {
    throw new Error(`Pure launch planning reports process-launch side effects: ${JSON.stringify(plan, null, 2)}`);
  }
});

When("I dry-run launch from hermetic fixtures", function () {
  runLaunchReadinessPython(pyPrelude(this.launchStagingDir) + `
try:
    name, fn = public_callable(CANDIDATES["dry_run"])
    if fn is None:
        emit({
            "ok": False,
            "contract": "dry-run launch planning",
            "message": "Missing semantic dry-run launch API. Expected one of: " + ", ".join(CANDIDATES["dry_run"]),
            "details": {"candidateNames": CANDIDATES["dry_run"]},
        })
    else:
        fixtures = setup_profile_package_runtime()
        registry_path, runtime = write_runtime_registry(fixtures)
        out_path = fixtures["profile_dir"] / "BaronyModLoader" / "manifests" / "dry-run-runtime-manifest.json"
        run_count = {"value": 0}
        original_run = subprocess.run
        def forbidden_run(*args, **kwargs):
            run_count["value"] += 1
            raise AssertionError("Dry-run launch attempted to start Barony")
        subprocess.run = forbidden_run
        stdout = io.StringIO()
        try:
            if name == "command_launch":
                with contextlib.redirect_stdout(stdout):
                    code = int(fn(argparse.Namespace(
                        profile_dir=str(fixtures["profile_dir"]),
                        package=str(fixtures["package_root"]),
                        registry=str(registry_path),
                        runtime=None,
                        out=str(out_path),
                        dry_run=True,
                        barony_args=[],
                    )))
                payload = json.loads(stdout.getvalue()) if stdout.getvalue().strip().startswith("{") else None
            else:
                label, payload = call_first_success(fn, [
                    ("keywords", lambda: fn(profile=fixtures["profile"], profile_dir=fixtures["profile_dir"], package=fixtures["package"], registry_path=registry_path, dry_run=True, out_path=out_path)),
                    ("positional", lambda: fn(fixtures["profile"], fixtures["profile_dir"], fixtures["package"], registry_path, True)),
                ])
                code = 0
        finally:
            subprocess.run = original_run
        emit({
            "ok": code == 0,
            "contract": "dry-run launch planning",
            "function": name,
            "exitCode": code,
            "stdout": stdout.getvalue(),
            "payload": to_plain(payload),
            "manifestPath": str(out_path),
            "manifestExists": out_path.exists(),
            "processStartCount": run_count["value"],
            "runtime": to_plain(runtime),
            "candidateNames": CANDIDATES["dry_run"],
        })
except Exception as exc:
    emit({"ok": False, "contract": "dry-run launch planning", "message": str(exc), "details": {"candidateNames": CANDIDATES["dry_run"]}})
`, this);
});

Then("the dry-run reports a launch command and manifest path", function () {
  const result = assertContractOk(this);
  const payload = result.payload || {};
  if (payload.status !== "dry-run") {
    throw new Error(`Dry-run payload must report status=dry-run: ${JSON.stringify(payload, null, 2)}`);
  }
  if (!Array.isArray(payload.command) || payload.command.length < 1 || !/barony/i.test(payload.command[0])) {
    throw new Error(`Dry-run payload lacks launch command metadata: ${JSON.stringify(payload, null, 2)}`);
  }
  if (typeof payload.runtimeManifest !== "string" || !payload.runtimeManifest.endsWith("dry-run-runtime-manifest.json")) {
    throw new Error(`Dry-run payload lacks runtime manifest path: ${JSON.stringify(payload, null, 2)}`);
  }
  if (!result.manifestExists) {
    throw new Error(`Dry-run did not create the planned runtime manifest at ${result.manifestPath}`);
  }
  if (!payload.environment || payload.environment.BML_RUNTIME_MANIFEST !== payload.runtimeManifest) {
    throw new Error(`Dry-run payload lacks semantic BML launch environment: ${JSON.stringify(payload, null, 2)}`);
  }
});

Then("dry-run launch does not start a process", function () {
  const result = assertContractOk(this);
  if (result.processStartCount !== 0) {
    throw new Error(`Dry-run launch invoked subprocess.run ${result.processStartCount} time(s); it must never start Barony.`);
  }
  const payload = result.payload || {};
  if (/launcher-runtime\.log$/.test(String(payload.launchLog || ""))) {
    const logPath = path.resolve(payload.launchLog);
    if (fs.existsSync(logPath)) throw new Error(`Dry-run created a runtime launch log as if Barony started: ${logPath}`);
  }
});

When("I evaluate Windows launch readiness without live runtime evidence", function () {
  runLaunchReadinessPython(pyPrelude(this.launchStagingDir) + `
try:
    name, fn = public_callable(CANDIDATES["readiness"])
    if fn is None:
        emit({
            "ok": False,
            "contract": "Windows launch disabled readiness",
            "message": "Missing semantic readiness matrix API. Expected one of: " + ", ".join(CANDIDATES["readiness"]),
            "details": {"candidateNames": CANDIDATES["readiness"]},
        })
    else:
        runtime_info = json.loads(SOURCE_RUNTIME_INFO.read_text(encoding="utf-8"))
        windows_runtime = None
        for entry in runtime_info.get("platforms", []):
            if isinstance(entry, dict) and entry.get("platform") == "windows-x86_64":
                windows_runtime = dict(entry)
                break
        if windows_runtime is None:
            raise AssertionError("Hermetic runtime-info fixture lacks windows-x86_64 platform entry")
        label, matrix = call_first_success(fn, [
            ("keywords", lambda: fn(
                install={"status": "found", "platform": "windows-x86_64"},
                profile={"status": "selected", "id": "windows-profile"},
                package={"status": "valid", "id": "jml.runebound-elixirs"},
                runtime=windows_runtime,
                platform="windows-x86_64",
            )),
            ("dto", lambda: fn({
                "install": {"status": "found", "platform": "windows-x86_64"},
                "profile": {"status": "selected", "id": "windows-profile"},
                "package": {"status": "valid", "id": "jml.runebound-elixirs"},
                "runtime": windows_runtime,
                "platform": "windows-x86_64",
            })),
        ])
        emit({"ok": True, "contract": "Windows launch disabled readiness", "function": name, "call": label, "matrix": to_plain(matrix), "runtime": to_plain(windows_runtime), "candidateNames": CANDIDATES["readiness"]})
except Exception as exc:
    emit({"ok": False, "contract": "Windows launch disabled readiness", "message": str(exc), "details": {"candidateNames": CANDIDATES["readiness"]}})
`, this);
});

Then("Windows launch readiness is fail-closed and disabled", function () {
  const result = assertContractOk(this);
  const matrix = result.matrix || {};
  const text = flattenValues(matrix);
  if (!/blocked/i.test(String(matrix.status || ""))) {
    throw new Error(`Windows readiness status must be blocked without live Windows evidence: ${JSON.stringify(matrix, null, 2)}`);
  }
  if (!text.some(value => /windows/i.test(value) && /fail.?closed|unverified|disabled|blocked|live/i.test(value))) {
    throw new Error(`Windows readiness lacks explicit fail-closed live-evidence blocker: ${JSON.stringify(matrix, null, 2)}`);
  }
});

When("I evaluate playable launch claims from non-production evidence", function () {
  runLaunchReadinessPython(pyPrelude(this.launchStagingDir) + `
try:
    name, fn = public_callable(CANDIDATES["playable"])
    if fn is None:
        emit({
            "ok": False,
            "contract": "playable claim boundary",
            "message": "Missing semantic playable-claim boundary API. Expected one of: " + ", ".join(CANDIDATES["playable"]),
            "details": {"candidateNames": CANDIDATES["playable"]},
        })
    else:
        fake_case = {
            "install": {"status": "found", "platform": "linux-x86_64"},
            "profile": {"status": "selected", "id": "fake-evidence-profile"},
            "package": {"status": "valid", "id": "jml.runebound-elixirs"},
            "runtime": {
                "status": "ready",
                "platform": "linux-x86_64",
                "verificationStatus": "fake_provider_live_hook_evidence_only",
                "evidenceKind": "fake-symbol-provider",
                "evidenceScope": "fake-provider",
                "playableClaim": True,
            },
            "platform": "linux-x86_64",
        }
        scaffold_case = {
            "install": {"status": "found", "platform": "windows-x86_64"},
            "profile": {"status": "selected", "id": "scaffold-evidence-profile"},
            "package": {"status": "valid", "id": "jml.runebound-elixirs"},
            "runtime": {
                "status": "ready",
                "platform": "windows-x86_64",
                "verificationStatus": "scaffold_buildable_fail_closed_self_test_only_windows_runtime_unverified",
                "evidenceKind": "scaffold-build-self-test",
                "evidenceScope": "self-test",
                "playableClaim": True,
            },
            "platform": "windows-x86_64",
        }
        if name == "build_readiness_matrix":
            _, fake = call_first_success(fn, [("fake-keywords", lambda: fn(**fake_case)), ("fake-dto", lambda: fn(fake_case))])
            _, scaffold = call_first_success(fn, [("scaffold-keywords", lambda: fn(**scaffold_case)), ("scaffold-dto", lambda: fn(scaffold_case))])
        else:
            _, fake = call_first_success(fn, [("fake-dto", lambda: fn(fake_case)), ("fake-keywords", lambda: fn(**fake_case))])
            _, scaffold = call_first_success(fn, [("scaffold-dto", lambda: fn(scaffold_case)), ("scaffold-keywords", lambda: fn(**scaffold_case))])
        emit({
            "ok": True,
            "contract": "playable claim boundary",
            "function": name,
            "fake": to_plain(fake),
            "scaffold": to_plain(scaffold),
            "candidateNames": CANDIDATES["playable"],
        })
except Exception as exc:
    emit({"ok": False, "contract": "playable claim boundary", "message": str(exc), "details": {"candidateNames": CANDIDATES["playable"]}})
`, this);
});

Then("fake or scaffold-only evidence cannot be reported as playable", function () {
  const result = assertContractOk(this);
  for (const [label, response] of [["fake provider", result.fake], ["scaffold-only", result.scaffold]]) {
    const text = flattenValues(response);
    const status = String(response?.status || response?.readinessStatus || response?.launchStatus || "").toLowerCase();
    const playableValue = response?.playable ?? response?.isPlayable ?? response?.playableClaimAllowed ?? response?.launchPlayable;
    const blockedText = text.some(value => /fake|scaffold|self-test|not production|non.?production|not playable|cannot claim|fail.?closed|blocked|unverified/i.test(value));
    const explicitPlayableFalse = playableValue === false || /not playable|cannot claim playable|playable.*false/i.test(text.join("\n"));
    const statusBlocked = /blocked|disabled|not.?playable|fail.?closed/.test(status);
    if (!(blockedText && (explicitPlayableFalse || statusBlocked))) {
      throw new Error(`${label} evidence must be blocked from playable claims. This is a product contract gap if only generic readiness exists; expected one of ${result.candidateNames.join(", ")} to expose playable=false or a fail-closed blocker.\n${JSON.stringify(response, null, 2)}`);
    }
  }
});
