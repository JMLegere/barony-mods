from __future__ import annotations

import ctypes
import importlib.util
import json
import os
import struct
import sys
import subprocess
import tempfile
from pathlib import Path
import unittest
ROOT = Path(__file__).resolve().parents[1]
HOOK_DLL = ROOT / "build" / "bml_hook_win.dll"
FAKE_PROVIDER_DLL = ROOT / "build" / "bml_fake_provider_win.dll"

POWERSHELL_SCRIPT = ROOT / "tests" / "run_bml_hook_win_selftest.ps1"
INIT_CLASS_PROBE_SCRIPT = ROOT / "tests" / "run_bml_init_class_probe.ps1"
DO_NEW_GAME_PROBE_SCRIPT = ROOT / "tests" / "run_bml_do_new_game_probe.ps1"
PLACEMENT_FAILCLOSED_SCRIPT = ROOT / "tests" / "run_bml_placement_discovery_failclosed.ps1"
PLACEMENT_DISCOVERY_PROBE_SCRIPT = ROOT / "tests" / "run_bml_placement_discovery_probe.ps1"
ASSIGN_ACTIONS_PROBE_SCRIPT = ROOT / "tests" / "run_bml_assign_actions_probe.ps1"
NEW_ENTITY_PROBE_SCRIPT = ROOT / "tests" / "run_bml_new_entity_probe.ps1"
NEW_ITEM_PROBE_SCRIPT = ROOT / "tests" / "run_bml_new_item_probe.ps1"
SET_SPRITE_PROBE_SCRIPT = ROOT / "tests" / "run_bml_set_sprite_probe.ps1"
GET_ITEM_INSTALL_PROBE_SCRIPT = ROOT / "tests" / "run_bml_get_item_probe.ps1"
ADD_ITEM_VOID_PROBE_SCRIPT = ROOT / "tests" / "run_bml_add_item_void_probe.ps1"
GET_CHEST_LIST_PROBE_SCRIPT = ROOT / "tests" / "run_bml_get_chest_list_probe.ps1"
REMOVE_ITEM_VOID_PROBE_SCRIPT = ROOT / "tests" / "run_bml_remove_item_void_probe.ps1"
CLOSE_CHEST_SERVER_PROBE_SCRIPT = ROOT / "tests" / "run_bml_close_chest_server_probe.ps1"
STASH_CORE_BEHAVIOR_PROBE_SCRIPT = ROOT / "tests" / "run_bml_stash_core_behavior_probe.ps1"
INSTALL_PROBE_STEAM_EXE = Path(r"C:\Program Files (x86)\Steam\steamapps\common\Barony\barony.exe")
INSTALL_PROBE_PYTHON = Path(r"C:\Program Files (x86)\GOG Galaxy\python\python.exe")
INSTALL_PROBE_LAUNCHER = ROOT / "build" / "bml_launcher_win.exe"
GET_ITEM_PROBE_PROFILE_ROOT = ROOT.parent.parent / ".tmp" / "windows-get-item-probe-profile"
GET_ITEM_PROBE_REGISTRY = ROOT.parent.parent / ".tmp" / "windows-get-item-probe-registry.json"
ADD_ITEM_VOID_PROBE_PROFILE_ROOT = ROOT.parent.parent / ".tmp" / "windows-add-item-void-probe-profile"
ADD_ITEM_VOID_PROBE_REGISTRY = ROOT.parent.parent / ".tmp" / "windows-add-item-void-probe-registry.json"
GET_CHEST_LIST_PROBE_PROFILE_ROOT = ROOT.parent.parent / ".tmp" / "windows-get-chest-list-probe-profile"
GET_CHEST_LIST_PROBE_REGISTRY = ROOT.parent.parent / ".tmp" / "windows-get-chest-list-probe-registry.json"
REMOVE_ITEM_VOID_PROBE_PROFILE_ROOT = ROOT.parent.parent / ".tmp" / "windows-remove-item-void-probe-profile"
REMOVE_ITEM_VOID_PROBE_REGISTRY = ROOT.parent.parent / ".tmp" / "windows-remove-item-void-probe-registry.json"
CLOSE_CHEST_SERVER_PROBE_PROFILE_ROOT = ROOT.parent.parent / ".tmp" / "windows-close-chest-server-probe-profile"
CLOSE_CHEST_SERVER_PROBE_REGISTRY = ROOT.parent.parent / ".tmp" / "windows-close-chest-server-probe-registry.json"
ASSIGN_ACTIONS_PROBE_PROFILE_ROOT = ROOT.parent.parent / ".tmp" / "windows-assign-actions-probe-profile"
ASSIGN_ACTIONS_PROBE_REGISTRY = ROOT.parent.parent / ".tmp" / "windows-assign-actions-probe-registry.json"
SET_SPRITE_PROBE_PROFILE_ROOT = ROOT.parent.parent / ".tmp" / "windows-set-sprite-probe-profile"
SET_SPRITE_PROBE_REGISTRY = ROOT.parent.parent / ".tmp" / "windows-set-sprite-probe-registry.json"
NEW_ENTITY_PROBE_PROFILE_ROOT = ROOT.parent.parent / ".tmp" / "windows-new-entity-probe-profile"
NEW_ENTITY_PROBE_REGISTRY = ROOT.parent.parent / ".tmp" / "windows-new-entity-probe-registry.json"
NEW_ITEM_PROBE_PROFILE_ROOT = ROOT.parent.parent / ".tmp" / "windows-new-item-probe-profile"
NEW_ITEM_PROBE_REGISTRY = ROOT.parent.parent / ".tmp" / "windows-new-item-probe-registry.json"
DO_NEW_GAME_PROBE_PROFILE_ROOT = ROOT.parent.parent / ".tmp" / "windows-do-new-game-probe-profile"
DO_NEW_GAME_PROBE_REGISTRY = ROOT.parent.parent / ".tmp" / "windows-do-new-game-probe-registry.json"
INIT_CLASS_PROBE_PROFILE_ROOT = ROOT.parent.parent / ".tmp" / "windows-init-class-probe-profile"
INIT_CLASS_PROBE_REGISTRY = ROOT.parent.parent / ".tmp" / "windows-init-class-probe-registry.json"
SHOPPING_SPREE_SUMMON_PROFILE_ROOT = ROOT.parent.parent / ".tmp" / "windows-shopping-spree-summon-profile"
SHOPPING_SPREE_SUMMON_REGISTRY = ROOT.parent.parent / ".tmp" / "windows-shopping-spree-summon-registry.json"
STASH_CORE_BEHAVIOR_PROFILE_ROOT = ROOT.parent.parent / ".tmp" / "windows-stash-core-behavior-profile"
STASH_CORE_BEHAVIOR_REGISTRY = ROOT.parent.parent / ".tmp" / "windows-stash-core-behavior-registry.json"
PLACEMENT_FAILCLOSED_PROFILE_ROOT = ROOT.parent.parent / ".tmp" / "windows-placement-failclosed-profile"
PLACEMENT_FAILCLOSED_REGISTRY = ROOT.parent.parent / ".tmp" / "windows-placement-failclosed-registry.json"

FIXTURES_DIR = ROOT.parent.parent / "framework" / "BaronyModLoader" / "fixtures"
APP_MODULE_PATH = ROOT.parent.parent / "framework" / "BaronyModLoader" / "app" / "barony_mod_loader.py"


def load_app_module():
    spec = importlib.util.spec_from_file_location("barony_mod_loader_app", APP_MODULE_PATH)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"Unable to load app module from {APP_MODULE_PATH}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def write_runtime_manifest_with_app(profile_root: Path, package_dir: Path, runtime_info_path: Path, hook_manifest: Path) -> Path:
    app = load_app_module()
    package, package_load_result = app.load_package(str(package_dir))
    if package is None or not package_load_result.ok:
        raise AssertionError([problem.code for problem in package_load_result.problems])
    runtime_info, _, runtime_load_result = app.load_runtime_info(str(runtime_info_path))
    if runtime_info is None or not runtime_load_result.ok:
        raise AssertionError([problem.code for problem in runtime_load_result.problems])

    profile = {
        "profile": {"id": "windows-direct-test"},
        "runtime": {
            "baronyExecutable": r"C:\Games\Barony\barony.exe",
            "gameSource": "steam",
            "steam": {"appId": "371970", "buildId": "22630456"},
        },
    }
    runtime_registration = {
        "id": f"{runtime_info['runtimeId']}-registration",
        "runtimeStrategy": "installed-binary-hook",
        "platform": "windows-x86_64",
        "storefront": "steam",
        "steamExecutable": r"C:\Games\Barony\barony.exe",
        "steamExecutableSha256": "1" * 64,
        "steamExecutableBuildId": "22630456",
        "gameVersionString": "v5.0.2",
        "hookLibrary": str(HOOK_DLL),
        "hookLibrarySha256": "2" * 64,
        "hookManifest": str(hook_manifest),
        "hookManifestSha256": "3" * 64,
    }
    manifest_path = profile_root / "BaronyModLoader" / "runtime-manifest.json"
    app.write_launch_artifacts(
        profile,
        profile_root,
        package,
        runtime_info,
        manifest_path,
        runtime_registration=runtime_registration,
    )
    return manifest_path

class WindowsHookDllTests(unittest.TestCase):
    @unittest.skipUnless(os.name == "nt", "Windows PowerShell test requires Windows")
    @unittest.skipUnless(POWERSHELL_SCRIPT.is_file(), "run_bml_hook_win_selftest.ps1 is missing")
    @unittest.skipUnless(HOOK_DLL.is_file(), "bml_hook_win.dll has not been built")
    @unittest.skipUnless(FAKE_PROVIDER_DLL.is_file(), "bml_fake_provider_win.dll has not been built")
    def test_powershell_selftest_script_passes(self):
        powershell = Path(r"C:\Windows\Sysnative\WindowsPowerShell\v1.0\powershell.exe")
        if not powershell.is_file():
            powershell = Path(r"C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe")
        result = subprocess.run(
            [str(powershell), "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", str(POWERSHELL_SCRIPT)],
            cwd=str(ROOT.parent.parent),
            capture_output=True,
            text=True,
            check=False,
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("OK: Windows hook direct self-tests passed.", result.stdout)

        report_root = ROOT.parent.parent / ".tmp" / "windows-direct-selftest-profile" / "BaronyModLoader" / "reports"
        fake_report = json.loads((report_root / "windows-fake-stash-detour-report.json").read_text(encoding="utf-8"))
        self.assertEqual(fake_report["status"], "installed")
        self.assertTrue(fake_report["replacementCallsOriginal"])
        self.assertEqual(fake_report["before"], 12884901916)
        self.assertEqual(fake_report["after"], 12884901929)
        self.assertEqual(fake_report["trampoline"], 12884901916)
        detour_report = json.loads((report_root / "windows-detour-self-test-report.json").read_text(encoding="utf-8"))
        self.assertEqual(detour_report["status"], "installed")
        self.assertTrue(detour_report["callRelocated"]["installed"])
        self.assertEqual(detour_report["callRelocated"]["before"], 7)
        self.assertEqual(detour_report["callRelocated"]["after"], 11)
        self.assertEqual(detour_report["callRelocated"]["trampoline"], 7)
        self.assertTrue(detour_report["ripRelocated"]["installed"])
        self.assertEqual(detour_report["ripRelocated"]["before"], 7)
        self.assertEqual(detour_report["ripRelocated"]["after"], 11)
        self.assertEqual(detour_report["ripRelocated"]["trampoline"], 7)
    @unittest.skipUnless(os.name == "nt", "Windows PowerShell test requires Windows")
    @unittest.skipUnless(GET_ITEM_INSTALL_PROBE_SCRIPT.is_file(), "run_bml_get_item_probe.ps1 is missing")
    def test_powershell_get_item_install_probe_passes(self):
        powershell = Path(r"C:\Windows\Sysnative\WindowsPowerShell\v1.0\powershell.exe")
        if not powershell.is_file():
            powershell = Path(r"C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe")
        env = dict(os.environ)
        env.setdefault("BML_TEST_PYTHON", sys.executable)
        env.setdefault("BML_TEST_STEAM_EXE", str(INSTALL_PROBE_STEAM_EXE))
        env.setdefault("BML_TEST_LAUNCHER_HELPER", str(INSTALL_PROBE_LAUNCHER))
        env.setdefault("BML_TEST_HOOK_DLL", str(HOOK_DLL))
        env.setdefault("BML_TEST_PROFILE_ROOT", str(GET_ITEM_PROBE_PROFILE_ROOT))
        env.setdefault("BML_TEST_REGISTRY", str(GET_ITEM_PROBE_REGISTRY))
        result = subprocess.run(
            [str(powershell), "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", str(GET_ITEM_INSTALL_PROBE_SCRIPT)],
            cwd=str(ROOT.parent.parent),
            capture_output=True,
            text=True,
            check=False,
            env=env,
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        if result.stdout.strip().startswith("SKIP:"):
            self.skipTest(result.stdout.strip())
        self.assertIn("OK: Windows getItemFromChest install-only probe passed.", result.stdout)

        report_root = GET_ITEM_PROBE_PROFILE_ROOT / "BaronyModLoader" / "reports"
        probe_report = json.loads((report_root / "windows-get-item-passthrough-install-report.json").read_text(encoding="utf-8"))
        self.assertEqual(probe_report["status"], "installed")
        self.assertTrue(probe_report["codeViewMatch"])
        self.assertEqual(probe_report["rva"], 3044448)
        self.assertTrue(probe_report["prologueMatch"])
        self.assertTrue(probe_report["installed"])
        self.assertEqual(probe_report["replacementCalls"], 0)

    @unittest.skipUnless(os.name == "nt", "Windows PowerShell test requires Windows")
    @unittest.skipUnless(ADD_ITEM_VOID_PROBE_SCRIPT.is_file(), "run_bml_add_item_void_probe.ps1 is missing")
    def test_powershell_add_item_void_install_probe_passes(self):
        powershell = Path(r"C:\Windows\Sysnative\WindowsPowerShell\v1.0\powershell.exe")
        if not powershell.is_file():
            powershell = Path(r"C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe")
        env = dict(os.environ)
        env.setdefault("BML_TEST_PYTHON", sys.executable)
        env.setdefault("BML_TEST_STEAM_EXE", str(INSTALL_PROBE_STEAM_EXE))
        env.setdefault("BML_TEST_LAUNCHER_HELPER", str(INSTALL_PROBE_LAUNCHER))
        env.setdefault("BML_TEST_HOOK_DLL", str(HOOK_DLL))
        env.setdefault("BML_TEST_PROFILE_ROOT", str(ADD_ITEM_VOID_PROBE_PROFILE_ROOT))
        env.setdefault("BML_TEST_REGISTRY", str(ADD_ITEM_VOID_PROBE_REGISTRY))
        result = subprocess.run(
            [str(powershell), "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", str(ADD_ITEM_VOID_PROBE_SCRIPT)],
            cwd=str(ROOT.parent.parent),
            capture_output=True,
            text=True,
            check=False,
            env=env,
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        if result.stdout.strip().startswith("SKIP:"):
            self.skipTest(result.stdout.strip())
        self.assertIn("OK: Windows addItemToVoidChestServer install-only probe passed.", result.stdout)

        report_root = ADD_ITEM_VOID_PROBE_PROFILE_ROOT / "BaronyModLoader" / "reports"
        probe_report = json.loads((report_root / "windows-add-item-void-chest-probe-report.json").read_text(encoding="utf-8"))
        self.assertEqual(probe_report["status"], "installed")
        self.assertTrue(probe_report["codeViewMatch"])
        self.assertEqual(probe_report["rva"], 3031968)
        self.assertTrue(probe_report["prologueMatch"])
        self.assertTrue(probe_report["installed"])
        self.assertEqual(probe_report["replacementCalls"], 0)

    @unittest.skipUnless(os.name == "nt", "Windows PowerShell test requires Windows")
    @unittest.skipUnless(GET_CHEST_LIST_PROBE_SCRIPT.is_file(), "run_bml_get_chest_list_probe.ps1 is missing")
    def test_powershell_get_chest_list_install_probe_passes(self):
        powershell = Path(r"C:\Windows\Sysnative\WindowsPowerShell\v1.0\powershell.exe")
        if not powershell.is_file():
            powershell = Path(r"C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe")
        env = dict(os.environ)
        env.setdefault("BML_TEST_PYTHON", sys.executable)
        env.setdefault("BML_TEST_STEAM_EXE", str(INSTALL_PROBE_STEAM_EXE))
        env.setdefault("BML_TEST_LAUNCHER_HELPER", str(INSTALL_PROBE_LAUNCHER))
        env.setdefault("BML_TEST_HOOK_DLL", str(HOOK_DLL))
        env.setdefault("BML_TEST_PROFILE_ROOT", str(GET_CHEST_LIST_PROBE_PROFILE_ROOT))
        env.setdefault("BML_TEST_REGISTRY", str(GET_CHEST_LIST_PROBE_REGISTRY))
        result = subprocess.run(
            [str(powershell), "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", str(GET_CHEST_LIST_PROBE_SCRIPT)],
            cwd=str(ROOT.parent.parent),
            capture_output=True,
            text=True,
            check=False,
            env=env,
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        if result.stdout.strip().startswith("SKIP:"):
            self.skipTest(result.stdout.strip())
        self.assertIn("OK: Windows getChestInventoryList install-only probe passed.", result.stdout)

        report_root = GET_CHEST_LIST_PROBE_PROFILE_ROOT / "BaronyModLoader" / "reports"
        probe_report = json.loads((report_root / "windows-get-chest-list-probe-report.json").read_text(encoding="utf-8"))
        self.assertEqual(probe_report["status"], "installed")
        self.assertTrue(probe_report["codeViewMatch"])
        self.assertEqual(probe_report["rva"], 3044368)
        self.assertTrue(probe_report["prologueMatch"])
        self.assertTrue(probe_report["installed"])
        self.assertEqual(probe_report["replacementCalls"], 0)

    @unittest.skipUnless(os.name == "nt", "Windows PowerShell test requires Windows")
    @unittest.skipUnless(REMOVE_ITEM_VOID_PROBE_SCRIPT.is_file(), "run_bml_remove_item_void_probe.ps1 is missing")
    def test_powershell_remove_item_void_install_probe_passes(self):
        powershell = Path(r"C:\Windows\Sysnative\WindowsPowerShell\v1.0\powershell.exe")
        if not powershell.is_file():
            powershell = Path(r"C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe")
        env = dict(os.environ)
        env.setdefault("BML_TEST_PYTHON", sys.executable)
        env.setdefault("BML_TEST_STEAM_EXE", str(INSTALL_PROBE_STEAM_EXE))
        env.setdefault("BML_TEST_LAUNCHER_HELPER", str(INSTALL_PROBE_LAUNCHER))
        env.setdefault("BML_TEST_HOOK_DLL", str(HOOK_DLL))
        env.setdefault("BML_TEST_PROFILE_ROOT", str(REMOVE_ITEM_VOID_PROBE_PROFILE_ROOT))
        env.setdefault("BML_TEST_REGISTRY", str(REMOVE_ITEM_VOID_PROBE_REGISTRY))
        result = subprocess.run(
            [str(powershell), "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", str(REMOVE_ITEM_VOID_PROBE_SCRIPT)],
            cwd=str(ROOT.parent.parent),
            capture_output=True,
            text=True,
            check=False,
            env=env,
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        if result.stdout.strip().startswith("SKIP:"):
            self.skipTest(result.stdout.strip())
        self.assertIn("OK: Windows removeItemFromVoidChestServer install-only probe passed.", result.stdout)

        report_root = REMOVE_ITEM_VOID_PROBE_PROFILE_ROOT / "BaronyModLoader" / "reports"
        probe_report = json.loads((report_root / "windows-remove-item-void-chest-probe-report.json").read_text(encoding="utf-8"))
        self.assertEqual(probe_report["status"], "installed")
        self.assertTrue(probe_report["codeViewMatch"])
        self.assertEqual(probe_report["rva"], 3045456)
        self.assertTrue(probe_report["prologueMatch"])
        self.assertTrue(probe_report["installed"])
        self.assertEqual(probe_report["replacementCalls"], 0)

    @unittest.skipUnless(os.name == "nt", "Windows PowerShell test requires Windows")
    @unittest.skipUnless(CLOSE_CHEST_SERVER_PROBE_SCRIPT.is_file(), "run_bml_close_chest_server_probe.ps1 is missing")
    def test_powershell_close_chest_server_install_probe_reports_stage(self):
        powershell = Path(r"C:\Windows\Sysnative\WindowsPowerShell\v1.0\powershell.exe")
        if not powershell.is_file():
            powershell = Path(r"C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe")
        env = dict(os.environ)
        env.setdefault("BML_TEST_PYTHON", sys.executable)
        env.setdefault("BML_TEST_STEAM_EXE", str(INSTALL_PROBE_STEAM_EXE))
        env.setdefault("BML_TEST_LAUNCHER_HELPER", str(INSTALL_PROBE_LAUNCHER))
        env.setdefault("BML_TEST_HOOK_DLL", str(HOOK_DLL))
        env.setdefault("BML_TEST_PROFILE_ROOT", str(CLOSE_CHEST_SERVER_PROBE_PROFILE_ROOT))
        env.setdefault("BML_TEST_REGISTRY", str(CLOSE_CHEST_SERVER_PROBE_REGISTRY))
        result = subprocess.run(
            [str(powershell), "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", str(CLOSE_CHEST_SERVER_PROBE_SCRIPT)],
            cwd=str(ROOT.parent.parent),
            capture_output=True,
            text=True,
            check=False,
            env=env,
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        if result.stdout.strip().startswith("SKIP:"):
            self.skipTest(result.stdout.strip())
        report_root = CLOSE_CHEST_SERVER_PROBE_PROFILE_ROOT / "BaronyModLoader" / "reports"
        probe_report = json.loads((report_root / "windows-close-chest-server-probe-report.json").read_text(encoding="utf-8"))
        runtime_report = json.loads((report_root / "runtime-load-report.json").read_text(encoding="utf-8"))
        self.assertEqual(probe_report["rva"], 3035632)
        self.assertTrue(probe_report["codeViewMatch"])
        self.assertTrue(probe_report["prologueMatch"])
        self.assertIn("patchWindowBytes", probe_report)
        self.assertIn("installCode", probe_report)
        self.assertIn("installMessage", probe_report)
        self.assertNotEqual(probe_report["installCode"], "unknown")
        self.assertTrue(probe_report["installMessage"])
        if probe_report["installed"]:
            self.assertEqual(probe_report["installCode"], "BML_WINDOWS_DETOUR_INSTALLED")
            self.assertEqual(runtime_report["status"], "loaded")
            self.assertEqual(probe_report["replacementCalls"], 0)
            self.assertIn("OK: Windows closeChestServer install-only probe passed.", result.stdout)
        else:
            self.assertEqual(runtime_report["status"], "failed")
            self.assertIn("BML_WINDOWS_CLOSE_CHEST_SERVER_PROBE_INSTALL_FAILED", [problem["code"] for problem in runtime_report["errors"]])
            self.assertIn("OK: Windows closeChestServer install-only probe failed with recorded install diagnostics.", result.stdout)

    @unittest.skipUnless(os.name == "nt", "Windows PowerShell test requires Windows")
    @unittest.skipUnless(STASH_CORE_BEHAVIOR_PROBE_SCRIPT.is_file(), "run_bml_stash_core_behavior_probe.ps1 is missing")
    def test_powershell_stash_core_behavior_probe_passes(self):
        powershell = Path(r"C:\Windows\Sysnative\WindowsPowerShell\v1.0\powershell.exe")
        if not powershell.is_file():
            powershell = Path(r"C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe")
        env = dict(os.environ)
        env.setdefault("BML_TEST_PYTHON", sys.executable)
        env.setdefault("BML_TEST_STEAM_EXE", str(INSTALL_PROBE_STEAM_EXE))
        env.setdefault("BML_TEST_LAUNCHER_HELPER", str(INSTALL_PROBE_LAUNCHER))
        env.setdefault("BML_TEST_HOOK_DLL", str(HOOK_DLL))
        env.setdefault("BML_TEST_PROFILE_ROOT", str(STASH_CORE_BEHAVIOR_PROFILE_ROOT))
        env.setdefault("BML_TEST_REGISTRY", str(STASH_CORE_BEHAVIOR_REGISTRY))
        result = subprocess.run(
            [str(powershell), "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", str(STASH_CORE_BEHAVIOR_PROBE_SCRIPT)],
            cwd=str(ROOT.parent.parent),
            capture_output=True,
            text=True,
            check=False,
            env=env,
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        if result.stdout.strip().startswith("SKIP:"):
            self.skipTest(result.stdout.strip())
        self.assertIn("OK: Windows stash core behavior install probe passed.", result.stdout)

        report_root = STASH_CORE_BEHAVIOR_PROFILE_ROOT / "BaronyModLoader" / "reports"
        core_report = json.loads((report_root / "stash-core-behavior-report.json").read_text(encoding="utf-8"))
        runtime_report = json.loads((report_root / "runtime-load-report.json").read_text(encoding="utf-8"))
        self.assertEqual(core_report["status"], "installed")
        self.assertEqual(len(core_report["targets"]), 7)
        self.assertEqual(sum(1 for target in core_report["targets"] if target["installed"]), 7)
        self.assertEqual(runtime_report["status"], "loaded")

    @unittest.skipUnless(os.name == "nt", "Windows PowerShell test requires Windows")
    @unittest.skipUnless(NEW_ITEM_PROBE_SCRIPT.is_file(), "run_bml_new_item_probe.ps1 is missing")
    def test_powershell_new_item_probe_passes_on_minetown(self):
        powershell = Path(r"C:\Windows\Sysnative\WindowsPowerShell\v1.0\powershell.exe")
        if not powershell.is_file():
            powershell = Path(r"C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe")
        env = dict(os.environ)
        env.setdefault("BML_TEST_PYTHON", sys.executable)
        env.setdefault("BML_TEST_STEAM_EXE", str(INSTALL_PROBE_STEAM_EXE))
        env.setdefault("BML_TEST_LAUNCHER_HELPER", str(INSTALL_PROBE_LAUNCHER))
        env.setdefault("BML_TEST_HOOK_DLL", str(HOOK_DLL))
        env["BML_TEST_NEW_ITEM_RVA"] = "5140912"
        env["BML_TEST_MAP"] = "minetown"
        env["BML_TEST_ACCEPT_ANY_FIRE"] = "1"
        env.setdefault("BML_TEST_PROFILE_ROOT", str(NEW_ITEM_PROBE_PROFILE_ROOT))
        env.setdefault("BML_TEST_REGISTRY", str(NEW_ITEM_PROBE_REGISTRY))
        result = subprocess.run(
            [str(powershell), "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", str(NEW_ITEM_PROBE_SCRIPT)],
            cwd=str(ROOT.parent.parent),
            capture_output=True,
            text=True,
            check=False,
            env=env,
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        if result.stdout.strip().startswith("SKIP:"):
            self.skipTest(result.stdout.strip())
        self.assertIn("OK: Windows newItem fired-hook probe passed.", result.stdout)

        report_root = NEW_ITEM_PROBE_PROFILE_ROOT / "BaronyModLoader" / "reports"
        probe_report = json.loads((report_root / "windows-new-item-probe-report.json").read_text(encoding="utf-8"))
        self.assertEqual(probe_report["status"], "fired")
        self.assertTrue(probe_report["installed"])
        self.assertEqual(probe_report["claimBoundary"], "real-barony-target-fired-probe-relaxed-prefix")
        self.assertLess(probe_report["prefixMatches"], 4)
        self.assertGreater(probe_report["replacementCalls"], 0)

    @unittest.skipUnless(os.name == "nt", "Windows PowerShell test requires Windows")
    @unittest.skipUnless(PLACEMENT_FAILCLOSED_SCRIPT.is_file(), "run_bml_placement_discovery_failclosed.ps1 is missing")
    def test_powershell_placement_discovery_fails_closed_without_rvas(self):
        powershell = Path(r"C:\Windows\Sysnative\WindowsPowerShell\v1.0\powershell.exe")
        if not powershell.is_file():
            powershell = Path(r"C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe")
        env = dict(os.environ)
        env.setdefault("BML_TEST_PYTHON", sys.executable)
        env.setdefault("BML_TEST_STEAM_EXE", str(INSTALL_PROBE_STEAM_EXE))
        env.setdefault("BML_TEST_LAUNCHER_HELPER", str(INSTALL_PROBE_LAUNCHER))
        env.setdefault("BML_TEST_HOOK_DLL", str(HOOK_DLL))
        env.setdefault("BML_TEST_PROFILE_ROOT", str(PLACEMENT_FAILCLOSED_PROFILE_ROOT))
        env.setdefault("BML_TEST_REGISTRY", str(PLACEMENT_FAILCLOSED_REGISTRY))
        result = subprocess.run(
            [str(powershell), "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", str(PLACEMENT_FAILCLOSED_SCRIPT)],
            cwd=str(ROOT.parent.parent),
            capture_output=True,
            text=True,
            check=False,
            env=env,
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        if result.stdout.strip().startswith("SKIP:"):
            self.skipTest(result.stdout.strip())
        self.assertIn("OK: Windows placement discovery fails closed without RVAs.", result.stdout)

        report_root = PLACEMENT_FAILCLOSED_PROFILE_ROOT / "BaronyModLoader" / "reports"
        runtime_report = json.loads((report_root / "runtime-load-report.json").read_text(encoding="utf-8"))
        self.assertEqual(runtime_report["status"], "failed")
        self.assertIn("BML_WINDOWS_PLACEMENT_DISCOVERY_INSTALL_FAILED", [problem["code"] for problem in runtime_report["errors"]])
        self.assertFalse((report_root / "stash-placement-discovery-report.json").exists())

    @unittest.skipUnless(os.name == "nt", "Windows PowerShell test requires Windows")
    @unittest.skipUnless(PLACEMENT_FAILCLOSED_SCRIPT.is_file(), "run_bml_placement_discovery_failclosed.ps1 is missing")
    def test_powershell_placement_discovery_fails_closed_with_bad_setsprite_rva(self):
        powershell = Path(r"C:\Windows\Sysnative\WindowsPowerShell\v1.0\powershell.exe")
        if not powershell.is_file():
            powershell = Path(r"C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe")
        profile_root = ROOT.parent.parent / ".tmp" / "windows-placement-bad-setsprite-profile"
        registry = ROOT.parent.parent / ".tmp" / "windows-placement-bad-setsprite-registry.json"
        env = dict(os.environ)
        env.setdefault("BML_TEST_PYTHON", sys.executable)
        env.setdefault("BML_TEST_STEAM_EXE", str(INSTALL_PROBE_STEAM_EXE))
        env.setdefault("BML_TEST_LAUNCHER_HELPER", str(INSTALL_PROBE_LAUNCHER))
        env.setdefault("BML_TEST_HOOK_DLL", str(HOOK_DLL))
        env.setdefault("BML_TEST_PROFILE_ROOT", str(profile_root))
        env.setdefault("BML_TEST_REGISTRY", str(registry))
        env["BML_STASH_PLACEMENT_DISCOVERY_ASSIGN_RVA"] = "3497952"
        env["BML_STASH_PLACEMENT_DISCOVERY_NEW_ENTITY_RVA"] = "6070496"
        env["BML_STASH_PLACEMENT_DISCOVERY_SET_SPRITE_RVA"] = "1"
        result = subprocess.run(
            [str(powershell), "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", str(PLACEMENT_FAILCLOSED_SCRIPT)],
            cwd=str(ROOT.parent.parent),
            capture_output=True,
            text=True,
            check=False,
            env=env,
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        if result.stdout.strip().startswith("SKIP:"):
            self.skipTest(result.stdout.strip())
        self.assertIn("OK: Windows placement discovery fails closed without RVAs.", result.stdout)
        report_root = profile_root / "BaronyModLoader" / "reports"
        runtime_report = json.loads((report_root / "runtime-load-report.json").read_text(encoding="utf-8"))
        self.assertEqual(runtime_report["status"], "failed")
        self.assertIn("BML_WINDOWS_PLACEMENT_DISCOVERY_INSTALL_FAILED", [problem["code"] for problem in runtime_report["errors"]])
        self.assertFalse((report_root / "stash-placement-discovery-report.json").exists())
        recovery_profile_root = ROOT.parent.parent / ".tmp" / "windows-placement-bad-setsprite-recovery-profile"
        recovery_registry = ROOT.parent.parent / ".tmp" / "windows-placement-bad-setsprite-recovery-registry.json"
        recovery_env = dict(os.environ)
        recovery_env.setdefault("BML_TEST_PYTHON", sys.executable)
        recovery_env.setdefault("BML_TEST_STEAM_EXE", str(INSTALL_PROBE_STEAM_EXE))
        recovery_env.setdefault("BML_TEST_LAUNCHER_HELPER", str(INSTALL_PROBE_LAUNCHER))
        recovery_env.setdefault("BML_TEST_HOOK_DLL", str(HOOK_DLL))
        recovery_env["BML_TEST_NEW_ENTITY_RVA"] = "6070496"
        recovery_env.setdefault("BML_TEST_PROFILE_ROOT", str(recovery_profile_root))
        recovery_env.setdefault("BML_TEST_REGISTRY", str(recovery_registry))
        recovery = subprocess.run(
            [str(powershell), "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", str(NEW_ENTITY_PROBE_SCRIPT)],
            cwd=str(ROOT.parent.parent),
            capture_output=True,
            text=True,
            check=False,
            env=recovery_env,
        )
        self.assertEqual(recovery.returncode, 0, recovery.stdout + recovery.stderr)
        self.assertIn("OK: Windows newEntity fired-hook probe passed.", recovery.stdout)

    @unittest.skipUnless(os.name == "nt", "Windows PowerShell test requires Windows")
    @unittest.skipUnless((ROOT / "tests" / "run_bml_placement_discovery_probe.ps1").is_file(), "run_bml_placement_discovery_probe.ps1 is missing")
    def test_powershell_placement_discovery_probe_requires_explicit_rvas(self):
        powershell = Path(r"C:\Windows\Sysnative\WindowsPowerShell\v1.0\powershell.exe")
        if not powershell.is_file():
            powershell = Path(r"C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe")
        profile_root = ROOT.parent.parent / ".tmp" / "windows-placement-discovery-probe-profile"
        registry = ROOT.parent.parent / ".tmp" / "windows-placement-discovery-probe-registry.json"
        env = dict(os.environ)
        env.setdefault("BML_TEST_PYTHON", sys.executable)
        env.setdefault("BML_TEST_STEAM_EXE", str(INSTALL_PROBE_STEAM_EXE))
        env.setdefault("BML_TEST_LAUNCHER_HELPER", str(INSTALL_PROBE_LAUNCHER))
        env.setdefault("BML_TEST_HOOK_DLL", str(HOOK_DLL))
        env.setdefault("BML_TEST_PROFILE_ROOT", str(profile_root))
        env.setdefault("BML_TEST_REGISTRY", str(registry))
        env.pop("BML_STASH_PLACEMENT_DISCOVERY_ASSIGN_RVA", None)
        env.pop("BML_STASH_PLACEMENT_DISCOVERY_NEW_ENTITY_RVA", None)
        env.pop("BML_STASH_PLACEMENT_DISCOVERY_SET_SPRITE_RVA", None)
        result = subprocess.run(
            [str(powershell), "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", str(ROOT / "tests" / "run_bml_placement_discovery_probe.ps1")],
            cwd=str(ROOT.parent.parent),
            capture_output=True,
            text=True,
            check=False,
            env=env,
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("SKIP: Placement discovery probe requires explicit assign/newEntity/setSprite RVAs.", result.stdout)

    @unittest.skipUnless(os.name == "nt", "Windows PowerShell test requires Windows")
    @unittest.skipUnless(SET_SPRITE_PROBE_SCRIPT.is_file(), "run_bml_set_sprite_probe.ps1 is missing")
    def test_powershell_set_sprite_probe_requires_explicit_rva(self):
        powershell = Path(r"C:\Windows\Sysnative\WindowsPowerShell\v1.0\powershell.exe")
        if not powershell.is_file():
            powershell = Path(r"C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe")
        profile_root = ROOT.parent.parent / ".tmp" / "windows-set-sprite-probe-profile"
        registry = ROOT.parent.parent / ".tmp" / "windows-set-sprite-probe-registry.json"
        env = dict(os.environ)
        env.setdefault("BML_TEST_PYTHON", sys.executable)
        env.setdefault("BML_TEST_STEAM_EXE", str(INSTALL_PROBE_STEAM_EXE))
        env.setdefault("BML_TEST_LAUNCHER_HELPER", str(INSTALL_PROBE_LAUNCHER))
        env.setdefault("BML_TEST_HOOK_DLL", str(HOOK_DLL))
        env.setdefault("BML_TEST_PROFILE_ROOT", str(profile_root))
        env.setdefault("BML_TEST_REGISTRY", str(registry))
        env.pop("BML_SET_SPRITE_PROBE_RVA", None)
        result = subprocess.run(
            [str(powershell), "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", str(SET_SPRITE_PROBE_SCRIPT)],
            cwd=str(ROOT.parent.parent),
            capture_output=True,
            text=True,
            check=False,
            env=env,
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("SKIP: BML_SET_SPRITE_PROBE_RVA is not set for the setSprite fired-hook probe.", result.stdout)

    @unittest.skipUnless(os.name == "nt", "Windows PowerShell test requires Windows")
    @unittest.skipUnless(ASSIGN_ACTIONS_PROBE_SCRIPT.is_file(), "run_bml_assign_actions_probe.ps1 is missing")
    @unittest.skipUnless(os.environ.get("BML_ENABLE_ASSIGN_ACTIONS_PROBE_TEST") == "1", "assignActions fired-hook probe is opt-in and requires a local Barony launch")
    def test_powershell_assign_actions_probe_passes(self):
        powershell = Path(r"C:\Windows\Sysnative\WindowsPowerShell\v1.0\powershell.exe")
        if not powershell.is_file():
            powershell = Path(r"C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe")
        env = dict(os.environ)
        env.setdefault("BML_TEST_PYTHON", sys.executable)
        env.setdefault("BML_TEST_STEAM_EXE", str(INSTALL_PROBE_STEAM_EXE))
        env.setdefault("BML_TEST_LAUNCHER_HELPER", str(INSTALL_PROBE_LAUNCHER))
        env.setdefault("BML_TEST_HOOK_DLL", str(HOOK_DLL))
        env.setdefault("BML_TEST_ASSIGN_ACTIONS_RVA", "")
        env.setdefault("BML_TEST_PROFILE_ROOT", str(ASSIGN_ACTIONS_PROBE_PROFILE_ROOT))
        env.setdefault("BML_TEST_REGISTRY", str(ASSIGN_ACTIONS_PROBE_REGISTRY))
        result = subprocess.run(
            [str(powershell), "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", str(ASSIGN_ACTIONS_PROBE_SCRIPT)],
            cwd=str(ROOT.parent.parent),
            capture_output=True,
            text=True,
            check=False,
            env=env,
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        if result.stdout.strip().startswith("SKIP:"):
            self.skipTest(result.stdout.strip())
        self.assertIn("OK: Windows assignActions fired-hook probe passed.", result.stdout)

        report_root = ASSIGN_ACTIONS_PROBE_PROFILE_ROOT / "BaronyModLoader" / "reports"
        probe_report = json.loads((report_root / "windows-assign-actions-probe-report.json").read_text(encoding="utf-8"))
        self.assertEqual(probe_report["status"], "fired")
        self.assertGreater(probe_report["replacementCalls"], 0)
        self.assertTrue(probe_report["installed"])
        self.assertTrue(probe_report["map"]["globalMapSymbol"] > 0)
        self.assertTrue(probe_report["map"]["argumentMatchesGlobal"])

    @unittest.skipUnless(os.name == "nt", "Windows PowerShell test requires Windows")
    @unittest.skipUnless(DO_NEW_GAME_PROBE_SCRIPT.is_file(), "run_bml_do_new_game_probe.ps1 is missing")
    @unittest.skipUnless(os.environ.get("BML_ENABLE_DO_NEW_GAME_PROBE_TEST") == "1", "doNewGame fired-hook probe is opt-in and requires a local Barony launch")
    def test_powershell_do_new_game_probe_passes(self):
        powershell = Path(r"C:\Windows\Sysnative\WindowsPowerShell\v1.0\powershell.exe")
        if not powershell.is_file():
            powershell = Path(r"C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe")
        env = dict(os.environ)
        env.setdefault("BML_TEST_PYTHON", sys.executable)
        env.setdefault("BML_TEST_STEAM_EXE", str(INSTALL_PROBE_STEAM_EXE))
        env.setdefault("BML_TEST_LAUNCHER_HELPER", str(INSTALL_PROBE_LAUNCHER))
        env.setdefault("BML_TEST_HOOK_DLL", str(HOOK_DLL))
        env.setdefault("BML_TEST_DO_NEW_GAME_RVA", "")
        env.setdefault("BML_TEST_PROFILE_ROOT", str(DO_NEW_GAME_PROBE_PROFILE_ROOT))
        env.setdefault("BML_TEST_REGISTRY", str(DO_NEW_GAME_PROBE_REGISTRY))
        result = subprocess.run(
            [str(powershell), "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", str(DO_NEW_GAME_PROBE_SCRIPT)],
            cwd=str(ROOT.parent.parent),
            capture_output=True,
            text=True,
            check=False,
            env=env,
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        if result.stdout.strip().startswith("SKIP:"):
            self.skipTest(result.stdout.strip())
        self.assertIn("OK: Windows doNewGame fired-hook probe passed.", result.stdout)

        report_root = DO_NEW_GAME_PROBE_PROFILE_ROOT / "BaronyModLoader" / "reports"
        probe_report = json.loads((report_root / "windows-do-new-game-probe-report.json").read_text(encoding="utf-8"))
        self.assertEqual(probe_report["status"], "fired")
        self.assertEqual(probe_report["replacementCalls"], 1)
        self.assertTrue(probe_report["installed"])
        self.assertFalse(probe_report["lastMakeHighscore"])
        runtime_report = json.loads((report_root / "runtime-load-report.json").read_text(encoding="utf-8"))
        self.assertEqual(runtime_report["status"], "loaded")
        self.assertEqual(runtime_report["errors"], [])

    @unittest.skipUnless(os.name == "nt", "Windows PowerShell test requires Windows")
    @unittest.skipUnless(DO_NEW_GAME_PROBE_SCRIPT.is_file(), "run_bml_do_new_game_probe.ps1 is missing")
    @unittest.skipUnless(os.environ.get("BML_ENABLE_SUMMON_NO_SMOKE_PROBE_TEST") == "1", "summonMonsterNoSmoke fired-hook probe is opt-in and requires a local Barony launch")
    def test_powershell_shopping_spree_summon_probe_passes(self):
        powershell = Path(r"C:\Windows\Sysnative\WindowsPowerShell\v1.0\powershell.exe")
        if not powershell.is_file():
            powershell = Path(r"C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe")
        env = dict(os.environ)
        env.setdefault("BML_TEST_PYTHON", sys.executable)
        env.setdefault("BML_TEST_STEAM_EXE", str(INSTALL_PROBE_STEAM_EXE))
        env.setdefault("BML_TEST_LAUNCHER_HELPER", str(INSTALL_PROBE_LAUNCHER))
        env.setdefault("BML_TEST_HOOK_DLL", str(HOOK_DLL))
        env.setdefault("BML_TEST_DO_NEW_GAME_RVA", "5411824")
        env.setdefault("BML_TEST_SUMMON_NO_SMOKE_RVA", "3469216")
        env.setdefault("BML_TEST_FORCE_SHOPPING_SPREE", "1")
        env.setdefault("BML_TEST_PROFILE_ROOT", str(SHOPPING_SPREE_SUMMON_PROFILE_ROOT))
        env.setdefault("BML_TEST_REGISTRY", str(SHOPPING_SPREE_SUMMON_REGISTRY))
        result = subprocess.run(
            [str(powershell), "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", str(DO_NEW_GAME_PROBE_SCRIPT)],
            cwd=str(ROOT.parent.parent),
            capture_output=True,
            text=True,
            check=False,
            env=env,
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        if result.stdout.strip().startswith("SKIP:"):
            self.skipTest(result.stdout.strip())
        self.assertIn("OK: Windows Shopping Spree summonMonsterNoSmoke probe passed.", result.stdout)

        report_root = SHOPPING_SPREE_SUMMON_PROFILE_ROOT / "BaronyModLoader" / "reports"
        summon_report = json.loads((report_root / "windows-summon-monster-probe-report.json").read_text(encoding="utf-8"))
        self.assertEqual(summon_report["status"], "fired")
        self.assertEqual(summon_report["shopkeeperCalls"], 1)
        self.assertEqual(summon_report["lastCreature"], 20)
        self.assertTrue(summon_report["lastForceLocation"])
        self.assertEqual(summon_report["lastReturnRva"], 5418932)
        runtime_report = json.loads((report_root / "runtime-load-report.json").read_text(encoding="utf-8"))
        self.assertEqual(runtime_report["status"], "loaded")
        self.assertEqual(runtime_report["errors"], [])

    @unittest.skipUnless(os.name == "nt", "Windows PowerShell test requires Windows")
    @unittest.skipUnless(INIT_CLASS_PROBE_SCRIPT.is_file(), "run_bml_init_class_probe.ps1 is missing")
    @unittest.skipUnless(os.environ.get("BML_ENABLE_INIT_CLASS_PROBE_TEST") == "1", "initClass fired-hook probe is opt-in and requires a local Barony launch")
    def test_powershell_init_class_probe_passes(self):
        powershell = Path(r"C:\Windows\Sysnative\WindowsPowerShell\v1.0\powershell.exe")
        if not powershell.is_file():
            powershell = Path(r"C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe")
        env = dict(os.environ)
        env.setdefault("BML_TEST_PYTHON", sys.executable)
        env.setdefault("BML_TEST_STEAM_EXE", str(INSTALL_PROBE_STEAM_EXE))
        env.setdefault("BML_TEST_LAUNCHER_HELPER", str(INSTALL_PROBE_LAUNCHER))
        env.setdefault("BML_TEST_HOOK_DLL", str(HOOK_DLL))
        env.setdefault("BML_INIT_CLASS_PROBE_RVA", "")
        env.setdefault("BML_TEST_PROFILE_ROOT", str(INIT_CLASS_PROBE_PROFILE_ROOT))
        env.setdefault("BML_TEST_REGISTRY", str(INIT_CLASS_PROBE_REGISTRY))
        result = subprocess.run(
            [str(powershell), "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", str(INIT_CLASS_PROBE_SCRIPT)],
            cwd=str(ROOT.parent.parent),
            capture_output=True,
            text=True,
            check=False,
            env=env,
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        if result.stdout.strip().startswith("SKIP:"):
            self.skipTest(result.stdout.strip())
        self.assertIn("OK: Windows initClass quickstart probe passed.", result.stdout)

        report_root = INIT_CLASS_PROBE_PROFILE_ROOT / "BaronyModLoader" / "reports"
        probe_report = json.loads((report_root / "windows-init-class-probe-report.json").read_text(encoding="utf-8"))
        self.assertEqual(probe_report["status"], "fired")
        self.assertGreater(probe_report["quickstartCalls"], 0)
        self.assertEqual(probe_report["lastPlayer"], 0)
        self.assertEqual(probe_report["lastReturnRva"], 5032615)
        runtime_report = json.loads((report_root / "runtime-load-report.json").read_text(encoding="utf-8"))
        self.assertEqual(runtime_report["status"], "loaded")
        self.assertEqual(runtime_report["errors"], [])

    @unittest.skipUnless(SET_SPRITE_PROBE_SCRIPT.is_file(), "run_bml_set_sprite_probe.ps1 is missing")
    @unittest.skipUnless(os.environ.get("BML_ENABLE_SET_SPRITE_PROBE_TEST") == "1", "setSprite fired-hook probe is opt-in and requires a local Barony launch")
    def test_powershell_set_sprite_probe_passes(self):
        powershell = Path(r"C:\Windows\Sysnative\WindowsPowerShell\v1.0\powershell.exe")
        if not powershell.is_file():
            powershell = Path(r"C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe")
        env = dict(os.environ)
        env.setdefault("BML_TEST_PYTHON", sys.executable)
        env.setdefault("BML_TEST_STEAM_EXE", str(INSTALL_PROBE_STEAM_EXE))
        env.setdefault("BML_TEST_LAUNCHER_HELPER", str(INSTALL_PROBE_LAUNCHER))
        env.setdefault("BML_TEST_HOOK_DLL", str(HOOK_DLL))
        env.setdefault("BML_TEST_PROFILE_ROOT", str(SET_SPRITE_PROBE_PROFILE_ROOT))
        env.setdefault("BML_TEST_REGISTRY", str(SET_SPRITE_PROBE_REGISTRY))
        env.setdefault("BML_SET_SPRITE_PROBE_RVA", "")
        result = subprocess.run(
            [str(powershell), "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", str(SET_SPRITE_PROBE_SCRIPT)],
            cwd=str(ROOT.parent.parent),
            capture_output=True,
            text=True,
            check=False,
            env=env,
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        if result.stdout.strip().startswith("SKIP:"):
            self.skipTest(result.stdout.strip())
        self.assertIn("OK: Windows setSprite fired-hook probe passed.", result.stdout)

        report_root = SET_SPRITE_PROBE_PROFILE_ROOT / "BaronyModLoader" / "reports"
        probe_report = json.loads((report_root / "windows-set-sprite-probe-report.json").read_text(encoding="utf-8"))
        self.assertEqual(probe_report["status"], "fired")
        self.assertGreater(probe_report["replacementCalls"], 0)
        self.assertTrue(probe_report["installed"])
        runtime_report = json.loads((report_root / "runtime-load-report.json").read_text(encoding="utf-8"))
        self.assertEqual(runtime_report["status"], "loaded")
        self.assertEqual(runtime_report["errors"], [])

    @unittest.skipUnless(os.name == "nt", "Windows PowerShell test requires Windows")
    @unittest.skipUnless(NEW_ENTITY_PROBE_SCRIPT.is_file(), "run_bml_new_entity_probe.ps1 is missing")
    @unittest.skipUnless(os.environ.get("BML_ENABLE_NEW_ENTITY_PROBE_TEST") == "1", "newEntity fired-hook probe is opt-in and requires a local Barony launch")
    def test_powershell_new_entity_probe_passes(self):
        powershell = Path(r"C:\Windows\Sysnative\WindowsPowerShell\v1.0\powershell.exe")
        if not powershell.is_file():
            powershell = Path(r"C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe")
        env = dict(os.environ)
        env.setdefault("BML_TEST_PYTHON", sys.executable)
        env.setdefault("BML_TEST_STEAM_EXE", str(INSTALL_PROBE_STEAM_EXE))
        env.setdefault("BML_TEST_LAUNCHER_HELPER", str(INSTALL_PROBE_LAUNCHER))
        env.setdefault("BML_TEST_HOOK_DLL", str(HOOK_DLL))
        env.setdefault("BML_TEST_NEW_ENTITY_RVA", "")
        env.setdefault("BML_TEST_PROFILE_ROOT", str(NEW_ENTITY_PROBE_PROFILE_ROOT))
        env.setdefault("BML_TEST_REGISTRY", str(NEW_ENTITY_PROBE_REGISTRY))
        result = subprocess.run(
            [str(powershell), "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", str(NEW_ENTITY_PROBE_SCRIPT)],
            cwd=str(ROOT.parent.parent),
            capture_output=True,
            text=True,
            check=False,
            env=env,
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        if result.stdout.strip().startswith("SKIP:"):
            self.skipTest(result.stdout.strip())
        self.assertIn("OK: Windows newEntity fired-hook probe passed.", result.stdout)

        report_root = NEW_ENTITY_PROBE_PROFILE_ROOT / "BaronyModLoader" / "reports"
        probe_report = json.loads((report_root / "windows-new-entity-probe-report.json").read_text(encoding="utf-8"))
        self.assertGreater(probe_report["replacementCalls"], 0)
        self.assertTrue(probe_report["installed"])
        runtime_report = json.loads((report_root / "runtime-load-report.json").read_text(encoding="utf-8"))
        self.assertEqual(runtime_report["status"], "loaded")
        self.assertEqual(runtime_report["errors"], [])
    @unittest.skipUnless(os.name == "nt", "Windows DLL test requires Windows")
    @unittest.skipUnless(struct.calcsize("P") == 8, "bml_hook_win.dll is x64 and requires a 64-bit Python process")
    @unittest.skipUnless(HOOK_DLL.is_file(), "bml_hook_win.dll has not been built")
    def test_detour_selftest_reports_failure_or_success_through_runtime_contract(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            root = Path(tmpdir)
            profile = root / "profile"
            bml_root = profile / "BaronyModLoader"
            hook_manifest = root / "hook-manifest.json"
            bml_root.mkdir(parents=True)
            manifest = write_runtime_manifest_with_app(
                profile,
                FIXTURES_DIR / "windows-smoke-package",
                FIXTURES_DIR / "runtime-info.installed-hook.windows-noop.json",
                hook_manifest,
            )
            hook_manifest.write_text(
                json.dumps(
                    {
                        "status": "noop-validated",
                        "failClosed": False,
                        "stashTargetResolution": {"status": "missing-windows-rva-or-signature-map", "failClosed": True},
                        "hook": {"entrypoint": "bml_hook_init", "implemented": True},
                    },
                    indent=2,
                )
                + "\n",
                encoding="utf-8",
            )
            old_env = {name: os.environ.get(name) for name in (
                "BML_PROFILE_DIR",
                "BML_RUNTIME_MANIFEST",
                "BML_HOOK_MANIFEST",
                "BML_HOOK_LIBRARY",
                "BML_WINDOWS_DETOUR_SELF_TEST",
            )}
            try:
                os.environ["BML_PROFILE_DIR"] = str(profile)
                os.environ["BML_RUNTIME_MANIFEST"] = str(manifest)
                os.environ["BML_HOOK_MANIFEST"] = str(hook_manifest)
                os.environ["BML_HOOK_LIBRARY"] = str(HOOK_DLL)
                os.environ["BML_WINDOWS_DETOUR_SELF_TEST"] = "1"
                hook = ctypes.WinDLL(str(HOOK_DLL))
                result = hook.bml_hook_init()
            finally:
                for name, value in old_env.items():
                    if value is None:
                        os.environ.pop(name, None)
                    else:
                        os.environ[name] = value

            runtime_report = json.loads((bml_root / "reports" / "runtime-load-report.json").read_text(encoding="utf-8"))
            detour_report = json.loads((bml_root / "reports" / "windows-detour-self-test-report.json").read_text(encoding="utf-8"))
            self.assertEqual(result, 0)
            self.assertEqual(runtime_report["runtime"]["id"], "barony-bml-runtime-windows-noop")
            self.assertEqual(runtime_report["profileId"], "windows-direct-test")
            self.assertEqual(runtime_report["status"], "loaded")
            self.assertEqual(runtime_report["loadedMods"][0]["id"], "jml.windows_smoke")
            self.assertEqual(runtime_report["errors"], [])
            self.assertEqual(detour_report["status"], "installed")
            self.assertEqual(detour_report["before"], 7)
            self.assertEqual(detour_report["after"], 11)
            self.assertEqual(detour_report["trampoline"], 7)
            self.assertTrue(detour_report["installed"])
            self.assertTrue(detour_report["callRelocated"]["installed"])
            self.assertEqual(detour_report["callRelocated"]["before"], 7)
            self.assertEqual(detour_report["callRelocated"]["after"], 11)
            self.assertEqual(detour_report["callRelocated"]["trampoline"], 7)
            self.assertTrue(detour_report["ripRelocated"]["installed"])
            self.assertEqual(detour_report["ripRelocated"]["before"], 7)
            self.assertEqual(detour_report["ripRelocated"]["after"], 11)
            self.assertEqual(detour_report["ripRelocated"]["trampoline"], 7)

    @unittest.skipUnless(os.name == "nt", "Windows DLL test requires Windows")
    @unittest.skipUnless(struct.calcsize("P") == 8, "bml_hook_win.dll is x64 and requires a 64-bit Python process")
    @unittest.skipUnless(HOOK_DLL.is_file(), "bml_hook_win.dll has not been built")
    def test_windows_stash_runtime_fails_with_incomplete_hook_manifest(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            root = Path(tmpdir)
            profile = root / "profile"
            bml_root = profile / "BaronyModLoader"
            hook_manifest = root / "hook-manifest.json"
            bml_root.mkdir(parents=True)
            manifest = write_runtime_manifest_with_app(
                profile,
                ROOT.parent.parent / "mods" / "stash",
                FIXTURES_DIR / "runtime-info.installed-hook.windows-stash.json",
                hook_manifest,
            )
            hook_manifest.write_text(
                json.dumps(
                    {
                        "status": "noop-validated",
                        "failClosed": False,
                        "stashTargetResolution": {"status": "missing-windows-rva-or-signature-map", "failClosed": True},
                        "hook": {"entrypoint": "bml_hook_init", "implemented": True},
                    },
                    indent=2,
                )
                + "\n",
                encoding="utf-8",
            )
            old_env = {name: os.environ.get(name) for name in (
                "BML_PROFILE_DIR",
                "BML_RUNTIME_MANIFEST",
                "BML_HOOK_MANIFEST",
                "BML_HOOK_LIBRARY",
                "BML_WINDOWS_DETOUR_SELF_TEST",
            )}
            try:
                os.environ["BML_PROFILE_DIR"] = str(profile)
                os.environ["BML_RUNTIME_MANIFEST"] = str(manifest)
                os.environ["BML_HOOK_MANIFEST"] = str(hook_manifest)
                os.environ["BML_HOOK_LIBRARY"] = str(HOOK_DLL)
                os.environ.pop("BML_WINDOWS_DETOUR_SELF_TEST", None)
                hook = ctypes.WinDLL(str(HOOK_DLL))
                result = hook.bml_hook_init()
            finally:
                for name, value in old_env.items():
                    if value is None:
                        os.environ.pop(name, None)
                    else:
                        os.environ[name] = value

            runtime_report = json.loads((bml_root / "reports" / "runtime-load-report.json").read_text(encoding="utf-8"))
            self.assertNotEqual(result, 0)
            self.assertEqual(runtime_report["runtime"]["id"], "barony-bml-runtime-stash-windows")
            self.assertEqual(runtime_report["profileId"], "windows-direct-test")
            self.assertEqual(runtime_report["status"], "failed")
            self.assertEqual(runtime_report["loadedMods"], [])
            self.assertIn("BML_WINDOWS_STASH_CORE_BEHAVIOR_FAILED", [error["code"] for error in runtime_report["errors"]])


if __name__ == "__main__":
    unittest.main()
