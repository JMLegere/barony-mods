from __future__ import annotations

import importlib.util
import json
import sys
import tempfile
import unittest
from pathlib import Path

APP_PATH = Path(__file__).resolve().parents[1] / "app" / "barony_mod_loader.py"
SCHEMA_PATH = Path(__file__).resolve().parents[1] / "schema" / "package.schema.json"


def load_app_module():
    spec = importlib.util.spec_from_file_location("barony_mod_loader_package_tests", APP_PATH)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"could not load app module from {APP_PATH}")
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


def write_package(root: Path, package_id: str, kind: str, capabilities: list[dict[str, object]]) -> Path:
    root.mkdir(parents=True, exist_ok=True)
    path = root / "bml-package.json"
    payload = {
        "formatVersion": "0.1.0",
        "id": package_id,
        "name": package_id,
        "version": "0.1.0",
        "kind": kind,
        "summary": "test package",
        "authors": [{"name": "tester"}],
        "layout": {
            "contentRoot": "content/",
            "assetRoot": "assets/",
            "nativeRoot": "native/",
            "migrationRoot": "migrations/",
        },
        "barony": {"supportedGameVersions": ["5.x"], "upstreamRepository": "https://github.com/TurningWheel/Barony"},
        "framework": {
            "id": "BaronyModLoader",
            "minimumAppVersion": "0.1.0",
            "packageSchema": "bml-package@0.1.0",
            "runtimeContract": "bml-runtime-contract@0.1.0",
        },
        "engine": {
            "runtimeContract": "bml-runtime-contract@0.1.0",
            "minimumRuntimeVersion": "0.1.0",
            "capabilities": capabilities,
        },
        "content": {"entries": []},
        "modules": {},
    }
    path.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")
    return path


def write_runtime_info(path: Path, capability_ids: list[str], *, windows_noop: bool) -> Path:
    payload = {
        "runtimeId": "runtime-under-test",
        "runtimeVersion": "0.1.0",
        "contractVersions": ["0.1.0"],
        "platforms": [{"platform": "windows-x86_64"}],
        "capabilities": [{"id": capability_id, "version": "0.1.0"} for capability_id in capability_ids],
        "policy": {
            "runtimeStrategy": "installed-binary-hook",
            "requiresValidatedManifest": True,
            "rejectsUnknownRequiredCapabilities": True,
            "partialStashLoadAllowed": False,
        },
    }
    if windows_noop:
        payload["policy"]["windowsSupportLevel"] = "noop-runtime-load"
    path.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")
    return path


class PackageCrossPlatformTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.app = load_app_module()

    def test_generic_package_allows_capability_subset(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            package_path = write_package(
                Path(tmpdir),
                "jml.example_subset",
                "gameplay-mod",
                [{"id": "persistent_storage", "version": "0.1.0", "required": True}],
            )
            package, load_result = self.app.load_package(str(package_path.parent))
            self.assertTrue(load_result.ok, [problem.code for problem in load_result.problems])
            result = self.app.validate_package(package)
            self.assertTrue(result.ok, [problem.code for problem in result.problems])

    def test_runtime_load_smoke_capability_must_stand_alone(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            package_path = write_package(
                Path(tmpdir),
                "jml.bad_smoke_mix",
                "gameplay-mod",
                [
                    {"id": "runtime_load_smoke", "version": "0.1.0", "required": True},
                    {"id": "persistent_storage", "version": "0.1.0", "required": True},
                ],
            )
            package, load_result = self.app.load_package(str(package_path.parent))
            self.assertTrue(load_result.ok, [problem.code for problem in load_result.problems])
            result = self.app.validate_package(package)
            self.assertIn(
                "BML_PACKAGE_DIAGNOSTIC_CAPABILITY_RESERVED",
                [problem.code for problem in result.problems],
            )

    def test_runtime_load_smoke_package_can_opt_in_without_platform_metadata(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            package_path = write_package(
                Path(tmpdir),
                "jml.example_smoke_only",
                "runtime-support",
                [{"id": "runtime_load_smoke", "version": "0.1.0", "required": True}],
            )
            package, load_result = self.app.load_package(str(package_path.parent))
            self.assertTrue(load_result.ok, [problem.code for problem in load_result.problems])
            result = self.app.validate_package(package)
            self.assertTrue(result.ok, [problem.code for problem in result.problems])

    def test_windows_noop_runtime_rejects_normal_mod_package(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            package_path = write_package(
                Path(tmpdir) / "package",
                "jml.example_normal",
                "gameplay-mod",
                [{"id": "persistent_storage", "version": "0.1.0", "required": True}],
            )
            runtime_info_path = write_runtime_info(Path(tmpdir) / "runtime-info.json", ["runtime_load_smoke"], windows_noop=True)
            package, load_result = self.app.load_package(str(package_path.parent))
            self.assertTrue(load_result.ok, [problem.code for problem in load_result.problems])
            runtime_info, _, runtime_load_result = self.app.load_runtime_info(str(runtime_info_path))
            self.assertTrue(runtime_load_result.ok, [problem.code for problem in runtime_load_result.problems])
            result = self.app.validate_runtime_info(runtime_info, package)
            self.assertIn(
                "BML_RUNTIME_NOOP_PACKAGE_UNSUPPORTED",
                [problem.code for problem in result.problems],
            )

    def test_windows_noop_runtime_accepts_smoke_package(self):
        fixtures_dir = Path(__file__).resolve().parents[1] / "fixtures"
        package, load_result = self.app.load_package(str(fixtures_dir / "windows-smoke-package"))
        self.assertTrue(load_result.ok, [problem.code for problem in load_result.problems])
        runtime_info, _, runtime_load_result = self.app.load_runtime_info(
            str(fixtures_dir / "runtime-info.installed-hook.windows-noop.json")
        )
        self.assertTrue(runtime_load_result.ok, [problem.code for problem in runtime_load_result.problems])
        result = self.app.validate_runtime_info(runtime_info, package)
        self.assertTrue(result.ok, [problem.code for problem in result.problems])


    def test_windows_stash_runtime_accepts_stash_package(self):
        fixtures_dir = Path(__file__).resolve().parents[1] / "fixtures"
        package, load_result = self.app.load_package(str(Path(__file__).resolve().parents[3] / "mods" / "stash"))
        self.assertTrue(load_result.ok, [problem.code for problem in load_result.problems])
        runtime_info, _, runtime_load_result = self.app.load_runtime_info(
            str(fixtures_dir / "runtime-info.installed-hook.windows-stash.json")
        )
        self.assertTrue(runtime_load_result.ok, [problem.code for problem in runtime_load_result.problems])
        result = self.app.validate_runtime_info(runtime_info, package)
        self.assertTrue(result.ok, [problem.code for problem in result.problems])
    def test_windows_noop_smoke_package_sets_launch_override(self):
        package, load_result = self.app.load_package(str((Path(__file__).resolve().parents[1] / "fixtures" / "windows-smoke-package")))
        self.assertTrue(load_result.ok, [problem.code for problem in load_result.problems])
        env: dict[str, str] = {}
        self.app.apply_package_launch_overrides(env, package.manifest)
        self.assertEqual(env.get("BML_VALIDATE_INJECTION_ONLY"), "1")

    def test_package_rejects_native_libraries_legacy_shape(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            package_path = write_package(
                Path(tmpdir),
                "jml.bad_native_libraries",
                "gameplay-mod",
                [{"id": "persistent_storage", "version": "0.1.0", "required": True}],
            )
            payload = json.loads(package_path.read_text(encoding="utf-8"))
            payload["native"] = {"libraries": []}
            package_path.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")
            package, load_result = self.app.load_package(str(package_path.parent))
            self.assertTrue(load_result.ok, [problem.code for problem in load_result.problems])
            result = self.app.validate_package(package)
            self.assertIn("BML_PACKAGE_NATIVE_LEGACY_FIELD_FORBIDDEN", [problem.code for problem in result.problems])

    def test_package_rejects_platform_specific_native_builds(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            package_path = write_package(
                Path(tmpdir),
                "jml.bad_native_build",
                "gameplay-mod",
                [{"id": "persistent_storage", "version": "0.1.0", "required": True}],
            )
            payload = json.loads(package_path.read_text(encoding="utf-8"))
            payload["native"] = {
                "builds": [
                    {
                        "platform": "linux-x86_64",
                        "artifact": "native/release/linux-x86_64/barony"
                    }
                ]
            }
            package_path.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")
            package, load_result = self.app.load_package(str(package_path.parent))
            self.assertTrue(load_result.ok, [problem.code for problem in load_result.problems])
            result = self.app.validate_package(package)
            codes = [problem.code for problem in result.problems]
            self.assertIn("BML_PACKAGE_NATIVE_LEGACY_FIELD_FORBIDDEN", codes)
            self.assertIn("BML_PACKAGE_PLATFORM_METADATA_FORBIDDEN", codes)

    def test_package_rejects_platform_specific_build_outputs(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            package_path = write_package(
                Path(tmpdir),
                "jml.bad_build_outputs",
                "gameplay-mod",
                [{"id": "persistent_storage", "version": "0.1.0", "required": True}],
            )
            payload = json.loads(package_path.read_text(encoding="utf-8"))
            payload["build"] = {
                "outputs": [
                    {
                        "platform": "windows-x86_64",
                        "path": "native/release/windows-x86_64/barony.exe"
                    }
                ]
            }
            package_path.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")
            package, load_result = self.app.load_package(str(package_path.parent))
            self.assertTrue(load_result.ok, [problem.code for problem in load_result.problems])
            result = self.app.validate_package(package)
            codes = [problem.code for problem in result.problems]
            self.assertIn("BML_PACKAGE_BUILD_OUTPUTS_FORBIDDEN", codes)
            self.assertIn("BML_PACKAGE_PLATFORM_METADATA_FORBIDDEN", codes)

    def test_package_rejects_top_level_hook_metadata(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            package_path = write_package(
                Path(tmpdir),
                "jml.bad_hook_metadata",
                "gameplay-mod",
                [{"id": "persistent_storage", "version": "0.1.0", "required": True}],
            )
            payload = json.loads(package_path.read_text(encoding="utf-8"))
            payload["hook"] = {"libraryName": "bml_hook.dll"}
            package_path.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")
            package, load_result = self.app.load_package(str(package_path.parent))
            self.assertTrue(load_result.ok, [problem.code for problem in load_result.problems])
            result = self.app.validate_package(package)
            self.assertIn("BML_PACKAGE_PLATFORM_METADATA_FORBIDDEN", [problem.code for problem in result.problems])

    def test_package_rejects_nested_platform_metadata(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            package_path = write_package(
                Path(tmpdir),
                "jml.bad_nested_platform_metadata",
                "gameplay-mod",
                [{"id": "persistent_storage", "version": "0.1.0", "required": True}],
            )
            payload = json.loads(package_path.read_text(encoding="utf-8"))
            payload["engine"]["platform"] = "windows-x86_64"
            payload["barony"]["steamBuildId"] = "22630456"
            payload["source"] = {"artifact": "native/release/windows-x86_64/barony.exe"}
            package_path.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")
            package, load_result = self.app.load_package(str(package_path.parent))
            self.assertTrue(load_result.ok, [problem.code for problem in load_result.problems])
            result = self.app.validate_package(package)
            codes = [problem.code for problem in result.problems]
            self.assertGreaterEqual(codes.count("BML_PACKAGE_PLATFORM_METADATA_FORBIDDEN"), 3)

    def test_package_allows_module_placement_hook_names(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            package_path = write_package(
                Path(tmpdir),
                "jml.good_placement_hook_name",
                "gameplay-mod",
                [{"id": "persistent_storage", "version": "0.1.0", "required": True}],
            )
            payload = json.loads(package_path.read_text(encoding="utf-8"))
            payload["modules"] = {"placements": [{"id": "entry", "hook": "lobby_assist_area"}]}
            package_path.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")
            package, load_result = self.app.load_package(str(package_path.parent))
            self.assertTrue(load_result.ok, [problem.code for problem in load_result.problems])
            result = self.app.validate_package(package)
            self.assertNotIn("BML_PACKAGE_PLATFORM_METADATA_FORBIDDEN", [problem.code for problem in result.problems])

    def test_schema_marks_runtime_metadata_fields_forbidden(self):
        schema = json.loads(SCHEMA_PATH.read_text(encoding="utf-8"))
        forbidden_keys = ("hook", "platform", "steamBuildId", "artifact")
        for key in forbidden_keys:
            self.assertIs(schema["properties"][key], False)
            self.assertIs(schema["properties"]["native"]["properties"][key], False)
            self.assertIs(schema["properties"]["build"]["properties"][key], False)
            self.assertIs(schema["properties"]["native"]["properties"]["runtimeRequirements"]["properties"][key], False)
            self.assertIs(schema["properties"]["native"]["properties"]["sourceReferences"]["items"]["properties"][key], False)
        self.assertNotIn("hook", schema["properties"]["modules"].get("properties", {}))


if __name__ == "__main__":
    unittest.main()
