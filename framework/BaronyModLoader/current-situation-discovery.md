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

Option C is the current Stash safety boundary: required gameplay hooks are represented as `hookTargets` tied to Stash capabilities, and the native hook analyzes those direct Stash targets through the abstract `linux-x86_64-direct-stash-detour` backend. The backend reports data targets as `ready`, uses the conservative detour decoder to mark function prologues as patch-window-ready, records `patchWindowBytes`, and requires unresolved symbols, blocked detour targets, or uninstalled required hook groups to prevent `runtime-load-report.json` from claiming Stash loaded. Separate opt-in bridges can install pass-through detours without changing that Stash load decision: `BML_STASH_INSTALL_ADD_ITEM_PASSTHROUGH=1` reports the single `Entity::addItemToVoidChestServer` bridge, `BML_STASH_INSTALL_CORE_PASSTHROUGH=1` reports the seven-target core inventory lifecycle bridge for `Entity::getChestInventoryList`, `Entity::addItemToChest`, `Entity::getItemFromChest`, `Entity::addItemToVoidChestServer`, `Entity::removeItemFromVoidChestServer`, `Entity::closeChest`, and `Entity::closeChestServer`, and `BML_STASH_INSTALL_ACCESS_PLACEMENT_PASSTHROUGH=1` reports the eight-target access/placement/prompt bridge for `actChest`, `actChestLid`, `generateDungeon`, `assignActions`, `newEntity`, `setSpriteAttributes`, `uidToEntity`, and `Language::get`. `BML_STASH_ACCESS_PLACEMENT_SELF_TEST=1` adds a fake-provider-only call-through check for those eight replacements, including selected-entity and uidToEntity-backed hover-tooltip scoped `Open stash` prompt replacement with non-Stash non-rename coverage, and the access/placement install report is rewritten at process exit so timed stock-game probes can record live replacement call counts. The relocator now handles the installed `Entity::addItemToChest` thunk shape where a copied prologue branches backward to an out-of-line `.part.0` body, plus the `uidToEntity(int)` prologue shape needed by the hover prompt. The hook also skips inherited `LD_PRELOAD` in non-Barony helper processes unless `BML_HOOK_ALLOW_NON_BARONY=1` is set for fake-provider tests. These reports are deliberately outside `stash-hook-report.json` and do not claim playable behavior.

Current Steam/Linux conservative direct-target scan for build `22630456`, generated by `python native/barony-modloader-hook/tools/analyze_stash_targets.py --manifest native/barony-modloader-hook/manifests/steam-371970-22630456-linux.json --executable /home/jerry/.local/share/Steam/steamapps/common/Barony/barony.x86_64`, using the decoder support implemented through the REX/register/non-RIP-memory, relative-control-flow relocation, RIP-relative ModRM displacement relocation, terminal return at the patch boundary, and remaining two-byte/multi-byte prologue slices:

| Hook group | Ready targets | Blocked targets | Interpretation |
| --- | ---: | ---: | --- |
| `stash_void_chest_binding` | 7 | 0 | `actChest`, `actChestLid`, `Entity::getChestInventoryList`, `Entity::addItemToVoidChestServer`, `Entity::removeItemFromVoidChestServer`, `Language::get`, and `selectedEntity` are ready for the current binding/prompt scope. |
| `stash_inventory_persistence` | 11 | 0 | The `stats` data symbol, getter/add/get/close chest paths including the terminal-return `Entity::closeChestServer` prologue, `newItem`, and the list helper prologues are patch-window-ready. |
| `stash_lobby_placement` | 10 | 0 | Lobby placement/prompt target prologues and data symbols, including `uidToEntity`, `Language::get`, and `selectedEntity`, are ready after relative branch and supported ModRM relocation support. |
| `stash_shop_placement` | 12 | 0 | Shop placement/prompt target prologues and data symbols, including `uidToEntity`, `Language::get`, `selectedEntity`, and `shoparea`, are ready after pxor/lea, relative branch, and supported ModRM relocation support. |
| `stash_multiplayer_metadata_gate` | 2 | 0 | Metadata data symbols are ready. |

The scan is intentionally conservative and byte-level. Data symbols can be ready because no prologue detour is required. A function target marked `ready` only means the current backend can reserve and relocate an absolute-jump patch window for that prologue; it is not a claim that every player-facing Stash scenario is verified. The current Steam/Linux static scan has 42 ready targets, 0 blocked targets, and 0 missing targets across 30 required symbols. Production activation includes `uidToEntity` and `Language::get` for selected-entity and hover-tooltip prompt scoping and loads Stash for validated manifests; real generated-shop placement, persistence, spell-created Void Chest interaction, save/resume, disabled behavior, and multiplayer mismatch proof remain tracked in the deliverables file.

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
5. Treat Stash `hookTargets` and `stash-hook-report.json` as the direct Stash hook readiness and production-load report: the bundle installs for validated Stash manifests, while unsupported builds or missing required symbols/targets still fail closed before launch can claim support.
6. Use `native/barony-modloader-runtime/patches/` only as source-level semantic reference while porting Stash behavior into the installed-binary hook runtime.
