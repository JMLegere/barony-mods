# Barony Mod Framework MVP Plan

## Problem

Barony's official modding path supports Custom Content, Steam Workshop packages, maps, JSON data, and Barony Script. That is enough for many content mods, but not enough for Stash.

Stash needs engine-owned behavior:

- persist a shared inventory across runs, not only inside one save/resume lifecycle;
- route existing Void Chest interactions, including spell-created Void Chests, to that persistent inventory;
- place permanent Void Chest access points in the lobby and every generated shop;
- keep multiplayer and save compatibility explicit.

A raw source fork can implement this, but it is a poor long-term packaging story. The framework MVP should make the required engine behavior small, data-driven, reviewable, and reusable for future mods.

## MVP scope

The MVP should provide only the framework pieces required to ship Stash cleanly and leave room for future mods:

1. **Manifest capabilities**
   - Mod declares required engine capabilities.
   - Engine rejects unsupported or incompatible manifests early.
   - Manifest includes mod id, version, target Barony version/source revision, multiplayer compatibility, and save/storage compatibility.

2. **Persistent mod storage**
   - Engine exposes a per-mod profile storage namespace.
   - Storage is owned by the engine, not by arbitrary plugin code.
   - Data is versioned so migrations can be explicit later.

3. **Persistent named inventories**
   - Engine supports named inventory declarations.
   - Stash declares one shared inventory mapped to `void_chest_inventory` behavior.
   - Inventory save/load must preserve enough item state to reconstruct stored items safely.

4. **Placement hooks**
   - Lobby hook can request a permanent access point near a known safe anchor such as the assist shrine area.
   - Shop hook can request one access point per generated shop room.
   - Engine decides the exact free tile/entity placement to avoid map corruption.

5. **Void Chest integration policy**
   - Engine can bind Void Chest access to a persistent named inventory.
   - Permanent access points and spell-created Void Chests use the same stash inventory.
   - Existing chest interaction and network behavior should be reused where possible.

6. **Version and multiplayer metadata**
   - Host advertises active framework mods and versions.
   - Client mismatch is rejected or clearly marked incompatible before gameplay state diverges.
   - Save files/storage records include enough metadata to detect incompatible state.

## Non-goals

The MVP explicitly excludes:

- dynamic native DLL/SO plugin loading;
- Lua runtime integration;
- WASM runtime integration;
- arbitrary entity behavior scripting;
- binary patching retail executables;
- broad engine event subscriptions;
- a full public SDK before Stash works;
- replacing Barony's existing Custom Content, Workshop, JSON, map, or Barony Script workflows.

Future versions can revisit broader extension points only after the Stash path proves that narrow manifest-driven hooks are useful and maintainable.

## Prior art to borrow from

Use existing clean modding frameworks as design pressure, not as scope expansion.

- **SMAPI:** Borrow mandatory manifests, stable unique ids, semantic versions, minimum game/API versions, dependency declarations, and clear load rejection. Do not borrow DLL entrypoints, Harmony patching, or a general native/plugin runtime.
- **Fabric:** Borrow explicit metadata such as mod id, version, dependencies, environment/side compatibility, and structured resources. Do not borrow entrypoints, mixins, or bytecode patching concepts.
- **Factorio:** Borrow explicit lifecycle phases, dependency-aware loading, versioned migrations, and multiplayer/desync discipline. Do not borrow a Lua runtime or broad event scripting for the Barony MVP.

Framework rule: if a prior-art pattern can be represented as manifest metadata plus engine-owned validation, it is in scope. If it requires arbitrary mod code execution, it is out of scope for v1.


## Required engine hooks

The first implementation should look for narrow insertion points in the C++ engine rather than a general plugin layer.

### Manifest loading

- Load framework manifests from a predictable content path, for example a `modframework` data folder.
- Validate ids, versions, declared capabilities, and target Barony version/source revision.
- Build a small runtime registry of approved mod capabilities.

### Mod storage

- Resolve a profile-safe storage path per mod id.
- Provide read/write helpers for versioned framework state.
- Avoid direct arbitrary file access from mod declarations.

### Named inventory persistence

- Add engine support for declared persistent named inventories.
- For Stash, persist the inventory associated with `void_chest_inventory` across runs.
- Save on safe boundaries such as chest close and normal save/update points so death/victory/new-run transitions do not lose stash contents.
- Treat malformed or incompatible inventory data as a recoverable mod-state error, not an engine crash.

### Void Chest access binding

- Reuse existing Void Chest routing where possible.
- Ensure every Void Chest access path resolves to the same persistent Stash inventory when the Stash manifest is active.
- Keep host/server authority for multiplayer stash state.

### Lobby placement

- Add a placement hook around lobby setup.
- For Stash, request one permanent Void Chest access point near the assist shrine area.
- Engine chooses a valid free tile and creates the chest/lid/entities consistently.

### Shop placement

- Add a placement hook after generated shop rooms are known.
- For Stash, request one permanent Void Chest access point in every shop.
- Engine should prefer a safe interior/counter-adjacent tile and fall back to any valid shop-area tile.

### Compatibility reporting

- Include active framework mods in save metadata and multiplayer session metadata.
- Reject incompatible framework state before item data or multiplayer state can be corrupted.

## Stash reference flow

1. Player starts Barony with the Stash framework manifest active.
2. Engine validates the manifest and confirms required capabilities are available.
3. Engine opens the Stash mod storage namespace and loads the persistent `void_chest_inventory` state if present.
4. Lobby generation/setup runs.
5. Stash lobby placement request creates one permanent Void Chest access point near the assist shrine area.
6. Dungeon/shop generation runs.
7. Stash shop placement request creates one permanent Void Chest access point in each generated shop.
8. Player opens any Stash Void Chest access point.
9. Engine routes the chest inventory to the shared persistent stash inventory.
10. Spell-created Void Chests use the same persistent inventory, not a separate temporary stash.
11. On chest close and normal save/update boundaries, engine writes the named inventory back to Stash storage.
12. In multiplayer, the host/server owns the persistent stash state and clients use the existing interaction/network flow where possible.

## Packaging/release model

The clean release model is a small engine patch package, not an opaque source fork.

Recommended artifacts:

- versioned patch series against upstream Barony;
- release notes identifying the upstream commit/tag targeted;
- optional reproducible Linux and Windows builds if the build process is controlled;
- optional Workshop/local content package for Stash metadata, preview assets, and install instructions;
- checksums for binary artifacts;
- rollback instructions for users applying a patched build.

The manifest/config package should remain separate from the engine patch. Users should be able to see which part is engine support and which part is the Stash mod declaration/assets.

## Upstream strategy

Primary upstream: <https://github.com/TurningWheel/Barony>

Working fork: <https://github.com/JMLegere/Barony>

The implementation should be easy to review upstream:

- keep hooks narrow and named by capability, not by Stash-only special cases;
- keep Stash as the reference mod/test case for persistent named inventories and placement hooks;
- preserve existing Custom Content, Workshop, JSON, map, and Barony Script behavior;
- avoid dynamic native plugins, Lua, WASM, or arbitrary scripting in the MVP;
- document all touched engine files and why each hook exists;
- prefer data declarations plus engine-owned validation over mod-owned code execution.

Fallback if upstream is not ready:

- maintain a small patch series against upstream releases;
- keep patches isolated so rebasing is predictable;
- avoid turning the user fork into the only explanation of the feature;
- keep the framework shape reusable for future mods.

## Next implementation steps

1. Confirm the exact upstream Barony revision to target in `JMLegere/Barony`.
2. Identify the smallest C++ insertion points for:
   - manifest loading;
   - mod storage path resolution;
   - named inventory serialization;
   - Void Chest inventory binding;
   - lobby placement;
   - shop placement;
   - multiplayer/version metadata.
3. Draft the first manifest schema around Stash only, with fields for id, version, capabilities, persistent inventories, placement requests, and compatibility metadata.
4. Implement engine-side manifest validation with unsupported capability rejection.
5. Implement persistent named inventory storage for the Stash `void_chest_inventory` case.
6. Add permanent Void Chest placement through lobby and shop hooks.
7. Verify the reference flow in single-player:
   - add item to Stash;
   - leave/end run;
   - start a new run;
   - confirm item remains accessible from lobby/shop/spell-created Void Chest access.
8. Verify multiplayer compatibility behavior:
   - host with Stash active;
   - client with matching Stash metadata;
   - client with missing/mismatched Stash metadata.
9. Package a versioned patch series and optional Stash metadata package.
10. Prepare an upstream proposal that describes the framework as a minimal modding extension proven by Stash.
