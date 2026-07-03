#!/usr/bin/env python3
"""BaronyModLoader standalone app skeleton.

This first executable slice intentionally stays narrow: it validates package
metadata, validates engine runtime capability metadata, creates profile-local
app state, and writes a launch-time runtime manifest. It does not execute
Barony or load arbitrary plugin code.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import platform
import re
import shutil
import tempfile
import zipfile
import subprocess
import sys
from dataclasses import dataclass, field
from datetime import datetime, timezone
from pathlib import Path, PurePosixPath
from typing import Any, Iterable

APP_ID = "BaronyModLoader"
APP_VERSION = "0.1.0"
SCHEMA_VERSION = "0.1.0"
RUNTIME_CONTRACT_ID = "bml-runtime-contract"
RUNTIME_CONTRACT_VERSION = "0.1.0"
RUNTIME_CONTRACT = f"{RUNTIME_CONTRACT_ID}@{RUNTIME_CONTRACT_VERSION}"
PACKAGE_MANIFEST_NAME = "bml-package.json"
PACKAGE_INSTALL_DIRECTORIES = ("content", "assets", "native", "migrations")
DETERMINISTIC_ZIP_TIMESTAMP = (1980, 1, 1, 0, 0, 0)
STEAM_BARONY_APP_ID = "371970"
STEAM_BARONY_EXECUTABLE = "barony.x86_64"
STEAM_LIBRARY_RELATIVE_INSTALL = Path("common") / "Barony"
STEAM_MANIFEST_NAME = f"appmanifest_{STEAM_BARONY_APP_ID}.acf"
RUNTIME_STRATEGY_INSTALLED_HOOK = "installed-binary-hook"
SUPPORTED_RUNTIME_STRATEGIES = (RUNTIME_STRATEGY_INSTALLED_HOOK,)

DYNAMIC_LOADER_ENV_PREFIXES = ("LD_", "DYLD_")
DYNAMIC_LOADER_ENV_KEYS = frozenset(
    {
        "LD_PRELOAD",
        "LD_AUDIT",
        "LD_LIBRARY_PATH",
        "LD_ORIGIN_PATH",
        "LD_DEBUG",
        "LD_DEBUG_OUTPUT",
        "LD_DYNAMIC_WEAK",
        "LD_BIND_NOW",
        "LD_BIND_NOT",
        "LD_PROFILE",
        "LD_PROFILE_OUTPUT",
        "LD_SHOW_AUXV",
        "LD_TRACE_LOADED_OBJECTS",
        "LD_USE_LOAD_BIAS",
        "LD_PREFER_MAP_32BIT_EXEC",
        "DYLD_INSERT_LIBRARIES",
        "DYLD_LIBRARY_PATH",
        "DYLD_FRAMEWORK_PATH",
        "DYLD_FALLBACK_LIBRARY_PATH",
        "DYLD_FALLBACK_FRAMEWORK_PATH",
        "DYLD_PRINT_LIBRARIES",
        "DYLD_PRINT_TO_FILE",
        "DYLD_SHARED_CACHE_DIR",
    }
)
BML_LAUNCH_ENV_KEYS = frozenset(
    {
        "BML_PROFILE_DIR",
        "BML_RUNTIME_MANIFEST",
        "BML_RUNTIME_STRATEGY",
        "BML_HOOK_MANIFEST",
        "BML_HOOK_LIBRARY",
    }
)
STEAM_LAUNCH_ENV_KEYS = frozenset({"SteamAppId", "SteamGameId"})

DEFAULT_RUNTIME_REGISTRY_PATH = Path.home() / ".local" / "share" / APP_ID / "runtime-registry.json"

CANONICAL_STASH_CAPABILITIES = (
    "persistent_storage",
    "persistent_inventory",
    "void_chest_binding",
    "placement_lobby",
    "placement_shop",
    "multiplayer_version_metadata",
)

OUTDATED_CAPABILITY_ALIASES = {
    "persistent_named_inventories": ("persistent_inventory",),
    "void_chest_inventory_binding": ("void_chest_binding",),
    "placement_hooks": ("placement_lobby", "placement_shop"),
    "multiplayer_metadata": ("multiplayer_version_metadata",),
}

CORE_STASH_MODULES = {
    "persistentStorage": dict,
    "persistentInventories": list,
    "voidChestBindings": list,
    "placements": list,
    "multiplayer": dict,
}

SEMVER_RE = re.compile(r"^(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)(?:[-+][0-9A-Za-z.-]+)?$")
PACKAGE_ID_RE = re.compile(r"^[A-Za-z0-9][A-Za-z0-9_.-]*$")


@dataclass
class Problem:
    code: str
    severity: str
    message: str
    details: dict[str, Any] = field(default_factory=dict)

    @property
    def is_error(self) -> bool:
        return self.severity in {"error", "fatal"}


@dataclass
class LoadedPackage:
    manifest: dict[str, Any]
    manifest_path: Path
    package_root: Path


@dataclass
class ValidationResult:
    subject: str
    problems: list[Problem] = field(default_factory=list)

    @property
    def ok(self) -> bool:
        return not any(problem.is_error for problem in self.problems)

    def add(self, code: str, severity: str, message: str, **details: Any) -> None:
        self.problems.append(Problem(code, severity, message, details))

    def extend(self, other: "ValidationResult") -> None:
        self.problems.extend(other.problems)


class PackageInstallError(Exception):
    """Fatal package install error with a user-facing message."""


def utc_now() -> str:
    return datetime.now(timezone.utc).replace(microsecond=0).isoformat().replace("+00:00", "Z")


def parse_json_file(path: Path) -> Any:
    with path.open("r", encoding="utf-8") as handle:
        return json.load(handle)


def write_json_file(path: Path, payload: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8") as handle:
        json.dump(payload, handle, indent=2, sort_keys=False)
        handle.write("\n")

def steam_manifest_candidates() -> list[Path]:
    home = Path.home()
    candidates = [
        home / ".local/share/Steam/steamapps" / STEAM_MANIFEST_NAME,
        home / ".steam/steam/steamapps" / STEAM_MANIFEST_NAME,
        home / ".var/app/com.valvesoftware.Steam/data/Steam/steamapps" / STEAM_MANIFEST_NAME,
        home / "snap/steam/common/.local/share/Steam/steamapps" / STEAM_MANIFEST_NAME,
    ]
    seen: set[Path] = set()
    unique: list[Path] = []
    for candidate in candidates:
        resolved = candidate.expanduser()
        key = resolved.resolve() if resolved.exists() else resolved.absolute()
        if key not in seen:
            seen.add(key)
            unique.append(resolved)
    return unique


def parse_steam_acf(path: Path) -> dict[str, str]:
    values: dict[str, str] = {}
    pattern = re.compile(r'^\s*"([^"]+)"\s+"([^"]*)"\s*$')
    with path.open("r", encoding="utf-8", errors="replace") as handle:
        for line in handle:
            match = pattern.match(line)
            if match:
                values[match.group(1)] = match.group(2)
    return values


def steam_manifest_for_install(install_path: Path) -> Path | None:
    parts = install_path.resolve().parts
    if "steamapps" not in parts:
        return None
    steamapps = Path(*parts[: parts.index("steamapps") + 1])
    candidate = steamapps / STEAM_MANIFEST_NAME
    return candidate if candidate.exists() else None


def detect_steam_install(manifest_arg: str | None = None, install_arg: str | None = None) -> tuple[dict[str, Any] | None, ValidationResult]:
    result = ValidationResult("Steam Barony install")
    manifest_path: Path | None = None
    manifest: dict[str, str] = {}

    if manifest_arg:
        manifest_path = Path(manifest_arg).expanduser()
        if not manifest_path.exists():
            result.add("BML_STEAM_MANIFEST_MISSING", "fatal", "Steam appmanifest does not exist.", path=str(manifest_path))
            return None, result
        manifest = parse_steam_acf(manifest_path)
    elif install_arg:
        guessed = steam_manifest_for_install(Path(install_arg).expanduser())
        if guessed:
            manifest_path = guessed
            manifest = parse_steam_acf(guessed)
    else:
        for candidate in steam_manifest_candidates():
            if candidate.exists():
                manifest_path = candidate
                manifest = parse_steam_acf(candidate)
                break

    if install_arg:
        install_path = Path(install_arg).expanduser()
    elif manifest_path:
        install_dir = manifest.get("installdir") or "Barony"
        install_path = manifest_path.parent / "common" / install_dir
    else:
        install_path = Path.home() / ".local/share/Steam/steamapps" / STEAM_LIBRARY_RELATIVE_INSTALL

    install_path = install_path.resolve() if install_path.exists() else install_path.absolute()
    executable = install_path / STEAM_BARONY_EXECUTABLE

    if not install_path.exists():
        result.add("BML_STEAM_INSTALL_MISSING", "fatal", "Steam Barony install directory was not found.", path=str(install_path))
    if not executable.exists():
        result.add("BML_STEAM_EXECUTABLE_MISSING", "fatal", "Steam Barony executable was not found.", path=str(executable))

    if manifest and manifest.get("appid") not in {None, STEAM_BARONY_APP_ID}:
        result.add("BML_STEAM_APP_ID_MISMATCH", "error", "Steam manifest appid is not Barony.", appid=manifest.get("appid"))

    provenance: dict[str, Any] = {}
    if executable.exists() and executable.is_file():
        try:
            provenance = executable_provenance(executable)
        except OSError as exc:
            result.add("BML_STEAM_EXECUTABLE_PROVENANCE_FAILED", "error", f"Could not inspect Steam executable provenance: {exc}", path=str(executable))

    payload = {
        "source": "steam",
        "appId": STEAM_BARONY_APP_ID,
        "name": manifest.get("name", "Barony"),
        "buildId": manifest.get("buildid"),
        "manifestPath": str(manifest_path.resolve()) if manifest_path and manifest_path.exists() else None,
        "installPath": str(install_path),
        "executable": str(executable),
        "executableSha256": provenance.get("sha256"),
        "executableBuildId": provenance.get("buildId"),
        "gameVersionString": provenance.get("gameVersionString"),
        "assetsRoot": str(install_path),
        "compatibility": {
            "stockExecutablePatched": False,
            "requiredRuntimeStrategy": RUNTIME_STRATEGY_INSTALLED_HOOK,
            "requiresBmlRuntimeExecutable": False,
            "requiresBmlHookRuntime": True,
            "runtimeUsesInstalledExecutable": True,
            "runtimeUsesSteamAssets": True,
        },
    }
    return payload, result


def is_semverish(value: Any) -> bool:
    return isinstance(value, str) and bool(SEMVER_RE.match(value))


def semver_key(value: str) -> tuple[int, int, int]:
    match = SEMVER_RE.match(value)
    if not match:
        return (0, 0, 0)
    return (int(match.group(1)), int(match.group(2)), int(match.group(3)))


def version_satisfies(provided: str | None, requested: str | None) -> bool:
    if not requested:
        return True
    if not provided or not is_semverish(provided) or not is_semverish(requested):
        return False
    return semver_key(provided) >= semver_key(requested)


def parse_contract(value: Any) -> tuple[str | None, str | None]:
    if isinstance(value, str):
        if "@" in value:
            contract_id, version = value.split("@", 1)
            return contract_id or None, version or None
        if is_semverish(value):
            return RUNTIME_CONTRACT_ID, value
        return value or None, None
    if isinstance(value, dict):
        return value.get("id"), value.get("version")
    return None, None


def format_problem(problem: Problem) -> str:
    prefix = problem.severity.upper()
    detail_text = ""
    if problem.details:
        rendered = ", ".join(f"{key}={value!r}" for key, value in sorted(problem.details.items()))
        detail_text = f" ({rendered})"
    return f"[{problem.code}] {prefix}: {problem.message}{detail_text}"


def print_report(result: ValidationResult, *, heading: str | None = None) -> None:
    print(heading or result.subject)
    print("=" * len(heading or result.subject))
    if not result.problems:
        print("OK: no validation problems found.")
        return
    for problem in result.problems:
        print(format_problem(problem))
    if result.ok:
        print("OK: validation completed with warnings only.")
    else:
        print("FAILED: validation errors must be fixed before launch.")


def resolve_package_path(path_arg: str) -> tuple[Path | None, Path | None, ValidationResult]:
    result = ValidationResult(f"package path {path_arg}")
    input_path = Path(path_arg).expanduser()
    if input_path.is_dir():
        manifest_path = input_path / PACKAGE_MANIFEST_NAME
        package_root = input_path
    else:
        manifest_path = input_path
        package_root = input_path.parent

    if not manifest_path.exists():
        result.add(
            "BML_PACKAGE_MANIFEST_MISSING",
            "fatal",
            f"Package manifest not found: {manifest_path}",
            expected=PACKAGE_MANIFEST_NAME if input_path.is_dir() else str(manifest_path),
        )
        return None, None, result
    if not manifest_path.is_file():
        result.add("BML_PACKAGE_MANIFEST_NOT_FILE", "fatal", f"Package manifest is not a file: {manifest_path}")
        return None, None, result
    return manifest_path.resolve(), package_root.resolve(), result


def load_package(path_arg: str) -> tuple[LoadedPackage | None, ValidationResult]:
    manifest_path, package_root, result = resolve_package_path(path_arg)
    if manifest_path is None or package_root is None:
        return None, result
    try:
        payload = parse_json_file(manifest_path)
    except json.JSONDecodeError as exc:
        result.add(
            "BML_PACKAGE_MANIFEST_PARSE_FAILED",
            "fatal",
            f"Package manifest is not valid JSON: {manifest_path}",
            line=exc.lineno,
            column=exc.colno,
            error=exc.msg,
        )
        return None, result
    except OSError as exc:
        result.add("BML_PACKAGE_MANIFEST_READ_FAILED", "fatal", f"Could not read package manifest: {exc}")
        return None, result
    if not isinstance(payload, dict):
        result.add("BML_PACKAGE_MANIFEST_INVALID", "fatal", "Package manifest root must be a JSON object.")
        return None, result
    return LoadedPackage(payload, manifest_path, package_root), result


def as_list(value: Any) -> list[Any]:
    return value if isinstance(value, list) else []


def capability_id_and_version(entry: Any) -> tuple[str | None, str | None, bool]:
    if isinstance(entry, str):
        return entry, None, True
    if isinstance(entry, dict):
        return entry.get("id"), entry.get("version"), bool(entry.get("required", True))
    return None, None, True


def package_capability_entries(manifest: dict[str, Any]) -> list[dict[str, Any]]:
    entries: list[dict[str, Any]] = []
    engine = manifest.get("engine")
    capabilities = engine.get("capabilities") if isinstance(engine, dict) else None
    for raw in as_list(capabilities):
        cap_id, version, required = capability_id_and_version(raw)
        entries.append({"id": cap_id, "version": version, "required": required, "source": "engine.capabilities"})
    return entries


def package_required_capabilities(manifest: dict[str, Any]) -> list[dict[str, Any]]:
    return [entry for entry in package_capability_entries(manifest) if entry.get("required", True)]


def validate_required_string(
    result: ValidationResult,
    obj: dict[str, Any],
    key: str,
    code: str,
    *parents: str,
    semver: bool = False,
) -> None:
    dotted = ".".join((*parents, key)) if parents else key
    value = obj.get(key)
    if not isinstance(value, str) or not value.strip():
        result.add(code, "error", f"Missing or invalid required string field: {dotted}", field=dotted)
        return
    if semver and not is_semverish(value):
        result.add("BML_VERSION_INVALID", "error", f"Field must be semver-ish x.y.z: {dotted}", field=dotted, value=value)


def validate_outdated_capability_id(result: ValidationResult, cap_id: str, source: str) -> None:
    replacement = OUTDATED_CAPABILITY_ALIASES.get(cap_id)
    if replacement:
        result.add(
            "BML_PACKAGE_CAPABILITY_OUTDATED",
            "error",
            f"Outdated capability id {cap_id!r}; use canonical v0 id(s): {', '.join(replacement)}.",
            capability=cap_id,
            replacement=list(replacement),
            source=source,
        )


def validate_capability_name_list(result: ValidationResult, values: Any, source: str, *, required_source: bool = False) -> set[str]:
    seen: set[str] = set()
    if values is None:
        if required_source:
            result.add("BML_PACKAGE_CAPABILITIES_MISSING", "error", f"Missing capability list: {source}", source=source)
        return seen
    if not isinstance(values, list):
        result.add("BML_PACKAGE_CAPABILITIES_INVALID", "error", f"Capability list must be an array: {source}", source=source)
        return seen
    for index, raw in enumerate(values):
        cap_id, version, _required = capability_id_and_version(raw)
        item_source = f"{source}[{index}]"
        if not isinstance(cap_id, str) or not cap_id:
            result.add("BML_PACKAGE_CAPABILITY_INVALID", "error", f"Capability entry is missing id: {item_source}", source=item_source)
            continue
        seen.add(cap_id)
        validate_outdated_capability_id(result, cap_id, item_source)
        if isinstance(raw, dict) and "version" in raw and not is_semverish(version):
            result.add("BML_PACKAGE_CAPABILITY_VERSION_INVALID", "error", f"Capability version must be semver-ish: {item_source}", source=item_source, capability=cap_id, version=version)
    return seen


def validate_stash_modules(manifest: dict[str, Any], result: ValidationResult) -> None:
    modules = manifest.get("modules")
    if not isinstance(modules, dict):
        result.add("BML_PACKAGE_MODULES_MISSING", "error", "Stash package must declare a modules object.")
        return

    for module_name, expected_type in CORE_STASH_MODULES.items():
        value = modules.get(module_name)
        if not isinstance(value, expected_type):
            expected = "array" if expected_type is list else "object"
            result.add(
                "BML_PACKAGE_STASH_MODULE_MISSING",
                "error",
                f"Stash package must declare modules.{module_name} as a non-empty {expected}.",
                module=module_name,
            )
            continue
        if expected_type is list and not value:
            result.add(
                "BML_PACKAGE_STASH_MODULE_EMPTY",
                "error",
                f"Stash module list must not be empty: modules.{module_name}.",
                module=module_name,
            )

    storage = modules.get("persistentStorage")
    if isinstance(storage, dict):
        validate_required_string(result, storage, "namespace", "BML_PACKAGE_STASH_STORAGE_INVALID", "modules", "persistentStorage")
        validate_required_string(result, storage, "schemaVersion", "BML_PACKAGE_STASH_STORAGE_INVALID", "modules", "persistentStorage", semver=True)

    inventory_ids: set[str] = set()
    inventories = modules.get("persistentInventories")
    if isinstance(inventories, list):
        for index, inventory in enumerate(inventories):
            if not isinstance(inventory, dict):
                result.add("BML_PACKAGE_STASH_INVENTORY_INVALID", "error", "Persistent inventory entry must be an object.", index=index)
                continue
            inventory_id = inventory.get("id")
            if isinstance(inventory_id, str) and inventory_id:
                inventory_ids.add(inventory_id)
            else:
                result.add("BML_PACKAGE_STASH_INVENTORY_INVALID", "error", "Persistent inventory entry is missing id.", index=index)
            for key in ("storageKey", "scope", "authority", "failurePolicy"):
                if not isinstance(inventory.get(key), str) or not inventory.get(key):
                    result.add("BML_PACKAGE_STASH_INVENTORY_INVALID", "error", f"Persistent inventory missing {key}.", index=index, field=key)

    if "void_chest_inventory" not in inventory_ids:
        result.add(
            "BML_PACKAGE_STASH_INVENTORY_MISSING",
            "error",
            "Stash must declare the core persistent inventory id 'void_chest_inventory'.",
            expected="void_chest_inventory",
        )

    bindings = modules.get("voidChestBindings")
    if isinstance(bindings, list):
        for index, binding in enumerate(bindings):
            if not isinstance(binding, dict):
                result.add("BML_PACKAGE_STASH_BINDING_INVALID", "error", "Void Chest binding entry must be an object.", index=index)
                continue
            inventory = binding.get("inventory")
            if inventory not in inventory_ids:
                result.add(
                    "BML_PACKAGE_STASH_BINDING_INVALID",
                    "error",
                    "Void Chest binding references an undeclared persistent inventory.",
                    index=index,
                    inventory=inventory,
                )

    placement_hooks: set[str] = set()
    placements = modules.get("placements")
    if isinstance(placements, list):
        for index, placement in enumerate(placements):
            if not isinstance(placement, dict):
                result.add("BML_PACKAGE_STASH_PLACEMENT_INVALID", "error", "Placement entry must be an object.", index=index)
                continue
            hook = placement.get("hook")
            if isinstance(hook, str):
                placement_hooks.add(hook)
            inventory = placement.get("inventory")
            if inventory not in inventory_ids:
                result.add(
                    "BML_PACKAGE_STASH_PLACEMENT_INVALID",
                    "error",
                    "Placement references an undeclared persistent inventory.",
                    index=index,
                    inventory=inventory,
                )
    for expected_hook in ("lobby_assist_area", "generated_shop"):
        if expected_hook not in placement_hooks:
            result.add(
                "BML_PACKAGE_STASH_PLACEMENT_MISSING",
                "error",
                f"Stash must declare placement hook {expected_hook!r}.",
                hook=expected_hook,
            )

    multiplayer = modules.get("multiplayer")
    if isinstance(multiplayer, dict):
        sync_inventories = multiplayer.get("syncInventories")
        if not isinstance(sync_inventories, list) or not sync_inventories:
            result.add("BML_PACKAGE_STASH_MULTIPLAYER_INVALID", "error", "Stash multiplayer module must declare syncInventories.")
        else:
            for inventory in sync_inventories:
                if inventory not in inventory_ids:
                    result.add(
                        "BML_PACKAGE_STASH_MULTIPLAYER_INVALID",
                        "error",
                        "Multiplayer syncInventories references an undeclared persistent inventory.",
                        inventory=inventory,
                    )


def validate_package(loaded: LoadedPackage) -> ValidationResult:
    manifest = loaded.manifest
    result = ValidationResult(f"package {loaded.manifest_path}")

    for key in ("formatVersion", "id", "name", "version", "kind"):
        validate_required_string(
            result,
            manifest,
            key,
            "BML_PACKAGE_FIELD_MISSING",
            semver=key in {"formatVersion", "version"},
        )

    package_id = manifest.get("id")
    if isinstance(package_id, str) and package_id and not PACKAGE_ID_RE.match(package_id):
        result.add("BML_PACKAGE_ID_INVALID", "error", "Package id contains unsupported characters.", package=package_id)

    engine = manifest.get("engine")
    if not isinstance(engine, dict):
        result.add("BML_PACKAGE_ENGINE_MISSING", "error", "Package must declare an engine object.")
        engine = {}

    runtime_contract_id, runtime_contract_version = parse_contract(engine.get("runtimeContract"))
    if runtime_contract_id != RUNTIME_CONTRACT_ID or runtime_contract_version != RUNTIME_CONTRACT_VERSION:
        result.add(
            "BML_PACKAGE_RUNTIME_CONTRACT_UNSUPPORTED",
            "error",
            f"Package must target {RUNTIME_CONTRACT} for this app slice.",
            found=engine.get("runtimeContract"),
            expected=RUNTIME_CONTRACT,
        )

    minimum_runtime = engine.get("minimumRuntimeVersion")
    if minimum_runtime is not None and not is_semverish(minimum_runtime):
        result.add(
            "BML_PACKAGE_RUNTIME_VERSION_INVALID",
            "error",
            "engine.minimumRuntimeVersion must be semver-ish x.y.z.",
            value=minimum_runtime,
        )

    capabilities = engine.get("capabilities")
    engine_cap_ids = validate_capability_name_list(result, capabilities, "engine.capabilities", required_source=True)

    required_capabilities = package_required_capabilities(manifest)
    required_ids = {entry["id"] for entry in required_capabilities if isinstance(entry.get("id"), str)}
    for required_id in required_ids:
        if required_id not in CANONICAL_STASH_CAPABILITIES and required_id not in OUTDATED_CAPABILITY_ALIASES:
            result.add(
                "BML_PACKAGE_CAPABILITY_UNKNOWN",
                "error",
                f"Unknown required capability id {required_id!r} for v0 BaronyModLoader.",
                capability=required_id,
                allowed=list(CANONICAL_STASH_CAPABILITIES),
            )

    missing = [capability for capability in CANONICAL_STASH_CAPABILITIES if capability not in required_ids]
    if missing:
        result.add(
            "BML_PACKAGE_CAPABILITY_REQUIRED_MISSING",
            "error",
            "Package is missing required canonical v0 Stash capability ids.",
            missing=missing,
            required=list(CANONICAL_STASH_CAPABILITIES),
            present=sorted(engine_cap_ids),
        )

    native = manifest.get("native")
    if isinstance(native, dict):
        patches = native.get("patches")
        if isinstance(patches, list):
            for patch_index, patch in enumerate(patches):
                if isinstance(patch, dict):
                    validate_capability_name_list(
                        result,
                        patch.get("providesCapabilities"),
                        f"native.patches[{patch_index}].providesCapabilities",
                    )

    runtime_reports = manifest.get("runtimeReports")
    if isinstance(runtime_reports, dict):
        validate_capability_name_list(
            result,
            runtime_reports.get("expectedLoadedCapabilities"),
            "runtimeReports.expectedLoadedCapabilities",
        )

    if manifest.get("id") == "jml.stash" or manifest.get("name") == "Stash":
        validate_stash_modules(manifest, result)

    return result


def load_runtime_info(path_arg: str) -> tuple[dict[str, Any] | None, Path, ValidationResult]:
    path = Path(path_arg).expanduser()
    result = ValidationResult(f"runtime info {path}")
    if not path.exists():
        result.add("BML_RUNTIME_INFO_MISSING", "fatal", f"Runtime info file not found: {path}")
        return None, path, result
    if not path.is_file():
        result.add("BML_RUNTIME_INFO_NOT_FILE", "fatal", f"Runtime info path is not a file: {path}")
        return None, path, result
    try:
        payload = parse_json_file(path)
    except json.JSONDecodeError as exc:
        result.add(
            "BML_RUNTIME_INFO_PARSE_FAILED",
            "fatal",
            f"Runtime info is not valid JSON: {path}",
            line=exc.lineno,
            column=exc.colno,
            error=exc.msg,
        )
        return None, path, result
    except OSError as exc:
        result.add("BML_RUNTIME_INFO_READ_FAILED", "fatal", f"Could not read runtime info: {exc}")
        return None, path, result
    if not isinstance(payload, dict):
        result.add("BML_RUNTIME_INFO_INVALID", "fatal", "Runtime info root must be a JSON object.")
        return None, path, result
    return payload, path.resolve(), result


def runtime_contract_versions(runtime_info: dict[str, Any]) -> set[tuple[str, str]]:
    versions: set[tuple[str, str]] = set()

    def add_contract_value(value: Any, default_contract_id: str | None = None) -> None:
        if isinstance(value, dict) and isinstance(value.get("versions"), list):
            contract_id = value.get("id") if isinstance(value.get("id"), str) else default_contract_id
            for item in value["versions"]:
                item_contract_id, item_version = parse_contract(item)
                if item_version:
                    versions.add((item_contract_id or contract_id or RUNTIME_CONTRACT_ID, item_version))
            return
        contract_id, version = parse_contract(value)
        if contract_id and version:
            versions.add((contract_id, version))

    for key in ("contractVersions", "contracts", "supportedContracts"):
        raw = runtime_info.get(key)
        if raw is None:
            continue
        values = raw if isinstance(raw, list) else [raw]
        for item in values:
            add_contract_value(item)
    add_contract_value(runtime_info.get("contract"))
    return versions


def runtime_capabilities(runtime_info: dict[str, Any]) -> dict[str, str | None]:
    raw = runtime_info.get("capabilities")
    capabilities: dict[str, str | None] = {}
    if isinstance(raw, dict):
        for cap_id, value in raw.items():
            if isinstance(value, dict):
                version = value.get("version")
            elif isinstance(value, str) and is_semverish(value):
                version = value
            else:
                version = None
            capabilities[str(cap_id)] = version
        return capabilities
    if isinstance(raw, list):
        for item in raw:
            cap_id, version, _required = capability_id_and_version(item)
            if isinstance(cap_id, str) and cap_id:
                capabilities[cap_id] = version
    return capabilities


def validate_runtime_info(runtime_info: dict[str, Any], package: LoadedPackage) -> ValidationResult:
    result = ValidationResult("runtime compatibility")

    validate_required_string(result, runtime_info, "runtimeId", "BML_RUNTIME_INFO_FIELD_MISSING")
    validate_required_string(result, runtime_info, "runtimeVersion", "BML_RUNTIME_INFO_FIELD_MISSING", semver=True)

    contract_versions = runtime_contract_versions(runtime_info)
    package_contract_id, package_contract_version = parse_contract(package.manifest.get("engine", {}).get("runtimeContract"))
    if (package_contract_id, package_contract_version) not in contract_versions:
        result.add(
            "BML_RUNTIME_CONTRACT_UNSUPPORTED",
            "fatal",
            "Runtime does not advertise the package runtime contract.",
            required=f"{package_contract_id}@{package_contract_version}",
            supported=[f"{cid}@{version}" for cid, version in sorted(contract_versions)],
        )

    runtime_version = runtime_info.get("runtimeVersion")
    minimum_runtime = package.manifest.get("engine", {}).get("minimumRuntimeVersion")
    if isinstance(minimum_runtime, str) and not version_satisfies(runtime_version, minimum_runtime):
        result.add(
            "BML_RUNTIME_VERSION_UNSUPPORTED",
            "fatal",
            "Runtime version does not satisfy package engine.minimumRuntimeVersion.",
            runtimeVersion=runtime_version,
            minimumRuntimeVersion=minimum_runtime,
        )

    available = runtime_capabilities(runtime_info)
    if not available:
        result.add("BML_RUNTIME_CAPABILITIES_MISSING", "fatal", "Runtime info must declare a capabilities list or object.")

    for cap_id in sorted(available):
        replacement = OUTDATED_CAPABILITY_ALIASES.get(cap_id)
        if replacement:
            result.add(
                "BML_RUNTIME_CAPABILITY_OUTDATED",
                "error",
                f"Runtime advertises outdated capability id {cap_id!r}; use canonical v0 id(s): {', '.join(replacement)}.",
                capability=cap_id,
                replacement=list(replacement),
            )

    for request in package_required_capabilities(package.manifest):
        cap_id = request.get("id")
        requested_version = request.get("version")
        if not isinstance(cap_id, str):
            continue
        if cap_id in OUTDATED_CAPABILITY_ALIASES:
            result.add(
                "BML_RUNTIME_PACKAGE_CAPABILITY_OUTDATED",
                "fatal",
                "Package uses an outdated required capability id, so runtime compatibility cannot be resolved safely.",
                capability=cap_id,
                replacement=list(OUTDATED_CAPABILITY_ALIASES[cap_id]),
            )
            continue
        if cap_id not in available:
            result.add(
                "BML_RUNTIME_CAPABILITY_MISSING",
                "fatal",
                f"Runtime is missing required package capability {cap_id!r}.",
                capability=cap_id,
                requested=requested_version,
                available=sorted(available),
            )
            continue
        provided_version = available.get(cap_id)
        if requested_version and provided_version and not version_satisfies(provided_version, requested_version):
            result.add(
                "BML_RUNTIME_CAPABILITY_VERSION_UNSUPPORTED",
                "fatal",
                f"Runtime capability {cap_id!r} is older than the package request.",
                capability=cap_id,
                requested=requested_version,
                provided=provided_version,
            )
        elif requested_version and provided_version is None:
            result.add(
                "BML_RUNTIME_CAPABILITY_VERSION_MISSING",
                "fatal",
                f"Runtime capability {cap_id!r} must include a version.",
                capability=cap_id,
                requested=requested_version,
            )

    return result

def load_runtime_report(path_arg: str) -> tuple[dict[str, Any] | None, Path, ValidationResult]:
    path = Path(path_arg).expanduser()
    result = ValidationResult(f"runtime load report {path}")
    if not path.exists():
        result.add("BML_RUNTIME_REPORT_MISSING", "fatal", f"Runtime load report file not found: {path}")
        return None, path, result
    if not path.is_file():
        result.add("BML_RUNTIME_REPORT_NOT_FILE", "fatal", f"Runtime load report path is not a file: {path}")
        return None, path, result
    try:
        payload = parse_json_file(path)
    except json.JSONDecodeError as exc:
        result.add(
            "BML_RUNTIME_REPORT_PARSE_FAILED",
            "fatal",
            f"Runtime load report is not valid JSON: {path}",
            line=exc.lineno,
            column=exc.colno,
            error=exc.msg,
        )
        return None, path, result
    except OSError as exc:
        result.add("BML_RUNTIME_REPORT_READ_FAILED", "fatal", f"Could not read runtime load report: {exc}")
        return None, path, result
    if not isinstance(payload, dict):
        result.add("BML_RUNTIME_REPORT_INVALID", "fatal", "Runtime load report root must be a JSON object.")
        return None, path, result
    return payload, path.resolve(), result


def runtime_report_contract(report: dict[str, Any]) -> tuple[str | None, str | None]:
    contract = report.get("contract")
    if not isinstance(contract, dict):
        return None, None
    contract_id = contract.get("id")
    version = contract.get("version")
    return (contract_id if isinstance(contract_id, str) else None, version if isinstance(version, str) else None)


def runtime_report_entry_message(entry: Any) -> str:
    if isinstance(entry, str):
        return entry
    if isinstance(entry, dict):
        code = entry.get("code")
        message = entry.get("message")
        if isinstance(code, str) and isinstance(message, str):
            return f"[{code}] {message}"
        if isinstance(message, str):
            return message
        if isinstance(code, str):
            return code
    return json.dumps(entry, sort_keys=True)


def runtime_report_error_is_fatal(entry: Any) -> bool:
    if not isinstance(entry, dict):
        return True
    severity = entry.get("severity", "fatal")
    if not isinstance(severity, str):
        return True
    return severity.lower() not in {"warning", "info"}


def validate_runtime_report_capabilities(result: ValidationResult, raw: Any, source: str) -> list[str]:
    capabilities: list[str] = []
    if raw is None:
        return capabilities
    if isinstance(raw, dict):
        items = raw.items()
        for cap_id, value in items:
            cap_source = f"{source}.{cap_id}"
            if not isinstance(cap_id, str) or not cap_id:
                result.add("BML_RUNTIME_REPORT_CAPABILITY_INVALID", "error", f"Capability entry is missing id: {cap_source}", source=cap_source)
                continue
            capabilities.append(cap_id)
            if isinstance(value, dict) and "version" in value and not is_semverish(value.get("version")):
                result.add("BML_RUNTIME_REPORT_CAPABILITY_VERSION_INVALID", "error", f"Capability version must be semver-ish: {cap_source}", source=cap_source, capability=cap_id, version=value.get("version"))
    elif isinstance(raw, list):
        for index, item in enumerate(raw):
            cap_id, version, _required = capability_id_and_version(item)
            cap_source = f"{source}[{index}]"
            if not isinstance(cap_id, str) or not cap_id:
                result.add("BML_RUNTIME_REPORT_CAPABILITY_INVALID", "error", f"Capability entry is missing id: {cap_source}", source=cap_source)
                continue
            capabilities.append(cap_id)
            if isinstance(item, dict) and "version" in item and not is_semverish(version):
                result.add("BML_RUNTIME_REPORT_CAPABILITY_VERSION_INVALID", "error", f"Capability version must be semver-ish: {cap_source}", source=cap_source, capability=cap_id, version=version)
    else:
        result.add("BML_RUNTIME_REPORT_CAPABILITIES_INVALID", "error", f"Capabilities must be an array or object when present: {source}", source=source)
        return capabilities

    canonical = set(CANONICAL_STASH_CAPABILITIES)
    for cap_id in capabilities:
        replacement = OUTDATED_CAPABILITY_ALIASES.get(cap_id)
        if replacement:
            result.add(
                "BML_RUNTIME_REPORT_CAPABILITY_OUTDATED",
                "error",
                f"Runtime report uses outdated capability id {cap_id!r}; use canonical v0 id(s): {', '.join(replacement)}.",
                source=source,
                capability=cap_id,
                replacement=list(replacement),
            )
        elif cap_id not in canonical:
            result.add(
                "BML_RUNTIME_REPORT_CAPABILITY_NONCANONICAL",
                "error",
                f"Runtime report capability id is not canonical for the v0 contract: {cap_id!r}.",
                source=source,
                capability=cap_id,
                canonical=sorted(canonical),
            )
    return capabilities


def validate_runtime_report(report: dict[str, Any]) -> ValidationResult:
    result = ValidationResult("runtime load report")

    contract = report.get("contract")
    if not isinstance(contract, dict):
        result.add("BML_RUNTIME_REPORT_CONTRACT_INVALID", "fatal", "Runtime load report must include contract object with id and version.")
    else:
        validate_required_string(result, contract, "id", "BML_RUNTIME_REPORT_CONTRACT_INVALID", "contract")
        validate_required_string(result, contract, "version", "BML_RUNTIME_REPORT_CONTRACT_INVALID", "contract", semver=True)
        contract_id, contract_version = runtime_report_contract(report)
        if contract_id and contract_version and (contract_id, contract_version) != (RUNTIME_CONTRACT_ID, RUNTIME_CONTRACT_VERSION):
            result.add(
                "BML_RUNTIME_REPORT_CONTRACT_UNSUPPORTED",
                "fatal",
                "Runtime load report contract does not match this loader.",
                expected=RUNTIME_CONTRACT,
                actual=f"{contract_id}@{contract_version}",
            )


    runtime = report.get("runtime")
    if not isinstance(runtime, dict):
        result.add("BML_RUNTIME_REPORT_RUNTIME_INVALID", "fatal", "Runtime load report must include runtime object with id, version, and strategy.")
    else:
        validate_required_string(result, runtime, "id", "BML_RUNTIME_REPORT_RUNTIME_INVALID", "runtime")
        validate_required_string(result, runtime, "version", "BML_RUNTIME_REPORT_RUNTIME_INVALID", "runtime", semver=True)
        validate_required_string(result, runtime, "strategy", "BML_RUNTIME_REPORT_RUNTIME_INVALID", "runtime")
    status = report.get("status")
    if status not in {"loaded", "failed"}:
        result.add("BML_RUNTIME_REPORT_STATUS_INVALID", "fatal", "Runtime load report status must be loaded or failed.", status=status)
    elif status == "failed":
        result.add("BML_RUNTIME_REPORT_FAILED", "fatal", "Runtime reported failed load status.")

    errors = report.get("errors")
    if not isinstance(errors, list):
        result.add("BML_RUNTIME_REPORT_ERRORS_INVALID", "fatal", "Runtime load report must include errors array.")
    else:
        for index, entry in enumerate(errors):
            if runtime_report_error_is_fatal(entry):
                result.add("BML_RUNTIME_REPORT_FATAL_ERROR", "fatal", runtime_report_entry_message(entry), source=f"errors[{index}]")

    warnings = report.get("warnings")
    if not isinstance(warnings, list):
        result.add("BML_RUNTIME_REPORT_WARNINGS_INVALID", "fatal", "Runtime load report must include warnings array.")

    loaded_mods = report.get("loadedMods")
    if not isinstance(loaded_mods, list):
        result.add("BML_RUNTIME_REPORT_LOADED_MODS_INVALID", "fatal", "Runtime load report must include loadedMods array.")
    else:
        for index, mod in enumerate(loaded_mods):
            if not isinstance(mod, dict):
                result.add("BML_RUNTIME_REPORT_LOADED_MOD_INVALID", "fatal", f"loadedMods[{index}] must be an object.", source=f"loadedMods[{index}]")
                continue
            if "capabilities" in mod:
                validate_runtime_report_capabilities(result, mod.get("capabilities"), f"loadedMods[{index}].capabilities")

    return result


def format_runtime_report_mod(mod: Any) -> str:
    if not isinstance(mod, dict):
        return "<invalid mod entry>"
    mod_id = mod.get("id") if isinstance(mod.get("id"), str) else "<unknown>"
    version = mod.get("version") if isinstance(mod.get("version"), str) else None
    status = mod.get("status") if isinstance(mod.get("status"), str) else None
    label = f"{mod_id}@{version}" if version else mod_id
    suffixes = []
    if status:
        suffixes.append(f"status={status}")
    caps = validate_runtime_report_capabilities(ValidationResult("runtime report summary"), mod.get("capabilities"), "capabilities")
    if caps:
        suffixes.append("capabilities=" + ", ".join(caps))
    return f"{label} ({'; '.join(suffixes)})" if suffixes else label


def print_runtime_report_summary(path: Path, report: dict[str, Any] | None, result: ValidationResult) -> None:
    print(f"Runtime load report: {path}")
    if report is None:
        for problem in result.problems:
            print(f"  {format_problem(problem)}")
        print("FAILED: runtime load report could not be read.")
        return

    contract_id, contract_version = runtime_report_contract(report)
    runtime = report.get("runtime")
    runtime_label = "<missing>"
    if isinstance(runtime, dict):
        runtime_id = runtime.get("id") if isinstance(runtime.get("id"), str) else "<missing>"
        runtime_version = runtime.get("version") if isinstance(runtime.get("version"), str) else "<missing>"
        runtime_strategy = runtime.get("strategy") if isinstance(runtime.get("strategy"), str) else "<missing>"
        runtime_label = f"{runtime_id}@{runtime_version} ({runtime_strategy})"
    print(f"  Contract: {contract_id or '<missing>'}@{contract_version or '<missing>'}")
    print(f"  Runtime: {runtime_label}")
    print(f"  Status: {report.get('status', '<missing>')}")

    loaded_mods = report.get("loadedMods")
    if isinstance(loaded_mods, list):
        print(f"  Loaded mods: {len(loaded_mods)}")
        for mod in loaded_mods:
            print(f"    - {format_runtime_report_mod(mod)}")
    else:
        print("  Loaded mods: <invalid>")

    warnings = report.get("warnings")
    if isinstance(warnings, list):
        print(f"  Warnings: {len(warnings)}")
        for entry in warnings:
            print(f"    - {runtime_report_entry_message(entry)}")
    else:
        print("  Warnings: <invalid>")

    errors = report.get("errors")
    if isinstance(errors, list):
        print(f"  Errors: {len(errors)}")
        for entry in errors:
            print(f"    - {runtime_report_entry_message(entry)}")
    else:
        print("  Errors: <invalid>")

    if result.problems:
        print("Problems:")
        for problem in result.problems:
            print(f"  {format_problem(problem)}")
    if result.ok:
        print("OK: runtime loaded without fatal errors.")
    else:
        print("FAILED: runtime failed, reported fatal errors, or has invalid shape.")


def command_runtime_report(args: argparse.Namespace) -> int:
    report, report_path, load_result = load_runtime_report(args.runtime_load_report)
    combined = ValidationResult("runtime load report")
    combined.extend(load_result)
    if report is not None:
        combined.extend(validate_runtime_report(report))
    print_runtime_report_summary(report_path, report, combined)
    return 0 if combined.ok else 1


def command_runtime_info(args: argparse.Namespace) -> int:
    runtime_info, runtime_path, result = load_runtime_info(args.runtime_info)
    if runtime_info is None:
        print_report(result, heading=f"Runtime info: {runtime_path}")
        return 1

    print(f"Runtime info: {runtime_path}")
    print(f"  Runtime: {runtime_info.get('runtimeId', '<missing>')}@{runtime_info.get('runtimeVersion', '<missing>')}")
    contracts = sorted(f"{contract_id}@{version}" for contract_id, version in runtime_contract_versions(runtime_info))
    print("  Contracts: " + (", ".join(contracts) if contracts else "<none>"))
    capabilities = runtime_capabilities(runtime_info)
    print(f"  Capabilities: {len(capabilities)}")
    for cap_id, version in sorted(capabilities.items()):
        print(f"    - {cap_id}" + (f"@{version}" if version else ""))
    if result.problems:
        print("Problems:")
        for problem in result.problems:
            print(f"  {format_problem(problem)}")
    return 0 if result.ok else 1

def runtime_registry_path(path_arg: str | None) -> Path:
    if path_arg:
        return Path(path_arg).expanduser().resolve()
    return DEFAULT_RUNTIME_REGISTRY_PATH


def file_sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()

def executable_build_id(path: Path) -> str | None:
    if not sys.platform.startswith("linux"):
        return None
    try:
        completed = subprocess.run(
            ["readelf", "-n", str(path)],
            check=False,
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
            text=True,
            timeout=5,
        )
    except (OSError, subprocess.TimeoutExpired):
        return None
    if completed.returncode != 0:
        return None
    for line in completed.stdout.splitlines():
        marker = "Build ID:"
        if marker in line:
            return line.split(marker, 1)[1].strip() or None
    return None


def detect_game_version_string(path: Path) -> str | None:
    version_pattern = re.compile(rb"v[0-9]+\.[0-9]+\.[0-9]+(?:[-+][0-9A-Za-z.-]+)?")
    carry = b""
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            match = version_pattern.search(carry + chunk)
            if match:
                return match.group(0).decode("ascii", errors="replace")
            carry = chunk[-64:]
    return None


def executable_provenance(path: Path) -> dict[str, str | None]:
    return {
        "sha256": file_sha256(path),
        "buildId": executable_build_id(path),
        "gameVersionString": detect_game_version_string(path),
    }


def current_platform_id() -> str:
    machine = platform.machine() or "unknown"
    if sys.platform.startswith("linux"):
        os_name = "linux"
    elif sys.platform == "darwin":
        os_name = "macos"
    elif sys.platform.startswith(("win32", "cygwin", "msys")):
        os_name = "windows"
    else:
        os_name = sys.platform or "unknown"
    return f"{os_name}-{machine}"


def runtime_info_platform_ids(runtime_info: dict[str, Any]) -> list[str]:
    raw_platforms = runtime_info.get("platforms")
    if not isinstance(raw_platforms, list):
        return []
    if not raw_platforms:
        return []
    platform_ids: list[str] = []
    for entry in raw_platforms:
        platform_id = entry.get("platform") if isinstance(entry, dict) else None
        if isinstance(platform_id, str) and platform_id:
            platform_ids.append(platform_id)
    return platform_ids


def validate_registered_runtime_host_platform(registered_platform: Any) -> ValidationResult:
    result = ValidationResult("registered runtime platform")
    current_platform = current_platform_id()
    if registered_platform != current_platform:
        result.add(
            "BML_REGISTERED_RUNTIME_PLATFORM_MISMATCH",
            "fatal",
            "Registered runtime platform does not match this host platform.",
            registeredPlatform=registered_platform,
            currentPlatform=current_platform,
        )
    return result


def validate_runtime_info_registered_platform(runtime_info: dict[str, Any], registered_platform: Any) -> ValidationResult:
    result = ValidationResult("registered runtime info platform")
    supported_platforms = runtime_info_platform_ids(runtime_info)
    if registered_platform not in supported_platforms:
        result.add(
            "BML_REGISTERED_RUNTIME_INFO_PLATFORM_UNSUPPORTED",
            "fatal",
            "Runtime info does not advertise the registered runtime platform.",
            registeredPlatform=registered_platform,
            supportedPlatforms=supported_platforms,
        )
    return result


def validate_runtime_info_metadata(runtime_info: dict[str, Any]) -> ValidationResult:
    result = ValidationResult("runtime info metadata")
    validate_required_string(result, runtime_info, "runtimeId", "BML_RUNTIME_INFO_FIELD_MISSING")
    validate_required_string(result, runtime_info, "runtimeVersion", "BML_RUNTIME_INFO_FIELD_MISSING", semver=True)

    contract_versions = runtime_contract_versions(runtime_info)
    if (RUNTIME_CONTRACT_ID, RUNTIME_CONTRACT_VERSION) not in contract_versions:
        result.add(
            "BML_RUNTIME_CONTRACT_UNSUPPORTED",
            "fatal",
            "Runtime does not advertise the active BaronyModLoader runtime contract.",
            required=RUNTIME_CONTRACT,
            supported=[f"{cid}@{version}" for cid, version in sorted(contract_versions)],
        )

    available = runtime_capabilities(runtime_info)
    if not available:
        result.add("BML_RUNTIME_CAPABILITIES_MISSING", "fatal", "Runtime info must declare a capabilities list or object.")
    for cap_id in sorted(available):
        replacement = OUTDATED_CAPABILITY_ALIASES.get(cap_id)
        if replacement:
            result.add(
                "BML_RUNTIME_CAPABILITY_OUTDATED",
                "error",
                f"Runtime advertises outdated capability id {cap_id!r}; use canonical v0 id(s): {', '.join(replacement)}.",
                capability=cap_id,
                replacement=list(replacement),
            )
    return result


def load_runtime_registry(path: Path, *, missing_ok: bool) -> tuple[dict[str, Any], ValidationResult]:
    result = ValidationResult(f"runtime registry {path}")
    if not path.exists():
        if missing_ok:
            return {
                "schemaVersion": SCHEMA_VERSION,
                "app": {"id": APP_ID, "version": APP_VERSION},
                "createdAt": utc_now(),
                "updatedAt": utc_now(),
                "runtimes": [],
            }, result
        result.add("BML_RUNTIME_REGISTRY_MISSING", "fatal", f"Runtime registry not found: {path}", hint="Run runtime register first.")
        return {}, result
    if not path.is_file():
        result.add("BML_RUNTIME_REGISTRY_NOT_FILE", "fatal", f"Runtime registry path is not a file: {path}")
        return {}, result
    try:
        payload = parse_json_file(path)
    except json.JSONDecodeError as exc:
        result.add("BML_RUNTIME_REGISTRY_PARSE_FAILED", "fatal", f"Runtime registry is not valid JSON: {path}", line=exc.lineno, column=exc.colno, error=exc.msg)
        return {}, result
    except OSError as exc:
        result.add("BML_RUNTIME_REGISTRY_READ_FAILED", "fatal", f"Could not read runtime registry: {exc}")
        return {}, result
    if not isinstance(payload, dict):
        result.add("BML_RUNTIME_REGISTRY_INVALID", "fatal", "Runtime registry root must be a JSON object.")
        return {}, result
    if not isinstance(payload.get("runtimes"), list):
        result.add("BML_RUNTIME_REGISTRY_RUNTIMES_INVALID", "fatal", "Runtime registry must include a runtimes array.")
    return payload, result


def runtime_registration_id(runtime_info: dict[str, Any], steam_build_id: str | None, platform_id: str, strategy: str = RUNTIME_STRATEGY_INSTALLED_HOOK) -> str:
    base = str(runtime_info.get("runtimeId") or "bml-runtime")
    suffix = f"steam-{STEAM_BARONY_APP_ID}-{steam_build_id}" if steam_build_id else "manual"
    return re.sub(r"[^A-Za-z0-9_.-]+", "-", f"{base}-{strategy}-{suffix}-{platform_id}").strip("-")


def runtime_registry_capability_entries(runtime_info: dict[str, Any]) -> list[dict[str, Any]]:
    return [
        {"id": cap_id, "version": version}
        for cap_id, version in sorted(runtime_capabilities(runtime_info).items())
    ]


def command_runtime_register(args: argparse.Namespace) -> int:
    registry_path = runtime_registry_path(args.registry)
    combined = ValidationResult("runtime registration")
    strategy = args.runtime_strategy

    if strategy not in SUPPORTED_RUNTIME_STRATEGIES:
        combined.add("BML_RUNTIME_STRATEGY_UNSUPPORTED", "fatal", f"Unsupported runtime strategy: {strategy}")

    platform_id = args.platform or current_platform_id()
    combined.extend(validate_registered_runtime_host_platform(platform_id))

    steam_executable_arg = args.steam_executable or args.executable
    steam_executable = Path(steam_executable_arg).expanduser().resolve() if steam_executable_arg else None
    hook_library = Path(args.hook_library).expanduser().resolve() if args.hook_library else None
    hook_manifest = Path(args.hook_manifest).expanduser().resolve() if args.hook_manifest else None

    if steam_executable is None:
        combined.add("BML_RUNTIME_STEAM_EXECUTABLE_MISSING", "fatal", "Installed executable path is required for installed-binary-hook registration.", hint="Pass --steam-executable /path/to/barony.x86_64")
    elif not steam_executable.exists():
        combined.add("BML_RUNTIME_STEAM_EXECUTABLE_NOT_FOUND", "fatal", f"Installed executable not found: {steam_executable}")
    elif not steam_executable.is_file():
        combined.add("BML_RUNTIME_STEAM_EXECUTABLE_NOT_FILE", "fatal", f"Installed executable path is not a file: {steam_executable}")

    if hook_library is None:
        combined.add("BML_RUNTIME_HOOK_LIBRARY_MISSING", "fatal", "Hook library path is required for installed-binary-hook registration.", hint="Pass --hook-library native/barony-modloader-hook/build/libbarony_bml.so")
    elif not hook_library.exists():
        combined.add("BML_RUNTIME_HOOK_LIBRARY_NOT_FOUND", "fatal", f"Hook library not found: {hook_library}")
    elif not hook_library.is_file():
        combined.add("BML_RUNTIME_HOOK_LIBRARY_NOT_FILE", "fatal", f"Hook library path is not a file: {hook_library}")

    if hook_manifest is None:
        combined.add("BML_RUNTIME_HOOK_MANIFEST_MISSING", "fatal", "Hook manifest path is required for installed-binary-hook registration.", hint="Pass --hook-manifest native/barony-modloader-hook/manifests/<build>.json")
    elif not hook_manifest.exists():
        combined.add("BML_RUNTIME_HOOK_MANIFEST_NOT_FOUND", "fatal", f"Hook manifest not found: {hook_manifest}")
    elif not hook_manifest.is_file():
        combined.add("BML_RUNTIME_HOOK_MANIFEST_NOT_FILE", "fatal", f"Hook manifest path is not a file: {hook_manifest}")

    runtime_info, runtime_info_path, runtime_load_result = load_runtime_info(args.runtime_info)
    combined.extend(runtime_load_result)
    if runtime_info is not None:
        combined.extend(validate_runtime_info_metadata(runtime_info))
        combined.extend(validate_runtime_info_registered_platform(runtime_info, platform_id))
    registry, registry_load_result = load_runtime_registry(registry_path, missing_ok=True)
    combined.extend(registry_load_result)

    if not combined.ok:
        print_report(combined, heading="Runtime registration")
        return 1

    assert runtime_info is not None
    assert steam_executable is not None
    assert hook_library is not None
    assert hook_manifest is not None
    runtime_id = args.id or runtime_registration_id(runtime_info, args.steam_build_id, platform_id, strategy)
    if not PACKAGE_ID_RE.match(runtime_id):
        combined.add("BML_RUNTIME_REGISTRY_ID_INVALID", "fatal", "Runtime id must use letters, numbers, dot, underscore, or dash.", runtimeId=runtime_id)
        print_report(combined, heading="Runtime registration")
        return 1

    steam_provenance = executable_provenance(steam_executable)
    now = utc_now()
    registration = {
        "id": runtime_id,
        "runtimeStrategy": strategy,
        "storefront": "steam",
        "steamAppId": args.steam_app_id,
        "steamBuildId": args.steam_build_id,
        "platform": platform_id,
        "runtimeVersion": runtime_info.get("runtimeVersion"),
        "runtimeContract": RUNTIME_CONTRACT,
        "executable": str(steam_executable),
        "sha256": steam_provenance["sha256"],
        "steamExecutable": str(steam_executable),
        "steamExecutableSha256": steam_provenance["sha256"],
        "steamExecutableBuildId": args.steam_executable_build_id or steam_provenance.get("buildId"),
        "gameVersionString": args.game_version_string or steam_provenance.get("gameVersionString"),
        "hookLibrary": str(hook_library),
        "hookLibrarySha256": file_sha256(hook_library),
        "hookManifest": str(hook_manifest),
        "hookManifestSha256": file_sha256(hook_manifest),
        "runtimeInfo": str(runtime_info_path),
        "capabilities": runtime_registry_capability_entries(runtime_info),
        "registeredAt": now,
    }

    existing = [entry for entry in registry.get("runtimes", []) if not (isinstance(entry, dict) and entry.get("id") == runtime_id)]
    registry.update(
        {
            "schemaVersion": SCHEMA_VERSION,
            "app": {"id": APP_ID, "version": APP_VERSION},
            "updatedAt": now,
            "runtimes": [*existing, registration],
        }
    )
    registry.setdefault("createdAt", now)
    write_json_file(registry_path, registry)
    print(json.dumps({"status": "registered", "registry": str(registry_path), "runtime": registration}, indent=2))
    return 0


def command_runtime_list(args: argparse.Namespace) -> int:
    registry_path = runtime_registry_path(args.registry)
    registry, result = load_runtime_registry(registry_path, missing_ok=False)
    if not result.ok:
        print_report(result, heading="Runtime registry")
        return 1
    runtimes = [entry for entry in registry.get("runtimes", []) if isinstance(entry, dict)]
    print(json.dumps({"registry": str(registry_path), "runtimeCount": len(runtimes), "runtimes": runtimes}, indent=2))
    return 0


def command_runtime_inspect(args: argparse.Namespace) -> int:
    registry_path = runtime_registry_path(args.registry)
    registry, result = load_runtime_registry(registry_path, missing_ok=False)
    if not result.ok:
        print_report(result, heading="Runtime registry")
        return 1
    for runtime in registry.get("runtimes", []):
        if isinstance(runtime, dict) and runtime.get("id") == args.runtime_id:
            print(json.dumps({"registry": str(registry_path), "runtime": runtime}, indent=2))
            return 0
    result.add("BML_RUNTIME_REGISTRY_ID_MISSING", "fatal", f"Runtime id not found in registry: {args.runtime_id}")
    print_report(result, heading="Runtime registry")
    return 1



def safe_archive_name(name: str) -> str | None:
    if not name or name.startswith(("/", "\\")) or "\\" in name:
        return None
    normalized = PurePosixPath(name)
    if normalized.is_absolute() or any(part in {"", ".", ".."} for part in normalized.parts):
        return None
    return normalized.as_posix()


def archive_name_is_installable(name: str) -> bool:
    return name == PACKAGE_MANIFEST_NAME or any(name.startswith(f"{directory}/") for directory in PACKAGE_INSTALL_DIRECTORIES)


def package_relative_files(package_root: Path, out_path: Path | None = None) -> list[tuple[str, Path]]:
    entries: list[tuple[str, Path]] = []
    out_resolved = out_path.resolve() if out_path is not None and out_path.exists() else None
    for path in package_root.rglob("*"):
        if not path.is_file() or path.is_symlink():
            continue
        resolved = path.resolve()
        if out_resolved is not None and resolved == out_resolved:
            continue
        relative_name = path.relative_to(package_root).as_posix()
        entries.append((relative_name, path))
    entries.sort(key=lambda item: item[0])
    return entries


def write_deterministic_package_archive(package_root: Path, out_path: Path) -> int:
    entries = package_relative_files(package_root, out_path)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(out_path, "w", compression=zipfile.ZIP_DEFLATED) as archive:
        for relative_name, source_path in entries:
            info = zipfile.ZipInfo(relative_name, DETERMINISTIC_ZIP_TIMESTAMP)
            info.compress_type = zipfile.ZIP_DEFLATED
            info.external_attr = 0o100644 << 16
            archive.writestr(info, source_path.read_bytes())
    return len(entries)


def load_package_archive(path_arg: str) -> tuple[LoadedPackage | None, Path, ValidationResult]:
    path = Path(path_arg).expanduser()
    result = ValidationResult(f"package archive {path}")
    if not path.exists():
        result.add("BML_PACKAGE_ARCHIVE_MISSING", "fatal", f"Package archive not found: {path}")
        return None, path, result
    if not path.is_file():
        result.add("BML_PACKAGE_ARCHIVE_NOT_FILE", "fatal", f"Package archive path is not a file: {path}")
        return None, path, result
    if not zipfile.is_zipfile(path):
        result.add("BML_PACKAGE_ARCHIVE_INVALID", "fatal", f"Package archive is not a zip-compatible .bmlpkg file: {path}")
        return None, path, result

    try:
        with zipfile.ZipFile(path) as archive:
            for info in archive.infolist():
                if safe_archive_name(info.filename) is None:
                    result.add("BML_PACKAGE_ARCHIVE_PATH_UNSAFE", "fatal", "Package archive contains an unsafe member path.", member=info.filename)
            if not result.ok:
                return None, path, result
            try:
                raw_manifest = archive.read(PACKAGE_MANIFEST_NAME)
            except KeyError:
                result.add(
                    "BML_PACKAGE_ARCHIVE_MANIFEST_MISSING",
                    "fatal",
                    f"Package archive must contain {PACKAGE_MANIFEST_NAME} at its root.",
                    expected=PACKAGE_MANIFEST_NAME,
                )
                return None, path, result
    except zipfile.BadZipFile as exc:
        result.add("BML_PACKAGE_ARCHIVE_INVALID", "fatal", f"Package archive could not be opened: {exc}")
        return None, path, result
    except OSError as exc:
        result.add("BML_PACKAGE_ARCHIVE_READ_FAILED", "fatal", f"Could not read package archive: {exc}")
        return None, path, result

    try:
        payload = json.loads(raw_manifest.decode("utf-8"))
    except UnicodeDecodeError as exc:
        result.add("BML_PACKAGE_MANIFEST_PARSE_FAILED", "fatal", "Package archive manifest is not UTF-8 JSON.", error=str(exc))
        return None, path, result
    except json.JSONDecodeError as exc:
        result.add(
            "BML_PACKAGE_MANIFEST_PARSE_FAILED",
            "fatal",
            "Package archive manifest is not valid JSON.",
            line=exc.lineno,
            column=exc.colno,
            error=exc.msg,
        )
        return None, path, result
    if not isinstance(payload, dict):
        result.add("BML_PACKAGE_MANIFEST_INVALID", "fatal", "Package archive manifest root must be a JSON object.")
        return None, path, result
    return LoadedPackage(payload, path.resolve(), path.resolve().parent), path.resolve(), result


def package_install_target(store_dir: Path, manifest: dict[str, Any]) -> Path:
    package_id = str(manifest.get("id"))
    package_version = str(manifest.get("version"))
    return store_dir / package_id / package_version


def replace_install_target(temp_target: Path, final_target: Path) -> None:
    if final_target.exists():
        if final_target.is_dir():
            shutil.rmtree(final_target)
        else:
            final_target.unlink()
    temp_target.rename(final_target)



def resolved_path_is_within(path: Path, root: Path) -> bool:
    try:
        path.relative_to(root)
    except ValueError:
        return False
    return True


def reject_package_symlink(path: Path, package_root: Path) -> None:
    try:
        relative_path = path.relative_to(package_root).as_posix()
    except ValueError:
        relative_path = str(path)
    raise PackageInstallError(f"Refusing to install package symlink: {relative_path}. Package installs cannot contain symlinks.")


def verify_install_source_path(path: Path, package_root: Path) -> Path:
    if path.is_symlink():
        reject_package_symlink(path, package_root)
    resolved = path.resolve(strict=True)
    package_root_resolved = package_root.resolve(strict=True)
    if not resolved_path_is_within(resolved, package_root_resolved):
        raise PackageInstallError(f"Refusing to install package path outside package root: {path}")
    return resolved


def copy_installed_tree_without_symlinks(package_root: Path, source: Path, target: Path, copied: list[str]) -> None:
    source_resolved = verify_install_source_path(source, package_root)
    for dirpath, dirnames, filenames in os.walk(source):
        dirnames.sort()
        current = Path(dirpath)
        current_resolved = verify_install_source_path(current, package_root)
        if not resolved_path_is_within(current_resolved, source_resolved):
            raise PackageInstallError(f"Refusing to install directory outside package root: {current}")
        for dirname in list(dirnames):
            child = current / dirname
            verify_install_source_path(child, package_root)
        for filename in sorted(filenames):
            source_file = current / filename
            verify_install_source_path(source_file, package_root)
            if not source_file.is_file():
                continue
            relative_name = source_file.relative_to(package_root).as_posix()
            destination = target / relative_name
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source_file, destination)
            copied.append(relative_name)

def copy_installed_package_files(package: LoadedPackage, target: Path) -> list[str]:
    copied = [PACKAGE_MANIFEST_NAME]
    target.mkdir(parents=True, exist_ok=True)
    verify_install_source_path(package.manifest_path, package.package_root)
    shutil.copy2(package.manifest_path, target / PACKAGE_MANIFEST_NAME)
    for directory in PACKAGE_INSTALL_DIRECTORIES:
        source = package.package_root / directory
        if source.is_symlink():
            reject_package_symlink(source, package.package_root)
        if not source.exists():
            continue
        if not source.is_dir():
            continue
        copy_installed_tree_without_symlinks(package.package_root, source, target, copied)
    return copied


def extract_installed_package_files(archive_path: Path, target: Path) -> list[str]:
    copied: list[str] = []
    target.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(archive_path) as archive:
        installable = []
        for info in archive.infolist():
            safe_name = safe_archive_name(info.filename)
            if safe_name is None or info.is_dir() or not archive_name_is_installable(safe_name):
                continue
            installable.append((safe_name, info))
        for safe_name, info in sorted(installable, key=lambda item: item[0]):
            destination = target / safe_name
            destination.parent.mkdir(parents=True, exist_ok=True)
            with archive.open(info) as source, destination.open("wb") as output:
                shutil.copyfileobj(source, output)
            copied.append(safe_name)
    return copied


def install_loaded_package(package: LoadedPackage, store_dir: Path, *, archive_path: Path | None = None) -> tuple[Path, list[str]]:
    final_target = package_install_target(store_dir, package.manifest)
    final_target.parent.mkdir(parents=True, exist_ok=True)
    temp_target = Path(tempfile.mkdtemp(prefix=f".{final_target.name}.", dir=final_target.parent))
    try:
        if archive_path is None:
            copied = copy_installed_package_files(package, temp_target)
        else:
            copied = extract_installed_package_files(archive_path, temp_target)
        replace_install_target(temp_target, final_target)
        return final_target, copied
    except Exception:
        if temp_target.exists():
            shutil.rmtree(temp_target)
        raise


def preferred_package_path(package: LoadedPackage) -> Path:
    if package.manifest_path.name == PACKAGE_MANIFEST_NAME:
        return package.package_root
    return package.manifest_path


def active_mods_json_path(profile_dir: Path) -> Path:
    return bml_profile_root(profile_dir) / "active-mods.json"


def profile_id(profile: dict[str, Any]) -> str | None:
    value = profile.get("profile", {}).get("id")
    return value if isinstance(value, str) else None


def profile_active_mods(profile: dict[str, Any]) -> list[dict[str, Any]]:
    mods = profile.get("activeMods")
    if not isinstance(mods, list):
        return []
    return [dict(mod) for mod in mods if isinstance(mod, dict)]


def load_active_mods_file(profile_dir: Path) -> list[dict[str, Any]]:
    path = active_mods_json_path(profile_dir)
    if not path.is_file():
        return []
    try:
        payload = parse_json_file(path)
    except (json.JSONDecodeError, OSError):
        return []
    mods = payload.get("mods") if isinstance(payload, dict) else None
    if not isinstance(mods, list):
        return []
    return [dict(mod) for mod in mods if isinstance(mod, dict)]


def profile_has_active_mod_state(profile: dict[str, Any], profile_dir: Path) -> bool:
    return bool(profile_active_mods(profile)) or active_mods_json_path(profile_dir).is_file()


def profile_authoritative_mods(profile: dict[str, Any], profile_dir: Path) -> list[dict[str, Any]]:
    profile_mods = profile_active_mods(profile)
    return profile_mods if profile_mods else load_active_mods_file(profile_dir)


def validate_profile_package_enabled(profile: dict[str, Any], profile_dir: Path, package: LoadedPackage) -> ValidationResult:
    result = ValidationResult("profile active mods")
    if not profile_has_active_mod_state(profile, profile_dir):
        return result

    package_id = str(package.manifest.get("id"))
    package_version = str(package.manifest.get("version"))
    active_mods = profile_authoritative_mods(profile, profile_dir)
    for mod in active_mods:
        if mod.get("id") != package_id:
            continue
        active_version = mod.get("version")
        if isinstance(active_version, str) and active_version and active_version != package_version:
            result.add(
                "BML_PROFILE_PACKAGE_VERSION_DISABLED",
                "fatal",
                "Requested package id is enabled in the profile with a different version.",
                packageId=package_id,
                requestedVersion=package_version,
                enabledVersion=active_version,
            )
        return result

    result.add(
        "BML_PROFILE_PACKAGE_DISABLED",
        "fatal",
        "Requested package is not enabled in the profile active mod state. Run profile enable before launch, or use profile disable to keep it inactive.",
        packageId=package_id,
        enabledPackageIds=[str(mod.get("id")) for mod in active_mods if mod.get("id") is not None],
    )
    return result


def write_profile_active_mods(profile_dir: Path, profile: dict[str, Any], mods: list[dict[str, Any]], generated_at: str) -> None:
    profile.setdefault("profile", {})["updatedAt"] = generated_at
    profile["activeMods"] = mods
    write_json_file(profile_json_path(profile_dir), profile)
    write_json_file(
        active_mods_json_path(profile_dir),
        {
            "schemaVersion": SCHEMA_VERSION,
            "profileId": profile_id(profile),
            "generatedAt": generated_at,
            "mods": mods,
        },
    )


def command_package_pack(args: argparse.Namespace) -> int:
    package_dir = Path(args.package_dir).expanduser()
    result = ValidationResult(f"package directory {package_dir}")
    if not package_dir.is_dir():
        result.add("BML_PACKAGE_DIR_REQUIRED", "fatal", f"Package pack requires a directory containing {PACKAGE_MANIFEST_NAME}: {package_dir}")
        print_report(result, heading="Package pack")
        return 1

    package, load_result = load_package(str(package_dir))
    combined = ValidationResult("package pack")
    combined.extend(load_result)
    if package is not None:
        combined.extend(validate_package(package))
    if not combined.ok or package is None:
        print_report(combined, heading="Package pack validation")
        return 1

    out_path = Path(args.out).expanduser().resolve()
    try:
        entry_count = write_deterministic_package_archive(package.package_root, out_path)
    except OSError as exc:
        result.add("BML_PACKAGE_ARCHIVE_WRITE_FAILED", "fatal", f"Could not write package archive: {exc}")
        print_report(result, heading="Package pack")
        return 1
    print(
        json.dumps(
            {
                "status": "packed",
                "archive": str(out_path),
                "package": {
                    "id": package.manifest.get("id"),
                    "version": package.manifest.get("version"),
                },
                "entries": entry_count,
            },
            indent=2,
        )
    )
    return 0


def command_package_install(args: argparse.Namespace) -> int:
    source_path = Path(args.package_or_archive).expanduser()
    store_dir = Path(args.store).expanduser().resolve()
    archive_path: Path | None = None

    if source_path.is_file() and zipfile.is_zipfile(source_path):
        package, archive_path, load_result = load_package_archive(args.package_or_archive)
    elif source_path.suffix == ".bmlpkg":
        package, archive_path, load_result = load_package_archive(args.package_or_archive)
    else:
        package, load_result = load_package(args.package_or_archive)

    combined = ValidationResult("package install")
    combined.extend(load_result)
    if package is not None:
        combined.extend(validate_package(package))
    if not combined.ok or package is None:
        print_report(combined, heading="Package install validation")
        return 1

    try:
        installed_path, copied = install_loaded_package(package, store_dir, archive_path=archive_path)
    except (PackageInstallError, OSError, zipfile.BadZipFile) as exc:
        result = ValidationResult("package install")
        result.add("BML_PACKAGE_INSTALL_FAILED", "fatal", f"Could not install package: {exc}")
        print_report(result, heading="Package install")
        return 1

    print(
        json.dumps(
            {
                "status": "installed",
                "installedPath": str(installed_path),
                "package": {
                    "id": package.manifest.get("id"),
                    "version": package.manifest.get("version"),
                },
                "files": copied,
            },
            indent=2,
        )
    )
    return 0


def command_profile_enable(args: argparse.Namespace) -> int:
    profile, profile_dir, profile_result = load_profile(args.profile_dir)
    package, package_load_result = load_package(args.package)
    combined = ValidationResult("profile enable")
    combined.extend(profile_result)
    combined.extend(package_load_result)
    if package is not None:
        combined.extend(validate_package(package))
    if not combined.ok or profile is None or package is None:
        print_report(combined, heading="Profile enable validation")
        return 1

    now = utc_now()
    mod_id = str(package.manifest.get("id"))
    entry = {
        "id": mod_id,
        "version": package.manifest.get("version"),
        "packagePath": str(preferred_package_path(package)),
        "enabledAt": now,
    }
    mods = [mod for mod in profile_active_mods(profile) if mod.get("id") != mod_id]
    mods.append(entry)
    mods.sort(key=lambda mod: str(mod.get("id", "")))
    try:
        write_profile_active_mods(profile_dir, profile, mods, now)
    except OSError as exc:
        result = ValidationResult("profile enable")
        result.add("BML_PROFILE_WRITE_FAILED", "fatal", f"Could not write profile active mods: {exc}")
        print_report(result, heading="Profile enable")
        return 1
    print(json.dumps({"status": "enabled", "profile": profile_id(profile), "mod": entry}, indent=2))
    return 0


def command_profile_disable(args: argparse.Namespace) -> int:
    profile, profile_dir, profile_result = load_profile(args.profile_dir)
    if not profile_result.ok or profile is None:
        print_report(profile_result, heading="Profile disable validation")
        return 1

    mod_id = args.mod_id
    profile_mods = profile_active_mods(profile)
    file_mods = load_active_mods_file(profile_dir)
    candidates = profile_mods if profile_mods else file_mods
    remaining = [mod for mod in candidates if mod.get("id") != mod_id]
    removed = len(candidates) - len(remaining)
    now = utc_now()
    try:
        write_profile_active_mods(profile_dir, profile, remaining, now)
    except OSError as exc:
        result = ValidationResult("profile disable")
        result.add("BML_PROFILE_WRITE_FAILED", "fatal", f"Could not write profile active mods: {exc}")
        print_report(result, heading="Profile disable")
        return 1
    print(json.dumps({"status": "disabled", "profile": profile_id(profile), "modId": mod_id, "removed": removed}, indent=2))
    return 0


def command_profile_inspect(args: argparse.Namespace) -> int:
    profile, profile_dir, profile_result = load_profile(args.profile_dir)
    if not profile_result.ok or profile is None:
        print_report(profile_result, heading="Profile inspect validation")
        return 1
    runtime = profile.get("runtime") if isinstance(profile.get("runtime"), dict) else {}
    print(
        json.dumps(
            {
                "profileId": profile_id(profile),
                "profilePath": str(profile_json_path(profile_dir)),
                "runtime": {
                    "baronyExecutable": runtime.get("baronyExecutable"),
                    "runtimeInfo": runtime.get("runtimeInfo"),
                },
                "activeMods": profile_active_mods(profile),
            },
            indent=2,
        )
    )
    return 0


def package_checksum(loaded: LoadedPackage) -> str:
    digest = hashlib.sha256()
    with loaded.manifest_path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return f"sha256:{digest.hexdigest()}"


def bml_profile_root(profile_dir: Path) -> Path:
    return profile_dir / APP_ID


def profile_json_path(profile_dir: Path) -> Path:
    return bml_profile_root(profile_dir) / "profile.json"


def command_version(_args: argparse.Namespace) -> int:
    print(f"{APP_ID} app {APP_VERSION}")
    print(f"runtime contract {RUNTIME_CONTRACT}")
    print("python stdlib standalone skeleton")
    return 0

def command_steam_detect(args: argparse.Namespace) -> int:
    steam_install, result = detect_steam_install(args.manifest, args.install)
    if result.problems:
        print_report(result, heading="Steam Barony detection")
    if steam_install is not None:
        print(json.dumps({"status": "found" if result.ok else "invalid", "steam": steam_install}, indent=2))
    return 0 if result.ok else 1


def command_package_validate(args: argparse.Namespace) -> int:
    package, load_result = load_package(args.path)
    if package is None:
        print_report(load_result, heading="Package validation")
        return 1
    result = validate_package(package)
    result.problems = load_result.problems + result.problems
    print_report(result, heading=f"Package validation: {package.manifest_path}")
    return 0 if result.ok else 1


def command_runtime_validate(args: argparse.Namespace) -> int:
    package, package_load_result = load_package(args.package)
    combined = ValidationResult("runtime validation")
    combined.extend(package_load_result)
    if package is not None:
        combined.extend(validate_package(package))
    runtime_info, runtime_path, runtime_load_result = load_runtime_info(args.runtime_info)
    combined.extend(runtime_load_result)
    if package is not None and runtime_info is not None:
        combined.extend(validate_runtime_info(runtime_info, package))
    print_report(combined, heading=f"Runtime validation: {runtime_path}")
    return 0 if combined.ok else 1


def command_profile_create(args: argparse.Namespace) -> int:
    profile_dir = Path(args.profile_dir).expanduser().resolve()
    bml_root = bml_profile_root(profile_dir)
    logs_dir = bml_root / "logs"
    reports_dir = bml_root / "reports"
    manifests_dir = bml_root / "manifests"
    state_dir = bml_root / "state"
    warnings: list[str] = []
    steam_install: dict[str, Any] | None = None
    game_source = "manual"

    if getattr(args, "steam", False):
        game_source = "steam"
        steam_install, steam_result = detect_steam_install(args.steam_manifest, args.steam_install)
        if not steam_result.ok:
            print_report(steam_result, heading="Steam Barony detection")
            return 1
        for problem in steam_result.problems:
            warnings.append(format_problem(problem))
        selected_executable = args.barony_executable or (steam_install or {}).get("executable")
    else:
        selected_executable = args.barony_executable

    if not selected_executable:
        print("FAILED: profile create requires --barony-executable unless --steam can detect the Steam install.", file=sys.stderr)
        return 2

    barony_executable = Path(str(selected_executable)).expanduser()
    barony_executable_abs = barony_executable.resolve() if barony_executable.exists() else barony_executable.absolute()
    if not barony_executable.exists():
        warnings.append(f"[BML_PROFILE_EXECUTABLE_MISSING] WARNING: Barony executable does not exist yet: {barony_executable_abs}")

    runtime_info_abs: str | None = None
    if args.runtime_info:
        runtime_info_path = Path(args.runtime_info).expanduser()
        runtime_info_abs = str(runtime_info_path.resolve() if runtime_info_path.exists() else runtime_info_path.absolute())
        if not runtime_info_path.exists():
            warnings.append(f"[BML_PROFILE_RUNTIME_INFO_MISSING] WARNING: Runtime info file does not exist yet: {runtime_info_abs}")

    for directory in (bml_root, logs_dir, reports_dir, manifests_dir, state_dir):
        directory.mkdir(parents=True, exist_ok=True)

    now = utc_now()
    profile_payload = {
        "schemaVersion": SCHEMA_VERSION,
        "profile": {
            "id": args.profile_id,
            "createdAt": now,
            "updatedAt": now,
        },
        "app": {
            "id": APP_ID,
            "version": APP_VERSION,
        },
        "paths": {
            "profileRoot": str(profile_dir),
            "bmlRoot": str(bml_root),
            "logs": str(logs_dir),
            "reports": str(reports_dir),
            "manifests": str(manifests_dir),
            "state": str(state_dir),
            "runtimeManifest": str(bml_root / "runtime-manifest.json"),
        },
        "activeMods": [],
        "runtime": {
            "gameSource": game_source,
            "baronyExecutable": str(barony_executable_abs),
            "runtimeInfo": runtime_info_abs,
            "steam": steam_install,
        },
    }
    write_json_file(profile_json_path(profile_dir), profile_payload)

    print(json.dumps({"status": "created", "profile": profile_payload, "warnings": warnings}, indent=2))
    return 0


def load_profile(profile_dir_arg: str) -> tuple[dict[str, Any] | None, Path, ValidationResult]:
    profile_dir = Path(profile_dir_arg).expanduser().resolve()
    path = profile_json_path(profile_dir)
    result = ValidationResult(f"profile {path}")
    if not path.exists():
        result.add("BML_PROFILE_MISSING", "fatal", f"Profile JSON not found: {path}", hint="Run profile create first.")
        return None, profile_dir, result
    try:
        payload = parse_json_file(path)
    except json.JSONDecodeError as exc:
        result.add("BML_PROFILE_PARSE_FAILED", "fatal", f"Profile JSON is not valid JSON: {path}", line=exc.lineno, column=exc.colno, error=exc.msg)
        return None, profile_dir, result
    except OSError as exc:
        result.add("BML_PROFILE_READ_FAILED", "fatal", f"Could not read profile JSON: {exc}")
        return None, profile_dir, result
    if not isinstance(payload, dict):
        result.add("BML_PROFILE_INVALID", "fatal", "Profile root must be a JSON object.")
        return None, profile_dir, result

    profile = payload.get("profile")
    runtime = payload.get("runtime")
    paths = payload.get("paths")
    if not isinstance(profile, dict) or not isinstance(profile.get("id"), str) or not profile.get("id"):
        result.add("BML_PROFILE_ID_MISSING", "fatal", "Profile must include profile.id.")
    if not isinstance(runtime, dict) or not isinstance(runtime.get("baronyExecutable"), str) or not runtime.get("baronyExecutable"):
        result.add("BML_PROFILE_RUNTIME_INVALID", "fatal", "Profile must include runtime.baronyExecutable.")
    if not isinstance(paths, dict):
        result.add("BML_PROFILE_PATHS_INVALID", "error", "Profile should include paths object.")
    return payload, profile_dir, result


def build_runtime_manifest(
    profile: dict[str, Any],
    profile_dir: Path,
    package: LoadedPackage,
    runtime_info: dict[str, Any],
    runtime_executable: Path | None = None,
    runtime_registration: dict[str, Any] | None = None,
) -> dict[str, Any]:
    profile_runtime = profile.get("runtime", {})
    profile_id = profile.get("profile", {}).get("id")
    barony_executable = profile_runtime.get("baronyExecutable") if isinstance(profile_runtime, dict) else None
    steam_install = profile_runtime.get("steam") if isinstance(profile_runtime, dict) and isinstance(profile_runtime.get("steam"), dict) else None
    runtime_strategy = runtime_registration.get("runtimeStrategy") if isinstance(runtime_registration, dict) else None
    game_install_id = "profile-local"
    if steam_install:
        build_id = steam_install.get("buildId") or "unknown-build"
        game_install_id = f"steam:{STEAM_BARONY_APP_ID}:{build_id}"
    required_capabilities = [
        {
            "id": entry.get("id"),
            "version": entry.get("version"),
            "required": True,
        }
        for entry in package_required_capabilities(package.manifest)
    ]
    launch_payload: dict[str, Any] = {
        "profileId": profile_id,
        "gameInstallId": game_install_id,
        "gameSource": profile_runtime.get("gameSource", "manual") if isinstance(profile_runtime, dict) else "manual",
        "baronyExecutable": str(Path(barony_executable).expanduser().absolute()) if barony_executable else None,
        "launchExecutable": str(runtime_executable) if runtime_executable else None,
        "bmlRuntimeExecutable": None,
        "runtimeStrategy": runtime_strategy,
        "createdAt": utc_now(),
        "runtime": {
            "runtimeId": runtime_info.get("runtimeId"),
            "runtimeVersion": runtime_info.get("runtimeVersion"),
            "registryRuntimeId": runtime_registration.get("id") if isinstance(runtime_registration, dict) else None,
        },
        "steam": steam_install,
    }
    if isinstance(runtime_registration, dict):
        launch_payload.update(
            {
                "storefront": runtime_registration.get("storefront"),
                "platform": runtime_registration.get("platform"),
                "steamExecutable": runtime_registration.get("steamExecutable"),
                "steamExecutableSha256": runtime_registration.get("steamExecutableSha256"),
                "steamExecutableBuildId": runtime_registration.get("steamExecutableBuildId"),
                "gameVersionString": runtime_registration.get("gameVersionString"),
                "hookLibrary": runtime_registration.get("hookLibrary"),
                "hookLibrarySha256": runtime_registration.get("hookLibrarySha256"),
                "hookManifest": runtime_registration.get("hookManifest"),
                "hookManifestSha256": runtime_registration.get("hookManifestSha256"),
            }
        )
    return {
        "contract": {
            "id": RUNTIME_CONTRACT_ID,
            "version": RUNTIME_CONTRACT_VERSION,
        },
        "app": {
            "id": APP_ID,
            "version": APP_VERSION,
        },
        "launch": launch_payload,
        "mods": [
            {
                "id": package.manifest.get("id"),
                "version": package.manifest.get("version"),
                "packagePath": str(package.manifest_path),
                "checksumSet": package_checksum(package),
                "loadOrder": 10,
                "capabilities": required_capabilities,
                "modules": package.manifest.get("modules", {}),
            }
        ],
    }


def write_launch_artifacts(
    profile: dict[str, Any],
    profile_dir: Path,
    package: LoadedPackage,
    runtime_info: dict[str, Any],
    out_path: Path,
    runtime_executable: Path | None = None,
    runtime_registration: dict[str, Any] | None = None,
) -> tuple[dict[str, Any], Path]:
    bml_root = bml_profile_root(profile_dir)
    log_dir = bml_root / "logs"
    report_dir = bml_root / "reports"
    state_dir = bml_root / "state"
    manifest_dir = bml_root / "manifests"
    for directory in (log_dir, report_dir, state_dir, manifest_dir, out_path.parent):
        directory.mkdir(parents=True, exist_ok=True)

    manifest = build_runtime_manifest(profile, profile_dir, package, runtime_info, runtime_executable, runtime_registration)
    manifest["launch"].update(
        {
            "runtimeManifest": "BaronyModLoader/runtime-manifest.json",
            "runtimeLoadReport": "BaronyModLoader/reports/runtime-load-report.json",
            "runtimeLog": "BaronyModLoader/logs/runtime.log",
            "stateRoot": "BaronyModLoader/state/",
        }
    )
    write_json_file(out_path, manifest)

    active_mods_path = active_mods_json_path(profile_dir)
    write_json_file(
        active_mods_path,
        {
            "schemaVersion": SCHEMA_VERSION,
            "profileId": profile.get("profile", {}).get("id"),
            "generatedAt": manifest["launch"]["createdAt"],
            "mods": [
                {
                    "id": package.manifest.get("id"),
                    "version": package.manifest.get("version"),
                    "packagePath": str(package.manifest_path),
                    "runtimeManifest": str(out_path),
                }
            ],
        },
    )
    return manifest, active_mods_path


def write_launcher_failure(profile_dir: Path, result: ValidationResult) -> None:
    try:
        failure_path = bml_profile_root(profile_dir) / "logs" / "launcher-failure.json"
        write_json_file(
            failure_path,
            {
                "schemaVersion": SCHEMA_VERSION,
                "app": {"id": APP_ID, "version": APP_VERSION},
                "createdAt": utc_now(),
                "subject": result.subject,
                "problems": [
                    {"code": problem.code, "severity": problem.severity, "message": problem.message, "details": problem.details}
                    for problem in result.problems
                ],
            },
        )
    except OSError:
        pass


def profile_steam_install(profile: dict[str, Any]) -> dict[str, Any] | None:
    runtime = profile.get("runtime")
    if not isinstance(runtime, dict):
        return None
    steam = runtime.get("steam")
    return steam if isinstance(steam, dict) else None


def validate_profile_steam_install(profile: dict[str, Any]) -> ValidationResult:
    result = ValidationResult("profile Steam install")
    steam = profile_steam_install(profile)
    if steam is None:
        return result
    detected, detect_result = detect_steam_install(steam.get("manifestPath"), steam.get("installPath"))
    result.extend(detect_result)
    if detected is None:
        return result
    if detected.get("appId") != steam.get("appId"):
        result.add("BML_STEAM_APP_MISMATCH", "fatal", "Detected Steam app id no longer matches the profile.", profileAppId=steam.get("appId"), detectedAppId=detected.get("appId"))
    if detected.get("buildId") != steam.get("buildId"):
        result.add("BML_STEAM_BUILD_MISMATCH", "fatal", "Detected Steam build id no longer matches the profile.", profileBuildId=steam.get("buildId"), detectedBuildId=detected.get("buildId"))
    if Path(str(detected.get("installPath"))).resolve() != Path(str(steam.get("installPath"))).resolve():
        result.add("BML_STEAM_INSTALL_MISMATCH", "fatal", "Detected Steam install path no longer matches the profile.", profileInstallPath=steam.get("installPath"), detectedInstallPath=detected.get("installPath"))
    if detected.get("executableSha256") and steam.get("executableSha256") and detected.get("executableSha256") != steam.get("executableSha256"):
        result.add("BML_STEAM_EXECUTABLE_SHA_MISMATCH", "fatal", "Detected Steam executable checksum no longer matches the profile.", profileSha256=steam.get("executableSha256"), detectedSha256=detected.get("executableSha256"))
    if detected.get("executableBuildId") and steam.get("executableBuildId") and detected.get("executableBuildId") != steam.get("executableBuildId"):
        result.add("BML_STEAM_EXECUTABLE_BUILD_ID_MISMATCH", "fatal", "Detected Steam executable build id no longer matches the profile.", profileBuildId=steam.get("executableBuildId"), detectedBuildId=detected.get("executableBuildId"))
    if detected.get("gameVersionString") and steam.get("gameVersionString") and detected.get("gameVersionString") != steam.get("gameVersionString"):
        result.add("BML_STEAM_GAME_VERSION_MISMATCH", "fatal", "Detected Steam executable version string no longer matches the profile.", profileVersion=steam.get("gameVersionString"), detectedVersion=detected.get("gameVersionString"))
    return result


def validate_registered_runtime(
    runtime: dict[str, Any],
    profile: dict[str, Any],
    package: LoadedPackage,
) -> tuple[dict[str, Any] | None, Path | None, Path | None, ValidationResult]:
    runtime_id = runtime.get("id")
    result = ValidationResult(f"registered runtime {runtime_id or '<missing>'}")
    registered_platform = runtime.get("platform")
    result.extend(validate_registered_runtime_host_platform(registered_platform))
    strategy = runtime.get("runtimeStrategy")
    if strategy != RUNTIME_STRATEGY_INSTALLED_HOOK:
        result.add(
            "BML_REGISTERED_RUNTIME_STRATEGY_UNSUPPORTED",
            "fatal",
            "Registered runtime must use the installed-binary-hook strategy for BML v1.",
            runtimeStrategy=strategy,
            supported=list(SUPPORTED_RUNTIME_STRATEGIES),
        )

    def registered_file(field: str, missing_code: str, not_found_code: str, not_file_code: str, label: str) -> Path | None:
        value = runtime.get(field)
        path = Path(str(value)).expanduser().resolve() if isinstance(value, str) and value else None
        if path is None:
            result.add(missing_code, "fatal", f"Registered runtime is missing {label} path.", field=field)
        elif not path.exists():
            result.add(not_found_code, "fatal", f"Registered runtime {label} no longer exists: {path}", field=field)
        elif not path.is_file():
            result.add(not_file_code, "fatal", f"Registered runtime {label} is not a file: {path}", field=field)
        return path

    steam_executable = registered_file(
        "steamExecutable",
        "BML_REGISTERED_RUNTIME_STEAM_EXECUTABLE_MISSING",
        "BML_REGISTERED_RUNTIME_STEAM_EXECUTABLE_NOT_FOUND",
        "BML_REGISTERED_RUNTIME_STEAM_EXECUTABLE_NOT_FILE",
        "Steam executable",
    )
    hook_library = registered_file(
        "hookLibrary",
        "BML_REGISTERED_RUNTIME_HOOK_LIBRARY_MISSING",
        "BML_REGISTERED_RUNTIME_HOOK_LIBRARY_NOT_FOUND",
        "BML_REGISTERED_RUNTIME_HOOK_LIBRARY_NOT_FILE",
        "hook library",
    )
    hook_manifest = registered_file(
        "hookManifest",
        "BML_REGISTERED_RUNTIME_HOOK_MANIFEST_MISSING",
        "BML_REGISTERED_RUNTIME_HOOK_MANIFEST_NOT_FOUND",
        "BML_REGISTERED_RUNTIME_HOOK_MANIFEST_NOT_FILE",
        "hook manifest",
    )

    runtime_info_path = registered_file(
        "runtimeInfo",
        "BML_REGISTERED_RUNTIME_INFO_MISSING",
        "BML_REGISTERED_RUNTIME_INFO_NOT_FOUND",
        "BML_REGISTERED_RUNTIME_INFO_NOT_FILE",
        "runtime info",
    )
    runtime_info: dict[str, Any] | None = None
    if runtime_info_path is not None and runtime_info_path.exists() and runtime_info_path.is_file():
        runtime_info, runtime_info_path, load_result = load_runtime_info(str(runtime_info_path))
        result.extend(load_result)
        if runtime_info is not None:
            result.extend(validate_runtime_info(runtime_info, package))
            result.extend(validate_runtime_info_registered_platform(runtime_info, registered_platform))

    def compare_sha(path: Path | None, field: str, code_prefix: str, label: str) -> None:
        expected = runtime.get(field)
        if not isinstance(expected, str) or not expected:
            result.add(f"{code_prefix}_SHA_MISSING", "fatal", f"Registered runtime is missing expected checksum for {label}.", field=field)
            return
        if path is not None and path.exists() and path.is_file():
            actual = file_sha256(path)
            if actual != expected:
                result.add(f"{code_prefix}_SHA_MISMATCH", "fatal", f"Registered runtime {label} checksum changed.", expected=expected, actual=actual, field=field)

    compare_sha(steam_executable, "steamExecutableSha256", "BML_REGISTERED_RUNTIME_STEAM_EXECUTABLE", "Steam executable")
    compare_sha(hook_library, "hookLibrarySha256", "BML_REGISTERED_RUNTIME_HOOK_LIBRARY", "hook library")
    compare_sha(hook_manifest, "hookManifestSha256", "BML_REGISTERED_RUNTIME_HOOK_MANIFEST", "hook manifest")

    if steam_executable is not None and steam_executable.exists() and steam_executable.is_file():
        expected_build_id = runtime.get("steamExecutableBuildId")
        if isinstance(expected_build_id, str) and expected_build_id:
            actual_build_id = executable_build_id(steam_executable)
            if actual_build_id is None:
                result.add("BML_REGISTERED_RUNTIME_STEAM_BUILD_ID_UNVERIFIED", "warning", "Could not verify registered Steam executable build id on this platform.", expected=expected_build_id)
            elif actual_build_id != expected_build_id:
                result.add("BML_REGISTERED_RUNTIME_STEAM_BUILD_ID_MISMATCH", "fatal", "Registered Steam executable build id changed.", expected=expected_build_id, actual=actual_build_id)
        expected_version = runtime.get("gameVersionString")
        if isinstance(expected_version, str) and expected_version:
            actual_version = detect_game_version_string(steam_executable)
            if actual_version is None:
                result.add("BML_REGISTERED_RUNTIME_GAME_VERSION_UNVERIFIED", "warning", "Could not verify registered game version string.", expected=expected_version)
            elif actual_version != expected_version:
                result.add("BML_REGISTERED_RUNTIME_GAME_VERSION_MISMATCH", "fatal", "Registered game version string changed.", expected=expected_version, actual=actual_version)

    steam = profile_steam_install(profile)
    if steam is not None:
        if runtime.get("storefront") not in {None, "steam"}:
            result.add("BML_REGISTERED_RUNTIME_STOREFRONT_MISMATCH", "fatal", "Runtime was not registered for Steam.", runtimeStorefront=runtime.get("storefront"))
        if runtime.get("steamAppId") != steam.get("appId"):
            result.add("BML_REGISTERED_RUNTIME_STEAM_APP_MISMATCH", "fatal", "Runtime was not registered for this Steam app.", runtimeSteamAppId=runtime.get("steamAppId"), profileSteamAppId=steam.get("appId"))
        if runtime.get("steamBuildId") != steam.get("buildId"):
            result.add("BML_REGISTERED_RUNTIME_STEAM_BUILD_MISMATCH", "fatal", "Runtime was not registered for this Steam build.", runtimeSteamBuildId=runtime.get("steamBuildId"), profileSteamBuildId=steam.get("buildId"))
        if isinstance(steam.get("executable"), str) and steam_executable is not None and Path(str(steam.get("executable"))).expanduser().resolve() != steam_executable:
            result.add("BML_REGISTERED_RUNTIME_STEAM_EXECUTABLE_MISMATCH", "fatal", "Runtime was not registered for the profile's Steam executable.", runtimeExecutable=str(steam_executable), profileExecutable=steam.get("executable"))
        if steam.get("executableSha256") and runtime.get("steamExecutableSha256") and steam.get("executableSha256") != runtime.get("steamExecutableSha256"):
            result.add("BML_REGISTERED_RUNTIME_STEAM_EXECUTABLE_SHA_MISMATCH", "fatal", "Runtime Steam executable checksum does not match the profile.", runtimeSha256=runtime.get("steamExecutableSha256"), profileSha256=steam.get("executableSha256"))
        if steam.get("executableBuildId") and runtime.get("steamExecutableBuildId") and steam.get("executableBuildId") != runtime.get("steamExecutableBuildId"):
            result.add("BML_REGISTERED_RUNTIME_STEAM_EXECUTABLE_BUILD_ID_MISMATCH", "fatal", "Runtime Steam executable build id does not match the profile.", runtimeBuildId=runtime.get("steamExecutableBuildId"), profileBuildId=steam.get("executableBuildId"))
        if steam.get("gameVersionString") and runtime.get("gameVersionString") and steam.get("gameVersionString") != runtime.get("gameVersionString"):
            result.add("BML_REGISTERED_RUNTIME_STEAM_GAME_VERSION_MISMATCH", "fatal", "Runtime Steam game version does not match the profile.", runtimeVersion=runtime.get("gameVersionString"), profileVersion=steam.get("gameVersionString"))

    return runtime_info, runtime_info_path, steam_executable, result


def select_registered_runtime(
    registry: dict[str, Any],
    profile: dict[str, Any],
    package: LoadedPackage,
    requested_runtime_id: str | None,
) -> tuple[dict[str, Any] | None, dict[str, Any] | None, Path | None, Path | None, ValidationResult]:
    result = ValidationResult("runtime selection")
    runtimes = [entry for entry in registry.get("runtimes", []) if isinstance(entry, dict)]
    if requested_runtime_id:
        runtimes = [entry for entry in runtimes if entry.get("id") == requested_runtime_id]
        if not runtimes:
            result.add("BML_RUNTIME_REGISTRY_ID_MISSING", "fatal", f"Runtime id not found in registry: {requested_runtime_id}")
            return None, None, None, None, result

    for runtime in runtimes:
        runtime_info, runtime_info_path, executable, runtime_result = validate_registered_runtime(runtime, profile, package)
        if runtime_result.ok and runtime_info is not None and executable is not None:
            return runtime, runtime_info, runtime_info_path, executable, result
        result.extend(runtime_result)

    if not result.problems:
        result.add("BML_RUNTIME_REGISTRY_NO_COMPATIBLE_RUNTIME", "fatal", "No registered installed hook runtime is compatible with this profile/package.", hint="Register a BML hook runtime for the detected installed PC build.")
    return None, None, None, None, result


def launch_working_directory(profile: dict[str, Any], executable: Path) -> Path:
    steam = profile_steam_install(profile)
    if steam and isinstance(steam.get("installPath"), str):
        return Path(steam["installPath"]).expanduser().resolve()
    return executable.parent


def launch_environment(profile: dict[str, Any], profile_dir: Path, manifest_path: Path, runtime: dict[str, Any] | None = None) -> dict[str, str]:
    env = dict(os.environ)
    for key in list(env):
        if key in BML_LAUNCH_ENV_KEYS or key in STEAM_LAUNCH_ENV_KEYS or key in DYNAMIC_LOADER_ENV_KEYS or key.startswith(DYNAMIC_LOADER_ENV_PREFIXES):
            env.pop(key, None)

    env["BML_PROFILE_DIR"] = str(profile_dir)
    env["BML_RUNTIME_MANIFEST"] = str(manifest_path)
    if isinstance(runtime, dict):
        env["BML_RUNTIME_STRATEGY"] = str(runtime.get("runtimeStrategy") or "")
        hook_manifest = runtime.get("hookManifest")
        if isinstance(hook_manifest, str) and hook_manifest:
            env["BML_HOOK_MANIFEST"] = hook_manifest
        hook_library = runtime.get("hookLibrary")
        if isinstance(hook_library, str) and hook_library:
            env["BML_HOOK_LIBRARY"] = hook_library
            if sys.platform.startswith("linux"):
                env["LD_PRELOAD"] = hook_library
            elif sys.platform == "darwin":
                env["DYLD_INSERT_LIBRARIES"] = hook_library
    steam = profile_steam_install(profile)
    if steam:
        env["SteamAppId"] = str(steam.get("appId") or STEAM_BARONY_APP_ID)
        env["SteamGameId"] = str(steam.get("appId") or STEAM_BARONY_APP_ID)
        install_path = steam.get("installPath")
        if sys.platform.startswith("linux") and isinstance(install_path, str) and install_path:
            env["LD_LIBRARY_PATH"] = install_path
    return env


def normalize_barony_args(raw_args: list[str]) -> list[str]:
    if raw_args and raw_args[0] == "--":
        return raw_args[1:]
    return raw_args


def command_launch_plan(args: argparse.Namespace) -> int:
    combined = ValidationResult("launch plan validation")

    profile, profile_dir, profile_result = load_profile(args.profile_dir)
    combined.extend(profile_result)

    package, package_load_result = load_package(args.package)
    combined.extend(package_load_result)
    if package is not None:
        combined.extend(validate_package(package))
    if profile is not None and package is not None:
        combined.extend(validate_profile_package_enabled(profile, profile_dir, package))


    runtime_info, runtime_info_path, runtime_load_result = load_runtime_info(args.runtime_info)
    combined.extend(runtime_load_result)
    if package is not None and runtime_info is not None:
        combined.extend(validate_runtime_info(runtime_info, package))

    if not combined.ok:
        print_report(combined, heading="Launch plan validation")
        return 1

    assert profile is not None
    assert package is not None
    assert runtime_info is not None

    if args.out:
        out_path = Path(args.out).expanduser().resolve()
    else:
        out_path = bml_profile_root(profile_dir) / "runtime-manifest.json"

    manifest, active_mods_path = write_launch_artifacts(profile, profile_dir, package, runtime_info, out_path)

    print(json.dumps({"status": "created", "runtimeManifest": str(out_path), "activeMods": str(active_mods_path), "runtimeInfo": str(runtime_info_path), "createdAt": manifest["launch"]["createdAt"]}, indent=2))
    return 0


def command_launch(args: argparse.Namespace) -> int:
    combined = ValidationResult("launch validation")

    profile, profile_dir, profile_result = load_profile(args.profile_dir)
    combined.extend(profile_result)

    package, package_load_result = load_package(args.package)
    combined.extend(package_load_result)
    if package is not None:
        combined.extend(validate_package(package))
    if profile is not None and package is not None:
        combined.extend(validate_profile_package_enabled(profile, profile_dir, package))


    registry_path = runtime_registry_path(args.registry)
    registry, registry_result = load_runtime_registry(registry_path, missing_ok=False)
    combined.extend(registry_result)

    if profile is not None:
        combined.extend(validate_profile_steam_install(profile))

    selected_runtime: dict[str, Any] | None = None
    runtime_info: dict[str, Any] | None = None
    runtime_info_path: Path | None = None
    runtime_executable: Path | None = None
    if profile is not None and package is not None and registry_result.ok:
        selected_runtime, runtime_info, runtime_info_path, runtime_executable, selection_result = select_registered_runtime(
            registry,
            profile,
            package,
            args.runtime,
        )
        combined.extend(selection_result)

    if args.out:
        out_path = Path(args.out).expanduser().resolve()
    else:
        out_path = bml_profile_root(profile_dir) / "runtime-manifest.json"

    if not combined.ok:
        print_report(combined, heading="Launch validation")
        write_launcher_failure(profile_dir, combined)
        return 1

    assert profile is not None
    assert package is not None
    assert selected_runtime is not None
    assert runtime_info is not None
    assert runtime_info_path is not None
    assert runtime_executable is not None

    manifest, active_mods_path = write_launch_artifacts(profile, profile_dir, package, runtime_info, out_path, runtime_executable, selected_runtime)
    barony_args = normalize_barony_args(args.barony_args)
    command = [
        str(runtime_executable),
        *barony_args,
    ]
    cwd = launch_working_directory(profile, runtime_executable)
    env = launch_environment(profile, profile_dir, out_path, selected_runtime)
    launch_log = bml_profile_root(profile_dir) / "logs" / "launcher-runtime.log"

    launch_payload = {
        "status": "dry-run" if args.dry_run else "launching",
        "registry": str(registry_path),
        "runtime": selected_runtime.get("id"),
        "runtimeInfo": str(runtime_info_path),
        "runtimeManifest": str(out_path),
        "activeMods": str(active_mods_path),
        "cwd": str(cwd),
        "command": command,
        "environment": {
            key: env[key]
            for key in ("SteamAppId", "SteamGameId", "BML_PROFILE_DIR", "BML_RUNTIME_MANIFEST", "BML_RUNTIME_STRATEGY", "BML_HOOK_MANIFEST", "BML_HOOK_LIBRARY", "LD_PRELOAD", "DYLD_INSERT_LIBRARIES", "LD_LIBRARY_PATH")
            if key in env
        },
        "launchLog": str(launch_log),
    }
    if args.dry_run:
        print(json.dumps(launch_payload, indent=2))
        return 0

    try:
        with launch_log.open("w", encoding="utf-8") as log_handle:
            log_handle.write(json.dumps({"createdAt": manifest["launch"]["createdAt"], "command": command, "cwd": str(cwd)}, indent=2))
            log_handle.write("\n\n")
            completed = subprocess.run(command, cwd=str(cwd), env=env, stdout=log_handle, stderr=subprocess.STDOUT, check=False)
    except OSError as exc:
        failure = ValidationResult("launch execution")
        failure.add("BML_LAUNCH_EXEC_FAILED", "fatal", f"Could not execute runtime: {exc}")
        print_report(failure, heading="Launch execution")
        write_launcher_failure(profile_dir, failure)
        return 1

    launch_payload["status"] = "exited"
    launch_payload["returnCode"] = completed.returncode
    print(json.dumps(launch_payload, indent=2))
    return int(completed.returncode)


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog="BaronyModLoader",
        description="BaronyModLoader standalone app skeleton for package/profile/runtime validation.",
    )
    subparsers = parser.add_subparsers(dest="command", required=True)

    version_parser = subparsers.add_parser("version", help="Print app and runtime contract version information.")
    version_parser.set_defaults(func=command_version)

    steam_parser = subparsers.add_parser("steam", help="Steam Barony install discovery commands.")
    steam_subparsers = steam_parser.add_subparsers(dest="steam_command", required=True)
    steam_detect = steam_subparsers.add_parser("detect", help="Detect the installed Steam copy of Barony.")
    steam_detect.add_argument("--manifest", help="Optional appmanifest_371970.acf path.")
    steam_detect.add_argument("--install", help="Optional Steam Barony install directory.")
    steam_detect.set_defaults(func=command_steam_detect)

    package_parser = subparsers.add_parser("package", help="Package commands.")
    package_subparsers = package_parser.add_subparsers(dest="package_command", required=True)
    package_validate = package_subparsers.add_parser("validate", help="Validate a package directory or JSON manifest file.")
    package_validate.add_argument("path", help=f"Package directory containing {PACKAGE_MANIFEST_NAME}, or direct manifest JSON path.")
    package_validate.set_defaults(func=command_package_validate)
    package_pack = package_subparsers.add_parser("pack", help="Validate and pack a package directory into a deterministic .bmlpkg archive.")
    package_pack.add_argument("package_dir", help=f"Package directory containing {PACKAGE_MANIFEST_NAME}.")
    package_pack.add_argument("--out", required=True, help="Output .bmlpkg archive path.")
    package_pack.set_defaults(func=command_package_pack)
    package_install = package_subparsers.add_parser("install", help="Install a package directory, direct manifest, or .bmlpkg archive into a package store.")
    package_install.add_argument("package_or_archive", help="Package directory, direct manifest JSON path, or zip-compatible .bmlpkg archive.")
    package_install.add_argument("--store", required=True, help="Package store directory. Packages install under <store>/<package-id>/<version>/.")
    package_install.set_defaults(func=command_package_install)

    runtime_parser = subparsers.add_parser("runtime", help="Runtime metadata commands.")
    runtime_subparsers = runtime_parser.add_subparsers(dest="runtime_command", required=True)
    runtime_validate = runtime_subparsers.add_parser("validate", help="Validate runtime info against a package.")
    runtime_validate.add_argument("runtime_info", help="Path to runtime-info JSON.")
    runtime_validate.add_argument("--package", required=True, help="Package directory or direct package manifest JSON path.")
    runtime_validate.set_defaults(func=command_runtime_validate)
    runtime_report = runtime_subparsers.add_parser("report", help="Summarize and validate a runtime-load-report JSON.")
    runtime_report.add_argument("runtime_load_report", help="Path to runtime-load-report JSON.")
    runtime_report.set_defaults(func=command_runtime_report)
    runtime_info = runtime_subparsers.add_parser("info", help="Summarize runtime-info JSON capabilities.")
    runtime_info.add_argument("runtime_info", help="Path to runtime-info JSON.")
    runtime_info.set_defaults(func=command_runtime_info)
    runtime_register = runtime_subparsers.add_parser("register", help="Register a BML hook runtime for an installed game executable.")
    runtime_register.add_argument("--registry", help=f"Runtime registry path. Defaults to {DEFAULT_RUNTIME_REGISTRY_PATH}.")
    runtime_register.add_argument("--id", help="Stable runtime id. Defaults to a runtime/build/platform-derived id.")
    runtime_register.add_argument("--runtime-strategy", choices=SUPPORTED_RUNTIME_STRATEGIES, default=RUNTIME_STRATEGY_INSTALLED_HOOK, help="Runtime integration strategy. v1 supports installed-binary-hook.")
    runtime_register.add_argument("--executable", help="Deprecated alias for --steam-executable.")
    runtime_register.add_argument("--steam-executable", help="Installed Barony executable path to launch, e.g. barony.x86_64.")
    runtime_register.add_argument("--hook-library", help="BML hook library injected into the installed executable, e.g. native/barony-modloader-hook/build/libbarony_bml.so.")
    runtime_register.add_argument("--hook-manifest", help="Build-specific hook manifest describing the symbol map and hook capabilities.")
    runtime_register.add_argument("--runtime-info", required=True, help="Runtime-info JSON path for this hook runtime.")
    runtime_register.add_argument("--steam-app-id", default=STEAM_BARONY_APP_ID, help="Steam app id this runtime targets.")
    runtime_register.add_argument("--steam-build-id", help="Steam build id this runtime targets.")
    runtime_register.add_argument("--steam-executable-build-id", help="Expected native build id for the installed Steam executable.")
    runtime_register.add_argument("--game-version-string", help="Expected game version string discovered inside the installed executable.")
    runtime_register.add_argument("--platform", help="Platform id. Defaults to the current OS/machine.")
    runtime_register.set_defaults(func=command_runtime_register)
    runtime_list = runtime_subparsers.add_parser("list", help="List registered BML runtime executables.")
    runtime_list.add_argument("--registry", help=f"Runtime registry path. Defaults to {DEFAULT_RUNTIME_REGISTRY_PATH}.")
    runtime_list.set_defaults(func=command_runtime_list)
    runtime_inspect = runtime_subparsers.add_parser("inspect", help="Inspect one registered BML runtime executable.")
    runtime_inspect.add_argument("runtime_id", help="Registered runtime id.")
    runtime_inspect.add_argument("--registry", help=f"Runtime registry path. Defaults to {DEFAULT_RUNTIME_REGISTRY_PATH}.")
    runtime_inspect.set_defaults(func=command_runtime_inspect)

    profile_parser = subparsers.add_parser("profile", help="Profile commands.")
    profile_subparsers = profile_parser.add_subparsers(dest="profile_command", required=True)
    profile_create = profile_subparsers.add_parser("create", help="Create a profile-local BaronyModLoader/profile.json.")
    profile_create.add_argument("profile_dir", help="Profile directory to create or update.")
    profile_create.add_argument("--id", dest="profile_id", required=True, help="Stable profile id.")
    profile_create.add_argument("--barony-executable", help="Selected Barony executable path. Required unless --steam detects it.")
    profile_create.add_argument("--steam", action="store_true", help="Detect and record the installed Steam copy of Barony as the game source.")
    profile_create.add_argument("--steam-install", help="Optional Steam Barony install directory for --steam.")
    profile_create.add_argument("--steam-manifest", help="Optional Steam appmanifest_371970.acf path for --steam.")
    profile_create.add_argument("--runtime-info", help="Optional runtime-info JSON path to record in the profile.")
    profile_create.set_defaults(func=command_profile_create)
    profile_enable = profile_subparsers.add_parser("enable", help="Enable an installed package in a profile.")
    profile_enable.add_argument("profile_dir", help="Profile directory created by profile create.")
    profile_enable.add_argument("--package", required=True, help="Installed package directory or direct package manifest JSON path.")
    profile_enable.set_defaults(func=command_profile_enable)
    profile_disable = profile_subparsers.add_parser("disable", help="Disable an active mod in a profile without deleting package files.")
    profile_disable.add_argument("profile_dir", help="Profile directory created by profile create.")
    profile_disable.add_argument("--mod-id", required=True, help="Mod/package id to disable.")
    profile_disable.set_defaults(func=command_profile_disable)
    profile_inspect = profile_subparsers.add_parser("inspect", help="Print selected runtime and active mods for a profile.")
    profile_inspect.add_argument("profile_dir", help="Profile directory created by profile create.")
    profile_inspect.set_defaults(func=command_profile_inspect)

    launch_plan = subparsers.add_parser("launch-plan", help="Validate profile/package/runtime and write runtime-manifest.json.")
    launch_plan.add_argument("profile_dir", help="Profile directory created by profile create.")
    launch_plan.add_argument("--package", required=True, help="Package directory or direct package manifest JSON path.")
    launch_plan.add_argument("--runtime-info", required=True, help="Runtime-info JSON path.")
    launch_plan.add_argument("--out", help="Output runtime-manifest.json path. Defaults to <profile>/BaronyModLoader/runtime-manifest.json.")
    launch_plan.set_defaults(func=command_launch_plan)

    launch = subparsers.add_parser("launch", help="Select a registered runtime, write launch artifacts, and start Barony.")
    launch.add_argument("profile_dir", help="Profile directory created by profile create.")
    launch.add_argument("--package", required=True, help="Installed package directory or direct package manifest JSON path.")
    launch.add_argument("--registry", help=f"Runtime registry path. Defaults to {DEFAULT_RUNTIME_REGISTRY_PATH}.")
    launch.add_argument("--runtime", help="Explicit registered runtime id. Defaults to the first compatible runtime.")
    launch.add_argument("--out", help="Output runtime-manifest.json path. Defaults to <profile>/BaronyModLoader/runtime-manifest.json.")
    launch.add_argument("--dry-run", action="store_true", help="Validate and print the launch command without starting Barony.")
    launch.set_defaults(func=command_launch)

    return parser


def main(argv: list[str] | None = None) -> int:
    raw_argv = list(sys.argv[1:] if argv is None else argv)
    barony_args: list[str] | None = None
    if raw_argv and raw_argv[0] == "launch" and "--" in raw_argv:
        separator = raw_argv.index("--")
        barony_args = raw_argv[separator + 1 :]
        raw_argv = raw_argv[:separator]

    parser = build_parser()
    args = parser.parse_args(raw_argv)
    if getattr(args, "command", None) == "launch":
        args.barony_args = barony_args or []
    return int(args.func(args))


if __name__ == "__main__":
    raise SystemExit(main())
