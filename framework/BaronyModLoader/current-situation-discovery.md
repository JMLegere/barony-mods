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

The current Steam executable exports the symbols needed for the safe symbol/provenance probe and the future Stash hook targets. `nm -D -C barony.x86_64` and `nm -D barony.x86_64` verify, among others:

```text
_Z8actChestP6Entity                                   actChest(Entity*)
_Z11actChestLidP6Entity                               actChestLid(Entity*)
_ZN6Entity21getChestInventoryListEv                   Entity::getChestInventoryList()
_ZN6Entity14addItemToChestEP4ItembS1_                 Entity::addItemToChest(Item*, bool, Item*)
_ZN6Entity16getItemFromChestEP4Itemib                 Entity::getItemFromChest(Item*, int, bool)
_ZN6Entity24addItemToVoidChestServerEiP4ItembS1_      Entity::addItemToVoidChestServer(int, Item*, bool, Item*)
_ZN6Entity29removeItemFromVoidChestServerEiP4Itemi    Entity::removeItemFromVoidChestServer(int, Item*, int)
_ZN6Entity10closeChestEv                              Entity::closeChest()
_ZN6Entity16closeChestServerEv                        Entity::closeChestServer()
_Z15generateDungeonPcjSt5tupleIJiiiiEE                generateDungeon(char*, unsigned int, std::tuple<int, int, int, int>)
_Z13assignActionsP5map_t                              assignActions(map_t*)
_Z9newEntityijP6list_tS0_                             newEntity(int, unsigned int, list_t*, list_t*)
_Z19setSpriteAttributesP6EntityS0_S0_                 setSpriteAttributes(Entity*, Entity*, Entity*)
_Z7newItem8ItemType6StatusssjbP6list_t                newItem(ItemType, Status, short, short, unsigned int, bool, list_t*)
_Z12list_FreeAllP6list_t                              list_FreeAll(list_t*)
_Z15list_RemoveNodeP6node_t                           list_RemoveNode(node_t*)
_Z16list_AddNodeLastP6list_t                          list_AddNodeLast(list_t*)
_Z17list_AddNodeFirstP6list_t                         list_AddNodeFirst(list_t*)
stats
map
map_rng
map_server_rng
multiplayer
clientnum
openedChest
shoparea
TileEntityList
```

Interpretation:

- The open-source code is still valuable as a semantic map: function names, expected behavior, structs, lifecycle points, and module boundaries.
- The installed Steam executable is not stripped, so BML can potentially resolve/call symbols from the actual installed binary.
- `LD_PRELOAD` can load a BML bootstrap library into the process, but internal game calls may not be fully interposable by symbol name alone. For reliable gameplay hooks, expect a detour/trampoline layer or a symbol-address based hook library.
- Because the executable is PIE, absolute addresses from `nm` must be relocated by the runtime base address.

Option A is now the implemented guardrail: the app/runtime registry and hook manifest pin Steam app `371970`, build `22630456`, game `v5.0.2`, executable SHA-256 `da858ad9636bb14dea18fbca28512c276b0c4e7359914b88acd365ed904bbade`, ELF build id `58089d84bce3afb48d5b19df032f7aa89d81b69a`, and required symbol targets. The native hook currently checks that `BML_HOOK_MANIFEST` is readable, then resolves its compiled-in probe table (mirrored by the manifest) in-process and writes `<profile>/BaronyModLoader/reports/symbol-probe-report.json`.

Option C is the current Stash safety boundary: required gameplay hooks are represented as `hookTargets` tied to Stash capabilities, and the native hook analyzes those direct Stash targets through the abstract `linux-x86_64-direct-stash-detour` backend. The backend is still `analyze-only`: it reports data targets as `ready`, uses the conservative detour decoder to mark a small subset of function prologues as patch-window-ready, records `patchWindowBytes`, and requires unresolved symbols, blocked detour targets, or uninstalled required hook groups to prevent `runtime-load-report.json` from claiming Stash loaded.

Current Steam/Linux conservative direct-target scan for build `22630456`, generated by `python native/barony-modloader-hook/tools/analyze_stash_targets.py --manifest native/barony-modloader-hook/manifests/steam-371970-22630456-linux.json --executable /home/jerry/.local/share/Steam/steamapps/common/Barony/barony.x86_64`, using the decoder support implemented through the REX push/pop slice:

| Hook group | Ready targets | Blocked targets | Interpretation |
| --- | ---: | ---: | --- |
| `stash_void_chest_binding` | 2 | 3 | `Entity::addItemToVoidChestServer` and `Entity::removeItemFromVoidChestServer` have decoder-safe REX-push patch windows; wrapper/getter/lid targets still need more instruction decoding or relocation. |
| `stash_inventory_persistence` | 2 | 9 | The `stats` data symbol and `Entity::getItemFromChest` are ready; the remaining inventory/list/chest function prologues remain blocked. |
| `stash_lobby_placement` | 4 | 3 | Data symbols are ready; placement functions remain blocked until broader prologue decoding/relocation exists. |
| `stash_shop_placement` | 5 | 4 | Data symbols are ready; shared dungeon/action/entity/sprite functions remain blocked. |
| `stash_multiplayer_metadata_gate` | 2 | 0 | Metadata data symbols are ready, but this hook group alone is not enough to load Stash. |

The scan is intentionally conservative and byte-level. Data symbols can be ready because no prologue detour is required. A function target marked `ready` only means the current backend can reserve an absolute-jump patch window for that prologue; it is not a claim that Stash gameplay detours are installed. A function target marked `blocked` means the direct backend must learn more instruction-aware decoding/relocation before it can patch that prologue without corrupting the installed Steam executable.

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

Current v1 work should not revive `/tmp/barony-src` as a runtime build. Instead:

1. Keep app-side runtime strategy metadata and guardrails oriented around installed PC executables.
2. Keep `native/barony-modloader-hook/` as the first Steam/Linux installed-binary hook runtime.
3. Treat `runtime-load-report.json` as the launch admission report, not proof of gameplay behavior.
4. Treat `symbol-probe-report.json` as proof that required installed-binary symbols resolved in the loaded process.
5. Treat Stash `hookTargets` and `stash-hook-report.json` as the direct Stash hook readiness report and fail-closed gameplay hook scaffold until relocation-safe detour installation exists.
6. Use `native/barony-modloader-runtime/patches/` only as source-level semantic reference while porting Stash behavior into the installed-binary hook runtime.
