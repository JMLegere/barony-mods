#!/usr/bin/env python3
"""Run the current BML + Stash smoke scenario against a Steam Barony install.

This scenario is intentionally black-box from the BML app boundary: it packages
Stash, installs it into a temporary package store, registers a BML-enabled Barony
runtime, creates a Steam-backed profile, launches the game for a short window,
and verifies the runtime report plus Stash diagnostics written by the native
runtime.

Default runtime paths match the local development build used by this repository:
    /tmp/barony-bml-build/barony
    /tmp/barony-bml-build/runtime-info.json
"""

from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import time
from typing import Any


REPO_ROOT = Path(__file__).resolve().parents[3]
BML_APP = REPO_ROOT / "framework" / "BaronyModLoader" / "app" / "barony_mod_loader.py"
STASH_PACKAGE_SOURCE = REPO_ROOT / "mods" / "stash"
DEFAULT_WORKSPACE = Path("/tmp/barony-bml-stash-smoke")
DEFAULT_RUNTIME_EXECUTABLE = Path("/tmp/barony-bml-build/barony")
DEFAULT_RUNTIME_INFO = Path("/tmp/barony-bml-build/runtime-info.json")
DEFAULT_BARONY_ARGS = ["-windowed", "-size=640x480", "-nosound", "-quickstart=barbarian"]


class ScenarioError(RuntimeError):
    pass


def run(args: list[str], *, cwd: Path = REPO_ROOT, env: dict[str, str] | None = None) -> subprocess.CompletedProcess[str]:
    result = subprocess.run(
        [sys.executable, str(BML_APP), *args],
        cwd=str(cwd),
        env=env,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        check=False,
    )
    if result.returncode != 0:
        raise ScenarioError(
            "BML command failed with exit code "
            f"{result.returncode}: {' '.join(args)}\n{result.stdout}"
        )
    return result


def run_json(args: list[str]) -> dict[str, Any]:
    output = run(args).stdout
    try:
        return json.loads(output)
    except json.JSONDecodeError as exc:
        raise ScenarioError(f"Expected JSON from {' '.join(args)} but got:\n{output}") from exc


def read_json(path: Path) -> dict[str, Any]:
    try:
        return json.loads(path.read_text())
    except FileNotFoundError as exc:
        raise ScenarioError(f"Missing expected JSON file: {path}") from exc
    except json.JSONDecodeError as exc:
        raise ScenarioError(f"Invalid JSON in {path}: {exc}") from exc


def read_jsonl(path: Path) -> list[dict[str, Any]]:
    if not path.exists():
        raise ScenarioError(f"Missing expected diagnostics file: {path}")
    events: list[dict[str, Any]] = []
    for line_number, line in enumerate(path.read_text().splitlines(), start=1):
        if not line.strip():
            continue
        try:
            events.append(json.loads(line))
        except json.JSONDecodeError as exc:
            raise ScenarioError(f"Invalid JSONL at {path}:{line_number}: {exc}") from exc
    return events


def prepare_workspace(workspace: Path, keep_workspace: bool) -> tuple[Path, Path, Path, Path]:
    if workspace.exists() and not keep_workspace:
        shutil.rmtree(workspace)
    workspace.mkdir(parents=True, exist_ok=True)
    profile = workspace / "profile"
    store = workspace / "store"
    registry = workspace / "runtime-registry.json"
    package_archive = workspace / "Stash-0.1.0.bmlpkg"
    return profile, store, registry, package_archive


def install_stash(store: Path, package_archive: Path) -> Path:
    run(["package", "validate", str(STASH_PACKAGE_SOURCE)])
    run(["package", "pack", str(STASH_PACKAGE_SOURCE), "--out", str(package_archive)])
    run(["package", "install", str(package_archive), "--store", str(store)])
    installed = store / "jml.stash" / "0.1.0"
    if not (installed / "bml-package.json").exists():
        raise ScenarioError(f"Expected installed Stash package at {installed}")
    return installed


def register_runtime(
    registry: Path,
    runtime_executable: Path,
    runtime_info: Path,
    steam_build_id: str,
) -> str:
    runtime_id = f"steam-371970-{steam_build_id}-stash-smoke"
    run([
        "runtime",
        "register",
        "--registry",
        str(registry),
        "--id",
        runtime_id,
        "--executable",
        str(runtime_executable),
        "--runtime-info",
        str(runtime_info),
        "--steam-app-id",
        "371970",
        "--steam-build-id",
        steam_build_id,
    ])
    return runtime_id


def create_profile(profile: Path, runtime_info: Path) -> None:
    run([
        "profile",
        "create",
        str(profile),
        "--id",
        "stash-smoke",
        "--steam",
        "--runtime-info",
        str(runtime_info),
    ])


def launch_game(
    profile: Path,
    installed_package: Path,
    registry: Path,
    runtime_id: str,
    seconds: int | None,
    barony_args: list[str],
    display: str | None,
    wayland_display: str | None,
) -> int:
    command = [
        sys.executable,
        str(BML_APP),
        "launch",
        str(profile),
        "--package",
        str(installed_package),
        "--registry",
        str(registry),
        "--runtime",
        runtime_id,
        "--",
        *barony_args,
    ]
    env = os.environ.copy()
    if display:
        env["DISPLAY"] = display
    if wayland_display:
        env["WAYLAND_DISPLAY"] = wayland_display
        env.setdefault("XDG_SESSION_TYPE", "wayland")

    proc = subprocess.Popen(
        command,
        cwd=str(REPO_ROOT),
        env=env,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
    )
    if seconds is None:
        return proc.wait()
    time.sleep(seconds)
    if proc.poll() is None:
        proc.terminate()
        try:
            proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            proc.kill()
            proc.wait(timeout=5)
    if proc.stdout:
        tail = "\n".join(proc.stdout.read().splitlines()[-20:])
        if tail:
            print("launcher stdout tail:")
            print(tail)
    return proc.returncode if proc.returncode is not None else 0


def assert_scenario(profile: Path, expect_shop: bool, expect_inventory_save: bool) -> tuple[dict[str, Any], list[dict[str, Any]]]:
    bml_root = profile / "BaronyModLoader"
    report = read_json(bml_root / "reports" / "runtime-load-report.json")
    if report.get("status") != "loaded" or report.get("loaded") is not True:
        raise ScenarioError(f"Runtime did not load cleanly: {json.dumps(report, indent=2)}")

    events = read_jsonl(bml_root / "state" / "stash-diagnostics.jsonl")
    if not any(event.get("event") == "runtime_loaded" for event in events):
        raise ScenarioError("Missing runtime_loaded diagnostic event")
    if not any(
        event.get("event") == "stash_access_point_created" and event.get("kind") == "lobby"
        for event in events
    ):
        raise ScenarioError("Missing lobby Stash placement diagnostic event")
    if expect_shop and not any(
        event.get("event") == "stash_access_point_created" and event.get("kind") == "shop"
        for event in events
    ):
        raise ScenarioError("Missing generated-shop Stash placement diagnostic event")
    if expect_inventory_save and not any(event.get("event") == "stash_inventory_saved" for event in events):
        raise ScenarioError("Missing Stash inventory save diagnostic event")
    return report, events


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Run the BML Stash smoke scenario.")
    parser.add_argument("--workspace", type=Path, default=DEFAULT_WORKSPACE)
    parser.add_argument("--runtime-executable", type=Path, default=DEFAULT_RUNTIME_EXECUTABLE)
    parser.add_argument("--runtime-info", type=Path, default=DEFAULT_RUNTIME_INFO)
    parser.add_argument("--seconds", type=int, default=35, help="How long to let Barony run before terminating it.")
    parser.add_argument("--interactive", action="store_true", help="Leave Barony running until you close it manually.")
    parser.add_argument("--display", default=os.environ.get("DISPLAY") or ":1")
    parser.add_argument("--wayland-display", default=os.environ.get("WAYLAND_DISPLAY") or "wayland-1")
    parser.add_argument("--keep-workspace", action="store_true")
    parser.add_argument("--expect-shop", action="store_true", help="Also require a generated-shop placement diagnostic.")
    parser.add_argument("--expect-inventory-save", action="store_true", help="Also require an inventory-save diagnostic.")
    parser.add_argument(
        "barony_args",
        nargs=argparse.REMAINDER,
        help="Optional Barony args after --. Defaults to a windowed Barbarian quickstart.",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    barony_args = args.barony_args
    if barony_args and barony_args[0] == "--":
        barony_args = barony_args[1:]
    if not barony_args:
        barony_args = DEFAULT_BARONY_ARGS

    if not args.runtime_executable.exists():
        raise ScenarioError(
            f"BML runtime executable not found: {args.runtime_executable}\n"
            "Build the native runtime first or pass --runtime-executable."
        )
    if not args.runtime_info.exists():
        raise ScenarioError(
            f"BML runtime-info not found: {args.runtime_info}\n"
            "Run the native runtime with --bml-runtime-info=<path> first or pass --runtime-info."
        )

    steam = run_json(["steam", "detect"])
    if steam.get("status") != "found":
        raise ScenarioError(f"Steam Barony install was not detected: {json.dumps(steam, indent=2)}")
    steam_info = steam["steam"]
    steam_build_id = str(steam_info["buildId"])

    profile, store, registry, package_archive = prepare_workspace(args.workspace, args.keep_workspace)
    installed_package = install_stash(store, package_archive)
    runtime_id = register_runtime(registry, args.runtime_executable, args.runtime_info, steam_build_id)
    create_profile(profile, args.runtime_info)
    run(["profile", "enable", str(profile), "--package", str(installed_package)])

    print("BML Stash smoke scenario")
    print(f"workspace: {args.workspace}")
    print(f"steam build: {steam_build_id}")
    print(f"runtime: {args.runtime_executable}")
    print(f"package: {installed_package}")
    print(f"barony args: {' '.join(barony_args)}")

    if args.interactive:
        print("interactive mode: close Barony to finish the scenario.")
        print("important: this launches the BML-enabled runtime, not the stock Steam executable.")
        seconds = None
    else:
        seconds = args.seconds

    return_code = launch_game(
        profile,
        installed_package,
        registry,
        runtime_id,
        seconds,
        barony_args,
        args.display,
        args.wayland_display,
    )
    report, events = assert_scenario(profile, args.expect_shop, args.expect_inventory_save)

    placements = [event for event in events if event.get("event") == "stash_access_point_created"]
    saves = [event for event in events if event.get("event") == "stash_inventory_saved"]
    print("scenario result: PASS")
    print(f"process return code: {return_code}")
    print(f"runtime status: {report['status']} ({report['errorCode']})")
    print(f"placements: {json.dumps(placements, indent=2)}")
    print(f"inventory saves: {json.dumps(saves, indent=2)}")
    print(f"diagnostics: {profile / 'BaronyModLoader' / 'state' / 'stash-diagnostics.jsonl'}")
    print(f"runtime log: {profile / 'BaronyModLoader' / 'logs' / 'launcher-runtime.log'}")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except ScenarioError as exc:
        print(f"scenario result: FAIL\n{exc}", file=sys.stderr)
        raise SystemExit(1)
