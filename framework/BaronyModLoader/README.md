# BaronyModLoader

BaronyModLoader is a planned standalone modding app and paired engine framework for Barony. It is not just a Stash patch, a one-off source fork, or a loose collection of engine edits. The product goal is to give players a reliable app for installing and launching Barony mods, while giving mod authors a small, explicit framework surface for engine-owned gameplay extensions that Barony's existing content tools cannot express safely.

Barony already has official content modding through Custom Content, Workshop/local packages, maps, JSON data, assets, and Barony Script. BaronyModLoader should complement those systems rather than replace them. Content that can remain data-only should stay data-only. BaronyModLoader exists for the narrower class of mods that need validated install state, profile-aware storage, deterministic runtime hooks, save/multiplayer compatibility metadata, and player-facing package management.

## Product shape

BaronyModLoader has two cooperating halves:

1. **Standalone loader app**
   - Discovers Barony installs and supported runtime/hook targets.
   - Manages mod profiles, enabled packages, dependency resolution, compatibility checks, and launch configuration.
   - Registers and launches the correct BML hook/bootstrap runtime for a profile.
   - Validates installed packages before launch and provides readable errors instead of letting the engine fail late.
   - Owns logs, diagnostics export, rollback guidance, package cache, release metadata, and user-facing mod management.

2. **Engine runtime/framework hook**
   - Lives in the BML-owned hook/bootstrap runtime loaded with the installed Barony executable selected by the loader app.
   - Loads validated framework manifests and exposes only named, safe gameplay capabilities.
   - Owns persistence, gameplay hook timing, placement resolution, item/container serialization, multiplayer compatibility negotiation, and save-state metadata.
   - Refuses unsupported or incompatible capabilities before gameplay state can be corrupted.

The app is responsible for installation, packages, versions, profiles, runtime provenance, hook lifecycle, launch, validation, and human-readable diagnostics. The engine runtime is responsible for authoritative gameplay behavior.

## Layer summary

| Layer | Responsibility | First Stash use |
| --- | --- | --- |
| Standalone loader app | Install discovery, profiles, packages, dependency/version checks, runtime/provenance management, launch, validation, logs | Install/enable Stash against a compatible installed Barony runtime target |
| Mod package format | Stable ids, versions, dependencies/conflicts, Barony/framework targets, capabilities, assets, metadata | A Stash package declaring persistent inventory, Void Chest binding, placement hooks, and multiplayer metadata |
| Engine runtime/framework hook | Safe engine-owned hooks and capability execution through BML-owned bootstrap code | Persistent storage, persistent `void_chest_inventory`, Void Chest routing, lobby/shop placement, multiplayer/version checks |
| Module SDK/interface | Documented declarations and narrow APIs for mod authors | The first module set required by Stash only |
| Stash reference mod | First real mod proving the framework end-to-end | Shared persistent Void Chest stash across runs, saves, spell chests, lobby, and shops |

## Full framework, narrow first implementation

BaronyModLoader should be designed as a real standalone framework from the beginning, but the first implementation should be deliberately narrow. The initial module set exists to ship **Stash** correctly and to prove the architecture without prematurely copying the full scope of larger modding ecosystems.

- `persistent_storage`: per-mod, profile-scoped, versioned storage that survives runs and normal save lifecycle boundaries.
- `persistent_inventory`: engine-owned serialization and restoration of named inventories, starting with `void_chest_inventory`.
- `void_chest_binding`: all Void Chest access paths, including spell-created Void Chests, can bind to the same persistent named inventory.
- `placement_lobby`: safe permanent Void Chest access point placement in the lobby.
- `placement_shop`: safe permanent Void Chest access point placement in every generated shop.
- `multiplayer_version_metadata`: host-owned state, compatible client negotiation, framework/mod version reporting, and save/storage compatibility detection.

This is not a rejection of a larger future SDK. It is a sequencing rule: BaronyModLoader should have the architecture of a full framework while implementing only the concrete modules Stash needs first.

## PC storefront compatibility stance

BaronyModLoader should work with installed PC copies of Barony, not with an opaque source fork and not by modifying the retail executable on disk. The v1 product target is:

| Storefront | PC platforms in scope |
| --- | --- |
| Steam | Windows, macOS, Linux |
| Epic Games Store | Windows, macOS |
| GOG / DRM-free | Windows, macOS, Linux |
| Humble Bundle | Windows, macOS, Linux |

Nintendo Switch is out of scope for this native PC mod-loader approach.

The first concrete implementation target is the local Steam/Linux executable because it is available for direct inspection (`appid 371970`, local build id `22630456`). The app/runtime contract should generalize to other PC storefronts by recording store/build/executable provenance and launching the installed game through a BML-owned hook/bootstrap library.

## Mod organization

The repository separates framework code, native hook/bootstrap work, historical source-patch references, reusable templates, and actual mods:

```text
framework/BaronyModLoader/   # standalone BML app, schemas, contracts, fixtures, scenarios, and architecture docs
native/barony-modloader-hook/ # planned BML-owned installed-executable hook/bootstrap runtime
native/barony-modloader-runtime/
  patches/                   # historical/semantic Barony source patch references, not the v1 runtime path
mods/
  stash/                     # source package for the Stash reference mod
templates/mod/               # skeleton layout for future mods
tools/                       # legacy/project helper scripts
```

Each mod package should live under `mods/<mod-id>/` and use this shape:

```text
mods/<mod-id>/
  bml-package.json           # BML package identity, versions, dependencies, capabilities, runtime requirements
  workshop.toml              # Steam Workshop/local publishing metadata
  preview.png                # workshop/manager preview image
  content/
    books/
    data/
    images/
    items/
    lang/
    maps/
    models/
    music/
    sounds/
```

`content/` mirrors Barony's content-mod categories so data/assets that can remain ordinary Barony content stay ordinary Barony content. `bml-package.json` declares the extra framework/runtime capabilities needed by mods that go beyond content, such as Stash's persistent inventory and placement hooks. For v1, the BML app installs package archives into a package store under `<store>/<package-id>/<version>/`, and profiles enable those installed package directories rather than the mutable source tree.

Stash is currently organized as:

```text
mods/stash/
  bml-package.json           # package id `jml.stash`, version `0.1.0`, Stash capability declarations
  workshop.toml              # human-facing publishing metadata
  preview.png                # placeholder preview
  content/                   # reserved for future Stash assets/data if needed
```

The native behavior for Stash is not stored inside `mods/stash/content/`; it is implemented by the BML-owned hook/bootstrap runtime and activated only when the validated Stash package/profile/runtime manifest is loaded. The old `native/barony-modloader-runtime/patches/` files are retained as semantic references for hook design, not as the supported v1 runtime path.


## Executable CLI workflow

The current app slice is a Python standard-library CLI. The existing commands can validate packages and detect the local Steam install:

```sh
python framework/BaronyModLoader/app/barony_mod_loader.py version
python framework/BaronyModLoader/app/barony_mod_loader.py steam detect
python framework/BaronyModLoader/app/barony_mod_loader.py package validate mods/stash
python framework/BaronyModLoader/app/barony_mod_loader.py package pack mods/stash --out .tmp/Stash-0.1.0.bmlpkg
python framework/BaronyModLoader/app/barony_mod_loader.py package install .tmp/Stash-0.1.0.bmlpkg --store .tmp/bml-package-store
python framework/BaronyModLoader/app/barony_mod_loader.py profile create .tmp/bml-steam-profile --id steam-default --steam
python framework/BaronyModLoader/app/barony_mod_loader.py profile enable .tmp/bml-steam-profile --package .tmp/bml-package-store/jml.stash/0.1.0
```

The current Linux hook smoke uses the built native hook library path from this repository. The example below is intentionally a development path; packaged releases can register a relocated absolute hook path while preserving the same installed-executable launch contract:

```sh
python framework/BaronyModLoader/app/barony_mod_loader.py runtime register \
  --registry .tmp/bml-runtime-registry.json \
  --id steam-linux-371970-22630456-hook-dev \
  --runtime-strategy installed-binary-hook \
  --steam-build-id 22630456 \
  --steam-executable /home/jerry/.local/share/Steam/steamapps/common/Barony/barony.x86_64 \
  --hook-library native/barony-modloader-hook/build/libbarony_bml.so \
  --hook-manifest native/barony-modloader-hook/manifests/steam-371970-22630456-linux.json \
  --runtime-info framework/BaronyModLoader/fixtures/runtime-info.installed-hook.stash.json
```

Launch should then execute the installed game executable with BML hook environment rather than executing a source-built `barony-bml.x86_64` sidecar:

```sh
python framework/BaronyModLoader/app/barony_mod_loader.py launch \
  .tmp/bml-steam-profile \
  --package .tmp/bml-package-store/jml.stash/0.1.0 \
  --registry .tmp/bml-runtime-registry.json \
  --runtime steam-linux-371970-22630456-hook-dev \
  --dry-run \
  -- --example-barony-arg
```

`package install` stores the package under the selected package store and prints the installed package path. The `profile enable`, `launch-plan`, and `launch` examples intentionally use that installed package directory instead of the source `mods/stash` tree so the launch contract reflects the archived, installed package bytes. This slice validates package/runtime metadata, writes profile activation state, writes runtime-manifest/active-mods artifacts, and dry-runs the installed-executable hook launch.

## Runnable verification scenarios

The current native hook smoke is Linux-only and proves injection/reporting and symbol-probe plumbing only: `LD_PRELOAD=native/barony-modloader-hook/build/libbarony_bml.so /usr/bin/true` should load the hook and write `<profile>/BaronyModLoader/reports/runtime-load-report.json` when the BML environment points at a profile, runtime manifest, and hook manifest. It does not install gameplay detours or create the Stash chest in-game.

The intended staged verification sequence is:

1. **No-op hook load:** launch `/usr/bin/true` or the installed Steam/Linux executable with `LD_PRELOAD=native/barony-modloader-hook/build/libbarony_bml.so`, a BML profile, and a runtime manifest; verify `BaronyModLoader/reports/runtime-load-report.json` is written.
2. **Provenance success/failure:** verify matching Steam build/executable hash/version succeeds and mismatched provenance fails closed before gameplay hooks install.
3. **Symbol probe:** resolve a harmless symbol/global for Steam/Linux build `22630456` and write diagnostics without mutating gameplay state.
4. **Lobby/shop placement:** after gameplay hooks exist, verify Stash access points are created by the installed game process when Stash is active.
5. **Inventory persistence:** after gameplay hooks exist, verify the shared Void Chest inventory survives save/resume, death/new-run, and relaunch boundaries.
6. **Disabled/mismatch behavior:** verify no Stash hooks activate when Stash is disabled and incompatible runtime/package metadata blocks clearly.

The previous `/tmp/barony-bml-build/barony` source-build smoke path is obsolete for v1 Steam-current support.


## Inspiration without premature scope

BaronyModLoader should borrow proven ideas from existing modding ecosystems while staying appropriate for Barony:

- **SMAPI**: manifest discipline, stable mod ids, semantic versions, dependency checks, readable load failures, and framework-owned services. Do not copy the compiled DLL plugin model as v1 scope.
- **Factorio**: package structure, dependency ordering, lifecycle boundaries, migration/version thinking, and multiplayer determinism. Do not add a general Lua runtime just to imitate Factorio.
- **Fabric**: explicit metadata, environment/side compatibility, dependency/conflict declarations, and namespaced custom fields. Do not start with mixins, bytecode-style patching, or broad entrypoints.
- **BepInEx**: predictable install paths, logging, config/data folders, plugin discovery discipline, and diagnostics. Do not begin by loading arbitrary native/user code into the game.
- **tModLoader**: named hooks, tooling, skeleton generation, build/reload ergonomics, and strong example mods. Do not copy a broad class-based content API before Barony's narrow hooks are proven.

The principle is to borrow product shape and operational discipline, not arbitrary code execution scope.

## Stash as the first reference mod

Stash is the first reference mod and the first acceptance test for BaronyModLoader.

The current Linux hook smoke does not implement the Stash chest, placement, persistence, or gameplay hooks. The target Stash behavior remains:

1. The Stash package declares a persistent named inventory mapped to Barony's existing `void_chest_inventory` concept.
2. The engine runtime loads that inventory from Stash's profile storage namespace.
3. Every Void Chest access route reads and writes the same persistent inventory, including spell-created Void Chests.
4. A permanent Void Chest access point appears in the lobby.
5. A permanent Void Chest access point appears in every shop.
6. Saves and multiplayer sessions record enough framework/mod metadata to reject incompatible states safely.
7. The loader app presents install, compatibility, launch, logs, and rollback information clearly to the user.

If Stash cannot be described as a package plus narrow framework modules, the framework design is not yet good enough.

## Non-goals for the first implementation

BaronyModLoader's initial implementation should not include:

- arbitrary native DLL/SO plugin loading by third-party mods;
- Lua, WASM, or another general scripting runtime;
- source-built Barony runtimes as a supported v1 path;
- binary patching of retail executables on disk;
- a broad event bus across every engine subsystem;
- hot reload before package validation and core runtime hooks are stable;
- a public SDK larger than the module surface required by Stash;
- replacement of Barony's official Custom Content, Workshop, JSON, map, asset, or Barony Script workflows.

These may be revisited later if real mods justify them. They are not required to make Stash work.

## Planned documentation set

- `vision.md` defines the product vision, user/modder value, prior-art influence, and phased scope.
- `architecture.md` defines the system layers, app/runtime split, package/runtime contracts, first module set, and Stash reference flow.
- `mvp-v1-long-term-plan.md` defines the long-term-aligned MVP plan for Stash and BaronyModLoader v1.0: installed PC executable hook/bootstrap runtime, app launcher, runtime registry, Stash modules, verification, and explicit post-v1 scope.
