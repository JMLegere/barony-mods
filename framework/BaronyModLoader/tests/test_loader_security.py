from __future__ import annotations

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

    def make_package(self, workspace: Path, name: str = "package") -> Path:
        package_dir = workspace / name
        (package_dir / "content").mkdir(parents=True)
        shutil.copy2(EXAMPLE_PACKAGE, package_dir / loader.PACKAGE_MANIFEST_NAME)
        (package_dir / "content" / "marker.txt").write_text("installed\n", encoding="utf-8")
        return package_dir

    def make_profile_and_registry(self, workspace: Path, package_dir: Path) -> tuple[Path, Path, Path]:
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
            "runtime": {"gameSource": "manual", "baronyExecutable": str(workspace / "barony.x86_64"), "runtimeInfo": None, "steam": None},
        }
        write_json(bml_root / "profile.json", profile)

        runtime_dir = workspace / "runtime"
        runtime_dir.mkdir()
        steam_executable = runtime_dir / "barony.x86_64"
        hook_library = runtime_dir / "libbarony_bml.so"
        hook_manifest = runtime_dir / "hook-manifest.json"
        runtime_info_path = runtime_dir / "runtime-info.json"
        steam_executable.write_bytes(b"fake executable v5.0.0\n")
        hook_library.write_bytes(b"fake hook\n")
        write_json(hook_manifest, {"hook": "manifest"})

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
        write_json(runtime_info_path, runtime_info)

        registry_path = workspace / "runtime-registry.json"
        registry = {
            "schemaVersion": loader.SCHEMA_VERSION,
            "app": {"id": loader.APP_ID, "version": loader.APP_VERSION},
            "createdAt": "2026-07-03T00:00:00Z",
            "updatedAt": "2026-07-03T00:00:00Z",
            "runtimes": [
                {
                    "id": "test-runtime-installed-hook",
                    "runtimeStrategy": loader.RUNTIME_STRATEGY_INSTALLED_HOOK,
                    "platform": loader.current_platform_id(),
                    "steamExecutable": str(steam_executable),
                    "steamExecutableSha256": sha256(steam_executable),
                    "hookLibrary": str(hook_library),
                    "hookLibrarySha256": sha256(hook_library),
                    "hookManifest": str(hook_manifest),
                    "hookManifestSha256": sha256(hook_manifest),
                    "runtimeInfo": str(runtime_info_path),
                    "capabilities": runtime_info["capabilities"],
                }
            ],
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

    def test_directory_install_rejects_package_symlink(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            workspace = Path(temp_dir)
            package_dir = self.make_package(workspace)
            outside = workspace / "outside-secret.txt"
            outside.write_text("secret\n", encoding="utf-8")
            (package_dir / "content" / "leak.txt").symlink_to(outside)

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
            self.assertEqual(environment.get("LD_PRELOAD"), str(hook_library))
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
            self.assertIn(str(enabled_dir.resolve()), launch.stdout)
            self.assertIn(str(package_dir.resolve()), launch.stdout)

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

    def test_steam_client_process_detection_uses_proc_metadata(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            proc_root = Path(temp_dir)
            self.assertFalse(loader.steam_client_process_running(proc_root))

            steam_proc = proc_root / "1234"
            steam_proc.mkdir()
            (steam_proc / "comm").write_text("steam\n", encoding="utf-8")
            (steam_proc / "cmdline").write_bytes(b"/home/jerry/.local/share/Steam/ubuntu12_32/steam\0-silent\0")
            self.assertTrue(loader.steam_client_process_running(proc_root))

    def test_steam_launch_preflight_blocks_missing_client(self) -> None:
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
