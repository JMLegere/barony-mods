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
import re
import sys
from dataclasses import dataclass, field
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Iterable

APP_ID = "BaronyModLoader"
APP_VERSION = "0.1.0"
SCHEMA_VERSION = "0.1.0"
RUNTIME_CONTRACT_ID = "bml-runtime-contract"
RUNTIME_CONTRACT_VERSION = "0.1.0"
RUNTIME_CONTRACT = f"{RUNTIME_CONTRACT_ID}@{RUNTIME_CONTRACT_VERSION}"
PACKAGE_MANIFEST_NAME = "bml-package.json"

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
    for key in ("contractVersions", "contracts", "supportedContracts"):
        raw = runtime_info.get(key)
        if raw is None:
            continue
        values = raw if isinstance(raw, list) else [raw]
        for item in values:
            contract_id, version = parse_contract(item)
            if contract_id and version:
                versions.add((contract_id, version))
    contract_id, version = parse_contract(runtime_info.get("contract"))
    if contract_id and version:
        versions.add((contract_id, version))
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

    barony_executable = Path(args.barony_executable).expanduser()
    barony_executable_abs = barony_executable.resolve() if barony_executable.exists() else Path(args.barony_executable).expanduser().absolute()
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
            "baronyExecutable": str(barony_executable_abs),
            "runtimeInfo": runtime_info_abs,
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
) -> dict[str, Any]:
    profile_id = profile.get("profile", {}).get("id")
    barony_executable = profile.get("runtime", {}).get("baronyExecutable")
    required_capabilities = [
        {
            "id": entry.get("id"),
            "version": entry.get("version"),
            "required": True,
        }
        for entry in package_required_capabilities(package.manifest)
    ]
    return {
        "contract": {
            "id": RUNTIME_CONTRACT_ID,
            "version": RUNTIME_CONTRACT_VERSION,
        },
        "app": {
            "id": APP_ID,
            "version": APP_VERSION,
        },
        "launch": {
            "profileId": profile_id,
            "gameInstallId": "profile-local",
            "baronyExecutable": str(Path(barony_executable).expanduser().absolute()) if barony_executable else None,
            "createdAt": utc_now(),
            "runtime": {
                "runtimeId": runtime_info.get("runtimeId"),
                "runtimeVersion": runtime_info.get("runtimeVersion"),
            },
        },
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


def command_launch_plan(args: argparse.Namespace) -> int:
    combined = ValidationResult("launch plan validation")

    profile, profile_dir, profile_result = load_profile(args.profile_dir)
    combined.extend(profile_result)

    package, package_load_result = load_package(args.package)
    combined.extend(package_load_result)
    if package is not None:
        combined.extend(validate_package(package))

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

    manifest = build_runtime_manifest(profile, profile_dir, package, runtime_info)
    write_json_file(out_path, manifest)

    active_mods_path = bml_profile_root(profile_dir) / "active-mods.json"
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

    print(json.dumps({"status": "created", "runtimeManifest": str(out_path), "runtimeInfo": str(runtime_info_path)}, indent=2))
    return 0


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog="BaronyModLoader",
        description="BaronyModLoader standalone app skeleton for package/profile/runtime validation.",
    )
    subparsers = parser.add_subparsers(dest="command", required=True)

    version_parser = subparsers.add_parser("version", help="Print app and runtime contract version information.")
    version_parser.set_defaults(func=command_version)

    package_parser = subparsers.add_parser("package", help="Package commands.")
    package_subparsers = package_parser.add_subparsers(dest="package_command", required=True)
    package_validate = package_subparsers.add_parser("validate", help="Validate a package directory or JSON manifest file.")
    package_validate.add_argument("path", help=f"Package directory containing {PACKAGE_MANIFEST_NAME}, or direct manifest JSON path.")
    package_validate.set_defaults(func=command_package_validate)

    runtime_parser = subparsers.add_parser("runtime", help="Runtime metadata commands.")
    runtime_subparsers = runtime_parser.add_subparsers(dest="runtime_command", required=True)
    runtime_validate = runtime_subparsers.add_parser("validate", help="Validate runtime info against a package.")
    runtime_validate.add_argument("runtime_info", help="Path to runtime-info JSON.")
    runtime_validate.add_argument("--package", required=True, help="Package directory or direct package manifest JSON path.")
    runtime_validate.set_defaults(func=command_runtime_validate)

    profile_parser = subparsers.add_parser("profile", help="Profile commands.")
    profile_subparsers = profile_parser.add_subparsers(dest="profile_command", required=True)
    profile_create = profile_subparsers.add_parser("create", help="Create a profile-local BaronyModLoader/profile.json.")
    profile_create.add_argument("profile_dir", help="Profile directory to create or update.")
    profile_create.add_argument("--id", dest="profile_id", required=True, help="Stable profile id.")
    profile_create.add_argument("--barony-executable", required=True, help="Selected Barony executable path. It may be created later.")
    profile_create.add_argument("--runtime-info", help="Optional runtime-info JSON path to record in the profile.")
    profile_create.set_defaults(func=command_profile_create)

    launch_plan = subparsers.add_parser("launch-plan", help="Validate profile/package/runtime and write runtime-manifest.json.")
    launch_plan.add_argument("profile_dir", help="Profile directory created by profile create.")
    launch_plan.add_argument("--package", required=True, help="Package directory or direct package manifest JSON path.")
    launch_plan.add_argument("--runtime-info", required=True, help="Runtime-info JSON path.")
    launch_plan.add_argument("--out", help="Output runtime-manifest.json path. Defaults to <profile>/BaronyModLoader/runtime-manifest.json.")
    launch_plan.set_defaults(func=command_launch_plan)

    return parser


def main(argv: list[str] | None = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)
    return int(args.func(args))


if __name__ == "__main__":
    raise SystemExit(main())
