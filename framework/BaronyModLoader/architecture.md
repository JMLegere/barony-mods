# BaronyModLoader Architecture

## Architecture goal

BaronyModLoader is a standalone loader app paired with a Barony engine runtime/framework patch. The architecture separates player-facing package/profile/build management from engine-authoritative gameplay hooks.

The key design rule is:

> Build the architecture as a full standalone modding framework, but implement only the concrete runtime modules and authoring interface required by Stash first.

This avoids two failure modes:

- **Too small:** a hardcoded Stash patch that cannot become a reusable framework.
- **Too broad:** an arbitrary plugin system copied from another ecosystem before Barony has a proven need for it.

## System layers

```text
BaronyModLoader standalone app
  ├─ install discovery
  ├─ profiles
  ├─ package manager
  ├─ dependency/version validator
  ├─ framework patch/build/release manager
  ├─ launcher
  └─ logs/diagnostics UI

Mod package format
  ├─ identity and version metadata
  ├─ Barony/framework compatibility
  ├─ dependencies/conflicts
  ├─ declared capabilities
  ├─ assets/content metadata
  └─ storage/multiplayer compatibility metadata

Barony engine runtime/framework patch
  ├─ manifest loader and validator
  ├─ persistent mod storage
  ├─ persistent named inventory module
  ├─ Void Chest binding module
  ├─ placement hook module
  └─ multiplayer/save/version metadata module

Module SDK/interface
  ├─ documented manifest fields
  ├─ supported capability names
  ├─ data schemas
  ├─ hook timing and guarantees
  └─ diagnostics/error contract

Stash reference mod
  ├─ package metadata
  ├─ Stash manifest declarations
  ├─ optional assets/presentation metadata
  └─ reference validation scenario
```

## Responsibility split

### Standalone loader app responsibilities

The app owns everything that happens before the selected Barony process starts and everything needed to explain a modded launch to a human.

Responsibilities:

- **Install discovery**
  - Locate Barony installs, source/build directories, and supported platform targets.
  - Detect whether an install is vanilla, framework-enabled, or incompatible.


#### Steam install target

The primary player target is the Steam version of Barony. The loader should treat the Steam install as the canonical owned game install and asset root, starting with app id `371970`.

The loader may detect and record the stock Steam executable, install directory, appmanifest path, and Steam build id, but v1 should not mutate the retail executable in place. Stash requires engine behavior changes, so a Steam-backed profile needs a BaronyModLoader-enabled runtime executable that is compatible with the detected Steam build and uses the Steam install for data/assets.

This makes the compatibility model explicit:

- **Stock Steam Barony**: discovered and launchable as vanilla, but not Stash-capable.
- **Steam-backed BML runtime**: preferred modded path; built/selected for the detected Steam build and pointed at the Steam install/profile metadata.
- **Source/build directory**: development path for producing and verifying the Steam-backed BML runtime.

- **Profiles**
  - Maintain isolated mod enablement sets.
  - Associate a profile with a Barony install/build, framework version, package set, and launch options.
  - Keep profile state distinct from engine-owned gameplay storage.

- **Packages**
  - Install, remove, enable, disable, and update mod packages.
  - Validate package layout and manifest syntax before launch.
  - Maintain package cache/release metadata and checksums when available.

- **Dependencies and conflicts**
  - Resolve required dependencies.
  - Warn on optional/recommended dependencies.
  - Reject known conflicts or unsupported framework capabilities.

- **Patch/build/release management**
  - Select or install a framework-enabled Barony build compatible with the active profile.
  - Track upstream Barony source revision/build compatibility.
  - Support rollback to prior framework builds or vanilla launch paths where possible.

- **Launch**
  - Construct the launch environment for the selected Barony build and profile.
  - Pass the active mod/profile metadata path to the engine runtime.
  - Refuse launch when validation failures would be unsafe.

- **Validation and diagnostics**
  - Present readable preflight errors.
  - Surface engine runtime logs after launch.
  - Export a diagnostics bundle that includes profile metadata, package manifests, framework version, engine version, and logs.

The app must not be responsible for authoritative item storage, placement, container routing, or multiplayer state ownership. Those belong in the engine runtime.

### Engine runtime/framework patch responsibilities

The engine runtime owns gameplay behavior once Barony starts. It must treat loader-provided package/profile state as input, then validate and execute only supported capabilities.

Responsibilities:

- **Manifest ingestion**
  - Read the active package manifests selected by the loader.
  - Validate schema version, mod id, mod version, target Barony/framework versions, and declared capabilities.
  - Reject unsupported or incompatible capabilities before gameplay begins.

- **Capability registry**
  - Build a runtime registry of enabled, validated capability declarations.
  - Keep hook execution explicit and capability-scoped.

- **Persistent storage**
  - Provide engine-owned per-mod, profile-scoped storage.
  - Version stored data and record framework/mod/schema metadata.
  - Perform safe reads/writes at known persistence boundaries.

- **Persistent named inventory**
  - Serialize and restore supported Barony item inventory data.
  - Track dirty state and persist on safe boundaries.
  - Treat malformed or incompatible mod state as a recoverable framework error where possible.

- **Void Chest binding**
  - Route declared Void Chest access paths to the selected persistent named inventory.
  - Preserve existing interaction/network behavior where possible.
  - Ensure spell-created and permanent Void Chests use the same declared inventory when Stash is active.

- **Placement hooks**
  - Resolve manifest placement requests into safe engine placements.
  - Choose valid tiles/entities; the manifest requests intent, not raw map mutation authority.
  - Log placement failures without corrupting the map.

- **Multiplayer and save compatibility**
  - Advertise active framework/mod/version/capability metadata.
  - Make host/server authority explicit for persistent Stash state.
  - Reject incompatible clients/saves before item state diverges.

The runtime must not become an arbitrary plugin loader in v1. It is a controlled set of engine-owned modules.

## Mod package format layer

The package format is the bridge between the app and the engine runtime. The app validates package shape and dependency metadata; the engine validates runtime capability declarations.

A package should eventually support:

- package id and display name;
- semantic package version;
- author/source/homepage/update metadata;
- Barony version/source revision compatibility;
- BaronyModLoader app/framework version compatibility;
- dependencies, optional dependencies, recommendations, conflicts, and known breaks;
- declared engine capabilities;
- storage schema version and migration metadata;
- multiplayer compatibility policy;
- assets/content folders that remain compatible with Barony's official content systems;
- diagnostics metadata for logs and support.

For Stash, the package format only needs enough to declare:

- `stash` as a stable mod id;
- Stash package version;
- compatible Barony/framework version range;
- required capabilities for persistent storage, persistent inventory, Void Chest binding, placement hooks, and multiplayer/version metadata;
- one persistent inventory declaration for `void_chest_inventory`;
- lobby and shop placement declarations;
- host-owned multiplayer state policy.

## Engine runtime module set for Stash

The first implementation should expose exactly the module set Stash needs. Each module should be designed with framework naming and validation, but implemented narrowly.

### 1. Manifest and capability validation

Purpose:

- Establish a supported mod identity and capability registry before gameplay begins.

Initial Stash need:

- Load the Stash manifest.
- Confirm Barony/framework compatibility.
- Confirm Stash's required capabilities exist.
- Reject the mod with a readable error if the build cannot support Stash.

Boundary:

- This is metadata validation, not arbitrary mod code execution.

### 2. Persistent storage

Purpose:

- Provide per-mod, profile-scoped state that survives run boundaries and normal save lifecycle behavior.

Initial Stash need:

- Store the persistent shared stash inventory outside a single save/run's lifetime.
- Include schema/version metadata so future Stash storage migrations are possible.

Boundary:

- The engine runtime owns paths and serialization. The mod does not get arbitrary filesystem access.

### 3. Persistent named inventory

Purpose:

- Let a validated mod declare an engine-owned named inventory that can be restored and saved consistently.

Initial Stash need:

- Declare a persistent named inventory bound to Barony's existing `void_chest_inventory` behavior.
- Restore stored item state when the profile/mod loads.
- Persist when inventory contents change and at safe save/update boundaries.

Boundary:

- The first implementation supports the Stash inventory case. It should not become a generalized arbitrary item scripting API.

### 4. Void Chest binding

Purpose:

- Route existing Void Chest access logic through a declared persistent named inventory when a validated capability is active.

Initial Stash need:

- Permanent Void Chests and spell-created Void Chests all open the same persistent stash inventory.
- Existing chest UI and interaction flow should be reused where possible.

Boundary:

- The module changes inventory binding policy, not every chest behavior in the game.

### 5. Placement hooks

Purpose:

- Let a manifest request engine-resolved permanent access point placement at supported map lifecycle points.

Initial Stash need:

- Lobby placement: create a permanent Void Chest access point in a safe lobby location.
- Shop placement: create one permanent Void Chest access point in every generated shop.

Boundary:

- The manifest should declare placement intent. The engine chooses safe coordinates/entities and may reject or log failures.

### 6. Multiplayer/version metadata

Purpose:

- Prevent silent desync or state corruption in modded sessions.

Initial Stash need:

- Host/server owns Stash persistent inventory state.
- Clients must match required framework/mod/capability versions or be rejected clearly.
- Saves/storage records should include enough metadata to detect incompatible state.

Boundary:

- This is compatibility negotiation and ownership metadata, not a broad networking SDK.

## Stash reference flow

The reference flow should prove both app and runtime responsibilities.

1. Player installs or selects a framework-enabled Barony build in the BaronyModLoader app.
2. Player creates or selects a profile.
3. Player installs/enables the Stash package.
4. The app validates package layout, dependency/conflict metadata, supported Barony/framework version ranges, and required capabilities.
5. The app launches the selected Barony build with the active profile/package metadata.
6. The engine runtime loads and validates the Stash manifest.
7. The engine opens Stash's profile-scoped storage namespace.
8. The persistent named inventory module restores `void_chest_inventory` if valid storage exists.
9. Lobby setup runs; the placement module creates a permanent Void Chest access point at a safe lobby location.
10. Dungeon/shop generation runs; the placement module creates one permanent Void Chest access point in each generated shop.
11. Player opens any Void Chest access point.
12. The Void Chest binding module routes the access to the persistent Stash inventory.
13. Spell-created Void Chests resolve to the same persistent inventory.
14. On chest close and safe persistence boundaries, the engine writes the inventory to Stash storage.
15. In multiplayer, host/server metadata controls whether clients can join and who owns the persistent stash state.
16. The app surfaces logs and compatibility diagnostics after the session.

## Loader app validation pipeline

The app should validate from cheapest/static checks to launch-time checks:

1. **Package shape**
   - Required manifest file exists.
   - Package id and version are valid.
   - Package contents are inside expected folders.

2. **Profile compatibility**
   - Enabled mods are compatible with the selected profile.
   - Dependencies are present and version-compatible.
   - Conflicts/breaks are absent.

3. **Barony/framework compatibility**
   - Selected Barony build matches package target range.
   - Framework runtime version supports required capabilities.
   - Required patch/build artifact is installed or selectable.

4. **Launch readiness**
   - Active profile has a resolved package graph.
   - Runtime manifest bundle can be generated.
   - No known unsafe save/multiplayer compatibility warning is unresolved.

5. **Post-launch diagnostics**
   - Runtime validation errors are collected.
   - Logs can be viewed/exported.
   - App can identify which package/profile/build caused the issue.

## Runtime validation pipeline

The engine runtime should still validate even if the app already did. The app improves UX; the runtime protects gameplay state.

1. Read active framework manifest bundle.
2. Validate manifest schema and ids.
3. Validate target Barony/framework versions.
4. Validate capabilities against compiled runtime support.
5. Validate storage schema compatibility.
6. Build capability registry.
7. Load profile-scoped persistent state.
8. Apply hooks only at documented lifecycle points.
9. Write compatibility metadata into saves/sessions.
10. Reject or degrade safely on unsupported/incompatible state.

## Lifecycle boundaries

The first runtime lifecycle can stay small:

- **Preflight/app validation**: package graph, versions, dependencies, conflicts, framework build selection.
- **Runtime manifest validation**: engine confirms active declarations and supported capabilities.
- **Profile state load**: persistent mod storage opens before relevant gameplay systems use it.
- **Map/lobby/shop hook points**: placement requests execute only at known safe generation/setup points.
- **Interaction hook points**: Void Chest access resolves through the persistent named inventory binding.
- **Persistence boundaries**: inventory writes happen on chest close and other safe save/update moments.
- **Multiplayer/session negotiation**: active framework metadata is exchanged before incompatible clients can participate.

This borrows lifecycle discipline from Factorio and hook discipline from tModLoader without adding a general script runtime.

## Error and diagnostics contract

BaronyModLoader should prefer explicit failure over silent corruption.

App-level errors should explain:

- missing/incompatible Barony install;
- missing framework-enabled build;
- unsupported package schema;
- missing dependency;
- conflict between enabled mods;
- unsupported required capability;
- package requires a different Barony/framework version.

Runtime-level errors should explain:

- manifest rejected by engine;
- storage schema incompatible;
- persistent inventory failed to load safely;
- placement request could not be fulfilled;
- multiplayer client/host framework metadata mismatch;
- save contains incompatible framework/mod state.

Both app and runtime logs should include mod id, mod version, framework version, Barony build/source revision where known, profile id/name, and the failing capability.

## Non-goals and deferred surfaces

The first architecture intentionally defers:

- arbitrary native DLL/SO plugins;
- broad C/C++/C#/Lua/WASM mod execution;
- executable patch injection into retail binaries as the main product path;
- universal event subscription across all engine systems;
- generalized entity scripting;
- hot reload;
- package signing or central registry requirements;
- broad SDK generation beyond the Stash-required manifest/module interface.

These are not banned forever. They are not first implementation requirements.

## Future expansion model

Future framework growth should follow a reference-mod rule:

1. A concrete mod demonstrates a real unsupported need.
2. The need is expressed as a narrow engine-owned module or manifest capability.
3. The app adds package/profile/build validation for that capability.
4. The runtime adds a named hook or service with clear lifecycle timing.
5. A reference mod and diagnostic scenario prove the module.

This keeps BaronyModLoader from becoming either a brittle Stash-only patch or an unsafe arbitrary plugin loader.

## Architecture acceptance for Stash

The first architecture is good enough when all of the following are true:

- BaronyModLoader is presented and implemented as a standalone app plus paired runtime, not only an engine patch.
- Stash is a mod package/reference mod, not a hidden hardcoded feature.
- App responsibilities are separate from runtime responsibilities.
- The first runtime modules cover persistent storage, persistent inventory, Void Chest binding, placement hooks, and multiplayer/version metadata.
- `void_chest_inventory` persists across runs and saves.
- Spell-created Void Chests use the same persistent inventory.
- Permanent Void Chest access exists in the lobby and every shop.
- Multiplayer/save incompatibilities are rejected clearly.
- The design borrows discipline from SMAPI, Factorio, Fabric, BepInEx, and tModLoader without copying their arbitrary plugin/runtime scope into v1.
