#!/usr/bin/env python3
"""Small Barony Workshop mod repo helper.

Keeps source mods in ./mods/<slug>/content, builds clean Barony-ready folders
under ./dist/<slug>, and optionally installs them into Barony/mods/<slug> or
writes SteamCMD VDF manifests.
"""

from __future__ import annotations

import argparse
import os
import re
import shutil
import struct
import subprocess
import sys
import textwrap
import zlib
import zipfile
from pathlib import Path
from typing import Any

try:
    import tomllib
except ModuleNotFoundError:  # pragma: no cover - Python < 3.11 fallback message
    tomllib = None  # type: ignore[assignment]

BARONY_APPID = 371970
PREVIEW_LIMIT_BYTES = 1_000_000
VISIBILITY = {0: "public", 1: "friends-only", 2: "hidden"}
REPO_ONLY_NAMES = {".gitkeep", ".DS_Store", "Thumbs.db"}
SLUG_RE = re.compile(r"^[a-z0-9][a-z0-9_-]*$")


def die(message: str, code: int = 1) -> None:
    print(f"error: {message}", file=sys.stderr)
    raise SystemExit(code)


def load_toml(path: Path) -> dict[str, Any]:
    if tomllib is None:
        die("Python 3.11+ is required because this script uses stdlib tomllib")
    try:
        return tomllib.loads(path.read_text(encoding="utf-8"))
    except FileNotFoundError:
        die(f"missing required file: {path}")
    except tomllib.TOMLDecodeError as exc:  # type: ignore[union-attr]
        die(f"invalid TOML in {path}: {exc}")


def repo_config(root: Path) -> dict[str, Any]:
    path = root / "barony-mods.toml"
    if not path.exists():
        return {"barony": {"appid": BARONY_APPID, "install_paths": []}, "build": {"dist_dir": "dist", "workshop_dir": ".workshop"}}
    return load_toml(path)


def mods_dir(root: Path) -> Path:
    return root / "mods"


def mod_path(root: Path, slug: str) -> Path:
    return mods_dir(root) / slug


def find_mods(root: Path) -> list[Path]:
    base = mods_dir(root)
    if not base.exists():
        return []
    return sorted(p for p in base.iterdir() if p.is_dir() and (p / "workshop.toml").exists())


def load_mod(root: Path, slug: str) -> tuple[Path, dict[str, Any]]:
    path = mod_path(root, slug)
    manifest = path / "workshop.toml"
    if not manifest.exists():
        die(f"mod '{slug}' not found at {manifest}")
    data = load_toml(manifest)
    data.setdefault("slug", slug)
    data.setdefault("preview", "preview.png")
    data.setdefault("visibility", repo_config(root).get("barony", {}).get("default_visibility", 2))
    data.setdefault("publishedfileid", "0")
    data.setdefault("changenote", "Update")
    data.setdefault("content", {"folder": "content"})
    return path, data


def validate_slug(slug: str) -> None:
    if not SLUG_RE.match(slug):
        die("slug must use lowercase letters, numbers, underscores, or hyphens, and start with a letter/number")


def png_chunk(kind: bytes, data: bytes) -> bytes:
    return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF)


def write_preview(path: Path, title: str) -> None:
    """Write a tiny valid PNG placeholder without external dependencies."""
    width = height = 512
    rows = []
    for y in range(height):
        row = bytearray([0])
        for x in range(width):
            band = (x + y) // 64
            r = 50 + (band % 4) * 18
            g = 18 + (x * 20 // width)
            b = 22 + (y * 24 // height)
            row.extend((r, g, b))
        rows.append(bytes(row))
    raw = b"".join(rows)
    png = b"\x89PNG\r\n\x1a\n" + png_chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)) + png_chunk(b"IDAT", zlib.compress(raw, 9)) + png_chunk(b"IEND", b"")
    path.write_bytes(png)


def toml_string(value: str) -> str:
    escaped = value.replace("\\", "\\\\").replace('"', '\\"')
    return f'"{escaped}"'


def create_mod(args: argparse.Namespace) -> None:
    root = args.root.resolve()
    slug = args.slug
    validate_slug(slug)
    target = mod_path(root, slug)
    if target.exists():
        die(f"mod already exists: {target}")
    title = args.title or slug.replace("-", " ").replace("_", " ").title()
    content = target / "content"
    for folder in ["data", "maps", "models", "images", "items", "lang", "music", "sounds", "books"]:
        (content / folder).mkdir(parents=True, exist_ok=True)
        (content / folder / ".gitkeep").write_text("", encoding="utf-8")
    manifest = f"""slug = {toml_string(slug)}
title = {toml_string(title)}
description = \"\"\"
Describe what this mod changes and any compatibility notes.
\"\"\"
preview = \"preview.png\"
visibility = 2
publishedfileid = \"0\"
changenote = \"Initial upload\"

[content]
folder = \"content\"
"""
    (target / "workshop.toml").write_text(manifest, encoding="utf-8")
    write_preview(target / "preview.png", title)
    print(f"created {target}")
    print("next: add Barony files under content/ then run: python tools/barony_mods.py validate")


def has_uploadable_files(content: Path) -> bool:
    for p in content.rglob("*"):
        if p.is_file() and p.name not in REPO_ONLY_NAMES:
            return True
    return False


def validate_one(root: Path, slug: str) -> tuple[int, int]:
    errors = 0
    warnings = 0
    path, data = load_mod(root, slug)
    declared_slug = str(data.get("slug", ""))
    if declared_slug != slug:
        print(f"ERROR {slug}: workshop.toml slug is {declared_slug!r}, expected {slug!r}")
        errors += 1
    if not SLUG_RE.match(slug):
        print(f"ERROR {slug}: invalid folder slug")
        errors += 1
    for key in ["title", "description", "preview", "visibility", "publishedfileid", "changenote"]:
        if key not in data:
            print(f"ERROR {slug}: missing {key} in workshop.toml")
            errors += 1
    title = str(data.get("title", "")).strip()
    description = str(data.get("description", "")).strip()
    if not title or title.lower().startswith("example"):
        print(f"WARN  {slug}: title still looks placeholder-ish")
        warnings += 1
    if not description or "Describe what this mod changes" in description:
        print(f"WARN  {slug}: description still looks placeholder-ish")
        warnings += 1
    try:
        visibility = int(data.get("visibility", 2))
    except (TypeError, ValueError):
        visibility = -1
    if visibility not in VISIBILITY:
        print(f"ERROR {slug}: visibility must be 0, 1, or 2")
        errors += 1
    preview = path / str(data.get("preview", "preview.png"))
    if not preview.exists():
        print(f"ERROR {slug}: preview file missing: {preview.relative_to(root)}")
        errors += 1
    elif preview.stat().st_size > PREVIEW_LIMIT_BYTES:
        print(f"ERROR {slug}: preview file is {preview.stat().st_size} bytes; Barony guides recommend <1 MB")
        errors += 1
    content_folder = str(data.get("content", {}).get("folder", "content"))
    content = path / content_folder
    if not content.exists():
        print(f"ERROR {slug}: content folder missing: {content.relative_to(root)}")
        errors += 1
    elif not has_uploadable_files(content):
        print(f"WARN  {slug}: content folder has no uploadable files yet")
        warnings += 1
    print(f"{slug}: {errors} error(s), {warnings} warning(s)")
    return errors, warnings


def validate(args: argparse.Namespace) -> None:
    root = args.root.resolve()
    slugs = [args.slug] if args.slug else [p.name for p in find_mods(root)]
    if not slugs:
        print("no mods found under mods/<slug>/workshop.toml")
        return
    total_errors = 0
    total_warnings = 0
    for slug in slugs:
        e, w = validate_one(root, slug)
        total_errors += e
        total_warnings += w
    if total_errors:
        die(f"validation failed with {total_errors} error(s) and {total_warnings} warning(s)")
    print(f"validation passed with {total_warnings} warning(s)")


def should_ignore(path: Path) -> bool:
    return path.name in REPO_ONLY_NAMES or path.name == "__pycache__"


def copy_content(src: Path, dst: Path) -> None:
    if dst.exists():
        shutil.rmtree(dst)
    dst.mkdir(parents=True)
    for item in src.iterdir():
        if should_ignore(item):
            continue
        target = dst / item.name
        if item.is_dir():
            shutil.copytree(item, target, ignore=lambda _dir, names: [n for n in names if n in REPO_ONLY_NAMES])
        elif item.is_file():
            shutil.copy2(item, target)


def build_one(root: Path, slug: str) -> Path:
    path, data = load_mod(root, slug)
    config = repo_config(root)
    dist_root = root / config.get("build", {}).get("dist_dir", "dist")
    out = dist_root / slug
    content_folder = path / str(data.get("content", {}).get("folder", "content"))
    if not content_folder.exists():
        die(f"content folder missing: {content_folder}")
    copy_content(content_folder, out)
    preview = path / str(data.get("preview", "preview.png"))
    if preview.exists():
        shutil.copy2(preview, out / preview.name)
    print(f"built {slug} -> {out}")
    return out


def build(args: argparse.Namespace) -> None:
    build_one(args.root.resolve(), args.slug)


def detect_barony_dir(root: Path, explicit: str | None) -> Path:
    if explicit:
        p = Path(explicit).expanduser().resolve()
        if p.exists():
            return p
        die(f"Barony directory does not exist: {p}")
    env_path = os.environ.get("BARONY_DIR")
    if env_path:
        p = Path(env_path).expanduser().resolve()
        if p.exists():
            return p
    config = repo_config(root)
    for raw in config.get("barony", {}).get("install_paths", []):
        p = Path(str(raw)).expanduser()
        if p.exists() and p.is_dir():
            return p.resolve()
    die("could not find Barony install. Pass --barony-dir /path/to/Barony or set BARONY_DIR")


def install(args: argparse.Namespace) -> None:
    root = args.root.resolve()
    built = build_one(root, args.slug)
    barony = detect_barony_dir(root, args.barony_dir)
    target = barony / "mods" / args.slug
    target.parent.mkdir(parents=True, exist_ok=True)
    if target.exists():
        shutil.rmtree(target)
    shutil.copytree(built, target)
    print(f"installed {args.slug} -> {target}")
    print("Barony path: Play Modded Game -> My Workshop Items -> select this local mod folder")


def vdf_escape(value: str) -> str:
    return value.replace("\\", "\\\\").replace('"', '\\"').replace("\n", "\\n")


def write_vdf(args: argparse.Namespace) -> None:
    root = args.root.resolve()
    built = build_one(root, args.slug)
    data_path, data = load_mod(root, args.slug)
    config = repo_config(root)
    appid = int(config.get("barony", {}).get("appid", BARONY_APPID))
    workshop_root = root / config.get("build", {}).get("workshop_dir", ".workshop")
    workshop_root.mkdir(parents=True, exist_ok=True)
    preview = built / Path(str(data.get("preview", "preview.png"))).name
    vdf_path = workshop_root / f"{args.slug}.vdf"
    fields = {
        "appid": str(appid),
        "publishedfileid": str(data.get("publishedfileid", "0")),
        "contentfolder": str(built.resolve()),
        "previewfile": str(preview.resolve()) if preview.exists() else "",
        "visibility": str(int(data.get("visibility", 2))),
        "title": str(data.get("title", args.slug)),
        "description": str(data.get("description", "")),
        "changenote": str(data.get("changenote", "Update")),
    }
    lines = ['"workshopitem"', "{"]
    for key, value in fields.items():
        if value != "":
            lines.append(f'  "{key}" "{vdf_escape(value)}"')
    lines.append("}")
    vdf_path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"wrote {vdf_path}")
    print(f"SteamCMD: steamcmd +login <username> +workshop_build_item {vdf_path} +quit")


def publish(args: argparse.Namespace) -> None:
    root = args.root.resolve()
    write_vdf(args)
    steamcmd = shutil.which("steamcmd")
    if not steamcmd:
        die("steamcmd not found. Use Barony's in-game Workshop uploader, or install steamcmd and rerun publish")
    user = args.steam_user or os.environ.get("STEAM_USER")
    if not user:
        die("pass --steam-user or set STEAM_USER. Do not put your Steam password in this repo")
    vdf_path = root / repo_config(root).get("build", {}).get("workshop_dir", ".workshop") / f"{args.slug}.vdf"
    cmd = [steamcmd, "+login", user, "+workshop_build_item", str(vdf_path), "+quit"]
    print("running SteamCMD; Steam Guard may prompt in the terminal")
    subprocess.run(cmd, check=True)


def zip_mod(args: argparse.Namespace) -> None:
    root = args.root.resolve()
    built = build_one(root, args.slug)
    zip_path = root / f"{args.slug}.zip"
    if zip_path.exists():
        zip_path.unlink()
    with zipfile.ZipFile(zip_path, "w", zipfile.ZIP_DEFLATED) as zf:
        for p in built.rglob("*"):
            if p.is_file():
                zf.write(p, p.relative_to(built.parent))
    print(f"wrote {zip_path}")


def list_mods(args: argparse.Namespace) -> None:
    root = args.root.resolve()
    mods = find_mods(root)
    if not mods:
        print("no mods yet. create one with: python tools/barony_mods.py new my-mod --title 'My Mod'")
        return
    for p in mods:
        data = load_toml(p / "workshop.toml")
        visibility = VISIBILITY.get(int(data.get("visibility", 2)), str(data.get("visibility")))
        published = str(data.get("publishedfileid", "0"))
        print(f"{p.name}\t{data.get('title', p.name)}\tvisibility={visibility}\tpublishedfileid={published}")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Manage a repo of Barony Steam Workshop mods.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=textwrap.dedent(
            """
            Typical flow:
              python tools/barony_mods.py new goblin-chaos --title "Goblin Chaos"
              python tools/barony_mods.py validate goblin-chaos
              python tools/barony_mods.py build goblin-chaos
              python tools/barony_mods.py install goblin-chaos --barony-dir ~/.local/share/Steam/steamapps/common/Barony
            """
        ),
    )
    parser.add_argument("--root", type=Path, default=Path.cwd(), help="repository root (default: current directory)")
    sub = parser.add_subparsers(dest="command", required=True)

    p = sub.add_parser("new", help="create a new mod scaffold")
    p.add_argument("slug")
    p.add_argument("--title")
    p.set_defaults(func=create_mod)

    p = sub.add_parser("list", help="list configured mods")
    p.set_defaults(func=list_mods)

    p = sub.add_parser("validate", help="validate one mod or all mods")
    p.add_argument("slug", nargs="?")
    p.set_defaults(func=validate)

    p = sub.add_parser("build", help="build a clean Barony mod folder under dist/")
    p.add_argument("slug")
    p.set_defaults(func=build)

    p = sub.add_parser("install", help="copy a built mod to Barony/mods/<slug>")
    p.add_argument("slug")
    p.add_argument("--barony-dir")
    p.set_defaults(func=install)

    p = sub.add_parser("vdf", help="generate a SteamCMD workshop_build_item VDF")
    p.add_argument("slug")
    p.set_defaults(func=write_vdf)

    p = sub.add_parser("publish", help="generate VDF then call steamcmd")
    p.add_argument("slug")
    p.add_argument("--steam-user")
    p.set_defaults(func=publish)

    p = sub.add_parser("zip", help="build and package a mod as a zip")
    p.add_argument("slug")
    p.set_defaults(func=zip_mod)

    args = parser.parse_args(argv)
    args.func(args)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
