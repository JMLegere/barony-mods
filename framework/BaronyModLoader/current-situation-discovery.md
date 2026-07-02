# Current situation discovery — installed PC executable path

Date: 2026-07-02

## Trigger

Jeremy flagged that the public open-source Barony checkout is behind the current Steam version and asked to step back into discovery. The source-built sidecar runtime was deleted from `/tmp` and should no longer be treated as the v1 Steam-current path.

## Current installed Steam target

`BaronyModLoader steam detect` reports:

```json
{
  "appId": "371970",
  "buildId": "22630456",
  "installPath": "/home/jerry/.local/share/Steam/steamapps/common/Barony",
  "executable": "/home/jerry/.local/share/Steam/steamapps/common/Barony/barony.x86_64"
}
```

Binary string scan finds the installed game version string:

```text
v5.0.2
```

Steam executable identity:

```text
ELF 64-bit LSB pie executable, x86-64, dynamically linked
BuildID[sha1]=58089d84bce3afb48d5b19df032f7aa89d81b69a
not stripped
```

This is a good discovery result: the current Steam executable is a PIE binary, but it is not stripped and exposes many useful C++ symbols.

## Deleted prototype source build

Removed local prototype artifacts:

```text
/tmp/barony-src
/tmp/barony-bml-src
/tmp/barony-bml-build
/tmp/barony-bml-src-verify
/tmp/barony-bml-patchwork
/tmp/barony-bml-stash-smoke
```

The previous source checkout was useful as a code-reading reference, but not a proven runtime target:

```text
origin: https://github.com/TurningWheel/Barony
commit: 962a5ce36d10207beef7d8673876e0cebf8e76e4
```

## Source-assisted hook feasibility

The current Steam executable exports symbols that line up with the source-level concepts needed for Stash. `nm -C barony.x86_64` / `nm -D -C barony.x86_64` show, among others:

```text
0000000000587010 T actChest(Entity*)
0000000000583de0 T actChestLid(Entity*)
00000000002eb0d0 T generateDungeon(char*, unsigned int, std::tuple<int, int, int, int>)
000000000035cd60 T loadMap(char const*, map_t*, list_t*, list_t*, int*)
0000000000627e40 T setSpriteAttributes(Entity*, Entity*, Entity*)
00000000003483e0 T newEntity(int, unsigned int, list_t*, list_t*)
0000000000348a80 T list_AddNodeFirst(list_t*)
0000000000348b00 T list_AddNodeLast(list_t*)
00000000003494d0 T list_FreeAll(list_t*)
0000000000584cd0 T Entity::getChestInventoryList()
0000000000584420 T Entity::addItemToChest(Item*, bool, Item*)
0000000000584990 T Entity::getItemFromChest(Item*, int, bool)
00000000005856d0 T Entity::removeItemFromVoidChestServer(int, Item*, int)
0000000000d37ce0 B map
0000000000d3bec0 B map_rng
0000000000d3bca0 B map_server_rng
0000000000d3897c B multiplayer
0000000000d38980 B clientnum
00000000011b87c0 B openedChest
0000000000d37ca0 B shoparea
0000000000e6b400 B stats
0000000000d69b00 B TileEntityList
```

Interpretation:

- The open-source code is still valuable as a semantic map: function names, expected behavior, structs, lifecycle points, and module boundaries.
- The installed Steam executable is not stripped, so BML can potentially resolve/call symbols from the actual installed binary.
- `LD_PRELOAD` can load a BML bootstrap library into the process, but internal game calls may not be fully interposable by symbol name alone. For reliable gameplay hooks, expect a detour/trampoline layer or a symbol-address based hook library.
- Because the executable is PIE, absolute addresses from `nm` must be relocated by the runtime base address.

## Mod-loader parallels

The useful prior-art distinction is:

- tModLoader-style product shape: separate modded launcher/app, separate modded profile/world state, structured hooks/API, and compatibility to known game versions.
- SKSE-style native reality: runtime hooking into the installed executable, version-specific addresses/symbols, and rebuild/guardrails when the game executable updates.
- Linux-native mechanism: `LD_PRELOAD` can inject a bootstrap `.so` at process start, but robust hooks still need symbol lookup and/or detours rather than assuming every internal C++ function call is symbol-interposable.

BML should borrow the product discipline from tModLoader (clean app, mod profiles, structured hooks, user-facing mod management) and the native Steam-current strategy from SKSE-style loaders (installed executable identity, version-specific hook maps, fail-closed on unsupported builds).


## Viable v1 path

### Installed PC executable hook runtime

Use the installed PC executable as the runtime and inject/load BML with a native hook/bootstrap library.

The first concrete target is Jeremy's local Steam/Linux install because it is available for direct inspection and verification:

```text
storefront: Steam
platform: Linux
app id: 371970
build id: 22630456
game version: v5.0.2
```

The product target should stay broader than Steam/Linux while remaining PC-only:

| Storefront | PC platforms in scope |
| --- | --- |
| Steam | Windows, macOS, Linux |
| Epic Games Store | Windows, macOS |
| GOG / DRM-free | Windows, macOS, Linux |
| Humble Bundle | Windows, macOS, Linux |

Nintendo Switch is out of scope for this native hook/bootstrap approach.

Pros:

- Targets the actual game the player owns and launches.
- Avoids stale public-source/build mismatch.
- Current Steam/Linux binary is not stripped and exports useful symbols.
- Open-source Barony can still guide hook design and struct/function understanding.
- The same app/profile/package/runtime contract can generalize to other PC storefronts and operating systems.

Cons:

- Requires native hooking infrastructure per OS/platform.
- Needs per-storefront/per-build guardrails, executable hashes, symbol maps, and failure-safe diagnostics.
- More fragile than source-level hooks if a storefront updates the executable.

## Discovery conclusion

The source-build path should be ignored for the main v1 plan. BML should be an installed PC executable mod loader: a standalone app that detects a supported PC install, verifies executable provenance, launches the installed game through a BML-owned hook/bootstrap library, and fails closed on unsupported builds.

The next implementation should not revive `/tmp/barony-src` as a runtime build. Instead:

1. Add app-side runtime strategy metadata and guardrails for installed PC executables.
2. Create a minimal `native/barony-modloader-hook/` no-op preload library for the first Steam/Linux target.
3. Prove the installed Steam executable launches with the hook loaded and writes a runtime-load-report.
4. Add provenance checks: storefront id/build id where available, executable SHA-256, ELF/Mach-O/PE build identity, and game version string.
5. Build a symbol map for Steam/Linux build `22630456`.
6. Only then port Stash hooks into the installed-binary hook runtime.
