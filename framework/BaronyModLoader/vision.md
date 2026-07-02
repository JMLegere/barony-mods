# BaronyModLoader Vision

## Thesis

BaronyModLoader should become the standalone mod management app and framework layer for Barony mods that need more than content replacement. It should give players a clear install/launch experience and give mod authors a stable, validated way to request engine-owned capabilities without maintaining opaque forks.

The project should be framed as a modding platform, not as a tiny engine hook patch. The first shipped capability set is intentionally narrow because it is driven by Stash, but the product architecture should be broad enough to support future framework modules, package tooling, validation, release channels, and compatibility workflows.

## Why a standalone app is necessary

A source patch alone can make Stash work, but it does not solve the player or maintainer problem:

- players need to know which Barony install/build is being launched;
- different profiles may need different mod sets;
- packages need dependency, conflict, and version checks before the game starts;
- a framework-enabled build or patch must be selected, installed, updated, or rolled back;
- logs and validation failures should be accessible outside the game;
- save/multiplayer compatibility should be visible before state is damaged;
- mod authors need a clear target contract instead of instructions to manually edit engine source.

The standalone app is the product surface that makes the framework usable. The engine runtime is the authority that makes gameplay behavior safe.

## Product audiences

### Players

Players should be able to:

- discover a Barony installation;
- create and switch mod profiles;
- install local or released mod packages;
- see compatibility status for their Barony version and framework version;
- launch the correct framework-enabled build;
- inspect logs and validation errors;
- disable a mod or roll back to an unmodified profile without guessing which files changed.

### Mod authors

Mod authors should be able to:

- package a mod with stable identity and version metadata;
- declare dependencies, conflicts, supported Barony/framework versions, and required capabilities;
- request only documented module interfaces;
- rely on framework-owned storage, logs, validation, and compatibility reporting;
- test against Stash as a reference implementation and against future reference mods as the module set expands.

### Engine/framework maintainers

Maintainers should be able to:

- review small, named engine hooks instead of a sprawling source fork;
- reject unsupported capability requests early;
- keep Barony's existing Custom Content, Workshop, maps, JSON, assets, and Barony Script flows intact;
- evolve storage schemas and compatibility rules explicitly;
- know which runtime behaviors are app responsibilities and which are engine responsibilities.

## Guiding principles

1. **Standalone first-class product**
   - BaronyModLoader is an app plus framework, not a folder of patches. The app owns installation and profile experience; the engine runtime owns gameplay.

2. **Manifest-first mods**
   - Mods should declare identity, version, dependencies, target Barony/framework versions, capabilities, storage schemas, multiplayer compatibility, and package metadata before any runtime behavior is enabled.

3. **Engine-owned safety**
   - The engine should provide safe capability modules. Mods do not get arbitrary access to internal state by default.

4. **Full architecture, narrow first modules**
   - Design the platform as if it will grow, but implement only the Stash-required modules first: persistent storage, persistent inventory, Void Chest binding, placement hooks, and multiplayer/version metadata.

5. **Compatibility before convenience**
   - If package, engine, profile, save, or multiplayer state is incompatible, the loader or runtime should reject it with a readable reason before gameplay state diverges or corrupts.

6. **Barony-native, not ecosystem cosplay**
   - Borrow proven concepts from SMAPI, Factorio, Fabric, BepInEx, and tModLoader, but do not copy their broad plugin/runtime scope unless Barony needs it later.

7. **Reference mods prove modules**
   - Stash is the first reference mod. Future module expansion should be justified by similarly concrete mods, not by speculative API design.

## Prior-art inspiration

### SMAPI

SMAPI shows the value of a disciplined mod loader with required manifests, stable ids, semantic versions, dependency declarations, friendly errors, config/data services, and a clear loader identity.

BaronyModLoader should borrow:

- stable unique ids;
- semantic mod/framework versions;
- dependency and conflict checks;
- readable compatibility failures;
- per-mod services such as storage and logging.

BaronyModLoader should not copy immediately:

- arbitrary compiled plugin entrypoints;
- broad runtime patching as the default extension model;
- a large public API before the first Barony-native module set is proven.

### Factorio

Factorio shows how a modded game can treat load order, package metadata, lifecycle boundaries, migrations, and multiplayer determinism as product features.

BaronyModLoader should borrow:

- folder/zip package thinking;
- explicit dependencies and incompatibilities;
- lifecycle vocabulary for validation, storage load, runtime hooks, and migrations;
- deterministic state handling for multiplayer and saves.

BaronyModLoader should not copy immediately:

- a general Lua runtime;
- broad data/control phases beyond what Stash actually requires;
- arbitrary script execution for gameplay logic.

### Fabric

Fabric shows the value of structured metadata and side/environment declarations.

BaronyModLoader should borrow:

- schema-versioned manifests;
- explicit id/version/environment metadata;
- dependency, breakage, and conflict fields;
- namespaced custom metadata for forward compatibility.

BaronyModLoader should not copy immediately:

- mixins;
- Java entrypoints;
- nested binary packages;
- patch injection as the default authoring model.

### BepInEx

BepInEx shows the importance of predictable install paths, loader logs, config files, plugin discovery, dependency metadata, and diagnostics for modded games.

BaronyModLoader should borrow:

- install layout discipline;
- loader logs and diagnostics export;
- per-mod config/data folders;
- dependency metadata and user-readable errors.

BaronyModLoader should not copy immediately:

- arbitrary native/managed plugin loading;
- placing user code directly inside the game process as the first supported model.

### tModLoader

tModLoader shows that a modding framework becomes durable when it has tooling, named hooks, examples, build/reload ergonomics, and a strong teaching path.

BaronyModLoader should borrow:

- documented hook names and timing;
- mod skeleton/package tooling later;
- reference examples as part of the SDK story;
- a path from app UX to authoring UX.

BaronyModLoader should not copy immediately:

- a broad class hierarchy for every content type;
- full hot reload before compatibility, package validation, and first runtime modules are stable.

## Stash as the first proof

Stash is the first reference mod because it needs capabilities that official content modding does not appear to provide cleanly:

- persistent `void_chest_inventory` across runs and saves;
- spell-created Void Chests using the same persistent inventory;
- permanent Void Chest access in the lobby;
- permanent Void Chest access in every shop;
- multiplayer and version metadata so incompatible hosts/clients/saves are rejected clearly.

Stash should prove that BaronyModLoader can express a real gameplay extension as:

1. a package with stable metadata;
2. a manifest with declared capabilities;
3. a narrow engine runtime module set;
4. a loader-managed install/profile/launch flow;
5. readable validation and compatibility behavior.

Stash should not become the only thing the framework can ever do. It should be the reference case that keeps the first implementation honest.

## Phased implementation vision

### Phase 0: Planning and contracts

- Define the product boundary between app, package, runtime, SDK, and reference mod.
- Define a Stash-first manifest shape.
- Define runtime capability names and validation behavior.
- Document what is explicitly out of scope for v1.

### Phase 1: Standalone app foundation

- Discover Barony installs and framework-compatible builds.
- Create profiles and mod enablement state.
- Read package metadata and validate dependencies/conflicts.
- Present compatibility status, launch configuration, and logs.
- Manage patch/build/release artifacts at a profile level.

### Phase 2: Stash-required engine runtime modules

- Add framework manifest loading/validation.
- Add profile-scoped persistent storage.
- Add persistent named inventory support for `void_chest_inventory`.
- Bind all Void Chest access paths to the persistent inventory when Stash is active.
- Add safe lobby and shop placement hooks.
- Add multiplayer/version/save metadata checks.

### Phase 3: Stash reference package

- Package Stash as a BaronyModLoader mod.
- Declare persistent storage, inventory, Void Chest binding, placement hooks, and compatibility metadata.
- Use Stash as the reference mod for app validation, runtime behavior, release notes, and troubleshooting.

### Phase 4: Release and upstreamability

- Produce a versioned framework patch/build against a known Barony source revision.
- Make the app's profile and rollback behavior explicit.
- Keep engine hooks narrow and reviewable.
- Prepare the framework as an upstreamable capability rather than a private fork where possible.

### Phase 5: Future modules only after evidence

Potential later modules should be considered only after the Stash path is working and another concrete mod proves the need. Examples might include additional placement categories, declared NPC/item behavior hooks, richer config UI, package signing, authoring templates, or stronger release channels. Arbitrary code plugins should remain a later, evidence-driven decision, not the foundation of v1.

## Success criteria

BaronyModLoader succeeds when:

- a player can install/enable Stash in a profile and launch a compatible Barony build through the app;
- Stash persists the shared Void Chest inventory across runs and saves;
- spell-created, lobby, and shop Void Chests all reach the same stash inventory;
- multiplayer and save incompatibilities are rejected before state corruption;
- logs and validation errors are understandable outside the game;
- the Stash implementation looks like a framework reference mod, not a pile of Stash-only engine branches;
- future mods have a clear path to request new capabilities without expanding v1 into arbitrary plugin execution.
