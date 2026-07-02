# BaronyModLoader Package Format Draft

BaronyModLoader packages are clean, inspectable mod archives consumed by a standalone app and a small paired Barony engine runtime. The format is intentionally explicit: a player, host, pack maintainer, or upstream reviewer should be able to understand what a package changes without diffing an opaque game fork.

The package format is designed for a full standalone modding framework, even though the first implementation should expose only the canonical Stash v0 capabilities: `persistent_storage`, `persistent_inventory`, `void_chest_binding`, `placement_lobby`, `placement_shop`, and `multiplayer_version_metadata`.

## Goals

- Keep every mod install reproducible from a declared package, not from hidden manual file edits.
- Separate user-facing content, engine capability requests, native patch/build requirements, and runtime state.
- Let the standalone app validate dependencies, conflicts, checksums, profile compatibility, and launch readiness before Barony starts.
- Let the engine runtime reject unsupported capabilities with clear errors before gameplay can corrupt saves.
- Make source and build provenance visible so native framework work can be reviewed and upstreamed instead of becoming a long-lived opaque fork.

## Non-goals for the first implementation

- Arbitrary DLL/SO injection.
- Lua, WASM, or a general native plugin ABI.
- Binary patching a retail executable in-place.
- A generic event bus for every engine subsystem.
- Implementing modules that Stash does not need yet.

Those can be revisited only after a concrete mod requires them and the app/runtime contract can keep them safe.

## Package artifact

Recommended file extension: `.bmlpkg`.

A `.bmlpkg` is a ZIP-compatible archive with deterministic paths and UTF-8 JSON manifests. Local development may also use an unpacked directory with the same layout.

```text
Stash-0.1.0.bmlpkg
├── bml-package.json              # required package manifest
├── content/                      # optional official Barony content assets
├── assets/                       # optional app/storefront/readme/media assets
├── native/                       # optional patch/build descriptors, not loaded as plugins
│   ├── patches/
│   ├── build/
│   └── release/
├── migrations/                   # optional data/state migration descriptors
├── checksums.json                # required for release packages
└── signatures/                   # optional maintainer signatures
```

The app must not infer behavior from arbitrary files in the archive. Behavior comes from `bml-package.json`; `checksums.json` proves the bytes that were installed.

## Archive and install workflow

The `.bmlpkg` archive is the distributable package artifact. It should be produced from an unpacked package directory, then installed into a package store before a profile enables it. For Stash local development the unpacked package directory is `mods/stash/`, with `mods/stash/bml-package.json` as the source manifest.

Expected local flow:

```sh
python framework/BaronyModLoader/app/barony_mod_loader.py package validate mods/stash
python framework/BaronyModLoader/app/barony_mod_loader.py package pack mods/stash --out .tmp/Stash-0.1.0.bmlpkg
python framework/BaronyModLoader/app/barony_mod_loader.py package install .tmp/Stash-0.1.0.bmlpkg --store .tmp/bml-package-store
python framework/BaronyModLoader/app/barony_mod_loader.py profile create .tmp/bml-profile --id default --barony-executable /path/to/barony --runtime-info framework/BaronyModLoader/fixtures/runtime-info.stash.json
python framework/BaronyModLoader/app/barony_mod_loader.py profile enable .tmp/bml-profile --package .tmp/bml-package-store/jml.stash/0.1.0
python framework/BaronyModLoader/app/barony_mod_loader.py profile inspect .tmp/bml-profile
python framework/BaronyModLoader/app/barony_mod_loader.py launch-plan .tmp/bml-profile --package .tmp/bml-package-store/jml.stash/0.1.0 --runtime-info framework/BaronyModLoader/fixtures/runtime-info.stash.json --out .tmp/bml-profile/BaronyModLoader/runtime-manifest.json
python framework/BaronyModLoader/app/barony_mod_loader.py profile disable .tmp/bml-profile --mod-id jml.stash
```

Install rules:

- `package pack` reads the package directory and writes a ZIP-compatible `.bmlpkg`; the archive must preserve normalized relative paths and must not include profile-local state.
- `package install` accepts a package directory, a direct `bml-package.json`, or a `.bmlpkg` archive and installs immutable package bytes under `<store>/<package-id>/<version>/`.
- `profile enable` records a profile activation that points at the installed package path; it must not mutate the archive or the unpacked source package.
- `profile disable` removes the package from the active profile set without deleting unrelated Barony content, unrelated packages, or profile-scoped Stash storage.
- `profile inspect` is the human-readable support surface for active package ids, package paths, selected runtime metadata, and launch readiness.
- `launch-plan` should consume the installed package path so the runtime manifest is tied to the same package bytes that validation and profile activation saw.

This workflow proves package management behavior only. It does not assert that a patched Barony executable has run Stash gameplay scenarios until built-game verification evidence exists.

## Manifest identity

`bml-package.json` starts with stable identity metadata.

```json
{
  "formatVersion": "0.1.0",
  "id": "jml.stash",
  "name": "Stash",
  "version": "0.1.0",
  "kind": "gameplay-mod",
  "summary": "Profile-persistent shared Void Chest inventory.",
  "license": "MIT",
  "authors": [
    { "name": "JMLegere" }
  ]
}
```

Identity rules:

- `id` is globally stable and should use reverse-DNS or similarly namespaced lowercase form.
- `version` should be SemVer for releases.
- `formatVersion` is the package schema version, not the mod version.
- Package identity is immutable within an installed profile. Renames require a migration entry.
- User-facing names may change; ids must not.

## Declared layout

The manifest lists every meaningful file class so validation does not rely on directory guessing.

```json
{
  "layout": {
    "contentRoot": "content/",
    "assetRoot": "assets/",
    "nativeRoot": "native/",
    "migrationRoot": "migrations/"
  }
}
```

### Content and assets

Use existing Barony content systems wherever possible. The package format should wrap and validate official content rather than replacing it.

```json
{
  "content": {
    "officialSystems": ["Custom Content", "Steam Workshop", "maps", "JSON", "Barony Script"],
    "entries": [
      {
        "id": "example.texture_pack",
        "type": "custom-content",
        "path": "content/example/",
        "mount": "custom_content",
        "required": false
      }
    ]
  },
  "assets": {
    "icon": "assets/icon.png",
    "previewImages": ["assets/preview-1.png"],
    "readme": "assets/README.txt"
  }
}
```

Content entries are declarative mounts. They do not grant native runtime capabilities. A package can contain only metadata and no content files if it depends entirely on engine-owned framework behavior, as Stash initially does.

## Engine capabilities

Capabilities are the safe vocabulary shared by the app, package, and engine runtime. A package requests capabilities; the app checks whether the selected Barony runtime can provide them; the engine runtime confirms what actually loaded.

```json
{
  "engine": {
    "runtimeContract": "bml-runtime-contract@0.1.0",
    "minimumRuntimeVersion": "0.1.0",
    "supportedGameVersions": ["4.x"],
    "capabilities": [
      {
        "id": "persistent_storage",
        "version": "0.1.0",
        "required": true,
        "reason": "Store profile-scoped mod state outside a single dungeon run."
      }
    ]
  }
}
```

Capability ids should be narrow and owned by the engine runtime. For the Stash-first v0 phase, the canonical ids are:

- `persistent_storage`: namespaced engine-managed storage under the selected profile/mod root.
- `persistent_inventory`: named inventories that can survive run/save boundaries.
- `void_chest_binding`: binding points for Barony's existing Void Chest inventory behavior.
- `placement_lobby`: engine-owned placement request for the lobby access point.
- `placement_shop`: engine-owned placement request for generated shop access points.
- `multiplayer_version_metadata`: manifest/runtime metadata needed for host/client compatibility checks.

Future packages may declare additional capabilities, but the engine must reject unknown required capabilities. Optional capabilities may be skipped only if the package declares a fallback that preserves save safety.

## Modules

Modules are package-level use of engine capabilities. Capabilities describe what the runtime can do; modules describe what this mod wants done.

```json
{
  "modules": {
    "persistentStorage": {
      "namespace": "stash",
      "scope": "profile-and-save",
      "savePolicy": "on_inventory_close"
    },
    "persistentInventories": [
      {
        "id": "void_chest_inventory",
        "storageKey": "void_chest_inventory",
        "scope": "profile-and-save",
        "authority": "host"
      }
    ],
    "voidChestBindings": [
      {
        "inventory": "void_chest_inventory",
        "sources": ["engine_void_chest", "spell_created_void_chest", "framework_placed_void_chest"]
      }
    ],
    "placements": [
      {
        "id": "lobby_void_chest_access",
        "hook": "lobby_assist_area",
        "entity": "void_chest_access",
        "inventory": "void_chest_inventory",
        "frequency": "once"
      }
    ],
    "multiplayer": {
      "policy": "host_authoritative",
      "versionPolicy": "same-package-and-runtime-required"
    }
  }
}
```

Module descriptors must be data-only. They are interpreted by engine-owned code paths; they are not script entrypoints.

## Native requirements

Native requirements describe the paired engine runtime needed to support declared capabilities. They are not dynamic plugins, and they must resolve to a source patch or reproducible build path that a maintainer can inspect.

```json
{
  "native": {
    "required": true,
    "mode": "paired-engine-runtime",
    "patches": [
      {
        "id": "bml-runtime-handshake",
        "appliesTo": "TurningWheel/Barony",
        "baseRevision": "upstream-or-release-git-sha",
        "path": "native/patches/0001-bml-runtime-handshake.patch",
        "sourceMap": "native/source-map.toml",
        "required": true,
        "providesCapabilities": [
          "persistent_storage",
          "persistent_inventory",
          "void_chest_binding",
          "placement_lobby",
          "placement_shop",
          "multiplayer_version_metadata"
        ]
      }
    ],
    "builds": [
      {
        "platform": "linux-x86_64",
        "baronyRevision": "git-sha",
        "runtimeVersion": "0.1.0",
        "reproducible": true,
        "buildRecipe": "native/build/build.json",
        "artifact": "native/release/linux-x86_64/barony"
      }
    ]
  }
}
```

Rules:

- Native code must be represented as source patches and/or reproducible build metadata; a package must not rely on an unexplained binary fork.
- The patch descriptor's `providesCapabilities` list must use only canonical engine capability ids. For the Stash v0 surface those ids are `persistent_storage`, `persistent_inventory`, `void_chest_binding`, `placement_lobby`, `placement_shop`, and `multiplayer_version_metadata`.
- A package may point to a reviewed runtime patch supplied by the framework instead of embedding a custom patch.
- The app should prefer known framework runtime builds over per-mod forks.
- If a mod needs a new engine hook, the package should declare the missing capability and fail cleanly until the framework/runtime supports it.

## Dependencies and conflicts

Packages can depend on other packages, the Barony runtime, app versions, and capabilities. Conflicts are explicit so the app can explain why a profile cannot launch.

```json
{
  "dependencies": [
    {
      "id": "barony",
      "kind": "game",
      "version": ">=4.0.0",
      "required": true
    },
    {
      "id": "bml.runtime",
      "kind": "engine-runtime",
      "version": ">=0.1.0 <0.2.0",
      "required": true
    }
  ],
  "conflicts": [
    {
      "id": "example.other_void_chest_overhaul",
      "kind": "package",
      "reason": "Both packages claim authoritative Void Chest inventory binding."
    }
  ],
  "loadAfter": [],
  "loadBefore": []
}
```

Dependency semantics:

- Required dependencies block activation if missing.
- Optional dependencies must declare what changes when absent.
- Conflicts block activation unless the user disables one side or a compatibility package resolves the conflict.
- Runtime capability conflicts are stronger than file conflicts; two packages may not both own the same exclusive engine binding.

## Migrations

Migrations describe package data changes across versions. They are applied by the app and/or engine runtime before gameplay uses old state.

```json
{
  "migrations": [
    {
      "id": "stash-0.1.0-initial",
      "from": null,
      "to": "0.1.0",
      "phase": "pre-launch",
      "path": "migrations/0001-initial.json",
      "affects": ["profile-storage", "save-metadata"],
      "rollback": "not-required-for-initial-install"
    }
  ]
}
```

Migration phases:

- `pre-activation`: app-level package/profile metadata changes before enabling a mod.
- `pre-launch`: app verifies state and writes launch manifests before Barony starts.
- `runtime-load`: engine runtime upgrades mod-scoped game data during profile/save load.
- `runtime-save`: engine runtime writes a newer state format at safe save boundaries.

Migrations must be idempotent. The engine runtime should record completed migrations in mod-scoped metadata so repeated launches cannot reapply destructive transforms.

## Checksums and signatures

Release packages include `checksums.json`; development packages may use app-generated transient checksums.

```json
{
  "algorithm": "sha256",
  "files": [
    {
      "path": "bml-package.json",
      "sha256": "...",
      "size": 12345
    }
  ],
  "createdAt": "2026-07-02T00:00:00Z"
}
```

Checksum policy:

- Every installed file must be listed, except app-generated profile state.
- The app verifies checksums before activation and before launch.
- Native patches/build artifacts require checksums in release packages.
- A checksum mismatch disables the package for that profile until the user repairs or reinstalls it.
- Maintainer signatures are optional in early local development but should be included for distributable releases.

## Source and build metadata

Packages that require native runtime support must identify their source and build provenance.

```json
{
  "source": {
    "homepage": "https://github.com/JMLegere/Barony",
    "upstream": "https://github.com/TurningWheel/Barony",
    "repository": "https://github.com/JMLegere/Barony",
    "revision": "git-sha-or-tag",
    "patchSeries": "native/patches/series.json"
  },
  "build": {
    "reproducible": true,
    "system": "documented-native-build",
    "instructions": "native/build/build.json",
    "outputs": [
      {
        "platform": "linux-x86_64",
        "path": "native/release/linux-x86_64/barony",
        "sha256": "..."
      }
    ]
  }
}
```

This metadata is not decorative. It is the difference between a reviewable framework patch and an opaque fork. The app should surface it prominently whenever a package requires a non-stock Barony runtime.


## Release manifest

A distributable release should include or reference a release manifest that ties the app, package archive, runtime patch artifacts, source revision, and verification status together. The release manifest is separate from `bml-package.json`: the package manifest declares what the mod needs, while the release manifest records exactly what was packaged and what has been verified.

Example shape:

```json
{
  "manifestVersion": "0.1.0",
  "createdAt": "2026-07-02T00:00:00Z",
  "app": {
    "id": "barony-mod-loader",
    "version": "0.1.0"
  },
  "package": {
    "id": "jml.stash",
    "version": "0.1.0",
    "archive": "Stash-0.1.0.bmlpkg",
    "sha256": "package-archive-sha256",
    "installedPathExample": ".tmp/bml-package-store/jml.stash/0.1.0"
  },
  "runtime": {
    "id": "bml-runtime",
    "version": "0.1.0",
    "contract": "bml-runtime-contract@0.1.0",
    "baronySource": {
      "upstream": "https://github.com/TurningWheel/Barony",
      "revision": "barony-source-revision",
      "patchArtifacts": [
        "native/barony-modloader-runtime/patches/0001-bml-runtime-handshake.patch",
        "native/barony-modloader-runtime/patches/0002-bml-stash-runtime.patch"
      ]
    },
    "capabilities": [
      "persistent_storage",
      "persistent_inventory",
      "void_chest_binding",
      "placement_lobby",
      "placement_shop",
      "multiplayer_version_metadata"
    ]
  },
  "verification": {
    "packageArchiveInstallEnableDisable": "verified-by-cli-evidence",
    "runtimeManifestGeneration": "verified-by-cli-evidence",
    "builtGameStashBehavior": "pending-built-game-verification"
  },
  "rollback": {
    "disableCommand": "profile disable <profile-dir> --mod-id jml.stash",
    "notes": "Disabling removes Stash from profile activation without deleting unrelated Barony content or profile storage."
  }
}
```

Release manifest policy:

- It must record the Barony source revision or tag used by the runtime patch artifacts.
- It must record the BaronyModLoader app/runtime versions and the runtime contract version.
- It must record the package archive checksum and the installed package path shape used by profile activation.
- It must list only canonical Stash v0 capability ids: `persistent_storage`, `persistent_inventory`, `void_chest_binding`, `placement_lobby`, `placement_shop`, and `multiplayer_version_metadata`.
- It must distinguish package/archive/profile workflow verification from pending built-game runtime behavior verification.
- It must include rollback notes that do not require deleting unrelated Barony installs, content, or profiles.

## Profile activation record

The app should not mutate packages during activation. It writes a profile-specific activation record outside the package archive, for example:

```text
<profile>/BaronyModLoader/active-mods.json
<profile>/BaronyModLoader/runtime-manifest.json
<profile>/BaronyModLoader/logs/app-validation.log
```

The package remains immutable. The activation record captures selected versions, load order, resolved capabilities, checksum results, and the runtime executable selected for launch.

## Validation stages

1. Parse `bml-package.json` and `checksums.json`.
2. Verify package id/version/format compatibility.
3. Verify archive paths are normalized and remain inside the package root.
4. Verify checksums and optional signatures.
5. Resolve dependencies, conflicts, and load order.
6. Match requested engine capabilities to the selected runtime.
7. Verify native patch/build metadata if native support is required.
8. Verify migrations needed for the selected profile/save can be applied.
9. Write the activation record and runtime manifest.
10. Launch Barony only after the app has a complete, validated runtime plan.

## Error model

Validation errors should use stable codes and human-readable detail.

Examples:

- `BML_PACKAGE_PARSE_FAILED`
- `BML_CHECKSUM_MISMATCH`
- `BML_DEPENDENCY_MISSING`
- `BML_CONFLICT_EXCLUSIVE_BINDING`
- `BML_RUNTIME_CAPABILITY_MISSING`
- `BML_NATIVE_BUILD_UNVERIFIED`
- `BML_MIGRATION_REQUIRED_BUT_UNAVAILABLE`

Errors should include package id/version, profile id, requested capability, and the app/runtime version that made the decision.

## Stash fit

The Stash package should be small despite the full model:

- no custom content files required initially;
- no arbitrary scripts;
- no plugin binary;
- only the Stash-needed modules;
- explicit dependency on `bml.runtime` with the six required capabilities;
- clear source/build metadata pointing at the framework runtime patch;
- persistent storage and migration metadata for `void_chest_inventory`.

This keeps the first implementation focused while ensuring the package can live inside a real standalone BaronyModLoader ecosystem.
