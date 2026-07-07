from __future__ import annotations

import re
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
LINUX_HOOK = ROOT / "src" / "bml_hook.c"
WINDOWS_HOOK = ROOT / "src" / "bml_hook_win.c"
WINDOWS_STUB = ROOT / "src" / "bml_windows_adapter_stub.c"


def read_source(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def define_value(source: str, name: str) -> str:
    match = re.search(r"^#define\s+" + re.escape(name) + r"\s+(.+?)\s*$", source, re.MULTILINE)
    if not match:
        raise AssertionError(f"Missing #define {name}")
    return match.group(1)


class StashBackendParityTests(unittest.TestCase):
    def test_linux_stash_lobby_spawn_uses_canonical_chest_initialization(self) -> None:
        linux = read_source(LINUX_HOOK)

        self.assertEqual(define_value(linux, "BML_STASH_LOBBY_PLACEMENT_X"), "232.0")
        self.assertEqual(define_value(linux, "BML_STASH_LOBBY_PLACEMENT_Y"), "280.0")
        self.assertEqual(define_value(linux, "BML_STASH_SPRITE_CHEST_SPAWN"), "21")
        self.assertEqual(define_value(linux, "BML_STASH_SPRITE_CHEST_VOID_VISUAL"), "1791")
        self.assertEqual(define_value(linux, "BML_STASH_SPRITE_LID_SPAWN"), "216")
        self.assertEqual(define_value(linux, "BML_STASH_SPRITE_LID_VOID_VISUAL"), "1790")

        self.assertIn("new_entity_original(BML_STASH_SPRITE_CHEST_SPAWN", linux)
        self.assertIn("new_entity_original(BML_STASH_SPRITE_LID_SPAWN", linux)
        self.assertIn("BML_STASH_ENTITY_OFFSET_SPRITE, BML_STASH_SPRITE_CHEST_VOID_VISUAL", linux)
        self.assertIn("BML_STASH_ENTITY_OFFSET_SPRITE, BML_STASH_SPRITE_LID_VOID_VISUAL", linux)

    def test_linux_stash_lobby_placement_uses_walkable_adjusted_coordinates(self) -> None:
        linux = read_source(LINUX_HOOK)

        self.assertIn("double x_pos = BML_STASH_LOBBY_PLACEMENT_X;", linux)
        self.assertIn("double y_pos = BML_STASH_LOBBY_PLACEMENT_Y;", linux)
        self.assertIn("bml_stash_find_nearest_walkable_tile(map_prefix, x_pos, y_pos, &x_pos, &y_pos)", linux)
        self.assertIn("place_chest_and_lid_at(map_argument, x_pos, y_pos, BML_STASH_LOBBY_PLACEMENT_YAW", linux)
        self.assertIn("stash_access_point_created", linux)

    def test_linux_stash_lobby_placement_keeps_clear_of_assist_shrine(self) -> None:
        linux = read_source(LINUX_HOOK)

        self.assertEqual(define_value(linux, "BML_STASH_ASSIST_SHRINE_CLEARANCE_TILES"), "2")
        self.assertIn("bml_stash_playable_tile_inside_assist_shrine_clearance", linux)
        self.assertNotIn("{ 1, 0 },", linux)
        self.assertNotIn("{ -1, 0 },", linux)
        self.assertNotIn("{ 0, -1 },", linux)
        self.assertNotIn("{ 0, 1 }", linux)
        self.assertIn("{ 0, 2 },", linux)
        self.assertIn("{ 2, 0 },", linux)

    def test_windows_backend_is_either_real_parity_or_explicit_fail_closed_stub(self) -> None:
        linux = read_source(LINUX_HOOK)
        if not WINDOWS_HOOK.exists():
            stub = read_source(WINDOWS_STUB)
            self.assertIn("unsupported_fail_closed", stub)
            self.assertIn("not a playable runtime", stub)
            return

        windows = read_source(WINDOWS_HOOK)
        for macro in (
            "BML_STASH_LOBBY_PLACEMENT_X",
            "BML_STASH_LOBBY_PLACEMENT_Y",
            "BML_STASH_SPRITE_CHEST_SPAWN",
            "BML_STASH_SPRITE_CHEST_VOID_VISUAL",
            "BML_STASH_SPRITE_LID_SPAWN",
            "BML_STASH_SPRITE_LID_VOID_VISUAL",
        ):
            self.assertEqual(define_value(windows, macro), define_value(linux, macro))
        self.assertIn("windows_stash_find_nearest_walkable_tile", windows)


if __name__ == "__main__":
    unittest.main()
