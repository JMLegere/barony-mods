from __future__ import annotations

import re
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
LINUX_HOOK = ROOT / "src" / "bml_hook.c"
WINDOWS_HOOK = ROOT / "src" / "bml_hook_win.c"


def read_source(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def define_value(source: str, name: str) -> str:
    match = re.search(r"^#define\s+" + re.escape(name) + r"\s+(.+?)\s*$", source, re.MULTILINE)
    if not match:
        raise AssertionError(f"Missing #define {name}")
    return match.group(1)


class NativeStashPlacementParityTests(unittest.TestCase):
    def test_linux_and_windows_share_lobby_spawn_semantics(self) -> None:
        linux = read_source(LINUX_HOOK)
        windows = read_source(WINDOWS_HOOK)

        self.assertEqual(define_value(linux, "BML_STASH_LOBBY_PLACEMENT_X"), "232.0")
        self.assertEqual(define_value(windows, "BML_STASH_LOBBY_PLACEMENT_X"), "232.0")
        self.assertEqual(define_value(linux, "BML_STASH_LOBBY_PLACEMENT_Y"), "280.0")
        self.assertEqual(define_value(windows, "BML_STASH_LOBBY_PLACEMENT_Y"), "280.0")

        for macro, expected in {
            "BML_STASH_SPRITE_CHEST_SPAWN": "21",
            "BML_STASH_SPRITE_CHEST_VOID_VISUAL": "1791",
            "BML_STASH_SPRITE_LID_SPAWN": "216",
            "BML_STASH_SPRITE_LID_VOID_VISUAL": "1790",
        }.items():
            self.assertEqual(define_value(linux, macro), expected)
            self.assertEqual(define_value(windows, macro), expected)

        self.assertIn("new_entity_original(BML_STASH_SPRITE_CHEST_SPAWN", linux)
        self.assertIn("new_entity_original(BML_STASH_SPRITE_LID_SPAWN", linux)
        self.assertIn("BML_STASH_ENTITY_OFFSET_SPRITE, BML_STASH_SPRITE_CHEST_VOID_VISUAL", linux)
        self.assertIn("BML_STASH_ENTITY_OFFSET_SPRITE, BML_STASH_SPRITE_LID_VOID_VISUAL", linux)

        self.assertIn("new_entity_with_diagnostics(\"before_chest_new_entity\", BML_STASH_SPRITE_CHEST_SPAWN", windows)
        self.assertIn("new_entity_with_diagnostics(\"before_lid_new_entity\", BML_STASH_SPRITE_LID_SPAWN", windows)
        self.assertIn("BML_STASH_ENTITY_OFFSET_SPRITE, BML_STASH_SPRITE_CHEST_VOID_VISUAL", windows)
        self.assertIn("BML_STASH_ENTITY_OFFSET_SPRITE, BML_STASH_SPRITE_LID_VOID_VISUAL", windows)

    def test_linux_and_windows_use_walkable_lobby_placement(self) -> None:
        linux = read_source(LINUX_HOOK)
        windows = read_source(WINDOWS_HOOK)

        for source, helper in (
            (linux, "bml_stash_find_nearest_walkable_tile"),
            (windows, "windows_stash_find_nearest_walkable_tile"),
        ):
            self.assertIn("double x_pos = BML_STASH_LOBBY_PLACEMENT_X;", source)
            self.assertIn("double y_pos = BML_STASH_LOBBY_PLACEMENT_Y;", source)
            self.assertIn(f"{helper}(map_prefix, x_pos, y_pos, &x_pos, &y_pos)", source)
            self.assertIn("place_chest_and_lid_at(map_argument, x_pos, y_pos, BML_STASH_LOBBY_PLACEMENT_YAW", source)


if __name__ == "__main__":
    unittest.main()
