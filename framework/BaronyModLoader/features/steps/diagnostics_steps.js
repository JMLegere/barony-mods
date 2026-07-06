"use strict";

const { After, Given, When, Then } = require("@cucumber/cucumber");
const { execFileSync } = require("child_process");
const fs = require("fs");
const os = require("os");
const path = require("path");

// paths: <repo>/framework/BaronyModLoader/features/steps/diagnostics_steps.js
//          ../../../../ from here = <repo>
const REPO_ROOT = path.resolve(__dirname, "../../../../");
const BML_APP = path.join(REPO_ROOT, "framework/BaronyModLoader/app/barony_mod_loader.py");
const RUNTIME_REPORT_LOADED = path.join(REPO_ROOT, "framework/BaronyModLoader/fixtures/runtime-load-report.loaded.json");

const DIAGNOSTICS_REPOSITORY_CANDIDATES = [
  "load_diagnostics_repository",
  "diagnostics_repository_load",
  "read_diagnostics_repository",
  "build_diagnostics_repository",
];

function runDiagnosticsPython(script, world) {
  const tmp = path.join(os.tmpdir(), `bml-diagnostics-${Date.now()}-${Math.random().toString(36).slice(2)}.py`);
  fs.writeFileSync(tmp, script, "utf-8");
  try {
    const stdout = execFileSync("/usr/bin/python3", [tmp], {
      cwd: REPO_ROOT,
      encoding: "utf-8",
      timeout: 30000,
      stdio: ["ignore", "pipe", "pipe"],
    });
    const marker = "__BML_DIAGNOSTICS_JSON__";
    const line = stdout.split(/\r?\n/).find(entry => entry.startsWith(marker));
    if (!line) {
      throw new Error(`Diagnostics contract script did not emit JSON marker. Output:\n${stdout}`);
    }
    world.diagnosticsContract = JSON.parse(line.slice(marker.length));
  } catch (err) {
    const stdout = err.stdout ? String(err.stdout) : "";
    const stderr = err.stderr ? String(err.stderr) : "";
    throw new Error(`Diagnostics contract script failed (exit ${err.status ?? 1}):\nSTDOUT:\n${stdout}\nSTDERR:\n${stderr}`);
  } finally {
    try { fs.unlinkSync(tmp); } catch (_) {}
  }
}

function pyPrelude(stagingDir) {
  return `
import dataclasses
import importlib.util
import json
from pathlib import Path
import shutil
import sys

REPO_ROOT = Path(${JSON.stringify(REPO_ROOT)})
BML_APP = Path(${JSON.stringify(BML_APP)})
RUNTIME_REPORT_LOADED = Path(${JSON.stringify(RUNTIME_REPORT_LOADED)})
STAGING_DIR = Path(${JSON.stringify(stagingDir)})
DIAGNOSTICS_REPOSITORY_CANDIDATES = ${JSON.stringify(DIAGNOSTICS_REPOSITORY_CANDIDATES)}

spec = importlib.util.spec_from_file_location("bml_app", BML_APP)
mod = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = mod
spec.loader.exec_module(mod)

def emit(payload):
    print("__BML_DIAGNOSTICS_JSON__" + json.dumps(payload, sort_keys=True))

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

def as_items(value):
    plain = to_plain(value)
    if isinstance(plain, dict):
        for key in ("items", "diagnostics", "reports", "runtimeReports", "entries", "rows"):
            if isinstance(plain.get(key), list):
                return plain[key]
    if isinstance(plain, list):
        return plain
    return []

def item_for_path(value, target_path):
    target = str(Path(target_path).resolve(strict=False))
    for item in as_items(value):
        if isinstance(item, dict) and str(Path(str(item.get("path", ""))).resolve(strict=False)) == target:
            return item
    return None

def load_repository(paths):
    name, fn = public_callable(DIAGNOSTICS_REPOSITORY_CANDIDATES)
    if fn is None:
        return {
            "ok": False,
            "contract": "diagnostics repository",
            "message": "No public diagnostics repository function found.",
            "details": {"expectedAnyOf": DIAGNOSTICS_REPOSITORY_CANDIDATES},
        }
    errors = []
    variants = [
        ("report_paths keyword", lambda: fn(report_paths=paths)),
        ("paths list", lambda: fn(paths)),
        ("single path", lambda: fn(paths[0]) if len(paths) == 1 else None),
    ]
    for label, thunk in variants:
        try:
            value = thunk()
            if value is None:
                continue
            return {
                "ok": True,
                "contract": "diagnostics repository",
                "function": name,
                "call": label,
                "repository": to_plain(value),
                "items": as_items(value),
            }
        except TypeError as exc:
            errors.append(f"{label}: {exc}")
    return {
        "ok": False,
        "contract": "diagnostics repository",
        "message": "No compatible diagnostics repository call signature accepted report paths.",
        "details": {"function": name, "tried": errors, "expectedAnyOf": DIAGNOSTICS_REPOSITORY_CANDIDATES},
    }

def loaded_fixture_payload():
    return json.loads(RUNTIME_REPORT_LOADED.read_text(encoding="utf-8"))

def write_report(name, payload):
    report_dir = STAGING_DIR / "reports"
    report_dir.mkdir(parents=True, exist_ok=True)
    report_path = report_dir / name
    report_path.write_text(json.dumps(payload, indent=2, sort_keys=True), encoding="utf-8")
    return report_path

def emit_repository_case(case_name, target_path, paths):
    result = load_repository(paths)
    if result.get("ok"):
        result["case"] = case_name
        result["targetPath"] = str(target_path)
        result["item"] = item_for_path(result.get("repository"), target_path)
        if result["item"] is None:
            result["ok"] = False
            result["message"] = "Diagnostics repository did not return an item for the target report path."
            result["details"] = {"targetPath": str(target_path), "items": result.get("items", [])}
    emit(result)
`;
}

function assertDiagnosticsOk(world) {
  const result = world.diagnosticsContract;
  if (!result) throw new Error("No diagnostics contract result was recorded.");
  if (!result.ok) {
    const details = result.details ? `\n${JSON.stringify(result.details, null, 2)}` : "";
    throw new Error(`${result.contract || "Diagnostics contract"} failed: ${result.message || "unsatisfied"}${details}`);
  }
  return result;
}

function resultItem(world) {
  const result = assertDiagnosticsOk(world);
  if (!result.item || typeof result.item !== "object") {
    throw new Error(`Diagnostics result does not include a semantic item for ${result.targetPath}:\n${JSON.stringify(result, null, 2)}`);
  }
  return result.item;
}

function textOf(value) {
  return JSON.stringify(value || {}).toLowerCase();
}

function problemsOf(item) {
  return Array.isArray(item.problems) ? item.problems : [];
}

function problemCodes(item) {
  return problemsOf(item).map(problem => String(problem.code || ""));
}

function requireProblemCode(item, code) {
  const codes = problemCodes(item);
  if (!codes.includes(code)) {
    throw new Error(`Expected problem code ${code}, got [${codes.join(", ")}]:\n${JSON.stringify(item, null, 2)}`);
  }
}

function requireText(item, needles, label) {
  const text = textOf(item);
  for (const needle of needles) {
    if (!text.includes(needle.toLowerCase())) {
      throw new Error(`Expected ${label} to include ${needle}:\n${JSON.stringify(item, null, 2)}`);
    }
  }
}

function runCase(world, pythonBody) {
  if (!world.diagnosticsStagingDir) {
    throw new Error("Diagnostics staging directory was not initialized.");
  }
  runDiagnosticsPython(pyPrelude(world.diagnosticsStagingDir) + pythonBody, world);
}

Given("a clean Diagnostics staging directory", function () {
  this.diagnosticsStagingDir = fs.mkdtempSync(path.join(os.tmpdir(), "bml-diagnostics-"));
});

After(function () {
  if (this.diagnosticsStagingDir) {
    fs.rmSync(this.diagnosticsStagingDir, { recursive: true, force: true });
  }
});

When("I load diagnostics for a missing runtime report", function () {
  runCase(this, `
target = STAGING_DIR / "reports" / "missing-runtime-load-report.json"
target.parent.mkdir(parents=True, exist_ok=True)
emit_repository_case("missing report", target, [target])
`);
});

Then("diagnostics marks the report as missing with a fatal missing-report problem", function () {
  const item = resultItem(this);
  if (item.status !== "missing") {
    throw new Error(`Expected missing status, got ${item.status}:\n${JSON.stringify(item, null, 2)}`);
  }
  requireProblemCode(item, "BML_DIAGNOSTIC_REPORT_MISSING");
  const missingProblem = problemsOf(item).find(problem => problem.code === "BML_DIAGNOSTIC_REPORT_MISSING");
  if (!missingProblem || missingProblem.severity !== "fatal") {
    throw new Error(`Expected fatal missing-report problem:\n${JSON.stringify(item, null, 2)}`);
  }
  requireText(item, ["missing", "runtime report"], "missing diagnostics item");
});

When("I load diagnostics for a malformed runtime report copy", function () {
  runCase(this, `
target = STAGING_DIR / "reports" / "malformed-runtime-load-report.json"
target.parent.mkdir(parents=True, exist_ok=True)
target.write_text("{ nope", encoding="utf-8")
emit_repository_case("malformed report", target, [target])
`);
});

Then("diagnostics marks the report as malformed with parse failure details", function () {
  const item = resultItem(this);
  if (item.status !== "malformed") {
    throw new Error(`Expected malformed status, got ${item.status}:\n${JSON.stringify(item, null, 2)}`);
  }
  requireProblemCode(item, "BML_RUNTIME_REPORT_PARSE_FAILED");
  const parseProblem = problemsOf(item).find(problem => problem.code === "BML_RUNTIME_REPORT_PARSE_FAILED");
  const details = parseProblem && parseProblem.details ? parseProblem.details : {};
  if (typeof details.line !== "number" || typeof details.column !== "number") {
    throw new Error(`Expected line/column parse details:\n${JSON.stringify(item, null, 2)}`);
  }
  requireText(item, ["malformed", "json"], "malformed diagnostics item");
});

When("I load diagnostics for the loaded runtime report fixture", function () {
  runCase(this, `
target = STAGING_DIR / "reports" / "runtime-load-report.loaded.json"
target.parent.mkdir(parents=True, exist_ok=True)
shutil.copyfile(RUNTIME_REPORT_LOADED, target)
emit_repository_case("loaded report", target, [target])
`);
});

Then("diagnostics summarizes the loaded runtime report with loaded status and Runebound module evidence", function () {
  const item = resultItem(this);
  if (item.status !== "loaded") {
    throw new Error(`Expected loaded status, got ${item.status}:\n${JSON.stringify(item, null, 2)}`);
  }
  if (!item.runtime || item.runtime.id !== "barony-bml-hook") {
    throw new Error(`Expected runtime id barony-bml-hook:\n${JSON.stringify(item, null, 2)}`);
  }
  const runebound = (Array.isArray(item.loadedMods) ? item.loadedMods : []).find(mod => mod.id === "jml.runebound-elixirs");
  if (!runebound || runebound.status !== "loaded" || !Array.isArray(runebound.modules) || !runebound.modules.includes("runeboundElixirs")) {
    throw new Error(`Expected loaded Runebound module evidence:\n${JSON.stringify(item, null, 2)}`);
  }
  requireText(item, ["fake-provider", "runtime evidence"], "loaded fixture evidence classification");
});

When("I load diagnostics for a runtime report with a missing native symbol", function () {
  runCase(this, `
payload = loaded_fixture_payload()
payload["runtime"]["symbolProbe"] = {
    "status": "missing_symbols",
    "ready": False,
    "missingSymbols": ["bml_runebound_elixir_apply_effect"],
    "blockedTargets": ["runeboundElixirs.applyEffect"],
}
payload["diagnostics"] = {
    "nativeSymbolReadiness": {
        "status": "degraded",
        "missing": ["bml_runebound_elixir_apply_effect"],
    }
}
target = write_report("runtime-load-report.missing-symbol.json", payload)
emit_repository_case("missing symbol", target, [target])
`);
});

Then("diagnostics degrades the report without claiming loaded runtime readiness", function () {
  const item = resultItem(this);
  const text = textOf(item);
  const status = String(item.status || "").toLowerCase();
  if (!text.includes("missing") || !text.includes("symbol")) {
    throw new Error(`Expected missing-symbol diagnostics to be surfaced semantically:\n${JSON.stringify(item, null, 2)}`);
  }
  if (!["degraded", "blocked", "invalid"].includes(status)) {
    throw new Error(`Expected degraded/blocked/invalid status for missing symbols, got ${item.status}:\n${JSON.stringify(item, null, 2)}`);
  }
  if (status === "loaded" || /"ready"\s*:\s*true/.test(text)) {
    throw new Error(`Missing-symbol report must not claim loaded runtime readiness:\n${JSON.stringify(item, null, 2)}`);
  }
});

When("I load diagnostics for a stale runtime report copy", function () {
  runCase(this, `
payload = loaded_fixture_payload()
payload["reportedAt"] = "2020-01-01T00:00:00Z"
payload["runtime"]["manifestGeneratedAt"] = "2026-07-06T00:00:00Z"
payload["appCoreSession"] = {
    "startedAt": "2026-07-06T00:00:00Z",
    "expectedRuntimeReportFreshAfter": "2026-07-06T00:00:00Z",
}
target = write_report("runtime-load-report.stale.json", payload)
emit_repository_case("stale report", target, [target])
`);
});

Then("diagnostics flags the report as stale against the current app-core session", function () {
  const item = resultItem(this);
  const text = textOf(item);
  const codes = problemCodes(item).join(" ").toLowerCase();
  if (!text.includes("stale") && !codes.includes("stale")) {
    throw new Error(`Expected stale report diagnostics against current session:\n${JSON.stringify(item, null, 2)}`);
  }
  if (String(item.status || "").toLowerCase() === "loaded") {
    throw new Error(`Stale report must not remain a loaded diagnostics item:\n${JSON.stringify(item, null, 2)}`);
  }
});

When("I load diagnostics for a production-evidence runtime report copy", function () {
  runCase(this, `
payload = loaded_fixture_payload()
payload["runtime"]["evidenceKind"] = "steam-linux-live-gameplay"
payload["runtime"]["hostOs"] = "linux"
payload["runtime"]["gameExecutableName"] = "barony.x86_64"
payload["runtime"]["launchEvidence"] = {
    "kind": "real-runtime-load-report",
    "source": "copied-fixture",
    "requiresLiveGameLaunch": False,
}
for loaded_mod in payload.get("loadedMods", []):
    if loaded_mod.get("id") == "jml.runebound-elixirs":
        loaded_mod.pop("evidenceScope", None)
        loaded_mod["liveHookBehaviorClaimed"] = True
        loaded_mod["productionEvidence"] = {
            "kind": "steam-linux-live-gameplay",
            "artifact": "real-runtime-load-report",
            "hostOs": "linux",
        }
target = write_report("runtime-load-report.production.json", payload)
emit_repository_case("production report", target, [target])
`);
});

Then("diagnostics parses production evidence without requiring a live game launch", function () {
  const item = resultItem(this);
  if (item.status !== "loaded") {
    throw new Error(`Expected production copy to parse as a loaded report, got ${item.status}:\n${JSON.stringify(item, null, 2)}`);
  }
  requireText(item, ["production", "steam-linux-live-gameplay", "real-runtime-load-report"], "production evidence classification");
  const text = textOf(item);
  if (text.includes("fake-provider")) {
    throw new Error(`Production diagnostics must not be classified as fake-provider evidence:\n${JSON.stringify(item, null, 2)}`);
  }
  if (problemCodes(item).some(code => code === "BML_RUNTIME_REPORT_RUNEBOUND_ELIXIRS_EVIDENCE_SCOPE_INVALID")) {
    throw new Error(`Production evidence copy must not require fake-provider evidenceScope:\n${JSON.stringify(item, null, 2)}`);
  }
});
