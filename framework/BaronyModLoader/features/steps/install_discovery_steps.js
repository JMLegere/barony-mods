"use strict";

const { After, Given, When, Then } = require("@cucumber/cucumber");
const { execFileSync } = require("child_process");
const fs = require("fs");
const os = require("os");
const path = require("path");

// paths: <repo>/framework/BaronyModLoader/features/steps/install_discovery_steps.js
//          ../../../../ from here = <repo>
const REPO_ROOT = path.resolve(__dirname, "../../../../");
const BML_APP = path.join(REPO_ROOT, "framework/BaronyModLoader/app/barony_mod_loader.py");

function runInstallDiscoveryPython(script, world) {
  const tmp = path.join(os.tmpdir(), `bml-install-discovery-${Date.now()}-${Math.random().toString(36).slice(2)}.py`);
  fs.writeFileSync(tmp, script, "utf-8");
  try {
    const stdout = execFileSync("/usr/bin/python3", [tmp], {
      cwd: REPO_ROOT,
      encoding: "utf-8",
      timeout: 30000,
      stdio: ["ignore", "pipe", "pipe"],
    });
    const marker = "__BML_INSTALL_DISCOVERY_JSON__";
    const line = stdout.split(/\r?\n/).find(entry => entry.startsWith(marker));
    if (!line) {
      throw new Error(`Install discovery script did not emit JSON marker. Output:\n${stdout}`);
    }
    world.installDiscoveryContract = JSON.parse(line.slice(marker.length));
  } catch (err) {
    const stdout = err.stdout ? String(err.stdout) : "";
    const stderr = err.stderr ? String(err.stderr) : "";
    throw new Error(`Install discovery contract script failed (exit ${err.status ?? 1}):\nSTDOUT:\n${stdout}\nSTDERR:\n${stderr}`);
  } finally {
    try { fs.unlinkSync(tmp); } catch (_) {}
  }
}

function pyPrelude(fixture) {
  return `
import dataclasses
import importlib.util
import inspect
import json
import os
from pathlib import Path
import shutil
import stat
import sys
import tempfile

REPO_ROOT = Path(${JSON.stringify(REPO_ROOT)})
BML_APP = Path(${JSON.stringify(BML_APP)})
FIXTURE = json.loads(${JSON.stringify(JSON.stringify(fixture))})
STAGING_DIR = Path(FIXTURE["stagingDir"])

spec = importlib.util.spec_from_file_location("bml_app", BML_APP)
mod = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = mod
spec.loader.exec_module(mod)

def emit(payload):
    print("__BML_INSTALL_DISCOVERY_JSON__" + json.dumps(payload, sort_keys=True))

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

def write_acf(path, build_id, install_dir="Barony", app_id="371970", name="Barony"):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(
        '"AppState"\\n{\\n'
        f'    "appid" "{app_id}"\\n'
        f'    "name" "{name}"\\n'
        f'    "installdir" "{install_dir}"\\n'
        f'    "buildid" "{build_id}"\\n'
        '}\\n',
        encoding="utf-8",
    )

def write_executable(path, marker):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(marker + "\\n", encoding="utf-8")
    path.chmod(path.stat().st_mode | stat.S_IXUSR)

def make_linux_library(name, *, build_id=None, executable=True):
    steamapps = STAGING_DIR / name / "steamapps"
    install = steamapps / "common" / "Barony"
    manifest = steamapps / "appmanifest_371970.acf"
    steamapps.mkdir(parents=True, exist_ok=True)
    if build_id is not None:
        write_acf(manifest, build_id)
        install.mkdir(parents=True, exist_ok=True)
        if executable:
            write_executable(install / "barony.x86_64", f"fake-linux-barony-build-{build_id}")
    return {
        "id": name,
        "platform": "linux",
        "steamappsPath": str(steamapps),
        "libraryPath": str(steamapps.parent),
        "manifestPath": str(manifest),
        "installPath": str(install),
        "executable": str(install / "barony.x86_64"),
        "buildId": build_id,
        "store": "steam",
        "appId": "371970",
    }

def make_windows_record():
    install = STAGING_DIR / "windows-library" / "steamapps" / "common" / "Barony"
    manifest = STAGING_DIR / "windows-library" / "steamapps" / "appmanifest_371970.acf"
    write_acf(manifest, "777777")
    write_executable(install / "barony.exe", "fake-windows-barony-build-777777")
    return {
        "id": "steam-windows-371970-777777",
        "platform": "windows",
        "platformTarget": "windows",
        "os": "windows",
        "store": "steam",
        "appId": "371970",
        "buildId": "777777",
        "manifestPath": str(manifest),
        "installPath": str(install),
        "executable": str(install / "barony.exe"),
        "liveVerification": None,
        "runtimeEvidence": None,
    }

def scenario_fixture():
    scenario = FIXTURE["scenario"]
    data = {
        "scenario": scenario,
        "platform": "linux",
        "steamLibraries": [],
        "discoveryRecords": [],
        "selectedInstallId": FIXTURE.get("selectedInstallId"),
        "selectedInstallPath": None,
        "rawDiscoveryOutput": None,
    }
    if scenario == "missing":
        missing = make_linux_library("empty-primary")
        data["steamLibraries"].append(missing)
    elif scenario == "linux-verified":
        record = make_linux_library("primary-linux", build_id=FIXTURE["buildId"])
        record["id"] = f"steam-linux-371970-{FIXTURE['buildId']}"
        data["steamLibraries"].append(record)
        data["discoveryRecords"].append(record)
    elif scenario == "windows-fail-closed":
        data["platform"] = "windows"
        record = make_windows_record()
        data["steamLibraries"].append(record)
        data["discoveryRecords"].append(record)
    elif scenario == "multiple-libraries":
        primary = make_linux_library("primary-empty")
        secondary = make_linux_library("secondary-with-barony", build_id=FIXTURE["buildId"])
        secondary["id"] = f"steam-linux-371970-{FIXTURE['buildId']}"
        data["steamLibraries"].extend([primary, secondary])
        data["discoveryRecords"].append(secondary)
    elif scenario == "malformed-output":
        data["rawDiscoveryOutput"] = "{ not valid steam discovery json"
        data["discoveryRecords"].append({
            "id": "malformed-steam-output",
            "platform": "linux",
            "store": "steam",
            "raw": data["rawDiscoveryOutput"],
            "error": "malformed discovery output",
        })
    elif scenario == "selected-propagation":
        record = make_linux_library("selected-linux", build_id="222222")
        record["id"] = FIXTURE["selectedInstallId"]
        record["selected"] = True
        data["steamLibraries"].append(record)
        data["discoveryRecords"].append(record)
        data["selectedInstallPath"] = record["installPath"]
    else:
        raise AssertionError(f"Unknown install discovery fixture scenario: {scenario}")
    return data

def call_install_discovery_api(fixture):
    candidate_names = (
        "build_install_discovery_state",
        "discover_install_state",
        "discover_install_states",
        "build_install_state",
        "build_barony_install_state",
        "discover_barony_install_state",
        "discover_barony_installs",
        "list_install_states",
    )
    api_name, api = public_callable(candidate_names)
    if api is None:
        emit({
            "ok": False,
            "contract": "Install discovery app-core API",
            "message": "Missing semantic install discovery API. Expected one of: " + ", ".join(candidate_names),
            "fixture": fixture,
            "details": {"candidateNames": list(candidate_names)},
        })
        return

    steamapps_paths = [entry["steamappsPath"] for entry in fixture.get("steamLibraries", []) if entry.get("steamappsPath")]
    library_paths = [entry.get("libraryPath") for entry in fixture.get("steamLibraries", []) if entry.get("libraryPath")]
    selected_id = fixture.get("selectedInstallId")
    selected_path = fixture.get("selectedInstallPath")
    variants = [
        ("fixture-positional", lambda: api(fixture)),
        ("fixture-keyword", lambda: api(discovery_fixture=fixture)),
        ("discovery-inputs-keyword", lambda: api(discovery_inputs=fixture)),
        ("steam-libraries-keyword", lambda: api(steam_libraries=library_paths, steamapps_paths=steamapps_paths, selected_install_id=selected_id, selected_install_path=selected_path, platform=fixture.get("platform"))),
        ("steamapps-keyword", lambda: api(steamapps_candidates=steamapps_paths, selected_install_id=selected_id, platform=fixture.get("platform"))),
        ("records-keyword", lambda: api(discovery_records=fixture.get("discoveryRecords", []), selected_install_id=selected_id, platform=fixture.get("platform"))),
        ("raw-output-keyword", lambda: api(raw_discovery_output=fixture.get("rawDiscoveryOutput"), selected_install_id=selected_id, platform=fixture.get("platform"))),
        ("no-args", lambda: api()),
    ]
    errors = []
    for label, thunk in variants:
        try:
            response = thunk()
            emit({
                "ok": True,
                "apiName": api_name,
                "callVariant": label,
                "fixture": fixture,
                "response": to_plain(response),
            })
            return
        except TypeError as exc:
            errors.append(f"{label}: {exc}")
    emit({
        "ok": False,
        "contract": "Install discovery app-core API",
        "message": f"{api_name} exists but has no compatible Cucumber contract call signature.",
        "fixture": fixture,
        "details": {"callErrors": errors},
    })

fixture = scenario_fixture()
call_install_discovery_api(fixture)
`;
}

function flattenValues(value, out = []) {
  if (value == null) return out;
  if (typeof value === "string" || typeof value === "number" || typeof value === "boolean") {
    out.push(String(value));
  } else if (Array.isArray(value)) {
    value.forEach(item => flattenValues(item, out));
  } else if (typeof value === "object") {
    Object.values(value).forEach(item => flattenValues(item, out));
  }
  return out;
}

function allText(value) {
  return flattenValues(value).join("\n").toLowerCase();
}

function requireContract(world) {
  const contract = world.installDiscoveryContract;
  if (!contract) throw new Error("No install discovery contract result was recorded.");
  if (!contract.ok) {
    const details = contract.details ? `\n${JSON.stringify(contract.details, null, 2)}` : "";
    const fixture = contract.fixture ? `\nFixture:\n${JSON.stringify(contract.fixture, null, 2)}` : "";
    throw new Error(`${contract.contract || "Install discovery contract"} failed: ${contract.message || "unsatisfied"}${details}${fixture}`);
  }
  return contract;
}

function objectsIn(value, out = []) {
  if (value == null) return out;
  if (Array.isArray(value)) {
    value.forEach(item => objectsIn(item, out));
  } else if (typeof value === "object") {
    out.push(value);
    Object.values(value).forEach(item => objectsIn(item, out));
  }
  return out;
}

function keyIncludes(key, needles) {
  const folded = String(key).toLowerCase();
  return needles.some(needle => folded.includes(needle));
}

function objectHasSemanticValue(obj, keyNeedles, valueNeedles) {
  return Object.entries(obj).some(([key, value]) => {
    if (!keyIncludes(key, keyNeedles)) return false;
    const folded = String(value).toLowerCase();
    return valueNeedles.some(needle => folded.includes(needle));
  });
}

function objectMentions(obj, needles) {
  const text = allText(obj);
  return needles.every(needle => text.includes(needle));
}

function installObjects(contract) {
  const fixturePaths = new Set();
  for (const library of contract.fixture?.steamLibraries || []) {
    for (const key of ["installPath", "executable", "manifestPath", "steamappsPath", "libraryPath"]) {
      if (library[key]) fixturePaths.add(String(library[key]));
    }
  }
  return objectsIn(contract.response).filter(obj => {
    const text = allText(obj);
    if ([...fixturePaths].some(item => text.includes(item.toLowerCase()))) return true;
    if (objectHasSemanticValue(obj, ["source", "store", "platform", "os"], ["steam", "linux", "windows"])) return true;
    if (objectHasSemanticValue(obj, ["status"], ["missing", "verified", "available", "blocked", "invalid", "malformed"])) return true;
    return false;
  });
}

function requireText(value, needles, label) {
  const text = allText(value);
  for (const needle of needles) {
    if (!text.includes(needle)) {
      throw new Error(`Expected ${label} to include ${JSON.stringify(needle)}.\nResponse:\n${JSON.stringify(value, null, 2)}`);
    }
  }
}

function requireAnyText(value, needles, label) {
  const text = allText(value);
  if (!needles.some(needle => text.includes(needle))) {
    throw new Error(`Expected ${label} to include one of ${needles.map(JSON.stringify).join(", ")}.\nResponse:\n${JSON.stringify(value, null, 2)}`);
  }
}

function requireSemanticIcon(response, keyNeedles, valueNeedles, label) {
  const found = objectsIn(response).some(obj => objectHasSemanticValue(obj, keyNeedles, valueNeedles));
  if (!found) {
    throw new Error(`Expected ${label} semantic icon value (${valueNeedles.join("/")}).\nResponse:\n${JSON.stringify(response, null, 2)}`);
  }
}

Given("a clean Install Discovery staging directory", function () {
  this.installDiscoveryStagingDir = fs.mkdtempSync(path.join(os.tmpdir(), "bml-install-discovery-"));
});

After(function () {
  if (this.installDiscoveryStagingDir) {
    fs.rmSync(this.installDiscoveryStagingDir, { recursive: true, force: true });
  }
});

Given("no Barony install exists in the discovery inputs", function () {
  this.installDiscoveryFixture = {
    scenario: "missing",
    stagingDir: this.installDiscoveryStagingDir,
  };
});

Given("a Linux Steam library contains a verified Barony install with build id {string}", function (buildId) {
  this.installDiscoveryFixture = {
    scenario: "linux-verified",
    stagingDir: this.installDiscoveryStagingDir,
    buildId,
  };
});

Given("a Windows Steam discovery record exists without live verification evidence", function () {
  this.installDiscoveryFixture = {
    scenario: "windows-fail-closed",
    stagingDir: this.installDiscoveryStagingDir,
  };
});

Given("multiple Steam libraries exist and only the secondary library contains Barony build id {string}", function (buildId) {
  this.installDiscoveryFixture = {
    scenario: "multiple-libraries",
    stagingDir: this.installDiscoveryStagingDir,
    buildId,
  };
});

Given("Steam discovery returns malformed install output", function () {
  this.installDiscoveryFixture = {
    scenario: "malformed-output",
    stagingDir: this.installDiscoveryStagingDir,
  };
});

Given("a Linux Steam install is selected for the active app-core session", function () {
  this.installDiscoveryFixture = {
    scenario: "selected-propagation",
    stagingDir: this.installDiscoveryStagingDir,
    selectedInstallId: "steam-linux-selected-install",
  };
});

When("I ask app-core to build the install discovery state", function () {
  if (!this.installDiscoveryFixture) throw new Error("Install discovery fixture was not prepared.");
  runInstallDiscoveryPython(pyPrelude(this.installDiscoveryFixture), this);
});

Then("the install discovery state is missing and disables install-dependent actions", function () {
  const contract = requireContract(this);
  requireAnyText(contract.response, ["missing", "not_found", "no install", "no_install"], "missing install state");
  requireAnyText(contract.response, ["disabled", "blocked", "fail-closed", "fail closed"], "missing install disabled state");
  requireAnyText(contract.response, ["install"], "missing install reason");
});

Then("the install discovery state exposes OS, store, and runtime status icons", function () {
  const contract = requireContract(this);
  requireSemanticIcon(contract.response, ["icon"], ["os.linux", "linux", "os.windows", "windows"], "OS");
  requireSemanticIcon(contract.response, ["icon"], ["store.steam", "steam"], "store");
  requireSemanticIcon(contract.response, ["icon"], ["runtime.", "not_run", "unverified", "verified", "failed"], "runtime status");
});

Then("the install discovery state includes a verified Linux Steam install with path and build evidence", function () {
  const contract = requireContract(this);
  const expected = contract.fixture.discoveryRecords[0];
  const matches = installObjects(contract).filter(obj => {
    const text = allText(obj);
    return text.includes(String(expected.installPath).toLowerCase()) || text.includes(String(expected.executable).toLowerCase());
  });
  if (matches.length === 0) {
    throw new Error(`Expected install state to include Linux Steam install path ${expected.installPath}.\nResponse:\n${JSON.stringify(contract.response, null, 2)}`);
  }
  const install = matches[0];
  requireText(install, ["linux", "steam", String(expected.buildId).toLowerCase()], "verified Linux Steam install");
  requireAnyText(install, ["verified", "available", "ready", "ok"], "verified Linux Steam status");
  requireAnyText(install, ["manifest", "build", "executable", "sha256"], "Linux path/build evidence");
});

Then("the install discovery state lists Windows but keeps it fail-closed", function () {
  const contract = requireContract(this);
  const windowsRecords = installObjects(contract).filter(obj => objectMentions(obj, ["windows"]));
  if (windowsRecords.length === 0) {
    throw new Error(`Expected a visible Windows install state.\nResponse:\n${JSON.stringify(contract.response, null, 2)}`);
  }
  const text = allText(windowsRecords);
  if (!(text.includes("disabled") || text.includes("blocked") || text.includes("fail-closed") || text.includes("fail closed") || text.includes("unverified"))) {
    throw new Error(`Expected Windows install state to be fail-closed/unverified.\nWindows records:\n${JSON.stringify(windowsRecords, null, 2)}`);
  }
});

Then("Windows install disabled reasons require live verification", function () {
  const contract = requireContract(this);
  const text = allText(contract.response);
  for (const needle of ["windows", "verification"]) {
    if (!text.includes(needle)) {
      throw new Error(`Expected Windows disabled reason to mention ${needle}.\nResponse:\n${JSON.stringify(contract.response, null, 2)}`);
    }
  }
  requireAnyText(contract.response, ["live", "production", "runtime evidence"], "Windows live verification disabled reason");
});

Then("the install discovery state includes the Barony install from the secondary Steam library", function () {
  const contract = requireContract(this);
  const secondary = contract.fixture.steamLibraries.find(entry => entry.id === "secondary-with-barony");
  if (!secondary) throw new Error("Secondary Steam library fixture was not recorded.");
  requireText(contract.response, [String(secondary.installPath).toLowerCase(), String(secondary.buildId).toLowerCase()], "secondary Steam library install");
  requireAnyText(contract.response, ["secondary", String(secondary.steamappsPath).toLowerCase(), String(secondary.libraryPath).toLowerCase()], "secondary Steam library provenance");
});

Then("the malformed discovery output is reported as a structured disabled install state", function () {
  const contract = requireContract(this);
  requireAnyText(contract.response, ["malformed", "invalid", "parse", "structured"], "malformed discovery status");
  requireAnyText(contract.response, ["disabled", "blocked", "fail-closed", "fail closed"], "malformed discovery disabled state");
  const text = allText(contract.response);
  if (text.includes("traceback")) {
    throw new Error(`Malformed discovery output leaked a Python traceback instead of structured state.\nResponse:\n${JSON.stringify(contract.response, null, 2)}`);
  }
});

Then("the selected install propagates to the dashboard and readiness state", function () {
  const contract = requireContract(this);
  const selectedId = contract.fixture.selectedInstallId;
  const selectedPath = contract.fixture.selectedInstallPath;
  requireText(contract.response, [selectedId.toLowerCase()], "selected install id propagation");
  requireText(contract.response, [String(selectedPath).toLowerCase()], "selected install path propagation");

  const selectedRecords = objectsIn(contract.response).filter(obj => {
    const text = allText(obj);
    return text.includes(selectedId.toLowerCase()) && (text.includes("selected") || text.includes("active"));
  });
  if (selectedRecords.length === 0) {
    throw new Error(`Expected at least one semantic state object to mark the install selected/active.\nResponse:\n${JSON.stringify(contract.response, null, 2)}`);
  }

  const responseText = allText(contract.response);
  for (const section of ["dashboard", "readiness"]) {
    if (!responseText.includes(section)) {
      throw new Error(`Expected selected install to propagate into ${section} state.\nResponse:\n${JSON.stringify(contract.response, null, 2)}`);
    }
  }
});
