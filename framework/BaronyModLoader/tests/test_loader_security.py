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

    def active_mod_entry_for_package(
        self,
        package_dir: Path,
        *,
        package_path: Path | None = None,
        manifest_path: Path | None = None,
        load_order: int | None = None,
    ) -> dict:
        manifest = json.loads((package_dir / loader.PACKAGE_MANIFEST_NAME).read_text(encoding="utf-8"))
        entry = {
            "id": manifest["id"],
            "version": manifest["version"],
            "packagePath": str((package_path or package_dir).resolve()),
            "enabledAt": "2026-07-03T00:00:00Z",
        }
        if manifest_path is not None:
            entry["manifestPath"] = str(manifest_path.resolve())
        if load_order is not None:
            entry["loadOrder"] = load_order
        return entry

    def write_profile_active_mods(self, profile_dir: Path, mods: list[dict]) -> None:
        profile_path = profile_dir / loader.APP_ID / "profile.json"
        profile = json.loads(profile_path.read_text(encoding="utf-8"))
        profile["activeMods"] = mods
        write_json(profile_path, profile)
        write_json(
            profile_dir / loader.APP_ID / "active-mods.json",
            {"schemaVersion": loader.SCHEMA_VERSION, "profileId": profile["profile"]["id"], "mods": mods},
        )

    def set_package_identity(
        self,
        package_dir: Path,
        package_id: str,
        *,
        version: str = "0.1.0",
        name: str | None = None,
    ) -> None:
        manifest_path = package_dir / loader.PACKAGE_MANIFEST_NAME
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
        manifest["id"] = package_id
        manifest["version"] = version
        manifest["name"] = name or package_id
        write_json(manifest_path, manifest)

    def package_dependency(self, package_id: str, *, required: bool = True, version: str = ">=0.1.0") -> dict:
        return {"id": package_id, "kind": "package", "version": version, "required": required}

    def package_conflict(self, package_id: str, reason: str = "Declared package conflict for test coverage.") -> dict:
        return {"id": package_id, "kind": "package", "reason": reason}

    def set_package_runtime_capabilities(self, package_dir: Path, capability_ids: list[str]) -> None:
        manifest_path = package_dir / loader.PACKAGE_MANIFEST_NAME
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
        manifest["dependencies"] = []
        manifest["engine"]["capabilities"] = [
            {
                "id": capability_id,
                "version": "0.1.0",
                "required": True,
                "reason": f"Test package requires {capability_id}.",
            }
            for capability_id in capability_ids
        ]
        write_json(manifest_path, manifest)

    def runtime_info_for_capabilities(self, capability_ids: list[str]) -> dict:
        return {
            "runtimeId": "modlist-test-runtime",
            "runtimeVersion": "0.1.0",
            "contract": {"id": loader.RUNTIME_CONTRACT_ID, "versions": [loader.RUNTIME_CONTRACT_VERSION]},
            "platforms": [{"platform": loader.current_platform_id()}],
            "capabilities": [{"id": capability_id, "version": "0.1.0"} for capability_id in capability_ids],
        }

    def set_registry_runtime_capabilities(self, registry_path: Path, capability_ids: list[str]) -> Path:
        registry = json.loads(registry_path.read_text(encoding="utf-8"))
        runtime = registry["runtimes"][0]
        runtime_info_path = Path(runtime["runtimeInfo"])
        existing_runtime_info = json.loads(runtime_info_path.read_text(encoding="utf-8"))
        runtime_info = self.runtime_info_for_capabilities(capability_ids)
        if "windowsRuntimeStatus" in existing_runtime_info:
            runtime_info["windowsRuntimeStatus"] = existing_runtime_info["windowsRuntimeStatus"]
        write_json(runtime_info_path, runtime_info)
        runtime["capabilities"] = runtime_info["capabilities"]
        write_json(registry_path, registry)
        return runtime_info_path


    def set_package_manifest_compatibility(
        self,
        package_dir: Path,
        *,
        package_dependencies: list[dict] | None = None,
        conflicts: list[dict] | None = None,
        load_after: list[str] | None = None,
        load_before: list[str] | None = None,
    ) -> None:
        manifest_path = package_dir / loader.PACKAGE_MANIFEST_NAME
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
        if package_dependencies is not None:
            non_package_dependencies = [
                dependency for dependency in manifest.get("dependencies", []) if dependency.get("kind") != "package"
            ]
            manifest["dependencies"] = [*non_package_dependencies, *package_dependencies]
        if conflicts is not None:
            manifest["conflicts"] = conflicts
        if load_after is not None:
            manifest["loadAfter"] = load_after
        if load_before is not None:
            manifest["loadBefore"] = load_before
        write_json(manifest_path, manifest)

    def neutralize_package_compatibility(self, *package_dirs: Path) -> None:
        for package_dir in package_dirs:
            manifest_path = package_dir / loader.PACKAGE_MANIFEST_NAME
            manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
            manifest["conflicts"] = []
            engine = manifest.get("engine")
            if isinstance(engine, dict):
                for capability in engine.get("capabilities", []):
                    if isinstance(capability, dict):
                        capability.pop("exclusive", None)
            modules = manifest.get("modules")
            if isinstance(modules, dict):
                for module_name in ("persistentStorage", "persistentInventories", "voidChestBindings", "multiplayer", "placements"):
                    module_value = modules.get(module_name)
                    module_items = module_value if isinstance(module_value, list) else [module_value]
                    for module_item in module_items:
                        if isinstance(module_item, dict):
                            module_item.pop("exclusive", None)
            write_json(manifest_path, manifest)

    def test_profile_state_exposes_profiles_list_for_profiles_card(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            workspace = Path(tmp)
            package_dir = self.make_package(workspace)
            profile_dir, _registry_path, _hook_library = self.make_profile_and_registry(workspace, package_dir)
            self.write_profile_active_mods(profile_dir, [self.active_mod_entry_for_package(package_dir)])
            alternate_dir = profile_dir.parent / "alternate"
            (alternate_dir / loader.APP_ID).mkdir(parents=True)
            write_json(
                alternate_dir / loader.APP_ID / "profile.json",
                {
                    "schemaVersion": loader.SCHEMA_VERSION,
                    "profile": {"id": "alternate"},
                    "runtime": {},
                    "activeMods": [],
                },
            )

            state = loader.build_profile_state(profile_dir)

            profile_ids = [item["id"] for item in state["profiles"]]
            self.assertIn("security-test", profile_ids)
            self.assertIn("alternate", profile_ids)
            self.assertEqual(state["profileCount"], 2)
            selected = [item for item in state["profileList"] if item["selected"]]
            self.assertEqual([item["id"] for item in selected], ["security-test"])

    def test_profiles_concept_and_compact_card_contain_profiles_list(self) -> None:
        profile_state = {
            "status": "selected",
            "id": "default",
            "path": "/profiles/default",
            "stableDefault": True,
            "profiles": [
                {"id": "default", "path": "/profiles/default", "selected": True, "activeModCount": 1},
                {"id": "challenge", "path": "/profiles/challenge", "selected": False, "activeModCount": 0},
            ],
        }
        concepts = loader._gui_build_concepts(
            install={"status": "ready"},
            profile_state=profile_state,
            package_catalog={"packages": []},
            selected_summary=None,
            selected_mod=None,
            active_mods=[],
            active_result=None,
            readiness={"readiness": {"rows": [], "disabledReasons": [], "status": "ready"}},
            launch_dry_run={"status": "ready", "disabledReasons": []},
            diagnostics={"items": [], "productionValidation": [], "label": "No diagnostics", "status": "not_run"},
            windows_status={"disabledReasons": []},
            workshop={"metadataRows": [], "previewAssets": [], "status": "ready"},
            environment_summary_items=[],
        )
        concept_map = {concept["key"]: concept for concept in concepts}
        profiles = concept_map["profiles"]
        self.assertEqual([item["id"] for item in profiles["state"]["profiles"]], ["default", "challenge"])
        compact = loader._gui_compact_status_cards(concept_map)
        profiles_card = next(card for card in compact if card["key"] == "profiles")
        self.assertEqual(profiles_card["profileCount"], 2)
        self.assertEqual([item["id"] for item in profiles_card["profileList"]], ["default", "challenge"])
        self.assertEqual([item["id"] for item in profiles_card["profileRows"]], ["default", "challenge"])
        self.assertTrue(profiles_card["profileRows"][0]["selected"])
        self.assertEqual([item["label"] for item in profiles_card["rows"][:2]], ["✓ default", "• challenge"])
        self.assertIn("(selected)", profiles_card["rows"][0]["value"])
        self.assertIn("Profiles: ✓ default", profiles_card["summary"])
        self.assertIn("• challenge", profiles_card["summary"])
        copy_context = loader._gui_copy_for_ai_context({"profilePath": "/profiles/default", "profile": profile_state, "profileList": profile_state["profiles"]})
        self.assertEqual(copy_context["bundle"]["profile"]["profileCount"], 2)
        self.assertEqual([item["id"] for item in copy_context["bundle"]["profile"]["profileList"]], ["default", "challenge"])

    def assert_issue_mentions(self, issues: list[loader.Problem], token: str) -> loader.Problem:
        token_lower = token.casefold()
        for problem in issues:
            haystack = " ".join(
                (
                    problem.code,
                    problem.message,
                    json.dumps(problem.details, sort_keys=True, default=str),
                )
            ).casefold()
            if token_lower in haystack:
                return problem
        summaries = [
            {"code": problem.code, "severity": problem.severity, "message": problem.message, "details": problem.details}
            for problem in issues
        ]
        self.fail(f"Expected an issue mentioning {token!r}; got {summaries!r}")

    def plan_item_package_id(self, item: object) -> str | None:
        if isinstance(item, str):
            return item
        if isinstance(item, dict):
            nested_package = item.get("package") if isinstance(item.get("package"), dict) else {}
            value = item.get("id") or item.get("packageId") or nested_package.get("id")
            return str(value) if value is not None else None
        manifest = getattr(item, "manifest", None)
        if isinstance(manifest, dict):
            value = manifest.get("id")
            return str(value) if value is not None else None
        return None

    def plan_package_ids(self, items: list) -> list[str]:
        return [package_id for item in items if (package_id := self.plan_item_package_id(item)) is not None]

    def enable_package_for_profile(
        self,
        profile_dir: Path,
        package_dir: Path,
        *,
        package_path: Path | None = None,
        manifest_path: Path | None = None,
    ) -> dict:
        entry = self.active_mod_entry_for_package(
            package_dir,
            package_path=package_path,
            manifest_path=manifest_path,
        )
        self.write_profile_active_mods(profile_dir, [entry])
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


    def test_launch_plan_without_package_writes_two_active_mod_artifacts(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            workspace = Path(temp_dir)
            alpha_dir = self.make_package(workspace, "alpha-package")
            beta_dir = self.make_package(workspace, "beta-package")
            self.set_package_identity(alpha_dir, "test.alpha")
            self.set_package_identity(beta_dir, "test.beta")
            self.neutralize_package_compatibility(alpha_dir, beta_dir)
            self.set_package_runtime_capabilities(alpha_dir, ["persistent_storage"])
            self.set_package_runtime_capabilities(beta_dir, ["multiplayer_version_metadata"])
            profile_dir, _registry_path, _hook_library = self.make_profile_and_registry(workspace, alpha_dir)
            runtime_info_path = workspace / "modlist-runtime-info.json"
            write_json(runtime_info_path, self.runtime_info_for_capabilities(["persistent_storage", "multiplayer_version_metadata"]))
            self.write_profile_active_mods(
                profile_dir,
                [
                    self.active_mod_entry_for_package(alpha_dir, load_order=20),
                    self.active_mod_entry_for_package(beta_dir, load_order=10),
                ],
            )

            launch_plan = self.run_cli("launch-plan", str(profile_dir), "--runtime-info", str(runtime_info_path))

            self.assertEqual(launch_plan.returncode, 0, launch_plan.stdout)
            payload = json.loads(launch_plan.stdout)
            self.assertEqual(payload["status"], "created")
            self.assertEqual(payload["runtimeInfo"], str(runtime_info_path.resolve()))
            manifest_path = Path(payload["runtimeManifest"])
            validation_report_path = profile_dir / loader.APP_ID / "validation-report.json"
            self.assertEqual(payload["validationReport"], str(validation_report_path))
            manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
            self.assertEqual([mod["id"] for mod in manifest["mods"]], ["test.beta", "test.alpha"])
            self.assertEqual([mod["loadOrder"] for mod in manifest["mods"]], [0, 1])
            validation_report = json.loads(validation_report_path.read_text(encoding="utf-8"))
            self.assertTrue(validation_report["launchable"])
            self.assertEqual(validation_report["blockingIssues"], [])
            self.assertEqual(self.plan_package_ids(validation_report["loadOrder"]), ["test.beta", "test.alpha"])

    def test_launch_plan_package_assertion_blocks_package_not_active(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            workspace = Path(temp_dir)
            alpha_dir = self.make_package(workspace, "alpha-package")
            beta_dir = self.make_package(workspace, "beta-package")
            inactive_dir = self.make_package(workspace, "inactive-package")
            self.set_package_identity(alpha_dir, "test.alpha")
            self.set_package_identity(beta_dir, "test.beta")
            self.set_package_identity(inactive_dir, "test.inactive")
            self.neutralize_package_compatibility(alpha_dir, beta_dir, inactive_dir)
            self.set_package_runtime_capabilities(alpha_dir, ["persistent_storage"])
            self.set_package_runtime_capabilities(beta_dir, ["multiplayer_version_metadata"])
            self.set_package_runtime_capabilities(inactive_dir, ["persistent_storage"])
            profile_dir, _registry_path, _hook_library = self.make_profile_and_registry(workspace, alpha_dir)
            runtime_info_path = workspace / "modlist-runtime-info.json"
            write_json(runtime_info_path, self.runtime_info_for_capabilities(["persistent_storage", "multiplayer_version_metadata"]))
            self.write_profile_active_mods(
                profile_dir,
                [
                    self.active_mod_entry_for_package(alpha_dir, load_order=10),
                    self.active_mod_entry_for_package(beta_dir, load_order=20),
                ],
            )

            launch_plan = self.run_cli(
                "launch-plan",
                str(profile_dir),
                "--package",
                str(inactive_dir),
                "--runtime-info",
                str(runtime_info_path),
            )

            self.assertNotEqual(launch_plan.returncode, 0, launch_plan.stdout)
            self.assertIn("BML_MODLIST_ASSERTED_PACKAGE_NOT_ACTIVE", launch_plan.stdout)
            self.assertIn("test.inactive", launch_plan.stdout)
            self.assertNotIn('"status": "created"', launch_plan.stdout)

    def test_launch_plan_declared_package_conflict_blocks_launchable_success(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            workspace = Path(temp_dir)
            alpha_dir = self.make_package(workspace, "alpha-package")
            beta_dir = self.make_package(workspace, "beta-package")
            self.set_package_identity(alpha_dir, "test.alpha")
            self.set_package_identity(beta_dir, "test.beta")
            self.neutralize_package_compatibility(alpha_dir, beta_dir)
            self.set_package_runtime_capabilities(alpha_dir, ["persistent_storage"])
            self.set_package_runtime_capabilities(beta_dir, ["multiplayer_version_metadata"])
            self.set_package_manifest_compatibility(
                alpha_dir,
                conflicts=[self.package_conflict("test.beta", "Alpha and beta both own the same gameplay surface.")],
            )
            self.set_package_manifest_compatibility(beta_dir, conflicts=[])
            profile_dir, _registry_path, _hook_library = self.make_profile_and_registry(workspace, alpha_dir)
            runtime_info_path = workspace / "modlist-runtime-info.json"
            write_json(runtime_info_path, self.runtime_info_for_capabilities(["persistent_storage", "multiplayer_version_metadata"]))
            self.write_profile_active_mods(
                profile_dir,
                [
                    self.active_mod_entry_for_package(alpha_dir),
                    self.active_mod_entry_for_package(beta_dir),
                ],
            )

            launch_plan = self.run_cli("launch-plan", str(profile_dir), "--runtime-info", str(runtime_info_path))

            self.assertNotEqual(launch_plan.returncode, 0, launch_plan.stdout)
            self.assertIn("BML_MODLIST_PACKAGE_CONFLICT", launch_plan.stdout)
            self.assertIn("test.beta", launch_plan.stdout)
            self.assertNotIn('"status": "created"', launch_plan.stdout)
            validation_report_path = profile_dir / loader.APP_ID / "validation-report.json"
            if validation_report_path.exists():
                validation_report = json.loads(validation_report_path.read_text(encoding="utf-8"))
                self.assertFalse(validation_report["launchable"])

    def test_launch_dry_run_without_package_writes_two_active_mod_artifacts(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            workspace = Path(temp_dir)
            alpha_dir = self.make_package(workspace, "alpha-package")
            beta_dir = self.make_package(workspace, "beta-package")
            self.set_package_identity(alpha_dir, "test.alpha")
            self.set_package_identity(beta_dir, "test.beta")
            self.neutralize_package_compatibility(alpha_dir, beta_dir)
            self.set_package_runtime_capabilities(alpha_dir, ["persistent_storage"])
            self.set_package_runtime_capabilities(beta_dir, ["multiplayer_version_metadata"])
            profile_dir, registry_path, _hook_library = self.make_profile_and_registry(workspace, alpha_dir)
            runtime_info_path = self.set_registry_runtime_capabilities(
                registry_path,
                ["persistent_storage", "multiplayer_version_metadata"],
            )
            self.write_profile_active_mods(
                profile_dir,
                [
                    self.active_mod_entry_for_package(alpha_dir, load_order=20),
                    self.active_mod_entry_for_package(beta_dir, load_order=10),
                ],
            )

            launch = self.run_cli("launch", str(profile_dir), "--registry", str(registry_path), "--dry-run")

            self.assertEqual(launch.returncode, 0, launch.stdout)
            payload = json.loads(launch.stdout)
            self.assertEqual(payload["status"], "dry-run")
            self.assertEqual(payload["runtimeInfo"], str(runtime_info_path))
            manifest_path = Path(payload["runtimeManifest"])
            validation_report_path = profile_dir / loader.APP_ID / "validation-report.json"
            self.assertEqual(payload["validationReport"], str(validation_report_path))
            manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
            self.assertEqual([mod["id"] for mod in manifest["mods"]], ["test.beta", "test.alpha"])
            self.assertEqual([mod["loadOrder"] for mod in manifest["mods"]], [0, 1])
            active_mods = json.loads(Path(payload["activeMods"]).read_text(encoding="utf-8"))
            self.assertEqual([mod["id"] for mod in active_mods["mods"]], ["test.beta", "test.alpha"])
            validation_report = json.loads(validation_report_path.read_text(encoding="utf-8"))
            self.assertTrue(validation_report["launchable"])
            self.assertEqual(validation_report["blockingIssues"], [])

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
            self.assertIn("BML_PROFILE_NO_ACTIVE_PACKAGE", launch.stdout)


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
            self.assertIn("BML_PROFILE_NO_ACTIVE_PACKAGE", launch.stdout)

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

    def test_launch_rejects_package_assertion_path_mismatch(self) -> None:
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
            self.assertIn("BML_MODLIST_ASSERTED_PACKAGE_NOT_ACTIVE", launch.stdout)
            stdout_for_paths = launch.stdout.replace("\\\\", "\\")
            self.assertIn(str(enabled_dir.resolve()), stdout_for_paths)
            self.assertIn(str(package_dir.resolve()), stdout_for_paths)

    def test_modlist_compatibility_plan_warning_only_issues_are_launchable(self) -> None:
        warning = loader.Problem("BML_TEST_WARNING", "warning", "Warn without blocking launch.")
        info = loader.Problem("BML_TEST_INFO", "info", "Informational issue.")
        warning_plan = loader.ModlistCompatibilityPlan(
            enabled_mods=[],
            packages=[],
            load_order=[],
            issues=[warning, info],
        )

        self.assertTrue(warning_plan.launchable)
        self.assertEqual(warning_plan.blocking_issues, [])
        self.assertEqual(
            [problem.code for problem in warning_plan.non_blocking_issues],
            ["BML_TEST_WARNING", "BML_TEST_INFO"],
        )

        error = loader.Problem("BML_TEST_ERROR", "error", "Error blocks launch.")
        fatal = loader.Problem("BML_TEST_FATAL", "fatal", "Fatal blocks launch.")
        blocking_plan = loader.ModlistCompatibilityPlan(
            enabled_mods=[],
            packages=[],
            load_order=[],
            issues=[warning, error, fatal],
        )

        self.assertFalse(blocking_plan.launchable)
        self.assertEqual(
            [problem.code for problem in blocking_plan.blocking_issues],
            ["BML_TEST_ERROR", "BML_TEST_FATAL"],
        )
        self.assertEqual(
            [problem.code for problem in blocking_plan.non_blocking_issues],
            ["BML_TEST_WARNING"],
        )

    def test_modlist_compatibility_plan_accepts_two_valid_active_packages(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            workspace = Path(temp_dir)
            alpha_dir = self.make_package(workspace, "alpha-package")
            beta_dir = self.make_package(workspace, "beta-package")
            self.set_package_identity(alpha_dir, "test.alpha")
            self.set_package_identity(beta_dir, "test.beta")
            self.neutralize_package_compatibility(alpha_dir, beta_dir)
            profile_dir, _registry_path, _hook_library = self.make_profile_and_registry(workspace, alpha_dir)
            mods = [
                self.active_mod_entry_for_package(alpha_dir),
                self.active_mod_entry_for_package(beta_dir),
            ]
            self.write_profile_active_mods(profile_dir, mods)
            profile = json.loads((profile_dir / loader.APP_ID / "profile.json").read_text(encoding="utf-8"))

            plan = loader.build_modlist_compatibility_plan(profile, profile_dir)

            self.assertTrue(plan.launchable, [problem.code for problem in plan.issues])
            self.assertEqual(plan.blocking_issues, [])
            self.assertCountEqual(self.plan_package_ids(plan.packages), ["test.alpha", "test.beta"])
            self.assertCountEqual(self.plan_package_ids(plan.enabled_mods), ["test.alpha", "test.beta"])


    def test_package_library_state_exposes_launchable_modlist_for_multiple_active_packages(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            workspace = Path(temp_dir)
            alpha_dir = self.make_package(workspace, "alpha-package")
            beta_dir = self.make_package(workspace, "beta-package")
            self.set_package_identity(alpha_dir, "test.alpha")
            self.set_package_identity(beta_dir, "test.beta")
            self.neutralize_package_compatibility(alpha_dir, beta_dir)
            profile_dir, _registry_path, _hook_library = self.make_profile_and_registry(workspace, alpha_dir)
            mods = [
                self.active_mod_entry_for_package(alpha_dir, load_order=10),
                self.active_mod_entry_for_package(beta_dir, load_order=20),
            ]
            self.write_profile_active_mods(profile_dir, mods)

            state = loader.build_package_library_state(alpha_dir, beta_dir, profile_dir=profile_dir, selected_package=alpha_dir)

            self.assertCountEqual(self.plan_package_ids(state.get("activeMods", [])), ["test.alpha", "test.beta"])
            disabled_text = "\n".join(str(reason) for reason in state.get("disabledReasons", []))
            self.assertNotRegex(
                disabled_text,
                r"multiple active|more than one active|one active package|one package at a time|disable all but one",
            )
            plan = state.get("modlistPlan")
            self.assertIsInstance(plan, dict, state)
            self.assertIs(plan.get("launchable"), True, plan)
            plan_mod_ids = self.plan_package_ids(
                plan.get("enabledMods")
                or plan.get("enabled_mods")
                or plan.get("loadOrder")
                or plan.get("load_order")
                or []
            )
            self.assertCountEqual(plan_mod_ids, ["test.alpha", "test.beta"])
            self.assertEqual(plan.get("blocking") or plan.get("blockingIssues") or plan.get("blocking_issues") or [], [])

    def test_modlist_compatibility_plan_uses_active_mod_load_order(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            workspace = Path(temp_dir)
            alpha_dir = self.make_package(workspace, "alpha-package")
            beta_dir = self.make_package(workspace, "beta-package")
            self.set_package_identity(alpha_dir, "test.alpha")
            self.set_package_identity(beta_dir, "test.beta")
            self.neutralize_package_compatibility(alpha_dir, beta_dir)
            profile_dir, _registry_path, _hook_library = self.make_profile_and_registry(workspace, alpha_dir)
            mods = [
                self.active_mod_entry_for_package(alpha_dir, load_order=20),
                self.active_mod_entry_for_package(beta_dir, load_order=10),
            ]
            self.write_profile_active_mods(profile_dir, mods)
            profile = json.loads((profile_dir / loader.APP_ID / "profile.json").read_text(encoding="utf-8"))

            plan = loader.build_modlist_compatibility_plan(profile, profile_dir)

            self.assertTrue(plan.launchable, [problem.code for problem in plan.issues])
            self.assertEqual(self.plan_package_ids(plan.load_order), ["test.beta", "test.alpha"])


    def test_modlist_compatibility_plan_orders_required_package_dependency_before_dependent(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            workspace = Path(temp_dir)
            library_dir = self.make_package(workspace, "library-package")
            consumer_dir = self.make_package(workspace, "consumer-package")
            self.set_package_identity(library_dir, "test.library")
            self.set_package_identity(consumer_dir, "test.consumer")
            self.set_package_manifest_compatibility(library_dir, conflicts=[])
            self.set_package_manifest_compatibility(
                consumer_dir,
                package_dependencies=[self.package_dependency("test.library")],
                conflicts=[],
            )
            profile_dir, _registry_path, _hook_library = self.make_profile_and_registry(workspace, consumer_dir)
            mods = [
                self.active_mod_entry_for_package(consumer_dir, load_order=10),
                self.active_mod_entry_for_package(library_dir, load_order=20),
            ]
            self.write_profile_active_mods(profile_dir, mods)
            profile = json.loads((profile_dir / loader.APP_ID / "profile.json").read_text(encoding="utf-8"))

            plan = loader.build_modlist_compatibility_plan(profile, profile_dir)

            self.assertTrue(plan.launchable, [problem.code for problem in plan.issues])
            self.assertEqual(plan.blocking_issues, [])
            self.assertEqual(self.plan_package_ids(plan.load_order), ["test.library", "test.consumer"])

    def test_modlist_compatibility_plan_blocks_missing_required_package_dependency(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            workspace = Path(temp_dir)
            consumer_dir = self.make_package(workspace, "consumer-package")
            self.set_package_identity(consumer_dir, "test.consumer")
            self.set_package_manifest_compatibility(
                consumer_dir,
                package_dependencies=[self.package_dependency("test.library")],
                conflicts=[],
            )
            profile_dir, _registry_path, _hook_library = self.make_profile_and_registry(workspace, consumer_dir)
            mods = [self.active_mod_entry_for_package(consumer_dir)]
            self.write_profile_active_mods(profile_dir, mods)
            profile = json.loads((profile_dir / loader.APP_ID / "profile.json").read_text(encoding="utf-8"))

            plan = loader.build_modlist_compatibility_plan(profile, profile_dir)

            self.assertFalse(plan.launchable)
            issue = self.assert_issue_mentions(plan.blocking_issues, "test.library")
            self.assertTrue(issue.is_error)

    def test_modlist_compatibility_plan_warns_for_missing_optional_package_dependency(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            workspace = Path(temp_dir)
            consumer_dir = self.make_package(workspace, "consumer-package")
            self.set_package_identity(consumer_dir, "test.consumer")
            self.set_package_manifest_compatibility(
                consumer_dir,
                package_dependencies=[self.package_dependency("test.library", required=False)],
                conflicts=[],
            )
            profile_dir, _registry_path, _hook_library = self.make_profile_and_registry(workspace, consumer_dir)
            mods = [self.active_mod_entry_for_package(consumer_dir)]
            self.write_profile_active_mods(profile_dir, mods)
            profile = json.loads((profile_dir / loader.APP_ID / "profile.json").read_text(encoding="utf-8"))

            plan = loader.build_modlist_compatibility_plan(profile, profile_dir)

            self.assertTrue(plan.launchable, [problem.code for problem in plan.issues])
            self.assertEqual(plan.blocking_issues, [])
            issue = self.assert_issue_mentions(plan.non_blocking_issues, "test.library")
            self.assertEqual(issue.severity, "warning")

    def test_modlist_compatibility_plan_applies_load_after_and_load_before_ordering(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            workspace = Path(temp_dir)
            alpha_dir = self.make_package(workspace, "alpha-package")
            beta_dir = self.make_package(workspace, "beta-package")
            gamma_dir = self.make_package(workspace, "gamma-package")
            self.set_package_identity(alpha_dir, "test.alpha")
            self.set_package_identity(beta_dir, "test.beta")
            self.set_package_identity(gamma_dir, "test.gamma")
            self.set_package_manifest_compatibility(alpha_dir, conflicts=[], load_after=["test.beta"])
            self.set_package_manifest_compatibility(beta_dir, conflicts=[])
            self.set_package_manifest_compatibility(gamma_dir, conflicts=[], load_before=["test.beta"])
            profile_dir, _registry_path, _hook_library = self.make_profile_and_registry(workspace, alpha_dir)
            mods = [
                self.active_mod_entry_for_package(alpha_dir, load_order=10),
                self.active_mod_entry_for_package(beta_dir, load_order=20),
                self.active_mod_entry_for_package(gamma_dir, load_order=30),
            ]
            self.write_profile_active_mods(profile_dir, mods)
            profile = json.loads((profile_dir / loader.APP_ID / "profile.json").read_text(encoding="utf-8"))

            plan = loader.build_modlist_compatibility_plan(profile, profile_dir)

            self.assertTrue(plan.launchable, [problem.code for problem in plan.issues])
            self.assertEqual(plan.blocking_issues, [])
            self.assertEqual(self.plan_package_ids(plan.load_order), ["test.gamma", "test.beta", "test.alpha"])

    def test_modlist_compatibility_plan_blocks_load_order_cycle(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            workspace = Path(temp_dir)
            alpha_dir = self.make_package(workspace, "alpha-package")
            beta_dir = self.make_package(workspace, "beta-package")
            self.set_package_identity(alpha_dir, "test.alpha")
            self.set_package_identity(beta_dir, "test.beta")
            self.set_package_manifest_compatibility(alpha_dir, conflicts=[], load_after=["test.beta"])
            self.set_package_manifest_compatibility(beta_dir, conflicts=[], load_after=["test.alpha"])
            profile_dir, _registry_path, _hook_library = self.make_profile_and_registry(workspace, alpha_dir)
            mods = [
                self.active_mod_entry_for_package(alpha_dir),
                self.active_mod_entry_for_package(beta_dir),
            ]
            self.write_profile_active_mods(profile_dir, mods)
            profile = json.loads((profile_dir / loader.APP_ID / "profile.json").read_text(encoding="utf-8"))

            plan = loader.build_modlist_compatibility_plan(profile, profile_dir)

            self.assertFalse(plan.launchable)
            self.assertTrue(plan.blocking_issues, [problem.code for problem in plan.issues])

    def test_modlist_compatibility_plan_blocks_declared_package_conflict(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            workspace = Path(temp_dir)
            alpha_dir = self.make_package(workspace, "alpha-package")
            beta_dir = self.make_package(workspace, "beta-package")
            self.set_package_identity(alpha_dir, "test.alpha")
            self.set_package_identity(beta_dir, "test.beta")
            self.set_package_manifest_compatibility(
                alpha_dir,
                conflicts=[self.package_conflict("test.beta", "Alpha and beta both own the same gameplay surface.")],
            )
            self.set_package_manifest_compatibility(beta_dir, conflicts=[])
            profile_dir, _registry_path, _hook_library = self.make_profile_and_registry(workspace, alpha_dir)
            mods = [
                self.active_mod_entry_for_package(alpha_dir),
                self.active_mod_entry_for_package(beta_dir),
            ]
            self.write_profile_active_mods(profile_dir, mods)
            profile = json.loads((profile_dir / loader.APP_ID / "profile.json").read_text(encoding="utf-8"))

            plan = loader.build_modlist_compatibility_plan(profile, profile_dir)

            self.assertFalse(plan.launchable)
            issue = self.assert_issue_mentions(plan.blocking_issues, "test.beta")
            self.assertTrue(issue.is_error)


    def test_modlist_compatibility_plan_blocks_wildcard_package_conflict(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            workspace = Path(temp_dir)
            alpha_dir = self.make_package(workspace, "alpha-package")
            beta_dir = self.make_package(workspace, "beta-package")
            self.set_package_identity(alpha_dir, "test.alpha")
            self.set_package_identity(beta_dir, "test.beta.overhaul")
            self.set_package_manifest_compatibility(
                alpha_dir,
                conflicts=[self.package_conflict("test.beta.*", "Alpha rejects beta-family overhauls.")],
            )
            self.set_package_manifest_compatibility(beta_dir, conflicts=[])
            profile_dir, _registry_path, _hook_library = self.make_profile_and_registry(workspace, alpha_dir)
            mods = [
                self.active_mod_entry_for_package(alpha_dir),
                self.active_mod_entry_for_package(beta_dir),
            ]
            self.write_profile_active_mods(profile_dir, mods)
            profile = json.loads((profile_dir / loader.APP_ID / "profile.json").read_text(encoding="utf-8"))

            plan = loader.build_modlist_compatibility_plan(profile, profile_dir)

            self.assertFalse(plan.launchable)
            issue = self.assert_issue_mentions(plan.blocking_issues, "test.beta.overhaul")
            self.assertEqual(issue.code, "BML_MODLIST_PACKAGE_CONFLICT")
            self.assertTrue(issue.is_error)

    def test_modlist_compatibility_plan_blocks_package_qualified_wildcard_exclusive_owner_conflict(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            workspace = Path(temp_dir)
            alpha_dir = self.make_package(workspace, "alpha-package")
            beta_dir = self.make_package(workspace, "beta-package")
            self.set_package_identity(alpha_dir, "test.alpha")
            self.set_package_identity(beta_dir, "test.beta")
            self.set_package_manifest_compatibility(
                alpha_dir,
                conflicts=[
                    {
                        "id": "test.beta.*.void_chest_binding",
                        "kind": "exclusive-capability-owner",
                        "reason": "Alpha cannot share authoritative Void Chest binding owners.",
                    }
                ],
            )
            self.set_package_manifest_compatibility(beta_dir, conflicts=[])
            profile_dir, _registry_path, _hook_library = self.make_profile_and_registry(workspace, alpha_dir)
            mods = [
                self.active_mod_entry_for_package(alpha_dir),
                self.active_mod_entry_for_package(beta_dir),
            ]
            self.write_profile_active_mods(profile_dir, mods)
            profile = json.loads((profile_dir / loader.APP_ID / "profile.json").read_text(encoding="utf-8"))

            plan = loader.build_modlist_compatibility_plan(profile, profile_dir)

            self.assertFalse(plan.launchable)
            issue = self.assert_issue_mentions(plan.blocking_issues, "test.beta")
            self.assertEqual(issue.code, "BML_MODLIST_EXCLUSIVE_CAPABILITY_CONFLICT")
            self.assertTrue(issue.is_error)

    def test_modlist_compatibility_plan_warns_for_missing_load_order_target_without_blocking_launch(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            workspace = Path(temp_dir)
            alpha_dir = self.make_package(workspace, "alpha-package")
            self.set_package_identity(alpha_dir, "test.alpha")
            self.set_package_manifest_compatibility(alpha_dir, conflicts=[], load_after=["test.missing"])
            profile_dir, _registry_path, _hook_library = self.make_profile_and_registry(workspace, alpha_dir)
            mods = [self.active_mod_entry_for_package(alpha_dir)]
            self.write_profile_active_mods(profile_dir, mods)
            profile = json.loads((profile_dir / loader.APP_ID / "profile.json").read_text(encoding="utf-8"))

            plan = loader.build_modlist_compatibility_plan(profile, profile_dir)

            self.assertTrue(plan.launchable, [problem.code for problem in plan.issues])
            self.assertEqual(plan.blocking_issues, [])
            issue = self.assert_issue_mentions(plan.non_blocking_issues, "test.missing")
            self.assertEqual(issue.severity, "warning")

    def test_modlist_compatibility_plan_blocks_active_package_path_mismatch(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            workspace = Path(temp_dir)
            declared_dir = self.make_package(workspace, "declared-package")
            actual_dir = self.make_package(workspace, "actual-package")
            self.set_package_identity(declared_dir, "test.declared")
            self.set_package_identity(actual_dir, "test.actual")
            profile_dir, _registry_path, _hook_library = self.make_profile_and_registry(workspace, declared_dir)
            mods = [self.active_mod_entry_for_package(declared_dir, package_path=actual_dir)]
            self.write_profile_active_mods(profile_dir, mods)
            profile = json.loads((profile_dir / loader.APP_ID / "profile.json").read_text(encoding="utf-8"))

            plan = loader.build_modlist_compatibility_plan(profile, profile_dir)

            blocking_codes = [problem.code for problem in plan.blocking_issues]
            self.assertFalse(plan.launchable)
            self.assertIn("BML_PROFILE_PACKAGE_PATH_MISMATCH", blocking_codes)
            self.assertIn(
                "BML_PROFILE_PACKAGE_PATH_MISMATCH",
                [problem.code for problem in plan.issues],
            )

    def test_validate_runtime_info_for_modlist_accepts_two_package_capability_sets(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            workspace = Path(temp_dir)
            alpha_dir = self.make_package(workspace, "alpha-package")
            beta_dir = self.make_package(workspace, "beta-package")
            self.set_package_identity(alpha_dir, "test.alpha")
            self.set_package_identity(beta_dir, "test.beta")
            self.neutralize_package_compatibility(alpha_dir, beta_dir)
            self.set_package_runtime_capabilities(alpha_dir, ["persistent_storage"])
            self.set_package_runtime_capabilities(beta_dir, ["multiplayer_version_metadata"])
            profile_dir, _registry_path, _hook_library = self.make_profile_and_registry(workspace, alpha_dir)
            mods = [
                self.active_mod_entry_for_package(alpha_dir, load_order=10),
                self.active_mod_entry_for_package(beta_dir, load_order=20),
            ]
            self.write_profile_active_mods(profile_dir, mods)
            profile = json.loads((profile_dir / loader.APP_ID / "profile.json").read_text(encoding="utf-8"))
            plan = loader.build_modlist_compatibility_plan(profile, profile_dir)
            runtime_info = self.runtime_info_for_capabilities(["persistent_storage", "multiplayer_version_metadata"])

            result = loader.validate_runtime_info_for_modlist(runtime_info, plan)

            self.assertTrue(plan.launchable, [problem.code for problem in plan.issues])
            self.assertTrue(result.ok, [problem.code for problem in result.problems])
            self.assertEqual(result.problems, [])

    def test_validate_runtime_info_for_modlist_blocks_missing_active_package_capability(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            workspace = Path(temp_dir)
            alpha_dir = self.make_package(workspace, "alpha-package")
            beta_dir = self.make_package(workspace, "beta-package")
            self.set_package_identity(alpha_dir, "test.alpha")
            self.set_package_identity(beta_dir, "test.beta")
            self.neutralize_package_compatibility(alpha_dir, beta_dir)
            self.set_package_runtime_capabilities(alpha_dir, ["persistent_storage"])
            self.set_package_runtime_capabilities(beta_dir, ["multiplayer_version_metadata"])
            profile_dir, _registry_path, _hook_library = self.make_profile_and_registry(workspace, alpha_dir)
            mods = [
                self.active_mod_entry_for_package(alpha_dir, load_order=10),
                self.active_mod_entry_for_package(beta_dir, load_order=20),
            ]
            self.write_profile_active_mods(profile_dir, mods)
            profile = json.loads((profile_dir / loader.APP_ID / "profile.json").read_text(encoding="utf-8"))
            plan = loader.build_modlist_compatibility_plan(profile, profile_dir)
            runtime_info = self.runtime_info_for_capabilities(["persistent_storage"])

            result = loader.validate_runtime_info_for_modlist(runtime_info, plan)

            self.assertTrue(plan.launchable, [problem.code for problem in plan.issues])
            self.assertFalse(result.ok)
            missing = [problem for problem in result.problems if problem.code == "BML_RUNTIME_CAPABILITY_MISSING"]
            self.assertEqual(len(missing), 1, [problem.code for problem in result.problems])
            self.assertEqual(missing[0].details["capability"], "multiplayer_version_metadata")

    def test_build_runtime_manifest_for_modlist_emits_plan_load_order_mods(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            workspace = Path(temp_dir)
            alpha_dir = self.make_package(workspace, "alpha-package")
            beta_dir = self.make_package(workspace, "beta-package")
            self.set_package_identity(alpha_dir, "test.alpha")
            self.set_package_identity(beta_dir, "test.beta")
            self.neutralize_package_compatibility(alpha_dir, beta_dir)
            self.set_package_runtime_capabilities(alpha_dir, ["persistent_storage"])
            self.set_package_runtime_capabilities(beta_dir, ["multiplayer_version_metadata"])
            profile_dir, _registry_path, _hook_library = self.make_profile_and_registry(workspace, alpha_dir)
            mods = [
                self.active_mod_entry_for_package(alpha_dir, load_order=20),
                self.active_mod_entry_for_package(beta_dir, load_order=10),
            ]
            self.write_profile_active_mods(profile_dir, mods)
            profile = json.loads((profile_dir / loader.APP_ID / "profile.json").read_text(encoding="utf-8"))
            plan = loader.build_modlist_compatibility_plan(profile, profile_dir)
            runtime_info = self.runtime_info_for_capabilities(["persistent_storage", "multiplayer_version_metadata"])

            manifest = loader.build_runtime_manifest_for_modlist(profile, profile_dir, plan, runtime_info)

            self.assertTrue(plan.launchable, [problem.code for problem in plan.issues])
            manifest_mods = manifest["mods"]
            self.assertEqual([mod["id"] for mod in manifest_mods], ["test.beta", "test.alpha"])
            self.assertEqual([mod["loadOrder"] for mod in manifest_mods], [0, 1])
            self.assertTrue(all(isinstance(mod["loadOrder"], int) for mod in manifest_mods))

    def test_write_modlist_launch_artifacts_writes_manifest_active_mods_and_validation_report(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            workspace = Path(temp_dir)
            alpha_dir = self.make_package(workspace, "alpha-package")
            beta_dir = self.make_package(workspace, "beta-package")
            self.set_package_identity(alpha_dir, "test.alpha")
            self.set_package_identity(beta_dir, "test.beta")
            self.neutralize_package_compatibility(alpha_dir, beta_dir)
            self.set_package_runtime_capabilities(alpha_dir, ["persistent_storage"])
            self.set_package_runtime_capabilities(beta_dir, ["multiplayer_version_metadata"])
            profile_dir, _registry_path, _hook_library = self.make_profile_and_registry(workspace, alpha_dir)
            mods = [
                self.active_mod_entry_for_package(alpha_dir, load_order=20),
                self.active_mod_entry_for_package(beta_dir, load_order=10),
            ]
            self.write_profile_active_mods(profile_dir, mods)
            profile = json.loads((profile_dir / loader.APP_ID / "profile.json").read_text(encoding="utf-8"))
            plan = loader.build_modlist_compatibility_plan(profile, profile_dir)
            runtime_info = self.runtime_info_for_capabilities(["persistent_storage", "multiplayer_version_metadata"])
            runtime_manifest_path = profile_dir / loader.APP_ID / "runtime-manifest.json"

            result = loader.write_modlist_launch_artifacts(profile, profile_dir, plan, runtime_info, runtime_manifest_path)

            self.assertTrue(plan.launchable, [problem.code for problem in plan.issues])
            manifest_payload = result[0] if isinstance(result, tuple) else result
            self.assertEqual([mod["id"] for mod in manifest_payload["mods"]], ["test.beta", "test.alpha"])
            self.assertEqual([mod["loadOrder"] for mod in manifest_payload["mods"]], [0, 1])
            written_manifest = json.loads(runtime_manifest_path.read_text(encoding="utf-8"))
            self.assertEqual([mod["id"] for mod in written_manifest["mods"]], ["test.beta", "test.alpha"])
            self.assertEqual([mod["loadOrder"] for mod in written_manifest["mods"]], [0, 1])

            active_mods = json.loads((profile_dir / loader.APP_ID / "active-mods.json").read_text(encoding="utf-8"))
            self.assertEqual([mod["id"] for mod in active_mods["mods"]], ["test.beta", "test.alpha"])
            self.assertEqual([mod["loadOrder"] for mod in active_mods["mods"]], [0, 1])

            validation_report_path = profile_dir / loader.APP_ID / "validation-report.json"
            validation_report = json.loads(validation_report_path.read_text(encoding="utf-8"))
            self.assertTrue(validation_report["launchable"])
            self.assertEqual(validation_report["issues"], [])
            self.assertEqual(validation_report["blockingIssues"], [])
            self.assertEqual(validation_report["nonBlockingIssues"], [])
            self.assertEqual(self.plan_package_ids(validation_report["loadOrder"]), ["test.beta", "test.alpha"])

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
