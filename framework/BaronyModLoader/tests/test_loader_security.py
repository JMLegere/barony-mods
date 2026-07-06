from __future__ import annotations

from contextlib import contextmanager
import hashlib
import importlib.util
import json
import os
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[3]
BML_ROOT = REPO_ROOT / "framework" / "BaronyModLoader"
APP_PATH = BML_ROOT / "app" / "barony_mod_loader.py"
EXAMPLE_PACKAGE = BML_ROOT / "example-stash-package.json"

spec = importlib.util.spec_from_file_location("barony_mod_loader", APP_PATH)
assert spec is not None and spec.loader is not None
loader = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = loader
spec.loader.exec_module(loader)


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def write_json(path: Path, payload: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")


class LoaderSecurityRegressionTests(unittest.TestCase):
    def run_cli(self, *args: str, env: dict[str, str] | None = None) -> subprocess.CompletedProcess[str]:
        run_env = os.environ.copy()
        if env:
            run_env.update(env)
        return subprocess.run(
            [sys.executable, str(APP_PATH), *args],
            cwd=str(REPO_ROOT),
            env=run_env,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            check=False,
        )

    @contextmanager
    def simulated_platform(
        self,
        sys_platform: str,
        machine: str = "AMD64",
        env: dict[str, str] | None = None,
    ):
        original_sys_platform = loader.sys.platform
        original_machine = loader.platform.machine
        original_env: dict[str, str | None] = {}
        if env:
            original_env = {key: os.environ.get(key) for key in env}
        try:
            loader.sys.platform = sys_platform
            loader.platform.machine = lambda: machine
            if env:
                os.environ.update(env)
            yield
        finally:
            loader.sys.platform = original_sys_platform
            loader.platform.machine = original_machine
            for key, value in original_env.items():
                if value is None:
                    os.environ.pop(key, None)
                else:
                    os.environ[key] = value

    def make_package(self, workspace: Path, name: str = "package") -> Path:
        package_dir = workspace / name
        (package_dir / "content").mkdir(parents=True)
        shutil.copy2(EXAMPLE_PACKAGE, package_dir / loader.PACKAGE_MANIFEST_NAME)
        (package_dir / "content" / "marker.txt").write_text("installed\n", encoding="utf-8")
        return package_dir

    def make_runebound_elixirs_package(self, workspace: Path) -> Path:
        package_dir = workspace / "runebound-elixirs"
        package_dir.mkdir(parents=True)
        data_dir = package_dir / "content" / "data" / "bml"
        data_dir.mkdir(parents=True)
        capabilities = list(loader.CANONICAL_RUNEBOUND_ELIXIR_CAPABILITIES)
        write_json(
            data_dir / "elixir-catalog.json",
            {
                "schemaVersion": "0.1.0",
                "namespace": "runebound_elixirs",
                "classIds": {"CLASS_BARBARIAN": "barbarian"},
                "effects": [
                    {
                        "id": "iron_skin_ac",
                        "opcode": "armor_ac_add",
                        "amount": 2,
                        "target": "player",
                    }
                ],
                "elixirs": [
                    {
                        "id": "iron_skin_bargain",
                        "displayName": "Elixir of Iron Skin",
                        "shortName": "Iron Skin",
                        "carrierItemType": "POTION_STRENGTH",
                        "classBindings": ["CLASS_BARBARIAN"],
                        "partySizeEligibility": {"minPartySize": 1, "maxPartySize": 4},
                        "lifecycle": "run_permanent",
                        "effects": ["iron_skin_ac"],
                        "upside": "Gain armor class.",
                        "tradeoffSummary": "Move more slowly after drinking.",
                        "dropWeight": 1,
                        "readabilityText": "Barbarian bargain: armor for speed.",
                        "consumeText": "Your skin hardens like iron.",
                        "visibilityText": "Iron Skin bargain active.",
                    }
                ],
                "validationRules": {
                    "requireClassBinding": True,
                    "requireTradeoff": True,
                    "requirePartySizeBounds": True,
                },
            },
        )
        write_json(
            data_dir / "elixir-drop-tables.json",
            {
                "schemaVersion": "0.1.0",
                "eligibleSources": ["chest"],
                "classPolicy": {
                    "soloPlayerClassOnly": True,
                    "multiplayerPresentClasses": True,
                },
                "partySizePolicy": {
                    "minPartySize": 1,
                    "maxPartySize": 4,
                    "eligibility": "generation_time_only",
                },
                "rolls": [
                    {
                        "source": "chest",
                        "baseChance": 0.12,
                        "weightTable": [{"elixirId": "iron_skin_bargain", "weight": 1}],
                        "maxPerFloor": 1,
                    }
                ],
                "antiBloatPolicy": {"maxConcurrentDrops": 2, "preventDuplicateElixirIds": True},
            },
        )
        write_json(
            package_dir / loader.PACKAGE_MANIFEST_NAME,
            {
                "formatVersion": loader.SCHEMA_VERSION,
                "id": "jml.runebound-elixirs",
                "name": "Runebound: Elixirs",
                "version": "0.1.0",
                "kind": "gameplay-mod",
                "engine": {
                    "runtimeContract": loader.RUNTIME_CONTRACT,
                    "minimumRuntimeVersion": "0.1.0",
                    "capabilities": [
                        {"id": capability, "version": "0.1.0", "required": True, "reason": "Runebound: Elixirs MVP validation"}
                        for capability in capabilities
                    ],
                },
                "modules": {
                    "runeboundElixirs": {
                        "namespace": "runebound_elixirs",
                        "schemaVersion": "0.1.0",
                        "authority": "host",
                        "carrierItemType": "POTION_STRENGTH",
                        "dataFiles": [
                            "content/data/bml/elixir-catalog.json",
                            "content/data/bml/elixir-drop-tables.json",
                        ],
                        "dropPolicy": {
                            "eligibleClasses": "present_party_classes",
                            "soloClassPolicy": "local_player_only",
                            "partySizeEligibility": "generation_time_only",
                            "rngAuthority": "host",
                        },
                        "activeEffects": {
                            "stateScope": "profile_save_sidecar",
                            "stateFile": "state/jml.runebound-elixirs/elixir-effects-v1.json",
                            "savePolicy": "runtime_owned",
                            "duplicatePolicy": "onePerElixirIdPerPlayer",
                            "failurePolicy": "fail-closed",
                        },
                        "display": {
                            "nameRendering": "elixir_display_name",
                            "tooltipRendering": "upside_and_tradeoff",
                            "consumeMessages": True,
                            "reminderPolicy": "runtime_diagnostics",
                        },
                        "multiplayer": {
                            "versionPolicy": "exact_package_and_contract",
                            "stateAuthority": "host",
                            "clientCompatibility": "reject_mismatch",
                            "failurePolicy": "fail-closed",
                        },
                        "failurePolicy": "fail-closed",
                    }
                },
            },
        )
        return package_dir


    def make_profile_and_registry(self, workspace: Path, package_dir: Path) -> tuple[Path, Path, Path]:
        runtime_dir = workspace / "runtime"
        runtime_dir.mkdir()
        current_target = loader.current_platform_target()
        is_windows = current_target.os_name == "windows"
        executable_name = loader.STEAM_BARONY_WINDOWS_EXECUTABLE if is_windows else "barony.x86_64"
        hook_name = loader.WINDOWS_HOOK_LIBRARY_NAME if is_windows else "libbarony_bml.so"
        steam_executable = runtime_dir / executable_name
        hook_library = runtime_dir / hook_name
        hook_manifest = runtime_dir / "hook-manifest.json"
        runtime_info_path = runtime_dir / "runtime-info.json"
        launcher_executable = runtime_dir / loader.WINDOWS_LAUNCHER_EXECUTABLE if is_windows else None
        steam_executable.write_bytes(b"fake executable v5.0.0\n")
        hook_library.write_bytes(b"fake hook\n")
        hook_manifest.write_text(json.dumps({"hook": "manifest"}) + "\n", encoding="utf-8")
        if launcher_executable is not None:
            launcher_executable.write_bytes(b"fake launcher\n")

        profile_dir = workspace / "profile"
        bml_root = profile_dir / loader.APP_ID
        for child in ("logs", "reports", "manifests", "state"):
            (bml_root / child).mkdir(parents=True, exist_ok=True)

        profile = {
            "schemaVersion": loader.SCHEMA_VERSION,
            "profile": {"id": "security-test", "createdAt": "2026-07-03T00:00:00Z", "updatedAt": "2026-07-03T00:00:00Z"},
            "app": {"id": loader.APP_ID, "version": loader.APP_VERSION},
            "paths": {"profileRoot": str(profile_dir), "bmlRoot": str(bml_root)},
            "activeMods": [],
            "runtime": {"gameSource": "manual", "baronyExecutable": str(steam_executable), "runtimeInfo": None, "steam": None},
        }
        write_json(bml_root / "profile.json", profile)

        package_manifest = json.loads((package_dir / loader.PACKAGE_MANIFEST_NAME).read_text(encoding="utf-8"))
        runtime_info = {
            "runtimeId": "test-runtime",
            "runtimeVersion": "0.1.0",
            "contract": {"id": loader.RUNTIME_CONTRACT_ID, "versions": [loader.RUNTIME_CONTRACT_VERSION]},
            "platforms": [{"platform": loader.current_platform_id()}],
            "capabilities": [
                {"id": cap["id"], "version": cap.get("version", "0.1.0")}
                for cap in package_manifest["engine"]["capabilities"]
            ],
        }
        windows_runtime_status = {
            "status": "verified",
            "evidence": {
                "evidenceKind": loader.WINDOWS_LIVE_RUNTIME_EVIDENCE_KIND,
                "platform": loader.current_platform_id(),
                "hostOs": "windows",
                "gameExecutableName": loader.STEAM_BARONY_WINDOWS_EXECUTABLE,
                "launcherExecutableName": loader.WINDOWS_LAUNCHER_EXECUTABLE,
                "hookLibraryName": loader.WINDOWS_HOOK_LIBRARY_NAME,
                "runtimeLoadReportSha256": "0" * 64,
                "verifiedAt": "2026-07-03T00:00:00Z",
            },
        }
        if is_windows:
            runtime_info["windowsRuntimeStatus"] = windows_runtime_status
        write_json(runtime_info_path, runtime_info)

        runtime_registration = {
            "id": "test-runtime-installed-hook",
            "runtimeStrategy": loader.RUNTIME_STRATEGY_INSTALLED_HOOK,
            "platform": loader.current_platform_id(),
            "launchAdapter": current_target.launch_adapter,
            "steamExecutable": str(steam_executable),
            "steamExecutableSha256": sha256(steam_executable),
            "hookLibrary": str(hook_library),
            "hookLibrarySha256": sha256(hook_library),
            "hookManifest": str(hook_manifest),
            "hookManifestSha256": sha256(hook_manifest),
            "runtimeInfo": str(runtime_info_path),
            "capabilities": runtime_info["capabilities"],
        }
        if launcher_executable is not None:
            runtime_registration["launcherExecutable"] = str(launcher_executable)
            runtime_registration["launcherExecutableSha256"] = sha256(launcher_executable)
            runtime_registration["windowsRuntimeStatus"] = windows_runtime_status

        registry_path = workspace / "runtime-registry.json"
        registry = {
            "schemaVersion": loader.SCHEMA_VERSION,
            "app": {"id": loader.APP_ID, "version": loader.APP_VERSION},
            "createdAt": "2026-07-03T00:00:00Z",
            "updatedAt": "2026-07-03T00:00:00Z",
            "runtimes": [runtime_registration],
        }
        write_json(registry_path, registry)
        return profile_dir, registry_path, hook_library

    def enable_package_for_profile(
        self,
        profile_dir: Path,
        package_dir: Path,
        *,
        package_path: Path | None = None,
        manifest_path: Path | None = None,
    ) -> dict[str, str]:
        manifest = json.loads((package_dir / loader.PACKAGE_MANIFEST_NAME).read_text(encoding="utf-8"))
        entry = {
            "id": manifest["id"],
            "version": manifest["version"],
            "packagePath": str((package_path or package_dir).resolve()),
            "enabledAt": "2026-07-03T00:00:00Z",
        }
        if manifest_path is not None:
            entry["manifestPath"] = str(manifest_path.resolve())
        profile_path = profile_dir / loader.APP_ID / "profile.json"
        profile = json.loads(profile_path.read_text(encoding="utf-8"))
        profile["activeMods"] = [entry]
        write_json(profile_path, profile)
        write_json(
            profile_dir / loader.APP_ID / "active-mods.json",
            {"schemaVersion": loader.SCHEMA_VERSION, "profileId": profile["profile"]["id"], "mods": [entry]},
        )
        return entry

    def test_package_validate_pack_and_install_happy_path(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            workspace = Path(temp_dir)
            package_dir = self.make_package(workspace)

            validate = self.run_cli("package", "validate", str(package_dir))
            self.assertEqual(validate.returncode, 0, validate.stdout)

            archive_path = workspace / "stash.bmlpkg"
            pack = self.run_cli("package", "pack", str(package_dir), "--out", str(archive_path))
            self.assertEqual(pack.returncode, 0, pack.stdout)
            self.assertTrue(archive_path.is_file())

            store_dir = workspace / "store"
            install = self.run_cli("package", "install", str(package_dir), "--store", str(store_dir))
            self.assertEqual(install.returncode, 0, install.stdout)
            installed = store_dir / "jml.stash" / "0.1.0"
            self.assertEqual((installed / "content" / "marker.txt").read_text(encoding="utf-8"), "installed\n")

            archive_store = workspace / "archive-store"
            archive_install = self.run_cli("package", "install", str(archive_path), "--store", str(archive_store))
            self.assertEqual(archive_install.returncode, 0, archive_install.stdout)
            self.assertEqual((archive_store / "jml.stash" / "0.1.0" / "content" / "marker.txt").read_text(encoding="utf-8"), "installed\n")

    def test_runebound_elixirs_capabilities_do_not_require_stash_modules(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            workspace = Path(temp_dir)
            package_dir = self.make_runebound_elixirs_package(workspace)

            validate = self.run_cli("package", "validate", str(package_dir))
            self.assertEqual(validate.returncode, 0, validate.stdout)
            self.assertNotIn("BML_PACKAGE_CAPABILITY_REQUIRED_MISSING", validate.stdout)
            self.assertNotIn("BML_PACKAGE_STASH_MODULE_MISSING", validate.stdout)

    def test_runebound_elixirs_rejects_missing_module(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            workspace = Path(temp_dir)
            package_dir = self.make_runebound_elixirs_package(workspace)
            manifest_path = package_dir / loader.PACKAGE_MANIFEST_NAME
            manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
            manifest["modules"] = {}
            write_json(manifest_path, manifest)

            validate = self.run_cli("package", "validate", str(package_dir))
            self.assertNotEqual(validate.returncode, 0, validate.stdout)
            self.assertIn("BML_PACKAGE_RUNEBOUND_ELIXIRS_MODULE_MISSING", validate.stdout)

    def test_runebound_elixirs_rejects_missing_data_file(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            workspace = Path(temp_dir)
            package_dir = self.make_runebound_elixirs_package(workspace)
            (package_dir / "content" / "data" / "bml" / "elixir-catalog.json").unlink()

            validate = self.run_cli("package", "validate", str(package_dir))
            self.assertNotEqual(validate.returncode, 0, validate.stdout)
            self.assertIn("BML_PACKAGE_RUNEBOUND_ELIXIRS_DATA_FILE_MISSING", validate.stdout)

    def test_runebound_elixirs_rejects_data_file_outside_bml_data_root(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            workspace = Path(temp_dir)
            package_dir = self.make_runebound_elixirs_package(workspace)
            manifest_path = package_dir / loader.PACKAGE_MANIFEST_NAME
            manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
            manifest["modules"]["runeboundElixirs"]["dataFiles"][0] = "content/elixir-catalog.json"
            write_json(manifest_path, manifest)
            write_json(package_dir / "content" / "elixir-catalog.json", {"schemaVersion": "0.1.0"})

            validate = self.run_cli("package", "validate", str(package_dir))
            self.assertNotEqual(validate.returncode, 0, validate.stdout)
            self.assertIn("BML_PACKAGE_RUNEBOUND_ELIXIRS_DATA_FILE_OUTSIDE_ROOT", validate.stdout)

    def test_runebound_elixirs_rejects_catalog_missing_tradeoff(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            workspace = Path(temp_dir)
            package_dir = self.make_runebound_elixirs_package(workspace)
            catalog_path = package_dir / "content" / "data" / "bml" / "elixir-catalog.json"
            catalog = json.loads(catalog_path.read_text(encoding="utf-8"))
            catalog["elixirs"][0].pop("tradeoffSummary")
            write_json(catalog_path, catalog)

            validate = self.run_cli("package", "validate", str(package_dir))
            self.assertNotEqual(validate.returncode, 0, validate.stdout)
            self.assertIn("BML_PACKAGE_RUNEBOUND_ELIXIR_TRADEOFF_MISSING", validate.stdout)

    def test_runebound_elixirs_rejects_unsupported_effect_opcode(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            workspace = Path(temp_dir)
            package_dir = self.make_runebound_elixirs_package(workspace)
            catalog_path = package_dir / "content" / "data" / "bml" / "elixir-catalog.json"
            catalog = json.loads(catalog_path.read_text(encoding="utf-8"))
            catalog["effects"][0]["opcode"] = "teleport_player"
            write_json(catalog_path, catalog)

            validate = self.run_cli("package", "validate", str(package_dir))
            self.assertNotEqual(validate.returncode, 0, validate.stdout)
            self.assertIn("BML_PACKAGE_RUNEBOUND_ELIXIR_EFFECT_OPCODE_UNSUPPORTED", validate.stdout)

    def test_runebound_elixirs_rejects_runtime_missing_required_capability(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            workspace = Path(temp_dir)
            package_dir = self.make_runebound_elixirs_package(workspace)
            package, package_result = loader.load_package(str(package_dir))
            self.assertTrue(package_result.ok, [problem.code for problem in package_result.problems])
            assert package is not None
            validation = loader.validate_package(package)
            self.assertTrue(validation.ok, [problem.code for problem in validation.problems])
            runtime_info = {
                "runtimeId": "runebound-test-runtime",
                "runtimeVersion": "0.1.0",
                "contract": {"id": loader.RUNTIME_CONTRACT_ID, "versions": [loader.RUNTIME_CONTRACT_VERSION]},
                "capabilities": [
                    {"id": capability, "version": "0.1.0"}
                    for capability in loader.CANONICAL_RUNEBOUND_ELIXIR_CAPABILITIES
                    if capability != "elixir_consumption"
                ],
            }

            result = loader.validate_runtime_info(runtime_info, package)
            self.assertFalse(result.ok)
            missing = [problem for problem in result.problems if problem.code == "BML_RUNTIME_CAPABILITY_MISSING"]
            self.assertEqual(len(missing), 1, [problem.code for problem in result.problems])
            self.assertEqual(missing[0].details["capability"], "elixir_consumption")

    def test_stash_package_still_requires_canonical_stash_capabilities(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            workspace = Path(temp_dir)
            package_dir = self.make_runebound_elixirs_package(workspace)
            manifest_path = package_dir / loader.PACKAGE_MANIFEST_NAME
            manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
            manifest["id"] = "jml.stash"
            manifest["name"] = "Stash"
            manifest["engine"]["capabilities"] = [
                {
                    "id": "persistent_storage",
                    "version": "0.1.0",
                    "required": True,
                    "reason": "Stash must keep canonical capability enforcement",
                }
            ]
            manifest["modules"] = {"runeboundElixirs": manifest["modules"]["runeboundElixirs"]}
            write_json(manifest_path, manifest)

            validate = self.run_cli("package", "validate", str(package_dir))
            self.assertNotEqual(validate.returncode, 0, validate.stdout)
            self.assertIn("BML_PACKAGE_CAPABILITY_REQUIRED_MISSING", validate.stdout)
            self.assertIn("BML_PACKAGE_STASH_MODULE_MISSING", validate.stdout)

    def test_directory_install_rejects_package_symlink(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            workspace = Path(temp_dir)
            package_dir = self.make_package(workspace)
            outside = workspace / "outside-secret.txt"
            outside.write_text("secret\n", encoding="utf-8")
            try:
                (package_dir / "content" / "leak.txt").symlink_to(outside)
            except OSError as exc:
                self.skipTest(f"package symlink security regression needs symlink privilege: {exc}")

            install = self.run_cli("package", "install", str(package_dir), "--store", str(workspace / "store"))
            self.assertNotEqual(install.returncode, 0, install.stdout)
            self.assertIn("Refusing to install package symlink", install.stdout)
            self.assertFalse((workspace / "store" / "jml.stash" / "0.1.0" / "content" / "leak.txt").exists())

    def test_launch_dry_run_strips_inherited_loader_environment(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            workspace = Path(temp_dir)
            package_dir = self.make_package(workspace)
            profile_dir, registry_path, hook_library = self.make_profile_and_registry(workspace, package_dir)
            self.enable_package_for_profile(profile_dir, package_dir)

            launch = self.run_cli(
                "launch",
                str(profile_dir),
                "--package",
                str(package_dir),
                "--registry",
                str(registry_path),
                "--dry-run",
                env={
                    "LD_PRELOAD": "/tmp/evil-preload.so",
                    "DYLD_INSERT_LIBRARIES": "/tmp/evil-dyld.dylib",
                    "LD_AUDIT": "/tmp/evil-audit.so",
                    "LD_LIBRARY_PATH": "/tmp/evil-lib",
                    "SteamAppId": "999999",
                    "SteamGameId": "999999",
                    "BML_STASH_PROFILE": "/tmp/evil-stash-profile",
                    "BML_HOOK_LIBRARY": "/tmp/evil-hook.so",
                    "BML_RUNTIME_MANIFEST": "/tmp/evil-runtime-manifest.json",
                },
            )
            self.assertEqual(launch.returncode, 0, launch.stdout)
            payload = json.loads(launch.stdout[launch.stdout.find("{") :])
            environment = payload["environment"]
            if loader.current_platform_target().os_name == "linux":
                self.assertEqual(environment.get("LD_PRELOAD"), str(hook_library))
            else:
                self.assertNotIn("LD_PRELOAD", environment)
            self.assertNotIn("/tmp/evil-preload.so", json.dumps(environment))
            self.assertNotIn("/tmp/evil-dyld.dylib", json.dumps(environment))
            self.assertNotIn("/tmp/evil-audit.so", json.dumps(environment))
            self.assertNotIn("/tmp/evil-lib", json.dumps(environment))
            self.assertNotIn("DYLD_INSERT_LIBRARIES", environment)
            self.assertNotIn("LD_AUDIT", environment)
            self.assertNotIn("LD_LIBRARY_PATH", environment)
            self.assertNotIn("SteamAppId", environment)
            self.assertNotIn("SteamGameId", environment)
            self.assertEqual(environment.get("BML_HOOK_LIBRARY"), str(hook_library))
            self.assertEqual(environment.get("BML_RUNTIME_MANIFEST"), payload["runtimeManifest"])
            self.assertNotIn("/tmp/evil-stash-profile", json.dumps(environment))
            self.assertNotIn("/tmp/evil-hook.so", json.dumps(environment))
            self.assertNotIn("/tmp/evil-runtime-manifest.json", json.dumps(environment))
            self.assertNotIn("BML_STASH_PROFILE", environment)

    def test_launch_rejects_disabled_package_when_profile_has_active_mod_state(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            workspace = Path(temp_dir)
            package_dir = self.make_package(workspace)
            profile_dir, registry_path, _hook_library = self.make_profile_and_registry(workspace, package_dir)
            write_json(profile_dir / loader.APP_ID / "active-mods.json", {"schemaVersion": loader.SCHEMA_VERSION, "mods": []})

            launch = self.run_cli(
                "launch",
                str(profile_dir),
                "--package",
                str(package_dir),
                "--registry",
                str(registry_path),
                "--dry-run",
            )
            self.assertNotEqual(launch.returncode, 0, launch.stdout)
            self.assertIn("BML_PROFILE_PACKAGE_DISABLED", launch.stdout)


    def test_launch_rejects_new_profile_empty_active_mods(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            workspace = Path(temp_dir)
            package_dir = self.make_package(workspace)
            profile_dir, registry_path, _hook_library = self.make_profile_and_registry(workspace, package_dir)

            launch = self.run_cli(
                "launch",
                str(profile_dir),
                "--package",
                str(package_dir),
                "--registry",
                str(registry_path),
                "--dry-run",
            )
            self.assertNotEqual(launch.returncode, 0, launch.stdout)
            self.assertIn("BML_PROFILE_PACKAGE_DISABLED", launch.stdout)

    def test_profile_disable_empty_profile_active_mods_ignores_stale_active_mods_file(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            workspace = Path(temp_dir)
            package_dir = self.make_package(workspace)
            profile_dir, _registry_path, _hook_library = self.make_profile_and_registry(workspace, package_dir)
            manifest = json.loads((package_dir / loader.PACKAGE_MANIFEST_NAME).read_text(encoding="utf-8"))
            stale_entry = {
                "id": manifest["id"],
                "version": manifest["version"],
                "packagePath": str(package_dir.resolve()),
                "enabledAt": "2026-07-03T00:00:00Z",
            }
            write_json(
                profile_dir / loader.APP_ID / "active-mods.json",
                {"schemaVersion": loader.SCHEMA_VERSION, "profileId": "security-test", "mods": [stale_entry]},
            )

            disable = self.run_cli("profile", "disable", str(profile_dir), "--mod-id", "unrelated")

            self.assertEqual(disable.returncode, 0, disable.stdout)
            payload = json.loads(disable.stdout)
            self.assertEqual(payload["removed"], 0)
            profile = json.loads((profile_dir / loader.APP_ID / "profile.json").read_text(encoding="utf-8"))
            active_mods = json.loads((profile_dir / loader.APP_ID / "active-mods.json").read_text(encoding="utf-8"))
            self.assertEqual(profile["activeMods"], [])
            self.assertEqual(active_mods["mods"], [])

    def test_launch_rejects_enabled_package_path_mismatch(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            workspace = Path(temp_dir)
            package_dir = self.make_package(workspace, "requested-package")
            enabled_dir = self.make_package(workspace, "enabled-package")
            profile_dir, registry_path, _hook_library = self.make_profile_and_registry(workspace, package_dir)
            self.enable_package_for_profile(profile_dir, enabled_dir)

            launch = self.run_cli(
                "launch",
                str(profile_dir),
                "--package",
                str(package_dir),
                "--registry",
                str(registry_path),
                "--dry-run",
            )
            self.assertNotEqual(launch.returncode, 0, launch.stdout)
            self.assertIn("BML_PROFILE_PACKAGE_PATH_MISMATCH", launch.stdout)
            stdout_for_paths = launch.stdout.replace("\\\\", "\\")
            self.assertIn(str(enabled_dir.resolve()), stdout_for_paths)
            self.assertIn(str(package_dir.resolve()), stdout_for_paths)

    def test_package_validate_rejects_missing_or_uninstallable_asset_references(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            workspace = Path(temp_dir)
            package_dir = self.make_package(workspace)
            manifest_path = package_dir / loader.PACKAGE_MANIFEST_NAME
            manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
            manifest["assets"] = {
                "icon": "preview.png",
                "previewImages": ["assets/missing-preview.png"],
                "readme": "assets/README.md",
            }
            write_json(manifest_path, manifest)
            (package_dir / "preview.png").write_text("workshop preview only\n", encoding="utf-8")
            (package_dir / "assets").mkdir()

            validate = self.run_cli("package", "validate", str(package_dir))
            self.assertNotEqual(validate.returncode, 0, validate.stdout)
            self.assertIn("BML_PACKAGE_ASSET_REFERENCE_OUTSIDE_INSTALLABLE_ROOT", validate.stdout)
            self.assertIn("BML_PACKAGE_ASSET_REFERENCE_MISSING", validate.stdout)
            self.assertIn("assets.icon", validate.stdout)
            self.assertIn("assets.previewImages[0]", validate.stdout)
            self.assertIn("assets.readme", validate.stdout)


    def test_windows_platform_target_uses_barony_exe_and_normalized_windows_id(self) -> None:
        with self.simulated_platform("win32", "AMD64"):
            target = loader.current_platform_target()

            self.assertEqual(target.os_name, "windows")
            self.assertEqual(target.executable_name, "barony.exe")
            self.assertEqual(target.hook_artifact_extension, ".dll")
            self.assertEqual(target.launch_adapter, loader.LAUNCH_ADAPTER_WINDOWS_CREATEPROCESS_LOADLIBRARY)
            self.assertEqual(loader.current_platform_id(), "windows-x86_64")
            self.assertEqual(target.platform_id("x64"), "windows-x86_64")
            self.assertEqual(target.platform_id("x86_64"), "windows-x86_64")
            self.assertEqual(target.platform_id("ARM64"), "windows-arm64")
            self.assertEqual(target.platform_id("aarch64"), "windows-arm64")

    def test_windows_steam_discovery_uses_program_files_and_library_root(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            workspace = Path(temp_dir)
            program_files = workspace / "Program Files (x86)"
            steam_root = program_files / "Steam"
            steamapps = steam_root / "steamapps"
            install_path = steamapps / "common" / "Barony"
            install_path.mkdir(parents=True)
            executable = install_path / "barony.exe"
            executable.write_bytes(b"fake windows executable v5.0.0\n")
            manifest = steamapps / loader.STEAM_MANIFEST_NAME
            manifest.write_text(
                '"appid" "371970"\n'
                '"name" "Barony"\n'
                '"installdir" "Barony"\n'
                '"buildid" "123456"\n',
                encoding="utf-8",
            )

            with self.simulated_platform("win32", "AMD64", {"ProgramFiles(x86)": str(program_files)}):
                detected, result = loader.detect_steam_install()
                self.assertTrue(result.ok, [problem.code for problem in result.problems])
                self.assertIsNotNone(detected)
                assert detected is not None
                self.assertEqual(detected["platform"], "windows-x86_64")
                self.assertEqual(detected["executableName"], "barony.exe")
                self.assertEqual(detected["launchAdapter"], loader.LAUNCH_ADAPTER_WINDOWS_CREATEPROCESS_LOADLIBRARY)
                self.assertEqual(Path(detected["executable"]), executable.resolve())
                self.assertEqual(Path(detected["manifestPath"]), manifest.resolve())

                from_library_root, library_result = loader.detect_steam_install(install_arg=str(steam_root))
                self.assertTrue(library_result.ok, [problem.code for problem in library_result.problems])
                self.assertIsNotNone(from_library_root)
                assert from_library_root is not None
                self.assertEqual(Path(from_library_root["installPath"]), install_path.resolve())
                self.assertEqual(Path(from_library_root["executable"]), executable.resolve())

    def test_windows_runtime_requires_adapter_launcher_and_verified_status(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            workspace = Path(temp_dir)
            package_dir = self.make_package(workspace)
            package, package_result = loader.load_package(str(package_dir))
            self.assertTrue(package_result.ok, [problem.code for problem in package_result.problems])
            assert package is not None

            profile_dir = workspace / "profile"
            bml_root = profile_dir / loader.APP_ID
            bml_root.mkdir(parents=True)
            profile = {
                "schemaVersion": loader.SCHEMA_VERSION,
                "profile": {"id": "windows-contract-test"},
                "runtime": {"gameSource": "manual", "baronyExecutable": str(workspace / "runtime" / "barony.exe"), "steam": None},
                "activeMods": [],
            }

            runtime_dir = workspace / "runtime"
            runtime_dir.mkdir()
            steam_executable = runtime_dir / "barony.exe"
            hook_library = runtime_dir / loader.WINDOWS_HOOK_LIBRARY_NAME
            hook_manifest = runtime_dir / "hook-manifest.json"
            launcher_executable = runtime_dir / loader.WINDOWS_LAUNCHER_EXECUTABLE
            runtime_info_path = runtime_dir / "runtime-info.json"
            steam_executable.write_bytes(b"fake windows executable v5.0.0\n")
            hook_library.write_bytes(b"fake dll\n")
            hook_manifest.write_text('{"hook":"manifest"}\n', encoding="utf-8")
            launcher_executable.write_bytes(b"fake launcher\n")
            wrong_steam_executable = runtime_dir / "not-barony.exe"
            wrong_steam_executable.write_bytes(b"fake wrong windows executable v5.0.0\n")
            wrong_hook_library = runtime_dir / "not_barony_bml.dll"
            wrong_hook_library.write_bytes(b"fake wrong dll\n")
            wrong_launcher_executable = runtime_dir / "not-bml-win-launcher.exe"
            wrong_launcher_executable.write_bytes(b"fake wrong launcher\n")

            package_manifest = json.loads((package_dir / loader.PACKAGE_MANIFEST_NAME).read_text(encoding="utf-8"))
            runtime_info = {
                "runtimeId": "windows-test-runtime",
                "runtimeVersion": "0.1.0",
                "contract": {"id": loader.RUNTIME_CONTRACT_ID, "versions": [loader.RUNTIME_CONTRACT_VERSION]},
                "platforms": [{"platform": "windows-x86_64"}],
                "capabilities": [
                    {"id": cap["id"], "version": cap.get("version", "0.1.0")}
                    for cap in package_manifest["engine"]["capabilities"]
                ],
            }
            write_json(runtime_info_path, runtime_info)
            base_runtime = {
                "id": "windows-runtime",
                "runtimeStrategy": loader.RUNTIME_STRATEGY_INSTALLED_HOOK,
                "platform": "windows-x86_64",
                "platformTarget": "windows",
                "hookArtifactExtension": ".dll",
                "steamExecutable": str(steam_executable),
                "steamExecutableSha256": sha256(steam_executable),
                "hookLibrary": str(hook_library),
                "hookLibrarySha256": sha256(hook_library),
                "hookManifest": str(hook_manifest),
                "hookManifestSha256": sha256(hook_manifest),
                "runtimeInfo": str(runtime_info_path),
                "capabilities": runtime_info["capabilities"],
            }

            with self.simulated_platform("win32", "AMD64"):
                _runtime_info, _runtime_info_path, _launch_executable, missing_result = loader.validate_registered_runtime(
                    base_runtime,
                    profile,
                    package,
                )
                missing_codes = {problem.code for problem in missing_result.problems}
                self.assertIn("BML_REGISTERED_RUNTIME_LAUNCH_ADAPTER_MISSING", missing_codes)
                self.assertIn("BML_REGISTERED_RUNTIME_LAUNCHER_EXECUTABLE_MISSING", missing_codes)
                self.assertIn("BML_REGISTERED_RUNTIME_WINDOWS_VERIFICATION_MISSING", missing_codes)

                fake_runtime = {
                    **base_runtime,
                    "launchAdapter": loader.LAUNCH_ADAPTER_WINDOWS_CREATEPROCESS_LOADLIBRARY,
                    "launcherExecutable": str(launcher_executable),
                    "launcherExecutableSha256": sha256(launcher_executable),
                }
                _fake_info, _fake_info_path, _fake_launch_executable, fake_result = loader.validate_registered_runtime(
                    fake_runtime,
                    profile,
                    package,
                )
                fake_codes = {problem.code for problem in fake_result.problems}
                bare_verified_runtime = {
                    **fake_runtime,
                    "windowsRuntimeStatus": {"status": "verified"},
                }
                _bare_info, _bare_info_path, _bare_launch_executable, bare_result = loader.validate_registered_runtime(
                    bare_verified_runtime,
                    profile,
                    package,
                )
                bare_codes = {problem.code for problem in bare_result.problems}
                self.assertFalse(bare_result.ok)
                self.assertIn("BML_REGISTERED_RUNTIME_WINDOWS_VERIFICATION_MISSING", bare_codes)
                scaffold_self_test_runtime = {
                    **fake_runtime,
                    "windowsRuntimeStatus": {
                        "status": "verified",
                        "evidence": {
                            "evidenceKind": "scaffold-build-self-test",
                            "platform": "windows-x86_64",
                            "hostOs": "windows",
                            "gameExecutableName": "barony.exe",
                            "hookLibraryName": loader.WINDOWS_HOOK_LIBRARY_NAME,
                            "launcherExecutableName": loader.WINDOWS_LAUNCHER_EXECUTABLE,
                            "runtimeLoadReportSha256": "b" * 64,
                            "selfTestReportSha256": "c" * 64,
                            "verifiedAt": "2026-07-05T00:00:00Z",
                        },
                    },
                }
                _self_test_info, _self_test_info_path, _self_test_launch_executable, self_test_result = loader.validate_registered_runtime(
                    scaffold_self_test_runtime,
                    profile,
                    package,
                )
                self_test_codes = {problem.code for problem in self_test_result.problems}
                self.assertFalse(self_test_result.ok)
                self.assertIn("BML_REGISTERED_RUNTIME_WINDOWS_VERIFICATION_MISSING", self_test_codes)


                self.assertFalse(fake_result.ok)
                self.assertIn("BML_REGISTERED_RUNTIME_WINDOWS_VERIFICATION_MISSING", fake_codes)

                wrong_target_runtime = {
                    **fake_runtime,
                    "steamExecutable": str(wrong_steam_executable),
                    "steamExecutableSha256": sha256(wrong_steam_executable),
                }
                _wrong_info, _wrong_info_path, _wrong_launch_executable, wrong_target_result = loader.validate_registered_runtime(
                    wrong_target_runtime,
                    profile,
                    package,
                )
                wrong_target_codes = {problem.code for problem in wrong_target_result.problems}
                self.assertFalse(wrong_target_result.ok)
                self.assertIn("BML_REGISTERED_RUNTIME_STEAM_EXECUTABLE_NAME_MISMATCH", wrong_target_codes)
                wrong_hook_runtime = {
                    **fake_runtime,
                    "hookLibrary": str(wrong_hook_library),
                    "hookLibrarySha256": sha256(wrong_hook_library),
                }
                _wrong_hook_info, _wrong_hook_info_path, _wrong_hook_launch_executable, wrong_hook_result = loader.validate_registered_runtime(
                    wrong_hook_runtime,
                    profile,
                    package,
                )
                wrong_hook_codes = {problem.code for problem in wrong_hook_result.problems}
                self.assertFalse(wrong_hook_result.ok)
                self.assertIn("BML_REGISTERED_RUNTIME_HOOK_LIBRARY_NAME_MISMATCH", wrong_hook_codes)
                wrong_launcher_runtime = {
                    **fake_runtime,
                    "launcherExecutable": str(wrong_launcher_executable),
                    "launcherExecutableSha256": sha256(wrong_launcher_executable),
                }
                (
                    _wrong_launcher_info,
                    _wrong_launcher_info_path,
                    _wrong_launcher_launch_executable,
                    wrong_launcher_result,
                ) = loader.validate_registered_runtime(
                    wrong_launcher_runtime,
                    profile,
                    package,
                )
                wrong_launcher_codes = {problem.code for problem in wrong_launcher_result.problems}
                self.assertFalse(wrong_launcher_result.ok)
                self.assertIn("BML_REGISTERED_RUNTIME_LAUNCHER_EXECUTABLE_NAME_MISMATCH", wrong_launcher_codes)



                verified_runtime = {
                    **fake_runtime,
                    "windowsRuntimeStatus": {
                        "status": "verified",
                        "evidence": {
                            "evidenceKind": loader.WINDOWS_LIVE_RUNTIME_EVIDENCE_KIND,
                            "platform": "windows-x86_64",
                            "hostOs": "windows",
                            "gameExecutableName": "barony.exe",
                            "hookLibraryName": loader.WINDOWS_HOOK_LIBRARY_NAME,
                            "launcherExecutableName": loader.WINDOWS_LAUNCHER_EXECUTABLE,
                            "runtimeLoadReportSha256": "a" * 64,
                            "verifiedAt": "2026-07-05T00:00:00Z",
                        },
                    },
                }
                selected_info, selected_info_path, launch_executable, valid_result = loader.validate_registered_runtime(
                    verified_runtime,
                    profile,
                    package,
                )
                self.assertTrue(valid_result.ok, [problem.code for problem in valid_result.problems])
                self.assertEqual(selected_info, runtime_info)
                self.assertEqual(selected_info_path, runtime_info_path.resolve())
                self.assertEqual(launch_executable, launcher_executable.resolve())

                out_path = bml_root / "runtime-manifest.json"
                manifest_payload, _active_mods_path = loader.write_launch_artifacts(
                    profile,
                    profile_dir,
                    package,
                    runtime_info,
                    out_path,
                    launch_executable,
                    verified_runtime,
                )
                launch = manifest_payload["launch"]
                self.assertEqual(launch["launchAdapter"], loader.LAUNCH_ADAPTER_WINDOWS_CREATEPROCESS_LOADLIBRARY)
                self.assertEqual(launch["launcherExecutable"], str(launcher_executable))
                self.assertEqual(launch["hookLibrary"], str(hook_library))
                self.assertEqual(launch["steamExecutable"], str(steam_executable))
                self.assertEqual(launch["launchExecutable"], str(launcher_executable.resolve()))

                environment = loader.launch_environment(profile, profile_dir, out_path, verified_runtime)
                self.assertEqual(environment["BML_LAUNCH_ADAPTER"], loader.LAUNCH_ADAPTER_WINDOWS_CREATEPROCESS_LOADLIBRARY)
                self.assertEqual(environment["BML_TARGET_EXECUTABLE"], str(steam_executable))
                self.assertEqual(environment["BML_LAUNCHER_EXECUTABLE"], str(launcher_executable))
                self.assertEqual(environment["BML_HOOK_LIBRARY"], str(hook_library))
                self.assertNotIn("LD_PRELOAD", environment)

    def test_steam_client_process_detection_uses_proc_metadata(self) -> None:
        with self.simulated_platform("linux", "x86_64"):
            with tempfile.TemporaryDirectory() as temp_dir:
                proc_root = Path(temp_dir)
                self.assertFalse(loader.steam_client_process_running(proc_root))

                steam_proc = proc_root / "1234"
                steam_proc.mkdir()
                (steam_proc / "comm").write_text("steam\n", encoding="utf-8")
                (steam_proc / "cmdline").write_bytes(b"/home/jerry/.local/share/Steam/ubuntu12_32/steam\0-silent\0")
                self.assertTrue(loader.steam_client_process_running(proc_root))

    def test_steam_launch_preflight_blocks_missing_client(self) -> None:
        with self.simulated_platform("linux", "x86_64"):
            with tempfile.TemporaryDirectory() as temp_dir:
                proc_root = Path(temp_dir)
                original = loader.steam_client_process_running
                try:
                    loader.steam_client_process_running = lambda: original(proc_root)
                    result = loader.validate_steam_client_ready_for_launch({"runtime": {"steam": {"appId": loader.STEAM_BARONY_APP_ID}}})
                finally:
                    loader.steam_client_process_running = original
                self.assertFalse(result.ok)
                self.assertEqual(result.problems[0].code, "BML_STEAM_CLIENT_NOT_RUNNING")



if __name__ == "__main__":
    unittest.main()
