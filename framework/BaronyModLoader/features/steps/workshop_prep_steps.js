"use strict";

const { After, Given, When, Then } = require("@cucumber/cucumber");
const { execFileSync } = require("child_process");
const fs = require("fs");
const os = require("os");
const path = require("path");

// paths: <repo>/framework/BaronyModLoader/features/steps/workshop_prep_steps.js
//          ../../../../ from here = <repo>
const REPO_ROOT = path.resolve(__dirname, "../../../../");
const BML_APP = path.join(REPO_ROOT, "framework/BaronyModLoader/app/barony_mod_loader.py");
const ELIXIRS_PKG = path.join(REPO_ROOT, "mods/runebound-elixirs");

const WORKSHOP_PREP_API_CANDIDATES = [
  "build_workshop_prep_state",
  "plan_workshop_prep",
  "prepare_workshop_dry_run",
  "build_workshop_publish_plan",
  "build_workshop_dry_run_report",
  "workshop_prep_state",
  "workshop_prepare",
  "prepare_workshop_package",
  "stage_workshop_package",
  "validate_workshop_prep",
  "build_steam_workshop_vdf_report",
  "plan_steam_workshop_publish",
];

function runWorkshopPrepPython(script, world) {
  const tmp = path.join(os.tmpdir(), `bml-workshop-prep-${Date.now()}-${Math.random().toString(36).slice(2)}.py`);
  fs.writeFileSync(tmp, script, "utf-8");
  try {
    const stdout = execFileSync("/usr/bin/python3", [tmp], {
      cwd: REPO_ROOT,
      encoding: "utf-8",
      timeout: 30000,
      stdio: ["ignore", "pipe", "pipe"],
    });
    const marker = "__BML_WORKSHOP_PREP_JSON__";
    const line = stdout.split(/\r?\n/).find(entry => entry.startsWith(marker));
    if (!line) {
      throw new Error(`Workshop Prep script did not emit JSON marker. Output:\n${stdout}`);
    }
    world.workshopPrepContract = JSON.parse(line.slice(marker.length));
  } catch (err) {
    const stdout = err.stdout ? String(err.stdout) : "";
    const stderr = err.stderr ? String(err.stderr) : "";
    throw new Error(`Workshop Prep contract script failed (exit ${err.status ?? 1}):\nSTDOUT:\n${stdout}\nSTDERR:\n${stderr}`);
  } finally {
    try { fs.unlinkSync(tmp); } catch (_) {}
  }
}

function pyPrelude(scenario, stagingDir) {
  return `
import dataclasses
import importlib.util
import json
import os
from pathlib import Path
import re
import shutil
import socket
import subprocess
import sys
import tempfile
import urllib.request

REPO_ROOT = Path(${JSON.stringify(REPO_ROOT)})
BML_APP = Path(${JSON.stringify(BML_APP)})
REAL_ELIXIRS_PKG = Path(${JSON.stringify(ELIXIRS_PKG)})
SCENARIO = ${JSON.stringify(scenario)}
STAGING_DIR = Path(${JSON.stringify(stagingDir)})
API_CANDIDATES = ${JSON.stringify(WORKSHOP_PREP_API_CANDIDATES)}

spec = importlib.util.spec_from_file_location("bml_app", BML_APP)
mod = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = mod
spec.loader.exec_module(mod)

def emit(payload):
    print("__BML_WORKSHOP_PREP_JSON__" + json.dumps(payload, sort_keys=True))

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

def write_workshop_toml(path, metadata):
    path.write_text(
        'title = ' + json.dumps(metadata["title"]) + '\\n'
        'description = ' + json.dumps(metadata["description"]) + '\\n'
        'preview = ' + json.dumps(metadata["preview"]) + '\\n'
        'visibility = ' + str(metadata["visibility"]) + '\\n'
        'publishedfileid = ' + json.dumps(metadata["publishedfileid"]) + '\\n'
        'changenote = ' + json.dumps(metadata["changenote"]) + '\\n\\n'
        '[content]\\n'
        'folder = "content"\\n',
        encoding="utf-8",
    )

def make_fixture():
    STAGING_DIR.mkdir(parents=True, exist_ok=True)
    package_root = STAGING_DIR / "selected-package" / "runebound-elixirs"
    if package_root.exists():
        shutil.rmtree(package_root)
    shutil.copytree(REAL_ELIXIRS_PKG, package_root)
    preview = package_root / "preview.png"
    preview.write_text("PNG placeholder -- dry-run fixture must be rejected before publishing\\n", encoding="utf-8")
    metadata = {
        "title": "Runebound: Elixirs",
        "description": "Class-bound bargain elixirs for Barony. Dry-run Workshop prep fixture; do not publish.",
        "preview": "preview.png",
        "visibility": 2,
        "publishedfileid": "0",
        "changenote": "Dry-run local Workshop preparation report; publishing disabled.",
    }
    if SCENARIO == "metadata validation rows":
        metadata = dict(metadata)
        metadata["title"] = ""
        metadata["description"] = "short"
        metadata["visibility"] = 0
        metadata["publishedfileid"] = "not-a-steam-id"
    write_workshop_toml(package_root / "workshop.toml", metadata)
    content_dir = package_root / "content"
    content_dir.mkdir(parents=True, exist_ok=True)
    install_root = STAGING_DIR / "steam-library" / "steamapps" / "common" / "Barony"
    install_root.mkdir(parents=True, exist_ok=True)
    install_context = {
        "id": "steam-linux-371970-123456",
        "platform": "linux-x86_64",
        "store": "steam",
        "appId": "371970",
        "buildId": "123456",
        "installPath": str(install_root),
        "manifestPath": str(STAGING_DIR / "steam-library" / "steamapps" / "appmanifest_371970.acf"),
        "icon": "store.steam",
        "label": "Steam / Linux Barony install",
    }
    selected_package = {
        "id": "jml.runebound-elixirs",
        "name": "Runebound: Elixirs",
        "version": "0.1.0",
        "packagePath": str(package_root),
        "manifestPath": str(package_root / "bml-package.json"),
        "workshopMetadataPath": str(package_root / "workshop.toml"),
        "previewPath": str(preview),
    }
    stage_dir = STAGING_DIR / "workshop-dry-run-stage"
    return {
        "scenario": SCENARIO,
        "packageRoot": str(package_root),
        "selectedPackage": selected_package,
        "install": install_context,
        "workshopMetadata": metadata,
        "stagingDir": str(stage_dir),
        "dryRun": True,
        "publish": False,
        "mode": "dry-run",
        "safety": {
            "allowPublish": False,
            "requireHiddenVisibility": True,
            "forbidSteamSideEffects": True,
            "reason": "BDD dry-run fixture: never call Steam or steamcmd.",
        },
    }

def command_text(args):
    if not args:
        return ""
    if isinstance(args, (list, tuple)):
        return " ".join(str(part) for part in args)
    return str(args)

def install_dry_run_guards(side_effects):
    originals = {
        "subprocess_run": subprocess.run,
        "subprocess_popen": subprocess.Popen,
        "subprocess_call": subprocess.call,
        "subprocess_check_call": subprocess.check_call,
        "subprocess_check_output": subprocess.check_output,
        "os_system": os.system,
        "socket_create_connection": socket.create_connection,
        "urlopen": urllib.request.urlopen,
    }
    def block_process(*args, **kwargs):
        text = command_text(args[0] if args else kwargs.get("args"))
        side_effects.append({"kind": "process", "command": text})
        if re.search(r"(^|[/\\\\\s])(steamcmd|steam)(\\.exe)?($|[/\\\\\s])", text, re.IGNORECASE):
            raise AssertionError("Workshop Prep attempted a forbidden Steam/steamcmd process in dry-run: " + text)
        raise AssertionError("Workshop Prep attempted an external process in dry-run: " + text)
    def block_system(command):
        side_effects.append({"kind": "process", "command": str(command)})
        raise AssertionError("Workshop Prep attempted os.system in dry-run: " + str(command))
    def block_network(*args, **kwargs):
        side_effects.append({"kind": "network", "target": repr(args[0] if args else kwargs)})
        raise AssertionError("Workshop Prep attempted network access in dry-run: " + repr(args[0] if args else kwargs))
    subprocess.run = block_process
    subprocess.Popen = block_process
    subprocess.call = block_process
    subprocess.check_call = block_process
    subprocess.check_output = block_process
    os.system = block_system
    socket.create_connection = block_network
    urllib.request.urlopen = block_network
    return originals

def restore_guards(originals):
    subprocess.run = originals["subprocess_run"]
    subprocess.Popen = originals["subprocess_popen"]
    subprocess.call = originals["subprocess_call"]
    subprocess.check_call = originals["subprocess_check_call"]
    subprocess.check_output = originals["subprocess_check_output"]
    os.system = originals["os_system"]
    socket.create_connection = originals["socket_create_connection"]
    urllib.request.urlopen = originals["urlopen"]

def call_first_success(fn, fixture):
    package_root = Path(fixture["packageRoot"])
    stage_dir = Path(fixture["stagingDir"])
    context = dict(fixture)
    variants = [
        ("context-keyword", lambda: fn(context=context)),
        ("fixture-keyword", lambda: fn(fixture=fixture)),
        ("current-app-core-keywords", lambda: fn(package_root=package_root, selected_package=package_root, install=fixture["install"], staging_dir=stage_dir, dry_run=True, publish_enabled=False)),
        ("publish-enabled-keywords", lambda: fn(package_root=package_root, install=fixture["install"], staging_dir=stage_dir, dry_run=True, publish_enabled=False)),
        ("dry-run-keywords", lambda: fn(package_root=package_root, selected_package=fixture["selectedPackage"], install=fixture["install"], workshop_metadata=fixture["workshopMetadata"], staging_dir=stage_dir, dry_run=True, publish=False, mode="dry-run")),
        ("package-root-keywords", lambda: fn(package_root=package_root, staging_dir=stage_dir, dry_run=True, publish=False)),
        ("selected-package-keywords", lambda: fn(selected_package=fixture["selectedPackage"], install_context=fixture["install"], workshop_context=fixture["workshopMetadata"], staging_dir=stage_dir, dry_run=True, allow_publish=False)),
        ("positional-context", lambda: fn(context)),
        ("positional-package-stage", lambda: fn(package_root, stage_dir, True)),
    ]
    errors = []
    for label, thunk in variants:
        try:
            return label, thunk()
        except TypeError as exc:
            errors.append(f"{label}: {exc}")
    raise AssertionError("No compatible Workshop Prep call signature. Tried: " + " | ".join(errors))
`;
}

function pyScenario(scenario, stagingDir) {
  return pyPrelude(scenario, stagingDir) + `
fixture = make_fixture()
api_name, api = public_callable(API_CANDIDATES)
if api is None:
    emit({
        "ok": False,
        "contract": "Workshop Prep semantic dry-run API",
        "scenario": SCENARIO,
        "message": "Missing semantic Workshop Prep API for " + SCENARIO + ". Expected one of: " + ", ".join(API_CANDIDATES),
        "details": {"candidateNames": API_CANDIDATES, "fixture": to_plain(fixture)},
        "sideEffects": [],
    })
else:
    side_effects = []
    originals = install_dry_run_guards(side_effects)
    try:
        call_label, response = call_first_success(api, fixture)
        emit({
            "ok": True,
            "contract": "Workshop Prep semantic dry-run API",
            "scenario": SCENARIO,
            "apiName": api_name,
            "callVariant": call_label,
            "fixture": to_plain(fixture),
            "response": to_plain(response),
            "sideEffects": to_plain(side_effects),
        })
    except Exception as exc:
        emit({
            "ok": False,
            "contract": "Workshop Prep semantic dry-run API",
            "scenario": SCENARIO,
            "apiName": api_name,
            "message": str(exc),
            "details": {"candidateNames": API_CANDIDATES, "fixture": to_plain(fixture)},
            "sideEffects": to_plain(side_effects),
        })
    finally:
        restore_guards(originals)
`;
}

function assertWorkshopPrepOk(world) {
  const result = world.workshopPrepContract;
  if (!result) throw new Error("No Workshop Prep contract result was recorded.");
  if (!result.ok) {
    const details = result.details ? `\n${JSON.stringify(result.details, null, 2)}` : "";
    throw new Error(`${result.contract || "Workshop Prep contract"} product gap (${result.scenario || "unknown scenario"}): ${result.message || "unsatisfied"}${details}`);
  }
  return result;
}

function walk(value, visitor, seen = new Set()) {
  if (value == null) return;
  if (typeof value === "object") {
    if (seen.has(value)) return;
    seen.add(value);
  }
  visitor(value);
  if (Array.isArray(value)) {
    value.forEach(item => walk(item, visitor, seen));
  } else if (typeof value === "object") {
    Object.values(value).forEach(item => walk(item, visitor, seen));
  }
}

function flattenValues(value) {
  const out = [];
  walk(value, item => {
    if (item == null) return;
    if (["string", "number", "boolean"].includes(typeof item)) out.push(String(item));
  });
  return out;
}

function valuesForKeys(value, wantedKeys) {
  const wanted = new Set(wantedKeys.map(key => key.toLowerCase()));
  const out = [];
  walk(value, item => {
    if (!item || typeof item !== "object" || Array.isArray(item)) return;
    for (const [key, val] of Object.entries(item)) {
      if (wanted.has(key.toLowerCase())) out.push(val);
    }
  });
  return out;
}

function arraysForKeys(value, wantedKeys) {
  return valuesForKeys(value, wantedKeys).filter(Array.isArray);
}

function objectsFromArrays(arrays) {
  return arrays.flatMap(array => array.filter(item => item && typeof item === "object" && !Array.isArray(item)));
}

function jsonText(value) {
  return JSON.stringify(value, null, 2);
}

function responseOf(result) {
  return result.response || {};
}

function assertNoSteamSideEffects(result) {
  const sideEffects = result.sideEffects || [];
  if (sideEffects.length) {
    throw new Error(`Workshop Prep attempted side effects during dry-run:\n${jsonText(sideEffects)}`);
  }
  const text = JSON.stringify(responseOf(result));
  if (/steamcmd\s+(?:\+login|\+workshop_build_item|\+quit)|workshop_build_item/i.test(text)) {
    throw new Error(`Workshop Prep response exposed an executable Steam publish command instead of a dry-run report:\n${text}`);
  }
}

function assertHasText(value, pattern, message) {
  const text = JSON.stringify(value);
  if (!pattern.test(text)) throw new Error(`${message}:\n${jsonText(value)}`);
}

Given("a clean Workshop Prep staging directory", function () {
  this.workshopPrepStagingDir = fs.mkdtempSync(path.join(os.tmpdir(), "bml-workshop-prep-"));
});

After(function () {
  if (this.workshopPrepStagingDir) {
    fs.rmSync(this.workshopPrepStagingDir, { recursive: true, force: true });
  }
});

When("I request the Workshop Prep {string} contract", function (scenario) {
  runWorkshopPrepPython(pyScenario(scenario, this.workshopPrepStagingDir), this);
});

Then("the Workshop Prep response is dry-run only and hidden by default", function () {
  const result = assertWorkshopPrepOk(this);
  const response = responseOf(result);
  const text = JSON.stringify(response);
  const dryRunValues = valuesForKeys(response, ["dryRun", "dry_run", "mode", "status", "publishMode"]);
  if (!dryRunValues.some(value => value === true || /dry-run|dry run|stub|preview/i.test(String(value)))) {
    throw new Error(`Workshop Prep response does not expose dry-run/stub mode:\n${jsonText(response)}`);
  }
  if (valuesForKeys(response, ["publishEnabled", "canPublish", "allowPublish", "publish"]).some(value => value === true || /^true$/i.test(String(value)))) {
    throw new Error(`Workshop Prep response enables publishing in a dry-run fixture:\n${jsonText(response)}`);
  }
  if (!/("visibility"\s*:\s*2|hidden|private)/i.test(text)) {
    throw new Error(`Workshop Prep response does not keep Workshop visibility hidden/private:\n${jsonText(response)}`);
  }
  if (!/("publishedfileid"\s*:\s*"?0"?|unpublished|not published)/i.test(text)) {
    throw new Error(`Workshop Prep response does not preserve unpublished publishedfileid=0 state:\n${jsonText(response)}`);
  }
  assertNoSteamSideEffects(result);
});

Then("the Workshop Prep response identifies the selected Runebound package", function () {
  const result = assertWorkshopPrepOk(this);
  const response = responseOf(result);
  const fixturePackage = result.fixture.selectedPackage;
  assertHasText(response, /jml\.runebound-elixirs/, "Workshop Prep response lacks selected package id");
  assertHasText(response, /Runebound: Elixirs/, "Workshop Prep response lacks selected package title/name");
  assertHasText(response, /0\.1\.0/, "Workshop Prep response lacks selected package version");
  if (!JSON.stringify(response).includes(fixturePackage.packagePath)) {
    throw new Error(`Workshop Prep response does not preserve selected package path ${fixturePackage.packagePath}:\n${jsonText(response)}`);
  }
  assertNoSteamSideEffects(result);
});

Then("the Workshop Prep response exposes install and Workshop icon labels", function () {
  const result = assertWorkshopPrepOk(this);
  const response = responseOf(result);
  assertHasText(response, /store\.steam|Steam \/ Linux|Steam/i, "Workshop Prep response lacks selected Steam install context");
  assertHasText(response, /store\.steam_workshop|Steam Workshop/i, "Workshop Prep response lacks Steam Workshop icon/label context");
  assertHasText(response, /os\.linux|linux-x86_64|Linux/i, "Workshop Prep response lacks Linux install icon/platform context");
  assertNoSteamSideEffects(result);
});

Then("the Workshop Prep response contains semantic metadata validation rows", function () {
  const result = assertWorkshopPrepOk(this);
  const response = responseOf(result);
  const rows = objectsFromArrays(arraysForKeys(response, ["metadataRows", "validationRows", "rows", "checks", "problems", "diagnostics"]));
  if (!rows.length) {
    throw new Error(`Workshop Prep response lacks semantic validation row objects:\n${jsonText(response)}`);
  }
  const rowsText = JSON.stringify(rows);
  for (const field of ["title", "description", "visibility", "publishedfileid"]) {
    if (!new RegExp(field, "i").test(rowsText)) {
      throw new Error(`Workshop Prep metadata rows lack field '${field}':\n${jsonText(rows)}`);
    }
  }
  if (!rows.some(row => /status|severity|level/i.test(Object.keys(row).join(" ")))) {
    throw new Error(`Workshop Prep metadata rows lack status/severity fields:\n${jsonText(rows)}`);
  }
  if (/^Package validation:/m.test(rowsText) || /\bFAILED:\s/m.test(rowsText)) {
    throw new Error(`Workshop Prep metadata rows embed raw command output instead of semantic rows:\n${rowsText}`);
  }
  assertNoSteamSideEffects(result);
});

Then("the Workshop Prep response blocks placeholder or invalid preview assets", function () {
  const result = assertWorkshopPrepOk(this);
  const response = responseOf(result);
  const text = JSON.stringify(response);
  if (!/preview/i.test(text)) {
    throw new Error(`Workshop Prep response lacks preview asset validation context:\n${jsonText(response)}`);
  }
  if (!/placeholder|invalid|missing|not an image|blocked|error|fail/i.test(text)) {
    throw new Error(`Workshop Prep response does not block the placeholder preview asset with a semantic validation status:\n${jsonText(response)}`);
  }
  if (valuesForKeys(response, ["previewValid", "validPreview", "assetValid"]).some(value => value === true || /^true$/i.test(String(value)))) {
    throw new Error(`Workshop Prep response marks placeholder preview as valid:\n${jsonText(response)}`);
  }
  assertNoSteamSideEffects(result);
});

Then("the Workshop Prep response stages only inside the local dry-run directory", function () {
  const result = assertWorkshopPrepOk(this);
  const response = responseOf(result);
  const stagingValues = valuesForKeys(response, ["stagingDir", "stagingFolder", "localStagingFolder", "stagingPath", "stagePath", "outputDir"])
    .map(String)
    .filter(value => value.includes("workshop") || value.includes("stage"));
  if (!stagingValues.length) {
    throw new Error(`Workshop Prep response lacks a local Workshop staging folder path:\n${jsonText(response)}`);
  }
  const root = path.resolve(this.workshopPrepStagingDir);
  for (const value of stagingValues) {
    const resolved = path.resolve(value);
    if (!(resolved === root || resolved.startsWith(root + path.sep))) {
      throw new Error(`Workshop Prep staging path escapes the dry-run temp directory. Root=${root}, path=${value}\n${jsonText(response)}`);
    }
    if (/steamapps|steamcmd|workshop\/content|\.local\/share\/Steam/i.test(value)) {
      throw new Error(`Workshop Prep staging path points at Steam-managed storage instead of local dry-run staging: ${value}`);
    }
  }
  if (!stagingValues.some(value => fs.existsSync(value))) {
    throw new Error(`Workshop Prep did not create any reported local staging folder: ${stagingValues.join(", ")}`);
  }
  assertNoSteamSideEffects(result);
});

Then("the Workshop Prep response includes a dry-run VDF report without publishing", function () {
  const result = assertWorkshopPrepOk(this);
  const response = responseOf(result);
  const vdfValues = valuesForKeys(response, ["vdf", "vdfReport", "dryRunVdfReport", "vdfFields", "workshopVdf", "vdfPreview", "report"]);
  if (!vdfValues.length) {
    throw new Error(`Workshop Prep response lacks a VDF report/preview:\n${jsonText(response)}`);
  }
  const text = JSON.stringify(vdfValues);
  for (const pattern of [/371970/, /contentfolder|content_folder/i, /previewfile|preview_file|preview/i, /visibility/i, /publishedfileid|published_file_id/i]) {
    if (!pattern.test(text)) {
      throw new Error(`Workshop Prep VDF report lacks ${pattern}:\n${jsonText(vdfValues)}`);
    }
  }
  if (!/dry-run|dry run|preview|not published|publish(?:ing)? disabled/i.test(JSON.stringify(response))) {
    throw new Error(`Workshop Prep VDF response does not label the report as dry-run/non-publishing:\n${jsonText(response)}`);
  }
  assertNoSteamSideEffects(result);
});

Then("the Workshop Prep response keeps publish disabled with explicit reasons", function () {
  const result = assertWorkshopPrepOk(this);
  const response = responseOf(result);
  if (valuesForKeys(response, ["publishEnabled", "canPublish", "allowPublish", "publish"]).some(value => value === true || /^true$/i.test(String(value)))) {
    throw new Error(`Workshop Prep response enables publish despite the disabled-publish contract:\n${jsonText(response)}`);
  }
  const reasonValues = valuesForKeys(response, ["disabledReasons", "disabled_reasons", "blockers", "reasons", "problems"]);
  if (!reasonValues.length) {
    throw new Error(`Workshop Prep response lacks disabled publish reasons:\n${jsonText(response)}`);
  }
  const text = JSON.stringify(reasonValues);
  if (!/publish|steam|workshop|dry-run|dry run|hidden|preview|evidence|disabled/i.test(text)) {
    throw new Error(`Workshop Prep disabled reasons are not explicit about publish/Workshop blocking:\n${jsonText(reasonValues)}`);
  }
  assertNoSteamSideEffects(result);
});

Then("the Workshop Prep response reports no Steam, steamcmd, or network side effects", function () {
  const result = assertWorkshopPrepOk(this);
  assertNoSteamSideEffects(result);
  const response = responseOf(result);
  if (valuesForKeys(response, ["steamCalled", "steamcmdCalled", "networkCalled", "published", "uploaded"]).some(value => value === true || /^true$/i.test(String(value)))) {
    throw new Error(`Workshop Prep response reports forbidden Steam/network side effects:\n${jsonText(response)}`);
  }
  assertHasText(response, /dry-run|dry run|stub|no side effects|publish(?:ing)? disabled/i, "Workshop Prep response does not explicitly report dry-run/no-publish semantics");
});
