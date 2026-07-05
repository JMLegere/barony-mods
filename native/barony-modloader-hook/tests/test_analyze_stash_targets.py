from __future__ import annotations

import contextlib
import io
import importlib.util
import json
import struct
import sys
import tempfile
import unittest
import uuid
from pathlib import Path

TOOL_PATH = Path(__file__).resolve().parents[1] / "tools" / "analyze_stash_targets.py"


def load_tool_module():
    spec = importlib.util.spec_from_file_location("analyze_stash_targets_tests", TOOL_PATH)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"could not load analyzer from {TOOL_PATH}")
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


class AnalyzeStashTargetsPeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tool = load_tool_module()

    def write_pe_fixture(
        self,
        root: Path,
        manifest: dict[str, object],
        code: bytes = b"\x90" * 16,
        code_view: dict[str, object] | None = None,
        data: bytes | None = None,
        data_raw_size: int | None = None,
    ) -> tuple[Path, Path]:
        manifest_path = root / "manifest.json"
        executable_path = root / "barony.exe"
        manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
        e_lfanew = 0x80
        optional_size = 0xF0
        header_size = 0x200
        text_raw_pointer = 0x200
        text_raw_size = 0x200
        pdata_raw_pointer = 0x400
        pdata_raw_size = 0x200
        data_raw_pointer = 0x600
        create_data_section = data is not None or data_raw_size is not None
        effective_data_raw_size = 0x200 if data_raw_size is None else data_raw_size
        dos = bytearray(64)
        dos[:2] = b"MZ"
        struct.pack_into("<I", dos, 0x3C, e_lfanew)
        coff = struct.pack("<HHIIIHH", 0x8664, 3 if create_data_section else 2, 0, 0, 0, optional_size, 0x0022)
        optional = bytearray(optional_size)
        struct.pack_into("<H", optional, 0, 0x20B)
        struct.pack_into("<II", optional, 0xB8, 0x2000, 12)
        text_section = struct.pack("<8sIIIIIIHHI", b".text\0\0\0", 0x180, 0x1000, text_raw_size, text_raw_pointer, 0, 0, 0, 0, 0x60000020)
        pdata_section = struct.pack("<8sIIIIIIHHI", b".pdata\0\0", 0x20, 0x2000, pdata_raw_size, pdata_raw_pointer, 0, 0, 0, 0, 0x40000040)
        data_section = struct.pack("<8sIIIIIIHHI", b".data\0\0\0", 0x80, 0x3000, effective_data_raw_size, data_raw_pointer if effective_data_raw_size else 0, 0, 0, 0, 0, 0xC0000040)
        text_body = bytearray(text_raw_size)
        text_body[: len(code)] = code
        pdata_body = bytearray(pdata_raw_size)
        struct.pack_into("<III", pdata_body, 0, 0x1000, 0x1100, 0)
        data_body = bytearray(effective_data_raw_size)
        if data is not None and effective_data_raw_size:
            data_body[: len(data)] = data
        if code_view is not None:
            debug_rva = 0x1100
            rsds_rva = 0x1120
            debug_offset = debug_rva - 0x1000
            rsds_offset = rsds_rva - 0x1000
            pdb_path = str(code_view.get("pdbPath") or "C:/build/barony.pdb")
            age = int(code_view.get("age") or 1)
            guid = uuid.UUID(str(code_view.get("guid") or "00000000-0000-0000-0000-000000000000"))
            rsds = b"RSDS" + guid.bytes_le + struct.pack("<I", age) + pdb_path.encode("utf-8") + b"\0"
            text_body[rsds_offset : rsds_offset + len(rsds)] = rsds
            struct.pack_into("<IIHHIIII", text_body, debug_offset, 0, 0, 0, 0, 2, len(rsds), rsds_rva, text_raw_pointer + rsds_offset)
            struct.pack_into("<II", optional, 0xA0, debug_rva, 28)
        image = bytearray()
        image.extend(dos)
        image.extend(b"\0" * (e_lfanew - len(image)))
        image.extend(b"PE\0\0")
        image.extend(coff)
        image.extend(optional)
        image.extend(text_section)
        image.extend(pdata_section)
        if create_data_section:
            image.extend(data_section)
        image.extend(b"\0" * (header_size - len(image)))
        image.extend(text_body)
        image.extend(pdata_body)
        if create_data_section and effective_data_raw_size:
            image.extend(data_body)
        executable_path.write_bytes(bytes(image))
        return manifest_path, executable_path

    def test_pe_report_fails_loudly_for_empty_target_manifest(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            manifest_path, executable_path = self.write_pe_fixture(
                Path(tmpdir),
                {
                    "steamAppId": "371970",
                    "steamBuildId": "22630456",
                    "platform": "windows-x86_64",
                    "executable": {"sha256": "fixture"},
                    "symbols": [],
                    "hookTargets": [],
                },
            )
            report = self.tool.build_report(manifest_path, executable_path)
            self.assertEqual(report["summary"]["targetResolutionStatus"], "empty-windows-target-manifest")
            self.assertEqual(report["summary"]["declaredSymbols"], 0)

    def test_pe_report_fails_closed_when_codeview_identity_missing(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            manifest_path, executable_path = self.write_pe_fixture(
                Path(tmpdir),
                {
                    "steamAppId": "371970",
                    "steamBuildId": "22630456",
                    "platform": "windows-x86_64",
                    "executable": {
                        "sha256": "fixture",
                        "pe": {
                            "codeView": {
                                "signature": "RSDS",
                                "guid": "b1ca0707-2b79-4e02-b9c5-1a8e0ecfd525",
                                "age": 2,
                                "pdbPath": "C:/Users/Ben/source/repos/Barony/VS.2015/x64/Steam Crossplay/barony.pdb",
                            }
                        },
                    },
                    "symbols": [
                        {
                            "id": "act_chest",
                            "kind": "function",
                            "demangledLabel": "actChest(Entity*)",
                            "required": True,
                            "stashCapabilities": ["void_chest_binding"],
                            "windowsResolution": {
                                "status": "resolved",
                                "rva": 4096,
                                "byteSignature": None,
                            },
                        }
                    ],
                    "hookTargets": [
                        {
                            "id": "stash_void_chest_binding",
                            "capability": "void_chest_binding",
                            "required": True,
                            "targetSymbolIds": ["act_chest"],
                        }
                    ],
                },
            )
            report = self.tool.build_report(manifest_path, executable_path)
            self.assertFalse(report["executable"]["codeViewMatchesManifest"])
            self.assertEqual(report["summary"]["targetResolutionStatus"], "windows-codeview-identity-mismatch")

    def test_pe_report_matches_manifest_codeview_identity(self):
        code_view = {
            "signature": "RSDS",
            "guid": "b1ca0707-2b79-4e02-b9c5-1a8e0ecfd525",
            "age": 2,
            "pdbPath": "C:/Users/Ben/source/repos/Barony/VS.2015/x64/Steam Crossplay/barony.pdb",
        }
        with tempfile.TemporaryDirectory() as tmpdir:
            manifest_path, executable_path = self.write_pe_fixture(
                Path(tmpdir),
                {
                    "steamAppId": "371970",
                    "steamBuildId": "22630456",
                    "platform": "windows-x86_64",
                    "executable": {"sha256": "fixture", "pe": {"codeView": code_view}},
                    "symbols": [],
                    "hookTargets": [],
                },
                code_view=code_view,
            )
            report = self.tool.build_report(manifest_path, executable_path)
            self.assertTrue(report["executable"]["codeViewMatchesManifest"])
            self.assertEqual(report["executable"]["codeView"]["symbolServerKey"], "barony.pdb/B1CA07072B794E02B9C51A8E0ECFD5252/barony.pdb")
            self.assertEqual(report["summary"]["targetResolutionStatus"], "empty-windows-target-manifest")

    def test_pe_report_fails_closed_on_codeview_age_mismatch(self):
        actual_code_view = {
            "signature": "RSDS",
            "guid": "b1ca0707-2b79-4e02-b9c5-1a8e0ecfd525",
            "age": 2,
            "pdbPath": "C:/Users/Ben/source/repos/Barony/VS.2015/x64/Steam Crossplay/barony.pdb",
        }
        expected_code_view = dict(actual_code_view)
        expected_code_view["age"] = 3
        with tempfile.TemporaryDirectory() as tmpdir:
            manifest_path, executable_path = self.write_pe_fixture(
                Path(tmpdir),
                {
                    "steamAppId": "371970",
                    "steamBuildId": "22630456",
                    "platform": "windows-x86_64",
                    "executable": {"sha256": "fixture", "pe": {"codeView": expected_code_view}},
                    "symbols": [
                        {
                            "id": "act_chest",
                            "kind": "function",
                            "demangledLabel": "actChest(Entity*)",
                            "required": True,
                            "stashCapabilities": ["void_chest_binding"],
                            "windowsResolution": {
                                "status": "resolved",
                                "rva": 4096,
                                "byteSignature": None,
                            },
                        }
                    ],
                    "hookTargets": [
                        {
                            "id": "stash_void_chest_binding",
                            "capability": "void_chest_binding",
                            "required": True,
                            "targetSymbolIds": ["act_chest"],
                        }
                    ],
                },
                code_view=actual_code_view,
            )
            report = self.tool.build_report(manifest_path, executable_path)
            self.assertFalse(report["executable"]["codeViewMatchesManifest"])
            self.assertEqual(report["summary"]["targetResolutionStatus"], "windows-codeview-identity-mismatch")
            self.assertEqual(report["summary"]["hookGroups"], 0)
            self.assertFalse(report["summary"]["playableBehaviorClaimed"])

    def test_pe_report_lists_unresolved_symbol_targets(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            manifest_path, executable_path = self.write_pe_fixture(
                Path(tmpdir),
                {
                    "steamAppId": "371970",
                    "steamBuildId": "22630456",
                    "platform": "windows-x86_64",
                    "executable": {"sha256": "fixture"},
                    "symbols": [
                        {
                            "id": "act_chest",
                            "kind": "function",
                            "demangledLabel": "actChest(Entity*)",
                            "required": True,
                            "stashCapabilities": ["void_chest_binding"],
                            "windowsResolution": {
                                "status": "unresolved",
                                "rva": None,
                                "byteSignature": None,
                            },
                        }
                    ],
                    "hookTargets": [
                        {
                            "id": "stash_void_chest_binding",
                            "capability": "void_chest_binding",
                            "required": True,
                            "targetSymbolIds": ["act_chest"],
                        }
                    ],
                },
            )
            report = self.tool.build_report(manifest_path, executable_path)
            self.assertEqual(report["summary"]["targetResolutionStatus"], "missing-windows-rva-or-signature-map")
            self.assertEqual(report["summary"]["blockedTargets"], 1)
            target = report["hooks"][0]["targets"][0]
            self.assertEqual(target["id"], "act_chest")
            self.assertEqual(target["status"], "blocked")
            self.assertEqual(target["resolutionStatus"], "unresolved")
            self.assertIn("mapped RVA", target["message"])

    def test_cli_returns_nonzero_for_incomplete_pe_target_map(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            manifest_path, executable_path = self.write_pe_fixture(
                Path(tmpdir),
                {
                    "steamAppId": "371970",
                    "steamBuildId": "22630456",
                    "platform": "windows-x86_64",
                    "executable": {"sha256": "fixture"},
                    "symbols": [
                        {
                            "id": "act_chest",
                            "kind": "function",
                            "demangledLabel": "actChest(Entity*)",
                            "required": True,
                            "stashCapabilities": ["void_chest_binding"],
                            "windowsResolution": {
                                "status": "unresolved",
                                "rva": None,
                                "byteSignature": None,
                            },
                        }
                    ],
                    "hookTargets": [
                        {
                            "id": "stash_void_chest_binding",
                            "capability": "void_chest_binding",
                            "required": True,
                            "targetSymbolIds": ["act_chest"],
                        }
                    ],
                },
            )
            with contextlib.redirect_stdout(io.StringIO()):
                exit_code = self.tool.main(["--manifest", str(manifest_path), "--executable", str(executable_path)])
            self.assertEqual(exit_code, 2)

    def test_pe_mapped_non_begin_rva_stays_unresolved(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            manifest_path, executable_path = self.write_pe_fixture(
                Path(tmpdir),
                {
                    "steamAppId": "371970",
                    "steamBuildId": "22630456",
                    "platform": "windows-x86_64",
                    "executable": {"sha256": "fixture"},
                    "symbols": [
                        {
                            "id": "act_chest",
                            "kind": "function",
                            "demangledLabel": "actChest(Entity*)",
                            "required": True,
                            "stashCapabilities": ["void_chest_binding"],
                            "windowsResolution": {
                                "status": "resolved",
                                "rva": 4097,
                                "byteSignature": None,
                            },
                        }
                    ],
                    "hookTargets": [
                        {
                            "id": "stash_void_chest_binding",
                            "capability": "void_chest_binding",
                            "required": True,
                            "targetSymbolIds": ["act_chest"],
                        }
                    ],
                },
            )
            report = self.tool.build_report(manifest_path, executable_path)
            self.assertEqual(report["summary"]["targetResolutionStatus"], "missing-windows-rva-or-signature-map")
            target = report["hooks"][0]["targets"][0]
            self.assertEqual(target["status"], "blocked")
            self.assertEqual(target["resolutionStatus"], "unresolved")
            self.assertFalse(target["runtimeFunctionStart"])
            self.assertIn("BeginAddress", target["message"])

    def test_pe_install_validated_leaf_rva_counts_as_resolved(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            manifest_path, executable_path = self.write_pe_fixture(
                Path(tmpdir),
                {
                    "steamAppId": "371970",
                    "steamBuildId": "22630456",
                    "platform": "windows-x86_64",
                    "executable": {"sha256": "fixture"},
                    "symbols": [
                        {
                            "id": "act_chest",
                            "kind": "function",
                            "demangledLabel": "actChest(Entity*)",
                            "required": True,
                            "stashCapabilities": ["void_chest_binding"],
                            "windowsResolution": {
                                "status": "resolved",
                                "rva": 4097,
                                "byteSignature": None,
                                "installProbeValidated": True,
                            },
                        }
                    ],
                    "hookTargets": [
                        {
                            "id": "stash_void_chest_binding",
                            "capability": "void_chest_binding",
                            "required": True,
                            "targetSymbolIds": ["act_chest"],
                        }
                    ],
                },
            )
            report = self.tool.build_report(manifest_path, executable_path)
            self.assertEqual(report["summary"]["targetResolutionStatus"], "windows-target-map-complete-detour-analysis-pending")
            target = report["hooks"][0]["targets"][0]
            self.assertEqual(target["status"], "blocked")
            self.assertEqual(target["resolutionStatus"], "resolved")
            self.assertFalse(target["runtimeFunctionStart"])
            self.assertIn("install-validated leaf function", target["message"])

    def test_pe_resolved_data_target_stays_resolved(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            manifest_path, executable_path = self.write_pe_fixture(
                Path(tmpdir),
                {
                    "steamAppId": "371970",
                    "steamBuildId": "22630456",
                    "platform": "windows-x86_64",
                    "executable": {"sha256": "fixture"},
                    "symbols": [
                        {
                            "id": "stats",
                            "kind": "data",
                            "demangledLabel": "stats",
                            "required": True,
                            "stashCapabilities": ["persistent_inventory"],
                            "windowsResolution": {
                                "status": "resolved",
                                "rva": 12289,
                                "byteSignature": None,
                            },
                        }
                    ],
                    "hookTargets": [
                        {
                            "id": "stash_inventory_persistence",
                            "capability": "persistent_inventory",
                            "required": True,
                            "targetSymbolIds": ["stats"],
                        }
                    ],
                },
                data=b"\x11\x22\x33\x44",
            )
            report = self.tool.build_report(manifest_path, executable_path)
            self.assertEqual(report["summary"]["targetResolutionStatus"], "windows-target-map-complete-detour-analysis-pending")
            target = report["hooks"][0]["targets"][0]
            self.assertEqual(target["status"], "blocked")
            self.assertEqual(target["resolutionStatus"], "resolved")
            self.assertIsNone(target["runtimeFunctionStart"])
            self.assertIsNone(target["prologue"])
            self.assertIn("PE data target", target["message"])

    def test_pe_zero_fill_data_target_stays_resolved(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            manifest_path, executable_path = self.write_pe_fixture(
                Path(tmpdir),
                {
                    "steamAppId": "371970",
                    "steamBuildId": "22630456",
                    "platform": "windows-x86_64",
                    "executable": {"sha256": "fixture"},
                    "symbols": [
                        {
                            "id": "shoparea",
                            "kind": "data",
                            "demangledLabel": "shoparea",
                            "required": True,
                            "stashCapabilities": ["placement_shop"],
                            "windowsResolution": {
                                "status": "resolved",
                                "rva": 12288,
                                "byteSignature": None,
                            },
                        }
                    ],
                    "hookTargets": [
                        {
                            "id": "stash_shop_placement",
                            "capability": "placement_shop",
                            "required": True,
                            "targetSymbolIds": ["shoparea"],
                        }
                    ],
                },
                data_raw_size=0,
            )
            report = self.tool.build_report(manifest_path, executable_path)
            self.assertEqual(report["summary"]["targetResolutionStatus"], "windows-target-map-complete-detour-analysis-pending")
            target = report["hooks"][0]["targets"][0]
            self.assertEqual(target["status"], "blocked")
            self.assertEqual(target["resolutionStatus"], "resolved")
            self.assertIsNone(target["runtimeFunctionStart"])
            self.assertIsNone(target["prologue"])
            self.assertIn("writable data section", target["message"])

    def test_pe_resolved_target_still_blocked_without_detour_analysis(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            manifest_path, executable_path = self.write_pe_fixture(
                Path(tmpdir),
                {
                    "steamAppId": "371970",
                    "steamBuildId": "22630456",
                    "platform": "windows-x86_64",
                    "executable": {"sha256": "fixture"},
                    "symbols": [
                        {
                            "id": "act_chest",
                            "kind": "function",
                            "demangledLabel": "actChest(Entity*)",
                            "required": True,
                            "stashCapabilities": ["void_chest_binding"],
                            "windowsResolution": {
                                "status": "resolved",
                                "rva": 4096,
                                "byteSignature": None,
                            },
                        }
                    ],
                    "hookTargets": [
                        {
                            "id": "stash_void_chest_binding",
                            "capability": "void_chest_binding",
                            "required": True,
                            "targetSymbolIds": ["act_chest"],
                        }
                    ],
                },
            )
            report = self.tool.build_report(manifest_path, executable_path)
            self.assertEqual(report["summary"]["readyTargets"], 0)
            self.assertEqual(report["summary"]["targetResolutionStatus"], "windows-target-map-complete-detour-analysis-pending")
            self.assertEqual(report["summary"]["blockedTargets"], 1)
            target = report["hooks"][0]["targets"][0]
            self.assertEqual(target["status"], "blocked")
            self.assertEqual(target["resolutionStatus"], "resolved")
            self.assertEqual(target["prologue"]["status"], "ready")
            self.assertIn("native Windows detour installation remains blocked", target["message"])

    def test_cli_returns_nonzero_for_mapped_rva_until_windows_install_is_supported(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            manifest_path, executable_path = self.write_pe_fixture(
                Path(tmpdir),
                {
                    "steamAppId": "371970",
                    "steamBuildId": "22630456",
                    "platform": "windows-x86_64",
                    "executable": {"sha256": "fixture"},
                    "symbols": [
                        {
                            "id": "act_chest",
                            "kind": "function",
                            "demangledLabel": "actChest(Entity*)",
                            "required": True,
                            "stashCapabilities": ["void_chest_binding"],
                            "windowsResolution": {
                                "status": "resolved",
                                "rva": 4096,
                                "byteSignature": None,
                            },
                        }
                    ],
                    "hookTargets": [
                        {
                            "id": "stash_void_chest_binding",
                            "capability": "void_chest_binding",
                            "required": True,
                            "targetSymbolIds": ["act_chest"],
                        }
                    ],
                },
            )
            with contextlib.redirect_stdout(io.StringIO()):
                exit_code = self.tool.main(["--manifest", str(manifest_path), "--executable", str(executable_path)])
            self.assertEqual(exit_code, 2)

    def test_pe_mapped_unsafe_prologue_blocks_target_map_completion(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            manifest_path, executable_path = self.write_pe_fixture(
                Path(tmpdir),
                {
                    "steamAppId": "371970",
                    "steamBuildId": "22630456",
                    "platform": "windows-x86_64",
                    "executable": {"sha256": "fixture"},
                    "symbols": [
                        {
                            "id": "act_chest",
                            "kind": "function",
                            "demangledLabel": "actChest(Entity*)",
                            "required": True,
                            "stashCapabilities": ["void_chest_binding"],
                            "windowsResolution": {
                                "status": "resolved",
                                "rva": 4096,
                                "byteSignature": None,
                            },
                        }
                    ],
                    "hookTargets": [
                        {
                            "id": "stash_void_chest_binding",
                            "capability": "void_chest_binding",
                            "required": True,
                            "targetSymbolIds": ["act_chest"],
                        }
                    ],
                },
                code=b"\xc3" + b"\x90" * 15,
            )
            report = self.tool.build_report(manifest_path, executable_path)
            self.assertEqual(report["summary"]["targetResolutionStatus"], "windows-target-map-complete-prologue-blocked")
            self.assertEqual(report["summary"]["readyTargets"], 0)
            target = report["hooks"][0]["targets"][0]
            self.assertEqual(target["status"], "blocked")
            self.assertEqual(target["resolutionStatus"], "resolved")
            self.assertEqual(target["prologue"]["status"], "blocked")
            self.assertIn("not safe", target["message"])

    def test_pe_signature_only_target_stays_unresolved_until_signature_scan_exists(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            manifest_path, executable_path = self.write_pe_fixture(
                Path(tmpdir),
                {
                    "steamAppId": "371970",
                    "steamBuildId": "22630456",
                    "platform": "windows-x86_64",
                    "executable": {"sha256": "fixture"},
                    "symbols": [
                        {
                            "id": "act_chest",
                            "kind": "function",
                            "demangledLabel": "actChest(Entity*)",
                            "required": True,
                            "stashCapabilities": ["void_chest_binding"],
                            "windowsResolution": {
                                "status": "resolved",
                                "rva": None,
                                "byteSignature": "90 90 90",
                            },
                        }
                    ],
                    "hookTargets": [
                        {
                            "id": "stash_void_chest_binding",
                            "capability": "void_chest_binding",
                            "required": True,
                            "targetSymbolIds": ["act_chest"],
                        }
                    ],
                },
            )
            report = self.tool.build_report(manifest_path, executable_path)
            self.assertEqual(report["summary"]["targetResolutionStatus"], "missing-windows-rva-or-signature-map")
            self.assertEqual(report["summary"]["readyTargets"], 0)
            self.assertEqual(report["summary"]["blockedTargets"], 1)
            target = report["hooks"][0]["targets"][0]
            self.assertEqual(target["status"], "blocked")
            self.assertEqual(target["resolutionStatus"], "unresolved")
            self.assertIn("signature scanning is not implemented", target["message"])

    def test_pe_unmapped_rva_stays_unresolved(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            manifest_path, executable_path = self.write_pe_fixture(
                Path(tmpdir),
                {
                    "steamAppId": "371970",
                    "steamBuildId": "22630456",
                    "platform": "windows-x86_64",
                    "executable": {"sha256": "fixture"},
                    "symbols": [
                        {
                            "id": "act_chest",
                            "kind": "function",
                            "demangledLabel": "actChest(Entity*)",
                            "required": True,
                            "stashCapabilities": ["void_chest_binding"],
                            "windowsResolution": {
                                "status": "resolved",
                                "rva": 0x5000,
                                "byteSignature": None,
                            },
                        }
                    ],
                    "hookTargets": [
                        {
                            "id": "stash_void_chest_binding",
                            "capability": "void_chest_binding",
                            "required": True,
                            "targetSymbolIds": ["act_chest"],
                        }
                    ],
                },
            )
            report = self.tool.build_report(manifest_path, executable_path)
            self.assertEqual(report["summary"]["targetResolutionStatus"], "missing-windows-rva-or-signature-map")
            target = report["hooks"][0]["targets"][0]
            self.assertEqual(target["status"], "blocked")
            self.assertEqual(target["resolutionStatus"], "unresolved")
            self.assertEqual(target["prologue"]["status"], "missing")
            self.assertIn("could not be mapped", target["message"])


if __name__ == "__main__":
    unittest.main()
