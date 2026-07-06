"use strict";

const { After, Given, When, Then } = require("@cucumber/cucumber");
const { spawnSync } = require("child_process");
const fs = require("fs");
const os = require("os");
const path = require("path");

// paths: <repo>/framework/BaronyModLoader/features/steps/gui_open_steps.js
//          ../../../../ from here = <repo>
const REPO_ROOT = path.resolve(__dirname, "../../../../");
const BML_BIN = path.join(REPO_ROOT, "framework/BaronyModLoader/bin/barony-mod-loader");
const RIGHT_SIDE_LABELS = ["Selected Mod", "Environment", "Profiles", "Workshop"];
const LEGACY_DETAIL_LABELS = ["Diagnostics", "Windows Status", "Launch Dry Run"];


const OPENED_FLAG_KEYS = new Set([
  "opened",
  "guiOpened",
  "rootOpened",
  "tkRootOpened",
  "windowOpened",
  "visibleRootOpened",
]);

function runGui(args, world) {
  const result = spawnSync(BML_BIN, ["gui", ...args], {
    cwd: REPO_ROOT,
    encoding: "utf-8",
    timeout: 20000,
    env: { ...process.env, PATH: `/usr/bin:${process.env.PATH || ""}` },
    stdio: ["ignore", "pipe", "pipe"],
  });
  world.guiCommand = {
    args,
    status: result.status === null ? 1 : result.status,
    signal: result.signal,
    error: result.error,
    stdout: result.stdout || "",
    stderr: result.stderr || "",
  };
}

function commandOutput(world) {
  const command = world.guiCommand || {};
  return `${command.stdout || ""}\n${command.stderr || ""}`;
}

function extractJsonObject(text) {
  const first = text.indexOf("{");
  const last = text.lastIndexOf("}");
  if (first === -1 || last === -1 || last < first) return null;
  try {
    return JSON.parse(text.slice(first, last + 1));
  } catch (_) {
    return null;
  }
}

function loadSmokeReport(world) {
  if (world.guiSmokeReport !== undefined) return world.guiSmokeReport;
  if (!world.guiSmokeReportPath || !fs.existsSync(world.guiSmokeReportPath)) {
    world.guiSmokeReport = null;
    world.guiSmokeReportRaw = "";
    return null;
  }
  const raw = fs.readFileSync(world.guiSmokeReportPath, "utf-8");
  world.guiSmokeReportRaw = raw;
  try {
    world.guiSmokeReport = JSON.parse(raw);
  } catch (_) {
    world.guiSmokeReport = { rawText: raw };
  }
  return world.guiSmokeReport;
}

function walk(value, visit, pathParts = []) {
  visit(value, pathParts);
  if (Array.isArray(value)) {
    value.forEach((item, index) => walk(item, visit, pathParts.concat(String(index))));
    return;
  }
  if (value && typeof value === "object") {
    for (const [key, child] of Object.entries(value)) {
      walk(child, visit, pathParts.concat(key));
    }
  }
}

function normalizeLabel(value) {
  return String(value || "").trim().toLowerCase().replace(/[\s_-]+/g, " ");
}

function displayLabelForObject(node, fallback = "") {
  if (typeof node === "string") return node;
  if (!node || typeof node !== "object") return fallback;
  for (const key of ["label", "title", "name", "concept", "conceptLabel", "section", "id", "key"]) {
    if (typeof node[key] === "string" && node[key].trim()) return node[key];
  }
  return fallback;
}

function pushUniqueLabel(labels, value) {
  const label = String(value || "").trim();
  if (!label) return;
  if (!labels.some((existing) => normalizeLabel(existing) === normalizeLabel(label))) labels.push(label);
}

function topLevelLabels(report) {
  if (!report || typeof report !== "object") return [];
  for (const key of ["renderedConceptTitles", "renderedSectionLabels"]) {
    if (Array.isArray(report[key]) && report[key].length) {
      const labels = [];
      report[key].forEach((label) => pushUniqueLabel(labels, label));
      return labels;
    }
  }

  const labels = [];
  for (const key of ["conceptCards", "concepts", "cards", "sections", "widgets"]) {
    const value = report[key];
    if (!Array.isArray(value)) continue;
    value.forEach((item) => pushUniqueLabel(labels, displayLabelForObject(item, typeof item === "string" ? item : "")));
  }
  return labels;
}

function structuralText(value) {
  const parts = [];
  walk(value, (node, pathParts) => {
    if (pathParts.length) parts.push(pathParts[pathParts.length - 1]);
    if (typeof node === "string" || typeof node === "number" || typeof node === "boolean") {
      parts.push(String(node));
    }
  });
  return parts.join("\n");
}

function findFlag(value, expected) {
  const matches = [];
  walk(value, (node, pathParts) => {
    const key = pathParts[pathParts.length - 1];
    if (!OPENED_FLAG_KEYS.has(key)) return;
    if (node === expected) matches.push({ key, value: node, path: pathParts.join(".") });
  });
  return matches;
}

function reportShowsOpened(value) {
  if (findFlag(value, true).length > 0) return true;
  let openedStatus = false;
  walk(value, (node, pathParts) => {
    const key = pathParts[pathParts.length - 1];
    if (/^(status|state|result)$/i.test(key || "") && typeof node === "string" && /^(opened|open|visible)$/i.test(node)) {
      openedStatus = true;
    }
  });
  return openedStatus;
}

function reportShowsNoPublish(value) {
  let found = false;
  walk(value, (node, pathParts) => {
    const key = pathParts[pathParts.length - 1] || "";
    const pathText = pathParts.join(".");
    if (/publishEnabled|publishable|canPublish/i.test(key) && node === false) found = true;
    if (/publishMode|publicationMode|workshopMode/i.test(key) && typeof node === "string" && /dry[- ]?run|no[- ]?publish|disabled/i.test(node)) found = true;
    if (/no[- ]?publish/i.test(key) && node === true) found = true;
    if (/workshop/i.test(pathText) && typeof node === "string" && /dry[- ]?run|no[- ]?publish|will not publish|not publish|publish disabled/i.test(node)) found = true;
  });
  return found || /workshop[\s\S]*(dry[- ]?run|no[- ]?publish|will not publish|not publish|publish disabled)/i.test(structuralText(value));
}

function hasAffirmativeRuneboundPlayableClaim(value) {
  let found = false;
  walk(value, (node, pathParts) => {
    const key = pathParts[pathParts.length - 1] || "";
    const pathText = pathParts.join(".");
    if (/playable/i.test(key) && node === true && /runebound/i.test(pathText)) found = true;
    if (typeof node !== "string") return;
    const text = node.toLowerCase();
    if (!text.includes("runebound") || !text.includes("playable")) return;
    if (/not playable|not yet playable|no playable claim|cannot be reported as playable|unsupported|unverified/.test(text)) return;
    found = true;
  });
  return found;
}

function explainCommand(world) {
  const command = world.guiCommand || {};
  return `ARGV: ${[BML_BIN, "gui", ...(command.args || [])].join(" ")}\nEXIT: ${command.status}\nSIGNAL: ${command.signal || ""}\nSTDOUT:\n${command.stdout || ""}\nSTDERR:\n${command.stderr || ""}`;
}

function failForDisplayGapIfPresent(world, prefix) {
  const parsed = extractJsonObject(commandOutput(world));
  const status = parsed && parsed.gui && parsed.gui.status;
  const text = commandOutput(world);
  if (status === "headless" || status === "unavailable" || /no graphical display|display is unavailable|Tk GUI runtime is unavailable|couldn.t connect to display|no display name/i.test(text)) {
    throw new Error(`${prefix}: product validation gap — a real Tk display/runtime is unavailable, so visible GUI opening cannot be validated.\n${explainCommand(world)}`);
  }
}

Given("a hermetic GUI open smoke report path", function () {
  this.guiOpenDir = fs.mkdtempSync(path.join(os.tmpdir(), "bml-gui-open-"));
  this.guiSmokeReportPath = path.join(this.guiOpenDir, "gui-smoke-report.json");
  this.guiSmokeReport = undefined;
  this.guiSmokeReportRaw = undefined;
});

After(function () {
  if (this.guiOpenDir) {
    try { fs.rmSync(this.guiOpenDir, { recursive: true, force: true }); } catch (_) {}
  }
});

When("I run the BaronyModLoader GUI command with arguments {string}", function (argString) {
  const args = argString.trim() ? argString.trim().split(/\s+/) : [];
  runGui(args, this);
});

When("I run the BaronyModLoader GUI command with auto-close smoke reporting", function () {
  this.guiSmokeReport = undefined;
  this.guiSmokeReportRaw = undefined;
  runGui(["--auto-close-ms", "750", "--smoke-report", this.guiSmokeReportPath], this);
});

Then("the GUI readiness check reports Tk availability", function () {
  const parsed = extractJsonObject(commandOutput(this));
  if (!parsed || !parsed.gui || typeof parsed.gui.status !== "string") {
    throw new Error(`Expected gui --check to report JSON with gui.status.\n${explainCommand(this)}`);
  }
  failForDisplayGapIfPresent(this, "gui --check");
  if (this.guiCommand.status !== 0) {
    throw new Error(`Expected gui --check to exit successfully while reporting Tk availability.\n${explainCommand(this)}`);
  }
  if (parsed.gui.status !== "available") {
    throw new Error(`Expected gui --check to report Tk status 'available', got ${JSON.stringify(parsed.gui.status)}.\n${explainCommand(this)}`);
  }
});

Then("the GUI readiness check reports that no Tk root was opened", function () {
  const parsed = extractJsonObject(commandOutput(this));
  if (!parsed) throw new Error(`Expected JSON output from gui --check.\n${explainCommand(this)}`);
  if (findFlag(parsed, true).length > 0) {
    throw new Error(`gui --check reported that a Tk root/window opened, but --check must be non-opening.\n${explainCommand(this)}`);
  }
  if (fs.existsSync(this.guiSmokeReportPath)) {
    throw new Error(`gui --check wrote a smoke/open report at ${this.guiSmokeReportPath}, but --check must be non-opening.\n${explainCommand(this)}`);
  }
});

Then("the GUI command exits successfully", function () {
  failForDisplayGapIfPresent(this, "gui open smoke");
  if (this.guiCommand.status !== 0) {
    throw new Error(`Expected GUI open smoke command to exit 0.\n${explainCommand(this)}`);
  }
});

Then("the GUI smoke report is written", function () {
  if (!fs.existsSync(this.guiSmokeReportPath)) {
    throw new Error(`Expected GUI smoke report at ${this.guiSmokeReportPath}.\n${explainCommand(this)}`);
  }
  const report = loadSmokeReport(this);
  if (!report) throw new Error(`Expected a readable GUI smoke report at ${this.guiSmokeReportPath}.`);
});

Then("the GUI smoke report says a Tk root was opened", function () {
  const report = loadSmokeReport(this);
  if (!reportShowsOpened(report)) {
    throw new Error(`Expected smoke report to say a real Tk root/window was opened.\nREPORT:\n${this.guiSmokeReportRaw || JSON.stringify(report, null, 2)}\n${explainCommand(this)}`);
  }
});

Then("the GUI smoke report contains the merged Mods list with Environment, Profiles, and Workshop cards", function () {
  const report = loadSmokeReport(this);
  const labels = topLevelLabels(report);
  const normalizedLabels = labels.map(normalizeLabel);
  const expected = RIGHT_SIDE_LABELS.map(normalizeLabel);
  const missing = RIGHT_SIDE_LABELS.filter((label) => !normalizedLabels.includes(normalizeLabel(label)));
  const extras = labels.filter((label) => !expected.includes(normalizeLabel(label)));
  const wrongOrder = labels.length === RIGHT_SIDE_LABELS.length && RIGHT_SIDE_LABELS.some((label, index) => normalizeLabel(labels[index]) !== normalizeLabel(label));
  const legacyTopLevel = labels.filter((label) => LEGACY_DETAIL_LABELS.some((legacy) => normalizeLabel(label) === normalizeLabel(legacy)));
  const modsTitle = String(report.modsSidebarTitle || report.detectedModsSidebarTitle || report.modsListTitle || "");
  const headerActions = Array.isArray(report.modsListHeaderActions) ? report.modsListHeaderActions : [];
  const headerActionIds = headerActions.map((action) => String(action && action.id ? action.id : action)).filter(Boolean);
  const hasModHeaderActions = ["scan-packages", "enable-package", "disable-package"].every((id) => headerActionIds.includes(id));
  if (missing.length || extras.length || labels.length !== RIGHT_SIDE_LABELS.length || wrongOrder || legacyTopLevel.length || normalizeLabel(modsTitle) !== "mods" || !hasModHeaderActions) {
    throw new Error(
      `GUI smoke report must render a merged left Mods list with scan/enable/disable header actions, and right-side cards ${RIGHT_SIDE_LABELS.join(", ")}.\n` +
      `FOUND RIGHT-SIDE LABELS: ${labels.join(", ") || "<none>"}\n` +
      `MISSING: ${missing.join(", ") || "<none>"}\n` +
      `EXTRA: ${extras.join(", ") || "<none>"}\n` +
      `MODS LIST TITLE: ${modsTitle || "<none>"}\n` +
      `MODS HEADER ACTIONS: ${headerActionIds.join(", ") || "<none>"}\n` +
      `REPORT:\n${this.guiSmokeReportRaw || JSON.stringify(report, null, 2)}`
    );
  }
});

Then("the GUI smoke report shows disabled or blocking reasons", function () {
  const report = loadSmokeReport(this);
  const text = structuralText(report);
  if (!/(disabled|blocked|blocking|blocker)/i.test(text) || !/(reason|missing|not selected|required|unavailable|unverified)/i.test(text)) {
    throw new Error(`Expected smoke report to include visible disabled/blocking reasons.\nREPORT:\n${this.guiSmokeReportRaw || JSON.stringify(report, null, 2)}`);
  }
});

Then("the GUI smoke report shows Workshop dry-run no-publish status", function () {
  const report = loadSmokeReport(this);
  if (!reportShowsNoPublish(report)) {
    throw new Error(`Expected smoke report to show Workshop dry-run/no-publish status.\nREPORT:\n${this.guiSmokeReportRaw || JSON.stringify(report, null, 2)}`);
  }
});

Then("the GUI smoke report contains no visible Runebound playable claim", function () {
  const report = loadSmokeReport(this);
  if (hasAffirmativeRuneboundPlayableClaim(report)) {
    throw new Error(`Smoke report contains an affirmative Runebound playable claim, which is not allowed without production evidence.\nREPORT:\n${this.guiSmokeReportRaw || JSON.stringify(report, null, 2)}`);
  }
});
