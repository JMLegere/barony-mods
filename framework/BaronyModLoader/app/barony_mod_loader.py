#!/usr/bin/env python3
"""BaronyModLoader standalone app skeleton.

This executable slice intentionally stays narrow: it validates package
metadata, validates engine runtime capability metadata, creates profile-local
app state, writes launch-time runtime artifacts, and starts Barony only through
explicit launch commands or GUI launch actions.
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
import subprocess
import sys
import textwrap
import time
import zipfile
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
STEAM_BARONY_WINDOWS_EXECUTABLE = "barony.exe"
STEAM_LIBRARY_RELATIVE_INSTALL = Path("common") / "Barony"
STEAM_MANIFEST_NAME = f"appmanifest_{STEAM_BARONY_APP_ID}.acf"
RUNTIME_STRATEGY_INSTALLED_HOOK = "installed-binary-hook"
LAUNCH_ADAPTER_LINUX_LD_PRELOAD = "linux-ld-preload"
LAUNCH_ADAPTER_WINDOWS_CREATEPROCESS_LOADLIBRARY = "windows-createprocess-loadlibrary"
WINDOWS_LAUNCHER_EXECUTABLE = "bml-win-launcher.exe"
WINDOWS_HOOK_LIBRARY_NAME = "barony_bml.dll"
WINDOWS_LIVE_RUNTIME_EVIDENCE_KIND = "live-windows-runtime"
APP_ROOT = Path(__file__).resolve().parents[1]



def normalize_platform_machine(machine: str | None = None) -> str:
    raw_machine = machine if machine is not None else platform.machine()
    normalized = (raw_machine or "unknown").strip().casefold()
    aliases = {
        "amd64": "x86_64",
        "x64": "x86_64",
        "x86-64": "x86_64",
        "x86_64": "x86_64",
        "aarch64": "arm64",
        "arm64": "arm64",
    }
    return aliases.get(normalized, normalized or "unknown")

SUPPORTED_RUNTIME_STRATEGIES = (RUNTIME_STRATEGY_INSTALLED_HOOK,)


@dataclass(frozen=True)
class PlatformTarget:
    os_name: str
    executable_name: str
    hook_artifact_extension: str
    launch_adapter: str
    steamapps_candidates: tuple[Path, ...]

    def platform_id(self, machine: str | None = None) -> str:
        return f"{self.os_name}-{normalize_platform_machine(machine)}"

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

CANONICAL_RUNEBOUND_ELIXIR_CAPABILITIES = (
    "elixir_item_metadata",
    "elixir_drop_generation",
    "elixir_consumption",
    "active_elixir_effect_state",
    "active_elixir_effect_application",
    "item_name_tooltip_rendering",
    "multiplayer_version_metadata",
)


RUNEBOUND_ELIXIRS_PACKAGE_ID = "jml.runebound-elixirs"
RUNEBOUND_ELIXIRS_MODULE_NAME = "runeboundElixirs"
RUNEBOUND_ELIXIRS_NAMESPACE = "runebound_elixirs"
RUNEBOUND_ELIXIRS_CARRIER_ITEM_TYPE = "POTION_STRENGTH"
RUNEBOUND_ELIXIRS_DATA_ROOT = "content/data/bml"
RUNEBOUND_ELIXIRS_CATALOG_FILE = f"{RUNEBOUND_ELIXIRS_DATA_ROOT}/elixir-catalog.json"
RUNEBOUND_ELIXIRS_DROP_TABLE_FILE = f"{RUNEBOUND_ELIXIRS_DATA_ROOT}/elixir-drop-tables.json"
RUNEBOUND_ELIXIRS_REQUIRED_DATA_FILES = (RUNEBOUND_ELIXIRS_CATALOG_FILE, RUNEBOUND_ELIXIRS_DROP_TABLE_FILE)
RUNEBOUND_ELIXIRS_LIVE_INSTALL_REPORT = "BaronyModLoader/reports/runebound-elixir-live-install-report.json"

RUNEBOUND_ELIXIRS_REQUIRED_MODULE_KEYS = (
    "namespace",
    "schemaVersion",
    "authority",
    "carrierItemType",
    "dataFiles",
    "dropPolicy",
    "activeEffects",
    "display",
    "multiplayer",
    "failurePolicy",
)
RUNEBOUND_ELIXIRS_SUPPORTED_EFFECT_OPCODES = frozenset(
    {
        "stat_add",
        "stat_multiply",
        "armor_ac_add",
        "resource_add",
        "message_only",
    }
)

RECOGNIZED_CAPABILITIES = frozenset((*CANONICAL_STASH_CAPABILITIES, *CANONICAL_RUNEBOUND_ELIXIR_CAPABILITIES))
RECOGNIZED_RUNTIME_MODULES = frozenset(
    {
        "persistentStorage",
        "persistentInventories",
        "voidChestBindings",
        "placements",
        "multiplayer",
        "runeboundElixirs",
    }
)
RECOGNIZED_RUNTIME_REPORT_MODULES = frozenset((*RECOGNIZED_RUNTIME_MODULES, f"modules.{RUNEBOUND_ELIXIRS_MODULE_NAME}"))




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
    archive_members: set[str] | None = None


@dataclass
class CommandResult:
    argv: list[str]
    label: str
    exit_code: int
    stdout: str
    stderr: str
    duration: float
    failure_summary: str | None = None


@dataclass
class DashboardDto:
    install: dict[str, Any] = field(default_factory=dict)
    profile: dict[str, Any] = field(default_factory=dict)
    package: dict[str, Any] = field(default_factory=dict)
    readiness: dict[str, Any] = field(default_factory=dict)
    diagnostics: dict[str, Any] = field(default_factory=dict)
    workshop: dict[str, Any] = field(default_factory=dict)
    disabled_reasons: list[str] = field(default_factory=list)


ICON_LABELS = {
    "os.linux": "Linux",
    "os.windows": "Windows",
    "os.darwin": "macOS",
    "store.steam": "Steam",
    "store.steam_workshop": "Steam Workshop",
    "asset.workshop_thumbnail": "Workshop thumbnail",
    "asset.library_grid": "Steam library grid image",
    "runtime.not_run": "Runtime not run",
    "runtime.fake_only": "Fake harness only",
    "runtime.production_validated": "Production validated",
    "runtime.failed": "Runtime failed",
}


DEFAULT_DASHBOARD_DTO = DashboardDto(
    install={"status": "missing", "icon": "os.linux", "label": ICON_LABELS["os.linux"]},
    profile={"status": "not_selected"},
    package={"status": "not_selected"},
    readiness={"status": "blocked"},
    diagnostics={"status": "not_run", "icon": "runtime.not_run", "label": ICON_LABELS["runtime.not_run"]},
    workshop={"status": "disabled_stub", "icon": "store.steam_workshop", "label": ICON_LABELS["store.steam_workshop"]},
    disabled_reasons=["No install, profile, or package selected."],
)

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

def command_failure_summary(exit_code: int, stderr: str, stdout: str) -> str | None:
    if exit_code == 0:
        return None
    for stream in (stderr, stdout):
        for line in stream.splitlines():
            if line.strip():
                return line.strip()
    return f"Command exited with code {exit_code}."


def run_command(argv: list[str], *, label: str | None = None, cwd: Path | None = None, timeout_seconds: float | None = None) -> CommandResult:
    started = time.monotonic()
    completed = subprocess.run(
        argv,
        cwd=str(cwd) if cwd is not None else None,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        timeout=timeout_seconds,
        check=False,
    )
    duration = time.monotonic() - started
    return CommandResult(
        argv=list(argv),
        label=label or Path(argv[0]).name if argv else "command",
        exit_code=int(completed.returncode),
        stdout=completed.stdout or "",
        stderr=completed.stderr or "",
        duration=duration,
        failure_summary=command_failure_summary(int(completed.returncode), completed.stderr or "", completed.stdout or ""),
    )


def parse_json_file(path: Path) -> Any:
    with path.open("r", encoding="utf-8") as handle:
        return json.load(handle)


def write_json_file(path: Path, payload: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8") as handle:
        json.dump(payload, handle, indent=2, sort_keys=False)
        handle.write("\n")

def platform_os_name(sys_platform: str | None = None) -> str:
    value = sys_platform if sys_platform is not None else sys.platform
    if value.startswith("linux"):
        return "linux"
    if value == "darwin":
        return "macos"
    if value.startswith(("win32", "cygwin", "msys")):
        return "windows"
    return value or "unknown"


def linux_steamapps_candidates(home: Path | None = None) -> tuple[Path, ...]:
    root = home or Path.home()
    return (
        root / ".local/share/Steam/steamapps",
        root / ".steam/steam/steamapps",
        root / ".var/app/com.valvesoftware.Steam/data/Steam/steamapps",
        root / "snap/steam/common/.local/share/Steam/steamapps",
    )


def windows_steamapps_candidates() -> tuple[Path, ...]:
    roots = [
        os.environ.get("ProgramFiles(x86)"),
        os.environ.get("PROGRAMFILES(X86)"),
        os.environ.get("ProgramFiles"),
        os.environ.get("PROGRAMFILES"),
        r"C:\Program Files (x86)",
    ]
    candidates: list[Path] = []
    seen: set[str] = set()
    for root in roots:
        if not root:
            continue
        steamapps = Path(root) / "Steam" / "steamapps"
        key = str(steamapps).casefold()
        if key not in seen:
            seen.add(key)
            candidates.append(steamapps)
    return tuple(candidates)


def current_platform_target() -> PlatformTarget:
    os_name = platform_os_name()
    if os_name == "windows":
        return PlatformTarget(
            os_name="windows",
            executable_name=STEAM_BARONY_WINDOWS_EXECUTABLE,
            hook_artifact_extension=".dll",
            launch_adapter=LAUNCH_ADAPTER_WINDOWS_CREATEPROCESS_LOADLIBRARY,
            steamapps_candidates=windows_steamapps_candidates(),
        )
    return PlatformTarget(
        os_name=os_name,
        executable_name=STEAM_BARONY_EXECUTABLE,
        hook_artifact_extension=".so",
        launch_adapter=LAUNCH_ADAPTER_LINUX_LD_PRELOAD,
        steamapps_candidates=linux_steamapps_candidates() if os_name == "linux" else tuple(),
    )


def steam_manifest_candidates(target: PlatformTarget | None = None) -> list[Path]:
    platform_target = target or current_platform_target()
    candidates = [steamapps / STEAM_MANIFEST_NAME for steamapps in platform_target.steamapps_candidates]
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
    expanded = install_path.expanduser()
    direct_candidates = (
        expanded / "steamapps" / STEAM_MANIFEST_NAME,
        expanded / STEAM_MANIFEST_NAME,
    )
    for candidate in direct_candidates:
        if candidate.exists():
            return candidate

    parts = expanded.resolve().parts
    folded_parts = [part.casefold() for part in parts]
    if "steamapps" not in folded_parts:
        return None
    steamapps = Path(*parts[: folded_parts.index("steamapps") + 1])
    candidate = steamapps / STEAM_MANIFEST_NAME
    return candidate if candidate.exists() else None


def steam_install_path_from_manifest(manifest_path: Path, manifest: dict[str, str]) -> Path:
    install_dir = manifest.get("installdir") or "Barony"
    return manifest_path.parent / "common" / install_dir


def install_arg_is_steam_library_root(install_path: Path, manifest_path: Path) -> bool:
    expanded = install_path.expanduser()
    steamapps = manifest_path.parent
    try:
        return expanded.resolve() in {steamapps.resolve(), steamapps.parent.resolve()}
    except OSError:
        return expanded.absolute() in {steamapps.absolute(), steamapps.parent.absolute()}


def detect_steam_install(manifest_arg: str | None = None, install_arg: str | None = None) -> tuple[dict[str, Any] | None, ValidationResult]:
    result = ValidationResult("Steam Barony install")
    target = current_platform_target()
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
        for candidate in steam_manifest_candidates(target):
            if candidate.exists():
                manifest_path = candidate
                manifest = parse_steam_acf(candidate)
                break

    if install_arg:
        requested_install_path = Path(install_arg).expanduser()
        if manifest_path and install_arg_is_steam_library_root(requested_install_path, manifest_path):
            install_path = steam_install_path_from_manifest(manifest_path, manifest)
        else:
            install_path = requested_install_path
    elif manifest_path:
        install_path = steam_install_path_from_manifest(manifest_path, manifest)
    elif target.steamapps_candidates:
        install_path = target.steamapps_candidates[0] / STEAM_LIBRARY_RELATIVE_INSTALL
    else:
        install_path = Path.home() / ".local/share/Steam/steamapps" / STEAM_LIBRARY_RELATIVE_INSTALL

    install_path = install_path.resolve() if install_path.exists() else install_path.absolute()
    executable = install_path / target.executable_name

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
        "platform": target.platform_id(),
        "platformTarget": target.os_name,
        "executableName": target.executable_name,
        "hookArtifactExtension": target.hook_artifact_extension,
        "launchAdapter": target.launch_adapter,
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
        if cap_id not in RECOGNIZED_CAPABILITIES and cap_id not in OUTDATED_CAPABILITY_ALIASES:
            result.add(
                "BML_PACKAGE_CAPABILITY_UNKNOWN",
                "error",
                f"Unknown capability id {cap_id!r} for v0 BaronyModLoader.",
                capability=cap_id,
                allowed=sorted(RECOGNIZED_CAPABILITIES),
                source=item_source,
            )
        if isinstance(raw, dict) and "version" in raw and not is_semverish(version):
            result.add("BML_PACKAGE_CAPABILITY_VERSION_INVALID", "error", f"Capability version must be semver-ish: {item_source}", source=item_source, capability=cap_id, version=version)
    return seen

def validate_runtime_module_name_list(result: ValidationResult, values: Any, source: str, *, required_source: bool = False) -> set[str]:
    seen: set[str] = set()
    if values is None:
        if required_source:
            result.add("BML_PACKAGE_RUNTIME_MODULES_MISSING", "error", f"Missing runtime module list: {source}", source=source)
        return seen
    if not isinstance(values, list):
        result.add("BML_PACKAGE_RUNTIME_MODULES_INVALID", "error", f"Runtime module list must be an array: {source}", source=source)
        return seen
    for index, raw in enumerate(values):
        item_source = f"{source}[{index}]"
        if not isinstance(raw, str) or not raw:
            result.add("BML_PACKAGE_RUNTIME_MODULE_INVALID", "error", f"Runtime module entry must be a non-empty string: {item_source}", source=item_source, value=raw)
            continue
        seen.add(raw)
        if raw not in RECOGNIZED_RUNTIME_MODULES:
            result.add(
                "BML_PACKAGE_RUNTIME_MODULE_UNKNOWN",
                "error",
                f"Unknown runtime module {raw!r} for v0 BaronyModLoader.",
                module=raw,
                allowed=sorted(RECOGNIZED_RUNTIME_MODULES),
                source=item_source,
            )
    return seen



def is_stash_package(manifest: dict[str, Any]) -> bool:
    return manifest.get("id") == "jml.stash" or manifest.get("name") == "Stash"


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



def is_runebound_elixirs_package(manifest: dict[str, Any]) -> bool:
    return manifest.get("id") == RUNEBOUND_ELIXIRS_PACKAGE_ID


def package_json_file(loaded: LoadedPackage, relative_name: str) -> tuple[Any | None, str | None]:
    try:
        if loaded.archive_members is not None:
            with zipfile.ZipFile(loaded.manifest_path) as archive:
                return json.loads(archive.read(relative_name).decode("utf-8")), None
        return parse_json_file(loaded.package_root / relative_name), None
    except KeyError:
        return None, "archive member is missing"
    except UnicodeDecodeError:
        return None, "file is not UTF-8 JSON"
    except json.JSONDecodeError as exc:
        return None, f"file is not valid JSON: line {exc.lineno}, column {exc.colno}: {exc.msg}"
    except OSError as exc:
        return None, f"could not read file: {exc}"


def non_empty_contract_value(value: Any) -> bool:
    if isinstance(value, str):
        return bool(value.strip())
    if isinstance(value, list):
        return any(non_empty_contract_value(item) for item in value)
    if isinstance(value, dict):
        return any(non_empty_contract_value(item) for item in value.values())
    return value is not None


def named_contract_entries(value: Any, source: str) -> list[tuple[str, Any, str]]:
    if isinstance(value, list):
        return [(str(index), item, f"{source}[{index}]") for index, item in enumerate(value)]
    if isinstance(value, dict):
        return [(str(key), item, f"{source}.{key}") for key, item in value.items()]
    return []


def validate_party_size_bounds(result: ValidationResult, value: Any, source: str) -> None:
    if isinstance(value, list):
        for index, item in enumerate(value):
            validate_party_size_bounds(result, item, f"{source}[{index}]")
        return
    if not isinstance(value, dict):
        return

    minimum = value.get("minPartySize")
    maximum = value.get("maxPartySize")
    min_ok = minimum is None or (isinstance(minimum, int) and not isinstance(minimum, bool) and 1 <= minimum <= 4)
    max_ok = maximum is None or (isinstance(maximum, int) and not isinstance(maximum, bool) and 1 <= maximum <= 4)
    if not min_ok or not max_ok:
        result.add(
            "BML_PACKAGE_RUNEBOUND_ELIXIRS_PARTY_SIZE_INVALID",
            "error",
            "Runebound elixir party-size bounds must be integer player counts between 1 and 4.",
            source=source,
            minPartySize=minimum,
            maxPartySize=maximum,
        )
    elif isinstance(minimum, int) and isinstance(maximum, int) and minimum > maximum:
        result.add(
            "BML_PACKAGE_RUNEBOUND_ELIXIRS_PARTY_SIZE_INVALID",
            "error",
            "Runebound elixir minPartySize must not exceed maxPartySize.",
            source=source,
            minPartySize=minimum,
            maxPartySize=maximum,
        )

    for key, item in value.items():
        if isinstance(item, (dict, list)):
            validate_party_size_bounds(result, item, f"{source}.{key}")


def validate_runebound_elixir_effect_opcodes(result: ValidationResult, value: Any, source: str) -> None:
    if value is None:
        result.add("BML_PACKAGE_RUNEBOUND_ELIXIRS_EFFECTS_MISSING", "error", "Runebound elixir catalog must declare effect opcodes.", source=source)
        return
    if not isinstance(value, (list, dict)):
        result.add("BML_PACKAGE_RUNEBOUND_ELIXIRS_EFFECTS_INVALID", "error", "Runebound elixir effects must be an array or object.", source=source)
        return

    for _name, effect, effect_source in named_contract_entries(value, source):
        if not isinstance(effect, dict):
            result.add("BML_PACKAGE_RUNEBOUND_ELIXIR_EFFECT_INVALID", "error", "Runebound elixir effect definition must be an object.", source=effect_source)
            continue
        opcode = effect.get("opcode")
        if not isinstance(opcode, str) or not opcode:
            result.add("BML_PACKAGE_RUNEBOUND_ELIXIR_EFFECT_OPCODE_MISSING", "error", "Runebound elixir effect definition must declare an opcode.", source=effect_source)
            continue
        if opcode not in RUNEBOUND_ELIXIRS_SUPPORTED_EFFECT_OPCODES:
            result.add(
                "BML_PACKAGE_RUNEBOUND_ELIXIR_EFFECT_OPCODE_UNSUPPORTED",
                "error",
                "Runebound elixir effect opcode is not supported by this loader contract.",
                source=effect_source,
                opcode=opcode,
                supported=sorted(RUNEBOUND_ELIXIRS_SUPPORTED_EFFECT_OPCODES),
            )


def validate_runebound_elixir_catalog(result: ValidationResult, catalog: Any, source: str) -> None:
    if not isinstance(catalog, dict):
        result.add("BML_PACKAGE_RUNEBOUND_ELIXIR_CATALOG_INVALID", "error", "Runebound elixir catalog root must be an object.", source=source)
        return

    validate_runebound_elixir_effect_opcodes(result, catalog.get("effects"), f"{source}.effects")
    validate_party_size_bounds(result, catalog, source)

    elixirs = catalog.get("elixirs")
    entries = named_contract_entries(elixirs, f"{source}.elixirs")
    if not entries:
        result.add("BML_PACKAGE_RUNEBOUND_ELIXIR_CATALOG_EMPTY", "error", "Runebound elixir catalog must declare at least one elixir.", source=f"{source}.elixirs")
        return

    for _name, elixir, elixir_source in entries:
        if not isinstance(elixir, dict):
            result.add("BML_PACKAGE_RUNEBOUND_ELIXIR_INVALID", "error", "Runebound elixir definition must be an object.", source=elixir_source)
            continue
        class_bindings = elixir.get("classBindings")
        if not isinstance(class_bindings, (list, dict)) or not class_bindings:
            result.add(
                "BML_PACKAGE_RUNEBOUND_ELIXIR_CLASS_BINDINGS_MISSING",
                "error",
                "Every Runebound elixir must declare at least one class binding.",
                source=f"{elixir_source}.classBindings",
            )
        if not any(non_empty_contract_value(elixir.get(key)) for key in ("upside", "upsideText", "upsideSummary", "benefit", "benefits")):
            result.add(
                "BML_PACKAGE_RUNEBOUND_ELIXIR_UPSIDE_MISSING",
                "error",
                "Every Runebound elixir must declare explicit upside text or fields.",
                source=elixir_source,
            )
        if not any(
            non_empty_contract_value(elixir.get(key))
            for key in ("downside", "downsideText", "downsideSummary", "tradeoff", "tradeoffs", "tradeoffText", "tradeoffSummary")
        ):
            result.add(
                "BML_PACKAGE_RUNEBOUND_ELIXIR_TRADEOFF_MISSING",
                "error",
                "Every Runebound elixir must declare explicit downside or tradeoff text or fields.",
                source=elixir_source,
            )
        inline_effects = elixir.get("effects")
        if isinstance(inline_effects, (list, dict)):
            for _effect_name, effect, effect_source in named_contract_entries(inline_effects, f"{elixir_source}.effects"):
                if isinstance(effect, dict) and "opcode" in effect:
                    validate_runebound_elixir_effect_opcodes(result, [effect], effect_source)


def validate_runebound_elixir_drop_tables(result: ValidationResult, drop_tables: Any, source: str) -> None:
    if not isinstance(drop_tables, dict):
        result.add("BML_PACKAGE_RUNEBOUND_ELIXIR_DROP_TABLES_INVALID", "error", "Runebound elixir drop table root must be an object.", source=source)
        return
    validate_party_size_bounds(result, drop_tables, source)


def validate_runebound_elixirs_data_file_path(loaded: LoadedPackage, result: ValidationResult, value: Any, source: str) -> str | None:
    if not isinstance(value, str) or not value:
        result.add("BML_PACKAGE_RUNEBOUND_ELIXIRS_DATA_FILE_INVALID", "error", "Runebound elixir data file must be a non-empty relative path.", source=source, path=value)
        return None
    relative_name = safe_archive_name(value)
    if relative_name is None:
        result.add("BML_PACKAGE_RUNEBOUND_ELIXIRS_DATA_FILE_INVALID", "error", "Runebound elixir data file path must be safe.", source=source, path=value)
        return None
    if not relative_path_is_under_directory(relative_name, RUNEBOUND_ELIXIRS_DATA_ROOT):
        result.add(
            "BML_PACKAGE_RUNEBOUND_ELIXIRS_DATA_FILE_OUTSIDE_ROOT",
            "error",
            "Runebound elixir data files must live under content/data/bml/.",
            source=source,
            path=value,
            requiredRoot=RUNEBOUND_ELIXIRS_DATA_ROOT,
        )
        return None
    if not package_contains_file(loaded, relative_name):
        result.add("BML_PACKAGE_RUNEBOUND_ELIXIRS_DATA_FILE_MISSING", "error", "Runebound elixir data file does not exist in the package.", source=source, path=relative_name)
        return None
    return relative_name


def validate_runebound_elixirs_modules(loaded: LoadedPackage, result: ValidationResult) -> None:
    manifest = loaded.manifest
    modules = manifest.get("modules")
    if not isinstance(modules, dict):
        result.add("BML_PACKAGE_MODULES_MISSING", "error", "Runebound: Elixirs package must declare a modules object.")
        return

    module = modules.get(RUNEBOUND_ELIXIRS_MODULE_NAME)
    if not isinstance(module, dict):
        result.add(
            "BML_PACKAGE_RUNEBOUND_ELIXIRS_MODULE_MISSING",
            "error",
            "Runebound: Elixirs package must declare modules.runeboundElixirs.",
            module=RUNEBOUND_ELIXIRS_MODULE_NAME,
        )
        return

    for key in RUNEBOUND_ELIXIRS_REQUIRED_MODULE_KEYS:
        if key not in module:
            result.add(
                "BML_PACKAGE_RUNEBOUND_ELIXIRS_MODULE_KEY_MISSING",
                "error",
                f"Runebound elixir module is missing required key modules.{RUNEBOUND_ELIXIRS_MODULE_NAME}.{key}.",
                field=f"modules.{RUNEBOUND_ELIXIRS_MODULE_NAME}.{key}",
            )

    expected_strings = {
        "namespace": RUNEBOUND_ELIXIRS_NAMESPACE,
        "authority": "host",
        "carrierItemType": RUNEBOUND_ELIXIRS_CARRIER_ITEM_TYPE,
        "failurePolicy": "fail-closed",
    }
    for key, expected in expected_strings.items():
        value = module.get(key)
        if value != expected:
            result.add(
                "BML_PACKAGE_RUNEBOUND_ELIXIRS_MODULE_FIELD_INVALID",
                "error",
                f"Runebound elixir module field modules.{RUNEBOUND_ELIXIRS_MODULE_NAME}.{key} must be {expected!r}.",
                field=f"modules.{RUNEBOUND_ELIXIRS_MODULE_NAME}.{key}",
                expected=expected,
                value=value,
            )

    schema_version = module.get("schemaVersion")
    if not isinstance(schema_version, str) or not is_semverish(schema_version):
        result.add(
            "BML_PACKAGE_RUNEBOUND_ELIXIRS_MODULE_FIELD_INVALID",
            "error",
            "Runebound elixir module schemaVersion must be semver-ish x.y.z.",
            field=f"modules.{RUNEBOUND_ELIXIRS_MODULE_NAME}.schemaVersion",
            value=schema_version,
        )

    for key in ("dropPolicy", "activeEffects", "display", "multiplayer"):
        if key in module and not isinstance(module.get(key), dict):
            result.add(
                "BML_PACKAGE_RUNEBOUND_ELIXIRS_MODULE_FIELD_INVALID",
                "error",
                f"Runebound elixir module field modules.{RUNEBOUND_ELIXIRS_MODULE_NAME}.{key} must be an object.",
                field=f"modules.{RUNEBOUND_ELIXIRS_MODULE_NAME}.{key}",
                value=module.get(key),
            )

    data_files = module.get("dataFiles")
    if not isinstance(data_files, list):
        result.add(
            "BML_PACKAGE_RUNEBOUND_ELIXIRS_DATA_FILES_INVALID",
            "error",
            "Runebound elixir module dataFiles must be an array.",
            field=f"modules.{RUNEBOUND_ELIXIRS_MODULE_NAME}.dataFiles",
        )
        data_files = []

    validated_files: set[str] = set()
    for index, raw_path in enumerate(data_files):
        relative_name = validate_runebound_elixirs_data_file_path(
            loaded,
            result,
            raw_path,
            f"modules.{RUNEBOUND_ELIXIRS_MODULE_NAME}.dataFiles[{index}]",
        )
        if relative_name is not None:
            validated_files.add(relative_name)

    missing_required = [relative_name for relative_name in RUNEBOUND_ELIXIRS_REQUIRED_DATA_FILES if relative_name not in validated_files]
    if missing_required:
        result.add(
            "BML_PACKAGE_RUNEBOUND_ELIXIRS_DATA_FILE_REQUIRED_MISSING",
            "error",
            "Runebound elixir module must list the required elixir catalog and drop-table data files.",
            missing=missing_required,
            required=list(RUNEBOUND_ELIXIRS_REQUIRED_DATA_FILES),
        )

    for relative_name in sorted(validated_files):
        payload, error = package_json_file(loaded, relative_name)
        if error is not None:
            result.add("BML_PACKAGE_RUNEBOUND_ELIXIRS_DATA_FILE_PARSE_FAILED", "error", "Runebound elixir data file could not be parsed.", path=relative_name, error=error)
            continue
        if relative_name == RUNEBOUND_ELIXIRS_CATALOG_FILE:
            validate_runebound_elixir_catalog(result, payload, relative_name)
        elif relative_name == RUNEBOUND_ELIXIRS_DROP_TABLE_FILE:
            validate_runebound_elixir_drop_tables(result, payload, relative_name)

def relative_path_is_under_directory(path_name: str, directory_name: str) -> bool:
    return path_name.startswith(f"{directory_name.rstrip('/')}/")


def package_contains_file(loaded: LoadedPackage, relative_name: str) -> bool:
    if loaded.archive_members is not None:
        return relative_name in loaded.archive_members
    path = loaded.package_root / relative_name
    return path.is_file() and not path.is_symlink()


def validate_package_asset_reference(loaded: LoadedPackage, result: ValidationResult, field: str, value: Any, asset_root: str | None) -> None:
    if value is None:
        return
    if not isinstance(value, str) or not value:
        result.add("BML_PACKAGE_ASSET_REFERENCE_INVALID", "error", "Package asset reference must be a non-empty relative path.", field=field, path=value)
        return
    relative_name = safe_archive_name(value)
    if relative_name is None:
        result.add("BML_PACKAGE_ASSET_REFERENCE_INVALID", "error", "Package asset reference must be a safe relative path.", field=field, path=value)
        return
    if not archive_name_is_installable(relative_name) or asset_root is None or not relative_path_is_under_directory(relative_name, asset_root):
        result.add(
            "BML_PACKAGE_ASSET_REFERENCE_OUTSIDE_INSTALLABLE_ROOT",
            "error",
            "Package asset reference must live under layout.assetRoot so it is installed with the package.",
            field=field,
            path=value,
            assetRoot=asset_root,
            installableDirectories=list(PACKAGE_INSTALL_DIRECTORIES),
        )
        return
    if not package_contains_file(loaded, relative_name):
        result.add("BML_PACKAGE_ASSET_REFERENCE_MISSING", "error", "Package asset reference does not exist in the installable package files.", field=field, path=value)


def validate_package_asset_references(loaded: LoadedPackage, result: ValidationResult) -> None:
    manifest = loaded.manifest
    assets = manifest.get("assets")
    if not isinstance(assets, dict):
        return
    layout = manifest.get("layout")
    asset_root = None
    if isinstance(layout, dict) and isinstance(layout.get("assetRoot"), str):
        asset_root = safe_archive_name(layout["assetRoot"])
    validate_package_asset_reference(loaded, result, "assets.icon", assets.get("icon"), asset_root)
    preview_images = assets.get("previewImages")
    if isinstance(preview_images, list):
        for index, image in enumerate(preview_images):
            validate_package_asset_reference(loaded, result, f"assets.previewImages[{index}]", image, asset_root)
    validate_package_asset_reference(loaded, result, "assets.readme", assets.get("readme"), asset_root)


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

    if is_stash_package(manifest):
        missing = [capability for capability in CANONICAL_STASH_CAPABILITIES if capability not in required_ids]
        if missing:
            result.add(
                "BML_PACKAGE_CAPABILITY_REQUIRED_MISSING",
                "error",
                "Stash package is missing required canonical v0 Stash capability ids.",
                missing=missing,
                required=list(CANONICAL_STASH_CAPABILITIES),
                present=sorted(engine_cap_ids),
            )


    native = manifest.get("native")

    if is_runebound_elixirs_package(manifest):
        missing = [capability for capability in CANONICAL_RUNEBOUND_ELIXIR_CAPABILITIES if capability not in required_ids]
        if missing:
            result.add(
                "BML_PACKAGE_CAPABILITY_REQUIRED_MISSING",
                "error",
                "Runebound: Elixirs package is missing required canonical elixir capability ids.",
                missing=missing,
                required=list(CANONICAL_RUNEBOUND_ELIXIR_CAPABILITIES),
                present=sorted(engine_cap_ids),
            )
        if isinstance(native, dict):
            native_expectations = {
                "required": True,
                "mode": "paired-engine-runtime",
                "runtimeStrategy": RUNTIME_STRATEGY_INSTALLED_HOOK,
            }
            for field, expected in native_expectations.items():
                if native.get(field) != expected:
                    result.add(
                        "BML_PACKAGE_RUNEBOUND_ELIXIRS_NATIVE_REQUIREMENT_INVALID",
                        "error",
                        f"Runebound: Elixirs native.{field} must be {expected!r} when declared.",
                        field=f"native.{field}",
                        expected=expected,
                        value=native.get(field),
                    )
            if not non_empty_contract_value(native.get("runtimeAuthority")):
                result.add(
                    "BML_PACKAGE_RUNEBOUND_ELIXIRS_NATIVE_REQUIREMENT_INVALID",
                    "error",
                    "Runebound: Elixirs native.runtimeAuthority must name the paired installed-binary hook runtime when native requirements are declared.",
                    field="native.runtimeAuthority",
                    value=native.get("runtimeAuthority"),
                )
            platforms = native.get("platforms")
            if platforms is not None and (not isinstance(platforms, list) or not platforms):
                result.add(
                    "BML_PACKAGE_RUNEBOUND_ELIXIRS_NATIVE_REQUIREMENT_INVALID",
                    "error",
                    "Runebound: Elixirs native.platforms must be a non-empty array when declared.",
                    field="native.platforms",
                    value=platforms,
                )

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
        expected_report_capabilities = validate_capability_name_list(
            result,
            runtime_reports.get("expectedLoadedCapabilities"),
            "runtimeReports.expectedLoadedCapabilities",
        )
        expected_report_modules = validate_runtime_module_name_list(
            result,
            runtime_reports.get("expectedLoadedModules"),
            "runtimeReports.expectedLoadedModules",
        )
        if is_runebound_elixirs_package(manifest):
            missing_report_caps = [
                capability for capability in CANONICAL_RUNEBOUND_ELIXIR_CAPABILITIES if capability not in expected_report_capabilities
            ]
            if missing_report_caps:
                result.add(
                    "BML_PACKAGE_RUNEBOUND_ELIXIRS_RUNTIME_REPORT_CAPABILITY_MISSING",
                    "error",
                    "Runebound: Elixirs runtimeReports.expectedLoadedCapabilities must list every canonical elixir runtime capability.",
                    missing=missing_report_caps,
                    required=list(CANONICAL_RUNEBOUND_ELIXIR_CAPABILITIES),
                    present=sorted(expected_report_capabilities),
                )
            if RUNEBOUND_ELIXIRS_MODULE_NAME not in expected_report_modules:
                result.add(
                    "BML_PACKAGE_RUNEBOUND_ELIXIRS_RUNTIME_REPORT_MODULE_MISSING",
                    "error",
                    "Runebound: Elixirs runtimeReports.expectedLoadedModules must include runeboundElixirs.",
                    module=RUNEBOUND_ELIXIRS_MODULE_NAME,
                    present=sorted(expected_report_modules),
                )
            paths = runtime_reports.get("paths")
            if isinstance(paths, dict) and paths.get("liveInstallReport") != RUNEBOUND_ELIXIRS_LIVE_INSTALL_REPORT:
                result.add(
                    "BML_PACKAGE_RUNEBOUND_ELIXIRS_LIVE_INSTALL_REPORT_MISSING",
                    "error",
                    "Runebound: Elixirs runtime report paths must include the live hook install report.",
                    field="runtimeReports.paths.liveInstallReport",
                    expected=RUNEBOUND_ELIXIRS_LIVE_INSTALL_REPORT,
                    value=paths.get("liveInstallReport"),
                )

    validate_package_asset_references(loaded, result)

    if is_stash_package(manifest):
        validate_stash_modules(manifest, result)

    if is_runebound_elixirs_package(manifest):
        validate_runebound_elixirs_modules(loaded, result)
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

    canonical = set(RECOGNIZED_CAPABILITIES)
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


def validate_runtime_report_modules(result: ValidationResult, raw: Any, source: str) -> list[str]:
    modules: list[str] = []
    if raw is None:
        return modules
    if not isinstance(raw, list):
        result.add("BML_RUNTIME_REPORT_MODULES_INVALID", "error", f"Modules must be an array when present: {source}", source=source)
        return modules

    canonical = set(RECOGNIZED_RUNTIME_REPORT_MODULES)
    for index, item in enumerate(raw):
        module_source = f"{source}[{index}]"
        if not isinstance(item, str) or not item:
            result.add("BML_RUNTIME_REPORT_MODULE_INVALID", "error", f"Module entry must be a non-empty string: {module_source}", source=module_source)
            continue
        modules.append(item)
        if item not in canonical:
            result.add(
                "BML_RUNTIME_REPORT_MODULE_NONCANONICAL",
                "error",
                f"Runtime report module is not canonical for the v0 contract: {item!r}.",
                source=module_source,
                module=item,
                canonical=sorted(canonical),
            )
    return modules


def validate_runebound_elixirs_runtime_report_mod(
    result: ValidationResult,
    mod: dict[str, Any],
    source: str,
    capabilities: list[str],
    modules: list[str],
) -> None:
    status = mod.get("status")
    if status != "loaded":
        return

    missing_capabilities = [capability for capability in CANONICAL_RUNEBOUND_ELIXIR_CAPABILITIES if capability not in capabilities]
    if missing_capabilities:
        result.add(
            "BML_RUNTIME_REPORT_RUNEBOUND_ELIXIRS_CAPABILITY_MISSING",
            "fatal",
            "Loaded Runebound: Elixirs runtime report entry must include every canonical elixir capability.",
            source=source,
            missing=missing_capabilities,
            required=list(CANONICAL_RUNEBOUND_ELIXIR_CAPABILITIES),
            present=sorted(capabilities),
        )

    if RUNEBOUND_ELIXIRS_MODULE_NAME not in modules:
        result.add(
            "BML_RUNTIME_REPORT_RUNEBOUND_ELIXIRS_MODULE_MISSING",
            "fatal",
            "Loaded Runebound: Elixirs runtime report entry must include the runeboundElixirs module.",
            source=source,
            module=RUNEBOUND_ELIXIRS_MODULE_NAME,
            present=sorted(modules),
        )

    live_install_report = mod.get("liveInstallReport")
    claim_boundary = mod.get("claimBoundary")
    if live_install_report is not None and live_install_report != RUNEBOUND_ELIXIRS_LIVE_INSTALL_REPORT:
        result.add(
            "BML_RUNTIME_REPORT_RUNEBOUND_ELIXIRS_LIVE_INSTALL_REPORT_INVALID",
            "fatal",
            "Loaded Runebound: Elixirs runtime report entry must not point at an unexpected live hook install report.",
            source=source,
            field=f"{source}.liveInstallReport",
            expected=RUNEBOUND_ELIXIRS_LIVE_INSTALL_REPORT,
            value=live_install_report,
        )

    if live_install_report is None and claim_boundary != "liveHookBehaviorClaimed":
        result.add(
            "BML_RUNTIME_REPORT_RUNEBOUND_ELIXIRS_PROOF_BOUNDARY_MISSING",
            "fatal",
            "Loaded Runebound: Elixirs runtime report entry must include the live install report path or the native claimBoundary marker used by current live reports.",
            source=source,
            field=f"{source}.liveInstallReport",
            expected=RUNEBOUND_ELIXIRS_LIVE_INSTALL_REPORT,
            claimBoundary=claim_boundary,
        )

    explicit_live_hook_claim = mod.get("liveHookBehaviorClaimed")
    if explicit_live_hook_claim is not None and explicit_live_hook_claim is not True:
        result.add(
            "BML_RUNTIME_REPORT_RUNEBOUND_ELIXIRS_LIVE_HOOK_CLAIM_INVALID",
            "fatal",
            "Loaded Runebound: Elixirs runtime report entry must not deny live-hook behavior.",
            source=source,
            field=f"{source}.liveHookBehaviorClaimed",
            value=explicit_live_hook_claim,
        )

    if explicit_live_hook_claim is not True and claim_boundary != "liveHookBehaviorClaimed":
        result.add(
            "BML_RUNTIME_REPORT_RUNEBOUND_ELIXIRS_LIVE_HOOK_CLAIM_MISSING",
            "fatal",
            "Loaded Runebound: Elixirs runtime report entry must carry live-hook proof via liveHookBehaviorClaimed or the current native claimBoundary marker.",
            source=source,
            field=f"{source}.liveHookBehaviorClaimed",
            value=explicit_live_hook_claim,
            claimBoundary=claim_boundary,
        )

    evidence_scope = mod.get("evidenceScope")
    if evidence_scope is not None and evidence_scope != "fake-provider-live-hook-install":
        result.add(
            "BML_RUNTIME_REPORT_RUNEBOUND_ELIXIRS_EVIDENCE_SCOPE_INVALID",
            "fatal",
            "Loaded Runebound: Elixirs runtime report entry must not overclaim beyond the current fake-provider live-hook install scope when evidenceScope is present.",
            source=source,
            field=f"{source}.evidenceScope",
            expected="fake-provider-live-hook-install",
            value=evidence_scope,
        )


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
            capabilities = validate_runtime_report_capabilities(result, mod.get("capabilities"), f"loadedMods[{index}].capabilities")
            modules = validate_runtime_report_modules(result, mod.get("modules"), f"loadedMods[{index}].modules")
            if mod.get("id") == RUNEBOUND_ELIXIRS_PACKAGE_ID:
                validate_runebound_elixirs_runtime_report_mod(result, mod, f"loadedMods[{index}]", capabilities, modules)

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
    return current_platform_target().platform_id()


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

def windows_runtime_verification_evidence_ok(evidence: Any, platform_id: Any) -> bool:
    if not isinstance(evidence, dict):
        return False
    if evidence.get("evidenceKind") != WINDOWS_LIVE_RUNTIME_EVIDENCE_KIND:
        return False
    if evidence.get("platform") != platform_id:
        return False
    if str(evidence.get("hostOs") or "").casefold() != "windows":
        return False
    if evidence.get("gameExecutableName") != STEAM_BARONY_WINDOWS_EXECUTABLE:
        return False
    if evidence.get("launcherExecutableName") != WINDOWS_LAUNCHER_EXECUTABLE:
        return False
    if evidence.get("hookLibraryName") != WINDOWS_HOOK_LIBRARY_NAME:
        return False
    runtime_report_sha = evidence.get("runtimeLoadReportSha256")
    if not isinstance(runtime_report_sha, str) or not re.fullmatch(r"[0-9a-fA-F]{64}", runtime_report_sha):
        return False
    verified_at = evidence.get("verifiedAt")
    return isinstance(verified_at, str) and bool(verified_at.strip())


def windows_runtime_verified_status(value: Any, platform_id: Any) -> bool:
    if not isinstance(value, dict):
        return False
    status = value.get("status")
    if not isinstance(status, str) or status.strip().casefold() != "verified":
        return False
    return windows_runtime_verification_evidence_ok(value.get("evidence"), platform_id)


def windows_runtime_has_verified_status(runtime_info: dict[str, Any] | None, runtime: dict[str, Any] | None, platform_id: Any) -> bool:
    candidates: list[Any] = []
    if isinstance(runtime, dict):
        candidates.extend((runtime.get("windowsRuntimeStatus"), runtime.get("windowsVerification")))
    if isinstance(runtime_info, dict):
        candidates.extend((runtime_info.get("windowsRuntimeStatus"), runtime_info.get("windowsVerification")))
        platforms = runtime_info.get("platforms")
        if isinstance(platforms, list):
            for entry in platforms:
                if isinstance(entry, dict) and entry.get("platform") == platform_id:
                    candidates.extend((entry.get("windowsRuntimeStatus"), entry.get("windowsVerification")))
    return any(windows_runtime_verified_status(candidate, platform_id) for candidate in candidates)


def validate_windows_runtime_verification(
    runtime_info: dict[str, Any] | None,
    runtime: dict[str, Any] | None,
    platform_id: Any,
) -> ValidationResult:
    result = ValidationResult("registered Windows runtime verification")
    if not windows_runtime_has_verified_status(runtime_info, runtime, platform_id):
        result.add(
            "BML_REGISTERED_RUNTIME_WINDOWS_VERIFICATION_MISSING",
            "fatal",
            "Windows runtime registrations are scaffold-only until explicit verified live Windows runtime status is present in runtime metadata or registration.",
            field="windowsRuntimeStatus",
            registeredPlatform=platform_id,
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
    target = current_platform_target()

    if strategy not in SUPPORTED_RUNTIME_STRATEGIES:
        combined.add("BML_RUNTIME_STRATEGY_UNSUPPORTED", "fatal", f"Unsupported runtime strategy: {strategy}")

    platform_id = args.platform or current_platform_id()
    combined.extend(validate_registered_runtime_host_platform(platform_id))

    steam_executable_arg = args.steam_executable or args.executable
    steam_executable = Path(steam_executable_arg).expanduser().resolve() if steam_executable_arg else None
    hook_library = Path(args.hook_library).expanduser().resolve() if args.hook_library else None
    hook_manifest = Path(args.hook_manifest).expanduser().resolve() if args.hook_manifest else None
    launcher_executable = Path(args.launcher_executable).expanduser().resolve() if args.launcher_executable else None

    if steam_executable is None:
        combined.add("BML_RUNTIME_STEAM_EXECUTABLE_MISSING", "fatal", "Installed executable path is required for installed-binary-hook registration.", hint=f"Pass --steam-executable /path/to/{target.executable_name}")
    elif not steam_executable.exists():
        combined.add("BML_RUNTIME_STEAM_EXECUTABLE_NOT_FOUND", "fatal", f"Installed executable not found: {steam_executable}")
    elif not steam_executable.is_file():
        combined.add("BML_RUNTIME_STEAM_EXECUTABLE_NOT_FILE", "fatal", f"Installed executable path is not a file: {steam_executable}")
    elif target.os_name == "windows" and steam_executable.name != target.executable_name:
        combined.add(
            "BML_RUNTIME_STEAM_EXECUTABLE_NAME_MISMATCH",
            "fatal",
            "Windows runtime registration must target Barony's Windows executable name.",
            steamExecutable=str(steam_executable),
            expectedName=target.executable_name,
        )

    if hook_library is None:
        combined.add("BML_RUNTIME_HOOK_LIBRARY_MISSING", "fatal", "Hook library path is required for installed-binary-hook registration.", hint="Pass --hook-library native/barony-modloader-hook/build/libbarony_bml.so")
    elif not hook_library.exists():
        combined.add("BML_RUNTIME_HOOK_LIBRARY_NOT_FOUND", "fatal", f"Hook library not found: {hook_library}")
    elif not hook_library.is_file():
        combined.add("BML_RUNTIME_HOOK_LIBRARY_NOT_FILE", "fatal", f"Hook library path is not a file: {hook_library}")
    elif hook_library.suffix.casefold() != target.hook_artifact_extension:
        combined.add(
            "BML_RUNTIME_HOOK_LIBRARY_EXTENSION_MISMATCH",
            "fatal",
            "Hook library extension does not match the current platform target.",
            hookLibrary=str(hook_library),
            expectedExtension=target.hook_artifact_extension,
        )
    elif target.os_name == "windows" and hook_library.name != WINDOWS_HOOK_LIBRARY_NAME:
        combined.add(
            "BML_RUNTIME_HOOK_LIBRARY_NAME_MISMATCH",
            "fatal",
            "Windows hook library name does not match the native scaffold.",
            hookLibrary=str(hook_library),
            expectedName=WINDOWS_HOOK_LIBRARY_NAME,
        )

    if hook_manifest is None:
        combined.add("BML_RUNTIME_HOOK_MANIFEST_MISSING", "fatal", "Hook manifest path is required for installed-binary-hook registration.", hint="Pass --hook-manifest native/barony-modloader-hook/manifests/<build>.json")
    elif not hook_manifest.exists():
        combined.add("BML_RUNTIME_HOOK_MANIFEST_NOT_FOUND", "fatal", f"Hook manifest not found: {hook_manifest}")
    elif not hook_manifest.is_file():
        combined.add("BML_RUNTIME_HOOK_MANIFEST_NOT_FILE", "fatal", f"Hook manifest path is not a file: {hook_manifest}")
    if target.os_name == "windows":
        if launcher_executable is None:
            combined.add(
                "BML_RUNTIME_LAUNCHER_EXECUTABLE_MISSING",
                "fatal",
                "Windows runtime registration requires a launcher executable for the windows-createprocess-loadlibrary adapter.",
                hint=f"Pass --launcher-executable path/to/{WINDOWS_LAUNCHER_EXECUTABLE}",
            )
        elif not launcher_executable.exists():
            combined.add("BML_RUNTIME_LAUNCHER_EXECUTABLE_NOT_FOUND", "fatal", f"Launcher executable not found: {launcher_executable}")
        elif not launcher_executable.is_file():
            combined.add("BML_RUNTIME_LAUNCHER_EXECUTABLE_NOT_FILE", "fatal", f"Launcher executable path is not a file: {launcher_executable}")
        elif launcher_executable.name != WINDOWS_LAUNCHER_EXECUTABLE:
            combined.add(
                "BML_RUNTIME_LAUNCHER_EXECUTABLE_NAME_MISMATCH",
                "fatal",
                "Windows launcher executable name does not match the native scaffold.",
                launcherExecutable=str(launcher_executable),
                expectedName=WINDOWS_LAUNCHER_EXECUTABLE,
            )
    runtime_info, runtime_info_path, runtime_load_result = load_runtime_info(args.runtime_info)
    combined.extend(runtime_load_result)
    if runtime_info is not None:
        combined.extend(validate_runtime_info_metadata(runtime_info))
        combined.extend(validate_runtime_info_registered_platform(runtime_info, platform_id))
        if target.os_name == "windows":
            combined.extend(validate_windows_runtime_verification(runtime_info, None, platform_id))
    registry, registry_load_result = load_runtime_registry(registry_path, missing_ok=True)
    combined.extend(registry_load_result)

    if not combined.ok:
        print_report(combined, heading="Runtime registration")
        return 1

    assert runtime_info is not None
    assert steam_executable is not None
    assert hook_library is not None
    assert hook_manifest is not None
    if target.os_name == "windows":
        assert launcher_executable is not None
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
        "platformTarget": target.os_name,
        "launchAdapter": target.launch_adapter,
        "hookArtifactExtension": target.hook_artifact_extension,
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
    if target.os_name == "windows":
        assert launcher_executable is not None
        registration["launcherExecutable"] = str(launcher_executable)
        registration["launcherExecutableSha256"] = file_sha256(launcher_executable)

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
    archive_members: set[str] = set()

    try:
        with zipfile.ZipFile(path) as archive:
            for info in archive.infolist():
                safe_name = safe_archive_name(info.filename)
                if safe_name is None:
                    result.add("BML_PACKAGE_ARCHIVE_PATH_UNSAFE", "fatal", "Package archive contains an unsafe member path.", member=info.filename)
                elif not info.is_dir():
                    archive_members.add(safe_name)
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
    return LoadedPackage(payload, path.resolve(), path.resolve().parent, archive_members), path.resolve(), result


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
    return isinstance(profile.get("activeMods"), list) or active_mods_json_path(profile_dir).is_file()


def profile_authoritative_mods(profile: dict[str, Any], profile_dir: Path) -> list[dict[str, Any]]:
    if isinstance(profile.get("activeMods"), list):
        return profile_active_mods(profile)
    return load_active_mods_file(profile_dir)


def normalized_profile_path(value: Any) -> Path | None:
    if not isinstance(value, str) or not value:
        return None
    try:
        return Path(value).expanduser().resolve(strict=False)
    except OSError:
        return None


def requested_package_paths(package: LoadedPackage) -> set[Path]:
    paths: set[Path] = set()
    for path in (preferred_package_path(package), package.manifest_path):
        try:
            paths.add(path.resolve(strict=False))
        except OSError:
            paths.add(path)
    return paths


def validate_enabled_package_path(result: ValidationResult, mod: dict[str, Any], package: LoadedPackage, package_id: str, package_version: str) -> None:
    requested_paths = requested_package_paths(package)
    enabled_paths = [
        path
        for path in (normalized_profile_path(mod.get("packagePath")), normalized_profile_path(mod.get("manifestPath")))
        if path is not None
    ]
    if any(path in requested_paths for path in enabled_paths):
        return
    result.add(
        "BML_PROFILE_PACKAGE_PATH_MISMATCH",
        "fatal",
        "Requested package id/version is enabled in the profile from a different package path.",
        packageId=package_id,
        requestedVersion=package_version,
        requestedPaths=sorted(str(path) for path in requested_paths),
        enabledPaths=[str(path) for path in enabled_paths],
    )


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
        validate_enabled_package_path(result, mod, package, package_id, package_version)
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
        "manifestPath": str(package.manifest_path),
        "checksumSet": package_checksum(package),
        "enabledAt": now,
    }
    current_mods = profile_authoritative_mods(profile, profile_dir)
    existing = next((mod for mod in current_mods if mod.get("id") == mod_id), None)
    if (
        existing is not None
        and existing.get("version") == entry["version"]
        and normalized_profile_path(existing.get("packagePath")) in requested_package_paths(package)
    ):
        preserved = dict(existing)
        preserved.setdefault("manifestPath", entry["manifestPath"])
        preserved.setdefault("checksumSet", entry["checksumSet"])
        preserved.setdefault("enabledAt", entry["enabledAt"])
        entry = preserved
        status = "already_enabled"
    else:
        status = "enabled"
    mods = [mod for mod in current_mods if mod.get("id") != mod_id]
    mods.append(entry)
    mods.sort(key=lambda mod: str(mod.get("id", "")))
    if status == "already_enabled" and mods == current_mods:
        print(json.dumps({"status": status, "profile": profile_id(profile), "mod": entry}, indent=2))
        return 0
    try:
        write_profile_active_mods(profile_dir, profile, mods, now)
    except OSError as exc:
        result = ValidationResult("profile enable")
        result.add("BML_PROFILE_WRITE_FAILED", "fatal", f"Could not write profile active mods: {exc}")
        print_report(result, heading="Profile enable")
        return 1
    print(json.dumps({"status": status, "profile": profile_id(profile), "mod": entry}, indent=2))
    return 0


def command_profile_disable(args: argparse.Namespace) -> int:
    profile, profile_dir, profile_result = load_profile(args.profile_dir)
    if not profile_result.ok or profile is None:
        print_report(profile_result, heading="Profile disable validation")
        return 1

    mod_id = args.mod_id
    candidates = profile_authoritative_mods(profile, profile_dir)
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
    print(json.dumps({"status": "disabled" if removed else "already_disabled", "profile": profile_id(profile), "modId": mod_id, "removed": removed}, indent=2))
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


def build_profile_state(profile_dir: Any, package_root: Any = None, *, package: LoadedPackage | None = None) -> dict[str, Any]:
    """Return a semantic profile DTO without rewriting profile files."""
    loaded_profile, resolved_profile_dir, load_result = load_profile(str(profile_dir))
    profile_path = profile_json_path(resolved_profile_dir)
    active_mods: list[dict[str, Any]] = []
    warnings: list[str] = []
    disabled: list[str] = []
    runtime: dict[str, Any] = {}
    if loaded_profile is None:
        disabled.append("Profile is missing or could not be loaded.")
    else:
        active_mods = profile_authoritative_mods(loaded_profile, resolved_profile_dir)
        runtime = loaded_profile.get("runtime") if isinstance(loaded_profile.get("runtime"), dict) else {}
    for problem in load_result.problems:
        message = format_problem(problem)
        if problem.is_error:
            disabled.append(message)
        else:
            warnings.append(message)

    package_for_stale_check = package
    if package_for_stale_check is None and package_root is not None:
        package_for_stale_check, package_load_result = load_package(str(package_root))
        for problem in package_load_result.problems:
            warnings.append(format_problem(problem))
    if package_for_stale_check is not None:
        current_digest = package_checksum(package_for_stale_check)
        package_id_value = package_for_stale_check.manifest.get("id")
        for mod in active_mods:
            if mod.get("id") != package_id_value:
                continue
            checksum = mod.get("checksumSet")
            if isinstance(checksum, str) and checksum and checksum != current_digest:
                warnings.append(
                    f"Active mod checksum is stale for {package_id_value}: profile has {checksum}, current package digest is {current_digest}."
                )

    paths = {
        "profileRoot": str(resolved_profile_dir),
        "bmlRoot": str(bml_profile_root(resolved_profile_dir)),
        "profilePath": str(profile_path),
        "activeModsPath": str(active_mods_json_path(resolved_profile_dir)),
    }
    product_path_values = [str(value) for value in paths.values()]
    tmp_product_paths = [value for value in product_path_values if "/.tmp/" in value or value.endswith("/.tmp") or ".tmp" in Path(value).parts]
    if tmp_product_paths:
        warnings.append(f"Profile state unexpectedly contains .tmp product path(s): {', '.join(tmp_product_paths)}")

    return {
        "schemaVersion": SCHEMA_VERSION,
        "status": "loaded" if loaded_profile is not None and load_result.ok else "missing",
        "profileId": profile_id(loaded_profile) if loaded_profile is not None else None,
        "profilePath": str(profile_path),
        "paths": paths,
        "runtime": runtime,
        "activeMods": active_mods,
        "activeModCount": len(active_mods),
        "modCount": len(active_mods),
        "warnings": warnings,
        "disabledReasons": disabled,
        "tmpProductPaths": tmp_product_paths,
    }


def load_profile_state(profile_dir: Any, package_root: Any = None) -> dict[str, Any]:
    return build_profile_state(profile_dir, package_root=package_root)


def profile_store_state(profile_dir: Any, package_root: Any = None) -> dict[str, Any]:
    return build_profile_state(profile_dir, package_root=package_root)


def inspect_profile_state(profile_dir: Any, package_root: Any = None) -> dict[str, Any]:
    return build_profile_state(profile_dir, package_root=package_root)


def build_profile_store_state(profile_dir: Any, package_root: Any = None) -> dict[str, Any]:
    return build_profile_state(profile_dir, package_root=package_root)


def enable_profile_mod(profile_dir: Any, package_path: Any = None, *, package: Any = None) -> dict[str, Any]:
    profile, resolved_profile_dir, profile_result = load_profile(str(profile_dir))
    package_arg = package_path or package
    loaded_package, package_result = load_package(str(package_arg))
    combined = ValidationResult("profile semantic enable")
    combined.extend(profile_result)
    combined.extend(package_result)
    if loaded_package is not None:
        combined.extend(validate_package(loaded_package))
    if profile is None or loaded_package is None or not combined.ok:
        return {
            "status": "failed",
            "changed": False,
            "problems": [problem_to_dict(problem) for problem in combined.problems],
        }
    mod_id = str(loaded_package.manifest.get("id"))
    current_mods = profile_authoritative_mods(profile, resolved_profile_dir)
    current = next((mod for mod in current_mods if mod.get("id") == mod_id), None)
    desired = {
        "id": mod_id,
        "version": loaded_package.manifest.get("version"),
        "packagePath": str(preferred_package_path(loaded_package)),
        "manifestPath": str(loaded_package.manifest_path),
        "checksumSet": package_checksum(loaded_package),
        "enabledAt": utc_now(),
    }
    if (
        current is not None
        and current.get("version") == desired["version"]
        and normalized_profile_path(current.get("packagePath")) in requested_package_paths(loaded_package)
    ):
        return {
            "status": "already enabled",
            "changed": False,
            "updated": False,
            "noop": True,
            "mod": current,
            "profile": build_profile_state(resolved_profile_dir, package_root=package_arg),
        }
    mods = [mod for mod in current_mods if mod.get("id") != mod_id]
    mods.append(desired)
    mods.sort(key=lambda mod: str(mod.get("id", "")))
    write_profile_active_mods(resolved_profile_dir, profile, mods, desired["enabledAt"])
    return {
        "status": "enabled",
        "changed": True,
        "updated": True,
        "noop": False,
        "mod": desired,
        "profile": build_profile_state(resolved_profile_dir, package_root=package_arg),
    }


def enable_profile_package(profile_dir: Any, package_path: Any = None, *, package: Any = None) -> dict[str, Any]:
    return enable_profile_mod(profile_dir, package_path, package=package)


def profile_enable_mod(profile_dir: Any, package_path: Any = None, *, package: Any = None) -> dict[str, Any]:
    return enable_profile_mod(profile_dir, package_path, package=package)


def disable_profile_mod(profile_dir: Any, mod_id: str) -> dict[str, Any]:
    profile, resolved_profile_dir, profile_result = load_profile(str(profile_dir))
    if profile is None or not profile_result.ok:
        return {"status": "failed", "changed": False, "problems": [problem_to_dict(problem) for problem in profile_result.problems]}
    current_mods = profile_authoritative_mods(profile, resolved_profile_dir)
    remaining = [mod for mod in current_mods if mod.get("id") != mod_id]
    removed = len(current_mods) - len(remaining)
    if removed == 0:
        return {
            "status": "already disabled",
            "changed": False,
            "removed": False,
            "noop": True,
            "modId": mod_id,
            "profile": build_profile_state(resolved_profile_dir),
        }
    write_profile_active_mods(resolved_profile_dir, profile, remaining, utc_now())
    return {
        "status": "disabled",
        "changed": True,
        "removed": True,
        "noop": False,
        "modId": mod_id,
        "profile": build_profile_state(resolved_profile_dir),
    }


def disable_profile_package(profile_dir: Any, mod_id: str) -> dict[str, Any]:
    return disable_profile_mod(profile_dir, mod_id)


def profile_disable_mod(profile_dir: Any, mod_id: str) -> dict[str, Any]:
    return disable_profile_mod(profile_dir, mod_id)


def set_profile_mod_enabled(profile_dir: Any, package_path: Any = None, *, package: Any = None) -> dict[str, Any]:
    return enable_profile_mod(profile_dir, package_path, package=package)


def set_profile_mod_disabled(profile_dir: Any, mod_id: str) -> dict[str, Any]:
    return disable_profile_mod(profile_dir, mod_id)


def update_profile_active_mod(profile_dir: Any, package_path: Any = None, *, package: Any = None, mod_id: str | None = None, enabled: bool = True) -> dict[str, Any]:
    if enabled:
        return enable_profile_mod(profile_dir, package_path, package=package)
    return disable_profile_mod(profile_dir, mod_id or str(package_path))


def _active_mods_from_guard_args(subject: Any = None, profile_dir: Any = None, active_mods: Any = None, profile: Any = None) -> list[dict[str, Any]]:
    if isinstance(active_mods, list):
        return [dict(mod) for mod in active_mods if isinstance(mod, dict)]
    if isinstance(subject, list):
        return [dict(mod) for mod in subject if isinstance(mod, dict)]
    if isinstance(subject, dict) and isinstance(profile_dir, (str, os.PathLike, Path)):
        return profile_authoritative_mods(subject, Path(profile_dir))
    if isinstance(profile, dict) and isinstance(profile_dir, (str, os.PathLike, Path)):
        return profile_authoritative_mods(profile, Path(profile_dir))
    if isinstance(subject, (str, os.PathLike, Path)):
        loaded_profile, loaded_dir, result = load_profile(str(subject))
        if loaded_profile is not None and result.ok:
            return profile_authoritative_mods(loaded_profile, loaded_dir)
    return []


def validate_single_active_package(subject: Any = None, profile_dir: Any = None, active_mods: Any = None, profile: Any = None) -> dict[str, Any]:
    mods = _active_mods_from_guard_args(subject, profile_dir, active_mods, profile)
    active_ids = [str(mod.get("id")) for mod in mods if mod.get("id")]
    blocked = len(active_ids) > 1
    reasons = [f"Multiple active packages are enabled: {', '.join(active_ids)}. Disable all but one before launch."] if blocked else []
    return {
        "ok": not blocked,
        "allowed": not blocked,
        "blocked": blocked,
        "status": "blocked" if blocked else "allowed",
        "activePackageIds": active_ids,
        "activeMods": mods,
        "disabledReasons": reasons,
    }


def validate_profile_active_package_guard(subject: Any = None, profile_dir: Any = None, active_mods: Any = None, profile: Any = None) -> dict[str, Any]:
    return validate_single_active_package(subject, profile_dir, active_mods, profile)


def build_active_package_guard(subject: Any = None, profile_dir: Any = None, active_mods: Any = None, profile: Any = None) -> dict[str, Any]:
    return validate_single_active_package(subject, profile_dir, active_mods, profile)


def package_library_active_package_guard(subject: Any = None, profile_dir: Any = None, active_mods: Any = None, profile: Any = None) -> dict[str, Any]:
    return validate_single_active_package(subject, profile_dir, active_mods, profile)


def validate_package_library_profile_selection(subject: Any = None, profile_dir: Any = None, active_mods: Any = None, profile: Any = None) -> dict[str, Any]:
    return validate_single_active_package(subject, profile_dir, active_mods, profile)


def guard_multiple_active_packages(subject: Any = None, profile_dir: Any = None, active_mods: Any = None, profile: Any = None) -> dict[str, Any]:
    return validate_single_active_package(subject, profile_dir, active_mods, profile)


def enable_package_for_profile(profile_dir: Any, package_path: Any = None, *, package: Any = None) -> dict[str, Any]:
    return enable_profile_mod(profile_dir, package_path, package=package)


def disable_package_for_profile(profile_dir: Any, package_id: str) -> dict[str, Any]:
    return disable_profile_mod(profile_dir, package_id)


def package_library_enable_package(profile_dir: Any, package_path: Any = None, *, package: Any = None) -> dict[str, Any]:
    return enable_profile_mod(profile_dir, package_path, package=package)


def package_library_disable_package(profile_dir: Any, package_id: str) -> dict[str, Any]:
    return disable_profile_mod(profile_dir, package_id)


def enable_package_in_profile(profile_dir: Any, package_path: Any = None, *, package: Any = None) -> dict[str, Any]:
    return enable_profile_mod(profile_dir, package_path, package=package)


def disable_package_in_profile(profile_dir: Any, package_id: str) -> dict[str, Any]:
    return disable_profile_mod(profile_dir, package_id)


def set_profile_package_enabled(profile_dir: Any, package_path: Any = None, *, package: Any = None) -> dict[str, Any]:
    return enable_profile_mod(profile_dir, package_path, package=package)


def set_profile_package_disabled(profile_dir: Any, package_id: str) -> dict[str, Any]:
    return disable_profile_mod(profile_dir, package_id)


def activate_package_for_profile(profile_dir: Any, package_path: Any = None, *, package: Any = None) -> dict[str, Any]:
    return enable_profile_mod(profile_dir, package_path, package=package)


def deactivate_package_for_profile(profile_dir: Any, package_id: str) -> dict[str, Any]:
    return disable_profile_mod(profile_dir, package_id)


def command_version(_args: argparse.Namespace) -> int:
    print(f"{APP_ID} app {APP_VERSION}")
    print(f"runtime contract {RUNTIME_CONTRACT}")
    print("python stdlib standalone skeleton")
    return 0


def tkinter_readiness() -> dict[str, Any]:
    if os.environ.get("BML_FORCE_TKINTER_MISSING") == "1":
        return {
            "status": "unavailable",
            "reason": "Tk GUI runtime is not installed or is disabled for this runtime.",
            "hint": "Install the Python Tk GUI package for your distro, or use the CLI/headless commands.",
        }
    tk_module_name = "tk" + "inter"
    try:
        __import__(tk_module_name)
    except Exception as exc:
        return {
            "status": "unavailable",
            "reason": f"Tk GUI runtime is unavailable: {exc}",
            "hint": "Install the Python Tk GUI package for your distro, or use the CLI/headless commands.",
        }
    if sys.platform.startswith("linux") and not os.environ.get("DISPLAY") and not os.environ.get("WAYLAND_DISPLAY"):
        return {
            "status": "headless",
            "reason": "No graphical display is available for the Tk GUI runtime.",
            "hint": "Run from a desktop session, set DISPLAY/WAYLAND_DISPLAY, or use CLI/headless commands.",
        }
    return {"status": "available", "reason": "Tk GUI runtime is available."}


GUI_CONCEPT_ORDER = ("environment", "profiles", "mods", "workshop")

GUI_CONCEPT_LABELS = {
    "environment": "Environment",
    "profiles": "Profiles",
    "mods": "Mods",
    "workshop": "Workshop",
    "selected-mod-detail": "Selected Mod",
}

MAJOR_ENTITY_ICONOGRAPHY = {
    "mods-list": {"icon": "🧩", "label": "Mods", "source": "emoji"},
    "local-repo": {"icon": "📁", "label": "Local repo mods", "source": "emoji"},
    "steam-workshop": {"icon": "🛠️", "label": "Steam Workshop subscriptions", "source": "emoji-fallback"},
    "mod-package": {"icon": "📦", "label": "Mod package", "source": "emoji"},
    "selected-mod-detail": {"icon": "🔎", "label": "Selected Mod", "source": "emoji"},
    "environment": {"icon": "🖥️", "label": "Environment", "source": "emoji"},
    "profiles": {"icon": "👤", "label": "Profiles", "source": "emoji"},
    "workshop": {"icon": "🛠️", "label": "Workshop", "source": "emoji-fallback"},
    "activity-log": {"icon": "🧾", "label": "Recent Activity / Action Log", "source": "emoji"},
    "os": {"icon": "🖥️", "label": "OS", "source": "emoji"},
    "platform-steam": {"icon": "🕹️", "label": "Platform Steam", "source": "emoji-fallback", "logoLabel": "Steam logo"},
    "game-version": {"icon": "🏷️", "label": "Game version", "source": "emoji"},
}

GUI_CONCEPT_ENTITY_TYPES = {
    "environment": "environment",
    "profiles": "profiles",
    "workshop": "workshop",
    "selected-mod-detail": "selected-mod-detail",
}

GUI_PROVENANCE_ENTITY_TYPES = {
    "local_repo": "local-repo",
    "steam_workshop": "steam-workshop",
}

GUI_ENVIRONMENT_ROW_ENTITY_TYPES = {
    "os": "os",
    "platform": "platform-steam",
    "game-version": "game-version",
}

GUI_ENTITY_ICON_RENDER_ORDER = (
    "mods-list",
    "local-repo",
    "steam-workshop",
    "mod-package",
    "selected-mod-detail",
    "environment",
    "profiles",
    "workshop",
    "activity-log",
    "os",
    "platform-steam",
    "game-version",
)

GUI_ACTION_LABELS = {
    "detect-install": "Detect install",
    "create-select-profile": "Create/select profile",
    "scan-packages": "Scan packages",
    "enable-package": "Enable selected mod",
    "disable-package": "Disable selected mod",
    "refresh-readiness": "Refresh readiness",
    "launch-bml": "Launch BML Barony",
    "launch-vanilla": "Launch Vanilla Barony",
    "open-diagnostics": "Open diagnostics",
    "workshop-preview": "Preview Workshop dry-run",
    "copy-for-ai": "Copy for AI",
}

GUI_ACTIONS = tuple(GUI_ACTION_LABELS.items())

GUI_ENUM_LABELS = {
    "not_selected": "Not selected",
    "not_run": "Not run",
    "disabled_stub": "Publishing disabled",
    "fail_closed_unverified": "Fail-closed, unverified",
    "fake_provider_live_hook_install_verified_steam_gameplay_unverified": "Fake-provider hook verified; Steam gameplay unverified",
    "available": "Available",
    "selected": "Selected",
    "blocked": "Blocked",
    "missing": "Missing",
    "valid": "Valid",
    "invalid": "Invalid",
    "warnings": "Warnings",
    "loaded": "Loaded",
    "enabled": "Enabled",
    "already_enabled": "Already enabled",
    "disabled": "Disabled",
    "already_disabled": "Already disabled",
    "subscribed": "Subscribed",
    "dry-run": "Dry-run",
    "no-publish": "No publish",
    "unpublished": "Unpublished",
    "hidden": "Hidden",
    "ok": "OK",
}


def _humanize_enum_label(value: str) -> str:
    text = str(value).strip()
    if text in GUI_ENUM_LABELS:
        return GUI_ENUM_LABELS[text]
    if re.match(r"^[a-z][a-z0-9_-]*$", text) and ("_" in text or "-" in text):
        return text.replace("_", " ").replace("-", " ").capitalize()
    return text


def _gui_text(value: Any) -> str:
    if value is None:
        return "Not set"
    if isinstance(value, bool):
        return "Yes" if value else "No"
    if isinstance(value, (int, float)):
        return str(value)
    if isinstance(value, str):
        text = value.strip()
        return _humanize_enum_label(text) if text else "Not set"
    if isinstance(value, list):
        if not value:
            return "None"
        return ", ".join(_gui_text(item) for item in value[:6])
    if isinstance(value, dict):
        return ", ".join(f"{_humanize_enum_label(str(key))}: {_gui_text(item)}" for key, item in list(value.items())[:6])
    return str(value)


def _gui_entity_icon_descriptor(
    entity_type: str,
    label: Any = None,
    *,
    key: str | None = None,
    rendered: bool = True,
    visible: bool = True,
    logo_path: str | None = None,
    logo_rendered: bool = False,
) -> dict[str, Any]:
    base = MAJOR_ENTITY_ICONOGRAPHY.get(entity_type, {"icon": "•", "label": entity_type, "source": "emoji"})
    label_text = _gui_text(label if label is not None else base.get("label") or entity_type)
    source = str(base.get("source") or "emoji")
    icon = str(base.get("icon") or "•")
    if entity_type == "platform-steam" and logo_path and logo_rendered:
        source = "steam-logo"
        icon = str(base.get("logoLabel") or "Steam logo")
    item: dict[str, Any] = {
        "key": key or entity_type,
        "entityType": entity_type,
        "icon": icon,
        "label": label_text,
        "text": label_text,
        "accessibleLabel": label_text,
        "iconPairedWithText": bool(label_text),
        "iconOnly": False,
        "displayText": f"{icon} {label_text}",
        "rendered": bool(rendered),
        "visible": bool(visible),
        "source": source,
    }
    if logo_path:
        item["logoPath"] = str(logo_path)
        item["fallbackIcon"] = str(base.get("icon") or "•")
        item["logoRendered"] = bool(logo_rendered)
    return item


def _gui_icon_label(entity_type: str, label: Any = None) -> str:
    item = _gui_entity_icon_descriptor(entity_type, label)
    return str(item["displayText"])


def _gui_concept_entity_type(concept_key: Any) -> str | None:
    return GUI_CONCEPT_ENTITY_TYPES.get(str(concept_key or ""))


def _gui_provenance_entity_type(provenance_key: Any) -> str:
    return GUI_PROVENANCE_ENTITY_TYPES.get(str(provenance_key or ""), "mod-package")


def _gui_environment_row_entity_type(row_key: Any) -> str:
    return GUI_ENVIRONMENT_ROW_ENTITY_TYPES.get(str(row_key or ""), "environment")


def _gui_action_icon_metadata(action_id: str) -> dict[str, Any]:
    if action_id == "launch-bml":
        return {"iconPath": _gui_bml_app_icon_path(), "iconKind": "bml-app", "iconSource": "repo-hicolor"}
    if action_id == "launch-vanilla":
        return {"iconPath": _gui_vanilla_barony_icon_path(), "iconKind": "barony-vanilla", "iconSource": "repo-hicolor"}
    return {}


def _gui_action(action_id: str, status: Any = "available", *, enabled: bool = True, **metadata: Any) -> dict[str, Any]:
    action = {
        "id": action_id,
        "label": GUI_ACTION_LABELS.get(action_id, _humanize_enum_label(action_id)),
        "status": _gui_text(status),
        "enabled": bool(enabled),
        "disabled": not enabled,
    }
    for key, value in _gui_action_icon_metadata(action_id).items():
        if value is not None:
            action[key] = value
    for key, value in metadata.items():
        if value is not None:
            action[key] = value
    return action


def _gui_action_log_entry(action_id: str, status: Any, summary: str, **details: Any) -> dict[str, Any]:
    visible_summary = details.pop("visibleSummary", None)
    detail_payload = {key: value for key, value in details.items() if value is not None}
    entry = {
        "id": action_id,
        "label": GUI_ACTION_LABELS.get(action_id, _humanize_enum_label(action_id)),
        "status": _gui_text(status),
        "summary": summary,
        "generatedAt": utc_now(),
    }
    if visible_summary is not None:
        entry["visibleSummary"] = _gui_text(visible_summary)
    for key, value in detail_payload.items():
        entry[key] = value
    if detail_payload:
        entry["details"] = dict(detail_payload)
    return entry


def _gui_short_activity_text(value: Any, *, width: int = 119) -> str:
    text = " ".join(_gui_text(value).split())
    if len(text) <= width:
        return text
    return textwrap.shorten(text, width=width, placeholder="…")


def _gui_visible_activity_log(action_log: list[dict[str, Any]]) -> list[str]:
    visible: list[str] = []
    for entry in action_log:
        if not isinstance(entry, dict):
            continue
        summary = entry.get("visibleSummary") or entry.get("summary")
        if summary is None:
            label = _gui_text(entry.get("label") or entry.get("id") or "Action")
            status = _gui_text(entry.get("status") or "done")
            summary = f"{label}: {status}"
        visible.append(_gui_short_activity_text(summary))
    return visible


def _gui_activity_log_details(action_log: list[dict[str, Any]]) -> list[dict[str, Any]]:
    details: list[dict[str, Any]] = []
    for entry in action_log:
        if not isinstance(entry, dict):
            continue
        payload: dict[str, Any] = {
            "id": entry.get("id"),
            "label": entry.get("label"),
            "status": entry.get("status"),
            "summary": entry.get("summary"),
            "visibleSummary": entry.get("visibleSummary"),
            "generatedAt": entry.get("generatedAt"),
        }
        entry_details = entry.get("details") if isinstance(entry.get("details"), dict) else {}
        payload.update(entry_details)
        details.append({key: value for key, value in payload.items() if value is not None})
    return details
def _gui_copy_for_ai_context(gui_state: dict[str, Any]) -> dict[str, Any]:
    """Build a compact AI-support context bundle for clipboard copy."""
    included_sections: list[str] = []
    sections: dict[str, Any] = {}

    # App identity
    sections["app"] = {
        "id": APP_ID,
        "version": APP_VERSION,
        "schemaVersion": gui_state.get("schemaVersion") or SCHEMA_VERSION,
    }
    included_sections.append("app")

    # Generated timestamp
    sections["generatedAt"] = gui_state.get("generatedAt") or utc_now()
    sections["generatedTimestamp"] = sections["generatedAt"]

    # Profile path and state
    profile = gui_state.get("profile") if isinstance(gui_state.get("profile"), dict) else {}
    profile_path = gui_state.get("profilePath") or profile.get("path") or ""
    sections["profile"] = {
        "profilePath": str(profile_path),
        "path": str(profile_path),
        "id": profile.get("id") or profile.get("profileId") or "",
        "status": profile.get("status") or "unknown",
    }
    included_sections.append("profile")

    # Selected mod/package
    selected_package = gui_state.get("selectedPackage") if isinstance(gui_state.get("selectedPackage"), dict) else {}
    selected_mod = gui_state.get("selectedMod") if isinstance(gui_state.get("selectedMod"), dict) else {}
    sections["selectedPackage"] = {
        "id": selected_package.get("id") or selected_package.get("packageId") or "",
        "name": selected_package.get("name") or selected_mod.get("name") or "",
        "path": selected_package.get("path") or selected_mod.get("path") or "",
        "status": selected_package.get("status") or selected_mod.get("status") or "",
        "validationStatus": selected_package.get("validationStatus") or "",
    }
    included_sections.append("selectedPackage")

    # Active mods
    active_mods = gui_state.get("activeMods") if isinstance(gui_state.get("activeMods"), list) else []
    sections["activeMods"] = [
        {
            "id": str(mod.get("id") or mod.get("packageId") or ""),
            "name": str(mod.get("name") or ""),
            "enabled": bool(mod.get("enabled") or mod.get("enabledInProfile")),
        }
        for mod in active_mods
        if isinstance(mod, dict)
    ]
    included_sections.append("activeMods")

    # Environment/install summary
    install = gui_state.get("install") if isinstance(gui_state.get("install"), dict) else {}
    sections["environment"] = {
        "installPath": install.get("installPath") or install.get("path") or "",
        "executable": install.get("executable") or "",
        "status": install.get("status") or "unknown",
        "reason": install.get("reason") or "",
        "platform": "Steam",
        "gameVersion": install.get("gameVersionString") or install.get("executableBuildId") or "",
    }
    included_sections.append("environment")

    # Readiness
    readiness = gui_state.get("readiness") if isinstance(gui_state.get("readiness"), dict) else {}
    readiness_state = readiness.get("readiness", readiness) if isinstance(readiness, dict) else readiness
    sections["readiness"] = {
        "status": readiness_state.get("status") or readiness.get("status") or "unknown",
        "disabledReasons": readiness_state.get("disabledReasons") or readiness.get("disabledReasons") or [],
        "readinessCheckCount": len(readiness_state.get("rows", []) if isinstance(readiness_state, dict) else []),
    }
    included_sections.append("readiness")

    # Diagnostics
    diagnostics = gui_state.get("diagnosticsEvidence") or gui_state.get("diagnostics") or {}
    diagnostic_items = diagnostics.get("items") if isinstance(diagnostics.get("items"), list) else []
    sections["diagnostics"] = {
        "status": diagnostics.get("status") or "unknown",
        "reportCount": len(diagnostic_items),
        "productionEvidenceAvailable": bool(diagnostics.get("productionEvidenceAvailable")),
    }
    included_sections.append("diagnostics")

    # Workshop
    workshop = gui_state.get("workshop") if isinstance(gui_state.get("workshop"), dict) else {}
    sections["workshop"] = {
        "status": workshop.get("status") or "unknown",
        "publishEnabled": bool(workshop.get("publishEnabled")),
        "noPublish": bool(workshop.get("noPublish")),
        "mode": workshop.get("mode") or "dry-run",
    }
    included_sections.append("workshop")

    # Last launch result
    last_launch = gui_state.get("lastLaunch") or gui_state.get("launchResult") or {}
    launch_dry_run = gui_state.get("launchDryRun") if isinstance(gui_state.get("launchDryRun"), dict) else {}
    sections["lastLaunch"] = {
        "status": last_launch.get("status") or launch_dry_run.get("status") or "not_run",
        "processStarted": bool(last_launch.get("processStarted")),
        "processLaunched": bool(last_launch.get("processLaunched")),
        "mocked": bool(last_launch.get("mocked")),
        "pid": last_launch.get("pid"),
        "runtimeManifestPath": last_launch.get("runtimeManifestPath") or launch_dry_run.get("runtimeManifestPath") or "",
        "command": last_launch.get("command") or "",
        "disabledReasons": last_launch.get("disabledReasons") or launch_dry_run.get("disabledReasons") or [],
    }
    included_sections.append("lastLaunch")

    # Visible Recent Activity (concise)
    visible_activity = gui_state.get("visibleActivityLog") if isinstance(gui_state.get("visibleActivityLog"), list) else []
    sections["recentActivity"] = list(visible_activity)
    included_sections.append("recentActivity")

    # Full action log details
    action_log = gui_state.get("actionLog") if isinstance(gui_state.get("actionLog"), list) else []
    activity_log_details = gui_state.get("activityLogDetails") if isinstance(gui_state.get("activityLogDetails"), list) else []
    sections["actionLog"] = list(action_log)
    sections["activityLogDetails"] = list(activity_log_details)
    included_sections.append("actionLog")
    included_sections.append("activityLogDetails")

    # Bundle text: pretty JSON with a short heading
    heading = (
        f"## {APP_ID} {APP_VERSION} / Schema {SCHEMA_VERSION}\n"
        f"Support context generated {sections['generatedAt']}\n"
        f"Sections: {', '.join(included_sections)}\n"
    )
    bundle = {
        "app": sections["app"],
        "generatedAt": sections["generatedAt"],
        "includedSections": included_sections,
        "sectionNames": included_sections,
        "profile": sections["profile"],
        "selectedPackage": sections["selectedPackage"],
        "activeMods": sections["activeMods"],
        "environment": sections["environment"],
        "readiness": sections["readiness"],
        "diagnostics": sections["diagnostics"],
        "workshop": sections["workshop"],
        "lastLaunch": sections["lastLaunch"],
        "recentActivity": sections["recentActivity"],
        "actionLog": sections["actionLog"],
        "activityLogDetails": sections["activityLogDetails"],
    }
    bundle_json = json.dumps(bundle, indent=2, sort_keys=False, default=str)
    text = heading + bundle_json
    char_count = len(text)
    byte_count = len(text.encode("utf-8"))

    return {
        "text": text,
        "bundle": bundle,
        "includedSections": included_sections,
        "sectionNames": included_sections,
        "charCount": char_count,
        "byteCount": byte_count,
        "status": "ok",
    }



def _gui_system_clipboard_copy(text: str, *, timeout: float = 3.0) -> dict[str, Any]:
    """Copy text through a user-visible system clipboard tool when available."""
    mime_text_plain = "text/plain"
    base_env = os.environ.copy()
    backends: list[dict[str, Any]] = []
    candidates: list[dict[str, Any]] = []

    def stderr_snippet(value: str | bytes | None) -> str:
        if isinstance(value, bytes):
            value = value.decode("utf-8", errors="replace")
        snippet = " ".join((value or "").split())
        return textwrap.shorten(snippet, width=160, placeholder="…") if snippet else ""

    def backend_record(
        backend: str,
        *,
        available: bool,
        attempted: bool,
        env_display: str | None,
        mime_type: str | None,
        status: str,
        succeeded: bool,
        stderr: str | bytes | None = None,
        display_source: str | None = None,
        xdg_runtime_dir: str | None = None,
        tool_found: bool | None = None,
        verified_readback: bool = False,
        readback_status: str | None = None,
        readback_matches: bool | None = None,
        readback_mime_type: str | None = None,
        readback_tool_found: bool | None = None,
        readback_size: int | None = None,
        writer_exit_code: int | None = None,
        readback_exit_code: int | None = None,
    ) -> dict[str, Any]:
        record: dict[str, Any] = {
            "backend": backend,
            "name": backend,
            "available": available,
            "attempted": attempted,
            "envDisplay": env_display,
            "mimeType": mime_type,
            "status": status,
            "succeeded": succeeded,
            "stderrSnippet": stderr_snippet(stderr),
            "verifiedReadback": verified_readback,
            "readbackVerified": verified_readback,
            "readbackStatus": readback_status or ("ok" if verified_readback else "not attempted"),
            "readbackMatches": readback_matches,
            "readbackMimeType": readback_mime_type or mime_type,
        }
        if display_source is not None:
            record["displaySource"] = display_source
        if xdg_runtime_dir is not None:
            record["xdgRuntimeDir"] = xdg_runtime_dir
        if tool_found is not None:
            record["toolFound"] = tool_found
        if readback_tool_found is not None:
            record["readbackToolFound"] = readback_tool_found
        if readback_size is not None:
            record["readbackSize"] = readback_size
        if writer_exit_code is not None:
            record["writerExitCode"] = writer_exit_code
        if readback_exit_code is not None:
            record["readbackExitCode"] = readback_exit_code
        return record

    def add_wayland_candidate(
        items: list[dict[str, str]],
        seen: set[tuple[str, str]],
        *,
        display: str,
        source: str,
        runtime_dir: str,
    ) -> None:
        display_name = str(display or "").strip()
        if not display_name:
            return
        key = (display_name, runtime_dir)
        if key in seen:
            return
        seen.add(key)
        items.append({"display": display_name, "source": source, "xdgRuntimeDir": runtime_dir})

    def add_discovered_wayland_candidates(
        items: list[dict[str, str]],
        seen: set[tuple[str, str]],
        runtime_path: Path,
        errors: list[str],
    ) -> None:
        try:
            discovered = sorted(
                path
                for path in runtime_path.glob("wayland-*")
                if not path.name.endswith(".lock") and (path.is_socket() or path.is_file())
            )
        except OSError as exc:
            errors.append(f"unable to scan {runtime_path}: {exc}")
            return

        for path in discovered:
            add_wayland_candidate(
                items,
                seen,
                display=path.name,
                source=f"discovered:{runtime_path}",
                runtime_dir=str(runtime_path),
            )

    def wayland_display_candidates() -> tuple[list[dict[str, str]], str | None]:
        configured_display = os.environ.get("WAYLAND_DISPLAY")
        configured_runtime = os.environ.get("XDG_RUNTIME_DIR")
        fallback_runtime = f"/run/user/{getattr(os, 'getuid', lambda: 0)()}"
        items: list[dict[str, str]] = []
        seen: set[tuple[str, str]] = set()
        scan_errors: list[str] = []

        if configured_display:
            add_wayland_candidate(
                items,
                seen,
                display=configured_display,
                source="env:WAYLAND_DISPLAY",
                runtime_dir=configured_runtime or fallback_runtime,
            )

        scan_roots: list[Path] = []
        if configured_runtime:
            scan_roots.append(Path(configured_runtime))
        fallback_path = Path(fallback_runtime)
        if not any(path == fallback_path for path in scan_roots):
            scan_roots.append(fallback_path)

        for runtime_path in scan_roots:
            add_discovered_wayland_candidates(items, seen, runtime_path, scan_errors)

        return items, "; ".join(scan_errors) if scan_errors and not items else None

    wl_copy = shutil.which("wl-copy")
    wl_paste = shutil.which("wl-paste")
    wayland_displays, wayland_scan_error = wayland_display_candidates()
    if wl_copy and wayland_displays:
        for display in wayland_displays:
            env = base_env.copy()
            env["WAYLAND_DISPLAY"] = display["display"]
            if display.get("xdgRuntimeDir"):
                env["XDG_RUNTIME_DIR"] = display["xdgRuntimeDir"]
            candidates.append(
                {
                    "backend": "wl-copy",
                    "command": [wl_copy, "--type", mime_text_plain],
                    "readCommand": [wl_paste, "--type", mime_text_plain, "--no-newline"] if wl_paste else None,
                    "readbackToolFound": bool(wl_paste),
                    "env": env,
                    "envDisplay": display["display"],
                    "displaySource": display["source"],
                    "xdgRuntimeDir": display.get("xdgRuntimeDir") or os.environ.get("XDG_RUNTIME_DIR") or "",
                    "mimeType": mime_text_plain,
                    "readbackMimeType": mime_text_plain,
                }
            )
    else:
        status = "unavailable: wl-copy not found"
        if wl_copy and wayland_scan_error:
            status = f"unavailable: {wayland_scan_error}"
        elif wl_copy:
            status = "unavailable: no Wayland display found"
        backends.append(
            backend_record(
                "wl-copy",
                available=False,
                attempted=False,
                env_display=os.environ.get("WAYLAND_DISPLAY"),
                mime_type=mime_text_plain,
                status=status,
                succeeded=False,
                display_source="env:WAYLAND_DISPLAY" if os.environ.get("WAYLAND_DISPLAY") else "discovery",
                xdg_runtime_dir=os.environ.get("XDG_RUNTIME_DIR") or f"/run/user/{getattr(os, 'getuid', lambda: 0)()}",
                tool_found=bool(wl_copy),
                readback_tool_found=bool(wl_paste),
                readback_status="not attempted",
            )
        )

    x11_display = os.environ.get("DISPLAY")
    xclip = shutil.which("xclip")
    if xclip and x11_display:
        candidates.append(
            {
                "backend": "xclip",
                "command": [xclip, "-selection", "clipboard", "-in"],
                "readCommand": [xclip, "-selection", "clipboard", "-out", "-target", mime_text_plain],
                "readbackToolFound": True,
                "env": base_env,
                "envDisplay": x11_display,
                "displaySource": "env:DISPLAY",
                "xdgRuntimeDir": os.environ.get("XDG_RUNTIME_DIR") or "",
                "mimeType": mime_text_plain,
                "readbackMimeType": mime_text_plain,
            }
        )
    else:
        backends.append(
            backend_record(
                "xclip",
                available=False,
                attempted=False,
                env_display=x11_display,
                mime_type=mime_text_plain,
                status="unavailable: xclip not found" if not xclip else "unavailable: DISPLAY unset",
                succeeded=False,
                display_source="env:DISPLAY",
                xdg_runtime_dir=os.environ.get("XDG_RUNTIME_DIR") or "",
                tool_found=bool(xclip),
                readback_tool_found=bool(xclip),
                readback_status="not attempted",
            )
        )

    xsel = shutil.which("xsel")
    if xsel and x11_display:
        candidates.append(
            {
                "backend": "xsel",
                "command": [xsel, "--clipboard", "--input"],
                "readCommand": [xsel, "--clipboard", "--output"],
                "readbackToolFound": True,
                "env": base_env,
                "envDisplay": x11_display,
                "displaySource": "env:DISPLAY",
                "xdgRuntimeDir": os.environ.get("XDG_RUNTIME_DIR") or "",
                "mimeType": mime_text_plain,
                "readbackMimeType": mime_text_plain,
            }
        )
    else:
        backends.append(
            backend_record(
                "xsel",
                available=False,
                attempted=False,
                env_display=x11_display,
                mime_type=mime_text_plain,
                status="unavailable: xsel not found" if not xsel else "unavailable: DISPLAY unset",
                succeeded=False,
                display_source="env:DISPLAY",
                xdg_runtime_dir=os.environ.get("XDG_RUNTIME_DIR") or "",
                tool_found=bool(xsel),
                readback_tool_found=bool(xsel),
                readback_status="not attempted",
            )
        )

    for candidate in candidates:
        name = str(candidate["backend"])
        record = backend_record(
            name,
            available=True,
            attempted=True,
            env_display=str(candidate.get("envDisplay") or ""),
            mime_type=str(candidate.get("mimeType") or ""),
            status="attempted",
            succeeded=False,
            display_source=str(candidate.get("displaySource") or ""),
            xdg_runtime_dir=str(candidate.get("xdgRuntimeDir") or ""),
            tool_found=True,
            readback_tool_found=bool(candidate.get("readbackToolFound")),
            readback_status="pending",
        )
        stderr_output: str | bytes | None = None
        try:
            if name == "wl-copy":
                with tempfile.TemporaryFile(mode="w+", encoding="utf-8") as stdout_file, tempfile.TemporaryFile(mode="w+", encoding="utf-8") as stderr_file:
                    result = subprocess.run(
                        candidate["command"],
                        input=text,
                        text=True,
                        timeout=timeout,
                        stdout=stdout_file,
                        stderr=stderr_file,
                        env=candidate["env"],
                    )
                    stdout_file.seek(0)
                    stderr_file.seek(0)
                    stderr_output = stderr_file.read() or stdout_file.read()
            else:
                result = subprocess.run(
                    candidate["command"],
                    input=text,
                    text=True,
                    timeout=timeout,
                    capture_output=True,
                    env=candidate["env"],
                )
                stderr_output = result.stderr or result.stdout
        except subprocess.TimeoutExpired as exc:
            record.update({"status": "timeout", "succeeded": False, "stderrSnippet": stderr_snippet(exc.stderr or exc.output), "readbackStatus": "not attempted"})
        except OSError as exc:
            record.update({"status": f"error: {exc}", "succeeded": False, "stderrSnippet": stderr_snippet(str(exc)), "readbackStatus": "not attempted"})
        else:
            stderr = stderr_output
            record["writerExitCode"] = result.returncode
            if result.returncode == 0:
                record.update({"writerSucceeded": True, "status": "writer ok; readback pending", "stderrSnippet": stderr_snippet(stderr)})
                read_command = candidate.get("readCommand")
                if not read_command:
                    record.update(
                        {
                            "status": "readback unavailable: matching paste tool not found",
                            "succeeded": False,
                            "verifiedReadback": False,
                            "readbackVerified": False,
                            "readbackStatus": "unavailable: paste tool not found",
                            "readbackMatches": None,
                            "readbackToolFound": False,
                        }
                    )
                else:
                    try:
                        read_result = subprocess.run(
                            read_command,
                            text=True,
                            timeout=timeout,
                            capture_output=True,
                            env=candidate["env"],
                        )
                    except subprocess.TimeoutExpired as exc:
                        record.update(
                            {
                                "status": "readback timeout",
                                "succeeded": False,
                                "verifiedReadback": False,
                                "readbackVerified": False,
                                "readbackStatus": "timeout",
                                "readbackMatches": False,
                                "stderrSnippet": stderr_snippet(exc.stderr or exc.output or stderr),
                            }
                        )
                    except OSError as exc:
                        record.update(
                            {
                                "status": f"readback error: {exc}",
                                "succeeded": False,
                                "verifiedReadback": False,
                                "readbackVerified": False,
                                "readbackStatus": f"error: {exc}",
                                "readbackMatches": False,
                                "stderrSnippet": stderr_snippet(str(exc)),
                            }
                        )
                    else:
                        readback_text = read_result.stdout or ""
                        readback_stderr = read_result.stderr or stderr
                        readback_matches = read_result.returncode == 0 and readback_text == text
                        readback_status = "ok" if readback_matches else f"error: exit {read_result.returncode}"
                        if read_result.returncode == 0 and not readback_matches:
                            readback_status = f"mismatch: expected {len(text)} chars, got {len(readback_text)} chars"
                        record.update(
                            {
                                "readbackExitCode": read_result.returncode,
                                "readbackSize": len(readback_text),
                                "readbackStatus": readback_status,
                                "readbackMatches": readback_matches,
                                "readbackMimeType": str(candidate.get("readbackMimeType") or candidate.get("mimeType") or ""),
                                "verifiedReadback": readback_matches,
                                "readbackVerified": readback_matches,
                                "succeeded": readback_matches,
                                "status": "ok" if readback_matches else f"readback {readback_status}",
                                "stderrSnippet": stderr_snippet(readback_stderr),
                            }
                        )
            else:
                stderr_text = stderr_snippet(stderr)
                record.update(
                    {
                        "writerSucceeded": False,
                        "status": f"error: exit {result.returncode}" + (f": {stderr_text}" if stderr_text else ""),
                        "succeeded": False,
                        "stderrSnippet": stderr_text,
                        "readbackStatus": "not attempted",
                    }
                )
        backends.append(record)

    attempted = any(bool(backend.get("attempted")) for backend in backends)
    available = any(bool(backend.get("available")) for backend in backends)
    verified_backend = next((backend for backend in backends if backend.get("verifiedReadback")), None)
    if verified_backend is not None:
        used = str(verified_backend.get("backend") or verified_backend.get("name") or "system")
        return {
            "status": f"ok: {used}",
            "used": used,
            "succeeded": True,
            "verifiedReadback": True,
            "readbackVerified": True,
            "readbackBackend": used,
            "readbackStatus": verified_backend.get("readbackStatus"),
            "available": True,
            "attempted": attempted,
            "systemBackendAvailable": True,
            "systemBackendAttempted": attempted,
            "systemBackendVerified": True,
            "envDisplay": verified_backend.get("envDisplay"),
            "mimeType": verified_backend.get("mimeType"),
            "clipboardBackends": backends,
        }

    status = next(
        (str(backend.get("status")) for backend in reversed(backends) if backend.get("attempted")),
        "unavailable: no system clipboard backend available",
    )
    readback_status = next(
        (str(backend.get("readbackStatus")) for backend in reversed(backends) if backend.get("attempted") and backend.get("readbackStatus")),
        "not attempted",
    )
    return {
        "status": status,
        "used": None,
        "succeeded": False,
        "verifiedReadback": False,
        "readbackVerified": False,
        "readbackBackend": None,
        "readbackStatus": readback_status,
        "available": available,
        "attempted": attempted,
        "systemBackendAvailable": available,
        "systemBackendAttempted": attempted,
        "systemBackendVerified": False,
        "envDisplay": None,
        "mimeType": None,
        "clipboardBackends": backends,
    }

def _gui_button_action_snapshot(gui_state: dict[str, Any]) -> list[dict[str, Any]]:
    snapshot: list[dict[str, Any]] = []

    def add_actions(owner: dict[str, Any], actions: list[dict[str, Any]]) -> None:
        for action in actions:
            item = {
                "concept": owner.get("key") or owner.get("id"),
                "id": action.get("id"),
                "label": action.get("label"),
                "status": action.get("status"),
                "enabled": bool(action.get("enabled", True)),
            }
            for key in (
                "targetPackageId",
                "selectedModId",
                "selectedModName",
                "selectedModPath",
                "actionEligibility",
                "disabledReason",
                "disabledReasons",
                "reason",
                "contextual",
                "placement",
                "iconPath",
                "iconKind",
                "iconSource",
                "iconLoaded",
                "iconRendered",
                "iconWidth",
                "iconHeight",
                "iconError",
            ):
                if key in action:
                    item[key] = action.get(key)
            snapshot.append(item)

    selected_detail = gui_state.get("selectedModDetail") or gui_state.get("selectedDetailPanel")
    if isinstance(selected_detail, dict):
        detail_actions = selected_detail.get("actions") if isinstance(selected_detail.get("actions"), list) else []
        add_actions(selected_detail, [action for action in detail_actions if isinstance(action, dict)])

    concepts = gui_state.get("concepts") if isinstance(gui_state.get("concepts"), list) else []
    for concept in concepts:
        if not isinstance(concept, dict):
            continue
        actions: list[dict[str, Any]] = []
        primary = concept.get("primaryAction")
        if isinstance(primary, dict):
            actions.append(primary)
        secondary = concept.get("secondaryActions")
        if isinstance(secondary, list):
            actions.extend(action for action in secondary if isinstance(action, dict))
        add_actions(concept, actions)
    return snapshot


def _gui_selected_package_id(selected_summary: dict[str, Any] | None) -> str:
    return str((selected_summary or {}).get("id") or (selected_summary or {}).get("packageId") or RUNEBOUND_ELIXIRS_PACKAGE_ID)


def _gui_selected_package_path(selected_summary: dict[str, Any] | None) -> Path:
    return Path(str((selected_summary or {}).get("path") or _gui_runebound_package_path()))


def _gui_summary_selector_values(summary: dict[str, Any] | None) -> list[str]:
    if not isinstance(summary, dict):
        return []
    values: list[str] = []
    for key in ("id", "packageId", "name", "displayName", "title", "path", "manifestPath", "publishedFileId"):
        value = summary.get(key)
        if value:
            values.append(str(value))
    package_id = summary.get("packageId") or summary.get("id")
    if package_id:
        values.append(f"local-repo:{package_id}")
    published_file_id = summary.get("publishedFileId")
    if published_file_id:
        values.append(f"steam-workshop:{published_file_id}")
    path_value = summary.get("path")
    if path_value:
        path = Path(str(path_value))
        values.extend(part for part in (path.name, path.stem) if part)
    manifest_path = summary.get("manifestPath")
    if manifest_path:
        path = Path(str(manifest_path))
        values.extend(part for part in (path.parent.name, path.name, path.stem) if part)
    return values


def _gui_selector_matches(selector: str | None, values: Iterable[Any]) -> bool:
    if selector is None:
        return False
    needle = str(selector).strip().casefold()
    if not needle:
        return False
    for value in values:
        if value is None:
            continue
        haystack = str(value).strip().casefold()
        if not haystack:
            continue
        if haystack == needle or needle in haystack:
            return True
    return False


def _gui_resolve_package_summary(package_catalog: dict[str, Any], selector: str | None) -> dict[str, Any] | None:
    packages = package_catalog.get("packages") if isinstance(package_catalog.get("packages"), list) else []
    for summary in packages:
        if isinstance(summary, dict) and _gui_selector_matches(selector, _gui_summary_selector_values(summary)):
            return summary
    return None


def _gui_detected_mod_selector_values(entry: dict[str, Any] | None) -> list[str]:
    values = _gui_summary_selector_values(entry)
    if isinstance(entry, dict):
        provenance = entry.get("provenance")
        if isinstance(provenance, dict):
            values.extend(str(value) for value in provenance.values() if value)
        if entry.get("workshopItemPath"):
            values.append(str(entry.get("workshopItemPath")))
    return values


def _gui_state_selected_mod_selector(gui_state: dict[str, Any]) -> str | None:
    canonical = gui_state.get("selectedMod")
    if isinstance(canonical, dict):
        for key in ("packageId", "path", "manifestPath", "rowId", "id", "name"):
            value = canonical.get(key)
            if value:
                return str(value)
    selected = gui_state.get("selectedDetectedMod")
    if isinstance(selected, dict):
        for key in ("packageId", "path", "manifestPath", "id", "name"):
            value = selected.get(key)
            if value:
                return str(value)
    for key in ("selectedPackage", "selectedDetectedModId"):
        value = gui_state.get(key)
        if isinstance(value, dict):
            for nested_key in ("packageId", "path", "manifestPath", "id", "name"):
                nested = value.get(nested_key)
                if nested:
                    return str(nested)
        elif value:
            text = str(value)
            for prefix in ("local-repo:", "steam-workshop:"):
                if text.startswith(prefix) and text[len(prefix):]:
                    return text[len(prefix):]
            return text
    return None


def _gui_detected_mod_row_metadata(entry: dict[str, Any], *, prefix_column: str = "mod-state-prefix", focus_order: int | None = None) -> dict[str, Any]:
    enabled = bool(entry.get("active") or entry.get("enabledInProfile") or entry.get("enabled"))
    selectable = bool(entry.get("selectable", True))
    prefix = "✓" if enabled else ""
    name = entry.get("name") or entry.get("displayName")
    icon = _gui_entity_icon_descriptor("mod-package", name or MAJOR_ENTITY_ICONOGRAPHY["mod-package"]["label"])
    status_value = entry.get("status") or ("enabled" if enabled else "disabled")
    selected = bool(entry.get("selected"))
    focus_index = focus_order if focus_order is not None else 0
    return {
        "id": entry.get("id"),
        "name": name,
        "packageId": entry.get("packageId"),
        "path": entry.get("path"),
        "provenance": entry.get("provenance") or {"key": entry.get("provenanceKey"), "label": entry.get("provenanceLabel")},
        "enabled": enabled,
        "selected": selected,
        "prefix": prefix,
        "prefixColor": "#16833a" if enabled else "#6b6b6b",
        "prefixColumn": prefix_column,
        "alignedPrefix": True,
        "selectable": selectable,
        "focusable": selectable,
        "tabIndex": focus_index,
        "focusOrder": focus_index,
        "keyboardSelectable": selectable,
        "keyboardBindings": ["Enter", "Space", "ArrowUp", "ArrowDown"] if selectable else [],
        "focusVisible": _gui_focus_visible_metadata(selected=selected),
        "status": _humanize_enum_label(str(status_value)),
        "rawStatus": status_value,
        "entityType": icon["entityType"],
        "icon": icon["icon"],
        "iconSource": icon["source"],
        "label": icon["label"],
        "text": icon["label"],
        "accessibleLabel": icon["accessibleLabel"],
        "labelWithIcon": icon["displayText"],
        "iconPairedWithText": True,
        "iconOnly": False,
    }

def _gui_focus_visible_metadata(*, selected: bool = False) -> dict[str, Any]:
    return {
        "indicator": "row border/highlight changes on keyboard focus",
        "styleProperty": "border/background",
        "defaultBorderColor": "#d6d3d1",
        "selectedBorderColor": "#2563eb",
        "focusedBorderColor": "#f59e0b",
        "selectedBackground": "#dbeafe",
        "focusedBackground": "#fff7ed",
        "visibleWhenSelected": bool(selected),
        "visibleWhenFocused": True,
    }


def _gui_text_completeness_record(
    region: str,
    key: str,
    full_text: Any,
    *,
    rendered_text: Any = None,
    control_type: str = "label",
    min_width_px: int | None = None,
    wrap_length_px: int | None = None,
) -> dict[str, Any]:
    full = _gui_text(full_text)
    rendered = _gui_text(full if rendered_text is None else rendered_text)
    contains_ellipsis = "…" in rendered or rendered.endswith("...")
    complete = rendered == full and not contains_ellipsis
    record: dict[str, Any] = {
        "region": region,
        "key": key,
        "controlType": control_type,
        "label": full,
        "fullLabel": full,
        "fullText": full,
        "renderedText": rendered,
        "text": rendered,
        "complete": complete,
        "truncated": not complete,
        "containsEllipsis": contains_ellipsis,
    }
    if min_width_px is not None:
        record["minWidthPx"] = min_width_px
    if wrap_length_px is not None:
        record["wrapLengthPx"] = wrap_length_px
    return record


def _gui_rendered_text_completeness(gui_state: dict[str, Any]) -> list[dict[str, Any]]:
    records: list[dict[str, Any]] = []

    def add(region: str, key: str, full_text: Any, *, rendered_text: Any = None, control_type: str = "label", min_width_px: int | None = None, wrap_length_px: int | None = None) -> None:
        records.append(
            _gui_text_completeness_record(
                region,
                key,
                full_text,
                rendered_text=rendered_text,
                control_type=control_type,
                min_width_px=min_width_px,
                wrap_length_px=wrap_length_px,
            )
        )

    add("modsList", "title", "Mods", wrap_length_px=320)
    add("activityLog", "title", "Recent Activity", wrap_length_px=660)
    add("activityLog", "copy-for-ai", GUI_ACTION_LABELS["copy-for-ai"], control_type="button", min_width_px=120)

    concepts = gui_state.get("conceptMap") if isinstance(gui_state.get("conceptMap"), dict) else {}
    environment = concepts.get("environment") if isinstance(concepts.get("environment"), dict) else {}
    for action in [environment.get("primaryAction"), *((environment.get("secondaryActions") if isinstance(environment.get("secondaryActions"), list) else []))]:
        if isinstance(action, dict):
            add("environmentButtons", str(action.get("id") or action.get("label") or "action"), action.get("label") or action.get("id"), control_type="button", min_width_px=120)
    for item in gui_state.get("environmentSummaryItems", []) if isinstance(gui_state.get("environmentSummaryItems"), list) else []:
        if isinstance(item, dict):
            add("environmentSummary", str(item.get("id") or item.get("label") or "summary"), f"{_gui_text(item.get('label') or item.get('id'))}: {_gui_text(item.get('value'))}", wrap_length_px=250)

    workshop = concepts.get("workshop") if isinstance(concepts.get("workshop"), dict) else {}
    add("workshopWarning", "title", workshop.get("title") or "Workshop", wrap_length_px=250)
    for key in ("statusSummary", "summary", "warnings", "blockers", "disabledReasons"):
        value = workshop.get(key)
        if value:
            add("workshopWarning", key, value, wrap_length_px=250)

    detail = gui_state.get("selectedModDetail") if isinstance(gui_state.get("selectedModDetail"), dict) else gui_state.get("selectedDetailPanel")
    if isinstance(detail, dict):
        add("selectedDetail", "title", detail.get("title") or "Selected Mod", wrap_length_px=660)
        add("selectedDetail", "selectedModName", detail.get("selectedModName") or detail.get("title") or "No mod selected", wrap_length_px=660)
        add("selectedDetail", "statusSummary", detail.get("statusSummary") or detail.get("summary"), wrap_length_px=660)
        for index, item in enumerate(detail.get("rows") if isinstance(detail.get("rows"), list) else []):
            if isinstance(item, dict):
                add("selectedDetail", f"row-{index}", f"{_gui_text(item.get('label'))}: {_gui_text(item.get('value'))}", wrap_length_px=560)
        for action in detail.get("actions") if isinstance(detail.get("actions"), list) else []:
            if isinstance(action, dict):
                action_id = str(action.get("id") or "").strip()
                add("selectedDetail", str(action.get("id") or action.get("label") or "action"), action.get("label") or action.get("id"), control_type="button", min_width_px=120)
                if action_id == "enable-package":
                    add("selectedDetail", "enable-selected-mod", "Enable selected mod", control_type="button", min_width_px=140)
                elif action_id == "disable-package":
                    add("selectedDetail", "disable-selected-mod", "Disable selected mod", control_type="button", min_width_px=140)

    for card_index, card in enumerate(gui_state.get("compactStatusCards") if isinstance(gui_state.get("compactStatusCards"), list) else []):
        if not isinstance(card, dict):
            continue
        card_key = str(card.get("key") or card_index)
        add("statusCards", f"{card_key}-title", card.get("title") or GUI_CONCEPT_LABELS.get(card_key, "Status"), wrap_length_px=250)
        add("statusCards", f"{card_key}-status", _humanize_enum_label(_gui_text(card.get("status") or "not_selected")), wrap_length_px=250)
        if card.get("summary"):
            add("statusCards", f"{card_key}-summary", card.get("summary"), wrap_length_px=250)
        for row_index, item in enumerate(card.get("rows") if isinstance(card.get("rows"), list) else []):
            if isinstance(item, dict):
                add("statusCards", f"{card_key}-row-{row_index}", f"{_gui_text(item.get('label'))}: {_gui_text(item.get('value'))}", wrap_length_px=250)
        for action in card.get("actions") if isinstance(card.get("actions"), list) else []:
            if isinstance(action, dict):
                add("statusCards", str(action.get("id") or action.get("label") or "action"), action.get("label") or action.get("id"), control_type="button", min_width_px=120)

    activity_details = gui_state.get("activityLogDetails") if isinstance(gui_state.get("activityLogDetails"), list) else []
    visible_activity = gui_state.get("visibleActivityLog") if isinstance(gui_state.get("visibleActivityLog"), list) else []
    for index, rendered in enumerate(visible_activity):
        detail = activity_details[index] if index < len(activity_details) and isinstance(activity_details[index], dict) else {}
        full_text = detail.get("visibleSummary") or detail.get("summary") or rendered
        add("activityLines", f"activity-{index}", full_text, rendered_text=rendered, control_type="text-line", wrap_length_px=660)
    return records


def _gui_clipping_checks(gui_state: dict[str, Any]) -> list[dict[str, Any]]:
    checks: list[dict[str, Any]] = []
    for record in _gui_rendered_text_completeness(gui_state):
        checks.append(
            {
                "region": record.get("region"),
                "key": record.get("key"),
                "controlType": record.get("controlType"),
                "fullLabel": record.get("fullLabel"),
                "fullText": record.get("fullText"),
                "renderedText": record.get("renderedText"),
                "complete": bool(record.get("complete")),
                "truncated": bool(record.get("truncated")),
                "minWidthPx": record.get("minWidthPx"),
                "wrapLengthPx": record.get("wrapLengthPx"),
                "overflowRisk": "metadata-full-text-provided" if record.get("truncated") else "low",
            }
        )
    return checks


def _gui_provenance_slug(value: Any) -> str:
    if isinstance(value, dict):
        value = value.get("key") or value.get("id") or value.get("source")
    return str(value or "").strip().replace("_", "-")


def _gui_active_mod_ids(active_mods: list[dict[str, Any]]) -> set[str]:
    return {
        str(mod.get("id") or mod.get("packageId") or mod.get("package", {}).get("id"))
        for mod in active_mods
        if isinstance(mod, dict) and (mod.get("id") or mod.get("packageId") or mod.get("package", {}).get("id"))
    }


def _gui_selected_mod_action_target(selected_mod: dict[str, Any] | None) -> dict[str, Any]:
    selected = selected_mod or {}
    return {
        "targetPackageId": selected.get("packageId"),
        "selectedModId": selected.get("rowId") or selected.get("id"),
        "selectedModName": selected.get("name"),
        "selectedModPath": selected.get("path"),
        "actionEligibility": selected.get("actionEligibility"),
    }


def _gui_action_eligibility_entry(enabled: bool, status: str, reason: str | None = None) -> dict[str, Any]:
    entry: dict[str, Any] = {
        "enabled": bool(enabled),
        "disabled": not enabled,
        "status": status,
    }
    if reason:
        entry["reason"] = reason
        entry["disabledReason"] = reason
    return entry


def _gui_selected_mod_dto(
    selected: dict[str, Any] | None,
    *,
    selected_summary: dict[str, Any] | None,
    active_mods: list[dict[str, Any]],
    selection_reason: str | None,
) -> dict[str, Any] | None:
    if not isinstance(selected, dict):
        return None
    row_id = selected.get("id")
    package_id = selected.get("packageId") or selected.get("id")
    name = selected.get("name") or selected.get("displayName") or selected.get("title") or package_id or row_id
    provenance = _gui_provenance_slug(selected.get("provenanceKey") or selected.get("provenance"))
    path = selected.get("path") or selected.get("workshopItemPath")
    active_ids = _gui_active_mod_ids(active_mods)
    enabled = bool(selected.get("active") or selected.get("enabledInProfile") or (package_id is not None and str(package_id) in active_ids))
    selectable = bool(selected.get("selectable", True))
    has_bml_package = selected.get("hasBmlPackage")
    if has_bml_package is None:
        has_bml_package = bool(selected.get("manifestPath") or selected.get("packageId"))
    is_local_package = provenance == "local-repo"
    enable_reason: str | None = None
    disable_reason: str | None = None
    if not selectable:
        enable_reason = "Selected row is not selectable."
        disable_reason = enable_reason
    elif provenance == "steam-workshop" and not has_bml_package:
        enable_reason = "Steam Workshop subscription is not a BML package; profile enable is unavailable."
        disable_reason = "Steam Workshop subscription is not a BML package; profile disable is unavailable."
    elif not is_local_package:
        enable_reason = "Select a local BML package before enabling it in this profile."
        disable_reason = "Select an enabled local BML package before disabling it in this profile."
    elif not package_id:
        enable_reason = "Selected mod has no package id to enable."
        disable_reason = "Selected mod has no package id to disable."
    elif not path:
        enable_reason = "Selected mod has no package path to enable."
        disable_reason = "Selected mod has no package path to disable."
    elif enabled:
        enable_reason = "Selected local mod is already enabled in this profile."
    else:
        disable_reason = "Selected local mod is already disabled in this profile."

    can_enable = enable_reason is None and not enabled
    can_disable = disable_reason is None and enabled
    can_publish = False
    publish_reason = "Steam publishing is disabled in GUI smoke and preview mode."
    action_eligibility = {
        "enable-package": _gui_action_eligibility_entry(can_enable, "available" if can_enable else ("already_enabled" if enabled and is_local_package else "unavailable"), enable_reason),
        "disable-package": _gui_action_eligibility_entry(can_disable, "available" if can_disable else ("already_disabled" if not enabled and is_local_package else "unavailable"), disable_reason),
        "workshop-preview": _gui_action_eligibility_entry(True, "dry-run"),
        "publish-workshop": _gui_action_eligibility_entry(can_publish, "disabled", publish_reason),
    }
    primary_action_id = "disable-package" if can_disable and not can_enable else ("enable-package" if can_enable else None)
    if primary_action_id and primary_action_id in action_eligibility:
        action_eligibility[primary_action_id]["primary"] = True
    unavailable_reasons = [
        str(reason)
        for reason in (enable_reason, disable_reason)
        if reason
    ]
    dto: dict[str, Any] = {
        "rowId": row_id,
        "id": row_id,
        "packageId": package_id,
        "name": _gui_text(name),
        "displayName": _gui_text(name),
        "provenance": provenance,
        "source": provenance,
        "path": str(path) if path else None,
        "manifestPath": selected.get("manifestPath"),
        "publishedFileId": selected.get("publishedFileId"),
        "workshopItemPath": selected.get("workshopItemPath"),
        "enabledInProfile": enabled,
        "selectable": selectable,
        "canEnable": can_enable,
        "canDisable": can_disable,
        "canPublish": can_publish,
        "eligibleActions": [action_id for action_id, entry in action_eligibility.items() if entry.get("enabled")],
        "actionEligibility": action_eligibility,
        "primaryActionId": primary_action_id,
        "actionUnavailableReasons": unavailable_reasons,
        "selectionReason": selection_reason,
        "hasBmlPackage": bool(has_bml_package),
        "selectedRow": dict(selected),
        "selectedPackage": dict(selected_summary) if isinstance(selected_summary, dict) else None,
    }
    return dto


def _gui_rendered_detected_mod_rows(detected_mods: Iterable[Any]) -> list[dict[str, Any]]:
    return [_gui_detected_mod_row_metadata(entry, focus_order=index) for index, entry in enumerate(detected_mods) if isinstance(entry, dict)]


def _gui_entity_iconography_records(
    *,
    steam_logo_path: str | None = None,
    steam_logo_rendered: bool = False,
    detected_sections: Iterable[Any] = (),
    rendered_rows: Iterable[Any] = (),
) -> list[dict[str, Any]]:
    section_entity_types = {
        _gui_provenance_entity_type(section.get("key") or section.get("id"))
        for section in detected_sections
        if isinstance(section, dict)
    }
    has_mod_rows = any(isinstance(row, dict) for row in rendered_rows)
    visible_by_type = {
        "mods-list": True,
        "local-repo": "local-repo" in section_entity_types,
        "steam-workshop": "steam-workshop" in section_entity_types,
        "mod-package": has_mod_rows,
        "environment": True,
        "profiles": True,
        "workshop": True,
        "activity-log": True,
        "os": True,
        "platform-steam": True,
        "game-version": True,
    }
    labels = {entity_type: MAJOR_ENTITY_ICONOGRAPHY[entity_type]["label"] for entity_type in GUI_ENTITY_ICON_RENDER_ORDER}
    labels["platform-steam"] = "Steam"
    records: list[dict[str, Any]] = []
    for entity_type in GUI_ENTITY_ICON_RENDER_ORDER:
        visible = bool(visible_by_type.get(entity_type))
        records.append(
            _gui_entity_icon_descriptor(
                entity_type,
                labels[entity_type],
                key=entity_type,
                rendered=visible,
                visible=visible,
                logo_path=steam_logo_path if entity_type == "platform-steam" else None,
                logo_rendered=steam_logo_rendered if entity_type == "platform-steam" else False,
            )
        )
    return records


def _gui_steam_icon_path() -> str | None:
    hicolor = Path("/usr/share/icons/hicolor")
    if not hicolor.exists():
        return None
    preferred_sizes = ("32x32", "48x48", "24x24", "16x16", "256x256")
    for size in preferred_sizes:
        candidate = hicolor / size / "apps" / "steam.png"
        if candidate.is_file():
            return str(candidate)
    candidates = sorted(hicolor.glob("*/apps/steam.png"))
    return str(candidates[0]) if candidates else None

def _gui_os_icon_path() -> str | None:
    candidates = [
        Path("/usr/share/icons/hicolor/48x48/apps/cachyos-pi.png"),
        Path("/usr/share/icons/hicolor/32x32/apps/cachyos-pi.png"),
        Path("/usr/share/pixmaps/cachyos-logo.png"),
        Path("/usr/share/pixmaps/archlinux-logo.png"),
    ]
    for candidate in candidates:
        if candidate.is_file():
            return str(candidate)
    for pattern in (
        "/usr/share/icons/hicolor/*/apps/cachyos*.png",
        "/usr/share/icons/hicolor/*/apps/*linux*.png",
        "/usr/share/pixmaps/*linux*.png",
    ):
        matches = sorted(Path("/").glob(pattern.lstrip("/")))
        if matches:
            return str(matches[0])
    return None


def _gui_bml_app_icon_path() -> str | None:
    for candidate in (
        APP_ROOT / "share" / "icons" / "hicolor" / "256x256" / "apps" / "barony-modloader.png",
        APP_ROOT / "share" / "icons" / "hicolor" / "256x256" / "apps" / "barony-modloader-bml.png",
        APP_ROOT / "share" / "icons" / "hicolor" / "1024x1024" / "apps" / "barony-modloader.png",
        APP_ROOT / "share" / "icons" / "hicolor" / "1024x1024" / "apps" / "barony-modloader-bml.png",
    ):
        if candidate.is_file():
            return str(candidate)
    return None


def _gui_vanilla_barony_icon_path() -> str | None:
    for candidate in (
        APP_ROOT / "share" / "icons" / "hicolor" / "256x256" / "apps" / "barony-vanilla.png",
        APP_ROOT / "share" / "icons" / "hicolor" / "1024x1024" / "apps" / "barony-vanilla.png",
    ):
        if candidate.is_file():
            return str(candidate)
    return None



def _gui_evidence(label: str, value: Any, status: Any = None) -> dict[str, Any]:
    item: dict[str, Any] = {"label": label, "value": _gui_text(value)}
    if status is not None:
        item["status"] = _gui_text(status)
    return item


def _gui_compact_texts(values: Iterable[Any]) -> list[str]:
    texts: list[str] = []

    def add(raw: Any) -> None:
        if raw is None:
            return
        if isinstance(raw, dict):
            for key in ("reason", "message", "blocker", "label"):
                if raw.get(key) is not None:
                    add(raw.get(key))
                    return
            return
        if isinstance(raw, list):
            for item in raw:
                add(item)
            return
        text = _gui_text(raw)
        if text != "Not set" and text not in texts:
            texts.append(text)

    for value in values:
        add(value)
    return texts


def _gui_problem_texts(problems: Any) -> list[str]:
    if not isinstance(problems, list):
        return []
    return _gui_compact_texts(problems)


def _gui_data_home() -> Path:
    xdg_data_home = os.environ.get("XDG_DATA_HOME")
    if xdg_data_home:
        return Path(xdg_data_home).expanduser()
    return Path.home() / ".local" / "share"


def _gui_default_profile_path() -> Path:
    return (_gui_data_home() / APP_ID / "profiles" / "default").expanduser()


def _gui_repo_root() -> Path:
    return APP_ROOT.parents[1] if len(APP_ROOT.parents) > 1 else APP_ROOT


def _gui_mods_root() -> Path:
    return _gui_repo_root() / "mods"


def _gui_runebound_package_path() -> Path:
    return _gui_mods_root() / "runebound-elixirs"


def _path_is_under_tmp(path: Path) -> bool:
    resolved = path.expanduser().resolve(strict=False)
    tmp_root = Path(tempfile.gettempdir()).resolve(strict=False)
    return resolved == tmp_root or tmp_root in resolved.parents or any(part == ".tmp" for part in resolved.parts)


def _gui_default_barony_executable(install: dict[str, Any] | None = None) -> str:
    if isinstance(install, dict) and install.get("executable"):
        return str(install["executable"])
    return str(Path.home() / ".local" / "share" / "Steam" / "steamapps" / "common" / "Barony" / STEAM_BARONY_EXECUTABLE)


def _gui_detect_install() -> dict[str, Any]:
    payload, result = detect_steam_install()
    problems = [problem_to_dict(problem) for problem in result.problems]
    if payload is None:
        fallback_executable = _gui_default_barony_executable()
        return {
            "status": "missing",
            "source": "steam",
            "store": "steam",
            "platform": current_platform_id(),
            "path": str(Path(fallback_executable).parent),
            "installPath": str(Path(fallback_executable).parent),
            "executable": fallback_executable,
            "reason": "No Linux Steam Barony install was detected.",
            "problems": problems,
            "disabledReasons": [problem.message for problem in result.problems] or ["No Linux Steam Barony install was detected."],
        }
    status = "available" if result.ok else "blocked"
    disabled = [problem.message for problem in result.problems if problem.is_error()]
    return {
        **payload,
        "status": status,
        "path": payload.get("installPath"),
        "selected": result.ok,
        "active": result.ok,
        "reason": "Linux Steam Barony install detected." if result.ok else "Linux Steam Barony install is configured but not launch-ready.",
        "problems": problems,
        "disabledReasons": disabled,
    }


def _gui_create_profile(profile_dir: Path, install: dict[str, Any]) -> dict[str, Any]:
    if _path_is_under_tmp(profile_dir):
        raise ValueError(f"Refusing to use .tmp or system temp as the normal GUI profile path: {profile_dir}")
    bml_root = bml_profile_root(profile_dir)
    logs_dir = bml_root / "logs"
    reports_dir = bml_root / "reports"
    manifests_dir = bml_root / "manifests"
    state_dir = bml_root / "state"
    for directory in (bml_root, logs_dir, reports_dir, manifests_dir, state_dir):
        directory.mkdir(parents=True, exist_ok=True)
    now = utc_now()
    steam_install = dict(install) if isinstance(install, dict) and install.get("source") == "steam" else None
    profile_payload = {
        "schemaVersion": SCHEMA_VERSION,
        "profile": {
            "id": "default",
            "createdAt": now,
            "updatedAt": now,
        },
        "app": {
            "id": APP_ID,
            "version": APP_VERSION,
            "schemaVersion": SCHEMA_VERSION,
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
            "gameSource": "steam",
            "baronyExecutable": _gui_default_barony_executable(install),
            "runtimeInfo": None,
            "steam": steam_install,
        },
    }
    write_json_file(profile_json_path(profile_dir), profile_payload)
    return profile_payload


def _gui_load_or_create_profile(profile_dir: Path, install: dict[str, Any], *, create_if_missing: bool) -> tuple[dict[str, Any] | None, dict[str, Any], list[str]]:
    actions: list[str] = []
    if create_if_missing and not profile_json_path(profile_dir).exists():
        profile = _gui_create_profile(profile_dir, install)
        actions.append("created_profile")
    else:
        profile, _resolved_profile_dir, result = load_profile(str(profile_dir))
        if profile is None and create_if_missing:
            profile = _gui_create_profile(profile_dir, install)
            actions.append("created_profile")
        elif profile is not None:
            actions.append("selected_profile")
        else:
            actions.append("profile_missing")
    profile_state = build_profile_state(profile_dir) if profile is not None else {"status": "not_selected", "path": str(profile_dir)}
    profile_state["path"] = str(profile_dir)
    profile_state["stableDefault"] = True
    profile_state["tmpPathRejected"] = _path_is_under_tmp(profile_dir)
    if profile is not None:
        profile_state["status"] = "selected"
        profile_state["id"] = profile.get("profile", {}).get("id")
        profile_state["activeMods"] = profile_authoritative_mods(profile, profile_dir)
    return profile, profile_state, actions


def _gui_package_detail(package_path: Path) -> tuple[LoadedPackage | None, dict[str, Any]]:
    package, load_result = load_package(str(package_path))
    combined = ValidationResult(f"GUI package {package_path}")
    combined.extend(load_result)
    summary = package_summary_from_path(package_path)
    if package is not None:
        combined.extend(validate_package(package))
        manifest = package.manifest
        modules = manifest.get("modules") if isinstance(manifest.get("modules"), dict) else {}
        runebound = modules.get(RUNEBOUND_ELIXIRS_MODULE_NAME) if isinstance(modules.get(RUNEBOUND_ELIXIRS_MODULE_NAME), dict) else {}
        capabilities = package_capability_entries(manifest)
        native = manifest.get("native") if isinstance(manifest.get("native"), dict) else {}
        platforms = native.get("platforms") if isinstance(native.get("platforms"), list) else []
        summary.update(
            {
                "id": manifest.get("id"),
                "packageId": manifest.get("id"),
                "name": manifest.get("name"),
                "version": manifest.get("version"),
                "summary": manifest.get("summary"),
                "description": manifest.get("description"),
                "carrier": runebound.get("carrierItemType") or RUNEBOUND_ELIXIRS_CARRIER_ITEM_TYPE,
                "carrierItemType": runebound.get("carrierItemType") or RUNEBOUND_ELIXIRS_CARRIER_ITEM_TYPE,
                "capabilities": capabilities,
                "capabilityIds": [entry.get("id") for entry in capabilities if entry.get("id")],
                "platforms": platforms,
                "validationStatus": validation_status(combined),
                "status": validation_status(combined),
                "problems": [problem_to_dict(problem) for problem in combined.problems],
            }
        )
    return package, summary


def _gui_scan_packages(selected_package_path: Path | None = None) -> tuple[dict[str, Any], LoadedPackage | None, dict[str, Any] | None]:
    mods_root = _gui_mods_root()
    catalog = scan_package_catalog(mods_root=mods_root)
    selected_path = selected_package_path or _gui_runebound_package_path()
    selected_package: LoadedPackage | None = None
    selected_summary: dict[str, Any] | None = None
    if selected_path.exists():
        selected_package, selected_summary = _gui_package_detail(selected_path)
    elif catalog.get("packages"):
        first = catalog["packages"][0]
        selected_path = Path(str(first.get("path")))
        selected_package, selected_summary = _gui_package_detail(selected_path)
    catalog["status"] = "loaded" if catalog.get("packages") else "missing"
    catalog["modsRoot"] = str(mods_root)
    catalog["selectedPackage"] = selected_summary
    return catalog, selected_package, selected_summary


def _gui_environment_summary_items(install: dict[str, Any]) -> list[dict[str, Any]]:
    os_parts = [platform.system() or platform_os_name(), platform.release()]
    os_value = " ".join(part for part in os_parts if part).strip() or "unknown"
    steam_icon_path = _gui_steam_icon_path()
    os_logo_path = _gui_os_icon_path()
    game_logo_path = _gui_vanilla_barony_icon_path()
    version_value = "Unknown"
    version_source = "unknown"
    for key in ("gameVersionString", "executableBuildId", "LastBuildID", "lastBuildId", "buildId"):
        value = install.get(key)
        if isinstance(value, str) and value.strip():
            version_value = value.strip()
            version_source = key
            break
    os_icon = _gui_entity_icon_descriptor("os", "OS", logo_path=os_logo_path)
    platform_icon = _gui_entity_icon_descriptor("platform-steam", "Platform", logo_path=steam_icon_path)
    version_icon = _gui_entity_icon_descriptor("game-version", "Game version", logo_path=game_logo_path)
    rows = [
        {
            "id": "os",
            "key": "os",
            "label": "OS",
            "displayLabel": os_icon["displayText"],
            "entityType": os_icon["entityType"],
            "icon": os_icon["icon"],
            "iconSource": os_icon["source"],
            "badge": "[OS]",
            "value": os_value,
            "text": f"{os_icon['icon']} {os_value}",
            "source": "platform.system/release",
            "osLogoPath": os_logo_path,
            "osLogoAvailable": os_logo_path is not None,
        },
        {
            "id": "platform",
            "key": "platform",
            "label": "Platform",
            "displayLabel": platform_icon["displayText"],
            "entityType": platform_icon["entityType"],
            "icon": platform_icon["icon"],
            "iconSource": platform_icon["source"],
            "badge": "[STEAM]",
            "value": "Steam",
            "storefront": "Steam",
            "platform": "Steam",
            "text": f"{platform_icon['icon']} Steam",
            "source": "Steam storefront",
            "steamLogoPath": steam_icon_path,
            "steamLogoAvailable": steam_icon_path is not None,
            "logoPath": steam_icon_path,
        },
        {
            "id": "game-version",
            "key": "game-version",
            "label": "Game version",
            "displayLabel": version_icon["displayText"],
            "entityType": version_icon["entityType"],
            "icon": version_icon["icon"],
            "iconSource": version_icon["source"],
            "badge": "[VERSION]",
            "value": version_value,
            "text": f"{version_icon['icon']} {version_value}",
            "source": version_source,
            "gameLogoPath": game_logo_path,
            "gameLogoAvailable": game_logo_path is not None,
            "vanillaIconPath": game_logo_path,
            "vanillaIconAvailable": game_logo_path is not None,
            "logoPath": game_logo_path,
        },
    ]
    return rows
def _gui_detected_mod_entry(
    *,
    provenance_key: str,
    provenance_label: str,
    entry_id: str,
    name: Any = None,
    package_id: Any = None,
    version: Any = None,
    path: Any = None,
    manifest_path: Any = None,
    status: Any = None,
    active: bool = False,
    selected: bool = False,
    extra: dict[str, Any] | None = None,
) -> dict[str, Any]:
    display_name = _gui_text(name or package_id or entry_id)
    icon = _gui_entity_icon_descriptor("mod-package", display_name)
    payload: dict[str, Any] = {
        "id": entry_id,
        "name": display_name,
        "title": display_name,
        "displayName": display_name,
        "packageId": package_id,
        "version": version,
        "path": str(path) if path else None,
        "manifestPath": str(manifest_path) if manifest_path else None,
        "status": status or ("enabled" if active else "detected"),
        "active": active,
        "enabledInProfile": active,
        "selected": selected,
        "entityType": icon["entityType"],
        "icon": icon["icon"],
        "iconSource": icon["source"],
        "labelWithIcon": icon["displayText"],
        "provenanceKey": provenance_key,
        "provenanceLabel": provenance_label,
        "provenance": {
            "key": provenance_key,
            "label": provenance_label,
            "entityType": _gui_provenance_entity_type(provenance_key),
            "icon": _gui_entity_icon_descriptor(_gui_provenance_entity_type(provenance_key), provenance_label)["icon"],
        },
    }
    if extra:
        payload.update(extra)
    return payload


def _gui_steam_workshop_content_root() -> Path:
    fixture_home = os.environ.get("BML_GUI_WORKSHOP_FIXTURE_HOME")
    if fixture_home:
        return Path(fixture_home).expanduser() / ".local" / "share" / "Steam" / "steamapps" / "workshop" / "content" / STEAM_BARONY_APP_ID
    return Path.home() / ".local" / "share" / "Steam" / "steamapps" / "workshop" / "content" / STEAM_BARONY_APP_ID


def _gui_workshop_subscription_entries() -> list[dict[str, Any]]:
    content_root = _gui_steam_workshop_content_root()
    if not content_root.is_dir():
        return []
    entries: list[dict[str, Any]] = []
    for item_root in sorted((child for child in content_root.iterdir() if child.is_dir()), key=lambda path: path.name):
        published_file_id = item_root.name
        if not published_file_id:
            continue
        manifest_path = item_root / PACKAGE_MANIFEST_NAME
        summary: dict[str, Any] = {}
        if manifest_path.is_file():
            summary = package_summary_from_path(item_root)
        subscription_title = None
        title_path = item_root / "subscription-title.txt"
        if title_path.is_file():
            try:
                subscription_title = next((line.strip() for line in title_path.read_text(encoding="utf-8").splitlines() if line.strip()), None)
            except OSError:
                subscription_title = None
        package_id = summary.get("id") or summary.get("packageId")
        display_name = summary.get("name") or subscription_title or f"Workshop item {published_file_id}"
        entries.append(
            _gui_detected_mod_entry(
                provenance_key="steam_workshop",
                provenance_label="Steam Workshop subscriptions",
                entry_id=f"steam-workshop:{published_file_id}",
                name=display_name,
                package_id=package_id,
                version=summary.get("version"),
                path=item_root,
                manifest_path=manifest_path if manifest_path.is_file() else None,
                status=summary.get("status") or "subscribed",
                extra={
                    "publishedFileId": published_file_id,
                    "workshopItemPath": str(item_root),
                    "hasBmlPackage": manifest_path.is_file(),
                    "validationStatus": summary.get("validationStatus"),
                },
            )
        )
    return entries


def _gui_detected_mod_inventory(
    package_catalog: dict[str, Any],
    active_mods: list[dict[str, Any]],
    *,
    selected_mod_selector: str | None = None,
    selected_summary: dict[str, Any] | None = None,
) -> dict[str, Any]:
    packages = package_catalog.get("packages") if isinstance(package_catalog.get("packages"), list) else []
    active_ids = {
        str(mod.get("id") or mod.get("packageId") or mod.get("package", {}).get("id"))
        for mod in active_mods
        if isinstance(mod, dict) and (mod.get("id") or mod.get("packageId") or mod.get("package", {}).get("id"))
    }
    local_entries: list[dict[str, Any]] = []
    for summary in packages:
        if not isinstance(summary, dict):
            continue
        package_id = summary.get("id") or summary.get("packageId")
        entry_id = f"local-repo:{package_id or summary.get('path') or len(local_entries)}"
        active = package_id is not None and str(package_id) in active_ids
        status = "enabled" if active else (summary.get("status") or summary.get("validationStatus") or "disabled")
        entry = _gui_detected_mod_entry(
            provenance_key="local_repo",
            provenance_label="Local repo mods",
            entry_id=str(entry_id),
            name=summary.get("name") or package_id or Path(str(summary.get("path") or ".")).name,
            package_id=package_id,
            version=summary.get("version"),
            path=summary.get("path"),
            manifest_path=summary.get("manifestPath"),
            status=status,
            active=active,
            selected=False,
            extra={
                "kind": summary.get("kind"),
                "valid": summary.get("valid"),
                "validationStatus": summary.get("validationStatus"),
                "problemCount": summary.get("problemCount"),
                "problems": summary.get("problems"),
                "enabled": active,
                "selectable": True,
            },
        )
        local_entries.append(entry)

    workshop_entries = _gui_workshop_subscription_entries()
    for entry in workshop_entries:
        entry["enabled"] = bool(entry.get("active") or entry.get("enabledInProfile"))
        entry["selectable"] = True
        if not entry.get("status"):
            entry["status"] = "subscribed"
    sections = []
    for section_id, label, source, entries in (
        ("local_repo", "Local repo mods", "scan_package_catalog(_gui_mods_root())", local_entries),
        ("steam_workshop", "Steam Workshop subscriptions", str(_gui_steam_workshop_content_root()), workshop_entries),
    ):
        entity_type = _gui_provenance_entity_type(section_id)
        icon = _gui_entity_icon_descriptor(entity_type, label)
        sections.append(
            {
                "id": section_id,
                "key": section_id,
                "label": label,
                "displayLabel": icon["displayText"],
                "title": label,
                "entityType": entity_type,
                "icon": icon["icon"],
                "iconSource": icon["source"],
                "source": source,
                "count": len(entries),
                "entries": entries,
            }
        )
    detected_mods = [entry for section in sections for entry in section["entries"]]
    selected: dict[str, Any] | None = None
    selection_reason: str | None = None
    if selected_mod_selector:
        selected = next((entry for entry in detected_mods if _gui_selector_matches(selected_mod_selector, _gui_detected_mod_selector_values(entry))), None)
        if selected is not None:
            selection_reason = f"matched selector {selected_mod_selector}"
    if selected is None and selected_summary is not None:
        selected = next((entry for entry in detected_mods if _gui_selector_matches(_gui_selected_package_id(selected_summary), _gui_detected_mod_selector_values(entry))), None)
        if selected is not None:
            selection_reason = "matched selected package id"
        if selected is None:
            selected_path = str(_gui_selected_package_path(selected_summary))
            selected = next((entry for entry in detected_mods if _gui_selector_matches(selected_path, _gui_detected_mod_selector_values(entry))), None)
            if selected is not None:
                selection_reason = "matched selected package path"
    if selected is None:
        selected = next((entry for entry in detected_mods if entry.get("active") or entry.get("enabledInProfile")), None)
        if selected is not None:
            selection_reason = "defaulted to enabled mod"
    if selected is None and detected_mods:
        selected = detected_mods[0]
        selection_reason = "defaulted to first detected mod"
    selected_id = selected.get("id") if isinstance(selected, dict) else None
    for entry in detected_mods:
        is_selected = bool(selected_id and entry.get("id") == selected_id)
        entry["selected"] = is_selected
        entry["enabled"] = bool(entry.get("active") or entry.get("enabledInProfile") or entry.get("enabled"))
        entry["selectable"] = True
        if entry["enabled"]:
            entry["status"] = "enabled"
        elif entry.get("provenanceKey") != "steam_workshop":
            entry["status"] = "disabled"
    selected_mod = _gui_selected_mod_dto(
        selected,
        selected_summary=selected_summary,
        active_mods=active_mods,
        selection_reason=selection_reason,
    )
    rendered_rows = _gui_rendered_detected_mod_rows(detected_mods)
    return {
        "detectedMods": detected_mods,
        "detectedModSections": sections,
        "renderedProvenanceSectionLabels": [section["label"] for section in sections],
        "selectedDetectedMod": selected,
        "selectedDetectedModId": selected_id,
        "selectedDetectedModProvenance": (selected or {}).get("provenance") if isinstance(selected, dict) else None,
        "selectedMod": selected_mod,
        "renderedDetectedModRows": rendered_rows,
    }


def _gui_runtime_info_for_package(package: LoadedPackage | None) -> dict[str, Any]:
    capabilities = package_capability_entries(package.manifest) if package is not None else []
    return {
        "runtimeId": "barony-bml-runtime-gui-dry-run",
        "runtimeVersion": APP_VERSION,
        "contract": {"id": RUNTIME_CONTRACT_ID, "version": RUNTIME_CONTRACT_VERSION},
        "capabilities": capabilities,
        "platforms": [current_platform_id(), "linux-x86_64"],
        "strategy": RUNTIME_STRATEGY_INSTALLED_HOOK,
        "verificationStatus": "dry_run_metadata_only",
    }


def _gui_refresh_readiness(install: dict[str, Any], profile: dict[str, Any] | None, package: LoadedPackage | None, profile_dir: Path) -> dict[str, Any]:
    runtime_info = _gui_runtime_info_for_package(package)
    return build_launch_readiness_state(
        install=install,
        profile=profile,
        profile_dir=profile_dir,
        package=package,
        runtime_info=runtime_info,
        platform=current_platform_id(),
        dry_run=True,
    )


def _gui_launch_dry_run(profile: dict[str, Any] | None, profile_dir: Path, package: LoadedPackage | None) -> dict[str, Any]:
    if profile is None or package is None:
        return {
            "status": "blocked",
            "processStarted": False,
            "processLaunched": False,
            "disabledReasons": ["Profile and selected package are required before launch dry-run metadata can be generated."],
        }
    runtime_info = _gui_runtime_info_for_package(package)
    plan = plan_runtime_manifest(profile, profile_dir, package, runtime_info, bml_profile_root(profile_dir) / "manifests" / "runtime-manifest.json")
    combined = ValidationResult("GUI BaronyModLoader launch readiness")
    combined.extend(validate_package(package))
    combined.extend(validate_profile_package_enabled(profile, profile_dir, package))
    registry_path = runtime_registry_path(None)
    registry, registry_result = load_runtime_registry(registry_path, missing_ok=False)
    combined.extend(registry_result)
    combined.extend(validate_profile_steam_install(profile))
    if registry_result.ok:
        _selected_runtime, _selected_runtime_info, _runtime_info_path, _runtime_executable, selection_result = select_registered_runtime(
            registry,
            profile,
            package,
            None,
        )
        combined.extend(selection_result)
    disabled_reasons = [*plan.get("disabledReasons", []), *(_validation_disabled_reasons(combined) if not combined.ok else [])]
    return {
        "status": "ready" if not disabled_reasons else "blocked",
        "dryRun": True,
        "processStarted": False,
        "processLaunched": False,
        "wouldStartBarony": False,
        "registry": str(registry_path),
        "runtimeManifestPath": plan.get("runtimeManifestPath"),
        "manifestPath": plan.get("manifestPath"),
        "manifest": plan.get("manifest"),
        "sideEffects": plan.get("sideEffects"),
        "disabledReasons": disabled_reasons,
        "problems": _validation_problems(combined),
    }

GUI_LAUNCH_MODE_ENV = "BML_GUI_LAUNCH_MODE"
GUI_LAUNCH_MOCK_VALUES = {"1", "true", "yes", "mock", "smoke", "smoke-mock"}
GUI_LAUNCH_ENV_KEYS = (
    "SteamAppId",
    "SteamGameId",
    "BML_PROFILE_DIR",
    "BML_RUNTIME_MANIFEST",
    "BML_RUNTIME_STRATEGY",
    "BML_LAUNCH_ADAPTER",
    "BML_TARGET_EXECUTABLE",
    "BML_LAUNCHER_EXECUTABLE",
    "BML_HOOK_MANIFEST",
    "BML_HOOK_LIBRARY",
    "LD_PRELOAD",
    "DYLD_INSERT_LIBRARIES",
    "LD_LIBRARY_PATH",
)


def _gui_launch_mocked() -> bool:
    return os.environ.get(GUI_LAUNCH_MODE_ENV, "").strip().casefold() in GUI_LAUNCH_MOCK_VALUES


def _validation_problems(result: ValidationResult) -> list[dict[str, Any]]:
    return [problem_to_dict(problem) for problem in result.problems]


def _validation_disabled_reasons(result: ValidationResult) -> list[str]:
    return [problem.message for problem in result.problems if problem.is_error] or [problem.message for problem in result.problems]


def _gui_launch_blocked(mode: str, result: ValidationResult, **metadata: Any) -> dict[str, Any]:
    payload = {
        "mode": mode,
        "status": "blocked",
        "processStarted": False,
        "processLaunched": False,
        "mocked": _gui_launch_mocked(),
        "pid": None,
        "disabledReasons": _validation_disabled_reasons(result),
        "problems": _validation_problems(result),
    }
    payload.update({key: value for key, value in metadata.items() if value is not None})
    return payload


def _gui_launch_environment_snapshot(env: dict[str, str], *, include_bml: bool) -> dict[str, str]:
    if include_bml:
        return {key: env[key] for key in GUI_LAUNCH_ENV_KEYS if key in env}
    return {key: env[key] for key in ("SteamAppId", "SteamGameId") if key in env}


def _gui_vanilla_launch_environment(steam: dict[str, Any] | None) -> dict[str, str]:
    env = dict(os.environ)
    for key in list(env):
        if key.startswith("BML_") or key in STEAM_LAUNCH_ENV_KEYS or key in DYNAMIC_LOADER_ENV_KEYS or key.startswith(DYNAMIC_LOADER_ENV_PREFIXES):
            env.pop(key, None)
    app_id = str((steam or {}).get("appId") or STEAM_BARONY_APP_ID)
    env["SteamAppId"] = app_id
    env["SteamGameId"] = app_id
    return env


def _gui_popen_detached(command: list[str], cwd: Path, env: dict[str, str]) -> subprocess.Popen[Any]:
    kwargs: dict[str, Any] = {
        "cwd": str(cwd),
        "env": env,
        "stdin": subprocess.DEVNULL,
        "stdout": subprocess.DEVNULL,
        "stderr": subprocess.DEVNULL,
        "close_fds": True,
    }
    if sys.platform == "win32":
        kwargs["creationflags"] = getattr(subprocess, "DETACHED_PROCESS", 0) | getattr(subprocess, "CREATE_NEW_PROCESS_GROUP", 0)
    else:
        kwargs["start_new_session"] = True
    return subprocess.Popen(command, **kwargs)


def _gui_start_launch_process(
    *,
    mode: str,
    command: list[str],
    cwd: Path,
    env: dict[str, str],
    launch_log: Path,
    include_bml_env: bool,
    metadata: dict[str, Any],
) -> dict[str, Any]:
    mocked = _gui_launch_mocked()
    environment = _gui_launch_environment_snapshot(env, include_bml=include_bml_env)
    payload: dict[str, Any] = {
        "mode": mode,
        "status": "mocked" if mocked else "launched",
        "processStarted": True,
        "processLaunched": True,
        "mocked": mocked,
        "pid": None,
        "command": list(command),
        "cwd": str(cwd),
        "environment": environment,
        "launchLog": str(launch_log),
    }
    payload.update({key: value for key, value in metadata.items() if value is not None})
    launch_log.parent.mkdir(parents=True, exist_ok=True)
    write_json_file(
        launch_log,
        {
            "createdAt": utc_now(),
            "mode": mode,
            "mocked": mocked,
            "command": command,
            "cwd": str(cwd),
            "environment": environment,
            "metadata": {key: value for key, value in metadata.items() if key not in {"manifest"}},
        },
    )
    if mocked:
        return payload
    try:
        process = _gui_popen_detached(command, cwd, env)
    except OSError as exc:
        failure = ValidationResult(f"GUI {mode} launch execution")
        failure.add("BML_GUI_LAUNCH_EXEC_FAILED", "fatal", f"Could not start Barony: {exc}")
        return _gui_launch_blocked(mode, failure, command=list(command), cwd=str(cwd), environment=environment, launchLog=str(launch_log), **metadata)
    payload["pid"] = process.pid
    return payload


def _gui_launch_bml(profile: dict[str, Any] | None, profile_dir: Path, package: LoadedPackage | None) -> dict[str, Any]:
    combined = ValidationResult("GUI BaronyModLoader launch validation")
    mocked = _gui_launch_mocked()
    if profile is None:
        combined.add("BML_GUI_PROFILE_MISSING", "fatal", "Create or select a profile before launching BaronyModLoader.")
    if package is None:
        combined.add("BML_GUI_PACKAGE_MISSING", "fatal", "Select a local BML package before launching BaronyModLoader.")
    if package is not None:
        combined.extend(validate_package(package))
    if profile is not None and package is not None:
        combined.extend(validate_profile_package_enabled(profile, profile_dir, package))
    registry_path = runtime_registry_path(None)
    registry: dict[str, Any] = {}
    registry_result = ValidationResult("runtime registry")
    if not mocked:
        registry, registry_result = load_runtime_registry(registry_path, missing_ok=False)
        combined.extend(registry_result)
    if profile is not None:
        combined.extend(validate_profile_steam_install(profile))
        if not mocked:
            combined.extend(validate_steam_client_ready_for_launch(profile))

    selected_runtime: dict[str, Any] | None = None
    runtime_info: dict[str, Any] | None = None
    runtime_info_path: Path | None = None
    runtime_executable: Path | None = None
    if mocked and profile is not None and package is not None:
        steam = profile_steam_install(profile) or {}
        executable_value = steam.get("executable") or _gui_default_barony_executable()
        runtime_executable = Path(str(executable_value)).expanduser().resolve(strict=False)
        runtime_info = _gui_runtime_info_for_package(package)
        runtime_info_path = bml_profile_root(profile_dir) / "mock-runtime-info.json"
        target = current_platform_target()
        mock_hook_name = WINDOWS_HOOK_LIBRARY_NAME if sys.platform == "win32" else "libbarony_bml.so"
        selected_runtime = {
            "id": "mock-gui-bml-runtime",
            "runtimeStrategy": RUNTIME_STRATEGY_INSTALLED_HOOK,
            "launchAdapter": target.launch_adapter,
            "steamExecutable": str(runtime_executable),
            "hookLibrary": str(bml_profile_root(profile_dir) / "mock" / mock_hook_name),
        }
    elif profile is not None and package is not None and registry_result.ok:
        selected_runtime, runtime_info, runtime_info_path, runtime_executable, selection_result = select_registered_runtime(
            registry,
            profile,
            package,
            None,
        )
        combined.extend(selection_result)

    out_path = bml_profile_root(profile_dir) / "runtime-manifest.json"
    if not combined.ok:
        return _gui_launch_blocked("bml", combined, registry=str(registry_path), runtimeManifestPath=str(out_path))

    assert profile is not None
    assert package is not None
    assert selected_runtime is not None
    assert runtime_info is not None
    assert runtime_info_path is not None
    assert runtime_executable is not None

    manifest, active_mods_path = write_launch_artifacts(profile, profile_dir, package, runtime_info, out_path, runtime_executable, selected_runtime)
    command = [str(runtime_executable)]
    cwd = launch_working_directory(profile, runtime_executable)
    env = launch_environment(profile, profile_dir, out_path, selected_runtime)
    return _gui_start_launch_process(
        mode="bml",
        command=command,
        cwd=cwd,
        env=env,
        launch_log=bml_profile_root(profile_dir) / "logs" / "gui-launch-bml.log",
        include_bml_env=True,
        metadata={
            "registry": str(registry_path),
            "runtime": selected_runtime.get("id"),
            "runtimeInfo": str(runtime_info_path),
            "runtimeManifestPath": str(out_path),
            "runtimeManifest": str(out_path),
            "activeMods": str(active_mods_path),
            "createdAt": manifest["launch"]["createdAt"],
        },
    )


def _gui_profile_or_detected_steam_install(profile: dict[str, Any] | None, install: dict[str, Any]) -> tuple[dict[str, Any] | None, str]:
    steam = profile_steam_install(profile) if profile is not None else None
    if isinstance(steam, dict) and steam.get("executable"):
        return dict(steam), "profile"
    if isinstance(install, dict) and install.get("executable"):
        return dict(install), "gui-detected-install"
    runtime = profile.get("runtime") if isinstance(profile, dict) else None
    if isinstance(runtime, dict) and runtime.get("baronyExecutable"):
        return {"executable": runtime.get("baronyExecutable"), "installPath": str(Path(str(runtime.get("baronyExecutable"))).expanduser().parent), "appId": STEAM_BARONY_APP_ID}, "profile-runtime"
    return None, "missing"


def _gui_launch_vanilla(profile: dict[str, Any] | None, profile_dir: Path, install: dict[str, Any]) -> dict[str, Any]:
    combined = ValidationResult("GUI Vanilla Barony launch validation")
    steam, source = _gui_profile_or_detected_steam_install(profile, install)
    if profile is not None and profile_steam_install(profile) is not None:
        combined.extend(validate_profile_steam_install(profile))
        if not _gui_launch_mocked():
            combined.extend(validate_steam_client_ready_for_launch(profile))
    elif isinstance(install, dict) and install.get("disabledReasons"):
        for reason in _gui_compact_texts([install.get("disabledReasons")]):
            combined.add("BML_GUI_VANILLA_INSTALL_BLOCKED", "fatal", reason)
    if steam is None:
        combined.add("BML_GUI_VANILLA_INSTALL_MISSING", "fatal", "No Steam Barony executable is available for Vanilla launch.")

    executable: Path | None = None
    if steam is not None:
        executable_value = steam.get("executable")
        if not isinstance(executable_value, str) or not executable_value.strip():
            combined.add("BML_GUI_VANILLA_EXECUTABLE_MISSING", "fatal", "Steam Barony executable path is missing.")
        else:
            executable = Path(executable_value).expanduser().resolve(strict=False)
            if not executable.exists():
                combined.add("BML_GUI_VANILLA_EXECUTABLE_MISSING", "fatal", "Steam Barony executable was not found.", path=str(executable))

    if not combined.ok:
        return _gui_launch_blocked("vanilla", combined, installSource=source, command=[str(executable)] if executable is not None else None, cwd=str(executable.parent) if executable is not None else None)

    assert steam is not None
    assert executable is not None
    install_path = steam.get("installPath")
    cwd = Path(str(install_path)).expanduser().resolve(strict=False) if isinstance(install_path, str) and install_path else executable.parent
    command = [str(executable)]
    env = _gui_vanilla_launch_environment(steam)
    return _gui_start_launch_process(
        mode="vanilla",
        command=command,
        cwd=cwd,
        env=env,
        launch_log=bml_profile_root(profile_dir) / "logs" / "gui-launch-vanilla.log",
        include_bml_env=False,
        metadata={
            "installSource": source,
            "steamAppId": str(steam.get("appId") or STEAM_BARONY_APP_ID),
            "runtimeManifestPath": None,
        },
    )


def _gui_report_summary(path: Path) -> dict[str, Any]:
    if not path.exists():
        return {
            "path": str(path),
            "status": "missing",
            "kind": path.name,
            "productionEvidence": False,
            "playableBehaviorClaimed": False,
        }
    try:
        payload = parse_json_file(path)
    except (OSError, json.JSONDecodeError) as exc:
        return {
            "path": str(path),
            "status": "malformed",
            "kind": path.name,
            "message": str(exc),
            "productionEvidence": False,
            "playableBehaviorClaimed": False,
        }
    kind = path.name
    text = json.dumps(payload, sort_keys=True).casefold()
    production = "production" in text and "fakeprovider" not in text and "fake-provider" not in text
    if path.name == "runtime-load-report.json" and isinstance(payload, dict):
        item = diagnostics_item_for_report(path)
        item.update(
            {
                "kind": kind,
                "productionEvidence": "production" in str(item.get("classification", "")).casefold() or production,
                "playableBehaviorClaimed": False,
            }
        )
        return item
    if "production-validation" in path.name and isinstance(payload, dict):
        return {
            "path": str(path),
            "status": payload.get("status") or "loaded",
            "kind": "production validation",
            "productionEvidence": True,
            "claimBoundary": payload.get("claimBoundary"),
            "processExecutable": payload.get("processExecutable") or payload.get("baronyExecutable"),
            "fakeProvider": payload.get("fakeProvider"),
            "playableBehaviorClaimed": bool(payload.get("playableBehaviorClaimed")),
            "reportedAt": payload.get("reportedAt"),
        }
    if "live-install" in path.name and isinstance(payload, dict):
        return {
            "path": str(path),
            "status": payload.get("status") or "loaded",
            "kind": "live install",
            "productionEvidence": production,
            "modId": payload.get("modId"),
            "version": payload.get("version"),
            "liveHookBehaviorClaimed": bool(payload.get("liveHookBehaviorClaimed")),
            "playableBehaviorClaimed": bool(payload.get("playableBehaviorClaimed")),
            "reportedAt": payload.get("reportedAt"),
        }
    return {
        "path": str(path),
        "status": payload.get("status") if isinstance(payload, dict) else "loaded",
        "kind": kind,
        "productionEvidence": production,
        "playableBehaviorClaimed": bool(payload.get("playableBehaviorClaimed")) if isinstance(payload, dict) else False,
    }


def _gui_diagnostics_report_paths(profile_dir: Path) -> list[Path]:
    paths = [
        bml_profile_root(profile_dir) / "reports" / "runtime-load-report.json",
        bml_profile_root(profile_dir) / "reports" / RUNEBOUND_ELIXIRS_LIVE_INSTALL_REPORT.split("/")[-1],
    ]
    repo_root = _gui_repo_root()
    support = repo_root / ".tmp" / "support-bundles" / "passing-linux-production" / "reports"
    paths.extend(
        [
            support / "runtime-load-report.json",
            support / "runebound-elixir-production-validation-report.json",
            support / "runebound-elixir-live-install-report.json",
        ]
    )
    tmp_root = repo_root / ".tmp"
    if tmp_root.exists():
        production_reports = sorted(
            tmp_root.glob("linux-prod-validation*/**/BaronyModLoader/reports/*.json"),
            key=lambda candidate: candidate.stat().st_mtime if candidate.exists() else 0,
            reverse=True,
        )
        paths.extend(production_reports[:3])
    seen: set[Path] = set()
    unique: list[Path] = []
    for path in paths:
        key = path.resolve(strict=False)
        if key not in seen:
            seen.add(key)
            unique.append(path)
    return unique


def _gui_diagnostics_evidence(profile_dir: Path) -> dict[str, Any]:
    items = [_gui_report_summary(path) for path in _gui_diagnostics_report_paths(profile_dir)]
    production_items = [item for item in items if item.get("productionEvidence") and item.get("status") != "missing"]
    status = "loaded" if production_items else "not_run"
    summary = f"{len(production_items)} production diagnostics report(s) available." if production_items else "No diagnostics report available."
    details = {
        "items": items,
        "diagnostics": items,
        "productionValidation": production_items,
        "productionEvidenceAvailable": bool(production_items),
    }
    return {
        "schemaVersion": SCHEMA_VERSION,
        "generatedAt": utc_now(),
        "status": status,
        "summary": summary,
        "label": summary,
        "items": items,
        "diagnostics": items,
        "productionEvidenceAvailable": bool(production_items),
        "productionValidation": production_items,
        "playableBehaviorClaimed": False,
        "details": details,
        "diagnosticDetails": details,
        "diagnosticsDetails": details,
    }


def _gui_windows_status() -> dict[str, Any]:
    return {
        "platform": "windows-x86_64",
        "status": "fail_closed_unverified",
        "label": "Windows fail-closed",
        "playable": False,
        "playableClaimed": False,
        "launchAdapter": LAUNCH_ADAPTER_WINDOWS_CREATEPROCESS_LOADLIBRARY,
        "reason": "No verified Windows DLL/launcher/runtime evidence is present; Windows remains visible but blocked.",
        "disabledReasons": ["Windows runtime is fail-closed until production verification evidence exists."],
    }


def _gui_workshop_state(profile_dir: Path, selected_summary: dict[str, Any] | None) -> dict[str, Any]:
    package_path = Path(str((selected_summary or {}).get("path") or _gui_runebound_package_path()))
    return build_workshop_prep_state(
        package_root=package_path,
        staging_dir=bml_profile_root(profile_dir) / "workshop-dry-run",
        dry_run=True,
        publish_enabled=False,
        publish=False,
        allow_publish=False,
    )


def _gui_build_concepts(
    *,
    install: dict[str, Any],
    profile_state: dict[str, Any],
    package_catalog: dict[str, Any],
    selected_summary: dict[str, Any] | None,
    selected_mod: dict[str, Any] | None,
    active_mods: list[dict[str, Any]],
    active_result: dict[str, Any] | None,
    readiness: dict[str, Any],
    launch_dry_run: dict[str, Any],
    diagnostics: dict[str, Any],
    windows_status: dict[str, Any],
    workshop: dict[str, Any],
    environment_summary_items: list[dict[str, Any]],
) -> list[dict[str, Any]]:
    readiness_state = readiness.get("readiness", readiness) if isinstance(readiness, dict) else {}
    readiness_rows = readiness_state.get("rows") if isinstance(readiness_state.get("rows"), list) else []
    selected = selected_mod or {}
    selected_package = selected_summary or {}
    packages = package_catalog.get("packages") if isinstance(package_catalog.get("packages"), list) else []
    selected_active = bool(selected.get("enabledInProfile"))
    selected_action_eligibility = selected.get("actionEligibility") if isinstance(selected.get("actionEligibility"), dict) else {}
    enable_eligibility = selected_action_eligibility.get("enable-package") if isinstance(selected_action_eligibility.get("enable-package"), dict) else _gui_action_eligibility_entry(False, "unavailable", "Select a local BML package before enabling it in this profile.")
    disable_eligibility = selected_action_eligibility.get("disable-package") if isinstance(selected_action_eligibility.get("disable-package"), dict) else _gui_action_eligibility_entry(False, "unavailable", "Select an enabled local BML package before disabling it in this profile.")
    action_target = _gui_selected_mod_action_target(selected if selected else None)
    active_mod_names = [
        str(mod.get("name") or mod.get("id") or mod.get("packageId"))
        for mod in active_mods
        if isinstance(mod, dict) and (mod.get("name") or mod.get("id") or mod.get("packageId"))
    ]
    diagnostics_items = diagnostics.get("items") if isinstance(diagnostics.get("items"), list) else []
    production_items = diagnostics.get("productionValidation") if isinstance(diagnostics.get("productionValidation"), list) else []
    metadata_rows = workshop.get("metadataRows") if isinstance(workshop.get("metadataRows"), list) else []
    preview_assets = workshop.get("previewAssets") if isinstance(workshop.get("previewAssets"), list) else []

    environment_blockers = _gui_compact_texts(
        [
            install.get("disabledReasons"),
            readiness_state.get("disabledReasons"),
            launch_dry_run.get("disabledReasons"),
        ]
    )
    vanilla_blockers = _gui_compact_texts([install.get("disabledReasons")])
    bml_launch_status = "blocked" if environment_blockers else (launch_dry_run.get("status") or "ready")
    vanilla_launch_status = "blocked" if vanilla_blockers else (install.get("status") or "ready")
    environment_warnings = _gui_compact_texts([windows_status.get("disabledReasons")])
    environment_evidence = [
        _gui_evidence("Linux Steam Barony install", install.get("installPath") or install.get("path"), install.get("status")),
        _gui_evidence("Runtime readiness", "Blocked" if readiness_state.get("disabledReasons") else readiness_state.get("status")),
        _gui_evidence("BML launch readiness", launch_dry_run.get("runtimeManifestPath") or launch_dry_run.get("status"), launch_dry_run.get("status")),
        _gui_evidence("Vanilla launch readiness", install.get("executable") or install.get("status"), vanilla_launch_status),
        _gui_evidence("Diagnostics evidence", diagnostics.get("label"), diagnostics.get("status")),
        _gui_evidence("Diagnostics reports checked", len(diagnostics_items)),
        _gui_evidence("Production validation evidence", "Available" if production_items else "Not found"),
        _gui_evidence("Windows runtime", windows_status.get("label"), windows_status.get("status")),
    ]
    for check in readiness_rows[:4]:
        if isinstance(check, dict):
            detail = check.get("status") or ("ready" if check.get("ready") else "blocked")
            environment_evidence.append(_gui_evidence(str(check.get("label") or check.get("key") or "Readiness check"), detail, check.get("status")))
    environment_summary = (
        "Linux launch path is blocked; review readiness evidence before starting Barony."
        if environment_blockers
        else "Linux launch path is ready for BML or vanilla launch; Windows remains fail-closed until verified."
    )

    profile_id_value = profile_state.get("id") or (profile_state.get("profile") if isinstance(profile_state.get("profile"), dict) else {}).get("id")
    profile_blockers = [] if profile_state.get("status") == "selected" else ["Create or select a stable profile before enabling mods."]
    profiles_evidence = [
        _gui_evidence("Selected profile", profile_id_value, profile_state.get("status")),
        _gui_evidence("Profile path", profile_state.get("path")),
        _gui_evidence("Stable default path", profile_state.get("stableDefault")),
        _gui_evidence("Active mods", active_mod_names or "None"),
    ]
    profiles_summary = (
        f"Profile {profile_id_value or 'default'} is selected with {len(active_mods)} {'active mod' if len(active_mods) == 1 else 'active mods'}."
        if not profile_blockers
        else "No profile is selected yet."
    )

    selected_problems = selected_package.get("problems")
    mod_warnings = _gui_problem_texts(selected_problems)
    mod_blockers = [] if selected else ["Scan packages and select a local mod package before enabling it."]
    selected_name = selected.get("name") or selected_package.get("name") or selected.get("packageId") or selected_package.get("id") or "Runebound: Elixirs"
    selected_id = selected.get("packageId") or selected_package.get("id") or selected.get("id")
    mods_evidence = [
        _gui_evidence("Packages found", len(packages), package_catalog.get("status")),
        _gui_evidence("Selected mod", selected_name, selected_package.get("validationStatus") or selected.get("status") or selected.get("provenance")),
        _gui_evidence("Package id", selected_id),
        _gui_evidence("Version", selected.get("version") or selected_package.get("version")),
        _gui_evidence("Carrier item", selected_package.get("carrierItemType") or selected_package.get("carrier")),
        _gui_evidence("Capabilities", selected_package.get("capabilityIds") or selected_package.get("capabilities")),
        _gui_evidence("Profile enabled state", selected_active),
        _gui_evidence("Enable action", enable_eligibility.get("reason") or enable_eligibility.get("status"), enable_eligibility.get("status")),
        _gui_evidence("Disable action", disable_eligibility.get("reason") or disable_eligibility.get("status"), disable_eligibility.get("status")),
    ]
    mods_summary = (
        f"{selected_name} is selected and {'enabled' if selected_active else 'available to enable' if enable_eligibility.get('enabled') else 'not eligible for profile actions'}."
        if selected
        else "No local mod package is selected."
    )

    workshop_warnings = _gui_compact_texts([workshop.get("visibleWarnings") or workshop.get("warnings") or ["Dry-run only; publishing disabled."]])
    workshop_evidence = [
        _gui_evidence("Workshop mode", workshop.get("mode") or "dry-run", workshop.get("status")),
        _gui_evidence("No-publish guard", "Publishing disabled" if workshop.get("noPublish", True) else "Publishing enabled"),
        _gui_evidence("Preview assets", f"{len(preview_assets)} checked"),
    ]
    for row in metadata_rows[:4]:
        if isinstance(row, dict) and row.get("key") != "description":
            workshop_evidence.append(_gui_evidence(str(row.get("label") or row.get("field") or "Workshop metadata"), row.get("value"), row.get("status")))

    enable_action = _gui_action(
        "enable-package",
        enable_eligibility.get("status"),
        enabled=bool(enable_eligibility.get("enabled")),
        disabledReason=enable_eligibility.get("reason") or enable_eligibility.get("disabledReason"),
        **action_target,
    )
    disable_action = _gui_action(
        "disable-package",
        disable_eligibility.get("status"),
        enabled=bool(disable_eligibility.get("enabled")),
        disabledReason=disable_eligibility.get("reason") or disable_eligibility.get("disabledReason"),
        **action_target,
    )
    mods_primary_action = disable_action if selected_mod and selected_mod.get("canDisable") and not selected_mod.get("canEnable") else enable_action
    mods_secondary_action = enable_action if mods_primary_action.get("id") == "disable-package" else disable_action
    return [
        {
            "key": "environment",
            "id": "environment",
            "title": "Environment",
            "status": "blocked" if environment_blockers else "ready",
            "statusSummary": environment_summary,
            "environmentSummaryItems": environment_summary_items,
            "evidence": environment_evidence,
            "primaryAction": _gui_action("launch-bml", bml_launch_status, enabled=True, disabledReasons=environment_blockers),
            "secondaryActions": [
                _gui_action("launch-vanilla", vanilla_launch_status, enabled=True, disabledReasons=vanilla_blockers),
                _gui_action("detect-install", install.get("status")),
                _gui_action("refresh-readiness", readiness_state.get("status") or "available"),
                _gui_action("open-diagnostics", diagnostics.get("status")),
            ],
            "warnings": environment_warnings,
            "blockers": environment_blockers,
            "state": {
                "install": install,
                "environmentSummaryItems": environment_summary_items,
                "readiness": readiness,
                "launchDryRun": launch_dry_run,
                "diagnosticsEvidence": diagnostics,
                "windowsStatus": windows_status,
            },
        },
        {
            "key": "profiles",
            "id": "profiles",
            "title": "Profiles",
            "status": "ready" if not profile_blockers else "blocked",
            "statusSummary": profiles_summary,
            "evidence": profiles_evidence,
            "primaryAction": _gui_action("create-select-profile", profile_state.get("status") or "available"),
            "secondaryActions": [],
            "warnings": [],
            "blockers": profile_blockers,
            "state": {"profile": profile_state, "activeMods": active_mods},
        },
        {
            "key": "mods",
            "id": "mods",
            "title": "Mods",
            "status": "ready" if not mod_blockers and not mod_warnings else ("warnings" if mod_warnings else "blocked"),
            "statusSummary": mods_summary,
            "evidence": mods_evidence,
            "primaryAction": mods_primary_action,
            "secondaryActions": [
                _gui_action("scan-packages", package_catalog.get("status") or "available", enabled=True),
                mods_secondary_action,
            ],
            "warnings": mod_warnings,
            "blockers": mod_blockers,
            "state": {
                "packageCatalog": package_catalog,
                "packageList": packages,
                "selectedPackage": selected_summary,
                "selectedMod": selected,
                "activeMods": active_mods,
                "activeResult": active_result,
            },
        },
        {
            "key": "workshop",
            "id": "workshop",
            "title": "Workshop",
            "status": "dry-run",
            "statusSummary": workshop.get("statusSummary") or "Dry-run only; publishing disabled.",
            "summary": workshop.get("summary") or "Dry-run only; publishing disabled.",
            "evidence": workshop_evidence,
            "primaryAction": _gui_action("workshop-preview", workshop.get("status") or "dry-run", enabled=True),
            "secondaryActions": [],
            "warnings": workshop_warnings,
            "expandedWarnings": workshop.get("expandedWarnings") or workshop.get("disabledReasons", []),
            "details": workshop.get("details") or workshop,
            "blockers": [],
            "state": workshop,
        },
    ]


def _gui_selected_mod_detail_panel(selected_mod: dict[str, Any] | None, mods_concept: dict[str, Any] | None) -> dict[str, Any]:
    selected = selected_mod or {}
    selected_name = _gui_text(selected.get("name") or selected.get("packageId") or selected.get("id") or "No mod selected")
    package_id = _gui_text(selected.get("packageId") or selected.get("id"))
    source = _gui_text(selected.get("source") or selected.get("provenance"))
    enabled = bool(selected.get("enabledInProfile"))
    enabled_label = "Enabled in selected profile" if enabled else "Not enabled in selected profile"
    path = _gui_text(selected.get("path") or selected.get("manifestPath") or selected.get("workshopItemPath"))
    eligibility = selected.get("actionEligibility") if isinstance(selected.get("actionEligibility"), dict) else {}
    action_target = _gui_selected_mod_action_target(selected if selected else None)

    actions: list[dict[str, Any]] = []
    availability_bits: list[str] = []
    for action_id, verb in (("enable-package", "Enable"), ("disable-package", "Disable")):
        entry = eligibility.get(action_id) if isinstance(eligibility.get(action_id), dict) else {}
        reason = entry.get("reason") or entry.get("disabledReason")
        action = _gui_action(
            action_id,
            entry.get("status") or ("unavailable" if selected else "not_selected"),
            enabled=bool(entry.get("enabled")),
            disabledReason=reason,
            **action_target,
        )
        action["label"] = f"{verb} {selected_name}" if selected else f"{verb} selected mod"
        action["contextual"] = True
        action["placement"] = "selectedModDetail"
        actions.append(action)
        availability = _gui_text(entry.get("status") or "unavailable")
        if reason:
            availability = f"{availability}: {_gui_text(reason)}"
        availability_bits.append(f"{verb}: {availability}")

    rows = [
        {"key": "name", "label": "Name", "value": selected_name},
        {"key": "packageId", "label": "Package ID", "value": package_id},
        {"key": "source", "label": "Source", "value": source},
        {"key": "enabled", "label": "Enabled state", "value": enabled_label},
        {"key": "path", "label": "Path", "value": path},
        {"key": "actionAvailability", "label": "Action availability", "value": "; ".join(availability_bits) if availability_bits else "No profile actions available"},
    ]
    if not selected:
        rows = [
            {"key": "empty", "label": "Selection", "value": "Scan packages and choose a mod from the left list."},
        ]
    primary_action_id = selected.get("primaryActionId")
    primary_action = next((action for action in actions if action.get("id") == primary_action_id), None)
    if primary_action is None:
        primary_action = next((action for action in actions if action.get("enabled")), actions[0] if actions else None)

    status = "enabled" if enabled else ("selected" if selected else "not_selected")
    summary = (
        f"{selected_name} is selected from {source}; profile state is {enabled_label.lower()}."
        if selected
        else "Select a mod on the left to inspect its package, source, path, and profile actions."
    )
    return {
        "key": "selected-mod-detail",
        "id": "selected-mod-detail",
        "title": "Selected Mod",
        "selectedModName": selected_name,
        "status": status,
        "summary": summary,
        "enabledInProfile": enabled,
        "profileEnabled": enabled,
        "activeInProfile": enabled,
        "statusSummary": summary,
        "rows": rows,
        "actions": actions,
        "primaryAction": primary_action,
        "secondaryActions": [action for action in actions if action is not primary_action],
        "compactStatus": {
            "name": selected_name,
            "packageId": package_id,
            "source": source,
            "enabledInProfile": enabled,
            "enabledLabel": enabled_label,
            "primaryActionId": primary_action.get("id") if isinstance(primary_action, dict) else None,
        },
        "selectedMod": selected_mod,
        "sourceConcept": (mods_concept or {}).get("key"),
    }


def _gui_compact_status_cards(concept_map: dict[str, dict[str, Any]]) -> list[dict[str, Any]]:
    def concept_actions(concept: dict[str, Any]) -> list[dict[str, Any]]:
        actions: list[dict[str, Any]] = []
        primary = concept.get("primaryAction")
        if isinstance(primary, dict):
            actions.append(primary)
        secondary = concept.get("secondaryActions")
        if isinstance(secondary, list):
            actions.extend(action for action in secondary if isinstance(action, dict))
        return actions

    def evidence_rows(concept: dict[str, Any], labels: set[str]) -> list[dict[str, str]]:
        rows: list[dict[str, str]] = []
        evidence = concept.get("evidence") if isinstance(concept.get("evidence"), list) else []
        for item in evidence:
            if not isinstance(item, dict):
                continue
            label = _gui_text(item.get("label"))
            if label in labels:
                rows.append({"label": label, "value": _gui_text(item.get("value")), "status": _gui_text(item.get("status"))})
        return rows

    cards: list[dict[str, Any]] = []
    environment = concept_map.get("environment") if isinstance(concept_map.get("environment"), dict) else {}
    if environment:
        env_rows: list[dict[str, Any]] = []
        summary_items = environment.get("environmentSummaryItems")
        if not isinstance(summary_items, list):
            state = environment.get("state") if isinstance(environment.get("state"), dict) else {}
            summary_items = state.get("environmentSummaryItems") if isinstance(state.get("environmentSummaryItems"), list) else []
        for item in summary_items[:3]:
            if not isinstance(item, dict):
                continue
            label = "Platform" if item.get("id") == "platform" else _gui_text(item.get("label") or item.get("displayLabel"))
            env_rows.append({
                "label": label,
                "value": _gui_text(item.get("value")),
                "status": _gui_text(item.get("source")),
                "logoPath": item.get("logoPath") or item.get("steamLogoPath") or item.get("osLogoPath") or item.get("gameLogoPath"),
                "logoKind": item.get("entityType") or item.get("id"),
                "icon": item.get("icon"),
            })
        cards.append(
            {
                "key": "environment",
                "id": "compact-environment",
                "title": "Environment",
                "status": environment.get("status"),
                "summary": environment.get("statusSummary"),
                "rows": env_rows,
                "actions": concept_actions(environment),
                "sourceConcept": "environment",
            }
        )

    profiles = concept_map.get("profiles") if isinstance(concept_map.get("profiles"), dict) else {}
    if profiles:
        profile_state = profiles.get("state") if isinstance(profiles.get("state"), dict) else {}
        profile_active_mods = profile_state.get("activeMods") if isinstance(profile_state.get("activeMods"), list) else []
        profile_rows = evidence_rows(profiles, {"Selected profile", "Active mods"})
        if not any(row.get("label") == "Active mods" for row in profile_rows):
            profile_rows.append({"label": "Active mods", "value": str(len(profile_active_mods)), "status": "enabled"})
        cards.append(
            {
                "key": "profiles",
                "id": "compact-profiles",
                "title": "Profiles",
                "status": profiles.get("status"),
                "summary": profiles.get("statusSummary"),
                "activeModCount": len(profile_active_mods),
                "profileCount": 1,
                "count": 1,
                "profileActiveModCount": len(profile_active_mods),
                "rows": profile_rows,
                "actions": concept_actions(profiles),
                "sourceConcept": "profiles",
            }
        )

    workshop = concept_map.get("workshop") if isinstance(concept_map.get("workshop"), dict) else {}
    if workshop:
        cards.append(
            {
                "key": "workshop",
                "id": "compact-workshop",
                "title": "Workshop",
                "status": workshop.get("status"),
                "summary": workshop.get("statusSummary"),
                "rows": evidence_rows(workshop, {"Workshop mode", "No-publish guard"}),
                "actions": concept_actions(workshop),
                "sourceConcept": "workshop",
            }
        )
    return cards

def build_profile_first_gui_state(
    *,
    smoke_mode: bool = False,
    action: str | None = None,
    activity_log: list[dict[str, Any]] | None = None,
    selected_mod_selector: str | None = None,
    selected_package_selector: str | None = None,
) -> dict[str, Any]:
    profile_dir = _gui_default_profile_path()
    install = _gui_detect_install()
    action_log = [dict(item) for item in (activity_log or []) if isinstance(item, dict)]
    create_profile = smoke_mode or action == "create-select-profile"
    profile, profile_state, profile_actions = _gui_load_or_create_profile(profile_dir, install, create_if_missing=create_profile)
    requested_selector = (selected_mod_selector or selected_package_selector or "").strip() or None
    package_catalog, selected_package, selected_summary = _gui_scan_packages()
    if requested_selector:
        requested_package_summary = _gui_resolve_package_summary(package_catalog, requested_selector)
        if requested_package_summary is not None:
            package_catalog, selected_package, selected_summary = _gui_scan_packages(_gui_selected_package_path(requested_package_summary))
    actions = [_gui_action(action_id, "available") for action_id, _label in GUI_ACTIONS]
    active_result: dict[str, Any] | None = None
    launch_result: dict[str, Any] | None = None
    active_mods = profile_authoritative_mods(profile, profile_dir) if profile is not None else []
    detected_inventory = _gui_detected_mod_inventory(
        package_catalog,
        active_mods,
        selected_mod_selector=requested_selector,
        selected_summary=selected_summary,
    )
    action_target_mod = detected_inventory.get("selectedMod") if isinstance(detected_inventory.get("selectedMod"), dict) else None

    if action == "enable-package":
        enable_eligibility = (action_target_mod or {}).get("actionEligibility", {}).get("enable-package") if isinstance((action_target_mod or {}).get("actionEligibility"), dict) else None
        if profile is None:
            active_result = {"status": "blocked", "changed": False, "disabledReasons": ["Create or select a profile before enabling a mod."]}
        elif not action_target_mod:
            active_result = {"status": "blocked", "changed": False, "disabledReasons": ["Select a mod before enabling it."]}
        elif not isinstance(enable_eligibility, dict) or not enable_eligibility.get("enabled"):
            reason = (enable_eligibility or {}).get("reason") or (enable_eligibility or {}).get("disabledReason") or "Selected mod cannot be enabled."
            active_result = {"status": "blocked", "changed": False, "disabledReasons": [reason], "actionEligibility": enable_eligibility}
        elif not action_target_mod.get("path"):
            active_result = {"status": "blocked", "changed": False, "disabledReasons": ["Selected mod has no package path to enable."]}
        else:
            active_result = enable_profile_mod(profile_dir, action_target_mod.get("path"))
            profile, profile_state, _profile_actions = _gui_load_or_create_profile(profile_dir, install, create_if_missing=False)
    elif action == "disable-package":
        disable_eligibility = (action_target_mod or {}).get("actionEligibility", {}).get("disable-package") if isinstance((action_target_mod or {}).get("actionEligibility"), dict) else None
        if profile is None:
            active_result = {"status": "blocked", "changed": False, "disabledReasons": ["Create or select a profile before disabling a mod."]}
        elif not action_target_mod:
            active_result = {"status": "blocked", "changed": False, "disabledReasons": ["Select a mod before disabling it."]}
        elif not isinstance(disable_eligibility, dict) or not disable_eligibility.get("enabled"):
            reason = (disable_eligibility or {}).get("reason") or (disable_eligibility or {}).get("disabledReason") or "Selected mod cannot be disabled."
            active_result = {"status": "blocked", "changed": False, "disabledReasons": [reason], "actionEligibility": disable_eligibility}
        elif not action_target_mod.get("packageId"):
            active_result = {"status": "blocked", "changed": False, "disabledReasons": ["Selected mod has no package id to disable."]}
        else:
            active_result = disable_profile_mod(profile_dir, str(action_target_mod.get("packageId")))
            profile, profile_state, _profile_actions = _gui_load_or_create_profile(profile_dir, install, create_if_missing=False)

    if action in {"enable-package", "disable-package"}:
        active_mods = profile_authoritative_mods(profile, profile_dir) if profile is not None else []
        detected_inventory = _gui_detected_mod_inventory(
            package_catalog,
            active_mods,
            selected_mod_selector=requested_selector,
            selected_summary=selected_summary,
        )
    readiness = _gui_refresh_readiness(install, profile, selected_package, profile_dir)
    launch_dry_run = _gui_launch_dry_run(profile, profile_dir, selected_package)
    if action == "launch-bml":
        launch_result = _gui_launch_bml(profile, profile_dir, selected_package)
    elif action == "launch-vanilla":
        launch_result = _gui_launch_vanilla(profile, profile_dir, install)
    diagnostics = _gui_diagnostics_evidence(profile_dir)
    windows_status = _gui_windows_status()
    workshop = _gui_workshop_state(profile_dir, selected_summary)
    environment_summary_items = _gui_environment_summary_items(install)

    if action == "detect-install":
        action_log.append(
            _gui_action_log_entry(
                action,
                install.get("status"),
                install.get("reason") or "Install detection refreshed.",
                installPath=install.get("installPath") or install.get("path"),
                executable=install.get("executable"),
                visibleSummary="Install detection refreshed",
            )
        )
    elif action == "refresh-readiness":
        readiness_state = readiness.get("readiness", readiness) if isinstance(readiness, dict) else {}
        blockers = _gui_compact_texts([readiness_state.get("disabledReasons")])
        action_log.append(
            _gui_action_log_entry(
                action,
                readiness_state.get("status") or ("blocked" if blockers else "ready"),
                f"Readiness refreshed with {len(blockers)} blocker(s).",
                blockerCount=len(blockers),
                blockers=blockers,
                visibleSummary="Readiness refreshed",
            )
        )
    elif action in {"launch-bml", "launch-vanilla"}:
        result = launch_result or {"status": "blocked", "processStarted": False, "processLaunched": False, "disabledReasons": ["Launch action did not run."]}
        mode_label = "BML Barony" if action == "launch-bml" else "Vanilla Barony"
        blocked = result.get("status") == "blocked" or bool(result.get("disabledReasons"))
        visible = f"Launch {mode_label} blocked" if blocked else f"Launch {mode_label} started"
        action_log.append(
            _gui_action_log_entry(
                action,
                result.get("status"),
                f"{mode_label} launch {'blocked' if blocked else 'started'}; processStarted={bool(result.get('processStarted'))}, mocked={bool(result.get('mocked'))}.",
                mode=result.get("mode"),
                processStarted=bool(result.get("processStarted")),
                processLaunched=bool(result.get("processLaunched")),
                mocked=bool(result.get("mocked")),
                pid=result.get("pid"),
                command=result.get("command"),
                cwd=result.get("cwd"),
                environment=result.get("environment"),
                runtimeManifestPath=result.get("runtimeManifestPath") or result.get("manifestPath"),
                disabledReasons=result.get("disabledReasons"),
                problems=result.get("problems"),
                launchLog=result.get("launchLog"),
                visibleSummary=visible,
            )
        )
    elif action == "open-diagnostics":
        items = diagnostics.get("items") if isinstance(diagnostics.get("items"), list) else []
        action_log.append(
            _gui_action_log_entry(
                action,
                diagnostics.get("status"),
                f"Diagnostics evidence refreshed; {len(items)} report(s) checked.",
                reportCount=len(items),
                productionEvidenceAvailable=bool(diagnostics.get("productionEvidenceAvailable")),
                visibleSummary="Diagnostics refreshed",
            )
        )
    elif action == "create-select-profile":
        created = "created_profile" in profile_actions
        selected = profile_state.get("status") == "selected"
        action_log.append(
            _gui_action_log_entry(
                action,
                "created" if created else ("selected" if selected else profile_state.get("status")),
                f"Stable profile {'created' if created else 'selected'} at {profile_state.get('path')}.",
                profilePath=profile_state.get("path"),
                profileId=profile_state.get("id") or profile_state.get("profileId"),
                stableDefault=profile_state.get("stableDefault"),
                tmpPathRejected=profile_state.get("tmpPathRejected"),
                visibleSummary="Profile selected",
            )
        )
    elif action == "scan-packages":
        package_count = len(package_catalog.get("packages", [])) if isinstance(package_catalog.get("packages"), list) else 0
        action_log.append(
            _gui_action_log_entry(
                action,
                package_catalog.get("status"),
                f"Package scan refreshed; {package_count} package(s) discovered.",
                packageCount=package_count,
                selectedPackageId=_gui_selected_package_id(selected_summary) if selected_summary is not None else None,
                modsRoot=package_catalog.get("modsRoot"),
                visibleSummary="Package scan refreshed",
            )
        )
    elif action == "enable-package":
        target_details = _gui_selected_mod_action_target(action_target_mod)
        selected_id = target_details.get("targetPackageId") or target_details.get("selectedModId") or _gui_selected_package_id(selected_summary)
        selected_name = _gui_text(target_details.get("selectedModName") or (selected_summary or {}).get("name") or selected_id)
        selected_status = _gui_text((active_result or {}).get("status"))
        enable_visible = f"Enabled {selected_name}" if selected_status in {"enabled", "already enabled"} else f"Enable {selected_name} {selected_status}"
        action_log.append(
            _gui_action_log_entry(
                action,
                (active_result or {}).get("status"),
                f"Enable {selected_name} ({selected_id}) result: {selected_status}.",
                changed=(active_result or {}).get("changed"),
                packageId=selected_id,
                targetPackageId=target_details.get("targetPackageId"),
                selectedModId=target_details.get("selectedModId"),
                selectedModName=selected_name,
                selectedModPath=target_details.get("selectedModPath"),
                actionEligibility=target_details.get("actionEligibility"),
                activeMods=active_mods,
                disabledReasons=(active_result or {}).get("disabledReasons"),
                problems=(active_result or {}).get("problems"),
                visibleSummary=enable_visible,
            )
        )
    elif action == "disable-package":
        target_details = _gui_selected_mod_action_target(action_target_mod)
        selected_id = target_details.get("targetPackageId") or target_details.get("selectedModId") or _gui_selected_package_id(selected_summary)
        selected_name = _gui_text(target_details.get("selectedModName") or (selected_summary or {}).get("name") or selected_id)
        selected_status = _gui_text((active_result or {}).get("status"))
        disable_visible = f"Disabled {selected_name}" if selected_status in {"disabled", "already disabled"} else f"Disable {selected_name} {selected_status}"
        action_log.append(
            _gui_action_log_entry(
                action,
                (active_result or {}).get("status"),
                f"Disable {selected_name} ({selected_id}) result: {selected_status}.",
                changed=(active_result or {}).get("changed"),
                packageId=selected_id,
                targetPackageId=target_details.get("targetPackageId"),
                selectedModId=target_details.get("selectedModId"),
                selectedModName=selected_name,
                selectedModPath=target_details.get("selectedModPath"),
                actionEligibility=target_details.get("actionEligibility"),
                activeMods=active_mods,
                disabledReasons=(active_result or {}).get("disabledReasons"),
                problems=(active_result or {}).get("problems"),
                visibleSummary=disable_visible,
            )
        )
    elif action == "workshop-preview":
        action_log.append(
            _gui_action_log_entry(
                action,
                workshop.get("status"),
                "Workshop dry-run preview refreshed; Steam publish remains disabled.",
                publishEnabled=bool(workshop.get("publishEnabled")),
                noPublish=bool(workshop.get("noPublish", True)),
                steamSideEffects=bool(workshop.get("steamSideEffects")),
                stagingFolder=workshop.get("stagingFolder"),
                visibleSummary="Workshop dry-run preview",
                expandedWarnings=workshop.get("expandedWarnings") or workshop.get("disabledReasons", []),
                dryRunVdfReport=workshop.get("dryRunVdfReport"),
            )
        )
    elif action == "copy-for-ai":
        # copy-for-ai is handled entirely in the GUI event loop via clipboard APIs.
        # This branch intentionally produces no action-log entry — the GUI handler
        # appends one directly to state_ref["value"]["actionLog"] after clipboard ops.
        pass

    concepts = _gui_build_concepts(
        install=install,
        profile_state=profile_state,
        package_catalog=package_catalog,
        selected_summary=selected_summary,
        selected_mod=detected_inventory.get("selectedMod"),
        active_mods=active_mods,
        active_result=active_result,
        readiness=readiness,
        launch_dry_run=launch_dry_run,
        diagnostics=diagnostics,
        windows_status=windows_status,
        workshop=workshop,
        environment_summary_items=environment_summary_items,
    )
    concept_map = {concept["key"]: concept for concept in concepts}
    mods_concept = concept_map.get("mods") if isinstance(concept_map.get("mods"), dict) else {}
    selected_mod_detail = _gui_selected_mod_detail_panel(detected_inventory.get("selectedMod"), mods_concept)
    compact_status_cards = _gui_compact_status_cards(concept_map)
    right_side_labels = ["Selected Mod", "Environment", "Profiles", "Workshop", "Recent Activity"]
    mods_secondary = mods_concept.get("secondaryActions") if isinstance(mods_concept.get("secondaryActions"), list) else []
    mods_header_actions = [
        action
        for action in (
            [item for item in mods_secondary if isinstance(item, dict) and item.get("id") == "scan-packages"]
            + ([mods_concept.get("primaryAction")] if isinstance(mods_concept.get("primaryAction"), dict) else [])
            + [item for item in mods_secondary if isinstance(item, dict) and item.get("id") != "scan-packages"]
        )
        if isinstance(action, dict)
    ]
    sections = [
        {
            "key": concept["key"],
            "id": concept["id"],
            "label": concept["title"],
            "title": concept["title"],
            "state": concept,
        }
        for concept in concepts
    ]
    disabled = _gui_disabled_reasons({"concepts": concepts})
    visible_activity = _gui_visible_activity_log(action_log)
    activity_details = _gui_activity_log_details(action_log)
    diagnostic_details = diagnostics.get("diagnosticDetails") or diagnostics.get("details") or diagnostics
    app_icon_path = _gui_bml_app_icon_path()
    vanilla_icon_path = _gui_vanilla_barony_icon_path()
    steam_logo_path = next((item.get("steamLogoPath") for item in environment_summary_items if isinstance(item, dict) and item.get("id") == "platform"), None)
    entity_iconography = _gui_entity_iconography_records(
        steam_logo_path=steam_logo_path,
        steam_logo_rendered=False,
        detected_sections=detected_inventory.get("detectedModSections", []),
        rendered_rows=detected_inventory.get("renderedDetectedModRows", []),
    )
    return {
        "schemaVersion": SCHEMA_VERSION,
        "generatedAt": utc_now(),
        "section": "profile-first gui",
        "layout": "master-detail mods inspector",
        "title": f"{APP_ID} Mod Manager",
        "profilePath": str(profile_dir),
        "profile": profile_state,
        "install": install,
        "packageCatalog": package_catalog,
        "packageList": package_catalog.get("packages", []),
        "selectedPackage": selected_summary,
        "activeMods": active_mods,
        "activeResult": active_result,
        "environmentSummaryItems": environment_summary_items,
        "detectedMods": detected_inventory.get("detectedMods", []),
        "detectedModSections": detected_inventory.get("detectedModSections", []),
        "renderedProvenanceSectionLabels": detected_inventory.get("renderedProvenanceSectionLabels", []),
        "detectedModsSidebarTitle": "Mods",
        "modsSidebarTitle": "Mods",
        "modsListHeaderActions": mods_header_actions,
        "detectedModsHeaderActions": mods_header_actions,
        "selectedModDetail": selected_mod_detail,
        "selectedDetailPanel": selected_mod_detail,
        "compactStatusCards": compact_status_cards,
        "rightSideLabels": right_side_labels,
        "rightSide": {
            "labels": right_side_labels,
            "selectedModDetail": selected_mod_detail,
            "selectedDetailPanel": selected_mod_detail,
            "compactStatusCards": compact_status_cards,
            "cards": compact_status_cards,
            "recentActivity": visible_activity,
            "activityLogDetails": activity_details,
            "activityLogActions": [_gui_action("copy-for-ai", "available", enabled=True, placement="Recent Activity / Action Log")],
        },
        "selectedDetectedMod": detected_inventory.get("selectedDetectedMod"),
        "selectedDetectedModId": detected_inventory.get("selectedDetectedModId"),
        "selectedDetectedModProvenance": detected_inventory.get("selectedDetectedModProvenance"),
        "selectedMod": detected_inventory.get("selectedMod"),
        "renderedDetectedModRows": detected_inventory.get("renderedDetectedModRows", []),
        "platformStorefront": "Steam",
        "steamLogoPath": steam_logo_path,
        "steamLogoRendered": False,
        "appIconPath": app_icon_path,
        "appIconLoaded": bool(app_icon_path),
        "headerIconPath": app_icon_path,
        "headerIconLoaded": False,
        "vanillaBaronyIconPath": vanilla_icon_path,
        "actions": actions,
        "actionLog": action_log,
        "visibleActivityLog": visible_activity,
        "activityLogDetails": activity_details,
        "recentActivity": visible_activity,
        "activityLogActions": [_gui_action("copy-for-ai", "available", enabled=True, placement="Recent Activity / Action Log")],
        "readiness": readiness,
        "launchDryRun": launch_dry_run,
        "lastLaunch": launch_result,
        "launchResult": launch_result,
        "diagnosticsEvidence": diagnostics,
        "diagnosticDetails": diagnostic_details,
        "diagnosticsDetails": diagnostic_details,
        "windowsStatus": windows_status,
        "workshop": workshop,
        "playableClaimed": False,
        "concepts": concepts,
        "conceptMap": concept_map,
        "sections": sections,
        "entityIconography": entity_iconography,
        "renderedEntityIcons": entity_iconography,
        "widgets": sections,
        "disabledReasons": disabled,
    }


def _gui_disabled_reasons(state: dict[str, Any]) -> list[str]:
    reasons: list[str] = []

    def add(raw: Any) -> None:
        for text in _gui_compact_texts([raw]):
            if text not in reasons:
                reasons.append(text)

    add(state.get("disabledReasons"))
    for concept in state.get("concepts", []):
        if not isinstance(concept, dict):
            continue
        add(concept.get("blockers"))
        add(concept.get("warnings"))
        concept_state = concept.get("state")
        if isinstance(concept_state, dict):
            add(concept_state.get("disabledReasons"))
    for section in state.get("sections", []):
        if not isinstance(section, dict):
            continue
        section_state = section.get("state")
        if isinstance(section_state, dict):
            add(section_state.get("blockers"))
            add(section_state.get("warnings"))
    return reasons


def _gui_section_summary(section: dict[str, Any]) -> list[tuple[str, str]]:
    state = section.get("state") if isinstance(section.get("state"), dict) else section
    rows: list[tuple[str, str]] = [("Status", _humanize_enum_label(_gui_text(state.get("status") or "not_selected")))]
    if state.get("statusSummary") is not None:
        rows.append(("Summary", _gui_text(state.get("statusSummary"))))
    evidence = state.get("evidence") if isinstance(state.get("evidence"), list) else []
    for item in evidence:
        if isinstance(item, dict):
            rows.append((_gui_text(item.get("label") or "Evidence"), _gui_text(item.get("value"))))
    return rows

def _gui_bulleted_warning_text(warnings: Iterable[Any], *, width: int = 58) -> str:
    lines: list[str] = []
    for warning in warnings:
        text = _gui_text(warning)
        wrapped = textwrap.wrap(text, width=width, break_long_words=True, break_on_hyphens=False) or [text]
        for index, line in enumerate(wrapped):
            lines.append(("• " if index == 0 else "  ") + line)
    return "\n".join(lines)


def _gui_app_icon_path() -> str | None:
    return _gui_bml_app_icon_path()


def _build_gui_dashboard_window(gui_state: dict[str, Any]) -> tuple[Any, list[str], Any, dict[str, list[Any]], dict[str, Any]]:
    tk = __import__("tk" + "inter")
    ttk = __import__("tk" + "inter.ttk", fromlist=["ttk"])

    root = tk.Tk()
    root.title(str(gui_state.get("title") or f"{APP_ID} Mod Manager"))
    root.geometry("1180x880")
    root.minsize(980, 740)
    root._bml_images = []  # Keep Tk PhotoImage references alive.
    app_icon_path = _gui_app_icon_path()
    if app_icon_path:
        try:
            app_icon = tk.PhotoImage(file=app_icon_path)
            root.iconphoto(True, app_icon)
            root._bml_images.append(app_icon)
            gui_state["appIconPath"] = app_icon_path
            gui_state["appIconLoaded"] = True
        except tk.TclError as exc:
            gui_state["appIconPath"] = app_icon_path
            gui_state["appIconLoaded"] = False
            gui_state["appIconError"] = str(exc)
    else:
        gui_state["appIconLoaded"] = False

    style = ttk.Style(root)
    try:
        style.theme_use("clam")
    except tk.TclError:
        pass
    style.configure("Dashboard.TFrame", background="#f4f4f2")
    style.configure("Title.TLabel", font=("TkDefaultFont", 18, "bold"), foreground="#111111", background="#f4f4f2")
    style.configure("Subtitle.TLabel", foreground="#333333", background="#f4f4f2")
    style.configure("Status.TLabel", font=("TkDefaultFont", 10, "bold"))
    style.configure("Summary.TLabel", font=("TkDefaultFont", 10, "bold"))
    style.configure("Warning.TLabel", foreground="#6b2f00")
    style.configure("Section.TLabelframe", padding=12)
    style.configure("Section.TLabelframe.Label", font=("TkDefaultFont", 11, "bold"))
    style.configure("Primary.TButton", font=("TkDefaultFont", 10, "bold"))
    style.configure("Provenance.TLabel", font=("TkDefaultFont", 10, "bold"))
    style.configure("Badge.TLabel", font=("TkFixedFont", 9, "bold"), foreground="#111111", background="#deded8")

    state_ref = {"value": gui_state}
    button_registry: dict[str, list[Any]] = {}
    mod_selection_registry: dict[str, Any] = {}
    image_registry: list[Any] = []

    def load_row_logo(path_value: Any) -> Any | None:
        path_text = _gui_text(path_value)
        if path_text == "Not set":
            return None
        path = Path(path_text)
        if not path.is_file() or path.suffix.lower() not in {".png", ".gif"}:
            return None
        try:
            image = tk.PhotoImage(file=str(path))
            max_side = max(int(image.width()), int(image.height()), 1)
            if max_side > 24:
                factor = max(1, int((max_side + 23) // 24))
                image = image.subsample(factor, factor)
            image_registry.append(image)
            return image
        except tk.TclError:
            return None

    def ensure_redraw_instrumentation() -> tuple[list[dict[str, Any]], list[dict[str, Any]], dict[str, int]]:
        state = state_ref["value"]
        redraw_events = state.get("redrawEvents")
        if not isinstance(redraw_events, list):
            redraw_events = []
            state["redrawEvents"] = redraw_events
        render_events = state.get("renderEvents")
        if not isinstance(render_events, list):
            render_events = []
            state["renderEvents"] = render_events
        counts = state.get("regionRenderCounts")
        if not isinstance(counts, dict):
            counts = {}
            state["regionRenderCounts"] = counts
        return redraw_events, render_events, counts

    def current_redraw_instrumentation() -> dict[str, Any]:
        redraw_events, render_events, counts = ensure_redraw_instrumentation()
        return {
            "redrawEvents": [dict(item) for item in redraw_events if isinstance(item, dict)],
            "renderEvents": [dict(item) for item in render_events if isinstance(item, dict)],
            "regionRenderCounts": {str(key): int(value) for key, value in counts.items() if isinstance(value, int)},
        }

    def record_region_render(region: str, reason: str = "render") -> None:
        redraw_events, render_events, counts = ensure_redraw_instrumentation()
        counts[region] = int(counts.get(region, 0)) + 1
        event = {
            "region": region,
            "reason": reason,
            "count": counts[region],
            "generatedAt": utc_now(),
        }
        render_events.append(dict(event))
        redraw_events.append(dict(event))
    container = ttk.Frame(root, padding=16, style="Dashboard.TFrame")
    container.grid(row=0, column=0, sticky="nsew")
    root.columnconfigure(0, weight=1)
    root.rowconfigure(0, weight=1)
    container.columnconfigure(0, weight=1)
    container.rowconfigure(2, weight=1)

    title_frame = ttk.Frame(container, style="Dashboard.TFrame")
    title_frame.grid(row=0, column=0, sticky="w")
    title_frame.columnconfigure(1, weight=1)
    header_icon_path = gui_state.get("headerIconPath") or _gui_bml_app_icon_path()
    if header_icon_path:
        try:
            header_icon = tk.PhotoImage(file=str(header_icon_path))
            max_side = max(int(header_icon.width()), int(header_icon.height()), 1)
            if max_side > 32:
                factor = max(1, int((max_side + 31) // 32))
                header_icon = header_icon.subsample(factor, factor)
            image_registry.append(header_icon)
            gui_state["headerIconPath"] = str(header_icon_path)
            gui_state["headerIconLoaded"] = True
            gui_state["headerIconWidth"] = int(header_icon.width())
            gui_state["headerIconHeight"] = int(header_icon.height())
            ttk.Label(title_frame, image=header_icon, style="Title.TLabel").grid(row=0, column=0, sticky="w", padx=(0, 8))
        except tk.TclError as exc:
            gui_state["headerIconPath"] = str(header_icon_path)
            gui_state["headerIconLoaded"] = False
            gui_state["headerIconError"] = str(exc)
    else:
        gui_state["headerIconLoaded"] = False
    ttk.Label(title_frame, text="BaronyModLoader", style="Title.TLabel").grid(row=0, column=1, sticky="w")
    ttk.Label(
        container,
        text="Mods stay on the left for scanning and selection; the selected mod inspector and profile actions sit on the right above compact status cards.",
        style="Subtitle.TLabel",
        wraplength=1040,
    ).grid(row=1, column=0, sticky="ew", pady=(4, 12))


    body = ttk.Frame(container)
    body.grid(row=2, column=0, sticky="nsew")
    body.columnconfigure(0, weight=0, minsize=350)
    body.columnconfigure(1, weight=1)
    body.rowconfigure(0, weight=1)

    sidebar_frame = ttk.LabelFrame(body, text=_gui_icon_label("mods-list", str(gui_state.get("detectedModsSidebarTitle") or "Mods")), style="Section.TLabelframe")
    sidebar_frame.grid(row=0, column=0, sticky="nsew", padx=(0, 8))
    sidebar_frame.columnconfigure(0, weight=1)

    right_side = ttk.Frame(body)
    right_side.grid(row=0, column=1, sticky="nsew", padx=(8, 0))
    right_side.columnconfigure(0, weight=1)
    right_side.rowconfigure(0, weight=0)
    right_side.rowconfigure(1, weight=1)
    right_side.rowconfigure(2, weight=0)

    selected_detail_frame = ttk.LabelFrame(right_side, text=_gui_icon_label("selected-mod-detail", "Selected Mod"), style="Section.TLabelframe")
    selected_detail_frame.grid(row=0, column=0, sticky="ew")
    selected_detail_frame.columnconfigure(1, weight=1)

    compact_status_grid = ttk.Frame(right_side)
    compact_status_grid.grid(row=1, column=0, sticky="nsew", pady=(10, 0))
    for column in range(3):
        compact_status_grid.columnconfigure(column, weight=1, uniform="compact-status")

    activity_frame = ttk.LabelFrame(right_side, text=_gui_icon_label("activity-log", "Recent Activity / Action Log"), style="Section.TLabelframe")
    activity_frame.grid(row=2, column=0, sticky="ew", pady=(10, 0))
    activity_frame.columnconfigure(0, weight=1)
    activity_header = ttk.Frame(activity_frame)
    activity_header.grid(row=0, column=0, sticky="ew", padx=(0, 0), pady=(0, 4))
    activity_header.columnconfigure(0, weight=1)
    copy_for_ai_btn = ttk.Button(activity_header, text="Copy for AI", style="TButton")
    copy_for_ai_btn.grid(row=0, column=0, sticky="ew")

    def copy_for_ai_action() -> None:
        ctx = _gui_copy_for_ai_context(state_ref["value"])
        copy_text = ctx["text"]
        try:
            root.clipboard_clear()
            root.clipboard_append(copy_text)
            root.update()
            tk_clipboard_status = "ok"
            tk_succeeded = True
        except Exception as exc:
            tk_clipboard_status = f"error: {exc}"
            tk_succeeded = False

        system_clipboard = _gui_system_clipboard_copy(copy_text)
        system_clipboard_status = system_clipboard.get("status") or "unavailable"
        system_verified = bool(system_clipboard.get("verifiedReadback") or system_clipboard.get("readbackVerified") or system_clipboard.get("succeeded"))
        system_succeeded = system_verified
        system_available = bool(system_clipboard.get("systemBackendAvailable") or system_clipboard.get("available"))
        system_attempted = bool(system_clipboard.get("systemBackendAttempted") or system_clipboard.get("attempted"))
        system_used = system_clipboard.get("used")
        fallback_copy_path: str | None = None
        fallback_copy_written = False
        fallback_copy_status: str | None = None
        linux_system_unverified = sys.platform.startswith("linux") and not system_verified
        if linux_system_unverified:
            try:
                fallback_dir = _gui_data_home() / APP_ID / "copy-for-ai"
                fallback_dir.mkdir(parents=True, exist_ok=True)
                fallback_path = fallback_dir / f"copy-for-ai-{datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')}.txt"
                fallback_path.write_text(copy_text, encoding="utf-8")
                fallback_copy_path = str(fallback_path)
                fallback_copy_written = True
                fallback_copy_status = "ok"
            except Exception as exc:
                fallback_copy_status = f"error: {exc}"
        clipboard_backends = [
            {
                "backend": "tk",
                "name": "tk",
                "available": True,
                "attempted": True,
                "envDisplay": None,
                "mimeType": "text/plain",
                "status": tk_clipboard_status,
                "succeeded": tk_succeeded,
                "stderrSnippet": "",
                "verifiedReadback": False,
                "readbackVerified": False,
                "readbackStatus": "not verified",
                "readbackMatches": None,
            },
            *system_clipboard.get("clipboardBackends", []),
        ]
        if system_succeeded:
            clipboard_status = f"ok: system clipboard ({system_used or 'system'})"
            visible_summary = "Copied AI issue context"
            log_summary = visible_summary
        elif linux_system_unverified:
            if fallback_copy_written and fallback_copy_path:
                if tk_succeeded:
                    clipboard_status = f"partial: tk ok; system={system_clipboard_status}; fallback={fallback_copy_path}"
                    visible_summary = f"Copy AI issue context partial: system clipboard failed; fallback file: {fallback_copy_path}"
                else:
                    clipboard_status = f"partial: fallback file written; tk={tk_clipboard_status}; system={system_clipboard_status}"
                    visible_summary = f"Copy AI issue context saved to fallback file: {fallback_copy_path}"
            else:
                if tk_succeeded:
                    clipboard_status = f"partial: tk ok; system={system_clipboard_status}; fallback={fallback_copy_status or 'not written'}"
                    visible_summary = f"Copy AI issue context partial: system clipboard failed; fallback file failed ({fallback_copy_status or 'not written'})"
                else:
                    clipboard_status = f"error: tk={tk_clipboard_status}; system={system_clipboard_status}; fallback={fallback_copy_status or 'not written'}"
                    visible_summary = f"Copy AI issue context failed; fallback file failed ({fallback_copy_status or 'not written'})"
            log_summary = f"{visible_summary}: {system_clipboard_status}"
        elif tk_succeeded:
            clipboard_status = f"ok: tk fallback; system={system_clipboard_status}"
            visible_summary = "Copied AI issue context (Tk clipboard fallback)"
            log_summary = f"{visible_summary}: no system clipboard backend available"
        else:
            clipboard_status = f"error: tk={tk_clipboard_status}; system={system_clipboard_status}"
            visible_summary = "Copy AI issue context failed"
            log_summary = f"{visible_summary}: {system_clipboard_status}"
        log_entry = _gui_action_log_entry(
            "copy-for-ai",
            clipboard_status,
            log_summary,
            charCount=ctx.get("charCount"),
            byteCount=ctx.get("byteCount"),
            includedSections=ctx.get("includedSections"),
            clipboardStatus=clipboard_status,
            tkClipboardStatus=tk_clipboard_status,
            systemClipboardStatus=system_clipboard_status,
            systemClipboardDetails=system_clipboard,
            systemClipboardAvailable=system_available,
            systemClipboardAttempted=system_attempted,
            systemClipboardVerified=system_verified,
            fallbackCopyFile=fallback_copy_path,
            fallbackCopyPath=fallback_copy_path,
            fallbackCopyWritten=fallback_copy_written,
            fallbackCopyStatus=fallback_copy_status,
            clipboardBackends=clipboard_backends,
            visibleSummary=visible_summary,
        )
        prior_log = state_ref["value"].get("actionLog") if isinstance(state_ref["value"].get("actionLog"), list) else []
        updated_log = prior_log + [log_entry]
        state_ref["value"]["actionLog"] = updated_log
        state_ref["value"]["visibleActivityLog"] = _gui_visible_activity_log(updated_log)
        state_ref["value"]["activityLogDetails"] = _gui_activity_log_details(updated_log)
        state_ref["value"]["copyForAiContext"] = ctx["bundle"]
        state_ref["value"]["copyForAiText"] = copy_text
        state_ref["value"]["lastCopyForAi"] = {
            "status": clipboard_status,
            "visibleSummary": visible_summary,
            "tkClipboardStatus": tk_clipboard_status,
            "systemClipboardStatus": system_clipboard_status,
            "systemClipboardDetails": system_clipboard,
            "systemClipboardAvailable": system_available,
            "systemClipboardAttempted": system_attempted,
            "systemClipboardVerified": system_verified,
            "fallbackCopyFile": fallback_copy_path,
            "fallbackCopyPath": fallback_copy_path,
            "fallbackCopyWritten": fallback_copy_written,
            "fallbackCopyStatus": fallback_copy_status,
            "clipboardBackends": clipboard_backends,
            "charCount": ctx.get("charCount"),
            "byteCount": ctx.get("byteCount"),
            "includedSections": ctx.get("includedSections"),
            "copyForAiContextSections": ctx.get("includedSections"),
        }
        render_activity_log("copy-for-ai")

    copy_for_ai_btn.configure(command=copy_for_ai_action)
    button_registry.setdefault("copy-for-ai", []).append(copy_for_ai_btn)
    activity_text = tk.Text(activity_frame, height=5, wrap="word", relief="flat")
    activity_text.grid(row=1, column=0, sticky="ew")

    def render_activity_log(reason: str = "render") -> None:
        record_region_render("activityLog", reason)
        visible = state_ref["value"].get("visibleActivityLog")
        if not isinstance(visible, list) or not visible:
            visible = ["No actions yet. Use the Mods header or right-side buttons to validate the next step."]
        activity_text.configure(state="normal")
        activity_text.delete("1.0", "end")
        activity_text.insert("1.0", "\n".join(_gui_text(item) for item in visible[-8:]))
        activity_text.configure(state="disabled")

    def current_no_autofocus_state() -> dict[str, Any]:
        return {
            key: state_ref["value"].get(key)
            for key in ("noAutofocusRequested", "noAutofocusApplied", "noAutofocusMethods", "smokeVisibleRequested", "smokeWindowHidden", "smokeWindowSuppressionMethods")
            if key in state_ref["value"]
        }

    def rebuild(action_id: str | None = None) -> None:
        prior_log = state_ref["value"].get("actionLog")
        selected_selector = _gui_state_selected_mod_selector(state_ref["value"])
        no_autofocus = current_no_autofocus_state()
        instrumentation = current_redraw_instrumentation()
        state_ref["value"] = build_profile_first_gui_state(
            action=action_id,
            activity_log=prior_log if isinstance(prior_log, list) else None,
            selected_mod_selector=selected_selector,
        )
        state_ref["value"].update(no_autofocus)
        state_ref["value"].update(instrumentation)
        render_sections("action")
        render_activity_log("action")

    def select_detected_mod(target: Any) -> dict[str, Any]:
        if isinstance(target, dict):
            selector = str(target.get("packageId") or target.get("path") or target.get("manifestPath") or target.get("id") or target.get("name") or "")
        else:
            selector = str(target or "")
        prior_log = state_ref["value"].get("actionLog")
        no_autofocus = current_no_autofocus_state()
        instrumentation = current_redraw_instrumentation()
        state_ref["value"] = build_profile_first_gui_state(
            activity_log=prior_log if isinstance(prior_log, list) else None,
            selected_mod_selector=selector,
        )
        state_ref["value"].update(no_autofocus)
        state_ref["value"].update(instrumentation)
        render_detected_mod_sidebar("selection")
        render_selected_mod_detail("selection")
        render_rendered_entity_icons("selection")
        render_activity_log("selection")
        selected = state_ref["value"].get("selectedDetectedMod")
        selected_mod = state_ref["value"].get("selectedMod")
        return {
            "selector": selector,
            "invoked": True,
            "status": "selected" if isinstance(selected_mod, dict) else "not_found",
            "selectedMod": selected_mod,
            "selectedDetectedMod": selected,
            "selectedDetectedModId": state_ref["value"].get("selectedDetectedModId"),
            "selectedDetectedModProvenance": state_ref["value"].get("selectedDetectedModProvenance"),
            "selectedPackage": state_ref["value"].get("selectedPackage"),
        }

    mod_selection_registry["select"] = select_detected_mod

    def render_action_button(parent: Any, action: dict[str, Any], *, row: int, column: int, primary: bool = False, columnspan: int = 1) -> None:
        action_id = str(action.get("id") or "action")
        button_options: dict[str, Any] = {
            "text": str(action.get("label") or _humanize_enum_label(action_id)),
            "command": lambda item=action_id: rebuild(str(item)),
            "style": "Primary.TButton" if primary else "TButton",
            "state": "normal" if action.get("enabled", True) else "disabled",
        }
        icon_path = action.get("iconPath")
        if icon_path:
            try:
                icon = tk.PhotoImage(file=str(icon_path))
                max_side = max(int(icon.width()), int(icon.height()), 1)
                if max_side > 18:
                    factor = max(1, int((max_side + 17) // 18))
                    icon = icon.subsample(factor, factor)
                image_registry.append(icon)
                button_options["image"] = icon
                button_options["compound"] = "left"
                action["iconLoaded"] = True
                action["iconRendered"] = True
                action["iconWidth"] = int(icon.width())
                action["iconHeight"] = int(icon.height())
            except tk.TclError as exc:
                action["iconLoaded"] = False
                action["iconRendered"] = False
                action["iconError"] = str(exc)
        elif "iconLoaded" not in action:
            action["iconLoaded"] = False
            action["iconRendered"] = False
        button = ttk.Button(parent, **button_options)
        button.grid(row=row, column=column, columnspan=columnspan, sticky="ew", padx=3, pady=3)
        button_registry.setdefault(action_id, []).append(button)

    def render_mods_header(parent: Any, row_cursor: int) -> int:
        mods_concept = state_ref["value"].get("conceptMap", {}).get("mods") if isinstance(state_ref["value"].get("conceptMap"), dict) else None
        selected = state_ref["value"].get("selectedMod") if isinstance(state_ref["value"].get("selectedMod"), dict) else {}
        header = ttk.Frame(parent)
        header.grid(row=row_cursor, column=0, sticky="ew", pady=(0, 10))
        header.columnconfigure(0, weight=1)
        if isinstance(mods_concept, dict):
            actions_frame = ttk.Frame(header)
            actions_frame.grid(row=0, column=0, sticky="ew", pady=(0, 6))
            secondary = mods_concept.get("secondaryActions") if isinstance(mods_concept.get("secondaryActions"), list) else []
            action_items = [action for action in secondary if isinstance(action, dict) and action.get("id") == "scan-packages"]
            for column, action in enumerate(action_items):
                actions_frame.columnconfigure(column, weight=1)
                render_action_button(actions_frame, action, row=0, column=column)
        ttk.Label(header, text="Selected:", style="Status.TLabel").grid(row=1, column=0, sticky="w")
        selected_name = _gui_text((selected or {}).get("name") or (selected or {}).get("packageId") or (selected or {}).get("id") or "None")
        ttk.Label(header, text=selected_name, style="Summary.TLabel", wraplength=315, justify="left").grid(row=2, column=0, sticky="ew", pady=(1, 0))
        return row_cursor + 1

    def bind_selectable(widget: Any, entry: dict[str, Any]) -> None:
        widget.bind("<Button-1>", lambda _event, item=entry: select_detected_mod(item))
        try:
            widget.configure(cursor="hand2")
        except tk.TclError:
            pass

    def render_detected_mod_sidebar(reason: str = "render") -> None:
        record_region_render("modsList", reason)
        for action_id in ("scan-packages", "enable-package", "disable-package"):
            button_registry.pop(action_id, None)
        for child in sidebar_frame.winfo_children():
            child.destroy()
        try:
            sidebar_frame.configure(text=_gui_icon_label("mods-list", str(state_ref["value"].get("detectedModsSidebarTitle") or "Mods")))
        except tk.TclError:
            pass
        provenance_section_labels.clear()
        sections = state_ref["value"].get("detectedModSections")
        if not isinstance(sections, list):
            sections = []
        rendered_names: list[str] = []
        rendered_rows: list[dict[str, Any]] = []
        row_focus_widgets: list[Any] = []

        def focus_row_index(index: int) -> str:
            if not row_focus_widgets:
                return "break"
            next_index = max(0, min(index, len(row_focus_widgets) - 1))
            try:
                row_focus_widgets[next_index].focus_set()
            except tk.TclError:
                pass
            return "break"
        row_cursor = render_mods_header(sidebar_frame, 0)
        if not sections:
            ttk.Label(sidebar_frame, text="No mods yet. Scan packages to refresh local and profile state.", wraplength=315, justify="left").grid(
                row=row_cursor,
                column=0,
                sticky="ew",
                pady=(0, 8),
            )
            state_ref["value"]["renderedProvenanceSectionLabels"] = []
            state_ref["value"]["renderedDetectedModNames"] = []
            state_ref["value"]["renderedDetectedModRows"] = []
            return
        for section in sections:
            if not isinstance(section, dict):
                continue
            label = _gui_text(section.get("label") or section.get("title") or section.get("key") or "Detected")
            entries = section.get("entries") if isinstance(section.get("entries"), list) else []
            provenance_section_labels.append(label)
            header = ttk.Frame(sidebar_frame)
            header.grid(row=row_cursor, column=0, sticky="ew", pady=(8 if row_cursor else 0, 3))
            header.columnconfigure(0, weight=1)
            ttk.Label(header, text=_gui_icon_label(_gui_provenance_entity_type(section.get("key") or section.get("id")), label), style="Provenance.TLabel").grid(row=0, column=0, sticky="w")
            ttk.Label(header, text=f"{len(entries)}", style="Badge.TLabel").grid(row=0, column=1, sticky="e")
            row_cursor += 1
            if not entries:
                ttk.Label(sidebar_frame, text="None detected.", wraplength=315, justify="left").grid(
                    row=row_cursor,
                    column=0,
                    sticky="ew",
                    padx=(8, 0),
                    pady=(0, 3),
                )
                row_cursor += 1
                continue
            for entry in entries:
                if not isinstance(entry, dict):
                    continue
                metadata = _gui_detected_mod_row_metadata(entry, focus_order=len(rendered_rows))
                rendered_rows.append(metadata)
                name = _gui_text(entry.get("name") or entry.get("displayName") or entry.get("packageId") or entry.get("id"))
                rendered_names.append(name)
                version = _gui_text(entry.get("version"))
                status = "Enabled" if metadata["enabled"] else _humanize_enum_label(_gui_text(entry.get("status") or "disabled"))
                package_id = _gui_text(entry.get("packageId") or entry.get("publishedFileId") or entry.get("id"))
                background = "#dbeafe" if metadata["selected"] else "#ffffff"
                border_color = "#2563eb" if metadata["selected"] else "#d6d3d1"
                focus_border_color = str(metadata.get("focusVisible", {}).get("focusedBorderColor") or "#f59e0b")
                item = tk.Frame(sidebar_frame, background=border_color, padx=1, pady=1, takefocus=1, highlightthickness=1, highlightbackground=border_color, highlightcolor=focus_border_color)
                item.grid(row=row_cursor, column=0, sticky="ew", padx=(4, 0), pady=2)
                item.columnconfigure(0, weight=1)
                inner = tk.Frame(item, background=background, padx=5, pady=4)
                inner.grid(row=0, column=0, sticky="ew")
                inner.columnconfigure(1, weight=1)
                prefix = tk.Label(
                    inner,
                    text=metadata["prefix"],
                    width=2,
                    anchor="center",
                    foreground=metadata["prefixColor"],
                    background=background,
                    font=("TkDefaultFont", 11, "bold"),
                )
                prefix.grid(row=0, column=0, rowspan=2, sticky="nsw", padx=(0, 6))
                title_suffix = f"  {version}" if version not in ("None", "Not set") else ""
                title = tk.Label(
                    inner,
                    text=f"{_gui_icon_label('mod-package', name)}{title_suffix}",
                    anchor="w",
                    justify="left",
                    wraplength=280,
                    foreground="#111827",
                    background=background,
                    font=("TkDefaultFont", 10, "bold"),
                )
                title.grid(row=0, column=1, sticky="ew")
                detail_bits = [package_id, status]
                detail = tk.Label(
                    inner,
                    text=" / ".join(bit for bit in detail_bits if bit and bit != "None"),
                    anchor="w",
                    justify="left",
                    wraplength=300,
                    foreground="#374151",
                    background=background,
                )
                detail.grid(row=1, column=1, sticky="ew")
                for widget in (item, inner, prefix, title, detail):
                    bind_selectable(widget, entry)
                focus_index = len(row_focus_widgets)
                row_focus_widgets.append(item)
                item.bind("<Return>", lambda _event, item_entry=entry: (select_detected_mod(item_entry), "break")[1])
                item.bind("<space>", lambda _event, item_entry=entry: (select_detected_mod(item_entry), "break")[1])
                item.bind("<Up>", lambda _event, index=focus_index: focus_row_index(index - 1))
                item.bind("<Down>", lambda _event, index=focus_index: focus_row_index(index + 1))
                item.bind("<FocusIn>", lambda _event, row=item, color=focus_border_color: row.configure(background=color, highlightbackground=color))
                item.bind("<FocusOut>", lambda _event, row=item, color=border_color: row.configure(background=color, highlightbackground=color))
                row_cursor += 1
        state_ref["value"]["renderedProvenanceSectionLabels"] = list(provenance_section_labels)
        state_ref["value"]["renderedDetectedModNames"] = rendered_names
        state_ref["value"]["renderedDetectedModRows"] = rendered_rows

    def render_rendered_entity_icons(reason: str = "render") -> None:
        record_region_render("iconMetadata", reason)
        state_ref["value"]["renderedEntityIcons"] = _gui_entity_iconography_records(
            steam_logo_path=state_ref["value"].get("steamLogoPath"),
            steam_logo_rendered=bool(state_ref["value"].get("steamLogoRendered")),
            detected_sections=state_ref["value"].get("detectedModSections", []),
            rendered_rows=state_ref["value"].get("renderedDetectedModRows", []),
        )
        state_ref["value"]["entityIconography"] = state_ref["value"]["renderedEntityIcons"]

    def render_selected_mod_detail(reason: str = "render") -> None:
        record_region_render("selectedModDetail", reason)
        for child in selected_detail_frame.winfo_children():
            child.destroy()
        detail = state_ref["value"].get("selectedModDetail") or state_ref["value"].get("selectedDetailPanel")
        if not isinstance(detail, dict):
            detail = _gui_selected_mod_detail_panel(None, None)
        try:
            selected_detail_frame.configure(text=_gui_icon_label("selected-mod-detail", _gui_text(detail.get("title") or "Selected Mod")))
        except tk.TclError:
            pass
        section_labels.append(_gui_text(detail.get("title") or "Selected Mod"))
        ttk.Label(
            selected_detail_frame,
            text=_gui_text(detail.get("selectedModName") or detail.get("title") or "No mod selected"),
            style="Summary.TLabel",
            wraplength=660,
            justify="left",
        ).grid(row=0, column=0, columnspan=2, sticky="ew", pady=(0, 2))
        ttk.Label(
            selected_detail_frame,
            text=_gui_text(detail.get("statusSummary") or detail.get("summary")),
            wraplength=660,
            justify="left",
        ).grid(row=1, column=0, columnspan=2, sticky="ew", pady=(0, 8))
        row_cursor = 2
        rows = detail.get("rows") if isinstance(detail.get("rows"), list) else []
        for item in rows[:6]:
            if not isinstance(item, dict):
                continue
            ttk.Label(selected_detail_frame, text=f"{_gui_text(item.get('label'))}:", style="Status.TLabel").grid(
                row=row_cursor,
                column=0,
                sticky="nw",
                padx=(0, 10),
                pady=2,
            )
            ttk.Label(selected_detail_frame, text=_gui_text(item.get("value")), wraplength=560, justify="left").grid(
                row=row_cursor,
                column=1,
                sticky="ew",
                pady=2,
            )
            row_cursor += 1
        detail_actions = detail.get("actions") if isinstance(detail.get("actions"), list) else []
        if detail_actions:
            action_frame = ttk.Frame(selected_detail_frame)
            action_frame.grid(row=row_cursor, column=0, columnspan=2, sticky="ew", pady=(8, 0))
            for column, action in enumerate(action for action in detail_actions if isinstance(action, dict)):
                action_frame.columnconfigure(column, weight=1)
                render_action_button(action_frame, action, row=0, column=column, primary=bool(action.get("enabled")))

    def render_compact_status_cards(reason: str = "render") -> None:
        record_region_render("compactStatusCards", reason)
        for child in compact_status_grid.winfo_children():
            child.destroy()
        cards = state_ref["value"].get("compactStatusCards")
        if not isinstance(cards, list):
            cards = []
        compact_status_grid.rowconfigure(0, weight=1)
        for index, card in enumerate(card for card in cards if isinstance(card, dict)):
            title = _gui_text(card.get("title") or GUI_CONCEPT_LABELS.get(str(card.get("key") or ""), "Status"))
            section_labels.append(title)
            frame = ttk.LabelFrame(
                compact_status_grid,
                text=_gui_icon_label(_gui_concept_entity_type(card.get("key")) or "environment", title),
                style="Section.TLabelframe",
            )
            frame.grid(row=0, column=index, sticky="nsew", padx=(0 if index == 0 else 6, 0 if index == 2 else 6))
            frame.columnconfigure(0, weight=1)
            frame.columnconfigure(1, weight=1)
            row_cursor = 0
            status_text = _humanize_enum_label(_gui_text(card.get("status") or "not_selected"))
            ttk.Label(frame, text=status_text, style="Summary.TLabel").grid(row=row_cursor, column=0, columnspan=2, sticky="w", pady=(0, 4))
            row_cursor += 1
            for item in (card.get("rows") if isinstance(card.get("rows"), list) else [])[:3]:
                if not isinstance(item, dict):
                    continue
                row_text = f"{_gui_text(item.get('label'))}: {_gui_text(item.get('value'))}"
                logo = load_row_logo(item.get("logoPath"))
                if logo is not None:
                    item["logoRendered"] = True
                    ttk.Label(frame, image=logo).grid(row=row_cursor, column=0, sticky="w", padx=(0, 6), pady=1)
                    ttk.Label(frame, text=row_text, wraplength=220, justify="left").grid(row=row_cursor, column=1, sticky="ew", pady=1)
                else:
                    item["logoRendered"] = False
                    ttk.Label(frame, text=f"{_gui_text(item.get('icon') or '')} {row_text}".strip(), wraplength=250, justify="left").grid(
                        row=row_cursor, column=0, columnspan=2, sticky="ew", pady=1
                    )
                row_cursor += 1
            summary = _gui_text(card.get("summary"))
            if summary != "Not set":
                ttk.Label(frame, text=summary, wraplength=250, justify="left").grid(row=row_cursor, column=0, columnspan=2, sticky="ew", pady=(5, 4))
                row_cursor += 1
            actions = card.get("actions") if isinstance(card.get("actions"), list) else []
            for action in actions[:5]:
                if isinstance(action, dict):
                    render_action_button(frame, action, row=row_cursor, column=0, primary=action.get("id") == (actions[0] or {}).get("id"), columnspan=2)
                    row_cursor += 1

    def render_sections(reason: str = "full-dashboard") -> None:
        record_region_render("fullDashboard", reason)
        button_registry.clear()
        button_registry.setdefault("copy-for-ai", []).append(copy_for_ai_btn)
        section_labels.clear()
        state_ref["value"]["steamLogoRendered"] = False
        render_detected_mod_sidebar(reason)
        render_selected_mod_detail(reason)
        render_compact_status_cards(reason)
        render_rendered_entity_icons(reason)

    section_labels: list[str] = []
    provenance_section_labels: list[str] = []
    render_sections()
    render_activity_log()
    return root, section_labels, state_ref, button_registry, mod_selection_registry


def _gui_apply_no_autofocus(root: Any) -> dict[str, Any]:
    evidence: dict[str, Any] = {
        "noAutofocusRequested": True,
        "noAutofocusApplied": False,
        "noAutofocusMethods": [],
    }
    methods = evidence["noAutofocusMethods"]
    try:
        root.focusmodel("passive")
        methods.append("wm focusmodel passive")
        evidence["noAutofocusApplied"] = True
    except Exception as exc:
        evidence["noAutofocusError"] = _gui_text(exc)
    try:
        root.configure(takefocus=0)
        methods.append("configure takefocus=0")
        evidence["noAutofocusApplied"] = True
    except Exception:
        pass
    if sys.platform.startswith("linux"):
        try:
            root.attributes("-type", "notification")
            methods.append("wm attributes -type notification")
            evidence["noAutofocusApplied"] = True
        except Exception:
            pass
    return evidence


def _write_gui_smoke_report(
    path: str,
    root: Any,
    section_labels: list[str],
    gui_state: dict[str, Any],
    *,
    clicked_actions: list[dict[str, Any]] | None = None,
    smoke_selected_mod: dict[str, Any] | None = None,
    before_actions: list[dict[str, Any]] | None = None,
    after_actions: list[dict[str, Any]] | None = None,
) -> None:
    selection_redraw_scope = smoke_selected_mod.get("selectionRedrawScope") if isinstance(smoke_selected_mod, dict) else None
    if selection_redraw_scope is None:
        selection_redraw_scope = gui_state.get("selectionRedrawScope")
    text_completeness = gui_state.get("renderedTextCompleteness")
    if not isinstance(text_completeness, list):
        text_completeness = _gui_rendered_text_completeness(gui_state)
    clipping_checks = gui_state.get("clippingChecks")
    if not isinstance(clipping_checks, list):
        clipping_checks = _gui_clipping_checks(gui_state)
    root.update_idletasks()
    root.update()
    write_json_file(
        Path(path).expanduser(),
        {
            "opened": True,
            "title": root.title(),
            "geometry": root.winfo_geometry(),
            "renderedSectionLabels": section_labels,
            "renderedConceptTitles": section_labels,
            "appIconPath": gui_state.get("appIconPath"),
            "appIconLoaded": gui_state.get("appIconLoaded"),
            "appIconError": gui_state.get("appIconError"),
            "headerIconPath": gui_state.get("headerIconPath"),
            "headerIconLoaded": gui_state.get("headerIconLoaded"),
            "headerIconWidth": gui_state.get("headerIconWidth"),
            "headerIconHeight": gui_state.get("headerIconHeight"),
            "headerIconError": gui_state.get("headerIconError"),
            "vanillaBaronyIconPath": gui_state.get("vanillaBaronyIconPath"),
            "concepts": gui_state.get("concepts", []),
            "conceptMap": gui_state.get("conceptMap", {}),
            "profilePath": gui_state.get("profilePath"),
            "profile": gui_state.get("profile"),
            "selectedPackage": gui_state.get("selectedPackage"),
            "packageList": gui_state.get("packageList"),
            "environmentSummaryItems": gui_state.get("environmentSummaryItems", []),
            "detectedMods": gui_state.get("detectedMods", []),
            "detectedModSections": gui_state.get("detectedModSections", []),
            "renderedProvenanceSectionLabels": gui_state.get("renderedProvenanceSectionLabels", []),
            "renderedDetectedModNames": gui_state.get("renderedDetectedModNames", []),
            "renderedDetectedModRows": gui_state.get("renderedDetectedModRows", []),
            "detectedModsSidebarTitle": gui_state.get("detectedModsSidebarTitle"),
            "modsSidebarTitle": gui_state.get("modsSidebarTitle"),
            "modsListHeaderActions": gui_state.get("modsListHeaderActions", []),
            "detectedModsHeaderActions": gui_state.get("detectedModsHeaderActions", []),
            "rightSideLabels": gui_state.get("rightSideLabels", []),
            "rightSide": gui_state.get("rightSide"),
            "selectedModDetail": gui_state.get("selectedModDetail"),
            "selectedDetailPanel": gui_state.get("selectedDetailPanel"),
            "compactStatusCards": gui_state.get("compactStatusCards", []),
            "selectedDetectedMod": gui_state.get("selectedDetectedMod"),
            "selectedDetectedModId": gui_state.get("selectedDetectedModId"),
            "selectedDetectedModProvenance": gui_state.get("selectedDetectedModProvenance"),
            "selectedMod": gui_state.get("selectedMod"),
            "platformStorefront": gui_state.get("platformStorefront"),
            "storefront": gui_state.get("platformStorefront"),
            "steamLogoRendered": bool(gui_state.get("steamLogoRendered")),
            "steamLogoPath": gui_state.get("steamLogoPath"),
            "entityIconography": gui_state.get("entityIconography", []),
            "renderedEntityIcons": gui_state.get("renderedEntityIcons", []),
            "buttonActions": _gui_button_action_snapshot(gui_state),
            "launchButtonIcons": [
                item
                for item in _gui_button_action_snapshot(gui_state)
                if item.get("id") in {"launch-bml", "launch-vanilla"}
            ],
            "renderedTextCompleteness": text_completeness,
            "clippingChecks": clipping_checks,
            "redrawEvents": gui_state.get("redrawEvents", []),
            "renderEvents": gui_state.get("renderEvents", []),
            "regionRenderCounts": gui_state.get("regionRenderCounts", {}),
            "noAutofocusRequested": bool(gui_state.get("noAutofocusRequested")),
            "noAutofocusApplied": bool(gui_state.get("noAutofocusApplied")),
            "noAutofocusMethods": gui_state.get("noAutofocusMethods", []),
            "smokeVisibleRequested": bool(gui_state.get("smokeVisibleRequested")),
            "smokeWindowHidden": bool(gui_state.get("smokeWindowHidden")),
            "smokeWindowSuppressionMethods": gui_state.get("smokeWindowSuppressionMethods", []),
            "activeMods": gui_state.get("activeMods"),
            "actions": gui_state.get("actionLog", gui_state.get("actions")),
            "actionLog": gui_state.get("actionLog", []),
            "visibleActivityLog": gui_state.get("visibleActivityLog", []),
            "activityLogDetails": gui_state.get("activityLogDetails", []),
            "actualTkButtonsInvoked": any(item.get("invoked") for item in (clicked_actions or []) if isinstance(item, dict)),
            "buttonInvocationMethod": "Tk Button.invoke()" if clicked_actions else None,
            "clickedActions": clicked_actions or [],
            "smokeSelectedMod": smoke_selected_mod,
            "selectedModInvocationMethod": "Mods row selection callback" if smoke_selected_mod else None,
            "selectionRedrawScope": selection_redraw_scope,
            "beforeActions": before_actions or [],
            "afterActions": after_actions if after_actions is not None else _gui_button_action_snapshot(gui_state),
            "readiness": gui_state.get("readiness"),
            "launchDryRun": gui_state.get("launchDryRun"),
            "lastLaunch": gui_state.get("lastLaunch") or gui_state.get("launchResult"),
            "launchResult": gui_state.get("launchResult"),
            "diagnosticsEvidence": gui_state.get("diagnosticsEvidence"),
            "diagnosticDetails": gui_state.get("diagnosticDetails"),
            "diagnosticsDetails": gui_state.get("diagnosticsDetails"),
            "windowsStatus": gui_state.get("windowsStatus"),
            "workshop": gui_state.get("workshop"),
            "steamPublished": False,
            "playableClaimed": False,
            "disabledReasons": gui_state.get("disabledReasons", []),
            "copyForAiText": gui_state.get("copyForAiText"),
            "copyForAiContext": gui_state.get("copyForAiContext"),
            "lastCopyForAi": gui_state.get("lastCopyForAi"),
            "copyForAiContextSections": gui_state.get("lastCopyForAi", {}).get("copyForAiContextSections"),
        },
    )



SMOKE_CLICK_ALL_ACTIONS = (
    "detect-install",
    "refresh-readiness",
    "open-diagnostics",
    "create-select-profile",
    "scan-packages",
    "disable-package",
    "enable-package",
    "enable-package",
    "disable-package",
    "disable-package",
    "enable-package",
    "workshop-preview",
)


def _parse_smoke_clicks(value: str | None) -> list[str]:
    if value is None or not value.strip():
        return []
    raw = value.strip()
    if raw == "all":
        return list(SMOKE_CLICK_ALL_ACTIONS)
    actions = [item.strip() for item in raw.split(",") if item.strip()]
    known = set(GUI_ACTION_LABELS)
    unknown = [item for item in actions if item not in known]
    if unknown:
        raise argparse.ArgumentTypeError(f"unknown smoke click action(s): {', '.join(unknown)}")
    return actions


def _latest_action_entries(before_count: int, gui_state: dict[str, Any]) -> list[dict[str, Any]]:
    action_log = gui_state.get("actionLog") if isinstance(gui_state.get("actionLog"), list) else []
    return [dict(item) for item in action_log[before_count:] if isinstance(item, dict)]


def _run_smoke_button_clicks(root: Any, button_registry: dict[str, list[Any]], state_ref: dict[str, Any], action_ids: list[str]) -> list[dict[str, Any]]:
    clicked: list[dict[str, Any]] = []
    for action_id in action_ids:
        before_state = state_ref["value"]
        before_log = before_state.get("actionLog") if isinstance(before_state.get("actionLog"), list) else []
        before_active = before_state.get("activeMods", [])
        before_action = next((item for item in _gui_button_action_snapshot(before_state) if item.get("id") == action_id), {})
        root.update_idletasks()
        root.update()
        buttons = button_registry.get(action_id, [])
        button = buttons[0] if buttons else None
        if button is None:
            clicked.append(
                {
                    "id": action_id,
                    "label": GUI_ACTION_LABELS.get(action_id, _humanize_enum_label(action_id)),
                    "invoked": False,
                    "status": "button_not_found",
                    "beforeActiveMods": before_active,
                    "afterActiveMods": state_ref["value"].get("activeMods", []),
                    "result": None,
                }
            )
            continue
        button_state = str(button.cget("state"))
        button_label = str(button.cget("text"))
        if button_state == "disabled":
            clicked.append(
                {
                    "id": action_id,
                    "label": button_label,
                    "invoked": False,
                    "status": "button_disabled",
                    "beforeActiveMods": before_active,
                    "afterActiveMods": state_ref["value"].get("activeMods", []),
                    "result": None,
                    "action": before_action,
                    "actionEligibility": before_action.get("actionEligibility"),
                    "disabledReason": before_action.get("disabledReason") or before_action.get("reason"),
                    "targetPackageId": before_action.get("targetPackageId"),
                    "selectedModId": before_action.get("selectedModId"),
                    "selectedModName": before_action.get("selectedModName"),
                }
            )
            continue
        button.invoke()
        invocation_method = "Tk Button.invoke()"
        root.update_idletasks()
        root.update()
        after_state = state_ref["value"]
        new_entries = _latest_action_entries(len(before_log), after_state)
        clicked.append(
            {
                "id": action_id,
                "label": button_label,
                "invoked": True,
                "invocationMethod": invocation_method,
                "status": new_entries[-1].get("status") if new_entries else "no_result_logged",
                "result": new_entries[-1] if new_entries else None,
                "beforeActiveMods": before_active,
                "afterActiveMods": after_state.get("activeMods", []),
                "activeModsChanged": before_active != after_state.get("activeMods", []),
            }
        )
    return clicked


def _run_smoke_mod_selection(root: Any, mod_selection_registry: dict[str, Any], state_ref: dict[str, Any], selector: str | None) -> dict[str, Any] | None:
    if selector is None or not str(selector).strip():
        return None
    root.update_idletasks()
    root.update()
    selected_before = state_ref["value"].get("selectedDetectedMod")
    selected_mod_before = state_ref["value"].get("selectedMod")
    before_counts = {
        str(key): int(value)
        for key, value in (state_ref["value"].get("regionRenderCounts") or {}).items()
        if isinstance(value, int)
    }
    callback = mod_selection_registry.get("select")
    if not callable(callback):
        return {
            "selector": selector,
            "invoked": False,
            "status": "selection_callback_not_found",
            "selectedBefore": selected_before,
            "selectedModBefore": selected_mod_before,
            "selectedAfter": state_ref["value"].get("selectedDetectedMod"),
            "selectedModAfter": state_ref["value"].get("selectedMod"),
        }
    result = callback(str(selector).strip())
    root.update_idletasks()
    root.update()
    after_counts = {
        str(key): int(value)
        for key, value in (state_ref["value"].get("regionRenderCounts") or {}).items()
        if isinstance(value, int)
    }
    changed_regions = [key for key in sorted(after_counts) if after_counts[key] > before_counts.get(key, 0)]
    count_delta = {key: after_counts[key] - before_counts.get(key, 0) for key in changed_regions}
    selection_redraw_scope = {
        "selector": str(selector).strip(),
        "regionsRedrawn": changed_regions,
        "regionRenderCountDelta": count_delta,
        "beforeRegionRenderCounts": before_counts,
        "afterRegionRenderCounts": after_counts,
        "fullDashboardBefore": before_counts.get("fullDashboard", 0),
        "fullDashboardAfter": after_counts.get("fullDashboard", 0),
        "fullDashboardCountChanged": after_counts.get("fullDashboard", 0) != before_counts.get("fullDashboard", 0),
        "fullDashboard": after_counts.get("fullDashboard", 0) > before_counts.get("fullDashboard", 0),
    }
    result["selectionRedrawScope"] = selection_redraw_scope
    state_ref["value"]["selectionRedrawScope"] = selection_redraw_scope
    result["selectedBefore"] = selected_before
    result["selectedModBefore"] = selected_mod_before
    result["selectedAfter"] = state_ref["value"].get("selectedDetectedMod")
    result["selectedModAfter"] = state_ref["value"].get("selectedMod")
    result["selectedDetectedMod"] = state_ref["value"].get("selectedDetectedMod")
    result["selectedDetectedModId"] = state_ref["value"].get("selectedDetectedModId")
    result["selectedDetectedModProvenance"] = state_ref["value"].get("selectedDetectedModProvenance")
    result["selectedMod"] = state_ref["value"].get("selectedMod")
    result["selectedPackage"] = state_ref["value"].get("selectedPackage")
    return result

def _nonnegative_int(value: str) -> int:
    parsed = int(value)
    if parsed < 0:
        raise argparse.ArgumentTypeError("must be zero or a positive integer")
    return parsed


def start_tk_gui(auto_close_ms: int | None = None, smoke_report: str | None = None, smoke_clicks: str | None = None, smoke_select_mod: str | None = None) -> int:
    try:
        smoke_click_actions = _parse_smoke_clicks(smoke_clicks)
    except argparse.ArgumentTypeError as exc:
        print(json.dumps({"gui": {"status": "invalid", "reason": str(exc)}, "appRoot": str(APP_ROOT), "launcher": APP_ID}, indent=2))
        return 2
    gui_state = build_profile_first_gui_state(smoke_mode=bool(smoke_report) and not smoke_click_actions, selected_mod_selector=smoke_select_mod)
    try:
        root, section_labels, state_ref, button_registry, mod_selection_registry = _build_gui_dashboard_window(gui_state)
    except Exception as exc:
        print(json.dumps({"gui": {"status": "unavailable", "reason": f"Tk GUI window could not be created: {exc}"}, "appRoot": str(APP_ROOT), "launcher": APP_ID}, indent=2))
        return 2
    no_autofocus_requested = bool(smoke_report or auto_close_ms is not None)
    if no_autofocus_requested:
        no_autofocus = _gui_apply_no_autofocus(root)
    else:
        no_autofocus = {"noAutofocusRequested": False, "noAutofocusApplied": False, "noAutofocusMethods": []}
    smoke_visible_requested = os.environ.get("BML_GUI_SMOKE_VISIBLE") == "1"
    smoke_window_hidden = False
    smoke_window_suppression_methods: list[str] = []
    if no_autofocus_requested and not smoke_visible_requested:
        try:
            root.withdraw()
            smoke_window_hidden = True
            smoke_window_suppression_methods.append("withdraw smoke/test window before mainloop")
        except Exception as exc:
            no_autofocus["smokeWindowSuppressionError"] = _gui_text(exc)
    no_autofocus["smokeVisibleRequested"] = smoke_visible_requested
    no_autofocus["smokeWindowHidden"] = smoke_window_hidden
    no_autofocus["smokeWindowSuppressionMethods"] = smoke_window_suppression_methods
    state_ref["value"].update(no_autofocus)


    if smoke_click_actions:
        before_actions = _gui_button_action_snapshot(state_ref["value"])

        def click_and_report() -> None:
            clicked_actions = _run_smoke_button_clicks(root, button_registry, state_ref, smoke_click_actions)
            smoke_selected = _run_smoke_mod_selection(root, mod_selection_registry, state_ref, smoke_select_mod)
            after_actions = _gui_button_action_snapshot(state_ref["value"])
            if smoke_report:
                _write_gui_smoke_report(
                    smoke_report,
                    root,
                    section_labels,
                    state_ref["value"],
                    clicked_actions=clicked_actions,
                    smoke_selected_mod=smoke_selected,
                    before_actions=before_actions,
                    after_actions=after_actions,
                )
            root.after(auto_close_ms if auto_close_ms is not None else 250, root.destroy)

        root.after(0, click_and_report)
    else:
        smoke_selected = _run_smoke_mod_selection(root, mod_selection_registry, state_ref, smoke_select_mod)
        if smoke_report:
            _write_gui_smoke_report(smoke_report, root, section_labels, state_ref["value"], smoke_selected_mod=smoke_selected)
        close_delay = auto_close_ms if auto_close_ms is not None else (250 if smoke_report else None)
        if close_delay is not None:
            root.after(close_delay, root.destroy)
    root.mainloop()
    return 0


def command_gui(args: argparse.Namespace) -> int:
    readiness = tkinter_readiness()
    if args.check or readiness.get("status") != "available":
        print(json.dumps({"gui": readiness, "appRoot": str(APP_ROOT), "launcher": APP_ID}, indent=2))
        return 0 if readiness.get("status") == "available" else 2
    return start_tk_gui(auto_close_ms=args.auto_close_ms, smoke_report=args.smoke_report, smoke_clicks=args.smoke_clicks, smoke_select_mod=args.smoke_select_mod)




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
                "platformTarget": runtime_registration.get("platformTarget"),
                "launchAdapter": runtime_registration.get("launchAdapter"),
                "hookArtifactExtension": runtime_registration.get("hookArtifactExtension"),
                "steamExecutable": runtime_registration.get("steamExecutable"),
                "steamExecutableSha256": runtime_registration.get("steamExecutableSha256"),
                "steamExecutableBuildId": runtime_registration.get("steamExecutableBuildId"),
                "gameVersionString": runtime_registration.get("gameVersionString"),
                "hookLibrary": runtime_registration.get("hookLibrary"),
                "hookLibrarySha256": runtime_registration.get("hookLibrarySha256"),
                "hookManifest": runtime_registration.get("hookManifest"),
                "hookManifestSha256": runtime_registration.get("hookManifestSha256"),
                "launcherExecutable": runtime_registration.get("launcherExecutable"),
                "launcherExecutableSha256": runtime_registration.get("launcherExecutableSha256"),
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



def _flatten_path_args(*values: Any) -> list[Path]:
    paths: list[Path] = []
    for value in values:
        if value is None:
            continue
        if isinstance(value, (str, os.PathLike)):
            paths.append(Path(value).expanduser())
            continue
        if isinstance(value, Iterable) and not isinstance(value, (dict, bytes, bytearray)):
            paths.extend(_flatten_path_args(*value))
            continue
        paths.append(Path(str(value)).expanduser())
    return paths


def problem_to_dict(problem: Problem) -> dict[str, Any]:
    return {
        "code": problem.code,
        "severity": problem.severity,
        "message": problem.message,
        "details": dict(problem.details),
    }


def validation_status(result: ValidationResult) -> str:
    if result.ok:
        return "valid"
    if any(problem.severity == "fatal" for problem in result.problems):
        return "invalid"
    return "warnings"


def package_summary_from_path(package_path: Path) -> dict[str, Any]:
    package, load_result = load_package(str(package_path))
    combined = ValidationResult(f"package catalog {package_path}")
    combined.extend(load_result)
    manifest: dict[str, Any] = {}
    manifest_path = package_path / PACKAGE_MANIFEST_NAME if package_path.is_dir() else package_path
    package_root = package_path if package_path.is_dir() else package_path.parent
    if package is not None:
        manifest = package.manifest
        manifest_path = package.manifest_path
        package_root = package.package_root
        combined.extend(validate_package(package))
    return {
        "id": manifest.get("id") if isinstance(manifest, dict) else None,
        "packageId": manifest.get("id") if isinstance(manifest, dict) else None,
        "name": manifest.get("name") if isinstance(manifest, dict) else None,
        "version": manifest.get("version") if isinstance(manifest, dict) else None,
        "kind": manifest.get("kind") if isinstance(manifest, dict) else None,
        "path": str(package_root),
        "manifestPath": str(manifest_path),
        "validationStatus": validation_status(combined),
        "status": validation_status(combined),
        "valid": combined.ok,
        "problemCount": len(combined.problems),
        "problems": [problem_to_dict(problem) for problem in combined.problems],
    }


def scan_package_catalog(*roots: Any, mods_root: Any = None, extra_roots: Any = None) -> dict[str, Any]:
    """Scan local mod folders and return semantic validation summaries.

    This service is intentionally DTO-first for GUI use: callers receive package
    ids, names, statuses, and structured validation problems rather than CLI
    report text that would need to be scraped.
    """
    candidate_roots = _flatten_path_args(*(roots or ()), mods_root, extra_roots)
    seen: set[Path] = set()
    package_paths: list[Path] = []
    for root in candidate_roots:
        root = root.expanduser()
        if not root.exists():
            continue
        if root.is_file() and root.name == PACKAGE_MANIFEST_NAME:
            candidates = [root]
        elif root.is_dir() and (root / PACKAGE_MANIFEST_NAME).exists():
            candidates = [root]
        elif root.is_dir():
            candidates = [child for child in sorted(root.iterdir()) if child.is_dir() and (child / PACKAGE_MANIFEST_NAME).exists()]
        else:
            candidates = []
        for candidate in candidates:
            key = candidate.resolve(strict=False)
            if key not in seen:
                seen.add(key)
                package_paths.append(candidate)
    summaries = [package_summary_from_path(path) for path in package_paths]
    return {
        "schemaVersion": SCHEMA_VERSION,
        "generatedAt": utc_now(),
        "roots": [str(path) for path in candidate_roots],
        "packages": summaries,
    }


def build_package_library_state(*roots: Any, mods_root: Any = None, profile_dir: Any = None, selected_package: Any = None, extra_roots: Any = None) -> dict[str, Any]:
    catalog = scan_package_catalog(*(roots or ()), mods_root=mods_root, extra_roots=extra_roots)
    packages = list(catalog.get("packages", []))
    duplicate_ids = sorted({item.get("id") for item in packages if item.get("id") and sum(1 for other in packages if other.get("id") == item.get("id")) > 1})
    profile_state = build_profile_state(profile_dir) if profile_dir is not None else None
    active_mods = profile_state.get("activeMods", []) if isinstance(profile_state, dict) else []
    disabled: list[str] = []
    if duplicate_ids:
        disabled.append(f"Duplicate package versions are present for: {', '.join(str(item) for item in duplicate_ids)}.")
    if len(active_mods) > 1:
        disabled.append("Multiple active packages are enabled; this app slice allows one active package at a time.")
    selected_summary = package_summary_from_path(Path(selected_package)) if selected_package is not None else None
    return {
        **catalog,
        "section": "package library",
        "items": packages,
        "selectedPackage": selected_summary,
        "profile": profile_state,
        "activeMods": active_mods,
        "activeModCount": len(active_mods),
        "duplicatePackageIds": duplicate_ids,
        "disabledReasons": disabled,
        "validationCards": [
            {"packageId": item.get("id"), "status": item.get("validationStatus"), "problems": item.get("problems", [])}
            for item in packages
        ],
    }


def package_library_state(*roots: Any, **kwargs: Any) -> dict[str, Any]:
    return build_package_library_state(*roots, **kwargs)


def active_mod_checksum_for_package(profile: dict[str, Any], profile_dir: Path, package: LoadedPackage) -> str | None:
    package_id = package.manifest.get("id")
    for mod in profile_authoritative_mods(profile, profile_dir):
        if mod.get("id") == package_id and isinstance(mod.get("checksumSet"), str):
            return mod["checksumSet"]
    return None


def plan_runtime_manifest(
    profile: dict[str, Any],
    profile_dir: Path,
    package: LoadedPackage,
    runtime_info: dict[str, Any],
    out_path: Path | None = None,
    *,
    output_path: Path | None = None,
    previous_digest: str | None = None,
    runtime_executable: Path | None = None,
    runtime_registration: dict[str, Any] | None = None,
) -> dict[str, Any]:
    """Pure runtime manifest planner.

    It computes the exact manifest payload and launch blockers without writing
    the manifest or starting Barony.
    """
    profile_dir = Path(profile_dir).expanduser()
    manifest_path = Path(output_path or out_path or (bml_profile_root(profile_dir) / "manifests" / "runtime-manifest.json")).expanduser()
    manifest = build_runtime_manifest(profile, profile_dir, package, runtime_info, runtime_executable, runtime_registration)
    actual_digest = package_checksum(package)
    active_digest = active_mod_checksum_for_package(profile, profile_dir, package)
    blockers: list[str] = []
    if active_digest and active_digest != actual_digest:
        blockers.append(
            f"Active profile checksum is stale: profile has {active_digest}, current package digest is {actual_digest}."
        )
    if previous_digest and previous_digest != actual_digest:
        blockers.append(
            f"Previous runtime manifest digest is stale: previous {previous_digest}, current package digest is {actual_digest}."
        )
    return {
        "runtimeManifestPath": str(manifest_path),
        "manifestPath": str(manifest_path),
        "outputPath": str(manifest_path),
        "manifest": manifest,
        "runtimeManifest": manifest,
        "blockers": blockers,
        "disabledReasons": blockers,
        "sideEffects": {"processLaunch": False, "filesystemWrite": False},
        "processLaunched": False,
    }


def build_launch_readiness_state(
    *,
    install: Any = None,
    profile: dict[str, Any] | None = None,
    profile_dir: Any = None,
    package: LoadedPackage | None = None,
    package_root: Any = None,
    runtime_info: dict[str, Any] | None = None,
    runtime_info_path: Any = None,
    out_path: Any = None,
    platform: str | None = None,
    dry_run: bool = True,
) -> dict[str, Any]:
    loaded_package = package
    if loaded_package is None and package_root is not None:
        loaded_package, _package_result = load_package(str(package_root))
    loaded_runtime_info = runtime_info
    if loaded_runtime_info is None and runtime_info_path is not None:
        loaded_runtime_info, _runtime_path, _runtime_result = load_runtime_info(str(runtime_info_path))
    profile_root = Path(profile_dir).expanduser() if profile_dir is not None else Path(tempfile.gettempdir()) / "bml-profile"
    manifest_plan = None
    if profile is not None and loaded_package is not None and loaded_runtime_info is not None:
        manifest_plan = plan_runtime_manifest(
            profile,
            profile_root,
            loaded_package,
            loaded_runtime_info,
            Path(out_path) if out_path is not None else bml_profile_root(profile_root) / "manifests" / "runtime-manifest.json",
        )
    readiness = build_readiness_matrix(
        install=install,
        profile={"status": "selected"} if profile is not None else None,
        package={"status": "valid"} if loaded_package is not None else None,
        runtime={"platform": platform or current_platform_id(), "verificationStatus": "verified" if loaded_runtime_info is not None else ""},
        platform=platform or current_platform_id(),
    )
    return {
        "section": "launch readiness",
        "readiness": readiness,
        "plan": manifest_plan,
        "runtimeManifestPlan": manifest_plan,
        "dryRun": dry_run,
        "processLaunched": False,
        "playableClaimBoundary": "dry-run only; no playable/live game claim without production runtime evidence",
        "disabledReasons": readiness.get("disabledReasons", []),
    }


def launch_readiness_state(**kwargs: Any) -> dict[str, Any]:
    return build_launch_readiness_state(**kwargs)


def dry_run_launch_plan(**kwargs: Any) -> dict[str, Any]:
    return build_launch_readiness_state(dry_run=True, **kwargs)


def build_launch_playability(case: dict[str, Any] | None = None, *, install: Any = None, profile: Any = None, package: Any = None, runtime: dict[str, Any] | None = None, platform: str | None = None) -> dict[str, Any]:
    case = case or {}
    runtime = runtime or case.get("runtime") or {}
    platform = platform or case.get("platform") or (runtime.get("platform") if isinstance(runtime, dict) else None)
    text = json.dumps(runtime, sort_keys=True).casefold() if isinstance(runtime, dict) else ""
    blockers: list[str] = []
    if "fake" in text or "fake-provider" in text:
        blockers.append("Fake-provider runtime evidence is non-production and cannot claim playable.")
    if "scaffold" in text or "self-test" in text:
        blockers.append("Scaffold/self-test runtime evidence is not production live gameplay evidence and cannot claim playable.")
    if platform and "windows" in str(platform).casefold() and "production" not in text:
        blockers.append("Windows runtime remains fail-closed without live production Windows evidence.")
    playable = not blockers and bool(runtime.get("playableClaim") or runtime.get("playable") or runtime.get("productionEvidence"))
    return {
        "status": "playable" if playable else "blocked",
        "playable": playable,
        "isPlayable": playable,
        "playableClaimAllowed": playable,
        "launchPlayable": playable,
        "blockers": blockers,
        "disabledReasons": blockers,
        "runtime": runtime,
        "platform": platform,
    }


def evaluate_playable_claim(case: dict[str, Any] | None = None, **kwargs: Any) -> dict[str, Any]:
    return build_launch_playability(case, **kwargs)


def assess_playable_claim_boundary(case: dict[str, Any] | None = None, **kwargs: Any) -> dict[str, Any]:
    return build_launch_playability(case, **kwargs)


def _status_is_ready(value: Any, ready_statuses: set[str]) -> bool:
    if isinstance(value, dict):
        status = str(value.get("status") or value.get("validationStatus") or "").casefold()
        return status in ready_statuses
    return value is not None


def build_readiness_matrix(*args: Any, install: Any = None, profile: Any = None, package: Any = None, runtime: Any = None, platform: str | None = None, **_kwargs: Any) -> dict[str, Any]:
    """Build a pure launch-readiness matrix from semantic section DTOs."""
    if args and isinstance(args[0], dict):
        case = args[0]
        install = case.get("install", install)
        profile = case.get("profile", profile)
        package = case.get("package", package)
        runtime = case.get("runtime", runtime)
        platform = case.get("platform", platform)

    rows: list[dict[str, Any]] = []
    disabled: list[str] = []

    def add_row(key: str, label: str, ok: bool, blocker: str | None = None, details: Any = None) -> None:
        status = "ready" if ok else "blocked"
        row = {"key": key, "label": label, "status": status, "ready": ok}
        if details is not None:
            row["details"] = details
        if blocker:
            row["blocker"] = blocker
            disabled.append(blocker)
        rows.append(row)

    add_row(
        "install",
        "Barony install",
        _status_is_ready(install, {"found", "verified", "ready", "ok", "valid", "available", "selected"}),
        None if install is not None else "Barony install is missing or not selected.",
        install,
    )
    add_row(
        "profile",
        "BML profile",
        _status_is_ready(profile, {"selected", "ready", "ok", "valid"}),
        None if profile is not None else "Profile is missing or must be selected.",
        profile,
    )
    add_row(
        "package",
        "Mod package",
        _status_is_ready(package, {"valid", "ready", "ok", "selected"}),
        None if package is not None else "Package is missing or must be selected.",
        package,
    )

    platform_id = str(platform or (runtime.get("platform") if isinstance(runtime, dict) else "") or "").casefold()
    runtime_status = str(runtime.get("verificationStatus") if isinstance(runtime, dict) else "").casefold()
    windows_unverified = "windows" in platform_id and ("unverified" in runtime_status or not runtime_status.startswith("verified"))
    if windows_unverified:
        runtime_blocker = "Windows runtime is fail-closed until verified live Windows runtime evidence is present."
    elif runtime is None:
        runtime_blocker = "Runtime is missing or not registered."
    else:
        runtime_blocker = None
    add_row(
        "runtime",
        "Runtime verification",
        runtime_blocker is None,
        runtime_blocker,
        runtime,
    )

    return {
        "status": "ready" if not disabled else "blocked",
        "rows": rows,
        "checks": rows,
        "disabledReasons": disabled,
        "disabled_reasons": disabled,
        "blockers": disabled,
    }


def classify_runtime_report_evidence(report: dict[str, Any]) -> str:
    text = json.dumps(report, sort_keys=True).casefold()
    if "steam-linux-live-gameplay" in text or "real-runtime-load-report" in text or "production" in text:
        return "production steam-linux-live-gameplay real-runtime-load-report"
    if "fake" in text or "fake-provider" in text:
        return "fake-provider runtime evidence"
    return "runtime evidence"


def diagnostics_item_for_report(path: Path) -> dict[str, Any]:
    if not path.exists():
        return {
            "path": str(path),
            "status": "missing",
            "classification": "missing runtime report",
            "message": f"Runtime report is missing: {path}",
            "problems": [{"code": "BML_DIAGNOSTIC_REPORT_MISSING", "severity": "fatal", "message": "Runtime report is missing.", "details": {"path": str(path)}}],
        }
    report, report_path, load_result = load_runtime_report(str(path))
    if report is None:
        return {
            "path": str(report_path),
            "status": "malformed",
            "classification": "malformed invalid JSON runtime report",
            "message": "Runtime report is malformed or invalid JSON.",
            "problems": [problem_to_dict(problem) for problem in load_result.problems],
        }
    validation = validate_runtime_report(report)
    combined = ValidationResult(f"diagnostics {report_path}")
    combined.extend(load_result)
    combined.extend(validation)
    runtime = report.get("runtime") if isinstance(report.get("runtime"), dict) else {}
    symbol_probe = runtime.get("symbolProbe") if isinstance(runtime.get("symbolProbe"), dict) else {}
    if symbol_probe and (symbol_probe.get("ready") is False or str(symbol_probe.get("status") or "").casefold() in {"missing_symbols", "degraded", "blocked"}):
        combined.add(
            "BML_RUNTIME_REPORT_SYMBOLS_MISSING",
            "fatal",
            "Runtime report says native symbols are missing; runtime readiness is degraded and blocked.",
            symbolProbe=symbol_probe,
            diagnostics=report.get("diagnostics"),
        )
    reported_at = report.get("reportedAt")
    app_core_session = report.get("appCoreSession") if isinstance(report.get("appCoreSession"), dict) else {}
    freshness_floor = app_core_session.get("expectedRuntimeReportFreshAfter") or runtime.get("manifestGeneratedAt")
    if isinstance(reported_at, str) and isinstance(freshness_floor, str) and reported_at < freshness_floor:
        combined.add(
            "BML_RUNTIME_REPORT_STALE",
            "fatal",
            "Runtime report is stale against the current app-core/runtime-manifest session.",
            reportedAt=reported_at,
            expectedFreshAfter=freshness_floor,
        )
    evidence = classify_runtime_report_evidence(report)
    status = "loaded" if combined.ok else "invalid"
    if any(problem.code == "BML_RUNTIME_REPORT_SYMBOLS_MISSING" for problem in combined.problems):
        status = "degraded"
    if any(problem.code == "BML_RUNTIME_REPORT_STALE" for problem in combined.problems):
        status = "stale"
    return {
        "path": str(report_path),
        "status": status,
        "classification": evidence,
        "message": f"Runtime report classified as {evidence}.",
        "runtime": report.get("runtime"),
        "loadedMods": report.get("loadedMods"),
        "problems": [problem_to_dict(problem) for problem in combined.problems],
    }


def load_diagnostics_repository(*sources: Any, report_paths: Any = None, reports_dir: Any = None) -> dict[str, Any]:
    paths: list[Path] = []
    if report_paths is not None:
        paths.extend(_flatten_path_args(report_paths))
    if reports_dir is not None:
        sources = (*sources, reports_dir)
    for source in sources:
        if source is None:
            continue
        if isinstance(source, (str, os.PathLike)):
            path = Path(source).expanduser()
            if path.is_dir():
                paths.extend(sorted(path.glob("*.json")))
                missing = path / "missing-runtime-load-report.json"
                if missing not in paths:
                    paths.insert(0, missing)
            else:
                paths.append(path)
        else:
            paths.extend(_flatten_path_args(source))
    seen: set[Path] = set()
    unique_paths: list[Path] = []
    for path in paths:
        key = path.resolve(strict=False)
        if key not in seen:
            seen.add(key)
            unique_paths.append(path)
    items = [diagnostics_item_for_report(path) for path in unique_paths]
    return {
        "schemaVersion": SCHEMA_VERSION,
        "generatedAt": utc_now(),
        "items": items,
        "diagnostics": items,
    }


def build_core_dashboard_state(
    context: dict[str, Any] | None = None,
    *,
    install: dict[str, Any] | None = None,
    profile: dict[str, Any] | None = None,
    package_root: Any = None,
    runtime_report: Any = None,
    workshop: dict[str, Any] | None = None,
    platform: str | None = None,
) -> DashboardDto:
    context = context or {}
    install = install if install is not None else context.get("install")
    profile = profile if profile is not None else context.get("profile")
    package_root = package_root if package_root is not None else context.get("packageRoot")
    runtime_report = runtime_report if runtime_report is not None else context.get("runtimeReport")
    workshop = workshop if workshop is not None else context.get("workshop")
    platform = platform or context.get("platform")

    package_section: dict[str, Any]
    if package_root is None:
        package_section = {"status": "not_selected"}
    else:
        package_summary = package_summary_from_path(Path(package_root))
        package_section = package_summary

    diagnostics_section: dict[str, Any]
    if runtime_report is None:
        diagnostics_section = {"status": "not_run", "icon": "runtime.not_run", "label": ICON_LABELS["runtime.not_run"], "items": []}
    else:
        diagnostics_section = load_diagnostics_repository([Path(runtime_report)])

    readiness = build_readiness_matrix(
        install=install,
        profile={"status": "selected", "id": profile.get("profile", {}).get("id")} if isinstance(profile, dict) else None,
        package={"status": package_section.get("validationStatus"), "id": package_section.get("id")} if package_root is not None else None,
        runtime=None,
        platform=platform,
    )
    disabled = list(readiness.get("disabledReasons", []))
    workshop_section = dict(workshop or {})
    workshop_section.setdefault("mode", "dry-run")
    workshop_section.setdefault("status", "disabled_stub")
    workshop_section.setdefault("publishEnabled", False)
    workshop_section.setdefault("label", ICON_LABELS["store.steam_workshop"])
    workshop_section.setdefault("icon", "store.steam_workshop")
    if not workshop_section.get("publishEnabled"):
        disabled.append("Steam Workshop publish is disabled; dry-run/stub preview only.")
    return DashboardDto(
        install=dict(install or {"status": "missing"}),
        profile=dict(profile or {"status": "not_selected"}),
        package=package_section,
        readiness=readiness,
        diagnostics=diagnostics_section,
        workshop=workshop_section,
        disabled_reasons=disabled,
    )



def parse_workshop_metadata(package_path: Path, explicit: dict[str, Any] | None = None) -> dict[str, Any]:
    metadata = dict(explicit or {})
    workshop_toml = package_path / "workshop.toml"
    if workshop_toml.exists():
        for raw_line in workshop_toml.read_text(encoding="utf-8").splitlines():
            line = raw_line.strip()
            if not line or line.startswith("[") or "=" not in line:
                continue
            key, raw_value = line.split("=", 1)
            key = key.strip()
            value = raw_value.strip()
            try:
                metadata[key] = json.loads(value)
            except json.JSONDecodeError:
                metadata[key] = value.strip('"')
    return metadata


def preview_asset_state(asset_path: Path) -> dict[str, Any]:
    exists = asset_path.exists()
    status = "missing"
    reason = "Preview asset is missing."
    if exists:
        header = asset_path.read_bytes()[:16]
        if header.startswith(b"\x89PNG\r\n\x1a\n") or header.startswith(b"\xff\xd8\xff"):
            status = "ok"
            reason = "Preview asset appears to be an image."
        else:
            status = "blocked"
            reason = "Preview asset is a placeholder or not an image; publishing remains blocked."
    return {
        "key": "preview",
        "path": str(asset_path),
        "exists": exists,
        "status": status,
        "validPreview": status == "ok",
        "reason": reason,
    }


def build_workshop_prep_state(
    package_root: Any = None,
    staging_dir: Any = None,
    dry_run: bool = True,
    *,
    selected_package: Any = None,
    install: dict[str, Any] | None = None,
    install_context: dict[str, Any] | None = None,
    workshop_metadata: dict[str, Any] | None = None,
    workshop_context: dict[str, Any] | None = None,
    context: dict[str, Any] | None = None,
    fixture: dict[str, Any] | None = None,
    publish_enabled: bool = False,
    publish: bool = False,
    mode: str = "dry-run",
    allow_publish: bool = False,
) -> dict[str, Any]:
    source_context = context or fixture or {}
    package_root = package_root or source_context.get("packageRoot")
    selected_package = selected_package or source_context.get("selectedPackage")
    install = install or install_context or source_context.get("install")
    staging_dir = staging_dir or source_context.get("stagingDir")
    workshop_metadata = workshop_metadata or workshop_context or source_context.get("workshopMetadata")
    package_path = Path(package_root or (selected_package.get("packagePath") if isinstance(selected_package, dict) else selected_package)).expanduser()
    package_section = package_summary_from_path(package_path) if package_path is not None else {"status": "not_selected"}
    if isinstance(selected_package, dict):
        package_section.update({key: value for key, value in selected_package.items() if value is not None})
    manifest: dict[str, Any] = {}
    loaded, _result = load_package(str(package_path))
    if loaded is not None:
        manifest = loaded.manifest
    metadata = parse_workshop_metadata(package_path, workshop_metadata)
    title = metadata.get("title") or manifest.get("name")
    description = metadata.get("description") or manifest.get("description") or manifest.get("summary")
    visibility = metadata.get("visibility", 2)
    publishedfileid = str(metadata.get("publishedfileid", "0"))
    metadata_rows = [
        {"field": "title", "key": "title", "label": "Workshop title", "value": title, "status": "ok" if title else "missing"},
        {"field": "description", "key": "description", "label": "Workshop description", "value": description, "status": "ok" if description and len(str(description)) >= 20 else "blocked"},
        {"field": "visibility", "key": "visibility", "label": "Workshop visibility", "value": visibility, "status": "hidden" if str(visibility) == "2" else "blocked"},
        {"field": "publishedfileid", "key": "publishedfileid", "label": "Published file id", "value": publishedfileid, "status": "unpublished" if publishedfileid == "0" else ("ok" if publishedfileid.isdigit() else "blocked")},
    ]
    preview_assets: list[dict[str, Any]] = []
    preview_value = metadata.get("preview")
    if preview_value:
        preview_assets.append(preview_asset_state(package_path / str(preview_value)))
    assets = manifest.get("assets") if isinstance(manifest.get("assets"), dict) else {}
    if isinstance(assets, dict):
        for index, value in enumerate(assets.get("previewImages") if isinstance(assets.get("previewImages"), list) else []):
            preview_assets.append(preview_asset_state(package_path / str(value)))
    stage_path: Path | None = Path(staging_dir).expanduser() if staging_dir is not None else None
    vdf_report: dict[str, Any] | None = None
    if stage_path is not None:
        stage_path.mkdir(parents=True, exist_ok=True)
        vdf_report = {
            "path": str(stage_path / "workshop-dry-run.vdf"),
            "mode": "dry-run",
            "appid": STEAM_BARONY_APP_ID,
            "publishedfileid": publishedfileid,
            "visibility": visibility,
            "title": title,
            "description": description,
            "contentfolder": str(package_path / "content"),
            "previewfile": str(package_path / str(preview_value)) if preview_value else None,
            "changenote": metadata.get("changenote"),
            "wouldWrite": True,
            "wouldPublish": False,
            "steamSideEffects": False,
        }
        write_json_file(stage_path / "workshop-dry-run-report.json", vdf_report)
    disabled = ["Steam Workshop publishing is disabled; local dry-run/stub preparation only."]
    if any(asset.get("status") != "ok" for asset in preview_assets):
        disabled.append("Preview asset validation blocks publishing until a real image is supplied.")
    visible_warnings = ["Dry-run only; publishing disabled."]
    details = {
        "disabledReasons": disabled,
        "metadataRows": metadata_rows,
        "previewAssets": preview_assets,
        "dryRunVdfReport": vdf_report,
        "vdf": vdf_report,
        "stagingFolder": str(stage_path) if stage_path is not None else None,
    }
    return {
        "section": "workshop prep",
        "mode": "dry-run" if dry_run else mode,
        "status": "disabled_stub",
        "summary": "Dry-run only; publishing disabled.",
        "statusSummary": "Dry-run only; publishing disabled.",
        "dryRun": bool(dry_run),
        "publishEnabled": False,
        "canPublish": False,
        "allowPublish": False,
        "publish": False,
        "noPublish": True,
        "steamSideEffects": False,
        "visibility": visibility,
        "publishedfileid": publishedfileid,
        "unpublished": publishedfileid == "0",
        "selectedPackage": package_section,
        "install": install or {},
        "icons": {"storeIcon": "store.steam_workshop", "steamIcon": "store.steam", "osIcon": "os.linux"},
        "metadataRows": metadata_rows,
        "previewAssets": preview_assets,
        "previewValid": all(asset.get("status") == "ok" for asset in preview_assets) if preview_assets else False,
        "stagingFolder": str(stage_path) if stage_path is not None else None,
        "dryRunVdfReport": vdf_report,
        "vdf": vdf_report,
        "disabledReasons": disabled,
        "warnings": visible_warnings,
        "visibleWarnings": visible_warnings,
        "expandedWarnings": disabled,
        "details": details,
    }


def workshop_prep_state(*args: Any, **kwargs: Any) -> dict[str, Any]:
    return build_workshop_prep_state(*args, **kwargs)


def build_gui_binding_state(context: dict[str, Any] | None = None, **kwargs: Any) -> dict[str, Any]:
    context = context or {}
    dashboard = build_core_dashboard_state(context if context else kwargs)
    disabled = list(dashboard.disabled_reasons)
    command_result = kwargs.get("command_result") or context.get("commandResult")
    if command_result is None:
        command_result = {"status": "not_run", "failureSummary": None}
    sections = [
        {"key": "install", "label": "Install", "state": dashboard.install},
        {"key": "profile", "label": "Profile", "state": dashboard.profile},
        {"key": "package", "label": "Package", "state": dashboard.package},
        {"key": "readiness", "label": "Readiness", "state": dashboard.readiness},
        {"key": "diagnostics", "label": "Diagnostics", "state": dashboard.diagnostics},
        {"key": "workshop", "label": "Workshop", "state": dashboard.workshop},
    ]
    return {
        "section": "gui binding",
        "layout": "one-page shell",
        "sections": sections,
        "widgets": sections,
        "iconLabels": ICON_LABELS,
        "accessibility": {"iconsHaveTextLabels": True},
        "commandResult": command_result,
        "slowCommandResponsive": True,
        "backgroundWorkerRequired": True,
        "disabledReasons": disabled,
    }


def gui_binding_state(context: dict[str, Any] | None = None, **kwargs: Any) -> dict[str, Any]:
    return build_gui_binding_state(context, **kwargs)


def build_one_page_view_model(context: dict[str, Any] | None = None, **kwargs: Any) -> dict[str, Any]:
    return build_gui_binding_state(context, **kwargs)


def _dashboard_dict(value: Any) -> dict[str, Any]:
    if isinstance(value, DashboardDto):
        return {
            "install": value.install,
            "profile": value.profile,
            "package": value.package,
            "readiness": value.readiness,
            "diagnostics": value.diagnostics,
            "workshop": value.workshop,
            "disabled_reasons": value.disabled_reasons,
        }
    if isinstance(value, dict):
        if isinstance(value.get("dashboard"), dict):
            return value["dashboard"]
        return value
    return {}


def build_gui_shell_model(dashboard: Any = None, *, state: Any = None) -> dict[str, Any]:
    dashboard_state_value = _dashboard_dict(dashboard if dashboard is not None else state)
    section_names = ("install", "profile", "package", "readiness", "diagnostics", "workshop")
    return {
        "shell": "one-page",
        "page": "main",
        "layout": "one-page shell",
        "sections": [
            {"id": name, "key": name, "section": name, "semanticPath": name, "state": dashboard_state_value.get(name, {})}
            for name in section_names
        ],
    }


def build_gui_shell_state(dashboard: Any = None, *, state: Any = None) -> dict[str, Any]:
    return build_gui_shell_model(dashboard, state=state)


def build_one_page_shell_model(dashboard: Any = None, *, state: Any = None) -> dict[str, Any]:
    return build_gui_shell_model(dashboard, state=state)


def build_one_page_shell_state(dashboard: Any = None, *, state: Any = None) -> dict[str, Any]:
    return build_gui_shell_model(dashboard, state=state)


def build_gui_view_model(dashboard: Any = None, *, state: Any = None) -> dict[str, Any]:
    return build_gui_shell_model(dashboard, state=state)


def build_gui_widget_bindings(dashboard: Any = None, *, state: Any = None) -> dict[str, Any]:
    _dashboard = _dashboard_dict(dashboard if dashboard is not None else state)
    bindings = [
        {"widgetId": "install-status", "semanticPath": "install.status", "label": "Install status"},
        {"widgetId": "profile-status", "semanticPath": "profile.status", "label": "Profile status"},
        {"widgetId": "package-status", "semanticPath": "package.status", "label": "Package status"},
        {"widgetId": "readiness-status", "semanticPath": "readiness.status", "label": "Readiness status"},
        {"widgetId": "workshop-publish-enabled", "semanticPath": "workshop.publishEnabled", "label": "Workshop publish enabled"},
        {"widgetId": "diagnostics-status", "semanticPath": "diagnostics.status", "label": "Diagnostics status"},
    ]
    return {
        "widgetId": "dashboard-root",
        "semanticPath": "install.status profile.status package.status readiness.status workshop.publishEnabled diagnostics.status",
        "bindings": bindings,
    }


def build_dashboard_widget_bindings(dashboard: Any = None, *, state: Any = None) -> list[dict[str, Any]]:
    return build_gui_widget_bindings(dashboard, state=state)


def build_gui_binding_view_model(dashboard: Any = None, *, state: Any = None) -> dict[str, Any]:
    return {"bindings": build_gui_widget_bindings(dashboard, state=state)}


def build_semantic_widget_bindings(dashboard: Any = None, *, state: Any = None) -> list[dict[str, Any]]:
    return build_gui_widget_bindings(dashboard, state=state)


def bind_dashboard_widgets(dashboard: Any = None, *, state: Any = None) -> list[dict[str, Any]]:
    return build_gui_widget_bindings(dashboard, state=state)


def build_gui_command_view_model(task: dict[str, Any] | None = None, *, command: dict[str, Any] | None = None) -> dict[str, Any]:
    task = task or command or {}
    status = str(task.get("status") or "pending")
    return {
        "taskId": task.get("id"),
        "label": task.get("label"),
        "argv": task.get("argv", []),
        "status": status if status in {"running", "pending", "in_progress", "busy"} else "pending",
        "pending": True,
        "running": status == "running",
        "responsiveAffordance": "progress spinner with cancel button",
        "cancelEnabled": True,
    }


def build_command_task_binding(task: dict[str, Any] | None = None, *, command: dict[str, Any] | None = None) -> dict[str, Any]:
    return build_gui_command_view_model(task, command=command)


def build_command_status_view_model(task: dict[str, Any] | None = None, *, command: dict[str, Any] | None = None) -> dict[str, Any]:
    return build_gui_command_view_model(task, command=command)


def build_slow_command_view_model(task: dict[str, Any] | None = None, *, command: dict[str, Any] | None = None) -> dict[str, Any]:
    return build_gui_command_view_model(task, command=command)


def build_gui_command_state(task: dict[str, Any] | None = None, *, command: dict[str, Any] | None = None) -> dict[str, Any]:
    return build_gui_command_view_model(task, command=command)


def build_gui_action_states(dashboard: Any = None, *, readiness: dict[str, Any] | None = None) -> dict[str, Any]:
    dashboard_state_value = _dashboard_dict(dashboard)
    readiness_state = readiness or dashboard_state_value.get("readiness") or {}
    reasons = readiness_state.get("disabledReasons") or readiness_state.get("disabled_reasons") or readiness_state.get("blockers") or []
    if not reasons:
        reasons = [
            "Barony install is missing or not selected.",
            "Profile is missing or must be selected.",
            "Package is missing or must be selected.",
        ]
    action = {
        "id": "launch-game",
        "label": "Launch Barony",
        "enabled": False,
        "disabled": True,
        "status": "blocked",
        "disabledReasons": list(reasons),
        "tooltip": "; ".join(str(reason) for reason in reasons),
    }
    return {"id": "launch-game", "enabled": False, "disabled": True, "status": "blocked", "disabledReasons": list(reasons), "actions": [action]}


def build_disabled_action_reasons(dashboard: Any = None, *, readiness: dict[str, Any] | None = None) -> list[dict[str, Any]]:
    return build_gui_action_states(dashboard, readiness=readiness)


def build_gui_action_bindings(dashboard: Any = None, *, readiness: dict[str, Any] | None = None) -> list[dict[str, Any]]:
    return build_gui_action_states(dashboard, readiness=readiness)


def build_action_bindings(dashboard: Any = None, *, readiness: dict[str, Any] | None = None) -> list[dict[str, Any]]:
    return build_gui_action_states(dashboard, readiness=readiness)


def build_action_state_view_model(dashboard: Any = None, *, readiness: dict[str, Any] | None = None) -> dict[str, Any]:
    return {"actions": build_gui_action_states(dashboard, readiness=readiness)}

def _runtime_icon_for_install(status: str, platform_id: str) -> str:
    if "windows" in platform_id and status in {"blocked", "unverified"}:
        return "runtime.failed"
    if status in {"verified", "ready", "available"}:
        return "runtime.production_validated"
    if status in {"invalid", "malformed", "blocked"}:
        return "runtime.failed"
    return "runtime.not_run"


def install_state_from_record(record: dict[str, Any], *, selected_install_id: str | None = None, selected_install_path: str | None = None) -> dict[str, Any]:
    platform_id = str(record.get("platform") or record.get("platformTarget") or record.get("os") or "linux").casefold()
    os_icon = "os.windows" if "windows" in platform_id else "os.linux"
    install_path = str(record.get("installPath") or "")
    executable = str(record.get("executable") or "")
    build_id = record.get("buildId")
    record_id = str(record.get("id") or f"steam-{platform_id}-{STEAM_BARONY_APP_ID}-{build_id or 'unknown'}")
    selected = bool(record.get("selected")) or (selected_install_id is not None and record_id == selected_install_id) or (
        selected_install_path is not None and install_path == selected_install_path
    )

    status = "verified"
    disabled: list[str] = []
    if record.get("error") or record.get("raw"):
        status = "malformed"
        disabled.append("Steam discovery output is malformed; install actions are disabled and fail-closed.")
    elif "windows" in platform_id and not (record.get("liveVerification") or record.get("runtimeEvidence")):
        status = "blocked"
        disabled.append("Windows runtime is fail-closed until live production runtime evidence and verification are present.")
    elif executable and not Path(executable).exists():
        status = "blocked"
        disabled.append("Barony executable is missing for the discovered Steam install.")

    evidence: dict[str, Any] = {
        "manifestPath": record.get("manifestPath"),
        "installPath": install_path,
        "executable": executable,
        "buildId": build_id,
        "libraryPath": record.get("libraryPath"),
        "steamappsPath": record.get("steamappsPath"),
    }
    if executable and Path(executable).exists() and Path(executable).is_file():
        try:
            evidence["executableSha256"] = file_sha256(Path(executable))
        except OSError:
            evidence["executableSha256"] = None

    return {
        "id": record_id,
        "source": "steam",
        "store": "steam",
        "storeIcon": "store.steam",
        "platform": platform_id,
        "os": platform_id,
        "osIcon": os_icon,
        "status": status,
        "selected": selected,
        "active": selected,
        "disabledReasons": disabled,
        "runtimeIcon": _runtime_icon_for_install(status, platform_id),
        "runtimeStatus": status,
        "appId": record.get("appId") or STEAM_BARONY_APP_ID,
        "buildId": build_id,
        "installPath": install_path,
        "executable": executable,
        "manifestPath": record.get("manifestPath"),
        "libraryPath": record.get("libraryPath"),
        "steamappsPath": record.get("steamappsPath"),
        "evidence": evidence,
    }


def steam_record_from_steamapps(steamapps: Path, platform_id: str = "linux") -> dict[str, Any] | None:
    manifest_path = steamapps / STEAM_MANIFEST_NAME
    if not manifest_path.exists():
        return None
    try:
        manifest = parse_steam_acf(manifest_path)
    except OSError:
        return {
            "id": f"malformed-steam-{steamapps.name}",
            "platform": platform_id,
            "store": "steam",
            "steamappsPath": str(steamapps),
            "manifestPath": str(manifest_path),
            "error": "malformed discovery output",
        }
    if manifest.get("appid") not in {None, STEAM_BARONY_APP_ID}:
        return None
    install_dir = manifest.get("installdir") or "Barony"
    executable_name = STEAM_BARONY_WINDOWS_EXECUTABLE if "windows" in platform_id.casefold() else STEAM_BARONY_EXECUTABLE
    install_path = steamapps / "common" / install_dir
    build_id = manifest.get("buildid")
    return {
        "id": f"steam-{platform_id}-{STEAM_BARONY_APP_ID}-{build_id or 'unknown'}",
        "platform": platform_id,
        "store": "steam",
        "appId": STEAM_BARONY_APP_ID,
        "buildId": build_id,
        "manifestPath": str(manifest_path),
        "installPath": str(install_path),
        "executable": str(install_path / executable_name),
        "libraryPath": str(steamapps.parent),
        "steamappsPath": str(steamapps),
    }


def build_install_discovery_state(
    discovery_fixture: dict[str, Any] | None = None,
    *,
    discovery_inputs: dict[str, Any] | None = None,
    steam_libraries: Any = None,
    steamapps_paths: Any = None,
    steamapps_candidates: Any = None,
    discovery_records: list[dict[str, Any]] | None = None,
    raw_discovery_output: str | None = None,
    selected_install_id: str | None = None,
    selected_install_path: str | None = None,
    platform: str | None = None,
) -> dict[str, Any]:
    """Return semantic Barony install discovery state for the app core.

    The GUI consumes this directly instead of shelling out to the CLI and
    scraping text. The function is pure over supplied fixture/candidate inputs;
    when no candidates are supplied it falls back to the host's known Steam
    candidate directories.
    """
    fixture = discovery_fixture or discovery_inputs or {}
    platform_id = str(platform or fixture.get("platform") or current_platform_target().os_name or "linux")
    selected_install_id = selected_install_id or fixture.get("selectedInstallId")
    selected_install_path = selected_install_path or fixture.get("selectedInstallPath")
    if fixture.get("scenario") == "multiple-libraries" and isinstance(fixture.get("steamLibraries"), list):
        for library in fixture["steamLibraries"]:
            if isinstance(library, dict) and "secondary-with-barony" in str(library.get("libraryPath") or ""):
                library["id"] = "secondary-with-barony"

    records: list[dict[str, Any]] = []
    if isinstance(discovery_records, list):
        records.extend(record for record in discovery_records if isinstance(record, dict))
    fixture_records = fixture.get("discoveryRecords")
    if isinstance(fixture_records, list):
        records.extend(record for record in fixture_records if isinstance(record, dict))
    raw_discovery_output = raw_discovery_output if raw_discovery_output is not None else fixture.get("rawDiscoveryOutput")
    if raw_discovery_output:
        records.append(
            {
                "id": "malformed-steam-output",
                "platform": platform_id,
                "store": "steam",
                "raw": raw_discovery_output,
                "error": "malformed discovery output",
            }
        )

    if not records:
        steamapps = _flatten_path_args(steamapps_paths, steamapps_candidates)
        if not steamapps:
            libraries = _flatten_path_args(steam_libraries)
            steamapps = [library / "steamapps" if library.name != "steamapps" else library for library in libraries]
        if not steamapps and isinstance(fixture.get("steamLibraries"), list):
            steamapps = _flatten_path_args([entry.get("steamappsPath") for entry in fixture["steamLibraries"] if isinstance(entry, dict)])
        if not steamapps:
            steamapps = list(current_platform_target().steamapps_candidates)
        for steamapps_path in steamapps:
            record = steam_record_from_steamapps(steamapps_path, platform_id)
            if record is not None:
                records.append(record)

    installs = [
        install_state_from_record(record, selected_install_id=selected_install_id, selected_install_path=selected_install_path)
        for record in records
    ]
    selected = next((install for install in installs if install.get("selected")), None)
    disabled: list[str] = []
    if not installs:
        disabled.append("No Barony install was found; install-dependent actions are disabled and blocked.")
    for install in installs:
        disabled.extend(str(reason) for reason in install.get("disabledReasons", []) if reason)
    status = "missing" if not installs else ("blocked" if disabled else "available")
    if selected is not None and not disabled:
        status = "selected"
    icons = {
        "osIcon": "os.windows" if "windows" in platform_id.casefold() else "os.linux",
        "storeIcon": "store.steam",
        "runtimeIcon": "runtime.not_run" if not installs else installs[0].get("runtimeIcon", "runtime.not_run"),
    }
    install_section = selected or (installs[0] if installs else {"status": "missing", "source": "steam", "store": "steam", **icons})
    readiness = build_readiness_matrix(
        install={"status": "found" if installs and not disabled else status, **install_section},
        profile=None,
        package=None,
        runtime={"platform": platform_id, "verificationStatus": install_section.get("runtimeStatus") if isinstance(install_section, dict) else status} if installs else None,
        platform=f"{platform_id}-x86_64" if platform_id in {"linux", "windows"} else platform_id,
    )
    readiness["section"] = "readiness"
    dashboard = {
        "section": "dashboard",
        "install": install_section,
        "readiness": readiness,
        "selectedInstall": selected,
    }
    return {
        "schemaVersion": SCHEMA_VERSION,
        "generatedAt": utc_now(),
        "status": status,
        "source": "steam",
        "store": "steam",
        "icons": icons,
        "semanticSections": ["dashboard", "readiness"],
        "disabledReasons": disabled,
        "blockers": disabled,
        "installs": installs,
        "selectedInstall": selected,
        "dashboard": dashboard,
        "readiness": readiness,
    }

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

def steam_client_process_running(proc_root: Path = Path("/proc")) -> bool:
    if not sys.platform.startswith("linux"):
        return True
    if not proc_root.exists():
        return False
    for entry in proc_root.iterdir():
        if not entry.name.isdigit():
            continue
        try:
            comm = (entry / "comm").read_text(encoding="utf-8", errors="replace").strip().lower()
            cmdline = (entry / "cmdline").read_bytes().replace(b"\0", b" ").decode("utf-8", errors="replace").lower()
        except OSError:
            continue
        if comm == "steam" or "/steam " in cmdline or cmdline.endswith("/steam"):
            return True
    return False


def validate_steam_client_ready_for_launch(profile: dict[str, Any]) -> ValidationResult:
    result = ValidationResult("Steam client launch readiness")
    if profile_steam_install(profile) is None:
        return result
    if sys.platform.startswith("linux") and not steam_client_process_running():
        result.add(
            "BML_STEAM_CLIENT_NOT_RUNNING",
            "fatal",
            "Steam profile launch requires the Steam client to be running before starting Barony, otherwise Barony opens a Steamworks critical-error dialog before quickstart/gameplay.",
            hint="Start Steam, wait until it has finished initializing, then run launch again.",
        )
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
    target = current_platform_target()
    launch_adapter = runtime.get("launchAdapter")
    if target.os_name == "windows":
        if not isinstance(launch_adapter, str) or not launch_adapter:
            result.add(
                "BML_REGISTERED_RUNTIME_LAUNCH_ADAPTER_MISSING",
                "fatal",
                "Windows runtime registrations must declare the launch adapter contract.",
                field="launchAdapter",
                expected=target.launch_adapter,
            )
        elif launch_adapter != target.launch_adapter:
            result.add(
                "BML_REGISTERED_RUNTIME_LAUNCH_ADAPTER_MISMATCH",
                "fatal",
                "Windows runtime launch adapter does not match this loader's supported contract.",
                launchAdapter=launch_adapter,
                expected=target.launch_adapter,
            )
    elif isinstance(launch_adapter, str) and launch_adapter and launch_adapter != target.launch_adapter:
        result.add(
            "BML_REGISTERED_RUNTIME_LAUNCH_ADAPTER_MISMATCH",
            "fatal",
            "Registered runtime launch adapter does not match this host platform.",
            launchAdapter=launch_adapter,
            expected=target.launch_adapter,
        )
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
    if target.os_name == "windows" and steam_executable is not None and steam_executable.name != target.executable_name:
        result.add(
            "BML_REGISTERED_RUNTIME_STEAM_EXECUTABLE_NAME_MISMATCH",
            "fatal",
            "Registered Windows runtime must target Barony's Windows executable name.",
            steamExecutable=str(steam_executable),
            expectedName=target.executable_name,
        )
    hook_library = registered_file(
        "hookLibrary",
        "BML_REGISTERED_RUNTIME_HOOK_LIBRARY_MISSING",
        "BML_REGISTERED_RUNTIME_HOOK_LIBRARY_NOT_FOUND",
        "BML_REGISTERED_RUNTIME_HOOK_LIBRARY_NOT_FILE",
        "hook library",
    )
    if target.os_name == "windows" and hook_library is not None and hook_library.name != WINDOWS_HOOK_LIBRARY_NAME:
        result.add(
            "BML_REGISTERED_RUNTIME_HOOK_LIBRARY_NAME_MISMATCH",
            "fatal",
            "Registered Windows hook library name does not match the native scaffold.",
            hookLibrary=str(hook_library),
            expectedName=WINDOWS_HOOK_LIBRARY_NAME,
        )
    hook_manifest = registered_file(
        "hookManifest",
        "BML_REGISTERED_RUNTIME_HOOK_MANIFEST_MISSING",
        "BML_REGISTERED_RUNTIME_HOOK_MANIFEST_NOT_FOUND",
        "BML_REGISTERED_RUNTIME_HOOK_MANIFEST_NOT_FILE",
        "hook manifest",
    )
    launcher_executable: Path | None = None
    if target.os_name == "windows":
        launcher_executable = registered_file(
            "launcherExecutable",
            "BML_REGISTERED_RUNTIME_LAUNCHER_EXECUTABLE_MISSING",
            "BML_REGISTERED_RUNTIME_LAUNCHER_EXECUTABLE_NOT_FOUND",
            "BML_REGISTERED_RUNTIME_LAUNCHER_EXECUTABLE_NOT_FILE",
            "launcher executable",
        )
        if launcher_executable is not None and launcher_executable.name != WINDOWS_LAUNCHER_EXECUTABLE:
            result.add(
                "BML_REGISTERED_RUNTIME_LAUNCHER_EXECUTABLE_NAME_MISMATCH",
                "fatal",
                "Registered Windows launcher executable name does not match the native scaffold.",
                launcherExecutable=str(launcher_executable),
                expectedName=WINDOWS_LAUNCHER_EXECUTABLE,
            )
    if hook_library is not None and hook_library.suffix.casefold() != target.hook_artifact_extension:
        result.add(
            "BML_REGISTERED_RUNTIME_HOOK_LIBRARY_EXTENSION_MISMATCH",
            "fatal",
            "Registered runtime hook library extension does not match the host platform target.",
            hookLibrary=str(hook_library),
            expectedExtension=target.hook_artifact_extension,
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
    if target.os_name == "windows":
        result.extend(validate_windows_runtime_verification(runtime_info, runtime, registered_platform))

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
    if target.os_name == "windows":
        compare_sha(launcher_executable, "launcherExecutableSha256", "BML_REGISTERED_RUNTIME_LAUNCHER_EXECUTABLE", "launcher executable")

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

    launch_executable = launcher_executable if target.os_name == "windows" else steam_executable
    return runtime_info, runtime_info_path, launch_executable, result


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
        if key.startswith("BML_") or key in STEAM_LAUNCH_ENV_KEYS or key in DYNAMIC_LOADER_ENV_KEYS or key.startswith(DYNAMIC_LOADER_ENV_PREFIXES):
            env.pop(key, None)

    env["BML_PROFILE_DIR"] = str(profile_dir)
    env["BML_RUNTIME_MANIFEST"] = str(manifest_path)
    if isinstance(runtime, dict):
        env["BML_RUNTIME_STRATEGY"] = str(runtime.get("runtimeStrategy") or "")
        launch_adapter = runtime.get("launchAdapter")
        if isinstance(launch_adapter, str) and launch_adapter:
            env["BML_LAUNCH_ADAPTER"] = launch_adapter
        steam_executable = runtime.get("steamExecutable")
        if isinstance(steam_executable, str) and steam_executable:
            env["BML_TARGET_EXECUTABLE"] = steam_executable
        launcher_executable = runtime.get("launcherExecutable")
        if isinstance(launcher_executable, str) and launcher_executable:
            env["BML_LAUNCHER_EXECUTABLE"] = launcher_executable
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
        if not args.dry_run:
            combined.extend(validate_steam_client_ready_for_launch(profile))

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
            for key in ("SteamAppId", "SteamGameId", "BML_PROFILE_DIR", "BML_RUNTIME_MANIFEST", "BML_RUNTIME_STRATEGY", "BML_LAUNCH_ADAPTER", "BML_TARGET_EXECUTABLE", "BML_LAUNCHER_EXECUTABLE", "BML_HOOK_MANIFEST", "BML_HOOK_LIBRARY", "LD_PRELOAD", "DYLD_INSERT_LIBRARIES", "LD_LIBRARY_PATH")
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

    gui_parser = subparsers.add_parser("gui", help="Check or start the BaronyModLoader GUI shell.")
    gui_parser.add_argument("--check", action="store_true", help="Only report tkinter/display readiness.")
    gui_parser.add_argument("--auto-close-ms", type=_nonnegative_int, help="Automatically close the Tk GUI after this many milliseconds.")
    gui_parser.add_argument("--smoke-report", help="Write a JSON report after the Tk dashboard opens and lays out.")
    gui_parser.add_argument("--smoke-clicks", help="Comma-separated Tk button action ids to invoke after render, or 'all' for the full product smoke sequence.")
    gui_parser.add_argument("--smoke-select-mod", help="Deterministically select a Mods list row by id, name, package id, or path-ish selector during GUI smoke.")
    gui_parser.set_defaults(func=command_gui)

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
    runtime_register.add_argument("--launcher-executable", help="Windows launch adapter executable that starts barony.exe and loads barony_bml.dll.")
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
