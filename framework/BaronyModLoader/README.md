# BaronyModLoader

BaronyModLoader is a planned standalone modding app and paired engine framework for Barony. It is not just a Stash patch, a one-off source fork, or a loose collection of engine edits. The product goal is to give players a reliable app for installing and launching Barony mods, while giving mod authors a small, explicit framework surface for engine-owned gameplay extensions that Barony's existing content tools cannot express safely.

Barony already has official content modding through Custom Content, Workshop/local packages, maps, JSON data, assets, and Barony Script. BaronyModLoader should complement those systems rather than replace them. Content that can remain data-only should stay data-only. BaronyModLoader exists for the narrower class of mods that need validated install state, profile-aware storage, deterministic runtime hooks, save/multiplayer compatibility metadata, and player-facing package management.

## Product shape

BaronyModLoader has two cooperating halves:

1. **Standalone loader app**
   - Discovers Barony installs and supported source/build targets.
   - Manages mod profiles, enabled packages, dependency resolution, compatibility checks, and launch configuration.
   - Applies or selects the correct framework-enabled engine build/patch for a profile.
   - Validates installed packages before launch and provides readable errors instead of letting the engine fail late.
   - Owns logs, diagnostics export, rollback guidance, package cache, release metadata, and user-facing mod management.

2. **Engine runtime/framework patch**
   - Lives in the Barony engine/source/build selected by the loader app.
   - Loads validated framework manifests and exposes only named, safe gameplay capabilities.
   - Owns persistence, gameplay hook timing, placement resolution, item/container serialization, multiplayer compatibility negotiation, and save-state metadata.
   - Refuses unsupported or incompatible capabilities before gameplay state can be corrupted.

The app is responsible for installation, packages, versions, profiles, patch/build lifecycle, launch, validation, and human-readable diagnostics. The engine runtime is responsible for authoritative gameplay behavior.

## Layer summary

| Layer | Responsibility | First Stash use |
| --- | --- | --- |
| Standalone loader app | Install discovery, profiles, packages, dependency/version checks, patch/build/release management, launch, validation, logs | Install/enable Stash against a compatible framework-enabled Barony build |
| Mod package format | Stable ids, versions, dependencies/conflicts, Barony/framework targets, capabilities, assets, metadata | A Stash package declaring persistent inventory, Void Chest binding, placement hooks, and multiplayer metadata |
| Engine runtime/framework patch | Safe engine-owned hooks and capability execution | Persistent storage, persistent `void_chest_inventory`, Void Chest routing, lobby/shop placement, multiplayer/version checks |
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

## Steam compatibility stance

BaronyModLoader should work with the Steam version of Barony, but not by blindly binary-patching the stock Steam executable. The intended clean path is:

1. Detect the installed Steam copy of Barony and record its app id, build id, install path, executable, and asset root.
2. Use the Steam install as the owned game/data/assets source.
3. Select or build a matching BaronyModLoader-enabled runtime executable for that Steam build.
4. Launch that framework-enabled runtime against the Steam install/profile metadata.

The current CLI can detect Jeremy's Steam install (`appid 371970`, local build id `22630456`) and create a Steam-backed BML profile. Stock `barony.x86_64` is not considered patched or Stash-capable until a matching BML runtime executable is built/selected.

This keeps the user experience aimed at the Steam version while avoiding the fragile route of modifying the retail binary in place.

## Executable CLI workflow

The current app slice is a Python standard-library CLI. From the repository root, the expected local Steam commands are:

```sh
python framework/BaronyModLoader/app/barony_mod_loader.py version
python framework/BaronyModLoader/app/barony_mod_loader.py steam detect
python framework/BaronyModLoader/app/barony_mod_loader.py profile create .tmp/bml-steam-profile --id steam-default --steam --runtime-info framework/BaronyModLoader/fixtures/runtime-info.stash.json
```

For non-Steam or not-yet-built runtime paths, pass the executable explicitly:

```sh
python framework/BaronyModLoader/app/barony_mod_loader.py profile create .tmp/bml-profile --id default --barony-executable /path/to/barony --runtime-info framework/BaronyModLoader/fixtures/runtime-info.stash.json
```

Then the package workflow is:

```sh
python framework/BaronyModLoader/app/barony_mod_loader.py package validate framework/BaronyModLoader/example-stash-package.json
python framework/BaronyModLoader/app/barony_mod_loader.py package validate mods/stash
python framework/BaronyModLoader/app/barony_mod_loader.py package pack mods/stash --out .tmp/Stash-0.1.0.bmlpkg
python framework/BaronyModLoader/app/barony_mod_loader.py package install .tmp/Stash-0.1.0.bmlpkg --store .tmp/bml-package-store
python framework/BaronyModLoader/app/barony_mod_loader.py runtime validate framework/BaronyModLoader/fixtures/runtime-info.stash.json --package .tmp/bml-package-store/jml.stash/0.1.0
python framework/BaronyModLoader/app/barony_mod_loader.py runtime info framework/BaronyModLoader/fixtures/runtime-info.stash.json
python framework/BaronyModLoader/app/barony_mod_loader.py runtime report framework/BaronyModLoader/fixtures/runtime-load-report.loaded.json
python framework/BaronyModLoader/app/barony_mod_loader.py runtime report framework/BaronyModLoader/fixtures/runtime-load-report.failed.json
python framework/BaronyModLoader/app/barony_mod_loader.py runtime register --registry .tmp/bml-runtime-registry.json --id steam-371970-22630456-dev --executable /path/to/barony-bml.x86_64 --runtime-info framework/BaronyModLoader/fixtures/runtime-info.stash.json --steam-build-id 22630456
python framework/BaronyModLoader/app/barony_mod_loader.py runtime list --registry .tmp/bml-runtime-registry.json
python framework/BaronyModLoader/app/barony_mod_loader.py runtime inspect steam-371970-22630456-dev --registry .tmp/bml-runtime-registry.json
python framework/BaronyModLoader/app/barony_mod_loader.py profile enable .tmp/bml-steam-profile --package .tmp/bml-package-store/jml.stash/0.1.0
python framework/BaronyModLoader/app/barony_mod_loader.py profile inspect .tmp/bml-steam-profile
python framework/BaronyModLoader/app/barony_mod_loader.py launch-plan .tmp/bml-steam-profile --package .tmp/bml-package-store/jml.stash/0.1.0 --runtime-info framework/BaronyModLoader/fixtures/runtime-info.stash.json --out .tmp/bml-steam-profile/BaronyModLoader/runtime-manifest.json
python framework/BaronyModLoader/app/barony_mod_loader.py launch .tmp/bml-steam-profile --package .tmp/bml-package-store/jml.stash/0.1.0 --registry .tmp/bml-runtime-registry.json --runtime steam-371970-22630456-dev --dry-run -- --example-barony-arg
python framework/BaronyModLoader/app/barony_mod_loader.py profile disable .tmp/bml-steam-profile --mod-id jml.stash
python framework/BaronyModLoader/app/barony_mod_loader.py profile inspect .tmp/bml-steam-profile
```

`package install` stores the package under the selected package store and prints the installed package path. The `profile enable`, `launch-plan`, and `launch` examples intentionally use that installed package directory instead of the source `mods/stash` tree so the launch contract reflects the archived, installed package bytes. This slice validates package/runtime metadata, writes profile activation state, registers BML runtime executables, writes runtime-manifest/active-mods artifacts, and can dry-run or execute a selected runtime command. It does not by itself verify a playable patched Barony runtime or Stash gameplay behavior until a real BML-enabled Barony executable is built and registered.

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

Stash augments Barony's existing Void Chest behavior so that players get a profile-persistent shared stash:

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

- arbitrary native DLL/SO plugin loading;
- Lua, WASM, or another general scripting runtime;
- binary patching of retail executables as the primary mod model;
- a broad event bus across every engine subsystem;
- hot reload before package validation and core runtime hooks are stable;
- a public SDK larger than the module surface required by Stash;
- replacement of Barony's official Custom Content, Workshop, JSON, map, asset, or Barony Script workflows.

These may be revisited later if real mods justify them. They are not required to make Stash work.

## Planned documentation set

- `vision.md` defines the product vision, user/modder value, prior-art influence, and phased scope.
- `architecture.md` defines the system layers, app/runtime split, package/runtime contracts, first module set, and Stash reference flow.
- `mvp-v1-long-term-plan.md` defines the long-term-aligned MVP plan for Stash and BaronyModLoader v1.0: Steam-backed runtime sidecar, app launcher, runtime registry, Stash modules, verification, and explicit post-v1 scope.
