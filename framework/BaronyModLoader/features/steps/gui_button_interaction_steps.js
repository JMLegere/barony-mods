"use strict";

const { After, Given, When, Then } = require("@cucumber/cucumber");
const { spawnSync } = require("child_process");
const fs = require("fs");
const os = require("os");
const path = require("path");

// paths: <repo>/framework/BaronyModLoader/features/steps/gui_button_interaction_steps.js
//          ../../../../ from here = <repo>
const REPO_ROOT = path.resolve(__dirname, "../../../../");
const BML_BIN = path.join(REPO_ROOT, "framework/BaronyModLoader/bin/barony-mod-loader");
const BUTTON_CONTRACT = path.join(REPO_ROOT, ".tmp/profile-first-button-interaction-contract.json");
const RUNEBOUND_ID = "jml.runebound-elixirs";
const RUNEBOUND_NAME = "Runebound: Elixirs";
const EXPECTED_SAFE_ALL_ACTIONS = [
  "detect-install",
  "refresh-readiness",
  "open-diagnostics",
  "create-select-profile",
  "scan-packages",
  "enable-package",
  "disable-package",
  "workshop-preview",
];
const LAUNCH_ACTIONS = [
  { id: "launch-bml", label: "Launch BaronyModLoader" },
  { id: "launch-vanilla", label: "Launch Vanilla Barony" },
];

function loadContract() {
  const raw = fs.readFileSync(BUTTON_CONTRACT, "utf8");
  return JSON.parse(raw);
}

function commandExplanation(world) {
  const result = world.guiButtonInteractionCommand || {};
  return [
    `Command: ${world.guiButtonInteractionCommandLine || "<not run>"}`,
    `Exit status: ${result.status === null || result.status === undefined ? "<unset>" : result.status}`,
    `Signal: ${result.signal || "<none>"}`,
    `STDOUT:\n${result.stdout || "<empty>"}`,
    `STDERR:\n${result.stderr || "<empty>"}`,
  ].join("\n");
}

function reportPreview(world) {
  if (world.guiButtonInteractionReportRaw) return world.guiButtonInteractionReportRaw;
  if (world.guiButtonInteractionReport) return JSON.stringify(world.guiButtonInteractionReport, null, 2);
  return "<no button interaction smoke report loaded>";
}

function contractGap(world, message, extra = "") {
  throw new Error(
    `BUTTON INTERACTION CONTRACT GAP: ${message}\n` +
      `Smoke report path: ${world.guiButtonInteractionReportPath || "<unset>"}\n` +
      (extra ? `${extra}\n` : "") +
      `REPORT:\n${reportPreview(world)}\n${commandExplanation(world)}`
  );
}

function walk(value, visit, pathParts = [], seen = new Set()) {
  visit(value, pathParts);
  if (!value || typeof value !== "object") return;
  if (seen.has(value)) return;
  seen.add(value);
  if (Array.isArray(value)) {
    value.forEach((item, index) => walk(item, visit, pathParts.concat(String(index)), seen));
    return;
  }
  Object.entries(value).forEach(([key, item]) => walk(item, visit, pathParts.concat(key), seen));
}

function structuralText(value) {
  const parts = [];
  walk(value, (node) => {
    if (node === null || node === undefined) return;
    if (["string", "number", "boolean"].includes(typeof node)) parts.push(String(node));
  });
  return parts.join("\n");
}

function normalizeActionId(value) {
  const normalized = String(value || "")
    .trim()
    .toLowerCase()
    .replace(/runebound:\s*elixirs/g, "runebound-elixirs")
    .replace(/[^a-z0-9]+/g, "-")
    .replace(/^-+|-+$/g, "");
  if (/^(launch-bml|launch-baronymodloader|launch-barony-mod-loader)$/.test(normalized)) return "launch-bml";
  if (/^(launch-vanilla|launch-vanilla-barony)$/.test(normalized)) return "launch-vanilla";
  return normalized;
}

function actionIdForEntry(entry) {
  if (typeof entry === "string") return normalizeActionId(entry);
  if (!entry || typeof entry !== "object") return "";
  for (const key of ["actionId", "id", "action", "command", "buttonAction", "name", "label", "text"]) {
    if (typeof entry[key] === "string" && entry[key].trim()) return normalizeActionId(entry[key]);
  }
  return normalizeActionId(structuralText(entry).split("\n")[0] || "");
}

function textForEntry(entry) {
  return typeof entry === "string" ? entry : JSON.stringify(entry, null, 2);
}

function directField(report, key) {
  return report && Object.prototype.hasOwnProperty.call(report, key) ? report[key] : undefined;
}

function clickedActions(report) {
  const raw = directField(report, "clickedActions");
  if (Array.isArray(raw)) return raw;
  if (raw && typeof raw === "object") return Object.values(raw);
  return null;
}

function actionEntries(report, actionId) {
  const actions = clickedActions(report);
  if (!actions) return [];
  const normalized = normalizeActionId(actionId);
  return actions.filter((entry) => {
    const entryId = actionIdForEntry(entry);
    if (entryId) return entryId === normalized;
    return new RegExp(`(^|[^a-z0-9])${normalized.replace(/[.*+?^${}()|[\]\\]/g, "\\$&")}([^a-z0-9]|$)`, "i").test(
      textForEntry(entry)
    );
  });
}

function ensureReport(world) {
  if (world.guiButtonInteractionReport) return world.guiButtonInteractionReport;
  const result = world.guiButtonInteractionCommand || {};
  if (result.error) {
    contractGap(world, `GUI button smoke command could not run: ${result.error.message || result.error}`);
  }
  if (result.status !== 0) {
    contractGap(world, "GUI button smoke command failed; expected real launcher button smoke to exit successfully.");
  }
  const reportPath = world.guiButtonInteractionReportPath;
  if (!reportPath || !fs.existsSync(reportPath)) {
    contractGap(world, "GUI button smoke report was not written.");
  }
  try {
    world.guiButtonInteractionReportRaw = fs.readFileSync(reportPath, "utf8");
    world.guiButtonInteractionReport = JSON.parse(world.guiButtonInteractionReportRaw);
  } catch (error) {
    contractGap(world, `GUI button smoke report is not valid JSON: ${error.message}`);
  }
  return world.guiButtonInteractionReport;
}

function requireClickedAction(world, actionId) {
  const report = ensureReport(world);
  const entries = actionEntries(report, actionId);
  if (entries.length === 0) {
    contractGap(world, `Expected clickedActions to include ${actionId}.`);
  }
  if (!entries.some((entry) => hasActionResult(entry))) {
    contractGap(world, `Expected clickedActions entry for ${actionId} to include a per-action result/status/feedback object.`);
  }
  if (entries.some((entry) => actionFailed(entry))) {
    contractGap(world, `Clicked action ${actionId} reported failure.`, `ACTION:\n${entries.map(textForEntry).join("\n")}`);
  }
  return entries;
}

function hasActionResult(entry) {
  if (!entry || typeof entry !== "object" || Array.isArray(entry)) return false;
  return ["result", "status", "outcome", "message", "feedback", "visibleFeedback", "activity", "success"].some((key) =>
    Object.prototype.hasOwnProperty.call(entry, key)
  );
}

function actionFailed(entry) {
  const text = textForEntry(entry).toLowerCase();
  if (entry && typeof entry === "object") {
    if (entry.success === false) return true;
    for (const key of ["status", "outcome", "result"]) {
      if (typeof entry[key] === "string" && /^(fail|failed|error|blocked)$/i.test(entry[key])) return true;
    }
  }
  return /button interaction contract gap|traceback|uncaught|command failed/.test(text);
}

function visibleActivityEntries(report) {
  const raw = directField(report, "visibleActivityLog");
  if (Array.isArray(raw)) return raw;
  if (typeof raw === "string") return raw.split(/\r?\n/).filter((line) => line.trim());
  if (raw && typeof raw === "object") {
    for (const key of ["entries", "items", "messages", "lines", "actions", "log"]) {
      if (Array.isArray(raw[key])) return raw[key];
    }
    return [raw];
  }
  return null;
}

function visibleActivityText(report) {
  const entries = visibleActivityEntries(report);
  return entries ? structuralText(entries).toLowerCase() : "";
}

function booleanFieldEvidence(value, keyPattern, expected) {
  const matches = [];
  walk(value, (node, pathParts) => {
    const key = pathParts[pathParts.length - 1] || "";
    if (keyPattern.test(key) && node === expected) matches.push(pathParts.join("."));
  });
  return matches;
}

function assertNoTrueSideEffect(world, keyPattern, sideEffectName) {
  const report = ensureReport(world);
  const trueMatches = booleanFieldEvidence(report, keyPattern, true);
  if (trueMatches.length) {
    contractGap(world, `${sideEffectName} side effect was true.`, `True fields: ${trueMatches.join(", ")}`);
  }
}

function valuesForKey(value, keyPattern) {
  const matches = [];
  walk(value, (node, pathParts) => {
    const key = pathParts[pathParts.length - 1] || "";
    if (keyPattern.test(key)) matches.push({ value: node, pathParts });
  });
  return matches;
}

function actionNeedles(actionId) {
  const normalized = normalizeActionId(actionId);
  const launch = LAUNCH_ACTIONS.find((entry) => entry.id === normalized);
  return [normalized, launch && normalizeActionId(launch.label)].filter(Boolean);
}

function actionLogEntries(report, actionId) {
  const candidates = [];
  for (const key of ["actionLog", "actions", "visibleActivityLog", "activityLog", "recentActivity", "feedback"]) {
    const value = directField(report, key);
    if (Array.isArray(value)) candidates.push(...value);
    else if (value && typeof value === "object") candidates.push(value);
  }
  const needles = actionNeedles(actionId);
  return candidates.filter((entry) => {
    const entryId = actionIdForEntry(entry);
    const text = normalizeActionId(textForEntry(entry));
    return needles.some((needle) => entryId === needle || text.includes(needle));
  });
}

function actionEvidenceEntries(report, actionId) {
  return [...actionEntries(report, actionId), ...actionLogEntries(report, actionId)];
}

function actionEvidenceText(report, actionId) {
  return actionEvidenceEntries(report, actionId).map(textForEntry).join("\n");
}

function requireActionBooleanEvidence(world, actionId, keyPattern, expected, description) {
  const report = ensureReport(world);
  const evidence = actionEvidenceEntries(report, actionId);
  const matches = evidence.flatMap((entry) => booleanFieldEvidence(entry, keyPattern, expected));
  if (matches.length === 0) {
    contractGap(
      world,
      `${actionId} does not report ${description}=${expected}.`,
      `ACTION EVIDENCE:\n${evidence.map(textForEntry).join("\n---\n") || "<none>"}`
    );
  }
}

function requireLaunchTkInvocation(world, actionId) {
  const report = ensureReport(world);
  const entries = actionEvidenceEntries(report, actionId);
  const localEvidence = entries.some(
    (entry) =>
      booleanFieldEvidence(entry, /^(actualTkButtonInvoked|tkButtonInvoked|smokeClickInvokedTkButton|buttonWidgetInvoked|buttonInvoked)$/i, true).length > 0 ||
      /actual\s+tk\s+button|tk\s+button|ttk\.button|button\.invoke|widget\s+invoke|invoked\s+.*button/i.test(textForEntry(entry))
  );
  if (!localEvidence && !reportHasTkButtonInvocation(report)) {
    contractGap(world, `${actionId} lacks evidence that a real Tk Button widget was invoked.`);
  }
}

function requireVisibleLaunchFeedback(world, actionId, label) {
  const report = ensureReport(world);
  const activity = visibleActivityText(report);
  const labelPattern = new RegExp(`${label.replace(/[.*+?^${}()|[\]\\]/g, "\\$&")}|${actionId}`, "i");
  if (!labelPattern.test(activity) || !/mock|started|launched|process|pid|vanilla|baronymodloader/i.test(activity)) {
    contractGap(world, `${actionId} lacks concise visible launch feedback in Recent Activity/Action Log.`);
  }
}

function collectPathStrings(value) {
  const paths = [];
  walk(value, (node, pathParts) => {
    const key = pathParts[pathParts.length - 1] || "";
    if (typeof node === "string" && /(profile.*(path|dir|root|folder|location)|selectedProfilePath)$/i.test(key)) {
      paths.push(node);
    }
  });
  return paths;
}

function pathHasDotTmp(candidate) {
  return String(candidate).split(/[\\/]+/).includes(".tmp");
}

function activeModsFrom(value) {
  if (!value) return [];
  if (Array.isArray(value)) return value.map((item) => (typeof item === "string" ? item : structuralText(item)));
  const matches = [];
  walk(value, (node, pathParts) => {
    const key = pathParts[pathParts.length - 1] || "";
    if (/^(activeMods|enabledMods|selectedMods)$/i.test(key) && Array.isArray(node)) {
      node.forEach((item) => matches.push(typeof item === "string" ? item : structuralText(item)));
    }
  });
  return matches;
}

function hasRunebound(mods) {
  return mods.some((mod) => /jml\.runebound-elixirs|runebound:\s*elixirs|runebound-elixirs/i.test(String(mod)));
}

function stateBucket(report, bucketName, actionId) {
  const bucket = report && report[bucketName];
  if (!bucket) return null;
  if (bucket[actionId]) return bucket[actionId];
  if (bucket[normalizeActionId(actionId)]) return bucket[normalizeActionId(actionId)];
  if (Array.isArray(bucket)) {
    return bucket.find((entry) => actionIdForEntry(entry) === normalizeActionId(actionId)) || null;
  }
  if (typeof bucket === "object") {
    return Object.values(bucket).find((entry) => actionIdForEntry(entry) === normalizeActionId(actionId)) || null;
  }
  return null;
}

function actionTransition(report, actionId) {
  const entries = actionEntries(report, actionId);
  for (const entry of entries) {
    if (!entry || typeof entry !== "object") continue;
    const hasBeforeActiveMods = Object.prototype.hasOwnProperty.call(entry, "beforeActiveMods");
    const hasAfterActiveMods = Object.prototype.hasOwnProperty.call(entry, "afterActiveMods");
    const before =
      entry.before ||
      entry.beforeState ||
      entry.beforeAction ||
      entry.stateBefore ||
      (hasBeforeActiveMods ? { activeMods: entry.beforeActiveMods } : undefined);
    const after =
      entry.after ||
      entry.afterState ||
      entry.afterAction ||
      entry.stateAfter ||
      (hasAfterActiveMods ? { activeMods: entry.afterActiveMods } : undefined);
    if (before || after) return { before, after, source: entry };
  }
  return {
    before: stateBucket(report, "beforeActions", actionId),
    after: stateBucket(report, "afterActions", actionId),
    source: null,
  };
}

function assertTransition(world, actionId, beforePredicate, afterPredicate, message) {
  const report = ensureReport(world);
  const transition = actionTransition(report, actionId);
  const beforeMods = activeModsFrom(transition.before);
  const afterMods = activeModsFrom(transition.after);
  if (!transition.before || !transition.after || !beforePredicate(beforeMods) || !afterPredicate(afterMods)) {
    contractGap(
      world,
      message,
      `Action ${actionId} before active mods: ${JSON.stringify(beforeMods)}\n` +
        `Action ${actionId} after active mods: ${JSON.stringify(afterMods)}\n` +
        `Transition source:\n${JSON.stringify(transition, null, 2)}`
    );
  }
}

function reportHasTkRootOpened(report) {
  return booleanFieldEvidence(report, /^(opened|guiOpened|rootOpened|tkRootOpened|windowOpened|visibleRootOpened)$/i, true).length > 0;
}

function reportHasTkButtonInvocation(report) {
  if (booleanFieldEvidence(report, /^(actualTkButtonsInvoked|tkButtonInvoked|smokeClicksInvokedTkButtons|buttonWidgetsInvoked)$/i, true).length) return true;
  return /actual\s+tk\s+button|tk\s+button|ttk\.button|button\.invoke|widget\s+invoke|invoked\s+.*button/i.test(
    structuralText(report)
  );
}

function hasDirectFalse(report, keyPattern) {
  return booleanFieldEvidence(report, keyPattern, false).length > 0;
}

function runeboundPlayableClaim(value) {
  let found = null;
  walk(value, (node, pathParts) => {
    if (found) return;
    const key = pathParts[pathParts.length - 1] || "";
    const pathText = pathParts.join(".");
    if (/playable/i.test(key) && node === true && /runebound/i.test(pathText)) {
      found = `${pathText}=true`;
      return;
    }
    if (typeof node !== "string") return;
    const text = node.toLowerCase();
    if (!text.includes("runebound") || !text.includes("playable")) return;
    if (/not playable|not yet playable|no playable claim|cannot be reported as playable|cannot claim playable|unsupported|unverified|without production|no .*playable/.test(text)) return;
    found = `${pathText}: ${node}`;
  });
  return found;
}

Given("a BaronyModLoader GUI button interaction smoke report path", function () {
  loadContract();
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), "bml-gui-button-interactions-"));
  this.guiButtonInteractionTempDir = dir;
  this.guiButtonInteractionReportPath = path.join(dir, "button-interaction-smoke-report.json");
});

After(function () {
  if (this.guiButtonInteractionTempDir && fs.existsSync(this.guiButtonInteractionTempDir)) {
    fs.rmSync(this.guiButtonInteractionTempDir, { recursive: true, force: true });
  }
});

When("I run the BaronyModLoader GUI with all smoke button clicks", function () {
  const args = ["gui", "--smoke-clicks", "all", "--smoke-report", this.guiButtonInteractionReportPath];
  this.guiButtonInteractionCommandLine = `${BML_BIN} ${args.join(" ")}`;
  this.guiButtonInteractionCommand = spawnSync(BML_BIN, args, {
    cwd: REPO_ROOT,
    env: { ...process.env, PATH: `/usr/bin:${process.env.PATH || ""}` },
    encoding: "utf8",
    timeout: 90000,
  });
  if (this.guiButtonInteractionCommand.error && this.guiButtonInteractionCommand.error.code === "ETIMEDOUT") {
    this.guiButtonInteractionCommand.status = 124;
  }
});

When("I run the BaronyModLoader GUI with mocked launch button clicks", function () {
  const args = ["gui", "--smoke-clicks", "launch-bml,launch-vanilla", "--smoke-report", this.guiButtonInteractionReportPath];
  this.guiButtonInteractionCommandLine = `BML_GUI_LAUNCH_MODE=mock ${BML_BIN} ${args.join(" ")}`;
  this.guiButtonInteractionCommand = spawnSync(BML_BIN, args, {
    cwd: REPO_ROOT,
    env: { ...process.env, PATH: `/usr/bin:${process.env.PATH || ""}`, BML_GUI_LAUNCH_MODE: "mock" },
    encoding: "utf8",
    timeout: 90000,
  });
  if (this.guiButtonInteractionCommand.error && this.guiButtonInteractionCommand.error.code === "ETIMEDOUT") {
    this.guiButtonInteractionCommand.status = 124;
  }
});

Then("the button smoke invoked the safe Environment button actions", function () {
  ["detect-install", "refresh-readiness", "open-diagnostics"].forEach((actionId) => requireClickedAction(this, actionId));
});

Then("Environment button feedback updates readiness and diagnostics without starting Barony", function () {
  const report = ensureReport(this);
  const activity = visibleActivityText(report);
  for (const expected of [/readiness|ready|blocked/, /diagnostic|evidence/]) {
    if (!expected.test(activity)) {
      contractGap(this, `Recent Activity/Action Log does not show Environment feedback matching ${expected}.`);
    }
  }
  assertNoTrueSideEffect(this, /^processStarted$|^processLaunched$|baronyStarted|gameStarted|startedProcess/i, "Barony process start");
});

Then("unsafe all-click smoke does not include GUI launch actions", function () {
  const report = ensureReport(this);
  for (const { id, label } of LAUNCH_ACTIONS) {
    if (actionEntries(report, id).length > 0) {
      contractGap(this, `Unsafe all-click smoke invoked ${label}; launch actions must require explicit mock launch smoke.`);
    }
  }
  assertNoTrueSideEffect(this, /^processStarted$|^processLaunched$|baronyStarted|gameStarted|startedProcess/i, "Barony process start");
});

Then("the mocked launch smoke invoked both GUI launch buttons through Tk", function () {
  for (const { id } of LAUNCH_ACTIONS) {
    requireClickedAction(this, id);
    requireLaunchTkInvocation(this, id);
  }
});

Then("mocked launch feedback reports BML and Vanilla process launch metadata without starting Barony", function () {
  const report = ensureReport(this);
  for (const { id, label } of LAUNCH_ACTIONS) {
    requireActionBooleanEvidence(this, id, /^processStarted$/i, true, "processStarted");
    requireActionBooleanEvidence(this, id, /^processLaunched$/i, true, "processLaunched");
    requireActionBooleanEvidence(this, id, /^mocked$/i, true, "mocked");
    requireVisibleLaunchFeedback(this, id, label);
  }
  const bmlText = actionEvidenceText(report, "launch-bml");
  if (!/runtimeManifestPath|runtimeManifest|runtime[-_ ]manifest|BML_RUNTIME_MANIFEST/i.test(bmlText)) {
    contractGap(this, "launch-bml lacks runtime manifest evidence.", `ACTION EVIDENCE:\n${bmlText || "<none>"}`);
  }
  if (!/\bBML_[A-Z0-9_]+\b|LD_PRELOAD/i.test(bmlText)) {
    contractGap(this, "launch-bml lacks BML launch environment evidence.", `ACTION EVIDENCE:\n${bmlText || "<none>"}`);
  }
  const vanillaText = actionEvidenceText(report, "launch-vanilla");
  if (/\bBML_[A-Z0-9_]+\b|LD_PRELOAD/i.test(vanillaText)) {
    contractGap(this, "launch-vanilla includes BML-specific environment or LD_PRELOAD evidence.", `ACTION EVIDENCE:\n${vanillaText}`);
  }
});

Then("the Profiles create-select-profile action creates and selects a stable profile outside .tmp", function () {
  const report = ensureReport(this);
  requireClickedAction(this, "create-select-profile");
  const candidates = collectPathStrings(report).filter((candidate) => /profile/i.test(candidate));
  const stable = candidates.find((candidate) => !pathHasDotTmp(candidate));
  if (!stable) {
    contractGap(this, "Expected a selected profile path outside the repository .tmp directory.", `Profile path candidates: ${JSON.stringify(candidates)}`);
  }
  if (path.resolve(stable).startsWith(path.join(REPO_ROOT, ".tmp") + path.sep)) {
    contractGap(this, `Selected profile is inside repository .tmp: ${stable}`);
  }
});

Then("the Mods scan-packages action lists Runebound: Elixirs", function () {
  const report = ensureReport(this);
  requireClickedAction(this, "scan-packages");
  const text = structuralText(report);
  if (!/Runebound:\s*Elixirs|jml\.runebound-elixirs|runebound-elixirs/i.test(text)) {
    contractGap(this, `Expected Mods scan report to list ${RUNEBOUND_NAME} (${RUNEBOUND_ID}).`);
  }
});

Then("the Runebound enable and disable button clicks mutate active mod state", function () {
  requireClickedAction(this, "enable-package");
  requireClickedAction(this, "disable-package");
  assertTransition(
    this,
    "enable-package",
    (mods) => !hasRunebound(mods),
    (mods) => hasRunebound(mods),
    "Expected enable-package to mutate active mods from no Runebound to Runebound enabled."
  );
  assertTransition(
    this,
    "disable-package",
    (mods) => hasRunebound(mods),
    (mods) => !hasRunebound(mods),
    "Expected disable-package to mutate active mods from Runebound enabled to disabled."
  );
});

Then("the Runebound enable and disable button clicks are idempotent", function () {
  const report = ensureReport(this);
  const enableEntries = actionEntries(report, "enable-package");
  const disableEntries = actionEntries(report, "disable-package");
  const enableText = enableEntries.map(textForEntry).join("\n").toLowerCase();
  const disableText = disableEntries.map(textForEntry).join("\n").toLowerCase();
  if (enableEntries.length < 2 || !/already[- ]enabled|idempotent/.test(enableText)) {
    contractGap(this, "Expected enable-package to be clicked repeatedly and report already-enabled/idempotent feedback.");
  }
  if (disableEntries.length < 2 || !/already[- ]disabled|idempotent/.test(disableText)) {
    contractGap(this, "Expected disable-package to be clicked repeatedly and report already-disabled/idempotent feedback.");
  }
});

Then("the Workshop preview action produces a dry-run no-publish preview", function () {
  const report = ensureReport(this);
  requireClickedAction(this, "workshop-preview");
  const text = structuralText(report).toLowerCase();
  if (!/dry[- ]run/.test(text) || !/workshop/.test(text) || !/(no[- ]publish|publishenabled.{0,20}false|publish enabled false|steampublished.{0,20}false|steam published false)/s.test(text)) {
    contractGap(this, "Expected Workshop preview to show dry-run/no-publish metadata.");
  }
});

Then("no Steam publish side effects are reported", function () {
  const report = ensureReport(this);
  if (!hasDirectFalse(report, /^steamPublished$/i)) {
    contractGap(this, "Expected smoke report to include steamPublished:false.");
  }
  assertNoTrueSideEffect(this, /^steamPublished$|publishedToSteam|steamUploadPerformed|publishPerformed|networkPublished/i, "Steam publish");
});

Then("the smoke report proves actual Tk buttons were invoked", function () {
  const report = ensureReport(this);
  if (!reportHasTkRootOpened(report)) {
    contractGap(this, "Expected report to prove a real Tk root/window was opened.");
  }
  if (!reportHasTkButtonInvocation(report)) {
    contractGap(this, "Expected report to prove actual Tk Button widgets were invoked, not backend functions only.");
  }
});

Then("the smoke report includes clickedActions and a visible activity log for every expected button action", function () {
  const report = ensureReport(this);
  const actions = clickedActions(report);
  if (!Array.isArray(actions) || actions.length === 0) {
    contractGap(this, "Expected non-empty clickedActions array in button smoke report.");
  }
  const activityEntries = visibleActivityEntries(report);
  if (!Array.isArray(activityEntries) || activityEntries.length === 0) {
    contractGap(this, "Expected non-empty visibleActivityLog in button smoke report.");
  }
  EXPECTED_SAFE_ALL_ACTIONS.forEach((actionId) => {
    const entries = requireClickedAction(this, actionId);
    const activity = visibleActivityText(report);
    const labelNeedles = entries
      .flatMap((entry) => [entry.label, entry.summary, entry.visibleSummary, entry.result && entry.result.label, entry.result && entry.result.summary, entry.result && entry.result.visibleSummary])
      .filter(Boolean)
      .map((value) => String(value).toLowerCase());
    const needles = [actionId, actionId.replace(/-/g, " "), ...labelNeedles];
    if (!needles.some((needle) => activity.includes(needle))) {
      contractGap(this, `Expected visibleActivityLog to include feedback for ${actionId}.`);
    }
  });
  if (!directField(report, "beforeActions") || !directField(report, "afterActions")) {
    contractGap(this, "Expected smoke report to include beforeActions and afterActions state snapshots.");
  }
  if (!directField(report, "activeMods")) {
    contractGap(this, "Expected smoke report to include activeMods summary after button clicks.");
  }
});

Then("no GUI button action claims Runebound: Elixirs is playable", function () {
  const report = ensureReport(this);
  if (directField(report, "playableClaimed") !== false) {
    contractGap(this, "Expected smoke report to include playableClaimed:false.");
  }
  const claim = runeboundPlayableClaim(report);
  if (claim) {
    contractGap(this, `Unexpected Runebound playable claim found: ${claim}`);
  }
});
