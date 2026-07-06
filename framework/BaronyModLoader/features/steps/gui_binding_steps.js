"use strict";

const { After, Given, When, Then } = require("@cucumber/cucumber");
const { execFileSync } = require("child_process");
const fs = require("fs");
const os = require("os");
const path = require("path");

// paths: <repo>/framework/BaronyModLoader/features/steps/gui_binding_steps.js
//          ../../../../ from here = <repo>
const REPO_ROOT = path.resolve(__dirname, "../../../../");
const BML_APP = path.join(REPO_ROOT, "framework/BaronyModLoader/app/barony_mod_loader.py");

function runGuiBindingPython(script, world) {
  const tmp = path.join(os.tmpdir(), `bml-gui-binding-${Date.now()}-${Math.random().toString(36).slice(2)}.py`);
  fs.writeFileSync(tmp, script, "utf-8");
  try {
    const stdout = execFileSync("/usr/bin/python3", [tmp], {
      cwd: REPO_ROOT,
      encoding: "utf-8",
      timeout: 30000,
      stdio: ["ignore", "pipe", "pipe"],
    });
    world.guiBindingRaw = stdout;
    world.guiBindingProbe = JSON.parse(stdout);
  } catch (err) {
    const stdout = err.stdout ? String(err.stdout) : "";
    const stderr = err.stderr ? String(err.stderr) : "";
    throw new Error(`GUI binding probe crashed.\nstdout:\n${stdout}\nstderr:\n${stderr}`);
  } finally {
    try {
      fs.unlinkSync(tmp);
    } catch (_) {
      // Best-effort temp cleanup only.
    }
  }
}

function pyProbe(body) {
  return `
import dataclasses
import importlib.util
import json
import sys
from pathlib import Path

APP_PATH = ${JSON.stringify(BML_APP)}

spec = importlib.util.spec_from_file_location("bml_app_gui_binding_probe", APP_PATH)
mod = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = mod
spec.loader.exec_module(mod)

SECTIONS = ["install", "profile", "package", "readiness", "diagnostics", "workshop"]


def to_plain(value):
    if dataclasses.is_dataclass(value):
        return {key: to_plain(val) for key, val in dataclasses.asdict(value).items()}
    if isinstance(value, dict):
        return {str(key): to_plain(val) for key, val in value.items()}
    if isinstance(value, (list, tuple, set)):
        return [to_plain(item) for item in value]
    if isinstance(value, (str, int, float, bool)) or value is None:
        return value
    if hasattr(value, "__dict__"):
        return {key: to_plain(val) for key, val in vars(value).items() if not key.startswith("_")}
    return repr(value)


def emit(payload):
    print(json.dumps(to_plain(payload), sort_keys=True))


def all_text(value):
    if isinstance(value, dict):
        return "\\n".join([str(k) for k in value.keys()] + [all_text(v) for v in value.values()])
    if isinstance(value, list):
        return "\\n".join(all_text(v) for v in value)
    return "" if value is None else str(value)


def values_for_key(value, wanted):
    out = []
    wanted_lower = {key.lower() for key in wanted}
    if isinstance(value, dict):
        for key, item in value.items():
            if str(key).lower() in wanted_lower:
                out.append(item)
            out.extend(values_for_key(item, wanted))
    elif isinstance(value, list):
        for item in value:
            out.extend(values_for_key(item, wanted))
    return out


def extract_section_names(value):
    names = set()
    if isinstance(value, str):
        lowered = value.casefold()
        for section in SECTIONS:
            if section in lowered:
                names.add(section)
    elif isinstance(value, dict):
        for key, item in value.items():
            key_lower = str(key).casefold()
            if key_lower in SECTIONS:
                names.add(key_lower)
            if key_lower in {"section", "sectionid", "name", "id", "key", "semanticsection"} and isinstance(item, str):
                item_lower = item.casefold()
                if item_lower in SECTIONS:
                    names.add(item_lower)
            names.update(extract_section_names(item))
    elif isinstance(value, list):
        for item in value:
            names.update(extract_section_names(item))
    return names


def find_callable(names):
    for name in names:
        fn = getattr(mod, name, None)
        if callable(fn):
            return name, fn
    return None, None


def try_call_candidates(names, variants):
    errors = {}
    for name in names:
        fn = getattr(mod, name, None)
        if not callable(fn):
            continue
        for label, args, kwargs in variants:
            try:
                return {
                    "found": name,
                    "variant": label,
                    "value": to_plain(fn(*args, **kwargs)),
                }
            except Exception as exc:
                errors[f"{name}:{label}"] = f"{type(exc).__name__}: {exc}"
    return {"found": None, "errors": errors}


def dashboard_state():
    if callable(getattr(mod, "build_core_dashboard_state", None)):
        return to_plain(mod.build_core_dashboard_state(
            install={"status": "verified", "path": "/tmp/Barony", "icon": "os.linux"},
            profile={"status": "selected", "profile": {"id": "gui-binding-profile"}},
            workshop={"publishEnabled": False, "status": "disabled_stub", "icon": "store.steam_workshop"},
            platform="linux-x86_64",
        ))
    return {
        "install": {"status": "verified", "icon": "os.linux"},
        "profile": {"status": "selected", "profile": {"id": "gui-binding-profile"}},
        "package": {"status": "valid", "id": "jml.runebound-elixirs"},
        "readiness": {"status": "blocked", "disabledReasons": ["Runtime is missing or not registered."]},
        "diagnostics": {"status": "not_run", "icon": "runtime.not_run"},
        "workshop": {"status": "disabled_stub", "publishEnabled": False, "icon": "store.steam_workshop"},
        "disabled_reasons": ["Steam Workshop publish is disabled; dry-run/stub preview only."],
    }

${body}
`;
}

function requireProbeOk(world, label) {
  const probe = world.guiBindingProbe;
  if (!probe) throw new Error(`${label} did not produce a probe result.`);
  if (!probe.ok) {
    const candidates = probe.candidates ? `\nCandidate APIs: ${probe.candidates.join(", ")}` : "";
    const errors = probe.errors ? `\nProbe errors: ${JSON.stringify(probe.errors, null, 2)}` : "";
    const details = probe.details ? `\nDetails: ${JSON.stringify(probe.details, null, 2)}` : "";
    throw new Error(`${probe.gap || `${label} contract failed.`}${candidates}${errors}${details}`);
  }
  return probe;
}

Given("a headless GUI Binding staging directory", function () {
  this.guiBindingStagingDir = fs.mkdtempSync(path.join(os.tmpdir(), "bml-gui-binding-"));
});

After(function () {
  if (this.guiBindingStagingDir) {
    fs.rmSync(this.guiBindingStagingDir, { recursive: true, force: true });
  }
});

Given("the GUI binding app-core module path", function () {
  this.guiBindingApp = BML_APP;
});

When("I probe the GUI one-page shell binding contract", function () {
  runGuiBindingPython(pyProbe(`
candidates = [
    "build_gui_shell_model",
    "build_gui_shell_state",
    "build_one_page_shell_model",
    "build_one_page_shell_state",
    "build_gui_view_model",
]
dashboard = dashboard_state()
result = try_call_candidates(candidates, [
    ("dashboard-positional", (dashboard,), {}),
    ("semantic-state-positional", ({"dashboard": dashboard, "sections": SECTIONS},), {}),
    ("dashboard-keyword", (), {"dashboard": dashboard}),
    ("state-keyword", (), {"state": {"dashboard": dashboard, "sections": SECTIONS}}),
])
if not result.get("found"):
    emit({
        "ok": False,
        "candidates": candidates,
        "errors": result.get("errors", {}),
        "gap": "Missing pure GUI shell binding/view-model API. Implement one candidate that returns a headless one-page shell model with install, profile, package, readiness, diagnostics, and workshop sections.",
    })
else:
    value = result["value"]
    text = all_text(value).casefold()
    sections = extract_section_names(value)
    pages = values_for_key(value, ["pages"])
    too_many_pages = any(isinstance(page_list, list) and len(page_list) > 1 for page_list in pages)
    has_shell_container = isinstance(value, dict) and any(key.casefold() in {"page", "pageid", "shell", "sections", "layout"} for key in value.keys())
    missing = [section for section in SECTIONS if section not in sections]
    emit({
        "ok": not missing and not too_many_pages and has_shell_container and "tkinter" not in text,
        "found": result["found"],
        "variant": result["variant"],
        "sections": sorted(sections),
        "details": {"missingSections": missing, "tooManyPages": too_many_pages, "hasShellContainer": has_shell_container},
        "gap": "GUI shell binding must be a single headless page model with all six semantic sections and no tkinter/display objects.",
    })
`), this);
});

Then("the shell binding exposes one page with install, profile, package, readiness, diagnostics, and workshop sections", function () {
  requireProbeOk(this, "GUI one-page shell binding");
});

When("I probe the GUI widget semantic binding contract", function () {
  runGuiBindingPython(pyProbe(`
def collect_bindings(value, out=None):
    out = out or []
    id_keys = {"id", "key", "widget", "widgetid", "control", "controlid"}
    path_keys = {"path", "source", "sourcepath", "semanticpath", "bindsto", "binding", "field"}
    if isinstance(value, dict):
        lowered = {str(key).casefold(): item for key, item in value.items()}
        has_id = any(key in lowered for key in id_keys)
        has_path = any(key in lowered for key in path_keys)
        if has_id and has_path:
            out.append(value)
        for key, item in value.items():
            if isinstance(item, str) and "." in item and str(key).casefold() not in {"label", "text"}:
                out.append({"widgetId": str(key), "semanticPath": item})
            collect_bindings(item, out)
    elif isinstance(value, list):
        for item in value:
            collect_bindings(item, out)
    return out

candidates = [
    "build_gui_widget_bindings",
    "build_dashboard_widget_bindings",
    "build_gui_binding_view_model",
    "build_semantic_widget_bindings",
    "bind_dashboard_widgets",
]
dashboard = dashboard_state()
result = try_call_candidates(candidates, [
    ("dashboard-positional", (dashboard,), {}),
    ("dashboard-keyword", (), {"dashboard": dashboard}),
    ("state-keyword", (), {"state": dashboard}),
])
if not result.get("found"):
    emit({
        "ok": False,
        "candidates": candidates,
        "errors": result.get("errors", {}),
        "gap": "Missing pure GUI widget binding API. Implement one candidate that maps stable widget ids to semantic dashboard paths such as install.status, profile.status, readiness.status, and workshop.publishEnabled.",
    })
else:
    bindings = collect_bindings(result["value"])
    path_text = all_text(bindings).casefold()
    required = ["install", "profile", "readiness", "workshop"]
    missing = [name for name in required if name not in path_text]
    emit({
        "ok": len(bindings) >= 4 and not missing,
        "found": result["found"],
        "variant": result["variant"],
        "bindingCount": len(bindings),
        "details": {"missingSemanticPaths": missing, "sample": bindings[:6]},
        "gap": "Widget binding view model must expose stable widget ids backed by semantic dashboard source paths, not raw CLI text or private widget state.",
    })
`), this);
});

Then("the widget bindings expose stable widget ids backed by dashboard semantic paths", function () {
  requireProbeOk(this, "GUI widget semantic binding");
});

When("I probe the GUI icon text accessibility binding contract", function () {
  runGuiBindingPython(pyProbe(`
def collect_icons(value, out=None):
    out = out or []
    if isinstance(value, dict):
        for key, item in value.items():
            key_lower = str(key).casefold()
            if key_lower in {"icon", "osicon", "storeicon", "runtimeicon"} and isinstance(item, str):
                out.append(item)
            collect_icons(item, out)
    elif isinstance(value, list):
        for item in value:
            collect_icons(item, out)
    return out

candidates = [
    "build_gui_icon_accessibility",
    "build_icon_accessibility_labels",
    "build_icon_label_bindings",
    "gui_icon_labels",
]
dashboard = dashboard_state()
surfaced_icons = sorted(set(collect_icons(dashboard) + ["os.linux", "os.windows", "os.darwin", "store.steam", "store.steam_workshop", "runtime.not_run", "runtime.failed"]))
result = try_call_candidates(candidates, [
    ("icons-positional", (surfaced_icons,), {}),
    ("dashboard-positional", (dashboard,), {}),
    ("icons-keyword", (), {"icons": surfaced_icons}),
])
labels = None
source = None
if result.get("found"):
    labels = result["value"]
    source = result["found"]
elif isinstance(getattr(mod, "ICON_LABELS", None), dict):
    labels = to_plain(getattr(mod, "ICON_LABELS"))
    source = "ICON_LABELS"
else:
    emit({
        "ok": False,
        "candidates": candidates + ["ICON_LABELS"],
        "errors": result.get("errors", {}),
        "gap": "Missing icon accessibility mapping. GUI bindings must expose text for every surfaced icon token.",
    })
    raise SystemExit(0)

missing = []
blank = []
for icon in surfaced_icons:
    label = labels.get(icon) if isinstance(labels, dict) else None
    if label is None:
        missing.append(icon)
    elif not isinstance(label, str) or not label.strip():
        blank.append(icon)
emit({
    "ok": not missing and not blank,
    "found": source,
    "icons": surfaced_icons,
    "details": {"missingLabels": missing, "blankLabels": blank},
    "gap": "Every icon token surfaced to GUI bindings must have non-empty accessible text.",
})
`), this);
});

Then("every surfaced GUI icon has accessible text", function () {
  requireProbeOk(this, "GUI icon text accessibility binding");
});

When("I execute a failing command through the GUI command result contract", function () {
  runGuiBindingPython(pyProbe(`
fn = getattr(mod, "run_command", None)
if not callable(fn):
    emit({
        "ok": False,
        "candidates": ["run_command", "run_gui_command"],
        "gap": "Missing command result API. GUI command binding needs a semantic command DTO preserving argv, exit code, stdout, stderr, duration, and failure summary.",
    })
else:
    command = ["/usr/bin/python3", "-c", "import sys; print('visible stdout'); print('visible stderr', file=sys.stderr); sys.exit(17)"]
    value = to_plain(fn(command, label="intentional failing gui command"))
    ok = (
        isinstance(value, dict)
        and value.get("exit_code") == 17
        and "visible stdout" in str(value.get("stdout", ""))
        and "visible stderr" in str(value.get("stderr", ""))
        and "visible stderr" in str(value.get("failure_summary", ""))
        and isinstance(value.get("argv"), list)
        and value.get("argv")[:2] == ["/usr/bin/python3", "-c"]
    )
    emit({
        "ok": ok,
        "found": "run_command",
        "result": value,
        "gap": "Command result DTO must preserve failing command argv, exact non-zero exit code, stdout, stderr, and user-facing failure summary for GUI rendering.",
    })
`), this);
});

Then("the command result preserves argv, exit code, stdout, stderr, and failure summary", function () {
  requireProbeOk(this, "GUI command failure preservation");
});

When("I probe the GUI slow command responsiveness contract", function () {
  runGuiBindingPython(pyProbe(`
candidates = [
    "build_gui_command_view_model",
    "build_command_task_binding",
    "build_command_status_view_model",
    "build_slow_command_view_model",
    "build_gui_command_state",
]
running_task = {
    "id": "package-install-runebound",
    "label": "Install Runebound: Elixirs",
    "argv": ["bml", "package", "install", "runebound"],
    "status": "running",
    "startedAt": "2026-07-06T00:00:00Z",
    "stdout": "",
    "stderr": "",
}
result = try_call_candidates(candidates, [
    ("task-positional", (running_task,), {}),
    ("task-keyword", (), {"task": running_task}),
    ("command-keyword", (), {"command": running_task}),
])
if not result.get("found"):
    emit({
        "ok": False,
        "candidates": candidates,
        "errors": result.get("errors", {}),
        "gap": "Missing non-blocking command view-model API. Implement a pure GUI command binding that can render a running/pending task immediately, without waiting for command completion or requiring a display.",
    })
else:
    value = result["value"]
    text = all_text(value).casefold()
    pending = any(word in text for word in ["running", "pending", "in_progress", "busy"])
    completed = any(word in text for word in ["completed", "succeeded", "success", "exit_code: 0"])
    responsive_affordance = any(word in text for word in ["cancel", "progress", "spinner", "busy", "pending", "running"])
    emit({
        "ok": pending and not completed and responsive_affordance,
        "found": result["found"],
        "variant": result["variant"],
        "details": {"pendingState": pending, "completedState": completed, "responsiveAffordance": responsive_affordance, "value": value},
        "gap": "Slow command binding must expose a pending/running state and responsive affordance instead of only returning completed command results.",
    })
`), this);
});

Then("the command binding exposes a pending state without blocking for command completion", function () {
  requireProbeOk(this, "GUI slow command responsiveness binding");
});

When("I probe the GUI disabled action reason contract", function () {
  runGuiBindingPython(pyProbe(`
def collect_disabled_actions(value, out=None):
    out = out or []
    reason_keys = {"reason", "reasons", "disabledreason", "disabledreasons", "why", "tooltip"}
    if isinstance(value, dict):
        lowered = {str(key).casefold(): item for key, item in value.items()}
        enabled = lowered.get("enabled")
        disabled = lowered.get("disabled")
        has_disabled_state = enabled is False or disabled is True or str(lowered.get("status", "")).casefold() in {"disabled", "blocked"}
        reasons = []
        for key, item in lowered.items():
            if key in reason_keys:
                if isinstance(item, list):
                    reasons.extend(str(entry) for entry in item if str(entry).strip())
                elif str(item).strip():
                    reasons.append(str(item))
        if has_disabled_state and reasons:
            out.append(value)
        for item in value.values():
            collect_disabled_actions(item, out)
    elif isinstance(value, list):
        for item in value:
            collect_disabled_actions(item, out)
    return out

candidates = [
    "build_gui_action_states",
    "build_disabled_action_reasons",
    "build_gui_action_bindings",
    "build_action_bindings",
    "build_action_state_view_model",
]
readiness = {
    "status": "blocked",
    "disabledReasons": [
        "Barony install is missing or not selected.",
        "Profile is missing or must be selected.",
        "Package is missing or must be selected.",
        "Runtime is missing or not registered.",
    ],
    "rows": [
        {"key": "install", "status": "blocked", "blocker": "Barony install is missing or not selected."},
        {"key": "profile", "status": "blocked", "blocker": "Profile is missing or must be selected."},
        {"key": "package", "status": "blocked", "blocker": "Package is missing or must be selected."},
    ],
}
dashboard = dashboard_state()
dashboard["readiness"] = readiness
result = try_call_candidates(candidates, [
    ("dashboard-positional", (dashboard,), {}),
    ("readiness-positional", (readiness,), {}),
    ("dashboard-keyword", (), {"dashboard": dashboard}),
    ("readiness-keyword", (), {"readiness": readiness}),
])
if not result.get("found"):
    emit({
        "ok": False,
        "candidates": candidates,
        "errors": result.get("errors", {}),
        "gap": "Missing disabled-action reason API. Implement a pure GUI action binding that marks actions disabled and carries specific user-facing reasons from readiness/dashboard state.",
    })
else:
    actions = collect_disabled_actions(result["value"])
    text = all_text(actions).casefold()
    missing_terms = [term for term in ["install", "profile", "package"] if term not in text]
    emit({
        "ok": len(actions) >= 1 and not missing_terms,
        "found": result["found"],
        "variant": result["variant"],
        "disabledActionCount": len(actions),
        "details": {"missingReasonTerms": missing_terms, "sample": actions[:5]},
        "gap": "Disabled GUI actions must include concrete user-facing reasons sourced from semantic readiness/dashboard state.",
    })
`), this);
});

Then("disabled GUI actions include specific user-facing reasons", function () {
  requireProbeOk(this, "GUI disabled action reason binding");
});
