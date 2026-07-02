# Prior art for the Barony Mod Framework MVP

This note captures existing modding-framework patterns worth borrowing. The goal is not to clone a large framework; it is to choose the smallest clean ideas that fit Barony and the Stash reference mod.

## Source baseline

Barony already has official content modding: Custom Content, Workshop/local mod folders, maps, JSON data files, assets, and map/editor scripting. Community tools help with downloading mods or editing saves. I did not find an existing Barony-native runtime plugin framework that can hook gameplay logic such as `void_chest_inventory` persistence.

## Frameworks reviewed

### SMAPI / Stardew Valley

Sources:
- https://stardewvalleywiki.com/Modding:Modder_Guide/APIs/Manifest
- https://stardewvalleywiki.com/Modding:Index

Useful ideas:
- Every mod/content pack has a manifest.
- Stable unique IDs are important because other mods, update checks, dependencies, and compatibility lists reference them.
- Separate code mods from content packs.
- Provide friendly compatibility checks: minimum game/API version, dependency checks, update keys.
- Provide framework-owned services: config, data storage, content loading, logging, multiplayer, translations.

What not to copy for Barony MVP:
- Do not require a full C#-style code plugin model.
- Do not start with Harmony/reflection-style patching.

Barony takeaway:
- Adopt a manifest-first model with stable IDs, version bounds, dependencies, and optional update/source metadata.
- Keep Stash as data/config on top of engine-owned hooks where possible.

### Factorio

Source:
- https://lua-api.factorio.com/latest/auxiliary/mod-structure.html

Useful ideas:
- A mod can be a folder or zip.
- `info.json` is mandatory and identifies name/version/title/author/game version/dependencies.
- Dependency syntax distinguishes hard, optional, recommended, and incompatible dependencies.
- Load lifecycle is explicit: settings, data/prototype phase, runtime/control phase.
- Migrations are first-class for mod data structure changes.

What not to copy for Barony MVP:
- Do not embed a general Lua runtime yet.
- Do not create multiple load phases until Barony actually needs them.

Barony takeaway:
- Use a simple load lifecycle vocabulary even if v1 only implements two phases: manifest/config load and runtime hook application.
- Add migration/versioning from day one for persistent mod storage such as Stash inventory data.

### Fabric Loader / Minecraft

Source:
- https://wiki.fabricmc.net/documentation:fabric_mod_json

Useful ideas:
- `fabric.mod.json` defines schema version, ID, version, metadata, environment, entrypoints, nested jars, dependencies, conflicts, and custom namespaced metadata.
- Dependency categories distinguish required, recommended, suggested, breaks, and conflicts.
- `custom` namespacing avoids collisions for future metadata.

What not to copy for Barony MVP:
- Do not start with Java-style entrypoints, mixins, or nested code packages.
- Do not create a patch/injection system as the first layer.

Barony takeaway:
- Manifest should support `requires`, `optional`, `conflicts`, and `breaks` early.
- Unknown/custom fields should be namespaced if supported.

### BepInEx

Source:
- https://docs.bepinex.dev/articles/dev_guide/plugin_tutorial/index.html

Useful ideas:
- Plugin systems need standard paths, logging, config, dependency management, and a predictable load mechanism.

What not to copy for Barony MVP:
- BepInEx primarily loads user code into games as compiled plugins. That is too broad and risky for this MVP.

Barony takeaway:
- Borrow the services idea: framework-owned logging, config paths, and per-mod data paths.
- Avoid arbitrary code loading in v1.

### tModLoader / Terraria

Source:
- https://github.com/tModLoader/tModLoader/wiki/Basic-tModLoader-Modding-Guide

Useful ideas:
- Tooling can generate a mod skeleton.
- A build/reload loop improves modder experience.
- Hooks are named extension points called by the game at appropriate times.
- Example mods are essential teaching tools.

What not to copy for Barony MVP:
- Do not start with a broad C# content-class system.
- Do not attempt full hot reload before the basic hooks are stable.

Barony takeaway:
- Use explicit hook names and document when they fire.
- Make Stash the example/reference mod.
- Later add a skeleton generator once the manifest shape stabilizes.

## Recommended design principles for Barony

1. Manifest first
   - Every framework mod has a stable ID, display name, version, Barony compatibility, dependencies/conflicts, and declared capabilities.

2. Engine-owned hooks, not arbitrary plugins
   - V1 should expose safe C++ engine hooks through data/config, not load arbitrary native code.

3. Small explicit hook catalog
   - Start with hooks Stash needs: persistent storage, persistent named inventories, lobby placement, shop placement, and multiplayer/version compatibility.

4. Data migrations from day one
   - Persistent inventories need versioned data and migration hooks/metadata.

5. Content packs before code mods
   - Treat Stash as a content/config package that uses engine capabilities. Add code-plugin support only if real future mods require it.

6. Friendly failure
   - If a mod requires an unsupported framework version or conflicts with another mod, Barony should fail with a readable message before starting a run.

7. Reference mod and tests
   - Stash is the reference mod. Framework acceptance should be measured by whether Stash can be expressed cleanly through the manifest and hooks.

## MVP shape inspired by prior art

```text
mods/<mod-id>/
  modframework.json
  data/
  images/
  lang/
```

`modframework.json` should include:

```json
{
  "schemaVersion": 1,
  "id": "stash",
  "name": "Stash",
  "version": "0.1.0",
  "barony": {
    "minimumVersion": "5.0.0"
  },
  "capabilities": [
    "persistent-storage",
    "persistent-inventory",
    "placement-hooks"
  ],
  "dependencies": [],
  "conflicts": [],
  "persistentInventories": [],
  "placements": [],
  "multiplayer": {}
}
```

## Stash-specific implication

Stash should not become a one-off hardcoded fork. The implementation can start hardcoded to prove behavior, but the target design is:

- framework owns persistent storage and hook timing;
- Stash manifest declares persistent `void_chest_inventory`;
- Stash manifest declares lobby/shop Void Chest placements;
- engine resolves placement and serialization safely;
- future mods reuse the same primitives.
