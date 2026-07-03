#!/usr/bin/env python3
"""Static Stash hook target readiness analyzer for installed Barony ELF builds.

This tool mirrors the native hook's conservative x86_64 patch-window decoder so
we can audit installed-executable target prologues without launching Barony.
It does not prove gameplay behavior and does not install detours.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import struct
import sys
from pathlib import Path
from typing import Any

PATCH_BYTES = 14
MAX_COPY_BYTES = 32
DECODER_ID = "fixture-safe-subset"
PATCH_STYLE = "rip-relative-indirect-jmp-absolute-slot"

SHT_SYMTAB = 2
SHT_DYNSYM = 11
STT_FUNC = 2
STT_OBJECT = 1
SHN_UNDEF = 0


def read_c_string(blob: bytes, offset: int) -> str:
    if offset < 0 or offset >= len(blob):
        return ""
    end = blob.find(b"\0", offset)
    if end == -1:
        end = len(blob)
    return blob[offset:end].decode("utf-8", errors="replace")


class Elf64Image:
    def __init__(self, path: Path) -> None:
        self.path = path
        self.data = path.read_bytes()
        self.sections: list[dict[str, Any]] = []
        self.symbols: dict[str, dict[str, Any]] = {}
        self._parse()

    def _parse(self) -> None:
        data = self.data
        if len(data) < 64 or data[:4] != b"\x7fELF":
            raise ValueError(f"{self.path} is not an ELF file")
        if data[4] != 2 or data[5] != 1:
            raise ValueError(f"{self.path} is not a little-endian ELF64 file")

        e_shoff = struct.unpack_from("<Q", data, 40)[0]
        e_shentsize = struct.unpack_from("<H", data, 58)[0]
        e_shnum = struct.unpack_from("<H", data, 60)[0]
        e_shstrndx = struct.unpack_from("<H", data, 62)[0]
        if e_shoff == 0 or e_shentsize == 0 or e_shnum == 0:
            raise ValueError(f"{self.path} has no section table")

        raw_sections: list[tuple[int, int, int, int, int, int, int, int, int, int]] = []
        for index in range(e_shnum):
            offset = e_shoff + index * e_shentsize
            if offset + 64 > len(data):
                raise ValueError(f"{self.path} has a truncated section table")
            raw_sections.append(struct.unpack_from("<IIQQQQIIQQ", data, offset))

        shstr = b""
        if 0 <= e_shstrndx < len(raw_sections):
            shstr_header = raw_sections[e_shstrndx]
            shstr = data[shstr_header[4] : shstr_header[4] + shstr_header[5]]

        for index, header in enumerate(raw_sections):
            name_offset, section_type, flags, address, file_offset, size, link, info, align, entsize = header
            self.sections.append(
                {
                    "index": index,
                    "name": read_c_string(shstr, name_offset),
                    "type": section_type,
                    "flags": flags,
                    "address": address,
                    "offset": file_offset,
                    "size": size,
                    "link": link,
                    "info": info,
                    "align": align,
                    "entsize": entsize,
                }
            )

        for section in self.sections:
            if section["type"] not in {SHT_SYMTAB, SHT_DYNSYM}:
                continue
            link = section["link"]
            if not (0 <= link < len(self.sections)):
                continue
            str_section = self.sections[link]
            strtab = data[str_section["offset"] : str_section["offset"] + str_section["size"]]
            entsize = section["entsize"] or 24
            count = section["size"] // entsize
            for index in range(count):
                sym_offset = section["offset"] + index * entsize
                if sym_offset + 24 > len(data):
                    break
                st_name, st_info, st_other, st_shndx, st_value, st_size = struct.unpack_from("<IBBHQQ", data, sym_offset)
                name = read_c_string(strtab, st_name)
                if not name:
                    continue
                symbol_type = st_info & 0x0F
                bind = st_info >> 4
                candidate = {
                    "name": name,
                    "value": st_value,
                    "size": st_size,
                    "type": symbol_type,
                    "bind": bind,
                    "sectionIndex": st_shndx,
                    "table": section["name"],
                }
                current = self.symbols.get(name)
                if current is None or self._symbol_score(candidate) > self._symbol_score(current):
                    self.symbols[name] = candidate

    @staticmethod
    def _symbol_score(symbol: dict[str, Any]) -> tuple[int, int, int, int]:
        return (
            1 if symbol["sectionIndex"] != SHN_UNDEF else 0,
            1 if symbol["bind"] != 0 else 0,
            1 if symbol["type"] in {STT_FUNC, STT_OBJECT} else 0,
            int(symbol["size"]),
        )

    def bytes_at_va(self, virtual_address: int, size: int) -> bytes | None:
        for section in self.sections:
            start = int(section["address"])
            end = start + int(section["size"])
            if start <= virtual_address < end:
                offset = int(section["offset"]) + (virtual_address - start)
                return self.data[offset : offset + size]
        return None


def byte_is_short_relative_branch(byte: int) -> bool:
    return 0x70 <= byte <= 0x7F


def modrm_is_register_only(modrm: int) -> bool:
    return (modrm & 0xC0) == 0xC0


def modrm_uses_rip_relative(modrm: int) -> bool:
    return (modrm & 0xC7) == 0x05


def decode_supported_instruction(code: bytes, offset: int, limit: int) -> tuple[int | None, str | None, str | None]:
    if offset >= limit:
        return None, "BML_DETOUR_PATCH_WINDOW_TOO_LARGE", "Detour decoder reached the bounded scan limit before finding a safe patch window."

    op = code[offset]
    if op in {0xC2, 0xC3, 0xCA, 0xCB}:
        return None, "BML_DETOUR_EARLY_RETURN_UNSUPPORTED", "Detour target returns before the absolute-jump patch window can be reserved."

    if (
        op in {0xE8, 0xE9, 0xEB}
        or byte_is_short_relative_branch(op)
        or (op == 0x0F and offset + 1 < limit and 0x80 <= code[offset + 1] <= 0x8F)
    ):
        return None, "BML_DETOUR_RELATIVE_CONTROL_FLOW_UNSUPPORTED", "Detour target prologue contains relative control flow that this substrate does not relocate."

    if 0x40 <= op <= 0x4F:
        if offset + 2 > limit:
            return None, "BML_DETOUR_TRUNCATED_INSTRUCTION", "Detour target prologue ended after a REX prefix."
        next_op = code[offset + 1]
        rex_w = (op & 0x08) != 0
        if 0x50 <= next_op <= 0x5F:
            return 2, None, None
        if 0xB8 <= next_op <= 0xBF:
            length = 10 if rex_w else 6
            if offset + length > limit:
                message = (
                    "Detour target prologue ended in the middle of a supported REX.W movabs immediate instruction."
                    if rex_w
                    else "Detour target prologue ended in the middle of a supported REX mov immediate instruction."
                )
                return None, "BML_DETOUR_TRUNCATED_INSTRUCTION", message
            return length, None, None
        if next_op in {0x31, 0x39, 0x3B, 0x85, 0x89, 0x8B}:
            if offset + 3 > limit:
                return None, "BML_DETOUR_TRUNCATED_INSTRUCTION", "Detour target prologue ended in the middle of a supported REX register instruction."
            modrm = code[offset + 2]
            if not modrm_is_register_only(modrm):
                code_id = "BML_DETOUR_RIP_RELATIVE_RELOCATION_REQUIRED" if modrm_uses_rip_relative(modrm) else "BML_DETOUR_MEMORY_OPERAND_UNSUPPORTED"
                return None, code_id, "Detour target prologue uses REX-prefixed memory addressing that this substrate does not relocate."
            return 3, None, None
        if next_op == 0x83:
            if offset + 4 > limit:
                return None, "BML_DETOUR_TRUNCATED_INSTRUCTION", "Detour target prologue ended in the middle of a supported REX add/sub immediate instruction."
            modrm = code[offset + 2]
            reg_opcode = (modrm >> 3) & 0x07
            if not modrm_is_register_only(modrm) or reg_opcode not in {0, 5}:
                code_id = "BML_DETOUR_RIP_RELATIVE_RELOCATION_REQUIRED" if modrm_uses_rip_relative(modrm) else "BML_DETOUR_UNSUPPORTED_INSTRUCTION"
                return None, code_id, "Detour target prologue uses an unsupported REX immediate arithmetic form."
            return 4, None, None
        if next_op == 0x81:
            if offset + 7 > limit:
                return None, "BML_DETOUR_TRUNCATED_INSTRUCTION", "Detour target prologue ended in the middle of a supported REX imm32 arithmetic form."
            modrm = code[offset + 2]
            reg_opcode = (modrm >> 3) & 0x07
            if not modrm_is_register_only(modrm) or reg_opcode not in {0, 5}:
                code_id = "BML_DETOUR_RIP_RELATIVE_RELOCATION_REQUIRED" if modrm_uses_rip_relative(modrm) else "BML_DETOUR_UNSUPPORTED_INSTRUCTION"
                return None, code_id, "Detour target prologue uses an unsupported REX imm32 arithmetic form."
            return 7, None, None

    if op == 0x90 or 0x50 <= op <= 0x57 or 0x58 <= op <= 0x5F:
        return 1, None, None

    if 0xB8 <= op <= 0xBF:
        if offset + 5 > limit:
            return None, "BML_DETOUR_TRUNCATED_INSTRUCTION", "Detour target prologue ended in the middle of a supported mov immediate instruction."
        return 5, None, None

    if op in {0x31, 0x39, 0x3B, 0x85}:
        if offset + 2 > limit:
            return None, "BML_DETOUR_TRUNCATED_INSTRUCTION", "Detour target prologue ended in the middle of a supported register ALU instruction."
        modrm = code[offset + 1]
        if not modrm_is_register_only(modrm):
            code_id = "BML_DETOUR_RIP_RELATIVE_RELOCATION_REQUIRED" if modrm_uses_rip_relative(modrm) else "BML_DETOUR_MEMORY_OPERAND_UNSUPPORTED"
            return None, code_id, "Detour target prologue uses memory addressing that this substrate does not relocate."
        return 2, None, None

    if op in {0x89, 0x8B}:
        if offset + 2 > limit:
            return None, "BML_DETOUR_TRUNCATED_INSTRUCTION", "Detour target prologue ended in the middle of a supported register mov instruction."
        modrm = code[offset + 1]
        if not modrm_is_register_only(modrm):
            code_id = "BML_DETOUR_RIP_RELATIVE_RELOCATION_REQUIRED" if modrm_uses_rip_relative(modrm) else "BML_DETOUR_MEMORY_OPERAND_UNSUPPORTED"
            return None, code_id, "Detour target prologue uses memory addressing that this substrate does not relocate."
        return 2, None, None

    if op == 0x83:
        if offset + 3 > limit:
            return None, "BML_DETOUR_TRUNCATED_INSTRUCTION", "Detour target prologue ended in the middle of a supported add/sub immediate instruction."
        modrm = code[offset + 1]
        reg_opcode = (modrm >> 3) & 0x07
        if not modrm_is_register_only(modrm) or reg_opcode not in {0, 5}:
            code_id = "BML_DETOUR_RIP_RELATIVE_RELOCATION_REQUIRED" if modrm_uses_rip_relative(modrm) else "BML_DETOUR_UNSUPPORTED_INSTRUCTION"
            return None, code_id, "Detour target prologue uses an unsupported immediate arithmetic form."
        return 3, None, None
    return None, "BML_DETOUR_UNSUPPORTED_INSTRUCTION", "Detour target prologue contains an instruction outside the conservative fixture-safe decoder subset."


def measure_patch_window(code: bytes) -> dict[str, Any]:
    patch_size = 0
    while patch_size < PATCH_BYTES:
        length, code_id, message = decode_supported_instruction(code, patch_size, min(MAX_COPY_BYTES, len(code)))
        if length is None:
            return {
                "status": "blocked",
                "patchWindowBytes": patch_size,
                "blockerCode": code_id,
                "message": message,
            }
        patch_size += length
    return {
        "status": "ready",
        "patchWindowBytes": patch_size,
        "message": "Target prologue can reserve a safe absolute-jump patch window under the conservative decoder.",
    }


def analyze_symbol(image: Elf64Image, symbol: dict[str, Any]) -> dict[str, Any]:
    result: dict[str, Any] = {
        "id": symbol["id"],
        "kind": symbol["kind"],
        "mangledSymbol": symbol.get("mangledSymbol"),
        "demangledLabel": symbol.get("demangledLabel"),
        "required": symbol.get("required", False),
    }
    if symbol["kind"] == "data":
        elf_symbol = image.symbols.get(symbol["mangledSymbol"])
        result["status"] = "ready" if elf_symbol is not None else "missing"
        result["address"] = f"0x{elf_symbol['value']:x}" if elf_symbol is not None else None
        result["message"] = "Data symbol resolved; no prologue detour required." if elf_symbol is not None else "Data symbol was not found in the executable symbol table."
        return result

    elf_symbol = image.symbols.get(symbol["mangledSymbol"])
    if elf_symbol is None:
        result.update({"status": "missing", "address": None, "message": "Function symbol was not found in the executable symbol table."})
        return result

    code = image.bytes_at_va(int(elf_symbol["value"]), MAX_COPY_BYTES)
    result["address"] = f"0x{elf_symbol['value']:x}"
    result["symbolSize"] = int(elf_symbol["size"])
    if code is None or len(code) == 0:
        result.update({"status": "missing", "message": "Function bytes could not be mapped from ELF section data."})
        return result

    measurement = measure_patch_window(code)
    result.update(measurement)
    result["prologueBytes"] = code[: min(16, len(code))].hex()
    return result


def build_report(manifest_path: Path, executable_path: Path) -> dict[str, Any]:
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    image = Elf64Image(executable_path)
    symbols_by_id = {symbol["id"]: symbol for symbol in manifest["symbols"]}

    hooks: list[dict[str, Any]] = []
    total_ready = 0
    total_blocked = 0
    total_missing = 0

    for hook in manifest["hookTargets"]:
        targets = []
        for symbol_id in hook["targetSymbolIds"]:
            target = analyze_symbol(image, symbols_by_id[symbol_id])
            targets.append(target)
        ready_count = sum(1 for target in targets if target["status"] == "ready")
        blocked_count = sum(1 for target in targets if target["status"] == "blocked")
        missing_count = sum(1 for target in targets if target["status"] == "missing")
        total_ready += ready_count
        total_blocked += blocked_count
        total_missing += missing_count
        hooks.append(
            {
                "id": hook["id"],
                "capability": hook["capability"],
                "required": hook.get("required", False),
                "installStatus": hook.get("installStatus"),
                "readyTargets": ready_count,
                "blockedTargets": blocked_count,
                "missingTargets": missing_count,
                "targets": targets,
            }
        )

    executable_bytes = executable_path.read_bytes()
    return {
        "schemaVersion": "0.1.0",
        "analyzer": {
            "decoder": DECODER_ID,
            "patchStyle": PATCH_STYLE,
            "patchBytes": PATCH_BYTES,
            "maxCopyBytes": MAX_COPY_BYTES,
            "claimBoundary": "static-prologue-readiness-only",
        },
        "manifest": {
            "path": str(manifest_path),
            "steamAppId": manifest.get("steamAppId"),
            "steamBuildId": manifest.get("steamBuildId"),
            "platform": manifest.get("platform"),
        },
        "executable": {
            "path": str(executable_path),
            "sha256": hashlib.sha256(executable_bytes).hexdigest(),
            "size": len(executable_bytes),
            "manifestSha256": manifest.get("executable", {}).get("sha256"),
            "manifestBuildId": manifest.get("executable", {}).get("buildId"),
        },
        "summary": {
            "hookGroups": len(hooks),
            "readyTargets": total_ready,
            "blockedTargets": total_blocked,
            "missingTargets": total_missing,
            "playableBehaviorClaimed": False,
        },
        "hooks": hooks,
    }


def default_manifest_path() -> Path:
    return Path(__file__).resolve().parents[1] / "manifests" / "steam-371970-22630456-linux.json"


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Analyze installed Barony Stash hook target prologue readiness without launching the game.")
    parser.add_argument("--manifest", type=Path, default=default_manifest_path(), help="Hook manifest JSON path.")
    parser.add_argument("--executable", type=Path, default=None, help="Installed Barony ELF executable path. Defaults to manifest executable.pathHint.")
    parser.add_argument("--out", type=Path, default=None, help="Write JSON report to this path instead of stdout.")
    parser.add_argument("--compact", action="store_true", help="Emit compact JSON.")
    args = parser.parse_args(argv)

    manifest = json.loads(args.manifest.read_text(encoding="utf-8"))
    executable = args.executable or Path(manifest.get("executable", {}).get("pathHint", ""))
    if not executable.is_file():
        raise SystemExit(f"executable not found: {executable}")

    report = build_report(args.manifest, executable)
    rendered = json.dumps(report, separators=(",", ":") if args.compact else None, indent=None if args.compact else 2)
    if args.out is not None:
        args.out.parent.mkdir(parents=True, exist_ok=True)
        args.out.write_text(rendered + "\n", encoding="utf-8")
    else:
        print(rendered)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
