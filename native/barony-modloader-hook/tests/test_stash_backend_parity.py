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

def c_static_function_body(source: str, name: str) -> str:
    match = re.search(
        r"static\s+[\w\s\*]+?\b" + re.escape(name) + r"\s*\([^;]*?\)\s*\{",
        source,
        re.MULTILINE,
    )
    if not match:
        raise AssertionError(f"Missing static function {name}")

    start = match.end() - 1
    depth = 0
    in_string = False
    in_char = False
    in_line_comment = False
    in_block_comment = False
    escaped = False

    for index in range(start, len(source)):
        char = source[index]
        next_char = source[index + 1] if index + 1 < len(source) else ""

        if in_line_comment:
            if char == "\n":
                in_line_comment = False
            continue
        if in_block_comment:
            if char == "*" and next_char == "/":
                in_block_comment = False
            continue
        if in_string:
            if escaped:
                escaped = False
            elif char == "\\":
                escaped = True
            elif char == '"':
                in_string = False
            continue
        if in_char:
            if escaped:
                escaped = False
            elif char == "\\":
                escaped = True
            elif char == "'":
                in_char = False
            continue

        if char == "/" and next_char == "/":
            in_line_comment = True
            continue
        if char == "/" and next_char == "*":
            in_block_comment = True
            continue
        if char == '"':
            in_string = True
            continue
        if char == "'":
            in_char = True
            continue
        if char == "{":
            depth += 1
        elif char == "}":
            depth -= 1
            if depth == 0:
                return source[start : index + 1]

    raise AssertionError(f"Could not find end of static function {name}")


def index_of(source: str, needle: str) -> int:
    index = source.find(needle)
    if index < 0:
        raise AssertionError(f"Missing expected source fragment: {needle}")
    return index


def regex_index(source: str, pattern: str) -> int:
    match = re.search(pattern, source)
    if not match:
        raise AssertionError(f"Missing expected source pattern: {pattern}")
    return match.start()


def assert_offset_present(test_case: unittest.TestCase, source: str, x: int, y: int) -> None:
    test_case.assertRegex(source, r"\{\s*" + re.escape(str(x)) + r"\s*,\s*" + re.escape(str(y)) + r"\s*\}")


def assert_offset_absent(test_case: unittest.TestCase, source: str, x: int, y: int) -> None:
    test_case.assertNotRegex(source, r"\{\s*" + re.escape(str(x)) + r"\s*,\s*" + re.escape(str(y)) + r"\s*\}")



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
        self.assertIn("bml_stash_find_nearest_clean_walkable_tile(map_argument, map_prefix, x_pos, y_pos, &x_pos, &y_pos, &chest_yaw)", linux)
        self.assertIn("place_chest_and_lid_at(map_argument, x_pos, y_pos, chest_yaw", linux)
        self.assertIn("stash_access_point_created", linux)

    def test_linux_stash_lobby_placement_keeps_clear_of_assist_shrine(self) -> None:
        linux = read_source(LINUX_HOOK)
        chooser = c_static_function_body(linux, "bml_stash_playable_choose_lobby_tile_near_assist_shrine")

        self.assertIn("bml_stash_playable_tile_inside_assist_shrine_clearance", linux)
        for unsafe_offset in (
            (0, 2),
            (1, 2),
            (-1, 2),
            (2, 0),
            (-2, 0),
            (0, -2),
            (1, -2),
            (-1, -2),
            (2, 1),
            (2, -1),
            (-2, 1),
            (-2, -1),
        ):
            with self.subTest(offset=unsafe_offset):
                assert_offset_absent(self, chooser, *unsafe_offset)
        for safer_offset in ((0, 3), (3, 0), (0, -3), (-3, 0)):
            with self.subTest(offset=safer_offset):
                assert_offset_present(self, chooser, *safer_offset)

    def test_linux_stash_lobby_placement_requires_back_wall_open_front_and_entity_clearance(self) -> None:
        linux = read_source(LINUX_HOOK)
        chooser = c_static_function_body(linux, "bml_stash_playable_choose_lobby_tile_near_assist_shrine")
        back_wall_open_front = c_static_function_body(
            linux,
            "bml_stash_playable_tile_has_back_wall_and_open_front",
        )
        clean_fallback = c_static_function_body(linux, "bml_stash_find_nearest_clean_walkable_tile")
        entity_clearance = c_static_function_body(linux, "bml_stash_playable_tile_has_entity_clearance")

        self.assertRegex(chooser, r"!\s*bml_stash_playable_tile_has_back_wall_and_open_front\s*\(")
        self.assertRegex(chooser, r"!\s*bml_stash_playable_tile_has_entity_clearance\s*\(")
        self.assertIn("bml_stash_playable_tile_has_back_wall_and_open_front(map_prefix, x, y", clean_fallback)
        self.assertNotRegex(
            chooser,
            r"bml_stash_playable_tile_is_occupied\s*\(\s*map_argument\s*,\s*\(unsigned int\)candidate_x\s*,\s*\(unsigned int\)candidate_y",
        )
        self.assertIn("bml_stash_map_tile_is_wall_or_blocked", back_wall_open_front)
        self.assertIn("BML_STASH_YAW_SOUTH", back_wall_open_front)
        self.assertIn("BML_STASH_YAW_NORTH", back_wall_open_front)
        self.assertIn("BML_STASH_YAW_EAST", back_wall_open_front)
        self.assertIn("BML_STASH_YAW_WEST", back_wall_open_front)
        self.assertIn("BML_STASH_ENTITY_OFFSET_X", entity_clearance)
        self.assertIn("BML_STASH_ENTITY_OFFSET_Y", entity_clearance)

    def test_linux_stash_seeds_runebound_iron_vow_elixir_in_persistent_inventory(self) -> None:
        linux = read_source(LINUX_HOOK)
        make_carrier = c_static_function_body(linux, "bml_runebound_elixir_make_fixture_carrier")
        load_inventory = c_static_function_body(linux, "bml_load_stash_inventory_if_needed")
        duplicate_guard = c_static_function_body(linux, "bml_stash_inventory_has_runebound_iron_vow_carrier")
        seed_elixir = c_static_function_body(linux, "bml_stash_seed_runebound_iron_vow_elixir_if_missing")

        self.assertEqual(define_value(linux, "BML_RUNES_ELIXIR_CARRIER_ITEM_TYPE_POTION_STRENGTH"), "225")
        self.assertEqual(define_value(linux, "BML_RUNES_ELIXIR_IRON_VOW_APPEARANCE"), "1380736049U")
        self.assertEqual(define_value(linux, "BML_RUNES_ELIXIR_STASH_SEED_APPEARANCE"), "0U")
        self.assertIn("metadata->instance_id = BML_RUNES_ELIXIR_IRON_VOW_APPEARANCE;", make_carrier)
        self.assertIn("bml_stash_seed_runebound_iron_vow_elixir_if_missing(inventory", load_inventory)
        self.assertIn("BML_RUNES_ELIXIR_CARRIER_ITEM_TYPE_POTION_STRENGTH", seed_elixir)
        self.assertIn("BML_RUNES_ELIXIR_STASH_SEED_APPEARANCE", seed_elixir)
        self.assertIn("bml_stash_inventory_has_runebound_iron_vow_carrier(inventory)", seed_elixir)
        self.assertLess(
            index_of(seed_elixir, "bml_stash_inventory_has_runebound_iron_vow_carrier(inventory)"),
            index_of(seed_elixir, "g_bml_stash_new_item("),
        )
        self.assertRegex(duplicate_guard, r"->type\s*==\s*BML_RUNES_ELIXIR_CARRIER_ITEM_TYPE_POTION_STRENGTH")
        self.assertIn("BML_RUNES_ELIXIR_STASH_SEED_APPEARANCE", duplicate_guard)
        self.assertIn("itemSpecialShopConsumable", duplicate_guard)
        self.assertIn("item->itemSpecialShopConsumable = true;", seed_elixir)
        self.assertIn("bml_mark_stash_inventory_dirty();", seed_elixir)
        self.assertIn('bml_append_stash_diagnostic_event("stash_runebound_iron_vow_seeded"', seed_elixir)

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
