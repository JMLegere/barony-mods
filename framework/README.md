# Barony Mod Framework MVP

The Barony Mod Framework MVP is a small engine-extension plan for mods that need safe, explicit support from Barony's C++ runtime while still remaining data-driven and reviewable. It is scoped around one reference mod first: **Stash**.

Stash turns Barony's existing Void Chest behavior into a profile-persistent shared stash. Instead of maintaining a private source fork with bespoke Stash logic scattered through engine code, the framework defines narrow hooks that the engine owns and mods request through manifests.

## Why this exists

Barony already supports official content modding through Custom Content, Steam Workshop distribution, maps, JSON files, and Barony Script. Those tools are the right path for assets, map content, data changes, and scripted map/editor events.

Stash needs a little more than content replacement:

- a named inventory that persists across runs and normal save lifecycles;
- safe loading and saving under the player profile/mod storage path;
- deterministic placement of Void Chest access points in the lobby and generated shops;
- consistent behavior between permanent Void Chests and spell-created Void Chests;
- multiplayer/version metadata so clients and hosts can reject incompatible mod state instead of corrupting it.

Those requirements are engine-level capabilities. The MVP is the smallest useful bridge between Barony's current content-mod pipeline and a fully custom source fork.

## What the MVP is

The MVP is a proposed upstreamable C++ engine extension that provides:

- a mod manifest format with explicit capability declarations;
- a per-mod storage namespace under the normal profile/output storage location;
- profile-persistent named inventories, starting with `void_chest_inventory`;
- placement hooks for lobby-safe locations and generated shop rooms;
- version, save-compatibility, and multiplayer compatibility metadata;
- a clear packaging model for source patches, builds, and optional Workshop/local metadata.

The intent is that the engine remains authoritative. Mods declare what they need; the engine validates capabilities, owns persistence, performs placement, and serializes multiplayer-relevant metadata.

## Prior-art principles to borrow

The MVP should reuse proven mod-framework patterns without importing their full runtime models:

- **SMAPI-style manifest discipline:** SMAPI requires a `manifest.json` for every mod/content pack and uses fields such as stable unique id, semantic version, minimum game/API versions, dependencies, and update keys. Barony should borrow stable ids, version checks, dependencies, and friendly load rejection, not SMAPI's DLL entrypoint model.
- **Fabric-style metadata separation:** Fabric's `fabric.mod.json` separates mod id, version, dependencies, environment, entrypoints, mixins, and resources. Barony should borrow the idea that metadata is explicit and machine-validated, but not Fabric's code entrypoints or mixin-style patching.
- **Factorio-style lifecycle boundaries:** Factorio separates startup/prototype work from active-save runtime work, tracks dependency order, supports migrations, and treats multiplayer/load-state determinism as a first-class concern. Barony should borrow explicit load phases, migration/version metadata, and desync avoidance, not Factorio's Lua runtime.


## What the MVP is not

The MVP is deliberately not a general native plugin framework.

It does **not** include:

- dynamic DLL/SO plugin loading;
- a Lua runtime;
- a WASM runtime;
- arbitrary entity scripting;
- binary patching of retail executables;
- an open-ended event bus for every engine subsystem;
- a large public API before Stash proves the need.

Those approaches increase security risk, portability risk, maintenance cost, and upstream review burden. They are not required for Stash.

## How Stash uses it

Stash is the reference mod and first acceptance test for the framework.

The intended Stash behavior is:

1. The mod manifest declares a profile-persistent named inventory backed by Barony's existing `void_chest_inventory` concept.
2. On profile/mod load, the engine restores that inventory from mod-scoped persistent storage.
3. Every Void Chest access path reads and writes the same persistent stash inventory, including spell-created temporary Void Chests.
4. The lobby placement hook adds a permanent Void Chest access point near the assist shrine area.
5. The shop placement hook adds one permanent Void Chest access point to every generated shop.
6. The host/server owns stash state in multiplayer and clients interact through the existing chest interaction flow where possible.
7. The engine writes stash state at safe persistence boundaries, such as chest close and normal save/update points.

This lets Stash remain a small mod package plus a narrow BML-owned runtime capability set instead of a long-lived opaque fork.

## Source and distribution stance

The public Barony source remains useful as semantic reference for function names, structs, lifecycle, and reviewable intent. It is not the v1 runtime authority unless it is proven to match a supported installed PC build, and that source-build path is intentionally out of scope for the current BML v1 plan.

The preferred release shape is:

- a standalone BaronyModLoader app for installed PC copies of Barony;
- a BML-owned hook/bootstrap runtime for supported storefront/build/platform combinations;
- versioned executable provenance, hook library checksums, and symbol maps;
- optional Workshop/local content metadata for discovery, preview images, and install instructions;
- rollback instructions that return users to vanilla installed Barony without deleting storefront files.

The framework should be written so its concepts can eventually be proposed upstream as general modding capabilities with Stash as the concrete example, but BML v1 should be judged against installed PC executable support.
