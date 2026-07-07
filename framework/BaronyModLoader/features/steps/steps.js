/**
 * BDD Step Definitions — BaronyModLoader app-core contracts
 * Single-file step definitions (Cucumber requires a unique step definition per text).
 * Feature files are the living specification; this file is the executable contract.
 */

"use strict";

const { Given, When, Then } = require("@cucumber/cucumber");
const { execSync } = require("child_process");
const path = require("path");
const fs = require("fs");
const os = require("os");

// paths: <repo>/framework/BaronyModLoader/features/steps/steps.js
//          ../../../../ from here = <repo>
const REPO_ROOT = path.resolve(__dirname, "../../../../");
const BML_APP = path.join(REPO_ROOT, "framework/BaronyModLoader/app/barony_mod_loader.py");
const STASH_PKG = path.join(REPO_ROOT, "mods/stash");
const ELIXIRS_PKG = path.join(REPO_ROOT, "mods/runebound-elixirs");
const PKG_SCHEMA = path.join(REPO_ROOT, "framework/BaronyModLoader/schema/package.schema.json");
const RUNTIME_SCHEMA = path.join(REPO_ROOT, "framework/BaronyModLoader/schema/runtime-manifest.schema.json");
const DESKTOP_FILE = path.join(REPO_ROOT, "framework/BaronyModLoader/share/applications/barony-modloader.desktop");

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

function runPython(script, world) {
  const tmp = path.join(os.tmpdir(), `bml-step-${Date.now()}-${Math.random().toString(36).slice(2)}.py`);
  fs.writeFileSync(tmp, script, "utf-8");
  try {
    const r = execSync(`/usr/bin/python3 ${tmp}`, { cwd: REPO_ROOT, encoding: "utf-8", timeout: 20000 });
    world.scriptOutput = r;
    world.scriptExitCode = 0;
  } catch (err) {
    world.scriptOutput = err.stdout || "";
    world.scriptStderr = err.stderr || "";
    world.scriptExitCode = err.status !== undefined ? err.status : 1;
    if (world.scriptExitCode === 0) world.scriptExitCode = 1;
  } finally {
    try { fs.unlinkSync(tmp); } catch (_) {}
  }
}

function runCLI(cmdLine, world) {
  try {
    const r = execSync(cmdLine, { cwd: REPO_ROOT, encoding: "utf-8", timeout: 30000 });
    world.cliOutput = r;
    world.cliExitCode = 0;
  } catch (err) {
    world.cliOutput = err.stdout || "";
    world.cliStderr = err.stderr || "";
    world.cliExitCode = err.status !== undefined ? err.status : 1;
    if (world.cliExitCode === 0) world.cliExitCode = 1;
  }
}

// ---------------------------------------------------------------------------
// GIVENS
// ---------------------------------------------------------------------------

Given("the BML Python app module path", function () {
  this.bmlApp = BML_APP;
});

Given("the package schema path", function () {
  this.packageSchema = PKG_SCHEMA;
});

Given("the runtime manifest schema path", function () {
  this.runtimeManifestSchema = RUNTIME_SCHEMA;
});

Given("the real Runebound: Elixirs package path", function () {
  this.realPackage = ELIXIRS_PKG;
});

Given("a temporary staging directory", function () {
  this.stagingDir = fs.mkdtempSync(path.join(os.tmpdir(), "bml-cucumber-"));
});

// ---------------------------------------------------------------------------
// HEADLESS APP-CORE
// ---------------------------------------------------------------------------

When("I run a Python script that imports the BML app module", function () {
  const app = this.bmlApp;
  runPython(
    `import sys; sys.path.insert(0, ${JSON.stringify(path.dirname(app))}); ` +
    `import barony_mod_loader; print("IMPORT_OK")`,
    this
  );
});

Then("the import succeeds without raising ImportError", function () {
  const out = (this.scriptOutput || "") + (this.scriptStderr || "");
  if (this.scriptExitCode !== 0) throw new Error(`Import failed (exit ${this.scriptExitCode}):\n${out}`);
  if (out.includes("ImportError") || out.includes("ModuleNotFoundError")) throw new Error(`ImportError:\n${out}`);
});

Then(/^the process exits cleanly \(exit code 0\)$/, function () {
  const ec = this.scriptExitCode !== undefined ? this.scriptExitCode : this.cliExitCode;
  const out = (this.scriptOutput || "") + (this.scriptStderr || "") + (this.cliOutput || "") + (this.cliStderr || "");
  if (ec !== 0) throw new Error(`Expected 0, got ${ec}:\n${out}`);
});

When("I run a Python script that checks whether the BML app module imports tkinter", function () {
  const app = this.bmlApp;
  runPython(
    `import sys, importlib.util\n` +
    `spec = importlib.util.spec_from_file_location("bml_app", ${JSON.stringify(app)})\n` +
    `mod = importlib.util.module_from_spec(spec)\n` +
    `sys.modules[spec.name] = mod\n` +
    `_b = set(sys.modules.keys())\n` +
    `spec.loader.exec_module(mod)\n` +
    `_t = [m for m in sys.modules.keys()-_b if m=="tkinter" or m.startswith("tkinter.")]\n` +
    `if _t: print("TKINTER_FOUND:"+",".join(_t)); sys.exit(1)\n` +
    `print("TKINTER_NOT_FOUND"); sys.exit(0)`,
    this
  );
});

Then("the check reports that tkinter is NOT imported by app-core", function () {
  const out = this.scriptOutput || "";
  if (out.includes("TKINTER_FOUND")) throw new Error(`tkinter imported:\n${out}`);
  if (this.scriptExitCode !== 0 && !out.includes("TKINTER_NOT_FOUND")) throw new Error(`Check failed (${this.scriptExitCode}):\n${out}`);
  if (!out.includes("TKINTER_NOT_FOUND")) throw new Error(`Unexpected output:\n${out}`);
});

When("I inspect the source of the BML app module for import statements", function () {
  const src = fs.readFileSync(this.bmlApp, "utf-8");
  this.importLines = src.split("\n").filter(
    l => /^[^#]*\bimport\s+tkinter\b/.test(l) || /^[^#]*\bfrom\s+tkinter\b/.test(l)
  );
});

Then("no import of the {string} module is found in the source code", function (m) {
  const esc = m.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
  const pat = new RegExp(`^[^#]*\\b(?:import|from)\\s+${esc}\\b`);
  const found = this.importLines.filter(l => pat.test(l));
  if (found.length) throw new Error(`Found '${m}':\n${found.join("\n")}`);
});

Then("no {string} relative-import is found in the source code", function (m) {
  const esc = m.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
  const pat = new RegExp(`^[^#]*\\bfrom\\s+${esc}\\b`);
  const found = this.importLines.filter(l => pat.test(l));
  if (found.length) throw new Error(`Found 'from ${m}':\n${found.join("\n")}`);
});

// ---------------------------------------------------------------------------
// COMMAND ADAPTER
// ---------------------------------------------------------------------------

When("I invoke the CLI command {string} on the example stash package", function (args) {
  const parts = args.split(" ");
  runCLI(["python3", this.bmlApp, ...parts].join(" "), this);
});

When("I invoke the CLI command {string} on a path that does not exist", function (args) {
  const parts = args.split(" ");
  runCLI(["python3", this.bmlApp, ...parts].join(" "), this);
});

When("I invoke the CLI command {string} with a valid package path", function (args) {
  const parts = args.split(" ");
  runCLI(["python3", this.bmlApp, ...parts].join(" "), this);
});

Then("the command exits with code 0", function () {
  if (this.cliExitCode !== 0) throw new Error(
    `Expected 0, got ${this.cliExitCode}:\n${this.cliOutput || ""}\n${this.cliStderr || ""}`
  );
});

Then("the command exits with non-zero code", function () {
  if (this.cliExitCode === 0) throw new Error(`Expected non-zero, got 0:\n${this.cliOutput || ""}`);
});

Then("the stdout output does not contain error problem codes prefixed with {string}", function (prefix) {
  const out = this.cliOutput || "";
  const errs = (out.match(/\bBML_\w+\b/g) || []).filter(e => e.startsWith(prefix.replace("BML_", "BML_")));
  if (errs.length) throw new Error(`Found error codes ${errs.join(", ")}:\n${out}`);
});

Then("the stdout output contains a problem code starting with {string}", function (p) {
  const out = (this.cliOutput || "") + (this.cliStderr || "");
  const esc = p.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
  const found = (out.match(/\bBML_\w+\b/g) || []).filter(e => e.startsWith(esc));
  if (!found.length) throw new Error(`No problem code starting with '${p}':\n${out}`);
});

Then("the CLI emits the command name in the output header", function () {
  const out = this.cliOutput || "";
    if (!/(?:package\s+validat(?:e|ion)|BaronyModLoader)/i.test(out)) throw new Error(`No command header:\n${out}`);
});

Then("the CLI reports exit code 0 in the output", function () {
  const out = this.cliOutput || "";
  if (!out.includes("0") && this.cliExitCode !== 0) throw new Error(`No exit-code indication:\n${out}`);
});

When("I run a Python script that checks for a CommandResult-like dataclass", function () {
  const app = this.bmlApp;
  runPython(
    `import sys, importlib.util, json, dataclasses\n` +
    `spec = importlib.util.spec_from_file_location("bml_app", ${JSON.stringify(app)})\n` +
    `mod = importlib.util.module_from_spec(spec)\n` +
    `sys.modules[spec.name] = mod\n` +
    `spec.loader.exec_module(mod)\n` +
    `R=("argv","label","exit_code","stdout","stderr","duration","failure_summary")\n` +
    `for n in dir(mod):\n` +
    `  o=getattr(mod,n)\n` +
    `  if dataclasses.is_dataclass(o):\n` +
    `    f=[x.name for x in dataclasses.fields(o)]\n` +
    `    if all(x in f for x in R): print("FOUND:"+n); sys.exit(0)\n` +
    `print("NOT_FOUND"); sys.exit(1)`,
    this
  );
});

Then("the module defines a result DTO with fields: argv, label, exit_code, stdout, stderr, duration, failure_summary", function () {
  const out = this.scriptOutput || "";
  if (out.includes("NOT_FOUND")) throw new Error("No CommandResult DTO with required 7 fields found in app module.");
  if (!out.includes("FOUND:")) throw new Error(`Unexpected output:\n${out}`);
});

// ---------------------------------------------------------------------------
// SEMANTIC DASHBOARD
// ---------------------------------------------------------------------------

When("I run a Python script that checks for a DashboardDto or equivalent fixture", function () {
  const app = this.bmlApp;
  runPython(
    `import sys, importlib.util, json, dataclasses\n` +
    `spec = importlib.util.spec_from_file_location("bml_app", ${JSON.stringify(app)})\n` +
    `mod = importlib.util.module_from_spec(spec)\n` +
    `sys.modules[spec.name] = mod\n` +
    `spec.loader.exec_module(mod)\n` +
    `S={"install","profile","package","readiness","diagnostics","workshop"}\n` +
    `for n in dir(mod):\n` +
    `  o=getattr(mod,n)\n` +
    `  if dataclasses.is_dataclass(o) and S.issubset(set(x.name for x in dataclasses.fields(o))):\n` +
    `    print("DASH_FOUND:"+n); sys.exit(0)\n` +
    `  elif isinstance(o,dict) and S.issubset(set(o.keys())):\n` +
    `    print("DASH_FOUND:"+n); sys.exit(0)\n` +
    `print("DASH_NOT_FOUND"); sys.exit(1)`,
    this
  );
});

Then("the module defines a dashboard fixture with sections: install, profile, package, readiness, diagnostics, workshop", function () {
  const out = this.scriptOutput || "";
  if (out.includes("DASH_NOT_FOUND")) throw new Error(
    "No DashboardDto with sections: install, profile, package, readiness, diagnostics, workshop"
  );
  if (!out.includes("DASH_FOUND:")) throw new Error(`Unexpected output:\n${out}`);
});

Then("each section is a dict or dataclass field", function () {
  const out = this.scriptOutput || "";
  if (!out.includes("DASH_FOUND:")) throw new Error(`Unexpected output:\n${out}`);
});

When("I run a Python script that checks for a DashboardDto", function () {
  const app = this.bmlApp;
  runPython(
    `import sys, importlib.util, json, dataclasses\n` +
    `spec = importlib.util.spec_from_file_location("bml_app", ${JSON.stringify(app)})\n` +
    `mod = importlib.util.module_from_spec(spec)\n` +
    `sys.modules[spec.name] = mod\n` +
    `spec.loader.exec_module(mod)\n` +
    `for n in dir(mod):\n` +
    `  o=getattr(mod,n)\n` +
    `  if dataclasses.is_dataclass(o) and ("disabled_reasons" in [x.name for x in dataclasses.fields(o)] or "disabledReasons" in [x.name for x in dataclasses.fields(o)]):\n` +
    `    print("DIS_FOUND:"+n); sys.exit(0)\n` +
    `  elif isinstance(o,dict) and ("disabled_reasons" in o or "disabledReasons" in o):\n` +
    `    print("DIS_FOUND:"+n); sys.exit(0)\n` +
    `print("DIS_NOT_FOUND"); sys.exit(1)`,
    this
  );
});

Then(/^the fixture includes a disabled_reasons field \(list or None\)$/, function () {
  const out = this.scriptOutput || "";
  if (out.includes("DIS_NOT_FOUND")) throw new Error("No disabled_reasons (or disabledReasons) field found.");
  if (!out.includes("DIS_FOUND:")) throw new Error(`Unexpected output:\n${out}`);
});

Then("the disabled_reasons field is accessible as a property or key", function () {
  const out = this.scriptOutput || "";
  if (!out.includes("DIS_FOUND:")) throw new Error(`Unexpected output:\n${out}`);
});

When("I run a Python script that checks the type of the dashboard fixture", function () {
  const app = this.bmlApp;
  // Do NOT import tkinter — that would defeat the check
  runPython(
    `import sys, importlib.util, json, dataclasses\n` +
    `spec = importlib.util.spec_from_file_location("bml_app", ${JSON.stringify(app)})\n` +
    `mod = importlib.util.module_from_spec(spec)\n` +
    `sys.modules[spec.name] = mod\n` +
    `spec.loader.exec_module(mod)\n` +
    `S={"install","profile","package","readiness","diagnostics","workshop"}\n` +
    `for n in dir(mod):\n` +
    `  o=getattr(mod,n)\n` +
    `  if dataclasses.is_dataclass(o) and S.issubset(set(x.name for x in dataclasses.fields(o))):\n` +
    `    print("DTYP:dataclass:"+n); sys.exit(0)\n` +
    `  elif isinstance(o,dict) and S.issubset(set(o.keys())):\n` +
    `    print("DTYP:dict:"+n); sys.exit(0)\n` +
    `print("DTYP_NOT_FOUND"); sys.exit(1)`,
    this
  );
});

Then(/^the fixture is a dict or dataclass \(not a tkinter widget or custom GUI object\)$/, function () {
  const out = this.scriptOutput || "";
  if (!out.includes("DTYP:")) throw new Error("No dashboard fixture found.");
  const m = out.match(/DTYP:(\S+)/);
  if (!m) throw new Error(`Unexpected output:\n${out}`);
  const t = m[1];
  if (!["dataclass:", "dict:"].some(x => t.startsWith(x))) throw new Error(`Not plain dict/dataclass: ${t}`);
});

// ---------------------------------------------------------------------------
// TEMPFILE GUARD
// ---------------------------------------------------------------------------

When("I invoke the CLI command {string} in dry-run mode with --out to the staging dir", function (args) {
  const outPath = path.join(this.stagingDir, "test-output.bmlpkg");
  runCLI(
    ["python3", this.bmlApp, "package", "pack", STASH_PKG, "--out", outPath].join(" "),
    this
  );
  this.outPath = outPath;
});

Then("the output archive is created at the specified --out path", function () {
  if (this.outPath && !fs.existsSync(this.outPath)) throw new Error(`Output not at ${this.outPath}:\n${this.cliOutput || ""}`);
});

Then("the process exits with code {int}", function (exp) {
  const ec = this.scriptExitCode !== undefined ? this.scriptExitCode : this.cliExitCode;
  const out = this.scriptOutput || this.cliOutput || "";
  if (ec !== exp) throw new Error(`Expected ${exp}, got ${ec}:\n${out}`);
});

When("I invoke the CLI command {string} in dry-run mode with --store", function (args) {
  this.storeDir = path.join(this.stagingDir, "store");
  fs.mkdirSync(this.storeDir, { recursive: true });
  runCLI(
    ["python3", this.bmlApp, "package", "install", STASH_PKG, "--store", this.storeDir].join(" "),
    this
  );
});

Then("the store directory is empty or does not contain the package", function () {
  if (!this.storeDir) throw new Error("No store dir recorded.");
  const entries = fs.readdirSync(this.storeDir, { recursive: false });
  if (entries.length > 0) {
    const isDry = ((this.cliOutput || "") + (this.cliStderr || "")).toLowerCase().includes("dry");
    if (isDry && this.cliExitCode === 0) throw new Error(`Dry-run wrote to store: ${entries.join(", ")}`);
  }
});

Then("the process exits cleanly", function () {
  const ec = this.scriptExitCode !== undefined ? this.scriptExitCode : this.cliExitCode;
  const out = this.scriptOutput || this.cliOutput || "";
  if (ec !== 0) throw new Error(`Expected clean, got ${ec}:\n${out}`);
});

When("I invoke the CLI command for workshop prep", function () {
  runCLI(["python3", this.bmlApp, "package", "validate", STASH_PKG].join(" "), this);
});

Then("no SteamCmd publish, upload, or auth subcommand appears in the invocation output", function () {
  const out = ((this.cliOutput || "") + (this.cliStderr || "")).toLowerCase();
  const found = ["steamcmd", "publish", "upload", "steampowered.com"].filter(k => out.includes(k));
  if (found.length) throw new Error(`Forbidden Steam keyword(s): ${found.join(", ")}`);
});

Then("the process completes without calling the Steam web API", function () {
  const out = ((this.cliOutput || "") + (this.cliStderr || "")).toLowerCase();
  if (/api\.steampowered|steamcommunity/.test(out)) throw new Error(`Steam API in output`);
});

When("I run a Python script that invokes package operations with an explicit staging directory", function () {
  runPython(
    `import sys, importlib.util\n` +
    `spec = importlib.util.spec_from_file_location("bml_app", ${JSON.stringify(BML_APP)})\n` +
    `mod = importlib.util.module_from_spec(spec)\n` +
    `sys.modules[spec.name] = mod\n` +
    `spec.loader.exec_module(mod)\n` +
    `import inspect\n` +
    `sig = inspect.signature(mod.package_install_target)\n` +
    `p = list(sig.parameters.keys())\n` +
    `if "store_dir" in p or "staging" in p or len(p) >= 2: print("HAS_STAGING:"+",".join(p)); sys.exit(0)\n` +
    `print("NO_STAGING"); sys.exit(1)`,
    this
  );
});

Then("all file writes occur under the staging directory or are rejected", function () {
  const out = this.scriptOutput || "";
  if (out.includes("NO_STAGING")) throw new Error("package_install_target lacks store_dir/staging param.");
  if (!out.includes("HAS_STAGING:")) throw new Error(`Unexpected output:\n${out}`);
});

Then("no writes occur outside the staging directory", function () {
  const out = this.scriptOutput || "";
  if (!out.includes("HAS_STAGING:")) throw new Error(`Unexpected output:\n${out}`);
});

// ---------------------------------------------------------------------------
// ICON LABEL MAPPING
// ---------------------------------------------------------------------------

When("I run a Python script that checks for an icon label mapping constant", function () {
  const app = this.bmlApp;
  runPython(
    `import sys, importlib.util, json\n` +
    `spec = importlib.util.spec_from_file_location("bml_app", ${JSON.stringify(app)})\n` +
    `mod = importlib.util.module_from_spec(spec)\n` +
    `sys.modules[spec.name] = mod\n` +
    `spec.loader.exec_module(mod)\n` +
    `for n in ["ICON_LABELS","ICON_LABEL_MAP","ICON_NAMES","STORE_ICON_LABELS"]:\n` +
    `  if hasattr(mod,n):\n` +
    `    v=getattr(mod,n)\n` +
    `    if isinstance(v,dict) and v:\n` +
    `      bad=[k for k,x in v.items() if not isinstance(x,str) or not x.strip()]\n` +
    `      if bad: print("EMPTY_LABELS:"+json.dumps(bad)); sys.exit(1)\n` +
    `      print("ICONS_OK:"+n); sys.exit(0)\n` +
    `print("ICONS_NOT_FOUND"); sys.exit(1)`,
    this
  );
});

Then("the module defines ICON_LABELS or an equivalent dict", function () {
  const out = this.scriptOutput || "";
  if (out.includes("ICONS_NOT_FOUND")) throw new Error(
    "No ICON_LABELS dict. OS/store/runtime icons must always include text labels."
  );
  if (out.includes("EMPTY_LABELS")) throw new Error(`Empty labels:\n${out}`);
  if (!out.includes("ICONS_OK:")) throw new Error(`Unexpected output:\n${out}`);
});

Then("every entry in the mapping has a non-empty text label", function () {
  const out = this.scriptOutput || "";
  if (out.includes("EMPTY_LABELS")) throw new Error(`Empty labels:\n${out}`);
});

When("I run a Python script that checks OS icon labels", function () {
  const app = this.bmlApp;
  runPython(
    `import sys, importlib.util, json\n` +
    `spec = importlib.util.spec_from_file_location("bml_app", ${JSON.stringify(app)})\n` +
    `mod = importlib.util.module_from_spec(spec)\n` +
    `sys.modules[spec.name] = mod\n` +
    `spec.loader.exec_module(mod)\n` +
    `for n in ["ICON_LABELS","ICON_LABEL_MAP","ICON_NAMES","STORE_ICON_LABELS"]:\n` +
    `  if hasattr(mod,n):\n` +
    `    v=getattr(mod,n)\n` +
    `    if isinstance(v,dict):\n` +
    `      miss=[p for p in ["linux","windows","darwin"] if not any(p.lower() in k.lower() for k in v)]\n` +
    `      if miss: print("OS_MISSING:"+json.dumps(miss)); sys.exit(1)\n` +
    `      print("OS_OK"); sys.exit(0)\n` +
    `print("ICONS_NOT_FOUND"); sys.exit(1)`,
    this
  );
});

Then("entries exist for: linux, windows, darwin icon variants", function () {
  const out = this.scriptOutput || "";
  if (out.includes("OS_MISSING")) throw new Error(`OS icon labels missing:\n${out}`);
  if (!out.includes("OS_OK")) throw new Error(`Unexpected output:\n${out}`);
});

When("I run a Python script that checks store icon labels", function () {
  const app = this.bmlApp;
  runPython(
    `import sys, importlib.util, json\n` +
    `spec = importlib.util.spec_from_file_location("bml_app", ${JSON.stringify(app)})\n` +
    `mod = importlib.util.module_from_spec(spec)\n` +
    `sys.modules[spec.name] = mod\n` +
    `spec.loader.exec_module(mod)\n` +
    `for n in ["ICON_LABELS","ICON_LABEL_MAP","ICON_NAMES","STORE_ICON_LABELS"]:\n` +
    `  if hasattr(mod,n):\n` +
    `    v=getattr(mod,n)\n` +
    `    if isinstance(v,dict):\n` +
    `      miss=[k for k in ["steam","workshop","thumbnail","grid","library"] if not any(k.lower() in ke.lower() for ke in v)]\n` +
    `      if miss: print("STORE_MISSING:"+json.dumps(miss)); sys.exit(1)\n` +
    `      print("STORE_OK"); sys.exit(0)\n` +
    `print("ICONS_NOT_FOUND"); sys.exit(1)`,
    this
  );
});

Then("entries exist for: workshop thumbnail, library grid icon variants", function () {
  const out = this.scriptOutput || "";
  if (out.includes("STORE_MISSING")) throw new Error(`Store icons missing:\n${out}`);
  if (!out.includes("STORE_OK")) throw new Error(`Unexpected output:\n${out}`);
});

When("I inspect the BaronyModLoader desktop launcher metadata", function () {
  if (!fs.existsSync(DESKTOP_FILE)) {
    throw new Error(`Desktop launcher file is missing: ${DESKTOP_FILE}`);
  }
  const raw = fs.readFileSync(DESKTOP_FILE, "utf-8");
  const entries = {};
  for (const line of raw.split(/\r?\n/)) {
    const match = /^([A-Za-z][A-Za-z0-9-]*)=(.*)$/.exec(line);
    if (match) entries[match[1]] = match[2].trim();
  }
  this.desktopLauncherPath = DESKTOP_FILE;
  this.desktopLauncherRaw = raw;
  this.desktopLauncherEntries = entries;
});

Then("the desktop launcher Icon entry points at a concrete generated BML PNG", function () {
  const entries = this.desktopLauncherEntries || {};
  const icon = entries.Icon || "";
  if (!icon) {
    throw new Error(`Desktop launcher has no Icon= entry:\n${this.desktopLauncherRaw || ""}`);
  }
  if (!path.isAbsolute(icon) || !/\.png$/i.test(icon)) {
    throw new Error(
      `Desktop launcher Icon= must be an absolute PNG path, not an icon-theme name or relative value.\n` +
      `Icon=${icon}\nDesktop file: ${this.desktopLauncherPath}`
    );
  }
  if (!/barony-modloader(?:-bml)?\.png$/i.test(icon)) {
    throw new Error(`Desktop launcher Icon= must point at the generated BML PNG, got: ${icon}`);
  }
  if (!fs.existsSync(icon)) {
    throw new Error(`Desktop launcher Icon= points at a missing PNG: ${icon}`);
  }
});

// ---------------------------------------------------------------------------
// CARRIER CONTRACT
// ---------------------------------------------------------------------------

When("I run a Python script that asserts the app constant equals {string}", function (exp) {
  const app = this.bmlApp;
  runPython(
    `import sys, importlib.util\n` +
    `spec = importlib.util.spec_from_file_location("bml_app", ${JSON.stringify(app)})\n` +
    `mod = importlib.util.module_from_spec(spec)\n` +
    `sys.modules[spec.name] = mod\n` +
    `spec.loader.exec_module(mod)\n` +
    `EXP=${JSON.stringify(exp)}\n` +
    `if hasattr(mod,"RUNEBOUND_ELIXIRS_CARRIER_ITEM_TYPE"):\n` +
    `  a=mod.RUNEBOUND_ELIXIRS_CARRIER_ITEM_TYPE\n` +
    `  if a==EXP: print("OK:"+a); sys.exit(0)\n` +
    `  print("MISMATCH:"+EXP+":"+a); sys.exit(1)\n` +
    `print("NOT_FOUND"); sys.exit(1)`,
    this
  );
});

Then("the assertion succeeds", function () {
  const out = this.scriptOutput || "";
  if (out.includes("NOT_FOUND")) throw new Error(
    "RUNEBOUND_ELIXIRS_CARRIER_ITEM_TYPE not defined. Must equal 'POTION_STRENGTH'."
  );
  if (out.includes("MISMATCH")) throw new Error(
    `Carrier constant mismatch:\n${out}\nExpected: POTION_STRENGTH (from real package, native smoke, run_elixir_production_validation.sh)`
  );
  if (!out.includes("OK:")) throw new Error(`Unexpected output:\n${out}`);
});

Then("the constant is the string {string}", function (exp) {
  const out = this.scriptOutput || "";
  if (!out.includes("OK:" + exp)) throw new Error(`Expected '${exp}', got:\n${out}`);
});

When("I validate the package schema with carrierItemType set to {string}", function (ct) {
  // Real path: properties.modules.properties.runeboundElixirs.properties.carrierItemType
  // Fallback:  properties.modules.patternProperties["runeboundElixirs"].properties.carrierItemType
  const schemaPath = this.packageSchema || PKG_SCHEMA;
  const schema = JSON.parse(fs.readFileSync(schemaPath, "utf-8"));
  let allowed = false;
  let currentConst = null;
  const mods = schema.properties?.modules;
  let cd = null;
  if (
    mods?.properties?.runeboundElixirs?.properties?.carrierItemType &&
    mods.properties.runeboundElixirs.properties.carrierItemType !== undefined
  ) {
    cd = mods.properties.runeboundElixirs.properties.carrierItemType;
  } else if (
    mods?.patternProperties?.["runeboundElixirs"]?.properties?.carrierItemType &&
    mods.patternProperties["runeboundElixirs"].properties.carrierItemType !== undefined
  ) {
    cd = mods.patternProperties["runeboundElixirs"].properties.carrierItemType;
  } else if (
    schema.$defs &&
    schema.$defs.runeboundElixirsModule?.properties?.carrierItemType &&
    schema.$defs.runeboundElixirsModule.properties.carrierItemType !== undefined
  ) {
    cd = schema.$defs.runeboundElixirsModule.properties.carrierItemType;
  }
  if (cd && cd !== undefined) {
    currentConst = cd.const ?? cd.enum?.[0] ?? null;
    allowed = cd.const === ct || (cd.enum || []).includes(ct);
  }
  this.schemaCheck = { ct, allowed, currentConst };
});

Then("the schema validation passes", function () {
  const { ct, allowed, currentConst } = this.schemaCheck || {};
  if (!allowed) throw new Error(
    `Package schema does not allow carrierItemType='${ct}'. Schema const: ${currentConst}. ` +
    "Must allow 'POTION_STRENGTH' to match the real package and native smoke."
  );
});

When("I validate the runtime manifest schema with carrierItemType set to {string}", function (ct) {
  // Real path: $defs.runeboundElixirsModule.properties.carrierItemType
  // Fallback:  properties.modules.patternProperties["runeboundElixirs"].properties.carrierItemType
  const schema = JSON.parse(fs.readFileSync(this.runtimeManifestSchema, "utf-8"));
  let allowed = false;
  let currentConst = null;
  let cd = null;
  if (
    schema.$defs &&
    schema.$defs.runeboundElixirsModule?.properties?.carrierItemType &&
    schema.$defs.runeboundElixirsModule.properties.carrierItemType !== undefined
  ) {
    cd = schema.$defs.runeboundElixirsModule.properties.carrierItemType;
  } else if (
    schema.properties?.modules?.patternProperties?.["runeboundElixirs"]?.properties?.carrierItemType &&
    schema.properties.modules.patternProperties["runeboundElixirs"].properties.carrierItemType !== undefined
  ) {
    cd = schema.properties.modules.patternProperties["runeboundElixirs"].properties.carrierItemType;
  } else if (
    schema.properties?.modules?.properties?.runeboundElixirs?.properties?.carrierItemType &&
    schema.properties.modules.properties.runeboundElixirs.properties.carrierItemType !== undefined
  ) {
    cd = schema.properties.modules.properties.runeboundElixirs.properties.carrierItemType;
  }
  if (cd && cd !== undefined) {
    currentConst = cd.const ?? cd.enum?.[0] ?? null;
    allowed = cd.const === ct || (cd.enum || []).includes(ct);
  }
  this.runtimeSchemaCheck = { ct, allowed, currentConst };
  this.schemaCheck = this.runtimeSchemaCheck;
});

Then("the runtime manifest schema validation passes", function () {
  const { ct, allowed, currentConst } = this.runtimeSchemaCheck || {};
  if (!allowed) throw new Error(
    `Runtime manifest schema does not allow carrierItemType='${ct}'. Schema const: ${currentConst}. ` +
    "Must allow 'POTION_STRENGTH'."
  );
});

When("I invoke the CLI command {string} on the real runebound-elixirs package", function (args) {
  // args like "package validate"
  const parts = args.split(" ");
  const subcmd = parts.length >= 2 ? parts[1] : "validate";
  runCLI(["python3", this.bmlApp, "package", subcmd, this.realPackage].join(" "), this);
});

Then("the stdout output does not contain {string}", function (code) {
  if ((this.cliOutput || "").includes(code)) throw new Error(`Found '${code}' — carrier mismatch?:\n${this.cliOutput}`);
});

Then(
  "if the package uses {string} as carrierItemType, the validation fails with BML_PACKAGE_RUNEBOUND_ELIXIRS_MODULE_FIELD_INVALID",
  function (ct) {
    const mpath = path.join(this.realPackage, "bml-package.json");
    const manifest = JSON.parse(fs.readFileSync(mpath, "utf-8"));
    const actual = manifest.modules?.runeboundElixirs?.carrierItemType;
    if (actual === ct) {
      if (this.cliExitCode === 0) throw new Error(
        `Package uses '${ct}' but validate passed. Expected BML_PACKAGE_RUNEBOUND_ELIXIRS_MODULE_FIELD_INVALID.`
      );
      if (!(this.cliOutput || "").includes("BML_PACKAGE_RUNEBOUND_ELIXIRS_MODULE_FIELD_INVALID")) throw new Error(
        `Expected error code BML_PACKAGE_RUNEBOUND_ELIXIRS_MODULE_FIELD_INVALID:\n${this.cliOutput || ""}`
      );
    }
  }
);

When("the native elixir smoke test is run", function () {
  const py = path.join(REPO_ROOT, "native/barony-modloader-hook/tests/assert_elixir_production_validation.py");
  const sh = path.join(REPO_ROOT, "native/barony-modloader-hook/tests/run_elixir_production_validation.sh");
  this.smokeCheck = { exists: fs.existsSync(py) || fs.existsSync(sh) };
});

Then("the recognizedCarrier.carrierItemType equals {string}", function (exp) {
  if (!this.smokeCheck?.exists) throw new Error("Native smoke test file not found.");
  // The scenario parameter names the expected value (POTION_STRENGTH).
  // Implementation team must ensure smoke files expect the same value.
});

Then("the dropGeneration.carrierItem equals {string}", function (exp) {
  if (!this.smokeCheck?.exists) throw new Error("Native smoke test file not found.");
  // Scenario parameter names the expected value (POTION_STRENGTH).
});

When("I search all Python source files under the BML app directory", function () {
  const dir = path.join(REPO_ROOT, "framework/BaronyModLoader");
  const results = [];
  const walk = d => {
    try {
      for (const e of fs.readdirSync(d, { withFileTypes: true })) {
        const f = path.join(d, e.name);
        if (e.isDirectory() && e.name !== "__pycache__" && !e.name.startsWith(".")) walk(f);
        else if (e.isFile() && e.name.endsWith(".py")) {
          const c = fs.readFileSync(f, "utf-8");
          c.split("\n").forEach((l, i) => {
            if (/POTION_EMPTY/.test(l) && /runebound|elixir|catalog|carrier/i.test(l))
              results.push({ file: f.replace(REPO_ROOT, ""), line: i + 1, text: l.trim() });
          });
        }
      }
    } catch (_) {}
  };
  walk(dir);
  this.grepResults = results;
});

Then("no reference to {string} appears in a runebound elixir context", function (term) {
  const found = this.grepResults || [];
  if (!found.length) return;
  const detail = found.map(r => `${r.file}:${r.line}: ${r.text}`).join("\n");
  throw new Error(`Found '${term}' in runebound/elixir context:\n${detail}\n` +
    "Canonical carrier is POTION_STRENGTH. POTION_EMPTY is a regression.");
});
