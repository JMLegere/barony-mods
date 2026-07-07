"use strict";

const { After, Given, Then, When } = require("@cucumber/cucumber");
const { spawnSync } = require("child_process");
const fs = require("fs");
const os = require("os");
const path = require("path");

// paths: <repo>/framework/BaronyModLoader/features/steps/gui_profile_first_steps.js
//          ../../../../ from here = <repo>
const REPO_ROOT = path.resolve(__dirname, "../../../../");
const BML_BIN = path.join(REPO_ROOT, "framework/BaronyModLoader/bin/barony-mod-loader");
const RUNEBOUND_ID = "jml.runebound-elixirs";
const RUNEBOUND_NAME = "Runebound: Elixirs";
const WORKSHOP_FIXTURE_ID = "2999999999";
const WORKSHOP_FIXTURE_TITLE = "Cucumber Workshop Fixture";
const WORKSHOP_FIXTURE_NOTE = "non-BML Workshop subscription fixture";
const STASH_ID = "jml.stash";
const STASH_NAME = "Stash";
const SMOKE_SELECT_MOD = RUNEBOUND_ID;

const CONCEPT_LABELS = ["Environment", "Profiles", "Mods", "Workshop"];
const FORBIDDEN_TOP_LEVEL_LABELS = ["Actions", "Views", "Diagnostics", "Windows Status", "Launch Dry Run"];
const CONCEPT_EXPECTED_ACTIONS = {
  Environment: {
    primary: /launch[\s\S]{0,80}(baronymodloader|barony mod loader|bml)|launch-bml/i,
    secondary: [/launch[\s\S]{0,80}vanilla[\s\S]{0,40}barony|launch-vanilla/i, /detect[\s\S]{0,40}install|install[\s\S]{0,40}detect/i, /refresh[\s\S]{0,40}readiness|readiness[\s\S]{0,40}refresh/i, /open[\s\S]{0,40}diagnostics|diagnostics[\s\S]{0,40}open/i],
  },
  Profiles: {
    primary: /create[\s\S]{0,80}profile|select[\s\S]{0,80}profile|profile[\s\S]{0,80}(create|select)/i,
    secondary: [],
  },
  Mods: {
    primary: /enable[\s\S]{0,80}(selected[\s\S]{0,40})?(mod|package|Runebound)|Runebound[\s\S]{0,80}enable/i,
    secondary: [/scan[\s\S]{0,40}(packages?|mods|library)|(?:packages?|mods|library)[\s\S]{0,40}scan/i, /disable[\s\S]{0,80}(selected[\s\S]{0,40})?(mod|package|Runebound)|Runebound[\s\S]{0,80}disable/i],
  },
  Workshop: {
    primary: /preview[\s\S]{0,80}(workshop|dry[- ]?run)|workshop[\s\S]{0,80}preview/i,
    secondary: [],
  },
};

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

function stringsFromValues(value) {
  const strings = [];
  walk(value, (node) => {
    if (typeof node === "string") strings.push(node);
    else if (typeof node === "number" || typeof node === "boolean") strings.push(String(node));
  });
  return strings;
}

function structuralText(value) {
  const parts = [];
  walk(value, (node, pathParts) => {
    if (pathParts.length) parts.push(pathParts[pathParts.length - 1]);
    if (typeof node === "string" || typeof node === "number" || typeof node === "boolean") parts.push(String(node));
  });
  return parts.join("\n");
}

function objectText(value) {
  return structuralText(value).replace(/\s+/g, " ").trim();
}

function allObjects(value) {
  const objects = [];
  walk(value, (node, pathParts) => {
    if (node && typeof node === "object" && !Array.isArray(node)) objects.push({ node, pathParts });
  });
  return objects;
}

function valuesForKey(value, keyPattern) {
  const values = [];
  walk(value, (node, pathParts) => {
    const key = pathParts[pathParts.length - 1] || "";
    if (keyPattern.test(key)) values.push({ value: node, pathParts });
  });
  return values;
}

function countInArray(array, expected) {
  return array.filter((item) => String(item) === expected).length;
}

function commandExplanation(world) {
  const command = world.profileFirstGuiCommand || {};
  return `ARGV: ${[BML_BIN, "gui", ...(command.args || [])].join(" ")}\nEXIT: ${command.status}\nSIGNAL: ${command.signal || ""}\nSTDOUT:\n${command.stdout || ""}\nSTDERR:\n${command.stderr || ""}`;
}

function reportPreview(world) {
  if (world.profileFirstGuiReportRaw) return world.profileFirstGuiReportRaw;
  if (world.profileFirstGuiReport) return JSON.stringify(world.profileFirstGuiReport, null, 2);
  return "<no smoke report loaded>";
}

function contractGap(world, message, extra = "") {
  throw new Error(
    `PROFILE-FIRST GUI CONTRACT GAP: ${message}\n` +
    `Smoke report path: ${world.profileFirstGuiReportPath || "<unset>"}\n` +
    (extra ? `${extra}\n` : "") +
    `REPORT:\n${reportPreview(world)}\n${commandExplanation(world)}`
  );
}

function reportShowsOpened(report) {
  let opened = false;
  walk(report, (node, pathParts) => {
    const key = pathParts[pathParts.length - 1] || "";
    if (/^(opened|guiOpened|rootOpened|tkRootOpened|windowOpened|visibleRootOpened)$/i.test(key) && node === true) opened = true;
    if (/^(status|state|result)$/i.test(key) && typeof node === "string" && /^(opened|open|visible)$/i.test(node)) opened = true;
  });
  return opened;
}

function normalizeLabel(value) {
  return String(value || "")
    .replace(/[_-]+/g, " ")
    .replace(/\s+/g, " ")
    .trim()
    .toLowerCase();
}

function canonicalConceptLabel(value) {
  const normalized = normalizeLabel(value);
  if (/^environment$/.test(normalized)) return "Environment";
  if (/^profiles?$/.test(normalized)) return "Profiles";
  if (/^(mods?|packages?|package library|local mods)$/.test(normalized)) return "Mods";
  if (/^(workshop|workshop prep|steam workshop)$/.test(normalized)) return "Workshop";
  return null;
}

function displayLabelForObject(node, fallback = "") {
  if (typeof node === "string") return node;
  if (!node || typeof node !== "object") return fallback;
  for (const key of ["label", "title", "name", "concept", "conceptLabel", "section", "id", "key"]) {
    if (typeof node[key] === "string" && node[key].trim()) return node[key];
  }
  return fallback;
}

function meaningfulObject(entry) {
  const node = entry.node;
  if (!node || typeof node !== "object") return false;
  const keys = Object.keys(node).filter((key) => !/^(key|id|label|title|name|concept|conceptLabel|section)$/i.test(key));
  return keys.length > 0 && objectText(node).split(/\s+/).filter(Boolean).length >= 4;
}

function collectTopLevelSectionEntries(report) {
  const entries = [];
  const addEntry = (node, pathParts, fallback = "") => {
    const label = displayLabelForObject(node, fallback);
    if (!label) return;
    entries.push({ label, node: typeof node === "object" && node !== null ? node : { label }, pathParts });
  };

  for (const key of ["conceptCards", "concepts", "cards", "sections", "widgets"]) {
    const value = report && report[key];
    if (Array.isArray(value)) {
      value.forEach((item, index) => addEntry(item, [key, String(index)], typeof item === "string" ? item : ""));
    } else if (value && typeof value === "object" && key !== "sections" && key !== "widgets") {
      for (const [entryKey, item] of Object.entries(value)) addEntry(item, [key, entryKey], entryKey);
    }
  }

  if (Array.isArray(report && report.renderedSectionLabels)) {
    report.renderedSectionLabels.forEach((label, index) => {
      const normalized = normalizeLabel(label);
      if (!entries.some((entry) => normalizeLabel(entry.label) === normalized)) {
        addEntry({ label }, ["renderedSectionLabels", String(index)], label);
      }
    });
  }

  return entries;
}

function topLevelLabels(report) {
  if (Array.isArray(report && report.renderedSectionLabels) && report.renderedSectionLabels.length) {
    return report.renderedSectionLabels.map((label) => String(label));
  }
  const labels = [];
  for (const entry of collectTopLevelSectionEntries(report)) {
    const label = String(entry.label || "").trim();
    if (label && !labels.some((existing) => normalizeLabel(existing) === normalizeLabel(label))) labels.push(label);
  }
  return labels;
}

function requireConceptEntry(world, conceptLabel) {
  const target = normalizeLabel(conceptLabel);
  const entries = collectTopLevelSectionEntries(world.profileFirstGuiReport).filter((entry) => {
    const canonical = canonicalConceptLabel(entry.label);
    return canonical && normalizeLabel(canonical) === target;
  });
  const directKey = conceptLabel.toLowerCase();
  const direct = world.profileFirstGuiReport && world.profileFirstGuiReport[directKey];
  if (direct && typeof direct === "object") entries.push({ label: conceptLabel, node: direct, pathParts: [directKey] });
  if (conceptLabel === "Profiles" && world.profileFirstGuiReport && world.profileFirstGuiReport.profiles) entries.push({ label: conceptLabel, node: world.profileFirstGuiReport.profiles, pathParts: ["profiles"] });
  if (conceptLabel === "Mods" && world.profileFirstGuiReport && world.profileFirstGuiReport.mods) entries.push({ label: conceptLabel, node: world.profileFirstGuiReport.mods, pathParts: ["mods"] });
  const rich = entries.find(meaningfulObject) || entries[0];
  if (!rich) {
    contractGap(
      world,
      `missing top-level ${conceptLabel} concept card`,
      `Top-level labels found: ${topLevelLabels(world.profileFirstGuiReport).join(", ") || "<none>"}`
    );
  }
  return rich;
}

function conceptText(world, conceptLabel) {
  const entry = requireConceptEntry(world, conceptLabel);
  return objectText(entry.node);
}

function requireConceptPatterns(world, conceptLabel, checks) {
  const text = conceptText(world, conceptLabel);
  const missing = checks.filter(([, pattern]) => !pattern.test(text)).map(([label]) => label);
  if (missing.length) {
    contractGap(world, `${conceptLabel} concept is missing: ${missing.join(", ")}`, `${conceptLabel.toUpperCase()} EVIDENCE:\n${text || "<none>"}`);
  }
  return text;
}

function topLevelForbiddenLabels(report) {
  const labels = topLevelLabels(report);
  return labels.filter((label) => FORBIDDEN_TOP_LEVEL_LABELS.some((forbidden) => normalizeLabel(label) === normalizeLabel(forbidden)));
}

function rootActionContainers(report) {
  const containers = [];
  if (!report || typeof report !== "object") return containers;
  for (const key of ["actions", "buttons", "controls", "toolbar", "actionStrip", "flatActions", "commands"]) {
    const value = report[key];
    if (Array.isArray(value) && value.length) containers.push({ key, value });
  }
  return containers;
}

function actionText(action) {
  if (typeof action === "string") return action;
  return objectText(action);
}

function actionScope(action) {
  if (!action || typeof action !== "object") return "";
  return String(action.concept || action.conceptKey || action.card || action.section || action.parentConcept || "");
}

function actionRank(action) {
  if (!action || typeof action !== "object") return "";
  return String(action.role || action.rank || action.priority || action.variant || action.style || action.hierarchy || action.importance || "");
}

function isPrimaryAction(action) {
  if (!action) return false;
  if (typeof action === "string") return false;
  const rank = actionRank(action);
  if (/primary|main|dominant/i.test(rank)) return true;
  const keyText = Object.keys(action).join(" ");
  return /primaryAction|primaryButton|primaryCta/i.test(keyText) && !/secondary/i.test(keyText);
}

function isSecondaryAction(action) {
  if (!action) return false;
  if (typeof action === "string") return false;
  return /secondary|tertiary|supporting/i.test(actionRank(action));
}

function arraysForKeys(value, keyPattern) {
  const arrays = [];
  walk(value, (node, pathParts) => {
    const key = pathParts[pathParts.length - 1] || "";
    if (Array.isArray(node) && keyPattern.test(key)) arrays.push({ value: node, pathParts });
  });
  return arrays;
}

function conceptActionArrays(conceptNode) {
  return arraysForKeys(conceptNode, /^(actions|buttons|controls|secondaryActions|secondaryButtons|secondaryCtas)$/i);
}

function conceptPrimaryActionTexts(conceptNode) {
  const texts = [];
  for (const entry of valuesForKey(conceptNode, /^(primaryAction|primaryButton|primaryCta)$/i)) {
    if (entry.value !== null && entry.value !== undefined && String(actionText(entry.value)).trim()) texts.push(actionText(entry.value));
  }
  for (const arrayEntry of conceptActionArrays(conceptNode)) {
    for (const action of arrayEntry.value) {
      if (isPrimaryAction(action)) texts.push(actionText(action));
    }
  }
  return texts;
}

function conceptSecondaryActionTexts(conceptNode) {
  const texts = [];
  for (const entry of valuesForKey(conceptNode, /^(secondaryActions|secondaryButtons|secondaryCtas)$/i)) {
    if (Array.isArray(entry.value)) {
      for (const action of entry.value) if (String(actionText(action)).trim()) texts.push(actionText(action));
    } else if (entry.value !== null && entry.value !== undefined && String(actionText(entry.value)).trim()) {
      texts.push(actionText(entry.value));
    }
  }
  for (const arrayEntry of conceptActionArrays(conceptNode)) {
    for (const action of arrayEntry.value) {
      if (isSecondaryAction(action)) texts.push(actionText(action));
    }
  }
  return texts;
}

function conceptExposesActionMetadata(conceptNode) {
  if (conceptPrimaryActionTexts(conceptNode).length || conceptSecondaryActionTexts(conceptNode).length) return true;
  return conceptActionArrays(conceptNode).some((entry) => entry.value.length > 0);
}

function assertConceptHierarchy(world, conceptLabel) {
  const entry = requireConceptEntry(world, conceptLabel);
  const node = entry.node;
  if (!conceptExposesActionMetadata(node)) return;

  const primaryTexts = conceptPrimaryActionTexts(node);
  if (primaryTexts.length === 0) {
    contractGap(world, `${conceptLabel} exposes action metadata but no primaryAction`, `${conceptLabel.toUpperCase()} ACTIONS:\n${objectText(node)}`);
  }
  if (primaryTexts.length > 1) {
    contractGap(world, `${conceptLabel} exposes more than one primary-looking action`, `${conceptLabel.toUpperCase()} PRIMARY ACTIONS:\n${primaryTexts.join("\n")}`);
  }
  const expected = CONCEPT_EXPECTED_ACTIONS[conceptLabel];
  if (expected && primaryTexts.length && !expected.primary.test(primaryTexts[0])) {
    contractGap(world, `${conceptLabel} primary action is not the expected concept action`, `Expected: ${expected.primary}\nActual: ${primaryTexts[0]}`);
  }

  const secondaryTexts = conceptSecondaryActionTexts(node);
  const secondaryCombined = secondaryTexts.join("\n");
  if (expected && secondaryTexts.length && expected.secondary.length) {
    const missing = expected.secondary.filter((pattern) => !pattern.test(secondaryCombined));
    if (missing.length) {
      contractGap(world, `${conceptLabel} secondaryActions are missing expected support actions`, `Expected patterns: ${missing.map(String).join(", ")}\nActual secondary actions:\n${secondaryCombined || "<none>"}`);
    }
  }

  for (const arrayEntry of conceptActionArrays(node)) {
    const actions = arrayEntry.value;
    if (actions.length <= 1) continue;
    const hasPrimary = actions.some(isPrimaryAction) || primaryTexts.length > 0;
    const hasSecondary = actions.some(isSecondaryAction) || secondaryTexts.length > 0;
    if (!hasPrimary || (!hasSecondary && actions.length > 1)) {
      contractGap(
        world,
        `${conceptLabel} exposes a flat undifferentiated action list at ${arrayEntry.pathParts.join(".")}`,
        `ACTIONS:\n${actions.map(actionText).join("\n")}`
      );
    }
  }
}

function allConceptCardsExposeHierarchy(world) {
  return CONCEPT_LABELS.every((conceptLabel) => {
    const entry = requireConceptEntry(world, conceptLabel);
    return conceptPrimaryActionTexts(entry.node).length > 0 || conceptSecondaryActionTexts(entry.node).length > 0;
  });
}

function assertNoFlatTopLevelActionStrip(world) {
  const conceptHierarchyPresent = allConceptCardsExposeHierarchy(world);
  for (const container of rootActionContainers(world.profileFirstGuiReport)) {
    if (conceptHierarchyPresent && /^(actions|commands)$/i.test(container.key)) {
      continue;
    }
    const unscopedOrUnranked = container.value.filter((action) => {
      const scope = canonicalConceptLabel(actionScope(action));
      const rank = actionRank(action);
      return !scope || !/(primary|secondary|tertiary|supporting|history|executed|audit)/i.test(rank);
    });
    if (unscopedOrUnranked.length) {
      contractGap(
        world,
        `top-level ${container.key} looks like a flat action strip instead of concept-scoped primary/secondary actions`,
        `TOP-LEVEL ${container.key.toUpperCase()}:\n${container.value.map(actionText).join("\n")}`
      );
    }
  }
}

function textHas(text, pattern) {
  return pattern.test(text.replace(/\s+/g, " "));
}

function packageListEntries(value) {
  const entries = [];
  walk(value, (node, pathParts) => {
    if (!Array.isArray(node)) return;
    const pathText = pathParts.join(".");
    if (!/(package|packages|packageList|library|catalog|mods|localPackages)/i.test(pathText)) return;
    for (const item of node) {
      if (item && typeof item === "object" && !Array.isArray(item)) entries.push({ node: item, pathParts });
    }
  });
  return entries;
}

function runeboundPackageEvidence(value) {
  const listEntries = packageListEntries(value).filter((entry) => /Runebound: Elixirs|jml\.runebound-elixirs/i.test(objectText(entry.node)));
  const packageObjects = allObjects(value).filter((entry) => {
    if (entry.pathParts.length === 0) return false;
    const pathText = entry.pathParts.join(".");
    if (!/(package|packages|selectedPackage|packageDetails|library|catalog|mods)/i.test(pathText)) return false;
    return /Runebound: Elixirs|jml\.runebound-elixirs/i.test(objectText(entry.node));
  });
  const evidenceObjects = [...listEntries, ...packageObjects];
  const combined = evidenceObjects.map((entry) => objectText(entry.node)).join("\n");
  return { listEntries, packageObjects, evidenceObjects, combined };
}

function createWorkshopSubscriptionFixture(homeDir) {
  const workshopDir = path.join(
    homeDir,
    ".local",
    "share",
    "Steam",
    "steamapps",
    "workshop",
    "content",
    "371970",
    WORKSHOP_FIXTURE_ID
  );
  fs.mkdirSync(workshopDir, { recursive: true });
  fs.writeFileSync(
    path.join(workshopDir, "subscription-title.txt"),
    `${WORKSHOP_FIXTURE_TITLE}\n${WORKSHOP_FIXTURE_NOTE}\n`,
    "utf-8"
  );
  fs.writeFileSync(
    path.join(workshopDir, "readme.txt"),
    "This fixture intentionally omits bml-package.json so GUI smoke can prove Workshop subscriptions are provenance entries even when they are not BaronyModLoader packages.\n",
    "utf-8"
  );
  return workshopDir;
}

function createActiveRuneboundProfileFixture(dataHome) {
  const profileDir = path.join(dataHome, "BaronyModLoader", "profiles", "default");
  const bmlRoot = path.join(profileDir, "BaronyModLoader");
  const packagePath = path.join(REPO_ROOT, "mods", "runebound-elixirs");
  const manifestPath = path.join(packagePath, "bml-package.json");
  const bmlExecutable = path.join(os.homedir(), ".local", "share", "Steam", "steamapps", "common", "Barony", "barony.x86_64");
  const activeMod = {
    id: RUNEBOUND_ID,
    version: "0.1.0",
    packagePath,
    manifestPath,
    checksumSet: "cucumber-fixture",
    enabledAt: "2026-07-06T00:00:00Z",
  };
  fs.mkdirSync(bmlRoot, { recursive: true });
  fs.writeFileSync(
    path.join(profileDir, "profile.json"),
    JSON.stringify(
      {
        schemaVersion: "0.1.0",
        profile: {
          id: "default",
          name: "Default",
          selectedAt: "2026-07-06T00:00:00Z",
          updatedAt: "2026-07-06T00:00:00Z",
        },
        app: {
          id: "barony-mod-loader",
          version: "0.1.0",
        },
        paths: {
          profileRoot: profileDir,
          bmlRoot,
          logs: path.join(bmlRoot, "logs"),
          reports: path.join(bmlRoot, "reports"),
          manifests: path.join(bmlRoot, "manifests"),
          state: path.join(bmlRoot, "state"),
          runtimeManifest: path.join(bmlRoot, "runtime-manifest.json"),
        },
        runtime: {
          gameSource: "steam",
          baronyExecutable: bmlExecutable,
          runtimeInfo: null,
        },
        activeMods: [activeMod],
      },
      null,
      2
    ),
    "utf-8"
  );
  fs.writeFileSync(
    path.join(bmlRoot, "active-mods.json"),
    JSON.stringify(
      {
        schemaVersion: "0.1.0",
        profileId: "default",
        generatedAt: "2026-07-06T00:00:00Z",
        mods: [activeMod],
      },
      null,
      2
    ),
    "utf-8"
  );
  return profileDir;
}

function getPathValue(value, pathParts) {
  let current = value;
  for (const part of pathParts) {
    if (!current || typeof current !== "object") return undefined;
    current = current[part];
  }
  return current;
}

function requireArrayField(world, fieldName) {
  const value = world.profileFirstGuiReport && world.profileFirstGuiReport[fieldName];
  if (!Array.isArray(value)) {
    contractGap(
      world,
      `missing required smoke report field ${fieldName} (expected an array from the profile-first mod provenance sidebar contract)`
    );
  }
  return value;
}

function labelText(value) {
  if (typeof value === "string") return value;
  if (!value || typeof value !== "object") return "";
  const direct = ["label", "title", "name", "provenanceLabel", "sectionLabel", "displayName", "heading"];
  for (const key of direct) {
    if (typeof value[key] === "string" && value[key].trim()) return value[key];
  }
  return objectText(value);
}

function provenanceDescriptor(section) {
  if (!section || typeof section !== "object") return String(section || "");
  const parts = [];
  for (const key of ["id", "key", "type", "kind", "source", "sourceType", "provenance", "provenanceKey", "provenanceLabel", "label", "displayLabel", "title", "name"]) {
    if (section[key] !== undefined && section[key] !== null) parts.push(String(section[key]));
  }
  return parts.join(" ");
}

function provenancePatterns(kind) {
  if (kind === "local") return [/local/i, /repo|repository/i];
  if (kind === "workshop") return [/workshop/i];
  throw new Error(`unknown provenance kind: ${kind}`);
}

function sectionMatchesKind(section, kind) {
  const text = `${provenanceDescriptor(section)} ${objectText(section)}`;
  return provenancePatterns(kind).every((pattern) => pattern.test(text));
}

function detectedModSections(world) {
  const sections = requireArrayField(world, "detectedModSections");
  if (!sections.length) {
    contractGap(world, "detectedModSections is present but empty; expected provenance sections for detected mods");
  }
  return sections;
}

function renderedProvenanceSectionLabels(world) {
  const labels = requireArrayField(world, "renderedProvenanceSectionLabels");
  if (!labels.length) {
    contractGap(world, "renderedProvenanceSectionLabels is present but empty; expected rendered provenance headings");
  }
  return labels.map((label) => String(label));
}

function containsEnabledProfileProvenance(value) {
  return /enabled\s+in\s+profile|profile[_-]enabled/i.test(String(value));
}

function assertNoEnabledProfileProvenanceSections(world) {
  const labels = renderedProvenanceSectionLabels(world);
  const forbiddenLabels = labels.filter(containsEnabledProfileProvenance);
  const sections = detectedModSections(world);
  const forbiddenSections = sections.filter((section) => containsEnabledProfileProvenance(provenanceDescriptor(section)));
  if (forbiddenLabels.length || forbiddenSections.length) {
    contractGap(
      world,
      "profile-first Mods list still exposes the removed Enabled in profile provenance section",
      [
        `FORBIDDEN RENDERED LABELS:\n${forbiddenLabels.join("\n") || "<none>"}`,
        `FORBIDDEN DETECTED SECTIONS:\n${forbiddenSections.map((section) => provenanceDescriptor(section) || objectText(section)).join("\n---\n") || "<none>"}`,
      ].join("\n\n")
    );
  }
}

function requireProvenanceSection(world, kind) {
  const sections = detectedModSections(world);
  const section = sections.find((candidate) => sectionMatchesKind(candidate, kind));
  if (!section) {
    const labels = sections.map((candidate) => labelText(candidate)).filter(Boolean);
    contractGap(
      world,
      `detectedModSections has no ${kind} provenance section`,
      `SECTION LABELS FOUND:\n${labels.join("\n") || "<none>"}`
    );
  }
  return section;
}

function sectionItemObjects(section) {
  const items = [];
  if (!section || typeof section !== "object") return items;
  for (const key of ["mods", "items", "entries", "subscriptions", "packages", "detectedMods", "children"]) {
    const value = section[key];
    if (!Array.isArray(value)) continue;
    for (const item of value) {
      if (item && typeof item === "object") items.push(item);
      else if (item !== undefined && item !== null) items.push({ label: String(item) });
    }
  }
  if (!items.length) {
    for (const entry of allObjects(section)) {
      if (entry.pathParts.length && /mods|items|entries|subscriptions|packages|detectedMods|children/i.test(entry.pathParts.join("."))) {
        items.push(entry.node);
      }
    }
  }
  return items;
}

function requireSectionText(world, section, pattern, message) {
  const text = objectText(section);
  if (!pattern.test(text)) {
    contractGap(world, message, `SECTION EVIDENCE:\n${text || "<none>"}`);
  }
  return text;
}

function candidateObjectsAtPaths(report, candidatePaths) {
  const entries = [];
  for (const pathParts of candidatePaths) {
    const value = getPathValue(report, pathParts);
    if (value && typeof value === "object") entries.push({ node: value, pathParts });
  }
  return entries;
}

function sidebarCandidates(report) {
  const candidates = candidateObjectsAtPaths(report, [
    ["modsList"],
    ["modsSidebar"],
    ["detectedModsSidebar"],
    ["leftSidebar"],
    ["leftSide"],
    ["leftPane"],
    ["leftColumn"],
    ["renderedLeftSide"],
    ["renderedSidebar"],
    ["sidebar"],
    ["mainBody", "left"],
    ["mainBody", "leftSide"],
    ["layout", "left"],
    ["layout", "leftSide"],
  ]);
  for (const titleField of ["modsListTitle", "modListTitle", "renderedModsListTitle", "leftListTitle", "detectedModsSidebarTitle"]) {
    if (report && typeof report[titleField] === "string") {
      candidates.push({
        node: {
          title: report[titleField],
          detectedModSections: report.detectedModSections,
          renderedProvenanceSectionLabels: report.renderedProvenanceSectionLabels,
          renderedDetectedModNames: report.renderedDetectedModNames,
          renderedDetectedModRows: report.renderedDetectedModRows,
          selectedDetectedMod: report.selectedDetectedMod,
          selectedDetectedModId: report.selectedDetectedModId,
          selectedDetectedModProvenance: report.selectedDetectedModProvenance,
        },
        pathParts: [titleField],
      });
    }
  }
  for (const entry of allObjects(report)) {
    const pathText = entry.pathParts.join(".");
    if (/(^|\.)(left|sidebar|detectedModsSidebar|detectedModsList|leftPane|leftColumn)(\.|$)/i.test(pathText)) {
      candidates.push(entry);
    }
  }
  return candidates;
}

function requireDetectedModsSidebar(world) {
  const candidates = sidebarCandidates(world.profileFirstGuiReport);
  if (!candidates.length) {
    contractGap(
      world,
      "missing sidebar layout field for the left side (expected leftSidebar, detectedModsSidebar, mainBody.left, or equivalent)"
    );
  }
  const sidebar = candidates.find((entry) => /detected\s+mods?/i.test(objectText(entry.node))) || candidates[0];
  const text = objectText(sidebar.node);
  if (!/detected\s+mods?/i.test(text)) {
    contractGap(
      world,
      "left-side sidebar field does not identify itself as Detected Mods",
      `SIDEBAR PATH: ${sidebar.pathParts.join(".")}\nSIDEBAR EVIDENCE:\n${text || "<none>"}`
    );
  }
  return sidebar;
}

function modsListTitle(world) {
  const report = world.profileFirstGuiReport || {};
  for (const key of ["modsListTitle", "modListTitle", "renderedModsListTitle", "leftListTitle", "detectedModsSidebarTitle"]) {
    if (typeof report[key] === "string" && report[key].trim()) return report[key].trim();
  }
  for (const entry of sidebarCandidates(report)) {
    for (const key of ["title", "label", "heading", "name"]) {
      if (typeof entry.node[key] === "string" && entry.node[key].trim()) return entry.node[key].trim();
    }
  }
  return "";
}

function requireModsList(world) {
  const candidates = sidebarCandidates(world.profileFirstGuiReport);
  if (!candidates.length) {
    contractGap(
      world,
      "missing left-side Mods list field (expected modsList, modsSidebar, leftSidebar, mainBody.left, or equivalent)"
    );
  }
  const list = candidates.find((entry) => /\bMods?\b/i.test(objectText(entry.node))) || candidates[0];
  const title = modsListTitle(world);
  const text = objectText(list.node);
  if (!/\bMods?\b/i.test(`${title} ${text}`)) {
    contractGap(
      world,
      "left-side list does not identify itself as Mods",
      `MODS LIST TITLE: ${title || "<none>"}\nMODS LIST EVIDENCE:\n${text || "<none>"}`
    );
  }
  return list;
}

function arraysAtCandidatePaths(report, candidatePaths) {
  const arrays = [];
  for (const candidatePath of candidatePaths) {
    const value = getPathValue(report, candidatePath);
    if (Array.isArray(value)) arrays.push({ value, pathParts: candidatePath });
  }
  return arrays;
}

function modsListHeaderActionArrays(world) {
  const report = world.profileFirstGuiReport || {};
  const arrays = arraysAtCandidatePaths(report, [
    ["modsListHeaderActions"],
    ["modsListHeaderButtons"],
    ["modListHeaderActions"],
    ["modListHeaderButtons"],
    ["detectedModsHeaderActions"],
    ["detectedModsHeaderButtons"],
    ["detectedModsListHeaderActions"],
    ["detectedModsListHeaderButtons"],
    ["modsList", "headerActions"],
    ["modsList", "headerButtons"],
    ["modsList", "header", "actions"],
    ["modsList", "header", "buttons"],
    ["modsSidebar", "headerActions"],
    ["modsSidebar", "headerButtons"],
    ["modsSidebar", "header", "actions"],
    ["modsSidebar", "header", "buttons"],
    ["detectedModsSidebar", "headerActions"],
    ["detectedModsSidebar", "headerButtons"],
    ["detectedModsSidebar", "header", "actions"],
    ["detectedModsSidebar", "header", "buttons"],
  ]);
  const list = requireModsList(world);
  for (const key of ["headerActions", "headerButtons", "topActions", "topButtons"]) {
    if (Array.isArray(list.node[key])) arrays.push({ value: list.node[key], pathParts: list.pathParts.concat(key) });
  }
  if (list.node.header && typeof list.node.header === "object") {
    for (const key of ["actions", "buttons", "controls"]) {
      if (Array.isArray(list.node.header[key])) arrays.push({ value: list.node.header[key], pathParts: list.pathParts.concat("header", key) });
    }
  }
  return arrays;
}

function requireModsListHeaderControls(world) {
  const arrays = modsListHeaderActionArrays(world).filter((entry) => entry.value.length);
  if (!arrays.length) {
    contractGap(
      world,
      "missing Mods list header controls/buttons (expected modsListHeaderActions, modsList.header.controls, detectedModsHeaderActions, or equivalent)"
    );
  }
  const actionTextCombined = arrays.flatMap((entry) => entry.value).map(actionText).join("\n");
  if (!/(scan|refresh|filter|search|sort|package|mod)/i.test(actionTextCombined)) {
    contractGap(
      world,
      "Mods list header controls/buttons do not expose scan, refresh, filter, search, or sort list controls",
      `HEADER CONTROLS:\n${actionTextCombined || "<none>"}`
    );
  }
  return arrays;
}

function requireModsListHeaderActions(world) {
  return requireModsListHeaderControls(world);
}

function renderedDetectedModRows(world) {
  const rows = requireArrayField(world, "renderedDetectedModRows");
  if (!rows.length) {
    contractGap(world, "renderedDetectedModRows is present but empty; expected rendered Mods list row metadata");
  }
  return rows;
}

function rowKeyValues(row, keyPattern) {
  return valuesForKey(row, keyPattern).map((entry) => entry.value);
}

function rowBoolean(row, keyPattern, expected) {
  return rowKeyValues(row, keyPattern).some((value) => value === expected);
}

function rowStringValues(row, keyPattern) {
  return rowKeyValues(row, keyPattern)
    .filter((value) => value !== undefined && value !== null)
    .map((value) => String(value));
}

function rowPrefixText(row) {
  return rowStringValues(row, /(prefix|check|indicator|stateIcon|glyph|mark|icon)$/i).join(" ");
}

function rowColorText(row) {
  return rowStringValues(row, /(prefix|check|indicator|stateIcon|glyph|mark|icon|foreground|fg|color|style|class|tag)$/i).join(" ");
}

function rowPrefixColumn(row) {
  const values = rowKeyValues(row, /^(prefixColumn|prefixColumnId|prefixColumnIndex|prefixCol|prefixSlot|prefixWidth)$/i)
    .filter((value) => value !== undefined && value !== null && String(value).trim() !== "");
  return values.length ? String(values[0]) : null;
}

function rowReservesPrefix(row) {
  return rowPrefixColumn(row) !== null ||
    rowBoolean(row, /^(alignedPrefix|prefixReserved|hasPrefixColumn|reservesPrefixColumn)$/i, true) ||
    /prefix(?:Column|Reserved|Width)/i.test(objectText(row));
}

function assertRowsExposeAlignedPrefix(world, rows) {
  if (booleanFlag(world.profileFirstGuiReport, /^(alignedDetectedModPrefixes|detectedModPrefixesAligned|renderedDetectedModPrefixesAligned)$/i, true)) return;
  const columns = rows.map(rowPrefixColumn);
  if (columns.every((column) => column !== null) && new Set(columns).size === 1) return;
  if (rows.every((row) => rowReservesPrefix(row) && rowBoolean(row, /^alignedPrefix$/i, true))) return;
  contractGap(
    world,
    "renderedDetectedModRows do not expose a common aligned prefix column",
    `ROWS:\n${rows.map(rowText).join("\n---\n") || "<none>"}`
  );
}

function isEnabledModRow(row) {
  if (rowBoolean(row, /^(disabled|inactive|isDisabled|isInactive)$/i, true)) return false;
  if (rowBoolean(row, /^(enabled|active|isEnabled|isActive|profileEnabled)$/i, true)) return true;
  if (rowBoolean(row, /^(enabled|active|isEnabled|isActive|profileEnabled)$/i, false)) return false;
  return /\b(enabled|active)\b/i.test(rowStatePlainText(row)) && !/\b(disabled|inactive)\b/i.test(rowStatePlainText(row));
}

function isDisabledModRow(row) {
  if (rowBoolean(row, /^(disabled|inactive|isDisabled|isInactive)$/i, true)) return true;
  if (rowBoolean(row, /^(enabled|active|isEnabled|isActive|profileEnabled)$/i, false)) return true;
  if (isEnabledModRow(row)) return false;
  return /\b(disabled|inactive|detected|subscribed|subscription)\b/i.test(objectText(row));
}

function hasGreenCheckPrefix(row) {
  const prefixText = rowPrefixText(row);
  const fullText = objectText(row);
  const hasCheck = /[✓✔☑]|check/i.test(prefixText) ||
    rowBoolean(row, /^(greenCheckPrefix|checkPrefix|enabledCheckPrefix|hasCheckPrefix)$/i, true) ||
    /[✓✔☑][\s\S]{0,40}(enabled|active)|check[\s\S]{0,40}(enabled|active)/i.test(fullText);
  const hasGreen = /\bgreen\b|\blime\b|\bsuccess\b|#[0-9a-f]{6}|#[0-9a-f]{3}/i.test(rowColorText(row)) ||
    rowBoolean(row, /^(greenPrefix|prefixGreen|checkPrefixGreen|greenCheckPrefix|isPrefixGreen)$/i, true);
  return hasCheck && hasGreen;
}

function rowStatePlainText(row) {
  return rowStringValues(row, /(state|status|subtitle|description|text|label|display|rendered)$/i).join(" ");
}

function rowSelectable(row) {
  return rowBoolean(row, /^(selectable|isSelectable|canSelect|rowSelectable)$/i, true) ||
    rowStringValues(row, /(selectionTarget|selectTarget|clickTarget|rowId|detectedModId|id|packageId)$/i).some((value) => value.trim());
}

function rowFocusable(row) {
  return rowBoolean(row, /^(focusable|isFocusable|canFocus|keyboardFocusable|tabFocusable|takeFocus|takefocus|tabStop|tabbable)$/i, true) ||
    rowStringValues(row, /^(takeFocus|takefocus|tabIndex|tabindex|role|widgetRole|accessibilityRole)$/i)
      .some((value) => /^(1|true|row|button|listbox|option|treeitem)$/i.test(value.trim())) ||
    /\b(takefocus|tabbable|tab stop|focusable|keyboard focus)\b/i.test(objectText(row));
}

function rowHasSelectionAffordance(row) {
  const text = objectText(row);
  return rowBoolean(row, /^(selected|isSelected|focused|isFocused|hasFocus|focusVisible|focusRing|selectionVisible|highlighted|isHighlighted)$/i, true) ||
    rowStringValues(row, /(style|class|tag|state|status|foreground|fg|background|bg|border|outline|relief|affordance|visual)$/i)
      .some((value) => /\b(selected|focused|focus|highlight|active|accent|outline|ring|raised|sunken)\b/i.test(value)) ||
    /\b(selected|focused|focus ring|selection highlight|selected row|active row)\b/i.test(text);
}

function keyboardNavigationEvidence(value) {
  if (booleanFlag(value, /^(modsRowsKeyboardNavigable|keyboardSelectionEnabled|modRowKeyboardNavigation|rowKeyboardNavigation|upDownEnterSelection|arrowKeySelection)$/i, true)) {
    return true;
  }
  const bindingText = rowStringValues(value, /^(keyboardBindings|keyboardShortcuts|keyBindings|keybinds|shortcuts)$/i).join(" ");
  const hasActivationBinding = /\b(Enter|Return|Space)\b/i.test(bindingText);
  const hasTraversalBinding = /\b(ArrowUp|Up|ArrowDown|Down)\b/i.test(bindingText);
  if (hasActivationBinding && hasTraversalBinding) return true;
  const text = objectText(value);
  const hasActivationText = /\b(enter|return|space)\b/i.test(text);
  const hasTraversalText = /\b(up|arrowup|down|arrowdown)\b/i.test(text);
  return (hasActivationText && hasTraversalText && /\b(key|keyboard|binding|shortcut|navigation|selection)\b/i.test(text)) ||
    /\bkeyboard\s+(?:navigation|selection)\b/i.test(text);
}

function assertModsRowsKeyboardAndFocusMetadata(world) {
  const rows = renderedDetectedModRows(world);
  const selectableRows = rows.filter(rowSelectable);
  if (!selectableRows.length) {
    contractGap(world, "renderedDetectedModRows has no selectable rows to validate keyboard focus metadata");
  }
  const notFocusable = selectableRows.filter((row) => !rowFocusable(row));
  if (notFocusable.length) {
    contractGap(
      world,
      "selectable Mods rows are missing focusable/tab-stop metadata",
      `ROWS:\n${notFocusable.map(rowText).join("\n---\n")}`
    );
  }
  if (!keyboardNavigationEvidence(world.profileFirstGuiReport || {}) && !selectableRows.some(keyboardNavigationEvidence)) {
    contractGap(
      world,
      "Mods rows lack keyboard navigation metadata for tab/focus plus Up/Down/Enter row selection",
      `ROWS:\n${selectableRows.map(rowText).join("\n---\n")}`
    );
  }
}

function selectedRenderedModRow(world) {
  const report = world.profileFirstGuiReport || {};
  const rows = renderedDetectedModRows(world);
  const selectedId = selectedDetectedModId(report);
  const selectedMod = selectedModObject(world);
  const selectedIdentities = [
    selectedId,
    ...selectedModIdentityValues(selectedMod),
    ...selectedModNameValues(selectedMod),
  ].filter(Boolean).map(String);
  const directlySelected = rows.find((row) => rowBoolean(row, /^(selected|isSelected|focused|isFocused|hasFocus)$/i, true));
  if (directlySelected) return directlySelected;
  const matched = rows.find((row) => selectedIdentities.some((value) => value && objectMatchesPackage(row, value, value)));
  if (matched) return matched;
  contractGap(
    world,
    "could not match selectedDetectedMod/selectedMod back to a rendered Mods row",
    `selectedDetectedModId=${selectedId || "<none>"}\nROWS:\n${rows.map(rowText).join("\n---\n") || "<none>"}`
  );
}

function assertSelectedRowFocusAffordance(world) {
  const row = selectedRenderedModRow(world);
  const selected = selectedModObject(world);
  const smokeSelected = smokeSelectedMod(world.profileFirstGuiReport || {});
  if (rowHasSelectionAffordance(row) || rowHasSelectionAffordance(selected) || (smokeSelected && rowHasSelectionAffordance(smokeSelected))) return;
  contractGap(
    world,
    "selected Mods row lacks visible focus/selection affordance metadata",
    `ROW:\n${rowText(row)}\nSELECTED MOD:\n${objectText(selected) || "<none>"}\nSMOKE SELECTED:\n${objectText(smokeSelected) || "<none>"}`
  );
}

function escapeRegex(value) {
  return String(value).replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
}

function packagePattern(packageId, packageName) {
  return new RegExp(`${escapeRegex(packageId)}|${escapeRegex(packageName)}`, "i");
}

function objectMatchesPackage(value, packageId, packageName) {
  return packagePattern(packageId, packageName).test(objectText(value));
}

function packageRow(rows, packageId, packageName) {
  const pattern = packagePattern(packageId, packageName);
  return rows.find((row) => pattern.test(objectText(row)));
}

function runeboundRow(rows) {
  return packageRow(rows, RUNEBOUND_ID, RUNEBOUND_NAME);
}

function stashRow(rows) {
  return packageRow(rows, STASH_ID, STASH_NAME);
}


function workshopSubscriptionRow(rows) {
  return packageRow(rows, WORKSHOP_FIXTURE_ID, WORKSHOP_FIXTURE_TITLE);
}

function selectedModObject(world) {
  const report = world.profileFirstGuiReport || {};
  const selectedMod = report.selectedMod;
  if (!selectedMod || typeof selectedMod !== "object" || Array.isArray(selectedMod)) {
    contractGap(
      world,
      "missing required canonical smoke report field selectedMod after deterministic Mods list selection",
      `Expected a single selectedMod object with identity, provenance, path, profile state, and action eligibility.\nREPORT PREVIEW:\n${reportPreview(world)}`
    );
  }
  return selectedMod;
}

function hasKeyMatching(value, keyPattern) {
  return valuesForKey(value, keyPattern).length > 0;
}

function nonEmptyValuesForKey(value, keyPattern) {
  return valuesForKey(value, keyPattern)
    .map((entry) => entry.value)
    .filter((entryValue) => entryValue !== undefined && entryValue !== null && String(entryValue).trim() !== "");
}

function booleanValuesForKey(value, keyPattern) {
  return valuesForKey(value, keyPattern)
    .map((entry) => entry.value)
    .filter((entryValue) => typeof entryValue === "boolean");
}

function firstBooleanValueForKey(value, keyPattern) {
  const values = booleanValuesForKey(value, keyPattern);
  return values.length ? values[0] : null;
}

function actionPattern(actionName) {
  if (/^enable$/i.test(actionName)) return /\benable\b|enable-package|enable selected mod/i;
  if (/^disable$/i.test(actionName)) return /\bdisable\b|disable-package|disable selected mod/i;
  if (/^publish$/i.test(actionName)) return /\bpublish\b|workshop-preview|steam publish/i;
  return new RegExp(escapeRegex(actionName), "i");
}

function actionDirectKeyPattern(actionName) {
  if (/^enable$/i.test(actionName)) return /^(canEnable|enableAvailable|enableEligible|enableEnabled|enableAllowed|mayEnable)$/i;
  if (/^disable$/i.test(actionName)) return /^(canDisable|disableAvailable|disableEligible|disableEnabled|disableAllowed|mayDisable)$/i;
  if (/^publish$/i.test(actionName)) return /^(canPublish|publishAvailable|publishEligible|publishEnabled|publishAllowed|mayPublish|eligibleToPublish)$/i;
  return new RegExp(`^can${escapeRegex(actionName)}$`, "i");
}

function objectLooksLikeAction(entry, actionName) {
  const pattern = actionPattern(actionName);
  return pattern.test(actionIdForEntry(entry)) || pattern.test(objectText(entry));
}

function actionAvailabilityFromObject(entry) {
  const positive = firstBooleanValueForKey(entry, /^(enabled|available|eligible|allowed|canInvoke|canRun|canClick|selectable)$/i);
  if (positive !== null) return positive;
  const negative = firstBooleanValueForKey(entry, /^(disabled|unavailable|blocked|forbidden)$/i);
  if (negative !== null) return !negative;
  return null;
}

function selectedModActionState(selectedMod, actionName) {
  const direct = firstBooleanValueForKey(selectedMod, actionDirectKeyPattern(actionName));
  const state = { available: direct, primary: null, evidence: [] };
  const pattern = actionPattern(actionName);

  walk(selectedMod, (node, pathParts) => {
    const key = pathParts[pathParts.length - 1] || "";
    if (typeof node === "boolean" && actionDirectKeyPattern(actionName).test(key)) {
      state.available = node;
      state.evidence.push(`${pathParts.join(".")}=${node}`);
      return;
    }
    if (typeof node === "string" && /^(primaryAction|primaryActionId|primaryActionLabel|sensiblePrimaryAction|defaultAction)$/i.test(key) && pattern.test(node)) {
      state.primary = true;
      state.evidence.push(`${pathParts.join(".")}=${node}`);
      return;
    }
    if (!node || typeof node !== "object") return;

    const previousKey = pathParts[pathParts.length - 2] || "";
    if (pattern.test(key) && /^(actionEligibility|actionEligibilityById|actionsById|availableActions|eligibleActions|buttons|controls)$/i.test(previousKey)) {
      const available = actionAvailabilityFromObject(node);
      if (available !== null) state.available = available;
      if (rowBoolean(node, /^(primary|isPrimary|default|isDefault)$/i, true) || /primary/i.test(String(actionRank(node)))) state.primary = true;
      state.evidence.push(`${pathParts.join(".")}: ${objectText(node)}`);
      return;
    }

    if (objectLooksLikeAction(node, actionName)) {
      const available = actionAvailabilityFromObject(node);
      if (available !== null) state.available = available;
      if (rowBoolean(node, /^(primary|isPrimary|default|isDefault)$/i, true) || /primary/i.test(String(actionRank(node)))) state.primary = true;
      state.evidence.push(`${pathParts.join(".")}: ${objectText(node)}`);
    }
  });

  return state;
}

function selectedModEnabledInProfile(selectedMod) {
  return firstBooleanValueForKey(selectedMod, /^(enabledInProfile|profileEnabled|activeInProfile|isEnabledInProfile|enabled|isEnabled|active)$/i);
}

function requireCanonicalSelectedModFields(world, selectedMod) {
  const missing = [];
  if (!nonEmptyValuesForKey(selectedMod, /^(rowId|id)$/i).length) missing.push("rowId/id");
  if (!hasKeyMatching(selectedMod, /^packageId$/i)) missing.push("packageId");
  if (!nonEmptyValuesForKey(selectedMod, /^(name|displayName|title|label)$/i).length) missing.push("name");
  if (!nonEmptyValuesForKey(selectedMod, /^(provenance|source|origin|type|kind)$/i).length) missing.push("provenance/source/type");
  if (!nonEmptyValuesForKey(selectedMod, /(path|root|directory|dir|folder|location)$/i).length) missing.push("path");
  if (selectedModEnabledInProfile(selectedMod) === null) missing.push("enabledInProfile");
  if (firstBooleanValueForKey(selectedMod, /^(selectable|isSelectable|canSelect|rowSelectable)$/i) !== true) missing.push("selectable=true");
  if (selectedModActionState(selectedMod, "enable").available === null) missing.push("canEnable/actionEligibility.enable");
  if (selectedModActionState(selectedMod, "disable").available === null) missing.push("canDisable/actionEligibility.disable");
  if (selectedModActionState(selectedMod, "publish").available === null) missing.push("canPublish/actionEligibility.publish");
  if (missing.length) {
    contractGap(
      world,
      "canonical selectedMod is missing required state-coherence fields",
      `Missing: ${missing.join(", ")}\nselectedMod:\n${objectText(selectedMod) || "<none>"}`
    );
  }
}

function requireSelectedModPackage(world, packageId, packageName) {
  const selectedMod = selectedModObject(world);
  requireCanonicalSelectedModFields(world, selectedMod);
  const packageIdValues = valuesForKey(selectedMod, /^packageId$/i).map((entry) => entry.value);
  if (!packageIdValues.some((value) => String(value) === packageId)) {
    contractGap(
      world,
      "canonical selectedMod packageId does not match the selected local package",
      `Expected packageId=${packageId}\npackageId values: ${packageIdValues.map(String).join(", ") || "<none>"}\nselectedMod:\n${objectText(selectedMod)}`
    );
  }
  const nameText = nonEmptyValuesForKey(selectedMod, /^(name|displayName|title|label)$/i).map(String).join(" ");
  if (!new RegExp(escapeRegex(packageName), "i").test(nameText)) {
    contractGap(
      world,
      "canonical selectedMod name does not match the selected local package",
      `Expected name=${packageName}\nname values: ${nameText || "<none>"}\nselectedMod:\n${objectText(selectedMod)}`
    );
  }
  const provenanceText = valuesForKey(selectedMod, /^(provenance|source|origin|type|kind)$/i).map((entry) => String(entry.value)).join(" ");
  if (!/(local|repo|repository|package|bml)/i.test(provenanceText)) {
    contractGap(
      world,
      "canonical selectedMod provenance does not identify a local BML package",
      `PROVENANCE: ${provenanceText || "<none>"}\nselectedMod:\n${objectText(selectedMod)}`
    );
  }
  return selectedMod;
}

function requireSelectedModProfileState(world, selectedMod, expectedEnabled, packageName) {
  const enabledInProfile = selectedModEnabledInProfile(selectedMod);
  if (enabledInProfile !== expectedEnabled) {
    contractGap(
      world,
      `canonical selectedMod enabledInProfile does not match selected ${packageName} profile state`,
      `Expected enabledInProfile=${expectedEnabled}\nActual enabledInProfile=${enabledInProfile}\nselectedMod:\n${objectText(selectedMod)}`
    );
  }
}

function requireSelectedModActionChoice(world, selectedMod, primaryAction, unavailableAction) {
  const primaryState = selectedModActionState(selectedMod, primaryAction);
  const unavailableState = selectedModActionState(selectedMod, unavailableAction);
  if (primaryState.available !== true || primaryState.primary !== true) {
    contractGap(
      world,
      `canonical selectedMod does not expose ${primaryAction} as the available primary action`,
      `Expected ${primaryAction}: available=true, primary=true\nObserved: ${JSON.stringify(primaryState)}\nselectedMod:\n${objectText(selectedMod)}`
    );
  }
  if (unavailableState.available !== false) {
    contractGap(
      world,
      `canonical selectedMod does not disable the redundant ${unavailableAction} action`,
      `Expected ${unavailableAction}: available=false\nObserved: ${JSON.stringify(unavailableState)}\nselectedMod:\n${objectText(selectedMod)}`
    );
  }
}

function selectedModDetailCandidates(report) {
  const candidates = candidateObjectsAtPaths(report, [
    ["selectedModDetail"],
    ["selectedDetailPanel"],
    ["selectedModPanel"],
    ["selectedModInspector"],
    ["selectedDetail"],
    ["rightSide", "selectedModDetail"],
    ["rightSide", "selectedDetailPanel"],
    ["rightSide", "selectedModPanel"],
    ["rightSide", "selectedModInspector"],
    ["rightSide", "detailPanel"],
    ["rightPane", "selectedModDetail"],
    ["rightPane", "selectedDetailPanel"],
    ["rightColumn", "selectedModDetail"],
    ["rightColumn", "selectedDetailPanel"],
    ["mainBody", "right", "selectedModDetail"],
    ["mainBody", "right", "selectedDetailPanel"],
    ["mainBody", "rightSide", "selectedModDetail"],
    ["mainBody", "rightSide", "selectedDetailPanel"],
  ]);
  for (const entry of allObjects(report)) {
    const pathText = entry.pathParts.join(".");
    if (/^selectedMod$/i.test(pathText)) continue;
    const label = String(entry.node.label || entry.node.title || entry.node.heading || entry.node.role || entry.node.kind || entry.node.id || "");
    if (/(selected.*(?:mod|detail|inspector|panel)|mod.*detail|selectedModDetail|selectedDetailPanel)/i.test(`${pathText} ${label}`)) {
      candidates.push(entry);
    }
  }
  return candidates;
}

function selectedModIdentityValues(selectedMod) {
  return nonEmptyValuesForKey(selectedMod, /^(packageId|rowId|id|modId|detectedModId|workshopId|steamWorkshopId|subscriptionId)$/i).map(String);
}

function selectedModNameValues(selectedMod) {
  return nonEmptyValuesForKey(selectedMod, /^(name|displayName|title|label)$/i).map(String);
}

function selectedModSourceValues(selectedMod) {
  return nonEmptyValuesForKey(selectedMod, /^(source|origin|type|kind|provenanceLabel|provenanceKey)$/i)
    .filter((value) => value === null || ["string", "number", "boolean"].includes(typeof value))
    .map(String);
}

function selectedModPathValues(selectedMod) {
  return nonEmptyValuesForKey(selectedMod, /(path|root|directory|dir|folder|location)$/i).map(String);
}

function detailContainsAny(detail, values) {
  const text = objectText(detail);
  return values.some((value) => value && new RegExp(escapeRegex(value), "i").test(text));
}

function directDetailValueMatches(detail, keyPattern, expectedValues) {
  const expected = expectedValues.map(String).filter(Boolean);
  if (!expected.length) return true;
  const direct = nonEmptyValuesForKey(detail, keyPattern).map(String);
  return direct.some((value) => expected.some((expectedValue) => value === expectedValue || value.includes(expectedValue) || expectedValue.includes(value)));
}

function detailSelectedStateMatches(detail, selectedMod) {
  const expected = selectedModEnabledInProfile(selectedMod);
  if (expected === null) return true;
  const direct = firstBooleanValueForKey(detail, /^(enabledInProfile|profileEnabled|activeInProfile|isEnabledInProfile|enabled|isEnabled|active)$/i);
  if (direct !== null) return direct === expected;
  const text = objectText(detail);
  if (expected === true) return /\b(enabled|active)\b/i.test(text);
  return /\b(disabled|inactive|not enabled|subscribed|unavailable)\b/i.test(text);
}

function collectActionEntriesFromNode(node) {
  const entries = [];
  walk(node, (current, pathParts) => {
    if (!current || typeof current !== "object") return;
    const key = pathParts[pathParts.length - 1] || "";
    const previousKey = pathParts[pathParts.length - 2] || "";
    if (Array.isArray(current)) {
      if (/(actions|buttons|controls|ctas)$/i.test(key)) {
        current.forEach((entry) => entries.push(entry));
      }
      return;
    }
    if (/(primaryAction|secondaryAction|contextualAction|detailAction)$/i.test(key)) {
      entries.push(current);
      return;
    }
    if (/(actionEligibility|actionEligibilityById|actionsById|availableActions|eligibleActions|buttons|controls|contextualActions|detailActions)$/i.test(previousKey)) {
      entries.push({ id: key, actionId: key, ...current });
      return;
    }
    if (objectLooksLikeAction(current, "enable") || objectLooksLikeAction(current, "disable") || objectLooksLikeAction(current, "publish")) {
      entries.push(current);
    }
  });
  return entries;
}

function detailActionState(detail, actionName) {
  const direct = firstBooleanValueForKey(detail, actionDirectKeyPattern(actionName));
  const state = { available: direct, primary: null, evidence: [] };
  for (const entry of collectActionEntriesFromNode(detail)) {
    if (!objectLooksLikeAction(entry, actionName)) continue;
    const available = actionAvailabilityFromObject(entry);
    if (available !== null) state.available = available;
    if (rowBoolean(entry, /^(primary|isPrimary|default|isDefault)$/i, true) || /primary/i.test(String(actionRank(entry)))) state.primary = true;
    state.evidence.push(objectText(entry));
  }
  return state;
}

function requireSelectedModDetailMatches(world, detail, selectedMod) {
  const identityValues = selectedModIdentityValues(selectedMod);
  const nameValues = selectedModNameValues(selectedMod);
  const sourceValues = selectedModSourceValues(selectedMod);
  const pathValues = selectedModPathValues(selectedMod);
  const missing = [];
  if (!detailContainsAny(detail, nameValues)) missing.push("selected name");
  if (!detailContainsAny(detail, identityValues) && !directDetailValueMatches(detail, /^(packageId|rowId|id|modId|detectedModId|workshopId|steamWorkshopId|subscriptionId)$/i, identityValues)) {
    missing.push("selected package/id");
  }
  if (sourceValues.length && !detailContainsAny(detail, sourceValues) && !directDetailValueMatches(detail, /^(provenance|source|origin|type|kind|provenanceLabel|provenanceKey)$/i, sourceValues)) {
    missing.push("selected source/provenance");
  }
  if (pathValues.length && !detailContainsAny(detail, pathValues) && !directDetailValueMatches(detail, /(path|root|directory|dir|folder|location)$/i, pathValues)) {
    missing.push("selected path");
  }
  if (!detailSelectedStateMatches(detail, selectedMod)) missing.push("selected enabled/profile state");
  if (missing.length) {
    contractGap(
      world,
      `selected mod detail panel does not match selectedMod fields: ${missing.join(", ")}`,
      `selectedMod:\n${objectText(selectedMod) || "<none>"}\nDETAIL:\n${objectText(detail) || "<none>"}`
    );
  }
}

function requireSelectedModDetailActions(world, detail, selectedMod) {
  const entries = collectActionEntriesFromNode(detail);
  if (!entries.length) {
    contractGap(
      world,
      "selected mod detail panel lacks contextual action entries/buttons",
      `DETAIL:\n${objectText(detail) || "<none>"}`
    );
  }
  for (const actionName of ["enable", "disable", "publish"]) {
    const selectedState = selectedModActionState(selectedMod, actionName);
    if (selectedState.available === null) continue;
    const detailState = detailActionState(detail, actionName);
    if (detailState.available !== selectedState.available) {
      contractGap(
        world,
        `selected mod detail action availability for ${actionName} is not derived from selectedMod`,
        `Expected available=${selectedState.available} from selectedMod.\nDetail observed: ${JSON.stringify(detailState)}\nselectedMod:\n${objectText(selectedMod)}\nDETAIL:\n${objectText(detail)}`
      );
    }
    if (/^(enable|disable)$/i.test(actionName) && selectedState.primary === true && detailState.primary !== true) {
      contractGap(
        world,
        `selected mod detail does not mark ${actionName} as the contextual primary action`,
        `selectedMod state: ${JSON.stringify(selectedState)}\nDetail observed: ${JSON.stringify(detailState)}\nDETAIL:\n${objectText(detail)}`
      );
    }
  }
}

function requireSelectedModDetailPanel(world) {
  const report = world.profileFirstGuiReport || {};
  const selectedMod = selectedModObject(world);
  const candidates = selectedModDetailCandidates(report).filter(meaningfulObject);
  if (!candidates.length) {
    contractGap(
      world,
      "missing right-side selected mod detail panel (expected selectedModDetail, selectedDetailPanel, or equivalent)",
      `selectedMod:\n${objectText(selectedMod) || "<none>"}\nREPORT PREVIEW:\n${reportPreview(world)}`
    );
  }
  const panel = candidates.find((entry) => detailContainsAny(entry.node, selectedModNameValues(selectedMod))) || candidates[0];
  requireSelectedModDetailMatches(world, panel.node, selectedMod);
  requireSelectedModDetailActions(world, panel.node, selectedMod);
  return panel.node;
}

function requireSelectedModWorkshopSubscription(world) {
  const selectedMod = selectedModObject(world);
  requireCanonicalSelectedModFields(world, selectedMod);
  const idText = valuesForKey(selectedMod, /^(rowId|id|modId|detectedModId|workshopId|steamWorkshopId|subscriptionId)$/i)
    .map((entry) => String(entry.value))
    .join(" ");
  if (!new RegExp(escapeRegex(WORKSHOP_FIXTURE_ID), "i").test(idText)) {
    contractGap(
      world,
      "canonical selectedMod identity does not match the selected Workshop subscription",
      `Expected id/rowId/workshopId=${WORKSHOP_FIXTURE_ID}\nIDENTITY: ${idText || "<none>"}\nselectedMod:\n${objectText(selectedMod)}`
    );
  }
  const titleText = valuesForKey(selectedMod, /^(name|displayName|title|label)$/i).map((entry) => String(entry.value)).join(" ");
  if (!new RegExp(escapeRegex(WORKSHOP_FIXTURE_TITLE), "i").test(titleText)) {
    contractGap(
      world,
      "canonical selectedMod name/title does not match the selected Workshop subscription",
      `Expected title=${WORKSHOP_FIXTURE_TITLE}\nTITLE: ${titleText || "<none>"}\nselectedMod:\n${objectText(selectedMod)}`
    );
  }
  const provenanceText = valuesForKey(selectedMod, /^(provenance|source|origin|type|kind)$/i).map((entry) => String(entry.value)).join(" ");
  if (!/workshop|steam|subscription/i.test(provenanceText)) {
    contractGap(
      world,
      "canonical selectedMod provenance/type does not identify a Workshop subscription",
      `PROVENANCE: ${provenanceText || "<none>"}\nselectedMod:\n${objectText(selectedMod)}`
    );
  }
  return selectedMod;
}
function smokeSelectedMod(report) {
  const direct = report && report.smokeSelectedMod;
  if (direct !== undefined && direct !== null) return direct;
  const values = valuesForKey(report, /^smokeSelectedMod$/i).map((entry) => entry.value);
  return values.length ? values[0] : null;
}

function selectedDetectedModId(report) {
  if (!report) return null;
  if (report.selectedDetectedModId !== undefined && report.selectedDetectedModId !== null) return String(report.selectedDetectedModId);
  if (report.selectedDetectedMod && typeof report.selectedDetectedMod === "object") {
    for (const key of ["id", "modId", "packageId", "slug", "name", "displayName"]) {
      if (report.selectedDetectedMod[key] !== undefined && report.selectedDetectedMod[key] !== null) return String(report.selectedDetectedMod[key]);
    }
  }
  return null;
}

function selectedPackageEvidence(report) {
  const direct = [
    report && report.selectedPackage,
    report && report.selectedPackageDetails,
    report && report.selectedModPackage,
    report && report.packageDetails,
  ].find((value) => value && typeof value === "object");
  if (direct) return direct;
  const candidates = candidateObjectsAtPaths(report, [
    ["modsList", "selectedPackage"],
    ["modsList", "selectedPackageDetails"],
    ["modsSidebar", "selectedPackage"],
    ["modsSidebar", "selectedPackageDetails"],
    ["selectedDetectedMod", "package"],
    ["selectedDetectedMod", "bmlPackage"],
  ]);
  return candidates.length ? candidates[0].node : null;
}

function normalizeActionId(value) {
  return String(value || "")
    .trim()
    .toLowerCase()
    .replace(/runebound:\s*elixirs/g, "runebound-elixirs")
    .replace(/[^a-z0-9]+/g, "-")
    .replace(/^-+|-+$/g, "");
}

function actionIdForEntry(entry) {
  if (typeof entry === "string") return normalizeActionId(entry);
  if (!entry || typeof entry !== "object") return "";
  for (const key of ["actionId", "id", "action", "command", "buttonAction", "name", "label", "text"]) {
    if (typeof entry[key] === "string" && entry[key].trim()) return normalizeActionId(entry[key]);
  }
  return normalizeActionId(structuralText(entry).split("\n")[0] || "");
}

function clickedActionEntries(report, actionId) {
  const raw = report && report.clickedActions;
  const actions = Array.isArray(raw) ? raw : (raw && typeof raw === "object" ? Object.values(raw) : []);
  const normalized = normalizeActionId(actionId);
  return actions.filter((entry) => actionIdForEntry(entry) === normalized);
}

function actionLogEntries(report, actionId) {
  const candidates = [];
  for (const key of ["actionLog", "actions", "visibleActivityLog"]) {
    const value = report && report[key];
    if (Array.isArray(value)) candidates.push(...value);
  }
  const normalized = normalizeActionId(actionId);
  return candidates.filter((entry) => actionIdForEntry(entry) === normalized || normalizeActionId(objectText(entry)).includes(normalized));
}

function activeModTextsFrom(value) {
  if (!value) return [];
  if (Array.isArray(value)) return value.map((item) => objectText(item));
  const matches = [];
  walk(value, (node, pathParts) => {
    const key = pathParts[pathParts.length - 1] || "";
    if (/^(activeMods|enabledMods|selectedMods|beforeActiveMods|afterActiveMods)$/i.test(key) && Array.isArray(node)) {
      node.forEach((item) => matches.push(objectText(item)));
    }
  });
  return matches;
}

function activeModsContain(value, packageId, packageName) {
  const pattern = packagePattern(packageId, packageName);
  return activeModTextsFrom(value).some((text) => pattern.test(text));
}

function explicitTargetPackageValues(value) {
  return valuesForKey(value, /^(packageId|selectedPackageId|targetPackageId|modId|selectedModId|selectedDetectedModId)$/i)
    .map((entry) => entry.value)
    .filter((target) => target !== undefined && target !== null)
    .map(String);
}

function entryTargetsPackage(entry, packageId, packageName) {
  const pattern = packagePattern(packageId, packageName);
  return explicitTargetPackageValues(entry).some((target) => pattern.test(target)) || pattern.test(objectText(entry));
}

function trueVisualClippingFlags(value) {
  const matches = [];
  walk(value, (node, pathParts) => {
    const key = pathParts[pathParts.length - 1] || "";
    if (!/(clipped|truncated|overflow|wrappedTextCutOff|textCutOff)$/i.test(key)) return;
    if (node === true || /^(true|yes|clipped|truncated|overflow)$/i.test(String(node))) {
      matches.push(`${pathParts.join(".")}=${String(node)}`);
    }
  });
  return matches;
}

function objectHasHiddenEvidence(node) {
  return booleanValuesForKey(node, /^(hidden|isHidden|withdrawn|isWithdrawn|suppressed|windowHidden)$/i).some(Boolean) ||
    valuesForKey(node, /^(visible|isVisible|shown|isShown|rendered|isRendered)$/i).some((entry) => entry.value === false) ||
    rowStringValues(node, /(display|visibility|state|windowState)$/i).some((value) => /\b(hidden|withdrawn|none|suppressed)\b/i.test(value));
}

function objectHasAutofocusEvidence(node) {
  return booleanValuesForKey(node, /^(autofocus|autoFocus|autoFocused|focused|isFocused|hasFocus|initialFocus|focusRequested)$/i).some(Boolean) ||
    rowStringValues(node, /(focus|autofocus|activeElement)$/i).some((value) => /\b(auto\s*focus|autofocused|focused|has focus|initial focus)\b/i.test(value));
}

function hiddenAutofocusViolations(value) {
  const matches = [];
  walk(value, (node, pathParts) => {
    if (!node || typeof node !== "object" || Array.isArray(node)) return;
    if (objectHasHiddenEvidence(node) && objectHasAutofocusEvidence(node)) {
      matches.push(`${pathParts.join(".") || "<root>"}: ${objectText(node)}`);
    }
  });
  return matches;
}

function requireHiddenNoAutofocusMetadata(world) {
  const report = world.profileFirstGuiReport || {};
  const violations = hiddenAutofocusViolations(report);
  if (violations.length) {
    contractGap(world, "hidden smoke UI metadata shows autofocus/focus on hidden widgets", `VIOLATIONS:\n${violations.join("\n---\n")}`);
  }
  const methods = Array.isArray(report.noAutofocusMethods) ? report.noAutofocusMethods : [];
  const suppressionMethods = Array.isArray(report.smokeWindowSuppressionMethods) ? report.smokeWindowSuppressionMethods : [];
  const failures = [];
  if (report.noAutofocusRequested !== true) failures.push("noAutofocusRequested=true");
  if (report.noAutofocusApplied !== true) failures.push("noAutofocusApplied=true");
  if (report.smokeVisibleRequested !== false) failures.push("smokeVisibleRequested=false");
  if (report.smokeWindowHidden !== true) failures.push("smokeWindowHidden=true");
  if (!methods.length) failures.push("non-empty noAutofocusMethods");
  if (!suppressionMethods.length) failures.push("non-empty smokeWindowSuppressionMethods");
  if (report.noAutofocusError || report.smokeWindowSuppressionError) failures.push("no no-autofocus/window-suppression errors");
  if (failures.length) {
    contractGap(
      world,
      `hidden smoke no-autofocus metadata is incomplete: expected ${failures.join(", ")}`,
      `noAutofocusRequested=${report.noAutofocusRequested}\nnoAutofocusApplied=${report.noAutofocusApplied}\n` +
      `noAutofocusMethods=${JSON.stringify(methods)}\nsmokeVisibleRequested=${report.smokeVisibleRequested}\n` +
      `smokeWindowHidden=${report.smokeWindowHidden}\nsmokeWindowSuppressionMethods=${JSON.stringify(suppressionMethods)}`
    );
  }
}

function visualTextMetadataEntries(value) {
  const entries = [];
  walk(value, (node, pathParts) => {
    if (!node || typeof node !== "object" || Array.isArray(node)) return;
    const text = labelText(node) || objectText(node);
    if (!text) return;
    const metadataText = Object.keys(node).join(" ");
    if (/(?:renderedTextCompleteness|textComplete|fullyRendered|completeText|min(?:imum)?Width|min_width|requiredWidth|allocatedWidth|measuredWidth|clipped|truncated|overflow|cutOff|fits)/i.test(metadataText)) {
      entries.push({ node, path: pathParts.join("."), text });
    }
  });
  return entries;
}

function visualEntryHasNoClippingEvidence(entry) {
  const node = entry.node;
  if (trueVisualClippingFlags(node).length) return false;
  if (booleanValuesForKey(node, /^(textComplete|fullyRendered|complete|isComplete|notClipped|noClip|noClipping|fits|meetsMinimumWidth)$/i).some(Boolean)) return true;
  if (booleanValuesForKey(node, /^(clipped|truncated|overflow|wrappedTextCutOff|textCutOff|isClipped|isTruncated)$/i).some((value) => value === false)) return true;
  if (valuesForKey(node, /^(renderedTextCompleteness|textCompleteness)$/i).some((entryValue) => /^(complete|ok|true|full|not clipped|fits)$/i.test(String(entryValue.value)))) return true;
  const minimumWidths = numericValuesForKey(node, /(min(?:imum)?Width|min_width|requiredWidth|requestedWidth)$/i);
  const allocatedWidths = numericValuesForKey(node, /(allocatedWidth|actualWidth|measuredWidth|renderedWidth|width)$/i);
  return minimumWidths.some((minimum) => minimum > 0) && allocatedWidths.some((allocated) => allocated >= minimum);
}

function requireImportantNoClippingMetadata(world) {
  const report = world.profileFirstGuiReport || {};
  const entries = visualTextMetadataEntries(report);
  const important = [
    ["Mods list title", /\bMods\b/i],
    ["Environment label", /\bEnvironment\b/i],
    ["Profiles label", /\bProfiles?\b/i],
    ["Workshop label", /\bWorkshop\b/i],
    ["Recent Activity label", /\bRecent Activity\b/i],
    ["Enable selected mod button", /\bEnable selected mod\b/i],
    ["Disable selected mod button", /\bDisable selected mod\b/i],
    ["Launch BaronyModLoader button", /\bLaunch BaronyModLoader\b/i],
    ["Launch Vanilla Barony button", /\bLaunch Vanilla Barony\b/i],
    ["Refresh readiness button", /\bRefresh readiness\b/i],
    ["Open diagnostics button", /\bOpen diagnostics\b/i],
    ["Workshop no-publish warning", /Steam publish(?:ing)? (?:remains )?disabled|Workshop preparation is dry-run only|No-publish guard/i],
  ];
  const missing = important.filter(([, pattern]) => !entries.some((entry) => pattern.test(entry.text) && visualEntryHasNoClippingEvidence(entry)));
  if (missing.length) {
    contractGap(
      world,
      `smoke report lacks minimum-width/renderedTextCompleteness no-clipping metadata for: ${missing.map(([name]) => name).join(", ")}`,
      `VISUAL TEXT METADATA:\n${entries.map((entry) => `${entry.path}: ${entry.text}`).join("\n") || "<none>"}`
    );
  }
}

function rightSideStructuredLabels(world) {
  const report = world.profileFirstGuiReport || {};
  const labels = [];
  for (const candidatePath of [
    ["rightSideLabels"],
    ["rightPaneLabels"],
    ["rightColumnLabels"],
    ["rightSide", "labels"],
    ["rightPane", "labels"],
    ["rightColumn", "labels"],
  ]) {
    const value = getPathValue(report, candidatePath);
    if (Array.isArray(value)) labels.push(...value.map(labelText).filter(Boolean));
  }
  const right = requireRightSide(world);
  for (const key of ["cards", "sections", "panels", "concepts", "items", "children"]) {
    if (Array.isArray(right.node[key])) labels.push(...right.node[key].map(labelText).filter(Boolean));
  }
  return labels;
}

function requireNoSeparateRightSideModsCard(world) {
  const labels = rightSideStructuredLabels(world);
  if (!labels.length) {
    contractGap(
      world,
      "right side lacks structured labels needed to prove there is no separate Mods card",
      "Expected rightSideLabels or rightSide.cards/sections/panels labels."
    );
  }
  const modsLabels = labels.filter((label) => /^Mods?$/i.test(label.trim()));
  if (modsLabels.length) {
    contractGap(
      world,
      "right side still exposes a separate Mods card",
      `RIGHT-SIDE LABELS:\n${labels.join("\n")}`
    );
  }
}

function requireRightSide(world) {
  const candidates = candidateObjectsAtPaths(world.profileFirstGuiReport, [
    ["rightSide"],
    ["rightPane"],
    ["rightColumn"],
    ["renderedRightSide"],
    ["mainBody", "right"],
    ["mainBody", "rightSide"],
    ["layout", "right"],
    ["layout", "rightSide"],
  ]);
  for (const entry of allObjects(world.profileFirstGuiReport)) {
    const pathText = entry.pathParts.join(".");
    if (/(^|\.)(right|rightSide|rightPane|rightColumn)(\.|$)/i.test(pathText)) candidates.push(entry);
  }
  const fallback = {
    labels: topLevelLabels(world.profileFirstGuiReport),
    concepts: world.profileFirstGuiReport && world.profileFirstGuiReport.concepts,
    environmentSummaryItems: world.profileFirstGuiReport && world.profileFirstGuiReport.environmentSummaryItems,
    visibleActivityLog: world.profileFirstGuiReport && world.profileFirstGuiReport.visibleActivityLog,
    activityLog: world.profileFirstGuiReport && world.profileFirstGuiReport.actionLog,
  };
  const right = candidates.find((entry) => /Environment/i.test(objectText(entry.node))) || (candidates[0] || { node: fallback, pathParts: ["<right-side-fallback>"] });
  const text = objectText(right.node);
  const missing = [
    ["Environment", /Environment|environmentSummaryItems/i],
    ["Profiles", /Profiles?/i],
    ["Workshop", /Workshop/i],
    ["Recent Activity", /Recent Activity|Activity Log|visibleActivityLog|actionLog/i],
  ].filter(([, pattern]) => !pattern.test(text)).map(([label]) => label);
  if (missing.length) {
    contractGap(
      world,
      `right side is missing expected cards/regions: ${missing.join(", ")}`,
      `RIGHT-SIDE EVIDENCE:\n${text || "<none>"}`
    );
  }
  return right;
}

function rightStatusContainerCandidates(world) {
  const report = world.profileFirstGuiReport || {};
  const candidates = [];
  for (const candidatePath of [
    ["compactStatusCards"],
    ["compactStatusStrip"],
    ["statusCards"],
    ["statusStrip"],
    ["rightSide", "compactStatusCards"],
    ["rightSide", "compactStatusStrip"],
    ["rightSide", "statusCards"],
    ["rightSide", "statusStrip"],
    ["rightSide", "statusItems"],
    ["rightSide", "summaryStrip"],
    ["rightPane", "compactStatusCards"],
    ["rightPane", "statusStrip"],
    ["rightColumn", "compactStatusCards"],
    ["rightColumn", "statusStrip"],
    ["mainBody", "right", "compactStatusCards"],
    ["mainBody", "right", "statusStrip"],
    ["mainBody", "rightSide", "compactStatusCards"],
    ["mainBody", "rightSide", "statusStrip"],
  ]) {
    const value = getPathValue(report, candidatePath);
    if (Array.isArray(value) || (value && typeof value === "object")) {
      candidates.push({ node: value, pathParts: candidatePath });
    }
  }
  for (const entry of allObjects(report)) {
    const pathText = entry.pathParts.join(".");
    const label = String(entry.node.label || entry.node.title || entry.node.role || entry.node.kind || entry.node.id || "");
    if (/(compact.*status|status.*(?:strip|cards|rail|summary|items)|summaryStrip)/i.test(`${pathText} ${label}`)) {
      candidates.push(entry);
    }
  }
  return candidates;
}

function statusItemsFromContainer(container) {
  if (Array.isArray(container)) return container;
  if (!container || typeof container !== "object") return [];
  const items = [];
  for (const key of ["cards", "items", "statusCards", "statusItems", "statuses", "sections", "children"]) {
    if (Array.isArray(container[key])) items.push(...container[key]);
  }
  return items.length ? items : [container];
}

function statusItemFor(items, kind) {
  const pattern = {
    environment: /\bEnvironment\b|^env$/i,
    profile: /\bProfiles?\b|profile-count|profileCount/i,
    workshop: /\bWorkshop\b|Steam Workshop/i,
  }[kind];
  return items.find((item) => pattern.test(`${labelText(item)} ${objectText(item)}`));
}

function hasCompactStatusEvidence(candidate) {
  const pathText = candidate.pathParts.join(".");
  const text = objectText(candidate.node);
  return (
    /(compact|strip|status|summary)/i.test(pathText) ||
    booleanFlag(candidate.node, /^(compact|isCompact|dense|isDense|small|statusCard|isStatusCard)$/i, true) ||
    /\b(compact|strip|status-card|status card|dense)\b/i.test(text)
  );
}

function statusHasProfileCount(profileItem) {
  const numeric = valuesForKey(profileItem, /^(profileCount|profilesCount|availableProfileCount|totalProfiles|count)$/i)
    .some((entry) => typeof entry.value === "number" || /^\d+$/.test(String(entry.value)));
  return numeric || /\bProfiles?\b[^0-9]{0,24}\d+|\d+[^A-Za-z0-9]{0,12}\bprofiles?\b/i.test(objectText(profileItem));
}

function statusHasWorkshopNoPublish(workshopItem) {
  const dryRun = booleanFlag(workshopItem, /^(dryRun|dry-run|isDryRun)$/i, true) || /dry[-\s]?run/i.test(objectText(workshopItem));
  const noPublish =
    booleanFlag(workshopItem, /^(noPublish|no-publish|publishDisabled|steamPublishingDisabled)$/i, true) ||
    booleanFlag(workshopItem, /^(publishEnabled|steamPublishEnabled|steamSideEffects)$/i, false) ||
    /no[-\s]?publish|publish(?:ing)? (?:remains )?disabled|Steam publish(?:ing)? remains disabled|Steam publishing is disabled/i.test(objectText(workshopItem));
  return dryRun && noPublish;
}

function requireCompactRightStatusCards(world) {
  const candidates = rightStatusContainerCandidates(world);
  const matching = candidates
    .map((candidate) => ({ candidate, items: statusItemsFromContainer(candidate.node) }))
    .find(({ candidate, items }) => (
      hasCompactStatusEvidence(candidate) &&
      statusItemFor(items, "environment") &&
      statusItemFor(items, "profile") &&
      statusItemFor(items, "workshop")
    ));
  if (!matching) {
    contractGap(
      world,
      "missing compact right-side Environment/Profile/Workshop status cards or status strip",
      `RIGHT SIDE:\n${objectText(requireRightSide(world).node) || "<none>"}`
    );
  }
  const environment = statusItemFor(matching.items, "environment");
  const profile = statusItemFor(matching.items, "profile");
  const workshop = statusItemFor(matching.items, "workshop");
  const environmentText = objectText(environment);
  const missingEnvironment = [
    ["OS", /\bOS\b|operating system|Linux|Windows|Darwin|macOS/i],
    ["Steam", /\bSteam\b|storefront/i],
    ["Game version", /game[-\s]?version|gameVersion|\bversion\b|v\d+(?:\.\d+)+/i],
  ].filter(([, pattern]) => !pattern.test(environmentText)).map(([label]) => label);
  if (missingEnvironment.length) {
    contractGap(
      world,
      `compact Environment status card is missing ${missingEnvironment.join(", ")}`,
      `ENVIRONMENT STATUS:\n${environmentText || "<none>"}`
    );
  }
  if (!statusHasProfileCount(profile)) {
    contractGap(
      world,
      "compact Profile status card is missing profile count evidence",
      `PROFILE STATUS:\n${objectText(profile) || "<none>"}`
    );
  }
  if (!statusHasWorkshopNoPublish(workshop)) {
    contractGap(
      world,
      "compact Workshop status card is missing dry-run/no-publish evidence",
      `WORKSHOP STATUS:\n${objectText(workshop) || "<none>"}`
    );
  }
  return { environment, profile, workshop };
}

function compactWorkshopStatusCard(world) {
  const statusCards = requireCompactRightStatusCards(world);
  return statusCards.workshop;
}

function directTextFields(node, keyPattern) {
  if (!node || typeof node !== "object" || Array.isArray(node)) return [];
  return Object.entries(node)
    .filter(([key, value]) => keyPattern.test(key) && ["string", "number", "boolean"].includes(typeof value))
    .map(([key, value]) => ({ key, value: String(value) }));
}

function directDetailFields(node) {
  if (!node || typeof node !== "object" || Array.isArray(node)) return [];
  return Object.entries(node)
    .filter(([key, value]) => /(details?|diagnosticsDetails|report|state|metadata|evidence|blockers|warnings|disabledReasons|problems)$/i.test(key) && value !== undefined && value !== null)
    .map(([key, value]) => ({ key, value }));
}

function visibleActivityEntries(report) {
  const raw = report && (report.visibleActivityLog || report.recentActivity || (report.rightSide && report.rightSide.recentActivity));
  if (Array.isArray(raw)) return raw;
  if (typeof raw === "string") return raw.split(/\r?\n/).filter((line) => line.trim());
  if (raw && typeof raw === "object") {
    for (const key of ["entries", "items", "messages", "lines", "actions", "log"]) {
      if (Array.isArray(raw[key])) return raw[key];
    }
    return [raw];
  }
  return [];
}

function visibleActivityLineText(entry) {
  if (typeof entry === "string") return entry.trim();
  if (!entry || typeof entry !== "object") return String(entry || "").trim();
  const primary = directTextFields(entry, /^(primaryText|visibleText|displayText|line|message|summary|text|label)$/i);
  if (primary.length) return primary.map((field) => field.value).join(" — ").replace(/\s+/g, " ").trim();
  return objectText(entry);
}

function detailedActionEntries(report) {
  const raw = report && report.actionLog;
  if (Array.isArray(raw)) return raw;
  if (raw && typeof raw === "object") return Object.values(raw).filter((entry) => entry && typeof entry === "object");
  return [];
}

function actionDetailsText(entry) {
  if (!entry || typeof entry !== "object") return "";
  const omitted = new Set(["id", "label", "status", "summary", "primaryText", "visibleText", "displayText", "line", "message", "text", "generatedAt"]);
  const detail = {};
  Object.entries(entry).forEach(([key, value]) => {
    if (!omitted.has(key)) detail[key] = value;
  });
  const explicit = directDetailFields(entry).map((field) => objectText(field.value)).join(" ");
  return `${explicit} ${objectText(detail)}`.trim();
}

function assertVisibleActivityLogConcise(world) {
  const report = world.profileFirstGuiReport || {};
  const entries = visibleActivityEntries(report);
  if (!entries.length) {
    contractGap(world, "smoke report does not expose visible Recent Activity entries");
  }
  const verbosePattern = /(\/home\/|sha256:|manifestPath|runtimeManifestPath|selectedModPath|activeMods|disabledReasons|traceback|\[object Object\]|\{|\})/i;
  const problems = [];
  for (const entry of entries) {
    const text = visibleActivityLineText(entry);
    if (!text) problems.push("<empty activity line>");
    if (text.length > 96) problems.push(`too long (${text.length} chars): ${text}`);
    if (verbosePattern.test(text)) problems.push(`verbose detail leaked into visible line: ${text}`);
    if (!/(ready|enabled|disabled|blocked|selected|created|refreshed|dry[- ]?run|no[- ]?publish|publishing disabled|done|ok|result)/i.test(text)) {
      problems.push(`missing visible result/status: ${text}`);
    }
    if (!/(Runebound|Stash|jml\.|Workshop|Steam|readiness|launch|profile|packages?|mods?|install|diagnostics)/i.test(text)) {
      problems.push(`missing target name: ${text}`);
    }
  }
  if (problems.length) {
    contractGap(world, "visible Recent Activity entries are not concise action + target + result lines", problems.join("\n"));
  }

  const actionDetails = detailedActionEntries(report).map(actionDetailsText).filter(Boolean);
  if (!actionDetails.some((text) => /(packageId|selectedModName|activeMods|runtimeManifestPath|stagingFolder|blockers|reportCount|productionEvidenceAvailable)/i.test(text))) {
    contractGap(
      world,
      "action log does not retain verbose data outside visible Recent Activity lines",
      `ACTION LOG:\n${objectText(report.actionLog) || "<none>"}`
    );
  }
}

function assertCompactWorkshopPrimaryText(world) {
  const workshop = compactWorkshopStatusCard(world);
  const primaryFields = directTextFields(workshop, /^(title|label|status|summary|statusSummary|primaryText|visibleText|displayText|badge|badgeText)$/i);
  const primaryText = primaryFields.map((field) => field.value).join(" ");
  if (!/dry[-\s]?run/i.test(primaryText) || !/(no[-\s]?publish|publish(?:ing)? disabled|publishing remains disabled)/i.test(primaryText)) {
    contractGap(
      world,
      "compact Workshop card primary text does not show dry-run/no-publish state",
      `PRIMARY TEXT:\n${primaryText || "<none>"}\nWORKSHOP CARD:\n${objectText(workshop)}`
    );
  }

  const detailFields = directDetailFields(workshop);
  if (!primaryFields.length || !detailFields.length) return;
  const tooLong = primaryFields.filter((field) => field.value.length > 120).map((field) => `${field.key}=${field.value}`);
  const verbosePrimary = primaryFields
    .filter((field) => /(\/home\/|\.json|\.vdf|disabledReasons|blockers|problems|traceback|checksum|manifest|stagingFolder|previewAssets)/i.test(field.value))
    .map((field) => `${field.key}=${field.value}`);
  const detailSnippets = detailFields
    .flatMap((field) => objectText(field.value).split(/\s{2,}|\n|;/))
    .map((text) => text.trim())
    .filter((text) => text.length > 60 || /(\/home\/|disabledReasons|blockers|problems|manifest|staging|preview|metadata|report)/i.test(text));
  const repeatedDetails = detailSnippets.filter((snippet) => primaryText.includes(snippet.slice(0, Math.min(snippet.length, 80))));
  if (tooLong.length || verbosePrimary.length || repeatedDetails.length) {
    contractGap(
      world,
      "compact Workshop card leaks long blocker/detail text into primary visible fields",
      [
        tooLong.length ? `TOO LONG:\n${tooLong.join("\n")}` : "",
        verbosePrimary.length ? `VERBOSE PRIMARY:\n${verbosePrimary.join("\n")}` : "",
        repeatedDetails.length ? `REPEATED DETAILS:\n${repeatedDetails.join("\n")}` : "",
        `DETAIL FIELDS:\n${detailFields.map((field) => field.key).join(", ")}`,
      ].filter(Boolean).join("\n")
    );
  }
}

function diagnosticsDetailCandidates(report) {
  const candidates = candidateObjectsAtPaths(report, [
    ["diagnosticsDetails"],
    ["diagnosticsEvidence"],
    ["details", "diagnostics"],
    ["rightSide", "diagnosticsDetails"],
    ["conceptMap", "environment", "diagnosticsDetails"],
    ["conceptMap", "environment", "details", "diagnostics"],
    ["conceptMap", "environment", "state", "diagnosticsEvidence"],
  ]);
  for (const entry of allObjects(report)) {
    const pathText = entry.pathParts.join(".");
    if (/diagnostics(?:Details|Evidence|Report|Reports)?$/i.test(pathText)) candidates.push(entry);
  }
  return candidates;
}

function assertDiagnosticsDetailsRemainAvailable(world) {
  const candidates = diagnosticsDetailCandidates(world.profileFirstGuiReport || {});
  const rich = candidates.find((entry) => {
    const text = objectText(entry.node);
    return /runtime-load-report|production|loaded|missing|reports? checked|classification|loadedMods|items/i.test(text) &&
      (Array.isArray(entry.node.items) || /runtime-load-report|production/i.test(text));
  });
  if (!rich) {
    contractGap(
      world,
      "diagnostic details/report evidence is not retained in smoke report details fields",
      `DIAGNOSTIC CANDIDATES:\n${candidates.map((entry) => `${entry.pathParts.join(".")}: ${objectText(entry.node)}`).join("\n---\n") || "<none>"}`
    );
  }
}

function environmentCandidates(report) {
  const candidates = candidateObjectsAtPaths(report, [
    ["environment"],
    ["environmentCard"],
    ["rightSide", "environment"],
    ["rightSide", "environmentCard"],
    ["mainBody", "right", "environment"],
    ["mainBody", "rightSide", "environment"],
  ]);
  for (const entry of allObjects(report)) {
    const pathText = entry.pathParts.join(".");
    if (/environment/i.test(pathText) || /^Environment$/i.test(String(entry.node.label || entry.node.title || ""))) candidates.push(entry);
  }
  return candidates;
}

function requireEnvironmentNode(world) {
  const rich = environmentCandidates(world.profileFirstGuiReport).find(meaningfulObject);
  if (rich) return rich.node;
  try {
    return requireConceptEntry(world, "Environment").node;
  } catch (error) {
    contractGap(world, "missing Environment card/state in smoke report");
  }
}

function environmentSummaryRows(world) {
  const report = world.profileFirstGuiReport;
  const directCandidates = [
    ["environmentSummaryItems"],
    ["environmentSummaryRows"],
    ["environment", "summaryItems"],
    ["environment", "summaryRows"],
    ["environmentCard", "summaryItems"],
    ["environmentCard", "summaryRows"],
    ["rightSide", "environment", "summaryItems"],
    ["rightSide", "environment", "summaryRows"],
    ["mainBody", "right", "environment", "summaryItems"],
    ["mainBody", "right", "environment", "summaryRows"],
  ];
  for (const candidatePath of directCandidates) {
    const value = getPathValue(report, candidatePath);
    if (Array.isArray(value)) return value;
  }
  for (const entry of allObjects(report)) {
    const pathText = entry.pathParts.join(".");
    if (!/environment/i.test(pathText)) continue;
    for (const key of ["summaryItems", "summaryRows", "rows", "items"]) {
      if (Array.isArray(entry.node[key]) && /summary|environment/i.test(`${pathText}.${key}`)) return entry.node[key];
    }
  }
  contractGap(
    world,
    "missing Environment summary rows/items field (expected environmentSummaryItems, environmentSummaryRows, environment.summaryItems, or equivalent structured rows)"
  );
}

function rowText(row) {
  return objectText(row);
}

function summaryRowDirectText(row) {
  if (!row || typeof row !== "object") return String(row || "");
  return ["id", "key", "label", "title", "name", "badge", "badgeLabel", "iconLabel"]
    .map((key) => row[key])
    .filter((value) => typeof value === "string")
    .join(" ");
}

function summaryRowFor(rows, kind) {
  const patterns = {
    os: [/^os$/i, /\bOS\b/i, /\[OS\]/i, /operating system/i],
    platform: [/^platform$/i, /\bPlatform\b/i, /\[PLATFORM\]/i],
    version: [/game-version/i, /gameVersion/i, /game version/i, /\[VERSION\]/i],
  }[kind];
  return rows.find((row) => patterns.some((pattern) => pattern.test(summaryRowDirectText(row))));
}

function compactLabelForSummaryRow(row) {
  if (!row || typeof row !== "object") return "";
  const candidates = [];
  for (const key of ["badge", "badgeLabel", "iconLabel", "compactLabel", "visualLabel", "prefix", "tag", "renderedLabel"]) {
    if (typeof row[key] === "string" && row[key].trim()) candidates.push(row[key].trim());
  }
  for (const entry of valuesForKey(row, /(badge|iconLabel|compactLabel|visualLabel|prefix|tag|renderedLabel)$/i)) {
    if (typeof entry.value === "string" && entry.value.trim()) candidates.push(entry.value.trim());
  }
  return candidates[0] || "";
}

function gameVersionSummaryRow(rows) {
  return rows.find((row) => /game-version|gameVersion|game version/i.test(summaryRowDirectText(row)));
}

function platformSteamLogoEvidence(world) {
  const report = world.profileFirstGuiReport || {};
  const logoPaths = valuesForKey(report, /(steamLogoPath|steamIconPath|platformSteamLogoPath|platformSteamIconPath|platformIconPath)$/i)
    .filter((entry) => typeof entry.value === "string" && /steam.*\.png$/i.test(entry.value));
  const renderedFlags = valuesForKey(report, /(steamLogoRendered|steamIconRendered|platformSteamLogoRendered|platformSteamIconRendered|platformIconRendered|steamLogoAvailable|steamIconAvailable|platformSteamLogoAvailable|platformSteamIconAvailable|platformIconAvailable)$/i)
    .filter((entry) => entry.value === true);
  return {
    paths: logoPaths,
    flags: renderedFlags,
    ok: logoPaths.length > 0 && renderedFlags.length > 0,
  };
}

const REQUIRED_ENTITY_ICON_TYPES = [
  "mods-list",
  "local-repo",
  "steam-workshop",
  "mod-package",
  "environment",
  "profiles",
  "workshop",
  "activity-log",
  "os",
  "platform-steam",
  "game-version",
];

function canonicalEntityIconType(value, entryText = "") {
  const raw = String(value || "").trim();
  const text = `${raw} ${entryText}`.replace(/[_\s]+/g, "-").toLowerCase();
  if (!text.trim()) return "";
  if (/mods?-?list|mods?-?panel|detected-mods?-?sidebar/.test(text)) return "mods-list";
  if (/local-?repo|local-repository|repo-mods?|local-mods?|local/.test(text)) return "local-repo";
  if (/steam-?workshop|workshop-subscriptions?|workshop-provenance/.test(text)) return "steam-workshop";
  if (/mod-?package|package-row|mod-row|individual-mod|bml-package/.test(text)) return "mod-package";
  if (/environment|runtime-environment/.test(text)) return "environment";
  if (/profiles?|profile-entity/.test(text)) return "profiles";
  if (/activity-?log|recent-activity|action-?log/.test(text)) return "activity-log";
  if (/game-?version|version-row/.test(text)) return "game-version";
  if (/\bos\b|operating-system/.test(text)) return "os";
  if (/platform-?steam|steam-platform|platform.*steam|steam.*platform/.test(text)) return "platform-steam";
  if (/workshop|publishing/.test(text)) return "workshop";
  return REQUIRED_ENTITY_ICON_TYPES.includes(text) ? text : "";
}

function iconographyEntryLike(node) {
  if (!node || typeof node !== "object" || Array.isArray(node)) return false;
  return valuesForKey(node, /(entityType|entity|type|kind|icon|glyph|emoji|symbol|logo|label|title|name|accessibleText|rendered|visible)$/i).length > 0;
}

function collectIconographyEntries(value, mapKey = "", entries = []) {
  if (Array.isArray(value)) {
    value.forEach((item) => collectIconographyEntries(item, mapKey, entries));
    return entries;
  }
  if (typeof value === "string" && mapKey) {
    entries.push({ mapKey, node: { label: value, renderedLabel: value } });
    return entries;
  }
  if (!value || typeof value !== "object") return entries;
  if (iconographyEntryLike(value)) entries.push({ mapKey, node: value });
  for (const [key, child] of Object.entries(value)) {
    if (child && typeof child === "object") collectIconographyEntries(child, key, entries);
    else if (typeof child === "string" && !iconographyEntryLike(value)) collectIconographyEntries(child, key, entries);
  }
  return entries;
}

function entityIconographyFields(report) {
  return valuesForKey(report, /^(entityIconography|renderedEntityIcons)$/i)
    .filter((entry) => entry.value && typeof entry.value === "object");
}

function entityIconographyEntries(world) {
  const fields = entityIconographyFields(world.profileFirstGuiReport || {});
  if (!fields.length) {
    contractGap(
      world,
      "missing entityIconography/renderedEntityIcons field in profile-first GUI smoke report"
    );
  }
  const entries = [];
  for (const field of fields) {
    collectIconographyEntries(field.value).forEach((entry) => {
      entries.push({ ...entry, fieldPath: field.pathParts.join(".") });
    });
  }
  return entries;
}

function entityIconTypeForEntry(entry) {
  const node = entry.node;
  const text = objectText(node);
  const explicit = ["entityType", "entity", "type", "kind", "id", "key", "slot"]
    .map((key) => node[key])
    .find((value) => typeof value === "string" && canonicalEntityIconType(value, text));
  return canonicalEntityIconType(explicit || entry.mapKey, text);
}

function iconEvidenceText(node) {
  return valuesForKey(node, /(icon|glyph|emoji|symbol|logo|image|asset|path|source|fallback|renderedLabel)$/i)
    .map((entry) => entry.value)
    .filter((value) => typeof value === "string" || value === true)
    .map(String)
    .join(" ");
}

function hasIconEvidence(node) {
  const evidence = iconEvidenceText(node);
  return /[^\w\s:./\\-]/u.test(evidence) || /\b(icon|glyph|emoji|symbol|logo|png|svg|fallback|steam)\b/i.test(evidence);
}

function labelEvidenceText(node) {
  return valuesForKey(node, /^(label|title|name|text|displayLabel|accessibleText|ariaLabel|caption|renderedLabel)$/i)
    .map((entry) => entry.value)
    .filter((value) => typeof value === "string")
    .join(" ");
}

function hasTextLabelEvidence(node) {
  return /[A-Za-z0-9]{2,}/.test(labelEvidenceText(node));
}

function falseRenderedIconFlags(node) {
  return valuesForKey(node, /^(rendered|visible|isRendered|isVisible|shown)$/i)
    .filter((entry) => entry.value === false)
    .map((entry) => entry.pathParts.join("."));
}

function indexedEntityIconography(world) {
  const entries = entityIconographyEntries(world);
  const byType = new Map();
  for (const entry of entries) {
    const type = entityIconTypeForEntry(entry);
    if (!type) continue;
    if (!byType.has(type)) byType.set(type, []);
    byType.get(type).push(entry);
  }
  return { entries, byType };
}

function entityEntryDebug(entry) {
  return `${entry.fieldPath}${entry.mapKey ? `.${entry.mapKey}` : ""}: ${objectText(entry.node)}`;
}

function steamIconHasLogoOrFallback(node) {
  const text = `${iconEvidenceText(node)} ${labelEvidenceText(node)} ${objectText(node)}`;
  return /steam.*(logo|icon|png|svg)|(?:logo|icon|png|svg).*steam|fallback|🛠|🔧|⚙/i.test(text);
}

function assertAllEntityIconography(world) {
  const { entries, byType } = indexedEntityIconography(world);
  const missingTypes = REQUIRED_ENTITY_ICON_TYPES.filter((type) => !byType.has(type));
  if (missingTypes.length) {
    contractGap(
      world,
      `entityIconography/renderedEntityIcons missing entity keys: ${missingTypes.join(", ")}`,
      `ENTITY ICON ENTRIES:\n${entries.map(entityEntryDebug).join("\n") || "<none>"}`
    );
  }
  const missingIconKeys = [];
  const missingLabelKeys = [];
  const falseRenderedKeys = [];
  for (const type of REQUIRED_ENTITY_ICON_TYPES) {
    const candidates = byType.get(type) || [];
    if (!candidates.some((entry) => hasIconEvidence(entry.node))) missingIconKeys.push(type);
    if (!candidates.some((entry) => hasTextLabelEvidence(entry.node))) missingLabelKeys.push(type);
    const falseFlags = candidates.flatMap((entry) => falseRenderedIconFlags(entry.node));
    if (falseFlags.length) falseRenderedKeys.push(`${type} (${falseFlags.join(", ")})`);
  }
  if (missingIconKeys.length || missingLabelKeys.length || falseRenderedKeys.length) {
    contractGap(
      world,
      [
        missingIconKeys.length ? `missing icon evidence for: ${missingIconKeys.join(", ")}` : "",
        missingLabelKeys.length ? `missing paired text labels for: ${missingLabelKeys.join(", ")}` : "",
        falseRenderedKeys.length ? `rendered/visible flags are false for: ${falseRenderedKeys.join("; ")}` : "",
      ].filter(Boolean).join("; "),
      `ENTITY ICON ENTRIES:\n${entries.map(entityEntryDebug).join("\n") || "<none>"}`
    );
  }
}

function assertRenderedEntityIconsPairTextLabels(world) {
  const renderedFields = valuesForKey(world.profileFirstGuiReport || {}, /^renderedEntityIcons$/i)
    .filter((entry) => entry.value && typeof entry.value === "object");
  if (!renderedFields.length) {
    contractGap(world, "missing renderedEntityIcons field in profile-first GUI smoke report");
  }
  const entries = [];
  for (const field of renderedFields) {
    collectIconographyEntries(field.value).forEach((entry) => {
      entries.push({ ...entry, fieldPath: field.pathParts.join(".") });
    });
  }
  if (!entries.length) {
    contractGap(world, "renderedEntityIcons is present but has no icon entries");
  }
  const missingLabels = entries.filter((entry) => hasIconEvidence(entry.node) && !hasTextLabelEvidence(entry.node));
  if (missingLabels.length) {
    contractGap(
      world,
      "renderedEntityIcons has icon/glyph/logo entries without paired text labels",
      `ICON ENTRIES:\n${missingLabels.map(entityEntryDebug).join("\n")}`
    );
  }
}

function assertSteamEntityIconography(world) {
  const { byType } = indexedEntityIconography(world);
  const missingSteamFallback = ["steam-workshop", "workshop"]
    .filter((type) => !(byType.get(type) || []).some((entry) => steamIconHasLogoOrFallback(entry.node)));
  const platformEntries = byType.get("platform-steam") || [];
  const platformHasEntryEvidence = platformEntries.some((entry) => steamIconHasLogoOrFallback(entry.node));
  const platformLogoEvidence = platformSteamLogoEvidence(world);
  const failures = [];
  if (missingSteamFallback.length) {
    failures.push(`missing Steam logo/fallback evidence for: ${missingSteamFallback.join(", ")}`);
  }
  if (!platformHasEntryEvidence) {
    failures.push("platform-steam icon entry lacks Steam logo/fallback evidence");
  }
  if (!platformLogoEvidence.ok) {
    failures.push("platform-steam must keep steamLogoPath and steamLogoRendered/platform Steam logo evidence");
  }
  if (failures.length) {
    contractGap(
      world,
      failures.join("; "),
      `steamLogoPath candidates:\n${platformLogoEvidence.paths.map((entry) => `${entry.pathParts.join(".")}=${entry.value}`).join("\n") || "<none>"}\n` +
      `steamLogoRendered candidates:\n${platformLogoEvidence.flags.map((entry) => entry.pathParts.join(".")).join("\n") || "<none>"}`
    );
  }
}

function pathLooksLikeProfile(pathText) {
  return /profile/i.test(pathText) && /(path|dir|root|folder|location)$/i.test(pathText.split(".").pop() || "");
}

function profilePathCandidates(value) {
  const candidates = [];
  walk(value, (node, pathParts) => {
    if (typeof node !== "string" || !node.trim()) return;
    const pathText = pathParts.join(".");
    if (!pathLooksLikeProfile(pathText)) return;
    if (!/[\\/]/.test(node)) return;
    candidates.push({ value: node, pathParts });
  });
  return candidates;
}

function pathHasDotTmp(candidate) {
  return candidate.split(/[\\/]+/).includes(".tmp");
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

function booleanFlag(value, keyPattern, expected) {
  return valuesForKey(value, keyPattern).some((entry) => entry.value === expected);
}

function trueBooleanFlags(value, keyPattern) {
  return valuesForKey(value, keyPattern).filter((entry) => entry.value === true).map((entry) => entry.pathParts.join("."));
}

function selectionRedrawInstrumentationEntries(report) {
  const entries = [];
  const directEntries = [];
  for (const key of ["selectionRedrawScope", "selectionRedraw", "lastSelectionRedraw"]) {
    if (report[key] && typeof report[key] === "object") directEntries.push({ node: report[key], pathParts: [key] });
  }
  if (report.smokeSelectedMod && typeof report.smokeSelectedMod === "object") {
    for (const key of ["selectionRedrawScope", "selectionRedraw", "lastSelectionRedraw"]) {
      if (report.smokeSelectedMod[key] && typeof report.smokeSelectedMod[key] === "object") {
        directEntries.push({ node: report.smokeSelectedMod[key], pathParts: ["smokeSelectedMod", key] });
      }
    }
  }
  if (directEntries.length) return directEntries;
  const instrumentationKeyPattern = /^(redrawEvents|redrawTrace|redrawSummary|redrawCounters|redrawCounts|renderEvents|renderTrace|renderCounters|renderCounts|renderInstrumentation|renderInstrumentationSummary|smokeRedrawEvents|smokeRedrawTrace|smokeRedrawSummary|smokeRenderCounters|selectionRedraw|selectionRedrawScope|selectionRedrawEvents|selectionRedrawMetadata|lastSelectionRedraw|scopedRedraws|refreshScope|updateScope|paintTrace)$/i;
  const instrumentationPathPattern = /(^|\.)(redraw|renderTrace|renderCounters|renderCounts|renderEvents|renderInstrumentation|smokeRedraw|selectionRedraw|scopedRedraw|refreshScope|updateScope|paintTrace)(\.|$)/i;
  const roots = [];
  for (const entry of valuesForKey(report, instrumentationKeyPattern)) {
    roots.push({ node: entry.value, pathParts: entry.pathParts });
  }
  for (const entry of allObjects(report)) {
    const pathText = entry.pathParts.join(".");
    if (instrumentationPathPattern.test(pathText)) roots.push(entry);
  }
  const seen = new Set();
  for (const root of roots) {
    const pathText = root.pathParts.join(".");
    const rootText = objectText(root.node);
    const rootLooksSelectionScoped = /select|selection|selectedDetectedMod|selected mod|Mods row/i.test(`${pathText} ${rootText}`);
    const pushEntry = (node, pathParts) => {
      const entryPath = pathParts.join(".");
      const text = objectText(node);
      const joined = `${entryPath} ${text}`;
      if (!/(redraw|render|refresh|update|paint|scope|region|destroy)/i.test(joined)) return;
      if (!/(select|selection|selectedDetectedMod|selected mod|Mods row)/i.test(joined) && !rootLooksSelectionScoped) return;
      const key = `${entryPath}\u0000${text}`;
      if (seen.has(key)) return;
      seen.add(key);
      entries.push({ node, pathParts });
    };
    pushEntry(root.node, root.pathParts);
    if (Array.isArray(root.node)) {
      root.node.forEach((item, index) => {
        if (item && typeof item === "object" && item.reason && String(item.reason) !== "selection") return;
        pushEntry(item, [...root.pathParts, String(index)]);
      });
    } else if (root.node && typeof root.node === "object") {
      for (const entry of allObjects(root.node)) {
        pushEntry(entry.node, [...root.pathParts, ...entry.pathParts]);
      }
    }
  }
  return entries;
}

function numericValuesForKey(value, keyPattern) {
  return valuesForKey(value, keyPattern)
    .filter((entry) => typeof entry.value === "number")
    .map((entry) => ({ ...entry, value: Number(entry.value) }));
}

function textValuesForKey(value, keyPattern) {
  const matches = [];
  for (const entry of valuesForKey(value, keyPattern)) {
    if (typeof entry.value === "string") {
      matches.push({ ...entry, value: String(entry.value) });
    } else if (Array.isArray(entry.value)) {
      entry.value.forEach((item, index) => {
        if (typeof item === "string") matches.push({ pathParts: [...entry.pathParts, String(index)], value: item });
      });
    }
  }
  return matches;
}

function assertSelectionUsesScopedRedraw(world) {
  const report = world.profileFirstGuiReport || {};
  const entries = selectionRedrawInstrumentationEntries(report);
  if (!entries.length) {
    contractGap(
      world,
      "missing selection redraw instrumentation in the GUI smoke report",
      "Expected smoke metadata such as selectionRedraw, redrawTrace/redrawEvents, renderCounters, or scopedRedraws that names the selection redraw scope."
    );
  }

  const badFlagPattern = /^(fullDashboard|fullDashboardCountChanged|dashboardFullRedraw|fullDashboardRedraw|dashboardFullRender|fullDashboardRender|rightSideFullRedraw|fullRightSideRedraw|rightSideFullRender|fullRightSideRender|rightSideDestroyed|rightSideRebuilt|compactStatusCardsDestroyed|compactStatusCardsRebuilt|compactStatusGridDestroyed|compactStatusGridRebuilt|statusGridDestroyed|statusGridRebuilt)$/i;
  const badRegionDeltaPattern = /(regionRenderCountDelta|renderCountDelta|redrawCountDelta|countDelta|delta).*(fullDashboard|rightSide|compactStatusCards|compactStatusGrid|statusGrid)/i;
  const badScopeTextPattern = /\b(fullDashboard|full[-\s_]*dashboard|dashboard[-\s_]*full|rightSide|right[-\s_]*side[-\s_]*full|full[-\s_]*right[-\s_]*side|compactStatusCards|compact[-\s_]*status[-\s_]*cards|compactStatusGrid|compact[-\s_]*status[-\s_]*grid|statusGrid|status[-\s_]*grid)\b/i;
  const badEntries = [];

  for (const entry of entries) {
    const trueBadFlags = valuesForKey(entry.node, badFlagPattern)
      .filter((flag) => flag.value === true)
      .map((flag) => `${[...entry.pathParts, ...flag.pathParts].join(".")}=${flag.value}`);
    const positiveBadCounts = numericValuesForKey(entry.node, /.*/)
      .filter((flag) => flag.value > 0 && badRegionDeltaPattern.test([...entry.pathParts, ...flag.pathParts].join(".")))
      .map((flag) => `${[...entry.pathParts, ...flag.pathParts].join(".")}=${flag.value}`);
    const badText = textValuesForKey(entry.node, /(scope|scopes|region|regions|redrawn|rendered|destroyed|operation|kind|type|target|targetRegion|affectedRegions|updatedRegions|regionsRedrawn)$/i)
      .filter((field) => badScopeTextPattern.test(field.value))
      .map((field) => `${[...entry.pathParts, ...field.pathParts].join(".")}=${field.value}`);
    if (trueBadFlags.length || positiveBadCounts.length || badText.length) {
      badEntries.push(...trueBadFlags, ...positiveBadCounts, ...badText, `${entry.pathParts.join(".")} ${objectText(entry.node)}`);
    }
  }

  if (badEntries.length) {
    contractGap(
      world,
      "selection redraw instrumentation reports a full dashboard or right-side full redraw",
      `BAD REDRAW EVIDENCE:\n${badEntries.join("\n")}`
    );
  }

  const scopedEntries = entries.filter((entry) => {
    const text = `${entry.pathParts.join(".")} ${objectText(entry.node)}`;
    const scopedFlag = valuesForKey(entry.node, /^(scoped|scopedRedraw|selectionScoped|selectionOnly|selectionOnlyRedraw|partial|partialRedraw)$/i).some((flag) => flag.value === true);
    const fullDashboardFalse = valuesForKey(entry.node, /(full.*dashboard|dashboard.*full|right.*side.*full|full.*right.*side)/i).some((flag) => flag.value === false || flag.value === 0);
    const scopedText = /(scoped|selection[-\s_]*only|partial|selected[-\s_]*mod[-\s_]*detail|selected[-\s_]*detail|mods[-\s_]*list|activity[-\s_]*log|recent[-\s_]*activity)/i.test(text);
    return scopedFlag || fullDashboardFalse || scopedText;
  });
  if (!scopedEntries.length) {
    contractGap(
      world,
      "selection redraw instrumentation does not prove scoped redraw",
      `SELECTION REDRAW ENTRIES:\n${entries.map((entry) => `${entry.pathParts.join(".")} ${objectText(entry.node)}`).join("\n---\n")}`
    );
  }
}

function maybeLoadReport(world) {
  if (!world.profileFirstGuiReportPath || !fs.existsSync(world.profileFirstGuiReportPath)) return null;
  const raw = fs.readFileSync(world.profileFirstGuiReportPath, "utf-8");
  world.profileFirstGuiReportRaw = raw;
  try {
    world.profileFirstGuiReport = JSON.parse(raw);
  } catch (error) {
    world.profileFirstGuiReport = { rawText: raw };
  }
  return world.profileFirstGuiReport;
}

function profileFirstSmokeEnv(world) {
  return {
    ...process.env,
    PATH: `/usr/bin:${process.env.PATH || ""}`,
    XDG_DATA_HOME: world.profileFirstGuiDataHome,
    BML_GUI_PROFILE_FIRST_SMOKE: "1",
    BML_GUI_WORKSHOP_FIXTURE_HOME: world.profileFirstGuiWorkshopHome,
    BML_GUI_WORKSHOP_FIXTURE_ROOT: world.profileFirstGuiWorkshopFixturePath,
  };
}

function runProfileFirstGuiSmoke(world, reportName, smokeClicks, selectedMod) {
  world.profileFirstGuiReportPath = path.join(world.profileFirstGuiDir, reportName);
  const args = [
    "--auto-close-ms",
    "750",
    "--smoke-clicks",
    smokeClicks.join(","),
    "--smoke-select-mod",
    selectedMod,
    "--smoke-report",
    world.profileFirstGuiReportPath,
  ];
  const result = spawnSync(BML_BIN, ["gui", ...args], {
    cwd: REPO_ROOT,
    encoding: "utf-8",
    timeout: 60000,
    env: profileFirstSmokeEnv(world),
    stdio: ["ignore", "pipe", "pipe"],
  });
  world.profileFirstGuiCommand = {
    args,
    status: result.status === null ? 1 : result.status,
    signal: result.signal,
    error: result.error,
    stdout: result.stdout || "",
    stderr: result.stderr || "",
  };
  maybeLoadReport(world);
  if (world.profileFirstGuiCommand.status !== 0) {
    contractGap(world, "bin/barony-mod-loader gui smoke automation did not exit successfully");
  }
  if (!world.profileFirstGuiReport) {
    contractGap(world, "bin/barony-mod-loader gui did not write a readable smoke report");
  }
  return world.profileFirstGuiReport;
}

Given("a completed profile-first GUI smoke report", function () {
  this.profileFirstGuiDir = fs.mkdtempSync(path.join(REPO_ROOT, ".cucumber-profile-first-"));
  this.profileFirstGuiDataHome = path.join(this.profileFirstGuiDir, "xdg-data");
  this.profileFirstGuiWorkshopHome = path.join(this.profileFirstGuiDir, "steam-home");
  fs.mkdirSync(this.profileFirstGuiDataHome, { recursive: true });
  fs.mkdirSync(this.profileFirstGuiWorkshopHome, { recursive: true });
  this.profileFirstGuiWorkshopFixturePath = createWorkshopSubscriptionFixture(this.profileFirstGuiWorkshopHome);
  const smokeEnv = {
    ...process.env,
    PATH: `/usr/bin:${process.env.PATH || ""}`,
    XDG_DATA_HOME: this.profileFirstGuiDataHome,
    BML_GUI_PROFILE_FIRST_SMOKE: "1",
    BML_GUI_WORKSHOP_FIXTURE_HOME: this.profileFirstGuiWorkshopHome,
    BML_GUI_WORKSHOP_FIXTURE_ROOT: this.profileFirstGuiWorkshopFixturePath,
  };
  const bootstrapReportPath = path.join(this.profileFirstGuiDir, "profile-first-bootstrap-smoke-report.json");
  const bootstrap = spawnSync(
    BML_BIN,
    ["gui", "--auto-close-ms", "250", "--smoke-clicks", "create-select-profile", "--smoke-report", bootstrapReportPath],
    {
      cwd: REPO_ROOT,
      encoding: "utf-8",
      timeout: 60000,
      env: smokeEnv,
      stdio: ["ignore", "pipe", "pipe"],
    }
  );
  if ((bootstrap.status === null ? 1 : bootstrap.status) !== 0) {
    throw new Error(`profile-first GUI bootstrap failed: ${bootstrap.stderr || bootstrap.stdout || bootstrap.error || "unknown error"}`);
  }
  this.profileFirstGuiProfileFixturePath = createActiveRuneboundProfileFixture(this.profileFirstGuiDataHome);
  this.profileFirstGuiReportPath = path.join(this.profileFirstGuiDir, "profile-first-gui-smoke-report.json");
  const smokeClicks = [
    "scan-packages",
    "enable-package",
    "refresh-readiness",
    "open-diagnostics",
    "workshop-preview",
  ].join(",");
  const args = ["--auto-close-ms", "750", "--smoke-clicks", smokeClicks, "--smoke-select-mod", SMOKE_SELECT_MOD, "--smoke-report", this.profileFirstGuiReportPath];
  const result = spawnSync(BML_BIN, ["gui", ...args], {
    cwd: REPO_ROOT,
    encoding: "utf-8",
    timeout: 60000,
    env: smokeEnv,
    stdio: ["ignore", "pipe", "pipe"],
  });
  this.profileFirstGuiCommand = {
    args,
    status: result.status === null ? 1 : result.status,
    signal: result.signal,
    error: result.error,
    stdout: result.stdout || "",
    stderr: result.stderr || "",
  };
  maybeLoadReport(this);
  if (this.profileFirstGuiCommand.status !== 0) {
    contractGap(this, "bin/barony-mod-loader gui smoke automation did not exit successfully");
  }
  if (!this.profileFirstGuiReport) {
    contractGap(this, "bin/barony-mod-loader gui did not write a readable smoke report");
  }
});

After(function () {
  if (this.profileFirstGuiDir) {
    try { fs.rmSync(this.profileFirstGuiDir, { recursive: true, force: true }); } catch (_) {}
  }
});

[
  "the user opens the profile-first mod manager for the first time",
  "the user reviews the Environment concept",
  "the user reviews the Environment card",
  "the user creates or selects a profile from the Profiles concept",
  "the user scans Mods and chooses Runebound: Elixirs",
  "the user scans the Mods list",
  "the user scans the Detected Mods sidebar",
  "the smoke selects Runebound: Elixirs from the Mods list",
  "the user previews Workshop for Runebound: Elixirs",
  "the user reviews the concept cards for Runebound evidence",
  "the user reviews the profile-first entity iconography",
  "the user reviews the Mods list for Runebound evidence",
  "the user reviews the Detected Mods sidebar for Runebound evidence",
].forEach((stepText) => {
  When(stepText, function () {
    if (!this.profileFirstGuiReport) maybeLoadReport(this);
  });
});

When("the smoke selects Stash from the Mods list and clicks Enable selected mod", function () {
  runProfileFirstGuiSmoke(this, "profile-first-stash-enable-smoke-report.json", ["scan-packages", "enable-package"], STASH_ID);
});

When("the smoke selects Stash from the Mods list", function () {
  runProfileFirstGuiSmoke(this, "profile-first-stash-selection-smoke-report.json", ["scan-packages"], STASH_ID);
});

When("the smoke selects the Workshop subscription from the Mods list", function () {
  runProfileFirstGuiSmoke(this, "profile-first-workshop-selection-smoke-report.json", ["scan-packages"], WORKSHOP_FIXTURE_ID);
});

Then("the left side of the main body is a Mods list", function () {
  if (!reportShowsOpened(this.profileFirstGuiReport)) {
    contractGap(this, "smoke report does not prove a Tk root/window opened");
  }
  requireModsList(this);
  detectedModSections(this);
});

Then("the Mods list is titled Mods", function () {
  const title = modsListTitle(this);
  if (!title) {
    contractGap(this, "missing rendered Mods list title (expected modsListTitle or equivalent)");
  }
  if (!/^Mods$/i.test(title)) {
    contractGap(this, "left list title is not exactly Mods", `TITLE: ${title}`);
  }
  if (/Detected\s+Mods/i.test(title)) {
    contractGap(this, "left list still uses old Detected Mods title", `TITLE: ${title}`);
  }
});

Then("the Mods list header exposes scan or filter list controls", function () {
  requireModsListHeaderControls(this);
});

Then("the Mods list header exposes mod actions", function () {
  requireModsListHeaderActions(this);
});

Then("the Mods list is not a concept-card column", function () {
  const list = requireModsList(this);
  const listText = objectText(list.node);
  if (typeof (this.profileFirstGuiReport && this.profileFirstGuiReport.layout) === "string" && /concept cards?/i.test(this.profileFirstGuiReport.layout)) {
    contractGap(this, "smoke report still describes the main layout as concept cards", `layout=${this.profileFirstGuiReport.layout}`);
  }
  const renderedLabels = renderedProvenanceSectionLabels(this);
  const conceptLabelsInList = CONCEPT_LABELS.filter((label) => new RegExp(`\\b${label}\\b`, "i").test(listText));
  if (conceptLabelsInList.length === CONCEPT_LABELS.length && !/local|repo|enabled|profile|workshop/i.test(renderedLabels.join(" "))) {
    contractGap(
      this,
      "Mods list appears to contain the old Environment/Profiles/Mods/Workshop concept-card column instead of provenance sections",
      `MODS LIST EVIDENCE:\n${listText || "<none>"}\nPROVENANCE LABELS:\n${renderedLabels.join(", ") || "<none>"}`
    );
  }
});

Then("the left side of the main body is a Detected Mods sidebar", function () {
  if (!reportShowsOpened(this.profileFirstGuiReport)) {
    contractGap(this, "smoke report does not prove a Tk root/window opened");
  }
  requireDetectedModsSidebar(this);
  detectedModSections(this);
});

Then("the Detected Mods sidebar is not a concept-card column", function () {
  const sidebar = requireDetectedModsSidebar(this);
  const sidebarText = objectText(sidebar.node);
  if (typeof (this.profileFirstGuiReport && this.profileFirstGuiReport.layout) === "string" && /concept cards?/i.test(this.profileFirstGuiReport.layout)) {
    contractGap(this, "smoke report still describes the main layout as concept cards", `layout=${this.profileFirstGuiReport.layout}`);
  }
  const renderedLabels = renderedProvenanceSectionLabels(this);
  const conceptLabelsInSidebar = CONCEPT_LABELS.filter((label) => new RegExp(`\\b${label}\\b`, "i").test(sidebarText));
  if (conceptLabelsInSidebar.length === CONCEPT_LABELS.length && !/local|repo|enabled|profile|workshop/i.test(renderedLabels.join(" "))) {
    contractGap(
      this,
      "Detected Mods sidebar appears to contain the old Environment/Profiles/Mods/Workshop concept-card column instead of provenance sections",
      `SIDEBAR EVIDENCE:\n${sidebarText || "<none>"}\nPROVENANCE LABELS:\n${renderedLabels.join(", ") || "<none>"}`
    );
  }
});

Then("the right side keeps Environment, Profiles, Mods, Workshop, and Recent Activity", function () {
  requireRightSide(this);
});

Then("the right side keeps Environment, Profiles, Workshop, and Recent Activity", function () {
  requireRightSide(this);
});

Then("the right side shows selected mod details with contextual actions", function () {
  requireSelectedModDetailPanel(this);
});

Then("the right side keeps compact Environment, Profile, Workshop status cards and Recent Activity", function () {
  requireRightSide(this);
  requireCompactRightStatusCards(this);
});

Then("the compact Workshop status card shows dry-run no-publish state without primary blocker dumps", function () {
  assertCompactWorkshopPrimaryText(this);
});

Then("visible Recent Activity entries are concise, target-named, and keep verbose data in report details", function () {
  assertVisibleActivityLogConcise(this);
});

Then("the right side does not keep a separate Mods card", function () {
  requireNoSeparateRightSideModsCard(this);
});


Then("the smoke report exposes detectedModSections", function () {
  detectedModSections(this);
});

Then("the smoke report exposes renderedDetectedModRows", function () {
  renderedDetectedModRows(this);
});

Then("enabled Mods rows render a green check prefix in the aligned prefix column", function () {
  const rows = renderedDetectedModRows(this);
  assertRowsExposeAlignedPrefix(this, rows);
  const enabledRows = rows.filter(isEnabledModRow);
  if (!enabledRows.length) {
    contractGap(this, "renderedDetectedModRows has no enabled/active row to validate green check prefix");
  }
  const matchingRows = enabledRows.filter(hasGreenCheckPrefix);
  if (!matchingRows.length) {
    contractGap(
      this,
      "enabled/active renderedDetectedModRows do not expose a green check prefix",
      `ENABLED ROWS:\n${enabledRows.map(rowText).join("\n---\n") || "<none>"}`
    );
  }
});

Then("disabled Mods rows reserve the aligned prefix column and include disabled, detected, or subscribed state text", function () {
  const rows = renderedDetectedModRows(this);
  assertRowsExposeAlignedPrefix(this, rows);
  const disabledRows = rows.filter(isDisabledModRow);
  if (!disabledRows.length) {
    contractGap(this, "renderedDetectedModRows has no disabled/inactive/detected/subscribed row to validate blank aligned prefix");
  }
  const badRows = disabledRows.filter((row) => !rowReservesPrefix(row) || !/\b(disabled|inactive|detected|subscribed|subscription)\b/i.test(rowStatePlainText(row) || objectText(row)));
  if (badRows.length) {
    contractGap(
      this,
      "disabled/inactive renderedDetectedModRows do not reserve the aligned prefix column with plain text disabled/detected/subscribed state",
      `BAD ROWS:\n${badRows.map(rowText).join("\n---\n")}`
    );
  }
});

Then("the smoke report exposes selectable Mods row metadata", function () {
  const rows = renderedDetectedModRows(this);
  const row = runeboundRow(rows);
  if (!row) {
    contractGap(
      this,
      "renderedDetectedModRows does not include Runebound: Elixirs selectable row metadata",
      `ROWS:\n${rows.map(rowText).join("\n---\n") || "<none>"}`
    );
  }
  if (!rowSelectable(row)) {
    contractGap(
      this,
      "Runebound: Elixirs renderedDetectedModRows entry is missing selectable metadata",
      `Expected selectable=true, selectionTarget/clickTarget, rowId, selectedDetectedModId, or equivalent.\nROW:\n${rowText(row)}`
    );
  }
});

Then("Mods rows expose keyboard focus and navigation metadata", function () {
  assertModsRowsKeyboardAndFocusMetadata(this);
});

Then("selected Mods row exposes visible focus or selection affordance", function () {
  assertSelectedRowFocusAffordance(this);
});

Then("selectedDetectedMod changes to the requested smoke-selected row", function () {
  const report = this.profileFirstGuiReport || {};
  const requested = smokeSelectedMod(report);
  if (requested === null) {
    contractGap(this, "missing required smoke report field smokeSelectedMod after --smoke-select-mod");
  }
  if (!new RegExp(`${RUNEBOUND_ID}|${RUNEBOUND_NAME}`, "i").test(objectText(requested))) {
    contractGap(this, "smokeSelectedMod does not match requested Runebound: Elixirs selection", `smokeSelectedMod=${objectText(requested) || String(requested)}`);
  }
  if (!report.selectedDetectedMod || typeof report.selectedDetectedMod !== "object") {
    contractGap(this, "missing required smoke report field selectedDetectedMod after deterministic Mods list selection");
  }
  const selectedId = selectedDetectedModId(report);
  if (!selectedId) {
    contractGap(this, "missing required smoke report field selectedDetectedModId after deterministic Mods list selection");
  }
  if (!new RegExp(`${RUNEBOUND_ID}|${RUNEBOUND_NAME}`, "i").test(`${selectedId} ${objectText(report.selectedDetectedMod)}`)) {
    contractGap(
      this,
      "selectedDetectedModId/selectedDetectedMod did not change to the requested smoke-selected row",
      `selectedDetectedModId=${selectedId}\nselectedDetectedMod=${objectText(report.selectedDetectedMod) || "<none>"}`
    );
  }
});

Then("the selected local BML package details match Runebound: Elixirs", function () {
  const selectedPackage = selectedPackageEvidence(this.profileFirstGuiReport || {});
  if (!selectedPackage) {
    contractGap(this, "missing selectedPackage details for local BML Mods list selection");
  }
  const text = objectText(selectedPackage);
  if (!new RegExp(`${RUNEBOUND_ID}|${RUNEBOUND_NAME}`, "i").test(text)) {
    contractGap(
      this,
      "selectedPackage details do not match selected local Runebound: Elixirs mod",
      `SELECTED PACKAGE:\n${text || "<none>"}`
    );
  }
});

Then("the canonical selectedMod matches enabled local Runebound: Elixirs", function () {
  const selectedMod = requireSelectedModPackage(this, RUNEBOUND_ID, RUNEBOUND_NAME);
  requireSelectedModProfileState(this, selectedMod, true, RUNEBOUND_NAME);
});

Then("selectedMod exposes Disable selected mod as the primary action and disables redundant Enable selected mod", function () {
  const selectedMod = selectedModObject(this);
  requireSelectedModActionChoice(this, selectedMod, "disable", "enable");
});

Then("the selected mod detail panel matches selectedMod and exposes detail actions", function () {
  requireSelectedModDetailPanel(this);
});

Then("selecting the mod uses scoped redraw metadata instead of a full dashboard redraw", function () {
  assertSelectionUsesScopedRedraw(this);
});


Then("the smoke report exposes selectable Mods row metadata for Stash", function () {
  const rows = renderedDetectedModRows(this);
  const row = stashRow(rows);
  if (!row) {
    contractGap(
      this,
      "renderedDetectedModRows does not include Stash selectable row metadata",
      `ROWS:\n${rows.map(rowText).join("\n---\n") || "<none>"}`
    );
  }
  if (!rowSelectable(row)) {
    contractGap(
      this,
      "Stash renderedDetectedModRows entry is missing selectable metadata",
      `Expected selectable=true, selectionTarget/clickTarget, rowId, selectedDetectedModId, or equivalent.\nROW:\n${rowText(row)}`
    );
  }
});

Then("the smoke report exposes selectable Mods row metadata for the Workshop subscription", function () {
  const rows = renderedDetectedModRows(this);
  const row = workshopSubscriptionRow(rows);
  if (!row) {
    contractGap(
      this,
      "renderedDetectedModRows does not include the Workshop subscription selectable row metadata",
      `ROWS:\n${rows.map(rowText).join("\n---\n") || "<none>"}`
    );
  }
  if (!rowSelectable(row)) {
    contractGap(
      this,
      "Workshop subscription renderedDetectedModRows entry is missing selectable metadata",
      `Expected selectable=true, selectionTarget/clickTarget, rowId, selectedDetectedModId, or equivalent.\nROW:\n${rowText(row)}`
    );
  }
});


Then("selectedDetectedMod changes to the requested Stash row", function () {
  const report = this.profileFirstGuiReport || {};
  const requested = smokeSelectedMod(report);
  if (requested === null) {
    contractGap(this, "missing required smoke report field smokeSelectedMod after --smoke-select-mod jml.stash");
  }
  if (!objectMatchesPackage(requested, STASH_ID, STASH_NAME)) {
    contractGap(this, "smokeSelectedMod does not match requested Stash selection", `smokeSelectedMod=${objectText(requested) || String(requested)}`);
  }
  if (!report.selectedDetectedMod || typeof report.selectedDetectedMod !== "object") {
    contractGap(this, "missing required smoke report field selectedDetectedMod after deterministic Stash Mods list selection");
  }
  const selectedId = selectedDetectedModId(report);
  if (!selectedId) {
    contractGap(this, "missing required smoke report field selectedDetectedModId after deterministic Stash Mods list selection");
  }
  if (!objectMatchesPackage(`${selectedId} ${objectText(report.selectedDetectedMod)}`, STASH_ID, STASH_NAME)) {
    contractGap(
      this,
      "selectedDetectedModId/selectedDetectedMod did not change to the requested Stash row",
      `selectedDetectedModId=${selectedId}\nselectedDetectedMod=${objectText(report.selectedDetectedMod) || "<none>"}`
    );
  }
});

Then("the canonical selectedMod matches disabled local Stash", function () {
  const selectedMod = requireSelectedModPackage(this, STASH_ID, STASH_NAME);
  requireSelectedModProfileState(this, selectedMod, false, STASH_NAME);
});

Then("selectedMod exposes Enable selected mod as the primary action and disables redundant Disable selected mod", function () {
  const selectedMod = selectedModObject(this);
  requireSelectedModActionChoice(this, selectedMod, "enable", "disable");
});

Then("the canonical selectedMod identifies Stash as the Enable selected mod target", function () {
  const selectedMod = requireSelectedModPackage(this, STASH_ID, STASH_NAME);
  const enableState = selectedModActionState(selectedMod, "enable");
  const disableState = selectedModActionState(selectedMod, "disable");
  if (enableState.available === null || disableState.available === null) {
    contractGap(
      this,
      "canonical selectedMod for Stash is missing Enable/Disable action eligibility",
      `Enable: ${JSON.stringify(enableState)}\nDisable: ${JSON.stringify(disableState)}\nselectedMod:\n${objectText(selectedMod)}`
    );
  }
});

Then("the canonical selectedMod matches the Workshop subscription", function () {
  requireSelectedModWorkshopSubscription(this);
});

Then("selectedMod marks package enable and disable actions unavailable for the Workshop subscription", function () {
  const selectedMod = requireSelectedModWorkshopSubscription(this);
  const enableState = selectedModActionState(selectedMod, "enable");
  const disableState = selectedModActionState(selectedMod, "disable");
  if (enableState.available !== false || disableState.available !== false) {
    contractGap(
      this,
      "canonical selectedMod exposes package enable/disable for a non-BML Workshop subscription",
      `Expected enable=false and disable=false.\nEnable: ${JSON.stringify(enableState)}\nDisable: ${JSON.stringify(disableState)}\nselectedMod:\n${objectText(selectedMod)}`
    );
  }
  const explanation = objectText(selectedMod);
  if (!/workshop|subscription/i.test(explanation) || !/(non[-\s]?BML|not\s+(a\s+)?(BML\s+)?package|missing\s+bml-package|unavailable|unsupported)/i.test(explanation)) {
    contractGap(
      this,
      "canonical selectedMod does not explain why package actions are unavailable for the Workshop subscription",
      `selectedMod:\n${explanation || "<none>"}`
    );
  }
});


Then("Enable selected mod targets Stash and does not reuse Runebound state", function () {
  const report = this.profileFirstGuiReport || {};
  const rows = renderedDetectedModRows(this);
  const row = stashRow(rows);
  const entries = clickedActionEntries(report, "enable-package");
  if (!entries.length) {
    contractGap(this, "clickedActions is missing Enable selected mod / enable-package evidence for the Stash smoke");
  }
  const enableEntry = entries[entries.length - 1];
  if (enableEntry && enableEntry.invoked === false) {
    contractGap(this, "Enable selected mod was not actually invoked for the Stash smoke", `ACTION:\n${objectText(enableEntry) || "<none>"}`);
  }
  const logEntries = actionLogEntries(report, "enable-package");
  const enableEvidence = [enableEntry, enableEntry && enableEntry.result, ...logEntries].filter(Boolean);
  const beforeHasStash = activeModsContain(enableEntry && enableEntry.beforeActiveMods, STASH_ID, STASH_NAME);
  const afterHasStash =
    activeModsContain(enableEntry && enableEntry.afterActiveMods, STASH_ID, STASH_NAME) ||
    activeModsContain(report.activeMods, STASH_ID, STASH_NAME);
  const evidenceText = enableEvidence.map(objectText).join("\n---\n");
  const targetStash = enableEvidence.some((entry) => entryTargetsPackage(entry, STASH_ID, STASH_NAME));
  const runeboundAlreadyEnabled =
    /runebound[\s\S]{0,160}already[_ -]?enabled|already[_ -]?enabled[\s\S]{0,160}runebound|jml\.runebound-elixirs[\s\S]{0,160}already[_ -]?enabled|already[_ -]?enabled[\s\S]{0,160}jml\.runebound-elixirs/i.test(evidenceText);
  const statusText = valuesForKey(enableEntry, /^(status|result|outcome)$/i).map((entry) => String(entry.value)).join(" ");

  if (beforeHasStash) {
    contractGap(this, "Stash was already active before Enable selected mod; this smoke must start with Stash disabled", `ACTION:\n${objectText(enableEntry)}`);
  }
  if (!targetStash) {
    contractGap(
      this,
      "Enable selected mod did not report Stash as the target package",
      `Expected packageId/selectedPackageId/selectedModId=${STASH_ID} or Stash text.\nENABLE EVIDENCE:\n${evidenceText || "<none>"}`
    );
  }
  if (runeboundAlreadyEnabled || (/already[_ -]?enabled/i.test(statusText) && !afterHasStash)) {
    contractGap(
      this,
      "Stash selected + Enable selected mod reused Runebound's already-enabled state instead of targeting Stash",
      `ENABLE EVIDENCE:\n${evidenceText || "<none>"}`
    );
  }
  if (afterHasStash) {
    if (!row || !isEnabledModRow(row)) {
      contractGap(
        this,
        "activeMods includes Stash after Enable, but renderedDetectedModRows does not show Stash as enabled",
        `STASH ROW:\n${row ? rowText(row) : "<none>"}\nACTION:\n${objectText(enableEntry)}`
      );
    }
    return;
  }
  if (!/blocked|invalid|warning|failed|error|disabled|unavailable/i.test(statusText) || !targetStash) {
    contractGap(
      this,
      "Enable selected mod neither enabled Stash nor returned a Stash-specific blocked/invalid result",
      `STATUS: ${statusText || "<none>"}\nENABLE EVIDENCE:\n${evidenceText || "<none>"}`
    );
  }
});


Then("the rendered provenance section labels include local repo and Workshop groups without enabled profile duplicates", function () {
  assertNoEnabledProfileProvenanceSections(this);
  const labels = renderedProvenanceSectionLabels(this);
  const checks = [
    ["local repo", /local[\s\S]{0,40}(repo|repository)|(repo|repository)[\s\S]{0,40}local/i],
    ["Workshop", /workshop/i],
  ];
  const combined = labels.join("\n");
  const missing = checks.filter(([, pattern]) => !pattern.test(combined)).map(([label]) => label);
  if (missing.length) {
    contractGap(
      this,
      `renderedProvenanceSectionLabels is missing provenance group labels: ${missing.join(", ")}`,
      `FOUND LABELS:\n${labels.join("\n") || "<none>"}`
    );
  }
  requireProvenanceSection(this, "local");
  requireProvenanceSection(this, "workshop");
});

Then("Runebound: Elixirs appears under local repo provenance", function () {
  const local = requireProvenanceSection(this, "local");
  requireSectionText(
    this,
    local,
    /Runebound: Elixirs|jml\.runebound-elixirs/i,
    "Runebound: Elixirs is not listed under the local repo provenance section"
  );
});

Then("active profile state is represented on the local repo row", function () {
  assertNoEnabledProfileProvenanceSections(this);
  const local = requireProvenanceSection(this, "local");
  const localItems = sectionItemObjects(local);
  const localRunebound = localItems.find((item) => objectMatchesPackage(item, RUNEBOUND_ID, RUNEBOUND_NAME));
  if (!localRunebound) {
    contractGap(
      this,
      "local repo provenance section does not include the active Runebound: Elixirs mod entry",
      `LOCAL SECTION:\n${objectText(local) || "<none>"}`
    );
  }
  const activeFlags = trueBooleanFlags(localRunebound, /^(enabled|active|isEnabled|isActive|enabledInProfile|profileEnabled)$/i);
  if (!activeFlags.length && !/\b(enabled|active)\b/i.test(objectText(localRunebound))) {
    contractGap(
      this,
      "Runebound: Elixirs local repo entry does not expose enabled active profile state",
      `LOCAL RUNEBOUND ENTRY:\n${objectText(localRunebound) || "<none>"}`
    );
  }
  const row = runeboundRow(renderedDetectedModRows(this));
  if (!row) {
    contractGap(this, "renderedDetectedModRows does not include Runebound: Elixirs", reportPreview(this));
  }
  if (!isEnabledModRow(row) || !hasGreenCheckPrefix(row)) {
    contractGap(
      this,
      "Runebound: Elixirs local Mods row does not preserve enabled state with a green check prefix",
      `RUNEBOUND ROW:\n${objectText(row) || "<none>"}`
    );
  }
});

Then("Steam Workshop subscriptions appear under Workshop provenance when detected", function () {
  const workshop = requireProvenanceSection(this, "workshop");
  requireSectionText(
    this,
    workshop,
    new RegExp(`${WORKSHOP_FIXTURE_ID}|${WORKSHOP_FIXTURE_TITLE}|Workshop subscription|hasBmlPackage`, "i"),
    "Workshop provenance section does not include detected Steam Workshop subscriptions"
  );
});

Then("the Workshop provenance section does not require subscriptions to be BML packages", function () {
  const workshop = requireProvenanceSection(this, "workshop");
  const items = sectionItemObjects(workshop);
  const hasFalseBmlFlag = (value) => valuesForKey(value, /(isBmlPackage|bmlPackage|hasBmlPackage|bmlPackageDetected|packageManifestPresent)$/i)
    .some((entry) => entry.value === false);
  const saysNonBml = (value) => /non[- ]?BML|not (?:a )?BML|without BML|no bml-package\.json|hasBmlPackage[\s\S]{0,20}false|packageManifestPresent[\s\S]{0,20}false/i.test(objectText(value));
  const nonBmlEntry =
    items.find((item) => new RegExp(`${WORKSHOP_FIXTURE_ID}|${WORKSHOP_FIXTURE_TITLE}`, "i").test(objectText(item)) && (hasFalseBmlFlag(item) || saysNonBml(item))) ||
    items.find((item) => hasFalseBmlFlag(item) || saysNonBml(item)) ||
    (hasFalseBmlFlag(workshop) || saysNonBml(workshop) ? workshop : null);
  if (!nonBmlEntry) {
    contractGap(
      this,
      "Workshop provenance does not prove subscriptions can be listed without BML package metadata",
      `WORKSHOP SECTION:\n${objectText(workshop) || "<none>"}`
    );
  }
});

Then("Environment summary rows include OS, Platform, and Game version", function () {
  const rows = environmentSummaryRows(this);
  const missing = [
    ["OS", "os"],
    ["Platform", "platform"],
  ].filter(([, kind]) => !summaryRowFor(rows, kind)).map(([label]) => label);
  if (!gameVersionSummaryRow(rows)) missing.push("Game version row");
  if (missing.length) {
    contractGap(
      this,
      `Environment summary rows/items are missing: ${missing.join(", ")}`,
      `ROWS:\n${rows.map(rowText).join("\n---\n") || "<none>"}`
    );
  }
});

Then("the Platform row value is Steam storefront instead of linux-x86_64", function () {
  const rows = environmentSummaryRows(this);
  const row = summaryRowFor(rows, "platform");
  if (!row) {
    contractGap(this, "Environment summary rows/items are missing Platform row");
  }
  const text = rowText(row);
  if (!/\bSteam\b|storefront/i.test(text)) {
    contractGap(this, "Platform row/value is not Steam/storefront", `PLATFORM ROW:\n${text || "<none>"}`);
  }
  if (/linux-x86_64/i.test(text)) {
    contractGap(this, "Platform row/value still shows linux-x86_64 instead of Steam/storefront", `PLATFORM ROW:\n${text}`);
  }
});

Then("the Environment smoke report proves the Steam logo is rendered", function () {
  const evidence = platformSteamLogoEvidence(this);
  if (!evidence.ok) {
    contractGap(
      this,
      "missing Steam logo/icon evidence in smoke report (expected steamLogoPath and steamLogoRendered, or platform Steam icon equivalents)",
      `steamLogoPath candidates:\n${evidence.paths.map((entry) => `${entry.pathParts.join(".")}=${entry.value}`).join("\n") || "<none>"}\n` +
      `steamLogoRendered candidates:\n${evidence.flags.map((entry) => entry.pathParts.join(".")).join("\n") || "<none>"}`
    );
  }
});

Then("the smoke report exposes rendered entity iconography for every major entity", function () {
  assertAllEntityIconography(this);
});

Then("Steam entity iconography keeps logo evidence or clear fallback text", function () {
  assertSteamEntityIconography(this);
});

Then("renderedEntityIcons pairs every icon with accessible text labels", function () {
  assertRenderedEntityIconsPairTextLabels(this);
});

Then("Environment summary rows render compact badge-like labels with text", function () {
  const rows = environmentSummaryRows(this);
  for (const [label, kind, expectedBadge] of [
    ["OS", "os", /\[OS\]|^OS$/i],
    ["Platform", "platform", /\[PLATFORM\]|^PLATFORM$|Steam/i],
    ["Game version", "version", /\[VERSION\]|^VERSION$/i],
  ]) {
    const row = summaryRowFor(rows, kind);
    if (!row) {
      contractGap(this, `Environment summary rows/items are missing ${label}`);
    }
    const compact = compactLabelForSummaryRow(row);
    if (!compact || compact.length > 18 || !expectedBadge.test(compact)) {
      contractGap(
        this,
        `${label} Environment summary row lacks a compact badge-like label`,
        `EXPECTED BADGE: ${expectedBadge}\nROW:\n${rowText(row) || "<none>"}`
      );
    }
    const text = rowText(row);
    if (!text || text.replace(compact, "").replace(label, "").trim().length < 2) {
      contractGap(this, `${label} Environment summary row lacks plain text value next to its compact label`, `ROW:\n${text || "<none>"}`);
    }
  }
});

Then("Environment compact status actions expose Launch BaronyModLoader and Launch Vanilla Barony", function () {
  const environment = requireEnvironmentNode(this);
  const actionTexts = [
    ...conceptPrimaryActionTexts(environment),
    ...conceptSecondaryActionTexts(environment),
    ...conceptActionArrays(environment).flatMap((entry) => entry.value.map((action) => JSON.stringify(action))),
  ].join("\n");
  const missing = [
    ["Launch BaronyModLoader", /Launch BaronyModLoader|launch-bml/i],
    ["Launch Vanilla Barony", /Launch Vanilla Barony|launch-vanilla/i],
  ].filter(([, pattern]) => !pattern.test(actionTexts)).map(([label]) => label);
  if (missing.length) {
    contractGap(
      this,
      `Environment launch action controls are missing: ${missing.join(", ")}`,
      `ENVIRONMENT ACTIONS:\n${actionTexts || "<none>"}`
    );
  }
  if (/Dry-run launch|dry-run-launch/i.test(actionTexts)) {
    contractGap(this, "Environment still exposes Dry-run launch as a user-facing action.", `ENVIRONMENT ACTIONS:\n${actionTexts}`);
  }
  const badTrueFlags = trueBooleanFlags(environment, /(processLaunched|processStarted|startedProcess|baronyStarted|startedBarony|launchedBarony|gameProcessStarted)$/i);
  if (badTrueFlags.length) {
    contractGap(this, `Environment says a Barony/game process started without explicit launch mock mode: ${badTrueFlags.join(", ")}`);
  }
});

Then("the smoke report includes full Environment launch action labels and Workshop warning text", function () {
  const environment = requireEnvironmentNode(this);
  const workshop = requireConceptEntry(this, "Workshop").node;
  const environmentText = JSON.stringify(environment, null, 2);
  const missingEnvironmentLabels = [
    "Launch BaronyModLoader",
    "Launch Vanilla Barony",
    "Detect install",
    "Refresh readiness",
    "Open diagnostics",
  ].filter((label) => !new RegExp(escapeRegex(label), "i").test(environmentText));
  if (missingEnvironmentLabels.length) {
    contractGap(
      this,
      `Environment action labels are missing or clipped: ${missingEnvironmentLabels.join(", ")}`,
      `ENVIRONMENT EVIDENCE:\n${environmentText || "<none>"}`
    );
  }
  const workshopText = objectText(workshop);
  if (!/Workshop preparation is dry-run only; Steam publishing remains disabled\.|Steam publish(?:ing)? (?:remains )?disabled|No-publish guard/i.test(workshopText)) {
    contractGap(
      this,
      "Workshop warning/status text is missing or clipped",
      `WORKSHOP EVIDENCE:\n${workshopText || "<none>"}`
    );
  }
  const clippingFlags = trueVisualClippingFlags({ environment, workshop });
  if (clippingFlags.length) {
    contractGap(this, `smoke report exposes clipped/truncated visual metadata: ${clippingFlags.join(", ")}`);
  }
});

Then("hidden smoke window metadata proves no widget was auto-focused", function () {
  requireHiddenNoAutofocusMetadata(this);
});

Then("important labels, buttons, and warnings expose no-clipping metadata", function () {
  requireImportantNoClippingMetadata(this);
});

Then("diagnostic details remain available in smoke report details fields", function () {
  assertDiagnosticsDetailsRemainAvailable(this);
});

Then("the top-level GUI cards are exactly Environment, Profiles, Mods, and Workshop", function () {
  if (!reportShowsOpened(this.profileFirstGuiReport)) {
    contractGap(this, "smoke report does not prove a Tk root/window opened");
  }
  const labels = topLevelLabels(this.profileFirstGuiReport);
  const canonical = labels.map(canonicalConceptLabel);
  const missing = CONCEPT_LABELS.filter((label) => !canonical.includes(label));
  const extras = labels.filter((label, index) => canonical[index] === null || !CONCEPT_LABELS.includes(canonical[index]));
  const wrongOrder = canonical.length === CONCEPT_LABELS.length && canonical.some((label, index) => label !== CONCEPT_LABELS[index]);
  if (missing.length || extras.length || labels.length !== CONCEPT_LABELS.length || wrongOrder) {
    contractGap(
      this,
      "top-level GUI cards must be exactly Environment, Profiles, Mods, Workshop in that order",
      `EXPECTED: ${CONCEPT_LABELS.join(", ")}\nFOUND: ${labels.join(", ") || "<none>"}`
    );
  }
});

Then("the GUI does not render separate top-level Actions, Views, Diagnostics, Windows Status, or Launch Dry Run sections", function () {
  const forbidden = topLevelForbiddenLabels(this.profileFirstGuiReport);
  if (forbidden.length) {
    contractGap(this, `forbidden top-level sections rendered separately: ${forbidden.join(", ")}`);
  }
});

Then("the concept cards expose visual hierarchy instead of a flat action strip", function () {
  assertNoFlatTopLevelActionStrip(this);
  for (const conceptLabel of CONCEPT_LABELS) assertConceptHierarchy(this, conceptLabel);
});

Then("the Environment concept contains readiness, runtime, diagnostics, Windows fail-closed, and GUI launch controls", function () {
  const text = requireConceptPatterns(this, "Environment", [
    ["readiness status", /readiness|ready|blocked/i],
    ["runtime evidence", /runtime|manifest|BML_RUNTIME_MANIFEST|Steam Barony|linux/i],
    ["diagnostics evidence", /diagnostics?|evidence|production|validation/i],
    ["Windows fail-closed status", /windows[\s\S]{0,160}(fail[- ]?closed|blocked|unsupported|unverified|not supported)|(?:fail[- ]?closed|blocked|unsupported|unverified|not supported)[\s\S]{0,160}windows/i],
    ["BML launch control", /Launch BaronyModLoader|launch-bml/i],
    ["Vanilla launch control", /Launch Vanilla Barony|launch-vanilla/i],
  ]);
  if (/windows[\s\S]{0,120}(playable|ready|supported)[\s\S]{0,80}(true|yes|available)|windows[\s\S]{0,120}(true|yes|available)[\s\S]{0,80}(playable|ready|supported)/i.test(text)) {
    contractGap(this, "Environment appears to claim Windows playable/ready support", `ENVIRONMENT EVIDENCE:\n${text}`);
  }
});

Then("Environment keeps diagnostics, Windows status, readiness, and GUI launch controls inside the concept card", function () {
  const forbidden = topLevelForbiddenLabels(this.profileFirstGuiReport);
  if (forbidden.length) {
    contractGap(this, `Environment-only details leaked into top-level sections: ${forbidden.join(", ")}`);
  }
  requireConceptPatterns(this, "Environment", [
    ["diagnostics", /diagnostics?/i],
    ["Windows", /windows/i],
    ["readiness", /readiness|ready|blocked/i],
    ["BML launch control", /Launch BaronyModLoader|launch-bml/i],
    ["Vanilla launch control", /Launch Vanilla Barony|launch-vanilla/i],
  ]);
});

Then("the Profiles concept shows a stable selected profile path outside the repository .tmp area", function () {
  const entry = requireConceptEntry(this, "Profiles");
  const candidates = profilePathCandidates(entry.node);
  if (!candidates.length) {
    contractGap(this, "Profiles concept has no selected profile path/profileDir/profileRoot", `PROFILES EVIDENCE:\n${objectText(entry.node) || "<none>"}`);
  }
  const selected = candidates.find((candidate) => /selected|current|active|profile/i.test(candidate.pathParts.join("."))) || candidates[0];
  if (pathHasDotTmp(selected.value)) {
    contractGap(this, `profile path must not be under a .tmp segment: ${selected.value}`);
  }
  if (!path.isAbsolute(selected.value)) {
    contractGap(this, `profile path should be absolute and stable, got: ${selected.value}`);
  }
});

Then("the Profiles concept shows active mods and create or select profile controls", function () {
  requireConceptPatterns(this, "Profiles", [
    ["selected/current profile", /selected|current|active|profile/i],
    ["active mods", /active[\s\S]{0,80}mods|mods[\s\S]{0,80}active|activePackageIds|enabledPackages/i],
    ["create/select profile control", /create[\s\S]{0,80}profile|select[\s\S]{0,80}profile|profile[\s\S]{0,80}(create|select)/i],
  ]);
});

Then("the Mods concept shows package list and Runebound: Elixirs package details with id, version, validation, carrier, and capabilities", function () {
  const entry = requireConceptEntry(this, "Mods");
  const text = objectText(entry.node);
  if (!/(package|packages|library|local mods|mod list)/i.test(text)) {
    contractGap(this, "Mods concept does not expose the local package list/library", `MODS EVIDENCE:\n${text || "<none>"}`);
  }
  const evidence = runeboundPackageEvidence(entry.node);
  if (!evidence.listEntries.length) {
    contractGap(this, "Runebound: Elixirs is not present as an object in the Mods package list/library/catalog", `MODS EVIDENCE:\n${text || "<none>"}`);
  }
  if (!evidence.packageObjects.length) {
    contractGap(this, "Runebound: Elixirs is listed but no selected/details package object is visible in Mods", `PACKAGE LIST ENTRY:\n${objectText(evidence.listEntries[0].node)}`);
  }
  const combined = evidence.combined || text;
  const checks = [
    ["package id jml.runebound-elixirs", /jml\.runebound-elixirs/],
    ["semantic version", /\bversion\b[\s\S]{0,80}(?:0\.1\.0|\d+\.\d+\.\d+)|(?:0\.1\.0|\d+\.\d+\.\d+)[\s\S]{0,80}\bversion\b/i],
    ["validation status", /validation[\s\S]{0,80}(valid|ok|passed|clean)|\b(valid|ok|passed|clean)\b[\s\S]{0,80}validation/i],
    ["carrier item", /carrier|POTION_STRENGTH/i],
    ["capabilities", /capabilit/i],
    ["elixir capability ids", /elixir_item_metadata|elixir_drop_generation|elixir_consumption|active_elixir_effect_state|item_name_tooltip_rendering|multiplayer_version_metadata/i],
  ];
  const missing = checks.filter(([, pattern]) => !pattern.test(combined)).map(([name]) => name);
  if (missing.length) {
    contractGap(this, `Runebound package details are missing from Mods: ${missing.join(", ")}`, `MODS PACKAGE EVIDENCE:\n${combined || "<none>"}`);
  }
});

Then("the Mods concept exposes enable and disable controls for Runebound: Elixirs", function () {
  const entry = requireConceptEntry(this, "Mods");
  const text = objectText(entry.node);
  if (!/\benable\b/i.test(text) || !/\bdisable\b/i.test(text)) {
    contractGap(this, "Mods concept does not expose both enable and disable controls", `MODS EVIDENCE:\n${text || "<none>"}`);
  }
  if (!/Runebound: Elixirs|jml\.runebound-elixirs/i.test(text)) {
    contractGap(this, "Mods enable/disable controls are not tied to Runebound: Elixirs", `MODS EVIDENCE:\n${text || "<none>"}`);
  }

  for (const item of valuesForKey(entry.node, /(activePackageIds|enabledPackageIds|enabledPackages|activePackages)$/i)) {
    if (Array.isArray(item.value)) {
      const stringified = item.value.map((packageItem) => typeof packageItem === "string" ? packageItem : JSON.stringify(packageItem));
      if (countInArray(stringified, RUNEBOUND_ID) > 1) {
        contractGap(this, `Runebound appears more than once in Mods active package state at ${item.pathParts.join(".")}`);
      }
    }
  }
});

Then("the Workshop concept shows metadata, preview validation, local staging, and dry-run no-publish status", function () {
  const text = requireConceptPatterns(this, "Workshop", [
    ["Workshop metadata", /metadata|title|description|visibility|publishedfileid/i],
    ["preview validation", /preview[\s\S]{0,80}(validation|valid|status|blocked|ok)|(?:validation|valid|status|blocked|ok)[\s\S]{0,80}preview/i],
    ["local staging/VDF", /staging|stage|vdf|dry[- ]?run[\s\S]{0,80}report/i],
    ["dry-run no-publish", /dry[- ]?run[\s\S]{0,120}(no[- ]?publish|publish(?:ing)?(?: is)? disabled|publishEnabled[\s\S]{0,40}false|canPublish[\s\S]{0,40}false)|(?:no[- ]?publish|publish(?:ing)?(?: is)? disabled|publishEnabled[\s\S]{0,40}false|canPublish[\s\S]{0,40}false)[\s\S]{0,120}dry[- ]?run/i],
  ]);
  const requiredMetadata = ["title", "description", "visibility", "publishedfileid"];
  const missingMetadata = requiredMetadata.filter((field) => !new RegExp(field, "i").test(text));
  if (missingMetadata.length) {
    contractGap(this, `Workshop concept metadata rows are missing: ${missingMetadata.join(", ")}`, `WORKSHOP EVIDENCE:\n${text || "<none>"}`);
  }
});

Then("the Workshop concept exposes no Steam publish side effects", function () {
  const entry = requireConceptEntry(this, "Workshop");
  const badPublishFlags = trueBooleanFlags(entry.node, /(publishEnabled|canPublish|allowPublish|wouldPublish|steamSideEffects)$/i);
  if (badPublishFlags.length) {
    contractGap(this, `Workshop exposes publishing or Steam side effects: ${badPublishFlags.join(", ")}`);
  }
  const text = objectText(entry.node);
  if (!/(no[- ]?publish|publish(?:ing)?(?: is)? disabled|publishEnabled[\s\S]{0,40}false|canPublish[\s\S]{0,40}false|steamSideEffects[\s\S]{0,40}false)/i.test(text)) {
    contractGap(this, "Workshop concept lacks explicit no-publish/no Steam side-effect evidence", `WORKSHOP EVIDENCE:\n${text || "<none>"}`);
  }
});

Then("the GUI makes no playable claim for Runebound: Elixirs", function () {
  const claim = runeboundPlayableClaim(this.profileFirstGuiReport);
  if (claim) {
    contractGap(this, `Runebound playable claim is not allowed without live gameplay proof: ${claim}`);
  }
  if (!stringsFromValues(this.profileFirstGuiReport).some((value) => value.includes(RUNEBOUND_NAME) || value.includes(RUNEBOUND_ID))) {
    contractGap(this, "smoke report contains no Runebound: Elixirs evidence to evaluate for playable claims");
  }
});
