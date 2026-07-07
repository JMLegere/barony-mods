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
const STASH_ID = "jml.stash";
const STASH_NAME = "Stash";
const STASH_PKG_PATH = path.join(REPO_ROOT, "mods/stash");
const RUNEBOUND_PKG_PATH = path.join(REPO_ROOT, "mods/runebound-elixirs");
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
  { id: "launch-bml", label: "Launch BML Barony" },
  { id: "launch-vanilla", label: "Launch Vanilla Barony" },
];

const CLIPBOARD_PROBE_MAX_BUFFER = 16 * 1024 * 1024;
const CLIPBOARD_TEXT_MIME = "text/plain";
const SYSTEM_CLIPBOARD_TOOL_NAMES = new Set(["wl-copy", "wl-paste", "xclip", "xsel"]);
const COPY_FOR_AI_VISIBLE_SUMMARY = "Copied AI issue context";

function findExecutable(commandName, searchPath = process.env.PATH || "") {
  const pathEntries = searchPath.split(path.delimiter).filter(Boolean);
  for (const dir of pathEntries) {
    const candidate = path.join(dir, commandName);
    try {
      fs.accessSync(candidate, fs.constants.X_OK);
      return candidate;
    } catch (_error) {
      // Keep searching PATH.
    }
  }
  return null;
}

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
  if (/^(launch-bml|launch-bml-barony|launch-baronymodloader|launch-barony-mod-loader)$/.test(normalized)) return "launch-bml";
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
  if (!labelPattern.test(activity) || !/mock|started|launched|process|pid|vanilla|bml|barony/i.test(activity)) {
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

function allObjects(value) {
  const objects = [];
  walk(value, (node, pathParts) => {
    if (node && typeof node === "object" && !Array.isArray(node)) objects.push({ node, pathParts });
  });
  return objects;
}

function imagePathExists(candidate) {
  const pathText = String(candidate || "");
  const resolved = path.isAbsolute(pathText) ? pathText : path.resolve(REPO_ROOT, pathText);
  return fs.existsSync(resolved);
}

function concreteIconPathEntries(value) {
  return valuesForKey(value, /(iconPath|imagePath|logoPath|assetPath|sourcePath|icon|image|logo|asset|source)$/i)
    .filter((entry) => typeof entry.value === "string" && /\.(png|jpg|jpeg|gif)$/i.test(entry.value))
    .filter((entry) => !pathHasDotTmp(entry.value));
}

function bmlIconPathEntries(value) {
  return concreteIconPathEntries(value)
    .filter((entry) => /barony-modloader(?:-bml)?\.png$/i.test(String(entry.value)))
    .filter((entry) => imagePathExists(entry.value));
}

function compactVanillaBaronyIconPathEntries(value) {
  return concreteIconPathEntries(value)
    .filter((entry) => /\.png$/i.test(String(entry.value)))
    .filter((entry) => /\bbarony\b/i.test(String(entry.value)))
    .filter((entry) => !/barony-modloader/i.test(String(entry.value)))
    .filter((entry) => !/(?:^|[\\/])(logo|wordmark)\.(?:png|jpg|jpeg|gif)$/i.test(String(entry.value)))
    .filter((entry) => !/wordmark|librarycache/i.test(String(entry.value)))
    .filter((entry) => imagePathExists(entry.value));
}

function directActionLabelMatches(node, label) {
  const labelPattern = new RegExp(label.replace(/[.*+?^${}()|[\]\\]/g, "\\$&"), "i");
  return ["label", "title", "name", "text", "displayLabel", "visibleLabel", "ariaLabel"]
    .some((key) => typeof node[key] === "string" && labelPattern.test(node[key]));
}

function launchActionMetadataObjects(report, actionId, label) {
  const normalized = normalizeActionId(actionId);
  return allObjects(report).filter((entry) => actionIdForEntry(entry.node) === normalized || directActionLabelMatches(entry.node, label));
}

function requireLaunchActionIconPath(world, actionId, label, pathMatcher, description) {
  const report = ensureReport(world);
  const candidates = launchActionMetadataObjects(report, actionId, label);
  const matches = candidates.flatMap((entry) => pathMatcher(entry.node).map((pathEntry) => ({ entry, pathEntry })));
  if (!matches.length) {
    contractGap(
      world,
      `${label} lacks ${description} icon path metadata`,
      `ACTION METADATA CANDIDATES:\n${candidates.map((entry) => `${entry.pathParts.join(".")}: ${textForEntry(entry.node)}`).join("\n---\n") || "<none>"}`
    );
  }
}

function requireMockedLaunchButtonIconMetadata(world) {
  requireLaunchActionIconPath(world, "launch-bml", "Launch BML Barony", bmlIconPathEntries, "generated BML");
  requireLaunchActionIconPath(world, "launch-vanilla", "Launch Vanilla Barony", compactVanillaBaronyIconPathEntries, "vanilla Barony compact");
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

function packageIdsFromValue(value) {
  const ids = new Set();
  walk(value, (node) => {
    if (!node || typeof node !== "object" || Array.isArray(node)) return;
    for (const key of ["id", "packageId", "modId"]) {
      if (typeof node[key] === "string") ids.add(node[key]);
    }
  });
  return [...ids];
}

function packageManifest(packagePath) {
  return JSON.parse(fs.readFileSync(path.join(packagePath, "bml-package.json"), "utf8"));
}

function activeModFixture(packagePath) {
  const manifest = packageManifest(packagePath);
  return {
    id: manifest.id,
    name: manifest.name,
    version: manifest.version,
    packagePath,
    manifestPath: path.join(packagePath, "bml-package.json"),
    checksumSet: `bdd-fixture:${manifest.id}:${manifest.version}`,
    enabledAt: "2026-07-07T00:00:00Z",
  };
}

function writeActiveProfileFixture(world, activePackagePaths, profileId) {
  const xdgDataHome = path.join(world.guiButtonInteractionTempDir, "xdg-data-home");
  const profileDir = path.join(xdgDataHome, "BaronyModLoader", "profiles", "default");
  const bmlRoot = path.join(profileDir, "BaronyModLoader");
  const logsDir = path.join(bmlRoot, "logs");
  const reportsDir = path.join(bmlRoot, "reports");
  const manifestsDir = path.join(bmlRoot, "manifests");
  const stateDir = path.join(bmlRoot, "state");
  fs.mkdirSync(logsDir, { recursive: true });
  fs.mkdirSync(reportsDir, { recursive: true });
  fs.mkdirSync(manifestsDir, { recursive: true });
  fs.mkdirSync(stateDir, { recursive: true });

  const fakeInstallDir = path.join(world.guiButtonInteractionTempDir, "fake-steam", "Barony");
  const fakeExecutable = path.join(fakeInstallDir, "barony.x86_64");
  fs.mkdirSync(fakeInstallDir, { recursive: true });
  fs.writeFileSync(fakeExecutable, "#!/usr/bin/env sh\nexit 0\n", "utf8");
  fs.chmodSync(fakeExecutable, 0o755);

  const activeMods = activePackagePaths.map(activeModFixture);
  const profile = {
    schemaVersion: "0.1.0",
    profile: {
      id: profileId,
      createdAt: "2026-07-07T00:00:00Z",
      updatedAt: "2026-07-07T00:00:00Z",
    },
    app: {
      id: "BaronyModLoader",
      version: "0.1.0",
      schemaVersion: "0.1.0",
    },
    paths: {
      profileRoot: profileDir,
      bmlRoot,
      logs: logsDir,
      reports: reportsDir,
      manifests: manifestsDir,
      state: stateDir,
      runtimeManifest: path.join(bmlRoot, "runtime-manifest.json"),
    },
    activeMods,
    runtime: {
      gameSource: "manual",
      baronyExecutable: fakeExecutable,
      runtimeInfo: null,
      steam: null,
    },
  };
  fs.writeFileSync(path.join(bmlRoot, "profile.json"), `${JSON.stringify(profile, null, 2)}\n`, "utf8");
  fs.writeFileSync(
    path.join(bmlRoot, "active-mods.json"),
    `${JSON.stringify({ schemaVersion: "0.1.0", generatedAt: "2026-07-07T00:00:00Z", mods: activeMods }, null, 2)}\n`,
    "utf8"
  );
  return {
    xdgDataHome,
    profileDir,
    bmlRoot,
    runtimeManifestPath: path.join(bmlRoot, "runtime-manifest.json"),
    activeModsPath: path.join(bmlRoot, "active-mods.json"),
    activeIds: activeMods.map((mod) => mod.id),
    selectedPackageId: RUNEBOUND_ID,
  };
}

function writeSingleActiveRuneboundProfileFixture(world) {
  world.singleActiveProfileFixture = writeActiveProfileFixture(world, [RUNEBOUND_PKG_PATH], "bdd-single-active-profile");
  return world.singleActiveProfileFixture;
}

function writeMultipleActiveProfileFixture(world) {
  world.multipleActiveProfileFixture = writeActiveProfileFixture(world, [STASH_PKG_PATH, RUNEBOUND_PKG_PATH], "bdd-multiple-active-profile");
  return world.multipleActiveProfileFixture;
}

function requireLaunchBmlEvidence(world) {
  const report = ensureReport(world);
  const evidence = actionEvidenceEntries(report, "launch-bml");
  if (!evidence.length) {
    contractGap(world, "Expected launch-bml action evidence in smoke report.");
  }
  return { report, evidence, text: evidence.map(textForEntry).join("\n") };
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

const COPY_FOR_AI_ACTION_ID = "copy-for-ai";
const MIN_COPY_FOR_AI_PAYLOAD_CHARS = 600;
const COPY_FOR_AI_REQUIRED_CONTEXT_SECTIONS = [
  ["app id", /\b(appId|app id|launcher|application|BaronyModLoader|barony-mod-loader|BML)\b/i],
  ["app version", /\b(appVersion|applicationVersion|launcherVersion|bmlVersion|BaronyModLoader version|BML version|version)\b/i],
  ["schema", /\bschema(?:Version)?\b/i],
  ["generated timestamp", /\b(generatedAt|generated timestamp|timestamp|createdAt)\b/i],
  ["profile path", /\b(profilePath|profile path|profileDir|profileRoot)\b/i],
  ["profile state", /\b(profile|profileState|selectedProfile|activeProfile)\b/i],
  ["selected mod/package", /\b(selectedMod|selected mod|selectedPackage|selected package|packageId)\b/i],
  ["active mods", /\b(activeMods|active mods|enabledMods|activePackageIds)\b/i],
  ["environment", /\b(environment|platform|operating system|OS)\b/i],
  ["install", /\b(install|installPath|install summary|gamePath|baronyPath)\b/i],
  ["readiness", /\b(readiness|ready|blocked|disabledReasons)\b/i],
  ["diagnostics", /\b(diagnostics|diagnosticDetails|diagnosticsDetails|diagnosticsEvidence)\b/i],
  ["workshop", /\b(workshop|Steam Workshop|steamPublished|steam publish)\b/i],
  ["last launch", /\b(lastLaunch|last launch|launchResult|launchDryRun)\b/i],
  ["visible activity", /\b(visibleActivityLog|visible activity|Recent Activity|recentActivity)\b/i],
  ["full action log", /\b(actionLog|action log)\b/i],
  ["activity log details", /\b(activityLogDetails|activity log details|full activity details)\b/i],
];

function copyForAiMetadataRoots(report) {
  const roots = actionEvidenceEntries(report, COPY_FOR_AI_ACTION_ID)
    .filter((entry) => entry && typeof entry === "object")
    .map((entry) => ({ value: entry, pathParts: ["copy-for-ai-evidence"] }));
  for (const entry of valuesForKey(report, /(copyForAi|copiedAi|aiSupport|supportContext|issueContext|clipboard|copiedContext|contextBundle)/i)) {
    if (entry.value && typeof entry.value === "object") roots.push(entry);
  }
  return roots;
}

function copyForAiPayloadCandidates(report) {
  const candidates = [];
  const keyPattern = /^(copyForAiText|copiedText|copiedPayload|payload|context|contextText|clipboardText|clipboardPayload|aiSupportContext|supportContext|issueContext|contextBundle|text)$/i;
  const pathPattern = /(copyForAi|copiedAi|aiSupport|supportContext|issueContext|clipboard|copiedContext|contextBundle|payload|context)/i;
  for (const entry of valuesForKey(report, keyPattern)) {
    if (typeof entry.value !== "string" || !entry.value.trim()) continue;
    const joinedPath = entry.pathParts.join(".");
    if (pathPattern.test(joinedPath) || /context|payload|clipboard|copy/i.test(joinedPath)) {
      candidates.push({ text: entry.value, path: joinedPath });
    }
  }
  for (const root of copyForAiMetadataRoots(report)) {
    walk(root.value, (node, pathParts) => {
      if (typeof node !== "string" || !node.trim()) return;
      const joinedPath = [...root.pathParts, ...pathParts].join(".");
      const key = pathParts[pathParts.length - 1] || "";
      if (!keyPattern.test(key) || !pathPattern.test(joinedPath)) return;
      candidates.push({ text: node, path: joinedPath });
    });
  }
  return candidates.sort((left, right) => right.text.length - left.text.length);
}

function parseStructuredPayload(text) {
  const trimmed = String(text || "").trim();
  const candidates = [trimmed];
  const fenced = trimmed.match(/```(?:json)?\s*([\s\S]*?)```/i);
  if (fenced) candidates.push(fenced[1].trim());
  const firstObject = trimmed.indexOf("{");
  const lastObject = trimmed.lastIndexOf("}");
  if (firstObject >= 0 && lastObject > firstObject) candidates.push(trimmed.slice(firstObject, lastObject + 1));
  for (const candidate of candidates) {
    try {
      return JSON.parse(candidate);
    } catch (_error) {
      // Structured prose is allowed; JSON is preferred but not mandatory.
    }
  }
  return null;
}

function copyForAiPayloadInfo(world) {
  if (world.copyForAiPayloadInfo) return world.copyForAiPayloadInfo;
  const report = ensureReport(world);
  const candidates = copyForAiPayloadCandidates(report);
  const best = candidates[0];
  if (!best) {
    contractGap(world, "Copy for AI smoke report does not expose copied AI context text/payload metadata.");
  }
  const parsed = parseStructuredPayload(best.text);
  world.copyForAiPayloadInfo = {
    text: best.text,
    path: best.path,
    parsed,
    evidenceText: `${best.text}\n${parsed ? structuralText(parsed) : ""}`,
  };
  return world.copyForAiPayloadInfo;
}

function copyForAiNumericMetadata(report, pattern) {
  const values = [];
  for (const root of copyForAiMetadataRoots(report)) {
    walk(root.value, (node, pathParts) => {
      const key = pathParts[pathParts.length - 1] || "";
      if (!pattern.test(key)) return;
      const numeric = typeof node === "number" ? node : (/^\d+$/.test(String(node)) ? Number(node) : NaN);
      if (Number.isFinite(numeric) && numeric > 0) values.push({ value: numeric, path: [...root.pathParts, ...pathParts].join(".") });
    });
  }
  return values;
}

function copyForAiClipboardStatusMetadata(report) {
  const values = [];
  for (const root of copyForAiMetadataRoots(report)) {
    walk(root.value, (node, pathParts) => {
      const key = pathParts[pathParts.length - 1] || "";
      const pathText = [...root.pathParts, ...pathParts].join(".");
      if (!/clipboard/i.test(pathText) || !/(status|state|result|error|reason|available|copied|written)/i.test(key)) return;
      if (node === undefined || node === null || String(node).trim() === "") return;
      values.push({ value: node, path: pathText });
    });
  }
  return values;
}

function copyForAiLastCopyMetadata(report) {
  const direct = directField(report, "lastCopyForAi");
  if (direct && typeof direct === "object") return { value: direct, pathParts: ["lastCopyForAi"] };
  const match = valuesForKey(report, /^lastCopyForAi$/i).find((entry) => entry.value && typeof entry.value === "object");
  return match || null;
}

function copyStatusSucceeded(value) {
  if (value === true) return true;
  if (value && typeof value === "object") {
    return /(success|succeeded|copied|written|ok)/i.test(structuralText(value)) && !/(failed|error|timeout|unavailable|skipped)/i.test(structuralText(value));
  }
  return /(success|succeeded|copied|written|ok)/i.test(String(value || "")) && !/(failed|error|timeout|unavailable|skipped)/i.test(String(value || ""));
}

function copyForAiBackendMetadata(report) {
  const lastCopy = copyForAiLastCopyMetadata(report);
  if (!lastCopy) {
    return {
      lastCopy: null,
      backendEntries: [],
      tkStatuses: [],
      systemStatuses: [],
      mimeEntries: [],
      displayEnvEntries: [],
      readbackEntries: [],
      fallbackEntries: [],
    };
  }
  const backendEntries = [];
  const tkStatuses = [];
  const systemStatuses = [];
  const mimeEntries = [];
  const displayEnvEntries = [];
  const readbackEntries = [];
  const fallbackEntries = [];
  walk(lastCopy.value, (node, pathParts) => {
    if (node === undefined || node === null || String(node).trim() === "") return;
    const key = pathParts[pathParts.length - 1] || "";
    const pathText = [...lastCopy.pathParts, ...pathParts].join(".");
    if (/(clipboardBackends|backend|backends|backendResults|backendStatuses)$/i.test(key) || /clipboard.*backend/i.test(pathText)) {
      backendEntries.push({ value: node, path: pathText });
    }
    if (/^tkClipboardStatus$|tk.*clipboard.*(status|result|error|reason|copied|written)/i.test(key) || /tk.*clipboard.*(status|result|error|reason|copied|written)/i.test(pathText)) {
      tkStatuses.push({ value: node, path: pathText });
    }
    if (/^(systemClipboardStatus|externalClipboardStatus|persistentClipboardStatus|waylandClipboardStatus|wlCopyStatus)$/i.test(key) || /(system|external|persistent|wayland|wl-copy|wlCopy|xclip|xsel).*clipboard.*(status|result|error|reason|copied|written|readback|verified)|clipboard.*(system|external|persistent|wayland|wl-copy|wlCopy|xclip|xsel)/i.test(pathText)) {
      systemStatuses.push({ value: node, path: pathText });
    }
    if (/^(mimeType|mime|contentType|clipboardMimeType|systemClipboardMimeType|waylandClipboardMimeType|wlCopyMimeType|readbackMimeType)$/i.test(key) || (/^type$/i.test(key) && /\//.test(String(node))) || /(mime|contentType|content-type)/i.test(pathText)) {
      mimeEntries.push({ value: node, path: pathText });
    }
    if (/(WAYLAND_DISPLAY|XDG_RUNTIME_DIR|waylandDisplay|display|runtimeDir|socket|environment|env)/i.test(key) || /(WAYLAND_DISPLAY|XDG_RUNTIME_DIR|waylandDisplay|display|runtimeDir|socket|environment|env)/i.test(pathText)) {
      displayEnvEntries.push({ value: node, path: pathText });
    }
    if (/(verifiedReadback|readback|readBack|verification|verified)/i.test(key) || /(verifiedReadback|readback|readBack|verification|verified)/i.test(pathText)) {
      readbackEntries.push({ value: node, path: pathText });
    }
    if (/(fallbackCopy|fallback.*copy|copyFallback|fallback.*file|fallback.*path)/i.test(key) || /(fallbackCopy|fallback.*copy|copyFallback|fallback.*file|fallback.*path)/i.test(pathText)) {
      fallbackEntries.push({ value: node, path: pathText });
    }
  });
  return { lastCopy, backendEntries, tkStatuses, systemStatuses, mimeEntries, displayEnvEntries, readbackEntries, fallbackEntries };
}

function entriesText(entries) {
  return entries.map((entry) => `${entry.path}=${textForEntry(entry.value)}`).join("\n");
}

function copyForAiTextPlainMimeRecorded(backendMetadata) {
  return backendMetadata.mimeEntries.some((entry) => new RegExp(`\\b${CLIPBOARD_TEXT_MIME.replace("/", "\\/")}\\b`, "i").test(textForEntry(entry.value)));
}

function copyForAiReadbackEntrySucceeded(entry) {
  const pathText = entry && entry.path ? String(entry.path) : "";
  const value = entry ? entry.value : undefined;
  const evidenceText = `${pathText}\n${textForEntry(value)}`;
  if (!/(readback|readBack|verified|verification)/i.test(evidenceText)) return false;
  const decisivePattern = /^(verifiedReadback|readbackMatches|readbackSucceeded|readbackVerified|readbackOk|matchesPayload|payloadMatches)$/i;
  if (booleanFieldEvidence(value, decisivePattern, false).length > 0) return false;
  if (booleanFieldEvidence(value, decisivePattern, true).length > 0) return true;
  const statusEntries = valuesForKey(value, /^(readbackStatus|verificationStatus|verifyStatus)$/i);
  if (statusEntries.some((statusEntry) => /^(ok|success|succeeded|verified|matched?|readback[-_ ]?ok)\b/i.test(String(statusEntry.value || "")))) {
    return true;
  }
  if (typeof value === "string") {
    return /(verified|readback).*(ok|success|succeeded|matched?)|(ok|success|succeeded|matched?).*(verified|readback)/i.test(value);
  }
  return false;
}

function copyForAiSystemReadbackVerified(backendMetadata) {
  const systemPattern = /(system|external|persistent|wayland|wl-copy|wlCopy|xclip|xsel|clipboardBackends)/i;
  const candidates = [
    ...backendMetadata.systemStatuses,
    ...backendMetadata.backendEntries,
    ...backendMetadata.readbackEntries,
  ];
  return candidates.some((entry) => {
    const evidenceText = `${entry.path}\n${textForEntry(entry.value)}`;
    return systemPattern.test(evidenceText) && copyForAiReadbackEntrySucceeded(entry);
  });
}

function copyForAiFallbackCopyPathEntries(backendMetadata) {
  return backendMetadata.fallbackEntries.filter((entry) => {
    if (!/(file|path)$/i.test(entry.path)) return false;
    return typeof entry.value === "string" && entry.value.trim();
  });
}

function copyForAiFallbackCopyWritten(backendMetadata) {
  return backendMetadata.fallbackEntries.some((entry) => /written|succeeded|success|ok/i.test(entry.path) && entry.value === true);
}

function copyForAiPlainSuccessClaims(report) {
  const claims = [];
  walk(report, (node, pathParts) => {
    if (typeof node !== "string") return;
    if (node.trim() !== COPY_FOR_AI_VISIBLE_SUMMARY) return;
    const pathText = pathParts.join(".");
    if (!/(visibleSummary|summary|visibleActivityLog|recentActivity|activityLog|clipboardStatus|status|lastCopyForAi)/i.test(pathText)) return;
    claims.push({ value: node, path: pathText });
  });
  return claims;
}

function copyForAiFallbackCopyFailures(world, backendMetadata) {
  const payload = copyForAiPayloadInfo(world);
  const failures = [];
  if (!copyForAiFallbackCopyWritten(backendMetadata)) failures.push("lastCopyForAi does not record fallbackCopyWritten:true");
  const pathEntries = copyForAiFallbackCopyPathEntries(backendMetadata);
  if (!pathEntries.length) failures.push("lastCopyForAi does not record a fallbackCopyFile/fallbackCopyPath");
  const existingPathEntry = pathEntries.find((entry) => fs.existsSync(entry.value));
  if (!existingPathEntry && pathEntries.length) failures.push(`fallback copy path does not exist: ${pathEntries.map((entry) => entry.value).join(", ")}`);
  if (existingPathEntry) {
    const copiedText = fs.readFileSync(existingPathEntry.value, "utf8");
    const expected = normalizeClipboardProbeText(payload.text);
    const actual = normalizeClipboardProbeText(copiedText);
    if (actual !== expected) {
      failures.push(`fallback copy file does not contain the copied AI issue context payload (expected ${expected.length} chars, got ${actual.length} chars)`);
    }
  }
  return failures;
}

function copyForAiWaylandDisplayMetadataRecorded(backendMetadata, probe) {
  if (!backendMetadata.displayEnvEntries.length) return false;
  const text = entriesText(backendMetadata.displayEnvEntries);
  const expectedNeedles = [
    "WAYLAND_DISPLAY",
    "XDG_RUNTIME_DIR",
    "waylandDisplay",
    "display",
    "runtimeDir",
    "socket",
    "environment",
    "env",
    probe && probe.waylandDisplay,
    probe && probe.effectiveEnv && probe.effectiveEnv.WAYLAND_DISPLAY,
    probe && probe.xdgRuntimeDir,
    probe && probe.socketPath,
  ].filter(Boolean);
  return expectedNeedles.some((needle) => text.includes(String(needle)));
}

function copyForAiWaylandSystemMetadataFailures(backendMetadata, probe) {
  const failures = [];
  if (!copyForAiSystemReadbackVerified(backendMetadata)) {
    failures.push("system/external clipboard backend does not report verified readback metadata despite Wayland wl-paste being available");
  }
  if (!copyForAiTextPlainMimeRecorded(backendMetadata)) failures.push(`lastCopyForAi does not record ${CLIPBOARD_TEXT_MIME} MIME metadata for the system clipboard backend`);
  if (!copyForAiWaylandDisplayMetadataRecorded(backendMetadata, probe)) failures.push("lastCopyForAi does not record attempted Wayland display/env metadata for the system clipboard backend");
  return failures;
}

function waylandSocketCandidates(baseEnv = process.env) {
  const runtimeDirs = [
    baseEnv.XDG_RUNTIME_DIR || "",
    `/run/user/${typeof process.getuid === "function" ? process.getuid() : ""}`,
  ].filter(Boolean);
  const bySocketPath = new Map();
  for (const runtimeDir of runtimeDirs) {
    let names;
    try {
      names = fs.readdirSync(runtimeDir);
    } catch (_error) {
      continue;
    }
    for (const display of names.filter((name) => /^wayland-\d+$/.test(name))) {
      const socketPath = path.join(runtimeDir, display);
      try {
        if (!fs.statSync(socketPath).isSocket()) continue;
      } catch (_error) {
        continue;
      }
      bySocketPath.set(socketPath, { display, socketPath, runtimeDir });
    }
  }
  return [...bySocketPath.values()].sort((left, right) => left.display.localeCompare(right.display, "en", { numeric: true }));
}

function envWithToolPath(baseEnv = process.env) {
  return { ...baseEnv, PATH: `/usr/bin:${baseEnv.PATH || process.env.PATH || ""}` };
}

function filteredSystemPathWithoutClipboardTools(world, baseEnv = process.env) {
  const pathDir = path.join(world.guiButtonInteractionTempDir || os.tmpdir(), "no-system-clipboard-tools-bin");
  fs.mkdirSync(pathDir, { recursive: true });
  const sourcePath = baseEnv.PATH || process.env.PATH || "";
  const linkedNames = new Set();
  for (const dir of sourcePath.split(path.delimiter).filter(Boolean)) {
    let entries;
    try {
      entries = fs.readdirSync(dir);
    } catch (_error) {
      continue;
    }
    for (const entry of entries) {
      if (linkedNames.has(entry) || SYSTEM_CLIPBOARD_TOOL_NAMES.has(entry)) continue;
      const source = path.join(dir, entry);
      let stats;
      try {
        stats = fs.statSync(source);
        if (!stats.isFile() || (stats.mode & 0o111) === 0) continue;
      } catch (_error) {
        continue;
      }
      const target = path.join(pathDir, entry);
      try {
        fs.symlinkSync(source, target);
      } catch (_error) {
        try {
          fs.copyFileSync(source, target);
          fs.chmodSync(target, stats.mode | 0o111);
        } catch (_copyError) {
          continue;
        }
      }
      linkedNames.add(entry);
    }
  }
  return pathDir;
}

function envWithoutSystemClipboardTools(world, baseEnv = process.env) {
  const toolDir = filteredSystemPathWithoutClipboardTools(world, baseEnv);
  const python = process.env.PYTHON || findExecutable("python3", baseEnv.PATH || process.env.PATH || "") || "/usr/bin/python3";
  const env = { ...baseEnv, PATH: toolDir, PYTHON: python };
  return { env, toolDir, python };
}

function waylandWlPasteProbeEligibility(baseEnv = process.env) {
  const probeEnv = envWithToolPath(baseEnv);
  const wlPastePath = findExecutable("wl-paste", probeEnv.PATH);
  const wlCopyPath = findExecutable("wl-copy", probeEnv.PATH);
  const discoveredSockets = waylandSocketCandidates(probeEnv);
  const configuredDisplay = probeEnv.WAYLAND_DISPLAY || "";
  const configuredMatch = configuredDisplay ? discoveredSockets.find((candidate) => candidate.display === configuredDisplay) : null;
  const fallbackSocket = discoveredSockets.find((candidate) => candidate.display !== configuredDisplay) || discoveredSockets[0];
  let selectedSocket = configuredMatch || null;
  let displaySource = configuredDisplay ? "WAYLAND_DISPLAY" : "";
  if (!selectedSocket && fallbackSocket) {
    selectedSocket = fallbackSocket;
    displaySource = configuredDisplay ? "XDG_RUNTIME_DIR socket discovery after stale WAYLAND_DISPLAY" : "XDG_RUNTIME_DIR socket discovery";
  }
  const waylandDisplay = selectedSocket ? selectedSocket.display : configuredDisplay;
  const xdgRuntimeDir = selectedSocket ? selectedSocket.runtimeDir : (probeEnv.XDG_RUNTIME_DIR || "");
  const socketPath = selectedSocket ? selectedSocket.socketPath : "";
  const base = {
    wlPastePath,
    wlCopyPath,
    waylandDisplay,
    configuredDisplay,
    configuredDisplayMatched: Boolean(configuredMatch),
    xdgRuntimeDir,
    socketPath,
    displaySource,
    discoveredDisplays: discoveredSockets.map((candidate) => candidate.display),
    mimeType: CLIPBOARD_TEXT_MIME,
    effectiveEnv: {
      WAYLAND_DISPLAY: waylandDisplay,
      XDG_RUNTIME_DIR: xdgRuntimeDir,
    },
  };
  if (!waylandDisplay) return { ...base, eligible: false, reason: "WAYLAND_DISPLAY is not set and no /run/user/<uid>/wayland-* or XDG_RUNTIME_DIR/wayland-* socket is discoverable" };
  if (!wlPastePath) return { ...base, eligible: false, reason: "wl-paste is not on PATH" };
  return { ...base, eligible: true, reason: "wl-paste available with a Wayland display", wlPastePath, waylandDisplay };
}

function normalizeClipboardProbeText(value) {
  return String(value || "").replace(/\r\n/g, "\n").replace(/\r/g, "\n").replace(/\n+$/g, "");
}

function probeWaylandClipboardAfterCopy(world, baseEnv = process.env) {
  const eligibility = waylandWlPasteProbeEligibility(baseEnv);
  world.copyForAiWlPasteProbeBaseEnv = baseEnv;
  if (!eligibility.eligible) {
    world.copyForAiWlPasteProbe = { ...eligibility, skipped: true };
    return world.copyForAiWlPasteProbe;
  }
  const probeEnv = envWithToolPath(baseEnv);
  probeEnv.WAYLAND_DISPLAY = eligibility.waylandDisplay;
  if (eligibility.xdgRuntimeDir) probeEnv.XDG_RUNTIME_DIR = eligibility.xdgRuntimeDir;
  const args = ["--type", CLIPBOARD_TEXT_MIME, "--no-newline"];
  const probe = spawnSync(eligibility.wlPastePath, args, {
    cwd: REPO_ROOT,
    env: probeEnv,
    encoding: "utf8",
    timeout: 3000,
    maxBuffer: CLIPBOARD_PROBE_MAX_BUFFER,
  });
  world.copyForAiWlPasteProbe = {
    ...eligibility,
    skipped: false,
    args,
    status: probe.error && probe.error.code === "ETIMEDOUT" ? 124 : probe.status,
    signal: probe.signal,
    error: probe.error ? `${probe.error.code || probe.error.name || "error"}: ${probe.error.message}` : "",
    stdout: probe.stdout || "",
    stderr: probe.stderr || "",
  };
  return world.copyForAiWlPasteProbe;
}

function copyForAiSectionMetadata(report, payloadText) {
  const explicit = [];
  for (const root of copyForAiMetadataRoots(report)) {
    walk(root.value, (node, pathParts) => {
      const key = pathParts[pathParts.length - 1] || "";
      if (!/(includedSections|sectionsIncluded|contextSections|sectionNames|sections)$/i.test(key)) return;
      if (Array.isArray(node)) explicit.push(...node.map(String));
      else if (typeof node === "string") explicit.push(node);
    });
  }
  if (explicit.length) return explicit;
  return COPY_FOR_AI_REQUIRED_CONTEXT_SECTIONS
    .filter(([, pattern]) => pattern.test(payloadText))
    .map(([label]) => label);
}

function assertCopyForAiPayloadUseful(world) {
  const { text, evidenceText, path: payloadPath } = copyForAiPayloadInfo(world);
  const trimmed = String(text || "").trim();
  const failures = [];
  if (!trimmed) failures.push("payload is empty");
  if (trimmed.length < MIN_COPY_FOR_AI_PAYLOAD_CHARS) failures.push(`payload is too short (${trimmed.length} chars, expected at least ${MIN_COPY_FOR_AI_PAYLOAD_CHARS})`);
  if (trimmed === COPY_FOR_AI_VISIBLE_SUMMARY) failures.push("payload is only the visible activity summary, not the AI issue context");
  if (/\[object Object\]/i.test(trimmed)) failures.push("payload contains [object Object]");
  if (/<[^>\n]+ object at 0x[0-9a-f]+>/i.test(trimmed)) failures.push("payload contains Python object repr");
  if (/\b(?:PosixPath|WindowsPath)\(/.test(trimmed)) failures.push("payload contains Python pathlib repr");
  const missing = COPY_FOR_AI_REQUIRED_CONTEXT_SECTIONS
    .filter(([, pattern]) => !pattern.test(evidenceText))
    .map(([label]) => label);
  if (missing.length) failures.push(`missing issue-fix context sections: ${missing.join(", ")}`);
  if (failures.length) {
    contractGap(
      world,
      "Copied AI issue context is not pasteable/useful enough for issue fixing.",
      `PAYLOAD PATH: ${payloadPath}\nFAILURES:\n${failures.join("\n")}\nPAYLOAD PREVIEW:\n${trimmed.slice(0, 4000) || "<empty>"}`
    );
  }
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

When("I run the BaronyModLoader GUI with Copy for AI smoke button click", function () {
  const args = ["gui", "--smoke-clicks", COPY_FOR_AI_ACTION_ID, "--smoke-report", this.guiButtonInteractionReportPath];
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
  probeWaylandClipboardAfterCopy(this);
});

When("I run the BaronyModLoader GUI with Copy for AI smoke button click and no WAYLAND_DISPLAY", function () {
  const args = ["gui", "--smoke-clicks", COPY_FOR_AI_ACTION_ID, "--smoke-report", this.guiButtonInteractionReportPath];
  const appEnv = envWithToolPath(process.env);
  delete appEnv.WAYLAND_DISPLAY;
  const probeEligibility = waylandWlPasteProbeEligibility(appEnv);
  const skippedReasons = [];
  if (!probeEligibility.eligible) skippedReasons.push(probeEligibility.reason);
  if (!probeEligibility.wlCopyPath) skippedReasons.push("wl-copy is not on PATH");
  this.copyForAiWaylandDisplayAbsent = {
    appEnvHadWaylandDisplay: Boolean(process.env.WAYLAND_DISPLAY),
    skipped: skippedReasons.length > 0,
    reason: skippedReasons.join("; "),
    probeEligibility,
    appEnvWaylandDisplay: appEnv.WAYLAND_DISPLAY || "",
    xdgRuntimeDir: appEnv.XDG_RUNTIME_DIR || "",
  };
  this.guiButtonInteractionCommandLine = `env -u WAYLAND_DISPLAY ${BML_BIN} ${args.join(" ")}`;
  if (this.copyForAiWaylandDisplayAbsent.skipped) {
    this.copyForAiWlPasteProbe = { ...probeEligibility, skipped: true };
    return;
  }
  this.guiButtonInteractionCommand = spawnSync(BML_BIN, args, {
    cwd: REPO_ROOT,
    env: appEnv,
    encoding: "utf8",
    timeout: 90000,
  });
  if (this.guiButtonInteractionCommand.error && this.guiButtonInteractionCommand.error.code === "ETIMEDOUT") {
    this.guiButtonInteractionCommand.status = 124;
  }
  probeWaylandClipboardAfterCopy(this, appEnv);
});

When("I run the BaronyModLoader GUI with Copy for AI smoke button click and stale WAYLAND_DISPLAY", function () {
  const args = ["gui", "--smoke-clicks", COPY_FOR_AI_ACTION_ID, "--smoke-report", this.guiButtonInteractionReportPath];
  const appEnv = envWithToolPath(process.env);
  const discoveredDisplays = waylandSocketCandidates(appEnv).map((candidate) => candidate.display);
  let staleDisplay = `wayland-bml-stale-${process.pid}`;
  while (discoveredDisplays.includes(staleDisplay)) staleDisplay = `${staleDisplay}-x`;
  appEnv.WAYLAND_DISPLAY = staleDisplay;
  const probeEligibility = waylandWlPasteProbeEligibility(appEnv);
  const skippedReasons = [];
  if (!probeEligibility.eligible) skippedReasons.push(probeEligibility.reason);
  if (!probeEligibility.wlCopyPath) skippedReasons.push("wl-copy is not on PATH");
  if (!probeEligibility.discoveredDisplays.length || probeEligibility.waylandDisplay === staleDisplay) {
    skippedReasons.push("no fallback Wayland socket is discoverable after stale WAYLAND_DISPLAY");
  }
  this.copyForAiStaleWaylandDisplay = {
    staleDisplay,
    skipped: skippedReasons.length > 0,
    reason: skippedReasons.join("; "),
    probeEligibility,
    fallbackWaylandDisplay: probeEligibility.waylandDisplay,
    discoveredDisplays: probeEligibility.discoveredDisplays,
    xdgRuntimeDir: probeEligibility.xdgRuntimeDir || appEnv.XDG_RUNTIME_DIR || "",
  };
  this.guiButtonInteractionCommandLine = `WAYLAND_DISPLAY=${staleDisplay} ${BML_BIN} ${args.join(" ")}`;
  if (this.copyForAiStaleWaylandDisplay.skipped) {
    this.copyForAiWlPasteProbe = { ...probeEligibility, skipped: true };
    return;
  }
  this.guiButtonInteractionCommand = spawnSync(BML_BIN, args, {
    cwd: REPO_ROOT,
    env: appEnv,
    encoding: "utf8",
    timeout: 90000,
  });
  if (this.guiButtonInteractionCommand.error && this.guiButtonInteractionCommand.error.code === "ETIMEDOUT") {
    this.guiButtonInteractionCommand.status = 124;
  }
  probeWaylandClipboardAfterCopy(this, appEnv);
});

When("I run the BaronyModLoader GUI with Copy for AI smoke button click and system clipboard tools unavailable", function () {
  const args = ["gui", "--smoke-clicks", COPY_FOR_AI_ACTION_ID, "--smoke-report", this.guiButtonInteractionReportPath];
  const unavailable = envWithoutSystemClipboardTools(this, process.env);
  this.copyForAiSystemClipboardToolsUnavailable = {
    toolDir: unavailable.toolDir,
    python: unavailable.python,
    PATH: unavailable.env.PATH,
    WAYLAND_DISPLAY: unavailable.env.WAYLAND_DISPLAY || "",
    DISPLAY: unavailable.env.DISPLAY || "",
    XDG_RUNTIME_DIR: unavailable.env.XDG_RUNTIME_DIR || "",
  };
  this.copyForAiWlPasteProbeBaseEnv = unavailable.env;
  this.copyForAiWlPasteProbe = { skipped: true, reason: "system clipboard tools intentionally unavailable on PATH" };
  this.guiButtonInteractionCommandLine = `PATH=${unavailable.env.PATH} PYTHON=${unavailable.env.PYTHON} ${BML_BIN} ${args.join(" ")}`;
  this.guiButtonInteractionCommand = spawnSync(BML_BIN, args, {
    cwd: REPO_ROOT,
    env: unavailable.env,
    encoding: "utf8",
    timeout: 90000,
  });
  if (this.guiButtonInteractionCommand.error && this.guiButtonInteractionCommand.error.code === "ETIMEDOUT") {
    this.guiButtonInteractionCommand.status = 124;
  }
});

When("I run the BaronyModLoader GUI with mocked launch button clicks", function () {
  const fixture = writeSingleActiveRuneboundProfileFixture(this);
  const args = ["gui", "--smoke-clicks", "launch-bml,launch-vanilla", "--smoke-report", this.guiButtonInteractionReportPath];
  this.guiButtonInteractionCommandLine = `XDG_DATA_HOME=${fixture.xdgDataHome} BML_GUI_LAUNCH_MODE=mock ${BML_BIN} ${args.join(" ")}`;
  this.guiButtonInteractionCommand = spawnSync(BML_BIN, args, {
    cwd: REPO_ROOT,
    env: { ...process.env, PATH: `/usr/bin:${process.env.PATH || ""}`, BML_GUI_LAUNCH_MODE: "mock", XDG_DATA_HOME: fixture.xdgDataHome },
    encoding: "utf8",
    timeout: 90000,
  });
  if (this.guiButtonInteractionCommand.error && this.guiButtonInteractionCommand.error.code === "ETIMEDOUT") {
    this.guiButtonInteractionCommand.status = 124;
  }
});

When("I run the BaronyModLoader GUI Launch BML smoke with Stash and Runebound active but Runebound selected", function () {
  const fixture = writeMultipleActiveProfileFixture(this);
  const args = [
    "gui",
    "--smoke-clicks",
    "launch-bml",
    "--smoke-select-mod",
    RUNEBOUND_ID,
    "--smoke-report",
    this.guiButtonInteractionReportPath,
  ];
  const env = {
    ...process.env,
    PATH: `/usr/bin:${process.env.PATH || ""}`,
    BML_GUI_LAUNCH_MODE: "mock",
    XDG_DATA_HOME: fixture.xdgDataHome,
  };
  this.guiButtonInteractionCommandLine = `XDG_DATA_HOME=${fixture.xdgDataHome} BML_GUI_LAUNCH_MODE=mock ${BML_BIN} ${args.join(" ")}`;
  this.guiButtonInteractionCommand = spawnSync(BML_BIN, args, {
    cwd: REPO_ROOT,
    env,
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

Then("the BML launch smoke accepts the compatible multi-active profile without a multiple-package blocker", function () {
  const { evidence, text } = requireLaunchBmlEvidence(this);
  const invokedEntries = evidence.filter((entry) => entry && typeof entry === "object" && entry.id === "launch-bml");
  if (!invokedEntries.some((entry) => entry.invoked === true || /tk button\.invoke|button\.invoke|invoked/i.test(textForEntry(entry)))) {
    contractGap(this, "Expected Launch BML Barony to be invoked through its Tk button.", `ACTION EVIDENCE:\n${text}`);
  }
  if (/multiple active|more than one active|one active package|one package at a time|disable all but one|only one package/i.test(text)) {
    contractGap(this, "Launch BML Barony still reports the retired multiple-active-package blocker.", `ACTION EVIDENCE:\n${text}`);
  }
  if (/\bblocked\b/i.test(text)) {
    contractGap(this, "Compatible Stash + Runebound profile should not be blocked merely because multiple packages are active.", `ACTION EVIDENCE:\n${text}`);
  }
  requireActionBooleanEvidence(this, "launch-bml", /^mocked$/i, true, "mocked");
});

Then("the BML launch smoke writes a multi-mod runtime manifest for Stash and Runebound", function () {
  const { report, text } = requireLaunchBmlEvidence(this);
  const fixture = this.multipleActiveProfileFixture || {};
  const candidatePaths = [
    fixture.runtimeManifestPath,
    fixture.bmlRoot && path.join(fixture.bmlRoot, "manifests", "runtime-manifest.json"),
    ...collectPathStrings(report).filter((candidate) => /runtime-manifest\.json$/i.test(candidate)),
  ].filter(Boolean);
  const seenPaths = new Set();
  const manifests = [];
  for (const candidate of candidatePaths) {
    const manifestPath = path.resolve(String(candidate));
    if (seenPaths.has(manifestPath) || !fs.existsSync(manifestPath)) continue;
    seenPaths.add(manifestPath);
    const raw = fs.readFileSync(manifestPath, "utf8");
    try {
      manifests.push({ path: manifestPath, payload: JSON.parse(raw), raw });
    } catch (_error) {
      manifests.push({ path: manifestPath, payload: null, raw });
    }
  }
  const matchingManifest = manifests.find((manifest) => {
    const ids = packageIdsFromValue(manifest.payload);
    return ids.includes(STASH_ID) && ids.includes(RUNEBOUND_ID);
  });
  if (!matchingManifest) {
    contractGap(
      this,
      "Launch BML Barony did not write a runtime manifest representing both active packages.",
      `Candidate paths: ${JSON.stringify([...seenPaths])}\nManifest previews: ${JSON.stringify(manifests.map((manifest) => ({ path: manifest.path, ids: packageIdsFromValue(manifest.payload), preview: manifest.raw.slice(0, 1000) })), null, 2)}\nACTION EVIDENCE:\n${text}`
    );
  }

  const activeModsPath = fixture.activeModsPath;
  if (!activeModsPath || !fs.existsSync(activeModsPath)) {
    contractGap(this, "Launch BML Barony did not preserve/write active-mods modlist evidence.", `activeModsPath: ${activeModsPath || "<missing>"}\nACTION EVIDENCE:\n${text}`);
  }
  const activeMods = JSON.parse(fs.readFileSync(activeModsPath, "utf8"));
  const activeIds = packageIdsFromValue(activeMods);
  for (const packageId of [STASH_ID, RUNEBOUND_ID]) {
    if (!activeIds.includes(packageId)) {
      contractGap(this, `Launch BML Barony active-mods modlist evidence does not include ${packageId}.`, `activeModsPath: ${activeModsPath}\nactiveIds: ${JSON.stringify(activeIds)}\nPayload:\n${JSON.stringify(activeMods, null, 2)}`);
    }
  }
});

Then("mocked launch button metadata includes generated BML and vanilla Barony icon paths", function () {
  requireMockedLaunchButtonIconMetadata(this);
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

Then("the Copy for AI smoke invoked the Tk button without launching Barony", function () {
  const report = ensureReport(this);
  const entries = requireClickedAction(this, COPY_FOR_AI_ACTION_ID);
  if (!entries.some((entry) => entry && entry.invoked === true)) {
    contractGap(this, "copy-for-ai clickedActions entry does not report invoked:true.", `ACTIONS:\n${entries.map(textForEntry).join("\n---\n")}`);
  }
  requireLaunchTkInvocation(this, COPY_FOR_AI_ACTION_ID);
  for (const { id, label } of LAUNCH_ACTIONS) {
    if (actionEntries(report, id).length > 0) {
      contractGap(this, `Copy for AI smoke must not invoke ${label}.`);
    }
  }
  assertNoTrueSideEffect(this, /^processStarted$|^processLaunched$|baronyStarted|gameStarted|startedProcess/i, "Barony process start");
});

Then("the Copy for AI smoke report exposes non-empty copied AI issue context metadata", function () {
  const report = ensureReport(this);
  const payload = copyForAiPayloadInfo(this);
  const charCounts = copyForAiNumericMetadata(report, /(char(?:acter)?Count|chars|payloadChars|copiedChars|clipboardChars)$/i);
  const byteCounts = copyForAiNumericMetadata(report, /(byteCount|bytes|payloadBytes|copiedBytes|clipboardBytes)$/i);
  const clipboardStatuses = copyForAiClipboardStatusMetadata(report);
  const sectionNames = copyForAiSectionMetadata(report, payload.evidenceText);
  const backendMetadata = copyForAiBackendMetadata(report);
  const failures = [];
  if (!payload.text.trim()) failures.push("missing non-empty copied context payload text");
  if (payload.text.trim() === COPY_FOR_AI_VISIBLE_SUMMARY) failures.push("copied payload text is only the visible summary");
  if (!charCounts.length && !byteCounts.length) failures.push("missing copied payload char/byte count metadata");
  if (!clipboardStatuses.length) failures.push("missing clipboard status/error metadata");
  if (!backendMetadata.lastCopy) failures.push("missing lastCopyForAi object");
  if (!backendMetadata.backendEntries.length) failures.push("lastCopyForAi is missing clipboardBackends/backend attempts metadata");
  if (!backendMetadata.tkStatuses.length) failures.push("lastCopyForAi is missing tkClipboardStatus metadata");
  if (!backendMetadata.systemStatuses.length) failures.push("lastCopyForAi is missing systemClipboardStatus/external clipboard metadata");
  if (sectionNames.length < 8) failures.push(`missing included section names metadata (found ${sectionNames.length})`);
  if (failures.length) {
    contractGap(
      this,
      "Copy for AI smoke report does not expose enough clipboard/copy proof metadata.",
      [
        `FAILURES:\n${failures.join("\n")}`,
        `PAYLOAD PATH: ${payload.path}`,
        `CHAR COUNTS: ${charCounts.map((entry) => `${entry.path}=${entry.value}`).join(", ") || "<none>"}`,
        `BYTE COUNTS: ${byteCounts.map((entry) => `${entry.path}=${entry.value}`).join(", ") || "<none>"}`,
        `CLIPBOARD STATUS: ${clipboardStatuses.map((entry) => `${entry.path}=${entry.value}`).join(", ") || "<none>"}`,
        `LAST COPY: ${backendMetadata.lastCopy ? textForEntry(backendMetadata.lastCopy.value) : "<none>"}`,
        `BACKENDS: ${backendMetadata.backendEntries.map((entry) => `${entry.path}=${textForEntry(entry.value)}`).join(", ") || "<none>"}`,
        `TK STATUS: ${backendMetadata.tkStatuses.map((entry) => `${entry.path}=${textForEntry(entry.value)}`).join(", ") || "<none>"}`,
        `SYSTEM STATUS: ${backendMetadata.systemStatuses.map((entry) => `${entry.path}=${textForEntry(entry.value)}`).join(", ") || "<none>"}`,
        `SECTIONS: ${sectionNames.join(", ") || "<none>"}`,
      ].join("\n")
    );
  }
});

Then("the Copy for AI smoke report proves verified backend clipboard readback when available", function () {
  const report = ensureReport(this);
  const payload = copyForAiPayloadInfo(this);
  const backendMetadata = copyForAiBackendMetadata(report);
  const failures = [];
  const tkSucceeded = backendMetadata.tkStatuses.some((entry) => copyStatusSucceeded(entry.value));
  if (!tkSucceeded) failures.push("Tk clipboard backend was not reported as successful");
  if (!backendMetadata.systemStatuses.length) failures.push("system/external clipboard backend was not attempted or reported");
  let probe = this.copyForAiWlPasteProbe || waylandWlPasteProbeEligibility(this.copyForAiWlPasteProbeBaseEnv || process.env);
  if (probe.eligible && !probe.skipped) {
    const expected = normalizeClipboardProbeText(payload.text);
    let actual = normalizeClipboardProbeText(probe.stdout);
    if (probe.status !== 0 || actual !== expected) {
      const refreshedProbe = probeWaylandClipboardAfterCopy(this, this.copyForAiWlPasteProbeBaseEnv || process.env);
      const refreshedActual = normalizeClipboardProbeText(refreshedProbe.stdout);
      if (refreshedProbe.status === 0 && refreshedActual === expected) {
        probe = refreshedProbe;
        actual = refreshedActual;
      }
    }
    if (probe.status !== 0) {
      failures.push(`wl-paste --type ${CLIPBOARD_TEXT_MIME} failed after smoke click (status ${probe.status === null || probe.status === undefined ? "<unset>" : probe.status})`);
    }
    if (actual === COPY_FOR_AI_VISIBLE_SUMMARY) {
      failures.push(`wl-paste --type ${CLIPBOARD_TEXT_MIME} returned only the visible summary instead of the AI issue context`);
    }
    if (actual !== expected) {
      failures.push(`wl-paste --type ${CLIPBOARD_TEXT_MIME} did not return the full report payload (expected ${expected.length} chars, got ${actual.length} chars)`);
    }
    failures.push(...copyForAiWaylandSystemMetadataFailures(backendMetadata, probe));
  }
  if (failures.length) {
    contractGap(
      this,
      "Copy for AI smoke did not prove verified persistent user-visible text/plain clipboard readback.",
      [
        `FAILURES:\n${failures.join("\n")}`,
        `WL-PASTE PROBE: ${textForEntry(probe)}`,
        `LAST COPY: ${backendMetadata.lastCopy ? textForEntry(backendMetadata.lastCopy.value) : "<none>"}`,
        `SYSTEM STATUS: ${entriesText(backendMetadata.systemStatuses) || "<none>"}`,
        `MIME METADATA: ${entriesText(backendMetadata.mimeEntries) || "<none>"}`,
        `DISPLAY/ENV METADATA: ${entriesText(backendMetadata.displayEnvEntries) || "<none>"}`,
        `READBACK METADATA: ${entriesText(backendMetadata.readbackEntries) || "<none>"}`,
        `PAYLOAD PATH: ${payload.path}`,
      ].join("\n")
    );
  }
});

Then(/^the no-WAYLAND_DISPLAY Copy for AI smoke either gracefully skips on unavailable Wayland probing or records verified text\/plain system clipboard readback$/, function () {
  const attempt = this.copyForAiWaylandDisplayAbsent;
  if (!attempt) {
    contractGap(this, "No-WAYLAND_DISPLAY Copy for AI smoke step did not record its Wayland probing decision.");
  }
  if (attempt.skipped) {
    if (!/(WAYLAND_DISPLAY|XDG_RUNTIME_DIR|wayland|socket|wl-paste|wl-copy)/i.test(attempt.reason || "")) {
      contractGap(this, "No-WAYLAND_DISPLAY Copy for AI smoke skipped for a reason unrelated to Wayland/wl-copy/wl-paste availability.", textForEntry(attempt));
    }
    return;
  }
  if (attempt.appEnvWaylandDisplay) {
    contractGap(this, "No-WAYLAND_DISPLAY Copy for AI smoke did not actually remove WAYLAND_DISPLAY from the app environment.", textForEntry(attempt));
  }
  const report = ensureReport(this);
  const backendMetadata = copyForAiBackendMetadata(report);
  const probe = this.copyForAiWlPasteProbe || {};
  const failures = copyForAiWaylandSystemMetadataFailures(backendMetadata, probe);
  if (failures.length || !probe.eligible || probe.skipped) {
    contractGap(
      this,
      "Copy for AI did not prove Wayland socket discovery and verified text/plain system clipboard readback when WAYLAND_DISPLAY was absent.",
      [
        `FAILURES:\n${failures.join("\n") || "<none>"}`,
        `NO-WAYLAND ATTEMPT: ${textForEntry(attempt)}`,
        `WL-PASTE PROBE: ${textForEntry(probe)}`,
        `LAST COPY: ${backendMetadata.lastCopy ? textForEntry(backendMetadata.lastCopy.value) : "<none>"}`,
      ].join("\n")
    );
  }
  const payload = copyForAiPayloadInfo(this);
  const expected = normalizeClipboardProbeText(payload.text);
  const actual = normalizeClipboardProbeText(probe.stdout);
  if (probe.status !== 0 || actual !== expected) {
    contractGap(
      this,
      `Copy for AI no-WAYLAND_DISPLAY smoke did not leave ${CLIPBOARD_TEXT_MIME} raw clipboard text available through wl-paste.`,
      `WL-PASTE PROBE: ${textForEntry(probe)}\nExpected chars: ${expected.length}\nActual chars: ${actual.length}`
    );
  }
});

Then(/^the stale-WAYLAND_DISPLAY Copy for AI smoke either gracefully skips on unavailable Wayland probing or records verified text\/plain system clipboard readback from a discovered fallback$/, function () {
  const attempt = this.copyForAiStaleWaylandDisplay;
  if (!attempt) {
    contractGap(this, "Stale-WAYLAND_DISPLAY Copy for AI smoke step did not record its Wayland probing decision.");
  }
  if (attempt.skipped) {
    if (!/(WAYLAND_DISPLAY|XDG_RUNTIME_DIR|wayland|socket|wl-paste|wl-copy)/i.test(attempt.reason || "")) {
      contractGap(this, "Stale-WAYLAND_DISPLAY Copy for AI smoke skipped for a reason unrelated to Wayland/wl-copy/wl-paste availability.", textForEntry(attempt));
    }
    return;
  }
  if (!attempt.fallbackWaylandDisplay || attempt.fallbackWaylandDisplay === attempt.staleDisplay) {
    contractGap(this, "Stale-WAYLAND_DISPLAY Copy for AI smoke did not select a discovered fallback Wayland display.", textForEntry(attempt));
  }
  const report = ensureReport(this);
  const backendMetadata = copyForAiBackendMetadata(report);
  const probe = this.copyForAiWlPasteProbe || {};
  const failures = copyForAiWaylandSystemMetadataFailures(backendMetadata, probe);
  const displayText = entriesText(backendMetadata.displayEnvEntries);
  if (!displayText.includes(attempt.staleDisplay)) failures.push(`lastCopyForAi does not record the stale WAYLAND_DISPLAY attempt (${attempt.staleDisplay})`);
  if (!displayText.includes(attempt.fallbackWaylandDisplay)) failures.push(`lastCopyForAi does not record the discovered fallback WAYLAND_DISPLAY (${attempt.fallbackWaylandDisplay})`);
  if (failures.length || !probe.eligible || probe.skipped) {
    contractGap(
      this,
      "Copy for AI did not prove fallback from stale WAYLAND_DISPLAY to discovered Wayland socket with verified text/plain readback.",
      [
        `FAILURES:\n${failures.join("\n") || "<none>"}`,
        `STALE-WAYLAND ATTEMPT: ${textForEntry(attempt)}`,
        `WL-PASTE PROBE: ${textForEntry(probe)}`,
        `LAST COPY: ${backendMetadata.lastCopy ? textForEntry(backendMetadata.lastCopy.value) : "<none>"}`,
        `DISPLAY/ENV METADATA: ${displayText || "<none>"}`,
        `READBACK METADATA: ${entriesText(backendMetadata.readbackEntries) || "<none>"}`,
      ].join("\n")
    );
  }
  const payload = copyForAiPayloadInfo(this);
  const expected = normalizeClipboardProbeText(payload.text);
  const actual = normalizeClipboardProbeText(probe.stdout);
  if (probe.status !== 0 || actual !== expected) {
    contractGap(
      this,
      `Copy for AI stale-WAYLAND_DISPLAY smoke did not leave ${CLIPBOARD_TEXT_MIME} raw clipboard text available through wl-paste on the fallback display.`,
      `WL-PASTE PROBE: ${textForEntry(probe)}\nExpected chars: ${expected.length}\nActual chars: ${actual.length}`
    );
  }
});

Then("the Copy for AI smoke writes a fallback copy file and does not claim plain clipboard success", function () {
  const report = ensureReport(this);
  const backendMetadata = copyForAiBackendMetadata(report);
  const failures = copyForAiFallbackCopyFailures(this, backendMetadata);
  const plainClaims = copyForAiPlainSuccessClaims(report);
  if (copyForAiSystemReadbackVerified(backendMetadata)) {
    failures.push("system clipboard unexpectedly reports verified readback in the system-tools-unavailable failure scenario");
  }
  if (plainClaims.length) {
    failures.push(`plain success claim found despite missing verified system readback: ${entriesText(plainClaims)}`);
  }
  if (!backendMetadata.systemStatuses.length) failures.push("lastCopyForAi is missing system clipboard status/details for the failed system backend path");
  if (failures.length) {
    contractGap(
      this,
      "Copy for AI failure/partial path did not write a durable fallback file or avoid a plain success claim.",
      [
        `FAILURES:\n${failures.join("\n")}`,
        `TOOLS-UNAVAILABLE ATTEMPT: ${textForEntry(this.copyForAiSystemClipboardToolsUnavailable || {})}`,
        `LAST COPY: ${backendMetadata.lastCopy ? textForEntry(backendMetadata.lastCopy.value) : "<none>"}`,
        `SYSTEM STATUS: ${entriesText(backendMetadata.systemStatuses) || "<none>"}`,
        `FALLBACK METADATA: ${entriesText(backendMetadata.fallbackEntries) || "<none>"}`,
        `PLAIN CLAIMS: ${entriesText(plainClaims) || "<none>"}`,
      ].join("\n")
    );
  }
});

Then("copied AI issue context includes issue-fix essentials and rejects useless payloads", function () {
  assertCopyForAiPayloadUseful(this);
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
