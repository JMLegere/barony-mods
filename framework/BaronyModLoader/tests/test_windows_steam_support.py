from __future__ import annotations

import contextlib
import hashlib
import io
import importlib.util
import os
import struct
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock
from types import SimpleNamespace


APP_PATH = Path(__file__).resolve().parents[1] / "app" / "barony_mod_loader.py"


def load_app_module():
    spec = importlib.util.spec_from_file_location("barony_mod_loader_under_test", APP_PATH)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"could not load app module from {APP_PATH}")
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


def write_synthetic_pe(path: Path, machine: int = 0x8664) -> bytes:
    """Write a tiny PE32+ image with one .text section."""
    e_lfanew = 0x80
    size_of_optional_header = 0xF0
    size_of_headers = 0x200
    file_alignment = 0x200
    section_alignment = 0x1000
    section_raw_size = 0x200
    section_raw_pointer = 0x200

    dos = bytearray(64)
    dos[:2] = b"MZ"
    struct.pack_into("<I", dos, 0x3C, e_lfanew)

    coff = struct.pack(
        "<HHIIIHH",
        machine,
        1,       # NumberOfSections
        0x63B00000,
        0,
        0,
        size_of_optional_header,
        0x0022,  # executable, large-address-aware
    )

    optional = bytearray(size_of_optional_header)
    struct.pack_into("<H", optional, 0x00, 0x20B)  # PE32+
    struct.pack_into("<BB", optional, 0x02, 14, 0)  # linker version
    struct.pack_into("<III", optional, 0x04, section_raw_size, 0, 0)
    struct.pack_into("<II", optional, 0x10, 0x1000, 0x1000)  # entry point, base of code
    struct.pack_into("<Q", optional, 0x18, 0x140000000)  # image base
    struct.pack_into("<II", optional, 0x20, section_alignment, file_alignment)
    struct.pack_into("<HHHHHH", optional, 0x28, 6, 0, 0, 0, 6, 0)
    struct.pack_into("<I", optional, 0x34, 0)
    struct.pack_into("<I", optional, 0x38, 0x2000)  # SizeOfImage
    struct.pack_into("<I", optional, 0x3C, size_of_headers)
    struct.pack_into("<I", optional, 0x40, 0)
    struct.pack_into("<HH", optional, 0x44, 3, 0)  # Windows CUI subsystem
    struct.pack_into("<Q", optional, 0x48, 0x100000)
    struct.pack_into("<Q", optional, 0x50, 0x1000)
    struct.pack_into("<Q", optional, 0x58, 0x100000)
    struct.pack_into("<Q", optional, 0x60, 0x1000)
    struct.pack_into("<II", optional, 0x68, 0, 16)  # loader flags, data directories

    section = struct.pack(
        "<8sIIIIIIHHI",
        b".text\0\0\0",
        0x180,
        0x1000,
        section_raw_size,
        section_raw_pointer,
        0,
        0,
        0,
        0,
        0x60000020,  # code | execute | read
    )

    image = bytearray()
    image.extend(dos)
    image.extend(b"\0" * (e_lfanew - len(image)))
    image.extend(b"PE\0\0")
    image.extend(coff)
    image.extend(optional)
    image.extend(section)
    image.extend(b"\0" * (size_of_headers - len(image)))
    image.extend(b"\x90" * 16)
    image.extend(b"Barony synthetic test executable v4.2.1\0")
    image.extend(b"\0" * (section_raw_size - (len(image) - section_raw_pointer)))

    data = bytes(image)
    path.write_bytes(data)
    return data


@contextlib.contextmanager
def windows_platform(module, machine: str = "AMD64"):
    with mock.patch.object(module.sys, "platform", "win32"), mock.patch.object(module.platform, "machine", return_value=machine):
        yield

def is_amd64_machine(value) -> bool:
    if value == 0x8664:
        return True
    text = str(value).lower()
    return "8664" in text or "amd64" in text or "x86_64" in text


def section_value(section: dict, *names: str):
    for name in names:
        if name in section:
            return section[name]
    return None


def int_or_hex_equals(value, expected: int) -> bool:
    if value == expected:
        return True
    if isinstance(value, str):
        try:
            return int(value, 0) == expected
        except ValueError:
            return False
    return False



class WindowsSteamSupportTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.app = load_app_module()

    def test_current_platform_id_normalizes_win32_amd64(self):
        with windows_platform(self.app, "AMD64"):
            self.assertEqual(self.app.current_platform_id(), "windows-x86_64")

    def test_detect_steam_install_uses_pe_architecture_for_runtime_platform(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            root = Path(tmpdir)
            steamapps = root / "steamapps"
            install_dir = steamapps / "common" / "Barony"
            install_dir.mkdir(parents=True)
            executable = install_dir / "barony.exe"
            write_synthetic_pe(executable)
            manifest = steamapps / "appmanifest_371970.acf"
            manifest.write_text(
                '"AppState"\n'
                "{\n"
                '    "appid"        "371970"\n'
                '    "name"         "Barony"\n'
                '    "installdir"   "Barony"\n'
                '    "buildid"      "123456789"\n'
                "}\n",
                encoding="utf-8",
            )

            with windows_platform(self.app, "ARM64"):
                payload, validation = self.app.detect_steam_install(manifest_arg=str(manifest))

            self.assertTrue(validation.ok, [problem.code for problem in validation.problems])
            self.assertIsNotNone(payload)
            self.assertEqual(payload["hostPlatform"], "windows-arm64")
            self.assertEqual(payload["platform"], "windows-x86_64")
            self.assertFalse(payload["compatibility"]["hostPlatformMatchesExecutable"])
            self.assertEqual(payload["compatibility"]["targetExecutableBitness"], 64)
            with windows_platform(self.app, "ARM64"), mock.patch.object(self.app, "process_bitness", return_value=64):
                profile_validation = self.app.validate_profile_steam_install({"runtime": {"steam": payload}})
            self.assertIn(
                "BML_STEAM_HOST_PLATFORM_MISMATCH",
                [problem.code for problem in profile_validation.problems],
            )

    def test_detect_steam_install_windows_manifest_resolves_exe_with_provenance(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            root = Path(tmpdir)
            steamapps = root / "steamapps"
            install_dir = steamapps / "common" / "Barony"
            install_dir.mkdir(parents=True)
            executable = install_dir / "barony.exe"
            executable_bytes = write_synthetic_pe(executable)
            manifest = steamapps / "appmanifest_371970.acf"
            manifest.write_text(
                '"AppState"\n'
                "{\n"
                '    "appid"        "371970"\n'
                '    "name"         "Barony"\n'
                '    "installdir"   "Barony"\n'
                '    "buildid"      "123456789"\n'
                "}\n",
                encoding="utf-8",
            )

            with windows_platform(self.app):
                payload, validation = self.app.detect_steam_install(manifest_arg=str(manifest))

            self.assertTrue(validation.ok, [problem.code for problem in validation.problems])
            self.assertIsNotNone(payload)
            self.assertEqual(Path(payload["executable"]).name, "barony.exe")
            self.assertNotEqual(Path(payload["executable"]).name, "barony.x86_64")
            self.assertEqual(payload["platform"], "windows-x86_64")
            self.assertEqual(payload["compatibility"]["launcherProcessBitness"], struct.calcsize("P") * 8)
            self.assertEqual(payload["compatibility"]["targetExecutableBitness"], 64)
            self.assertEqual(
                payload["compatibility"]["launcherProcessMatchesExecutable"],
                struct.calcsize("P") * 8 == 64,
            )
            self.assertEqual(payload["buildId"], "123456789")
            self.assertEqual(payload["executableSha256"], hashlib.sha256(executable_bytes).hexdigest())
            provenance = payload.get("provenance")
            self.assertIsInstance(provenance, dict)
            self.assertEqual(provenance.get("sha256"), payload["executableSha256"])
            self.assertIsInstance(provenance.get("pe"), dict)

    def test_validate_profile_steam_install_rejects_launcher_bitness_mismatch(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            root = Path(tmpdir)
            steamapps = root / "steamapps"
            install_dir = steamapps / "common" / "Barony"
            install_dir.mkdir(parents=True)
            write_synthetic_pe(install_dir / "barony.exe")
            manifest = steamapps / "appmanifest_371970.acf"
            manifest.write_text(
                '"AppState"\n'
                "{\n"
                '    "appid"        "371970"\n'
                '    "name"         "Barony"\n'
                '    "installdir"   "Barony"\n'
                '    "buildid"      "123456789"\n'
                "}\n",
                encoding="utf-8",
            )

            with windows_platform(self.app), mock.patch.object(self.app, "process_bitness", return_value=32):
                payload, validation = self.app.detect_steam_install(manifest_arg=str(manifest))
                self.assertTrue(validation.ok, [problem.code for problem in validation.problems])
                profile = {"runtime": {"steam": payload}}
                profile_validation = self.app.validate_profile_steam_install(profile)

            self.assertIn(
                "BML_STEAM_LAUNCHER_BITNESS_MISMATCH",
                [problem.code for problem in profile_validation.problems],
            )

    def test_runtime_register_requires_windows_launcher_helper(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            root = Path(tmpdir)
            steam_executable = root / "barony.exe"
            write_synthetic_pe(steam_executable)
            hook_library = root / "bml_hook_win.dll"
            hook_library.write_bytes(b"synthetic hook placeholder")
            hook_manifest = root / "hook-manifest.json"
            hook_manifest.write_text('{ "status": "unsupported", "failClosed": true, "hook": { "implemented": false } }', encoding="utf-8")
            runtime_info = root / "runtime-info.json"
            runtime_info.write_text(
                "{\n"
                '  "runtimeId": "barony-bml-runtime-stash-windows-test",\n'
                '  "runtimeVersion": "0.1.0",\n'
                '  "contractVersions": ["0.1.0"],\n'
                '  "platforms": [{"platform": "windows-x86_64"}]\n'
                "}\n",
                encoding="utf-8",
            )
            args = SimpleNamespace(
                registry=str(root / "registry.json"),
                runtime_strategy="installed-binary-hook",
                platform=None,
                steam_executable=str(steam_executable),
                executable=None,
                hook_library=str(hook_library),
                hook_manifest=str(hook_manifest),
                launcher_helper=None,
                runtime_info=str(runtime_info),
                id="windows-test-runtime",
                steam_build_id="22630456",
                steam_app_id="371970",
                steam_executable_build_id=None,
                game_version_string=None,
            )

            with windows_platform(self.app), mock.patch.object(self.app, "process_bitness", return_value=32):
                output = io.StringIO()
                with contextlib.redirect_stdout(output):
                    exit_code = self.app.command_runtime_register(args)

            self.assertEqual(exit_code, 1)
            self.assertIn("BML_RUNTIME_LAUNCHER_HELPER_MISSING", output.getvalue())
            self.assertIn("BML_HOOK_MANIFEST_UNSUPPORTED", output.getvalue())
            self.assertFalse((root / "registry.json").exists())

    def test_launcher_helper_must_match_windows_runtime_platform(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            helper = Path(tmpdir) / "bml_launcher_win.exe"
            write_synthetic_pe(helper, machine=0x014C)

            result = self.app.validate_launcher_helper(helper, "windows-x86_64")

            self.assertIn(
                "BML_RUNTIME_LAUNCHER_HELPER_PLATFORM_MISMATCH",
                [problem.code for problem in result.problems],
            )

    def test_hook_manifest_target_validation_rejects_wrong_build_and_hash(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            hook_manifest = Path(tmpdir) / "hook-manifest.json"
            hook_manifest.write_text(
                "{\n"
                '  "runtimeStrategy": "installed-binary-hook",\n'
                '  "storefront": "steam",\n'
                '  "steamAppId": "371970",\n'
                '  "steamBuildId": "wrong-build",\n'
                '  "platform": "windows-x86_64",\n'
                '  "executable": { "sha256": "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa" },\n'
                '  "hook": { "implemented": true }\n'
                "}\n",
                encoding="utf-8",
            )

            result = self.app.validate_hook_manifest_target(
                hook_manifest,
                "windows-x86_64",
                "371970",
                "22630456",
                "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb",
            )

            codes = [problem.code for problem in result.problems]
            self.assertIn("BML_HOOK_MANIFEST_TARGET_MISMATCH", codes)
            self.assertIn("BML_HOOK_MANIFEST_EXECUTABLE_SHA_MISMATCH", codes)

    def test_windows_runtime_manifest_uses_launcher_helper_as_launch_executable(self):
        profile = {
            "profile": {"id": "test-profile"},
            "runtime": {
                "baronyExecutable": "C:/Games/Barony/barony.exe",
                "gameSource": "steam",
                "steam": {"appId": "371970", "buildId": "22630456"},
            },
        }
        package = self.app.LoadedPackage({"id": "jml.test"}, Path(__file__), Path("."))
        runtime_info = {"runtimeId": "barony-bml-runtime", "runtimeVersion": "0.1.0"}
        runtime_registration = {
            "id": "windows-runtime",
            "runtimeStrategy": "installed-binary-hook",
            "platform": "windows-x86_64",
            "steamExecutable": "C:/Games/Barony/barony.exe",
            "launcherHelper": "C:/Tools/bml_launcher_win.exe",
            "launcherHelperSha256": "1" * 64,
            "launcherHelperBitness": 64,
        }

        manifest = self.app.build_runtime_manifest(
            profile,
            Path("."),
            package,
            runtime_info,
            Path("C:/Games/Barony/barony.exe"),
            runtime_registration,
        )

        self.assertEqual(manifest["launch"]["launchExecutable"], "C:/Tools/bml_launcher_win.exe")
        self.assertEqual(manifest["launch"]["targetExecutable"], "C:\\Games\\Barony\\barony.exe")
        self.assertEqual(manifest["launch"]["launcherHelper"], "C:/Tools/bml_launcher_win.exe")
        self.assertEqual(manifest["launch"]["adapterMode"], "windows-launcher-helper")
        self.assertEqual(manifest["launch"]["adapterStatus"], "available")
        env = self.app.launch_environment(profile, Path("."), Path("runtime-manifest.json"), runtime_registration)
        self.assertEqual(env["BML_TARGET_EXECUTABLE"], "C:/Games/Barony/barony.exe")
        self.assertEqual(env["BML_LAUNCHER_HELPER"], "C:/Tools/bml_launcher_win.exe")

    def test_hook_library_name_must_match_manifest(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            root = Path(tmpdir)
            hook_manifest = root / "hook-manifest.json"
            hook_manifest.write_text('{ "hook": { "libraryName": "bml_hook_win.dll", "implemented": true } }', encoding="utf-8")
            wrong_library = root / "different.dll"
            wrong_library.write_bytes(b"placeholder")

            result = self.app.validate_hook_library_matches_manifest(hook_manifest, wrong_library)

            self.assertIn(
                "BML_HOOK_LIBRARY_MANIFEST_NAME_MISMATCH",
                [problem.code for problem in result.problems],
            )

    def test_supported_hook_manifest_requires_library_sha(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            root = Path(tmpdir)
            hook_library = root / "bml_hook_win.dll"
            hook_library.write_bytes(b"actual hook bytes")
            hook_manifest = root / "hook-manifest.json"
            hook_manifest.write_text(
                '{ "hook": { "libraryName": "bml_hook_win.dll", "entrypoint": "bml_hook_init", "implemented": true } }',
                encoding="utf-8",
            )

            result = self.app.validate_hook_library_matches_manifest(hook_manifest, hook_library)

            self.assertIn(
                "BML_HOOK_MANIFEST_LIBRARY_SHA_MISSING",
                [problem.code for problem in result.problems],
            )

    def test_supported_hook_manifest_rejects_empty_library_sha(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            root = Path(tmpdir)
            hook_library = root / "bml_hook_win.dll"
            hook_library.write_bytes(b"actual hook bytes")
            hook_manifest = root / "hook-manifest.json"
            hook_manifest.write_text(
                '{ "hook": { "libraryName": "bml_hook_win.dll", "entrypoint": "bml_hook_init", "implemented": true, "sha256": "" } }',
                encoding="utf-8",
            )

            result = self.app.validate_hook_library_matches_manifest(hook_manifest, hook_library)

            self.assertIn(
                "BML_HOOK_MANIFEST_LIBRARY_SHA_MISSING",
                [problem.code for problem in result.problems],
            )

    def test_supported_hook_manifest_rejects_invalid_library_sha(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            root = Path(tmpdir)
            hook_library = root / "bml_hook_win.dll"
            hook_library.write_bytes(b"actual hook bytes")
            hook_manifest = root / "hook-manifest.json"
            hook_manifest.write_text(
                '{ "hook": { "libraryName": "bml_hook_win.dll", "entrypoint": "bml_hook_init", "implemented": true, "sha256": "xyz" } }',
                encoding="utf-8",
            )

            result = self.app.validate_hook_library_matches_manifest(hook_manifest, hook_library)

            self.assertIn(
                "BML_HOOK_MANIFEST_LIBRARY_SHA_INVALID",
                [problem.code for problem in result.problems],
            )

    def test_hook_library_sha_must_match_manifest(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            root = Path(tmpdir)
            hook_library = root / "bml_hook_win.dll"
            hook_library.write_bytes(b"actual hook bytes")
            hook_manifest = root / "hook-manifest.json"
            hook_manifest.write_text(
                '{ "hook": { "libraryName": "bml_hook_win.dll", "entrypoint": "bml_hook_init", "implemented": true, "sha256": "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa" } }',
                encoding="utf-8",
            )

            result = self.app.validate_hook_library_matches_manifest(hook_manifest, hook_library)

            self.assertIn(
                "BML_HOOK_LIBRARY_SHA_MISMATCH",
                [problem.code for problem in result.problems],
            )

    def test_hook_manifest_entrypoint_must_be_bml_hook_init(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            hook_manifest = Path(tmpdir) / "hook-manifest.json"
            hook_manifest.write_text(
                '{ "hook": { "libraryName": "bml_hook_win.dll", "entrypoint": "wrong_init", "implemented": true } }',
                encoding="utf-8",
            )

            result = self.app.validate_hook_manifest_supported(hook_manifest)

            self.assertIn(
                "BML_HOOK_MANIFEST_ENTRYPOINT_UNSUPPORTED",
                [problem.code for problem in result.problems],
            )

    def test_supported_windows_manifest_allows_nested_stash_failclosed_checklist(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            hook_manifest = Path(tmpdir) / "hook-manifest.json"
            hook_manifest.write_text(
                "{\n"
                '  "status": "noop-validated",\n'
                '  "failClosed": false,\n'
                '  "stashTargetResolution": {\n'
                '    "status": "missing-windows-rva-or-signature-map",\n'
                '    "failClosed": true,\n'
                '    "claimBoundary": "target-resolution-checklist-only"\n'
                "  },\n"
                '  "hook": { "libraryName": "bml_hook_win.dll", "entrypoint": "bml_hook_init", "implemented": true }\n'
                "}\n",
                encoding="utf-8",
            )

            result = self.app.validate_hook_manifest_supported(hook_manifest)

            self.assertTrue(result.ok, [problem.code for problem in result.problems])
            self.assertNotIn(
                "BML_HOOK_MANIFEST_UNSUPPORTED",
                [problem.code for problem in result.problems],
            )

    def test_hook_library_must_export_manifest_entrypoint(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            root = Path(tmpdir)
            hook_manifest = root / "hook-manifest.json"
            hook_manifest.write_text(
                '{ "hook": { "libraryName": "bml_hook_win.dll", "entrypoint": "bml_hook_init", "implemented": true } }',
                encoding="utf-8",
            )
            hook_library = root / "bml_hook_win.dll"
            write_synthetic_pe(hook_library)

            result = self.app.validate_hook_library_exports_entrypoint(hook_manifest, hook_library)

            self.assertIn(
                "BML_HOOK_LIBRARY_ENTRYPOINT_MISSING",
                [problem.code for problem in result.problems],
            )

    def test_hook_library_platform_must_match_windows_runtime(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            hook_library = Path(tmpdir) / "bml_hook_win.dll"
            write_synthetic_pe(hook_library, machine=0x014C)

            result = self.app.validate_hook_library_platform(hook_library, "windows-x86_64")

            self.assertIn(
                "BML_HOOK_LIBRARY_PLATFORM_MISMATCH",
                [problem.code for problem in result.problems],
            )

    def test_executable_provenance_includes_pe_machine_bitness_and_sections(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            executable = Path(tmpdir) / "barony.exe"
            executable_bytes = write_synthetic_pe(executable)

            with windows_platform(self.app):
                provenance = self.app.executable_provenance(executable)

            self.assertEqual(provenance["sha256"], hashlib.sha256(executable_bytes).hexdigest())
            self.assertIn("buildId", provenance)
            self.assertEqual(provenance["gameVersionString"], "v4.2.1")
            pe = provenance.get("pe")
            self.assertIsInstance(pe, dict)
            self.assertTrue(is_amd64_machine(pe.get("machine")), pe.get("machine"))
            self.assertIn(pe.get("bitness"), {64, "64", "x64", "x86_64"})
            sections = pe.get("sections")
            self.assertIsInstance(sections, list)
            self.assertGreaterEqual(len(sections), 1)
            text = sections[0]
            self.assertEqual(text.get("name"), ".text")
            self.assertTrue(int_or_hex_equals(section_value(text, "virtualAddress", "virtual_address", "rva"), 0x1000), text)
            self.assertTrue(int_or_hex_equals(section_value(text, "virtualSize", "virtual_size"), 0x180), text)
            self.assertEqual(text["rawSize"], 0x200)
            self.assertEqual(text["rawPointer"], 0x200)

    def test_launch_environment_windows_does_not_set_unix_loader_variables(self):
        profile = {
            "runtime": {
                "steam": {
                    "appId": "371970",
                    "installPath": r"C:\Games\Steam\steamapps\common\Barony",
                }
            }
        }
        runtime = {
            "runtimeStrategy": "installed-binary-hook",
            "hookLibrary": r"C:\Games\BaronyModLoader\bml.dll",
            "hookManifest": r"C:\Games\BaronyModLoader\hook-manifest.json",
        }

        with tempfile.TemporaryDirectory() as tmpdir:
            root = Path(tmpdir)
            with mock.patch.dict(os.environ, {}, clear=True), windows_platform(self.app):
                env = self.app.launch_environment(profile, root / "profile", root / "runtime-manifest.json", runtime)

        self.assertNotIn("LD_PRELOAD", env)
        self.assertNotIn("DYLD_INSERT_LIBRARIES", env)
        self.assertNotIn("LD_LIBRARY_PATH", env)


if __name__ == "__main__":
    unittest.main()
