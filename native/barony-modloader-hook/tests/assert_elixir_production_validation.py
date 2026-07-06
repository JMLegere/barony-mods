#!/usr/bin/env python3
"""Assert Runebound: Elixirs production validation reports.

This helper is intentionally stricter than the older fake-provider smoke checks:
production proof may validate installed hooks and detoured display behavior, but it
must not claim player-facing gameplay unless the four live gameplay gates carry
direct, safe, non-fake evidence from a real Barony executable run.
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path
from typing import Any, Iterable, Sequence

MOD_ID = "jml.runebound-elixirs"
EXPECTED_DISPLAY = "Elixir of the Iron Vow"
EXPECTED_DESCRIPTION = "Bargain: +2 STR, -1 DEX for the rest of the run."
EXPECTED_CARRIER_ID = "runebound_elixirs:iron_vow"
EXPECTED_CARRIER_ITEM_TYPE = "POTION_STRENGTH"
EXPECTED_HOOK_SYMBOLS = {
    "useItem": "_Z7useItemP4ItemiP6Entitybb",
    "Item::getName": "_ZNK4Item7getNameEv",
    "Item::description": "_ZNK4Item11descriptionEv",
    "statGetSTR": "_Z10statGetSTRP4StatP6Entity",
    "statGetDEX": "_Z10statGetDEXP4StatP6Entity",
    "actHudWeapon": "_Z12actHudWeaponP6Entity",
}
LIVE_GAMEPLAY_GATES = {
    "solo_drop_use_save",
    "present_class_pool",
    "party_size_gating",
    "mismatch_rejection",
}

MISSING = object()
PASS_STATUSES = {"pass", "passed", "ok", "success", "succeeded"}
SKIP_STATUSES = {"skip", "skipped", "not_run", "not-run", "not run"}


class ValidationError(Exception):
    """Raised when a report does not satisfy the production proof contract."""


def normalized_key(value: str) -> str:
    return re.sub(r"[^a-z0-9]", "", value.lower())


def describe(value: Any) -> str:
    rendered = repr(value)
    if len(rendered) > 240:
        return rendered[:237] + "..."
    return rendered


def fail(message: str) -> None:
    raise ValidationError(message)


def load_json(path: Path, label: str) -> Any:
    try:
        with path.open("r", encoding="utf-8") as file:
            return json.load(file)
    except FileNotFoundError:
        fail(f"{label}: report file does not exist: {path}")
    except json.JSONDecodeError as exc:
        fail(f"{label}: invalid JSON at line {exc.lineno} column {exc.colno}: {exc.msg}")


def get_path(report: Any, path: str) -> Any:
    value = report
    for part in path.split("."):
        if not isinstance(value, dict) or part not in value:
            return MISSING
        value = value[part]
    return value


def first_path(report: Any, *paths: str) -> tuple[str, Any]:
    for path in paths:
        value = get_path(report, path)
        if value is not MISSING:
            return path, value
    return "", MISSING


def require_path(report: Any, label: str, *paths: str) -> tuple[str, Any]:
    path, value = first_path(report, *paths)
    if value is MISSING:
        fail(f"{label}: missing required field; tried {', '.join(paths)}")
    return path, value


def require_equal(report: Any, label: str, expected: Any, *paths: str) -> Any:
    path, value = require_path(report, label, *paths)
    if value != expected:
        fail(f"{label}: expected {path} == {describe(expected)}, got {describe(value)}")
    return value


def require_bool(report: Any, label: str, expected: bool, *paths: str) -> bool:
    path, value = require_path(report, label, *paths)
    if value is not expected:
        fail(f"{label}: expected {path} is {expected}, got {describe(value)}")
    return value


def require_status(report: Any, label: str, allowed: Iterable[str], *paths: str) -> str:
    path, value = require_path(report, label, *paths)
    if not isinstance(value, str) or value.lower() not in set(allowed):
        fail(f"{label}: expected {path} status in {sorted(set(allowed))}, got {describe(value)}")
    return value


def require_int(report: Any, label: str, expected: int, *paths: str) -> int:
    path, value = require_path(report, label, *paths)
    if not isinstance(value, int) or value != expected:
        fail(f"{label}: expected {path} == {expected}, got {describe(value)}")
    return value


def direct_value(container: Any, *keys: str) -> Any:
    if not isinstance(container, dict):
        return MISSING
    wanted = {normalized_key(key) for key in keys}
    for key, value in container.items():
        if normalized_key(str(key)) in wanted:
            return value
    return MISSING

def first_direct_value(container: Any, *keys: str) -> Any:
    if not isinstance(container, dict):
        return MISSING
    for key in keys:
        wanted = normalized_key(key)
        for existing_key, value in container.items():
            if normalized_key(str(existing_key)) == wanted:
                return value
    return MISSING


def find_key_recursive(container: Any, *keys: str) -> Any:
    if isinstance(container, dict):
        direct = direct_value(container, *keys)
        if direct is not MISSING:
            return direct
        for value in container.values():
            found = find_key_recursive(value, *keys)
            if found is not MISSING:
                return found
    elif isinstance(container, list):
        for value in container:
            found = find_key_recursive(value, *keys)
            if found is not MISSING:
                return found
    return MISSING


def value_as_text(value: Any) -> str:
    if value is MISSING or value is None:
        return ""
    return str(value)


def status_text(value: Any) -> str:
    return value_as_text(value).strip().lower().replace("-", "_")


def errors_are_empty(report: Any, label: str) -> None:
    path, errors = first_path(report, "errors")
    if errors is MISSING:
        return
    if errors != []:
        fail(f"{label}: expected {path} to be empty, got {describe(errors)}")


def as_int(value: Any) -> int | None:
    if isinstance(value, bool):
        return None
    if isinstance(value, int):
        return value
    if isinstance(value, str) and value.isdigit():
        return int(value)
    return None

def positive_counter(container: Any, *keys: str) -> bool:
    value = find_key_recursive(container, *keys)
    numeric = as_int(value)
    return numeric is not None and numeric > 0


def has_string_token(container: Any, *tokens: str) -> bool:
    wanted = [token.lower() for token in tokens]
    if isinstance(container, dict):
        return any(has_string_token(key, *tokens) or has_string_token(value, *tokens) for key, value in container.items())
    if isinstance(container, list):
        return any(has_string_token(value, *tokens) for value in container)
    if isinstance(container, str):
        lowered = container.lower()
        return all(token in lowered for token in wanted)
    return False


def assert_runtime_report(runtime_report: Any) -> None:
    require_status(runtime_report, "runtime report", {"loaded"}, "status")
    errors_are_empty(runtime_report, "runtime report")

    _, loaded_mods = require_path(runtime_report, "runtime report", "loadedMods")
    if not isinstance(loaded_mods, list):
        fail(f"runtime report: loadedMods must be a list, got {describe(loaded_mods)}")

    runebound_mod = next(
        (mod for mod in loaded_mods if isinstance(mod, dict) and mod.get("id") == MOD_ID),
        None,
    )
    if runebound_mod is None:
        fail(f"runtime report: loadedMods does not contain loaded mod id {MOD_ID!r}")
    if runebound_mod.get("status") != "loaded":
        fail(f"runtime report: {MOD_ID} status must be 'loaded', got {describe(runebound_mod.get('status'))}")


def assert_symbol_report(symbol_report: Any) -> None:
    require_status(symbol_report, "symbol report", {"loaded", "passed"}, "status")
    errors_are_empty(symbol_report, "symbol report")

    _, required = require_path(symbol_report, "symbol report", "summary.required")
    _, resolved = require_path(symbol_report, "symbol report", "summary.resolved")
    _, missing = require_path(symbol_report, "symbol report", "summary.missing")
    if as_int(required) is None or as_int(required) <= 0:
        fail(f"symbol report: summary.required must be a positive integer, got {describe(required)}")
    if as_int(missing) != 0:
        fail(f"symbol report: summary.missing must be 0, got {describe(missing)}")
    if as_int(resolved) != as_int(required):
        fail(f"symbol report: summary.resolved must equal summary.required, got {describe(resolved)} vs {describe(required)}")

    path, symbols = require_path(symbol_report, "symbol report", "symbols")
    if not isinstance(symbols, list):
        fail(f"symbol report: {path} must be a list, got {describe(symbols)}")
    unresolved_required = [
        symbol
        for symbol in symbols
        if isinstance(symbol, dict)
        and symbol.get("required") is True
        and symbol.get("status") != "resolved"
    ]
    if unresolved_required:
        fail(f"symbol report: required symbols not resolved: {describe(unresolved_required)}")


def assert_live_install_report(live_report: Any) -> None:
    mod_path, mod_id = first_path(live_report, "modId", "mod.id")
    if mod_id != MOD_ID:
        fail(f"live install report: expected {mod_path or 'modId/mod.id'} == {MOD_ID!r}, got {describe(mod_id)}")
    require_status(live_report, "live install report", {"passed", "loaded", "installed"}, "status")
    errors_are_empty(live_report, "live install report")

    require_int(live_report, "live install report", len(EXPECTED_HOOK_SYMBOLS), "summary.installedHooks", "summary.installedHookCount")
    require_int(live_report, "live install report", len(EXPECTED_HOOK_SYMBOLS), "summary.installedHookCount", "summary.installedHooks")
    require_int(live_report, "live install report", 0, "summary.failedHookCount")
    require_bool(live_report, "live install report", True, "summary.allHooksInstalled")

    fake_probe_path, fake_probe = first_path(live_report, "summary.fakeProviderSelfProbe", "fakeProviderSelfProbe")
    if fake_probe is not MISSING and fake_probe is not False:
        fail(f"live install report: expected {fake_probe_path} false/absent in production, got {describe(fake_probe)}")

    target_symbols = get_path(live_report, "targetSymbols")
    if target_symbols is not MISSING and set(target_symbols) != set(EXPECTED_HOOK_SYMBOLS.values()):
        fail(
            "live install report: targetSymbols must be the expected Runebound hook symbols; "
            f"got {describe(target_symbols)}"
        )

    _, installed_hooks = require_path(live_report, "live install report", "installedHooks")
    if not isinstance(installed_hooks, list):
        fail(f"live install report: installedHooks must be a list, got {describe(installed_hooks)}")
    hooks_by_name = {
        hook.get("name"): hook
        for hook in installed_hooks
        if isinstance(hook, dict) and isinstance(hook.get("name"), str)
    }
    if set(hooks_by_name) != set(EXPECTED_HOOK_SYMBOLS):
        fail(f"live install report: installed hook names must be {sorted(EXPECTED_HOOK_SYMBOLS)}, got {sorted(hooks_by_name)}")
    for hook_name, expected_symbol in EXPECTED_HOOK_SYMBOLS.items():
        hook = hooks_by_name[hook_name]
        if hook.get("symbol") != expected_symbol:
            fail(f"live install report: {hook_name} symbol expected {expected_symbol!r}, got {describe(hook.get('symbol'))}")
        if hook.get("status") != "installed":
            fail(f"live install report: {hook_name} status expected 'installed', got {describe(hook.get('status'))}")
        patch_size = as_int(hook.get("patchSize"))
        if patch_size is None or patch_size < 14:
            fail(f"live install report: {hook_name} patchSize must be >= 14, got {describe(hook.get('patchSize'))}")

    carrier = get_path(live_report, "recognizedCarrier")
    if carrier is not MISSING:
        carrier_id = direct_value(carrier, "catalogId", "carrierId", "recognizedCarrierId", "elixirId")
        if carrier_id != EXPECTED_CARRIER_ID:
            fail(f"live install report: recognizedCarrier id expected {EXPECTED_CARRIER_ID!r}, got {describe(carrier_id)}")
        item_type = direct_value(carrier, "carrierItemType", "itemType", "carrierItem", "type")
        if item_type is not MISSING and item_type != EXPECTED_CARRIER_ITEM_TYPE:
            fail(f"live install report: recognizedCarrier item type expected {EXPECTED_CARRIER_ITEM_TYPE!r}, got {describe(item_type)}")


def contains_item_get_name_evidence(container: Any) -> bool:
    if isinstance(container, dict):
        for key, value in container.items():
            key_norm = normalized_key(str(key))
            if key_norm in {"detoureditemgetname", "itemgetnamedetoured"} and value is True:
                return True
            if contains_item_get_name_evidence(value):
                return True
    elif isinstance(container, list):
        return any(contains_item_get_name_evidence(value) for value in container)
    elif isinstance(container, str):
        return "Item::getName" in container or "_ZNK4Item7getNameEv" in container
    return False


def contains_item_description_evidence(container: Any) -> bool:
    if isinstance(container, dict):
        for key, value in container.items():
            key_norm = normalized_key(str(key))
            if key_norm in {"detoureditemdescription", "itemdescriptiondetoured", "calleddescriptionthroughinstalledhookpath"} and value is True:
                return True
            if contains_item_description_evidence(value):
                return True
    elif isinstance(container, list):
        return any(contains_item_description_evidence(value) for value in container)
    elif isinstance(container, str):
        return "Item::description" in container or "_ZNK4Item11descriptionEv" in container
    return False


def assert_hook_summary(production_report: Any) -> None:
    summary_path, summary = first_path(
        production_report,
        "hookInstallSummary",
        "hookSummary",
        "hooks.summary",
        "validation.hookInstallSummary",
        "summary.hookInstall",
        "summary.hooks",
        "summary",
    )
    if summary is MISSING or not isinstance(summary, dict):
        fail("production validation report: missing hook install summary object")

    installed = direct_value(summary, "installedHooks", "installedHookCount", "installed")
    if as_int(installed) != len(EXPECTED_HOOK_SYMBOLS):
        fail(f"production validation report: {summary_path} installed hook count must be {len(EXPECTED_HOOK_SYMBOLS)}, got {describe(installed)}")

    all_installed = direct_value(summary, "allHooksInstalled")
    if all_installed is not MISSING and all_installed is not True:
        fail(f"production validation report: {summary_path}.allHooksInstalled must be true, got {describe(all_installed)}")

    required_resolved = direct_value(summary, "requiredSymbolsResolved", "allRequiredSymbolsResolved")
    if required_resolved is not MISSING:
        if required_resolved is not True:
            fail(f"production validation report: required symbols must be resolved, got {describe(required_resolved)}")
        return

    required = direct_value(summary, "requiredSymbols", "requiredSymbolCount", "required")
    resolved = direct_value(summary, "resolvedSymbols", "resolvedSymbolCount", "resolved")
    missing = direct_value(summary, "missingSymbols", "missingSymbolCount", "missing")
    if missing is not MISSING and as_int(missing) != 0:
        fail(f"production validation report: missing required symbols must be 0, got {describe(missing)}")
    if required is not MISSING and resolved is not MISSING and as_int(required) != as_int(resolved):
        fail(f"production validation report: resolved symbols must equal required symbols, got {describe(resolved)} vs {describe(required)}")
    if required_resolved is MISSING and required is MISSING and resolved is MISSING and missing is MISSING:
        fail("production validation report: hook summary must state required symbols were resolved")


def display_probe(production_report: Any) -> Any:
    _, probe = first_path(
        production_report,
        "displayProbe",
        "displayCarrierProbe",
        "probes.display",
        "probes.itemGetName",
        "results.displayProbe",
        "results.display",
        "validation.displayProbe",
    )
    if probe is MISSING or not isinstance(probe, dict):
        fail("production validation report: missing display probe object")
    return probe


def assert_display_probe(production_report: Any) -> None:
    probe = display_probe(production_report)
    status = direct_value(probe, "status", "result", "outcome")
    if status is not MISSING and status_text(status) not in PASS_STATUSES:
        fail(f"production validation report: display probe must be passed, got {describe(status)}")

    if not contains_item_get_name_evidence(probe):
        fail("production validation report: display probe must explicitly show the detoured Item::getName path was exercised")
    if not contains_item_description_evidence(probe):
        fail("production validation report: display probe must explicitly show the detoured Item::description path used by chest/inventory messages was exercised")

    display = direct_value(probe, "display", "returnedDisplay", "actualDisplay", "renderedDisplay", "name")
    description = direct_value(probe, "description", "returnedDescription", "actualDescription", "renderedDescription")
    expected_display = direct_value(probe, "expectedDisplay")
    expected_description = direct_value(probe, "expectedDescription")
    if expected_display is MISSING:
        expected_display = EXPECTED_DISPLAY
    if expected_description is MISSING:
        expected_description = EXPECTED_DESCRIPTION
    if display != expected_display:
        fail(f"production validation report: Item::getName display expected {expected_display!r}, got {describe(display)}")
    if description != expected_description:
        fail(f"production validation report: Item::description text expected {expected_description!r}, got {describe(description)}")

    carrier = direct_value(probe, "recognizedCarrier", "carrier")
    carrier_search_root = carrier if isinstance(carrier, dict) else probe
    carrier_id = direct_value(carrier_search_root, "catalogId", "carrierId", "recognizedCarrierId", "elixirId")
    if carrier_id == MISSING and isinstance(carrier_search_root, dict) and carrier_search_root is not probe:
        carrier_id = direct_value(carrier_search_root, "id")
    if carrier_id != EXPECTED_CARRIER_ID:
        fail(f"production validation report: recognized carrier id expected {EXPECTED_CARRIER_ID!r}, got {describe(carrier_id)}")

    item_type = direct_value(carrier_search_root, "carrierItemType", "itemType", "carrierItem", "type")
    if item_type is not MISSING and item_type != EXPECTED_CARRIER_ITEM_TYPE:
        fail(f"production validation report: recognized carrier item type expected {EXPECTED_CARRIER_ITEM_TYPE!r}, got {describe(item_type)}")


def candidate_check_containers(production_report: Any) -> list[Any]:
    containers: list[Any] = []
    for path in (
        "gameplayChecks",
        "gameplay",
        "checks",
        "playerFacingGameplayChecks",
        "results.gameplayChecks",
        "results.gameplay",
        "results.checks",
        "validation.gameplayChecks",
        "validation.checks",
    ):
        value = get_path(production_report, path)
        if value is not MISSING:
            containers.append(value)
    containers.append(production_report)
    return containers


def names_match(value: Any, accepted: set[str]) -> bool:
    if not isinstance(value, str):
        return False
    value_norm = normalized_key(value)
    return value_norm in accepted


def find_check(production_report: Any, accepted_names: Sequence[str]) -> Any:
    accepted = {normalized_key(name) for name in accepted_names}
    for container in candidate_check_containers(production_report):
        if isinstance(container, dict):
            for key, value in container.items():
                if normalized_key(str(key)) in accepted:
                    return value
            for value in container.values():
                if isinstance(value, dict):
                    identity = direct_value(value, "name", "id", "check", "type", "target")
                    if names_match(identity, accepted):
                        return value
        elif isinstance(container, list):
            for value in container:
                if isinstance(value, dict):
                    identity = direct_value(value, "name", "id", "check", "type", "target")
                    if names_match(identity, accepted):
                        return value
    return MISSING


def reason_requires_live_player_state(reason: Any) -> bool:
    if not isinstance(reason, str) or not reason.strip():
        return False
    normalized = reason.lower().replace("-", "_").replace(" ", "_")
    return "requires_live_player_state" in normalized or (
        "live_player_state" in normalized and ("require" in normalized or "unsafe" in normalized or "safe" in normalized)
    )


def evidence_has_safe_live_player_state(evidence: Any) -> bool:
    if not isinstance(evidence, (dict, list)):
        return False
    marker = find_key_recursive(
        evidence,
        "directLivePlayerState",
        "safeLivePlayerState",
        "livePlayerStateObserved",
        "playerStateObserved",
        "livePlayerState",
    )
    if marker is True:
        return True
    source = find_key_recursive(evidence, "source", "kind", "evidenceType", "type")
    if isinstance(source, str) and normalized_key(source) in {
        "liveplayerstate",
        "safeliveplayerstate",
        "directliveplayerstate",
    }:
        return True
    return False


def check_passed_with_evidence(check: Any) -> bool:
    if not isinstance(check, dict):
        return False
    status = direct_value(check, "status", "result", "outcome")
    if status_text(status) not in PASS_STATUSES:
        return False
    evidence = direct_value(check, "evidence", "playerStateEvidence", "livePlayerStateEvidence", "safeLivePlayerState")
    if evidence is MISSING:
        evidence = check
    return evidence_has_safe_live_player_state(evidence)


def assert_gameplay_check(check: Any, label: str) -> bool:
    if check is MISSING:
        fail(f"production validation report: missing {label} gameplay check; expected passed-with-evidence or skipped/not_run with reason")
    if not isinstance(check, dict):
        fail(f"production validation report: {label} gameplay check must be an object, got {describe(check)}")

    status = direct_value(check, "status", "result", "outcome")
    status = status_text(status)
    if status in PASS_STATUSES:
        if not check_passed_with_evidence(check):
            fail(f"production validation report: {label} gameplay check passed without direct safe live player-state evidence")
        return True
    if status in SKIP_STATUSES:
        reason = direct_value(check, "reason", "skipReason", "notRunReason")
        if not reason_requires_live_player_state(reason):
            fail(f"production validation report: {label} gameplay check skipped/not_run without requires_live_player_state reason; got {describe(reason)}")
        return False
    fail(f"production validation report: {label} gameplay check status must be passed or skipped/not_run, got {describe(status)}")

def assert_live_gameplay_gates(production_report: Any) -> dict[str, bool]:
    gates_path, gates = require_path(production_report, "production validation report", "liveGameplayGates")
    if not isinstance(gates, list):
        fail(f"production validation report: {gates_path} must be a list, got {describe(gates)}")

    by_name: dict[str, Any] = {}
    for gate in gates:
        if not isinstance(gate, dict):
            fail(f"production validation report: liveGameplayGates entries must be objects, got {describe(gate)}")
        name = direct_value(gate, "name", "id", "gate", "check")
        if not isinstance(name, str):
            fail(f"production validation report: liveGameplayGates entry missing name/id: {describe(gate)}")
        normalized = normalized_key(name)
        if normalized in by_name:
            fail(f"production validation report: duplicate live gameplay gate {name!r}")
        by_name[normalized] = gate

    results: dict[str, bool] = {}
    for gate_name in sorted(LIVE_GAMEPLAY_GATES):
        gate = by_name.get(normalized_key(gate_name), MISSING)
        results[gate_name] = assert_gameplay_check(gate, f"live gate {gate_name}")
    return results


def assert_process_executable(production_report: Any, expected_barony_executable: Path | None) -> None:
    path, executable = first_path(production_report, "processExecutable", "baronyExecutable", "process.executable")
    if not isinstance(executable, str) or not executable:
        fail("production validation report: missing processExecutable/baronyExecutable proving the real process")
    executable_path = Path(executable)
    if executable_path.name != "barony.x86_64":
        fail(
            "production validation report: production proof must come from a barony.x86_64 process, "
            f"got {path}={executable!r}"
        )
    if expected_barony_executable is not None:
        expected_resolved = expected_barony_executable.expanduser().resolve(strict=False)
        actual_resolved = executable_path.expanduser().resolve(strict=False)
        if actual_resolved != expected_resolved:
            fail(
                "production validation report: process executable does not match configured Barony executable; "
                f"expected {str(expected_resolved)!r}, got {str(actual_resolved)!r}"
            )


def assert_production_report(production_report: Any, live_report: Any, expected_barony_executable: Path | None) -> None:
    schema = direct_value(production_report, "schemaVersion", "schema", "version")
    if not isinstance(schema, str) or not schema:
        fail(f"production validation report: missing non-empty schema identity, got {describe(schema)}")

    identity = direct_value(production_report, "test", "testName", "validation", "id")
    if not isinstance(identity, str) or not all(token in identity.lower() for token in ("runebound", "elixir", "production", "validation")):
        fail(f"production validation report: test identity must name Runebound Elixirs production validation, got {describe(identity)}")
    if "fake-provider" in identity.lower() or "fake_provider" in identity.lower():
        fail(f"production validation report: fake-provider test identity is not production proof: {identity!r}")

    require_status(production_report, "production validation report", {"passed"}, "status")
    require_equal(production_report, "production validation report", MOD_ID, "mod.id", "modId", "package.id", "packageId")

    claim_boundary = direct_value(production_report, "claimBoundary", "proofBoundary")
    if isinstance(claim_boundary, str) and "fake-provider" in claim_boundary.lower():
        fail(f"production validation report: fake-provider claim boundary is not production proof: {claim_boundary!r}")

    require_bool(production_report, "production validation report", False, "fakeProvider", "provider.fake", "summary.fakeProvider")
    assert_process_executable(production_report, expected_barony_executable)
    assert_hook_summary(production_report)
    assert_display_probe(production_report)

    use_check = find_check(production_report, ("use", "useItem", "useGameplay", "consumption", "itemUse"))
    stat_check = find_check(production_report, ("stat", "stats", "statGameplay", "statEffect", "statEffects", "statGetSTR", "statGetDEX"))
    use_passed = assert_gameplay_check(use_check, "use/item")
    stat_passed = assert_gameplay_check(stat_check, "stat")
    live_gate_results = assert_live_gameplay_gates(production_report)
    all_live_gates_passed = all(live_gate_results.values())

    playable_path, playable_claimed = require_path(production_report, "production validation report", "playableBehaviorClaimed")
    if playable_claimed is True and not (use_passed and stat_passed and all_live_gates_passed):
        fail(
            "production validation report: playableBehaviorClaimed true requires use/item, held-HUD item, "
            "stat gameplay, and all four live gameplay gates to pass with direct safe live-state evidence"
        )
    if playable_claimed is not True and playable_claimed is not False:
        fail(f"production validation report: {playable_path} must be boolean, got {describe(playable_claimed)}")

    live_playable_path, live_playable = first_path(live_report, "playableBehaviorClaimed")
    if live_playable is True and not (use_passed and stat_passed and all_live_gates_passed):
        fail(
            f"live install report: {live_playable_path} true requires production use/item, held-HUD item, "
            "stat gameplay checks and all four live gameplay gates with direct safe live-state evidence"
        )


def parse_args(argv: Sequence[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Validate Runebound: Elixirs production proof reports. Production success requires no fake "
            "provider, six live hooks installed, runtime/symbol reports loaded, Item::getName and "
            "Item::description display proof, and safe handling of unexercised held-HUD/gameplay checks."
        )
    )
    parser.add_argument("--runtime-report", required=True, type=Path, help="Path to runtime-load-report.json")
    parser.add_argument("--symbol-report", required=True, type=Path, help="Path to symbol-probe-report.json")
    parser.add_argument("--live-install-report", required=True, type=Path, help="Path to runebound-elixir-live-install-report.json")
    parser.add_argument(
        "--production-validation-report",
        required=True,
        type=Path,
        help="Path to the real Runebound: Elixirs production validation report",
    )
    parser.add_argument(
        "--expected-barony-executable",
        type=Path,
        help="Expected real Steam Barony executable path; production report processExecutable must match it.",
    )
    return parser.parse_args(argv)


def main(argv: Sequence[str] | None = None) -> int:
    args = parse_args(sys.argv[1:] if argv is None else argv)
    try:
        runtime_report = load_json(args.runtime_report, "runtime report")
        symbol_report = load_json(args.symbol_report, "symbol report")
        live_install_report = load_json(args.live_install_report, "live install report")
        production_report = load_json(args.production_validation_report, "production validation report")

        assert_runtime_report(runtime_report)
        assert_symbol_report(symbol_report)
        assert_live_install_report(live_install_report)
        assert_production_report(production_report, live_install_report, args.expected_barony_executable)
    except ValidationError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1

    print(f"Runebound: Elixirs production validation ok: {args.production_validation_report}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
