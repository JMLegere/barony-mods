# BaronyModLoader Package Format Draft

BaronyModLoader packages are clean, inspectable, platform-agnostic mod archives consumed by a standalone app and a small paired Barony engine runtime. The format is intentionally explicit: a player, host, pack maintainer, or upstream reviewer should be able to understand what abstract engine capabilities a package requests without diffing an opaque game fork.

The package format is designed for a full standalone modding framework, even though the first implementation should expose only the canonical Stash v0 capabilities: `persistent_storage`, `persistent_inventory`, `void_chest_binding`, `placement_lobby`, `placement_shop`, and `multiplayer_version_metadata`. Generic packages may request smaller capability subsets; the requirement that all six capabilities be present is specific to `jml.stash`.

## Goals

- Keep every mod install reproducible from a declared package, not from hidden manual file edits.
- Separate user-facing content, engine capability requests, framework runtime requirements, and runtime state.
- Let the standalone app validate dependencies, conflicts, checksums, profile compatibility, runtime registration compatibility, and launch readiness before Barony starts.
- Let the engine runtime reject unsupported capabilities with clear errors before gameplay can corrupt saves.
- Keep platform/store/build compatibility in the framework app and runtime registration layer, not in per-platform package manifests.
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
├── native/                       # optional semantic source references, not runtime artifacts
│   └── source-references/
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
python framework/BaronyModLoader/app/barony_mod_loader.py profile create .tmp/bml-profile --id default --steam
python framework/BaronyModLoader/app/barony_mod_loader.py profile enable .tmp/bml-profile --package .tmp/bml-package-store/jml.stash/0.1.0
python framework/BaronyModLoader/app/barony_mod_loader.py profile inspect .tmp/bml-profile
python framework/BaronyModLoader/app/barony_mod_loader.py launch-plan .tmp/bml-profile --package .tmp/bml-package-store/jml.stash/0.1.0 --runtime-info framework/BaronyModLoader/fixtures/runtime-info.installed-hook.stash.json --out .tmp/bml-profile/BaronyModLoader/runtime-manifest.json
python framework/BaronyModLoader/app/barony_mod_loader.py profile disable .tmp/bml-profile --mod-id jml.stash
```

Install rules:

- `package pack` reads the package directory and writes a ZIP-compatible `.bmlpkg`; the archive must preserve normalized relative paths and must not include profile-local state.
- `package install` accepts a package directory, a direct `bml-package.json`, or a `.bmlpkg` archive and installs immutable package bytes under `<store>/<package-id>/<version>/`.
- `profile enable` records a profile activation that points at the installed package path; it must not mutate the archive or the unpacked source package.
- `profile disable` removes the package from the active profile set without deleting unrelated Barony content, unrelated packages, or profile-scoped Stash storage.
- `profile inspect` is the human-readable support surface for active package ids, package paths, selected runtime metadata, and launch readiness.
- `launch-plan` should consume the installed package path so the runtime manifest is tied to the same package bytes that validation and profile activation saw.

This workflow proves package management behavior only. It does not assert that the installed executable plus BML hook runtime has run Stash gameplay scenarios until installed-game verification evidence exists.

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

Capabilities are the safe vocabulary shared by the app, package, and BML hook/runtime. A package requests abstract capabilities; the app checks whether the selected registered runtime for the user's platform/store/build can provide them; the hook/runtime confirms what actually loaded.

```json
{
  "engine": {
    "runtimeContract": "bml-runtime-contract@0.1.0",
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

The reserved `runtime_load_smoke` capability is only for runtime-load smoke diagnostic packages. It validates framework plumbing and is not a gameplay capability; `jml.windows_smoke` is the reference fixture. A Windows runtime registered with `windowsSupportLevel: noop-runtime-load` must accept only that smoke-style capability set and reject packages that request non-smoke capabilities with `BML_RUNTIME_NOOP_PACKAGE_UNSUPPORTED`.

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

## Framework runtime requirements

Some capability sets require BML-owned paired engine runtime support. A package manifest may state that such framework support is required, but it does not choose Windows, macOS, Linux, Steam, GOG, Epic, Humble, installed-binary-hook, or a specific Barony build. That compatibility is resolved by the app against registered runtime metadata at activation and launch.

Runtime requirements are not dynamic plugins, and a Stash package must not ship arbitrary native code into the game process.

```json
{
  "native": {
    "required": true,
    "mode": "paired-engine-runtime",
    "plugins": [],
    "runtimeRequirements": {
      "owner": "BaronyModLoader",
      "runtimeContract": "bml-runtime-contract@0.1.0",
      "requiresBmlRuntime": true,
      "mutatesExecutableOnDisk": false
    },
    "sourceReferences": [
      {
        "id": "stash-source-map",
        "path": "native/barony-modloader-runtime/stash-source-map.toml",
        "runtimeAuthority": false
      }
    ]
  }
}
```

Rules:

- Native gameplay behavior must be implemented by reviewed BML-owned hook/runtime code, not by arbitrary per-mod native plugins.
- The installed game executable must not be modified on disk.
- Runtime support must fail closed unless the app can match the selected installed executable, hook library, hook manifest, runtime info, and symbol map to a registered supported PC build.
- Source patch artifacts may be retained as semantic references, but they are not runtime strategies and must not be used to claim Steam-current or storefront-current compatibility.
- The package descriptor's capability list must use only canonical engine capability ids. For the Stash v0 surface those ids are `persistent_storage`, `persistent_inventory`, `void_chest_binding`, `placement_lobby`, `placement_shop`, and `multiplayer_version_metadata`; other packages may declare smaller valid subsets.
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
- Native hook libraries, hook manifests, runtime-info files, and any semantic source reference artifacts require checksums in release manifests.
- A checksum mismatch disables the package for that profile until the user repairs or reinstalls it.
- Maintainer signatures are optional in early local development but should be included for distributable releases.

## Runtime registration and provenance metadata

Runtime registrations and release manifests, not platform-agnostic `bml-package.json` manifests, identify the installed game target and the BML-owned hook/bootstrap artifacts used for launch. Source patch files may still be referenced as semantic design artifacts, but they are not the v1 runtime authority unless a future release explicitly selects and verifies a source-build strategy.

```json
{
  "runtimeStrategy": "installed-binary-hook",
  "storefront": "steam",
  "steam": {
    "appId": "371970",
    "buildId": "22630456"
  },
  "platform": "linux-x86_64",
  "gameVersion": "v5.0.2",
  "installedExecutable": {
    "pathHint": "/path/to/Steam/steamapps/common/Barony/barony.x86_64",
    "sha256": "installed-executable-sha256",
    "buildId": "installed-executable-elf-build-id",
    "status": "verified_local_target"
  },
  "hookLibrary": {
    "path": "native/barony-modloader-hook/build/libbarony_bml.so",
    "checksum": {
      "algorithm": "sha256",
      "sha256": "hook-library-sha256",
      "size": 12345
    },
    "status": "built_available_verified"
  },
  "hookManifest": {
    "path": "native/barony-modloader-hook/manifests/steam-371970-22630456-linux.json",
    "checksum": {
      "algorithm": "sha256",
      "sha256": "hook-manifest-sha256",
      "size": 12345
    },
    "status": "present"
  }
}
```

This metadata is not decorative. It is the difference between a reviewable installed-executable hook release and an opaque fork. The app should surface it prominently whenever it resolves a package's abstract capability requests to a platform/store/build-specific runtime.


## Release manifest

A distributable release should include or reference a release manifest that ties the app, package archive, selected runtime registration/provenance, hook/bootstrap artifacts, semantic source references, and verification status together. The release manifest is separate from `bml-package.json`: the package manifest declares what the mod needs, while the release manifest records exactly what was packaged, which runtime target was selected, and what has been verified.

Example shape:

```json
{
  "schemaVersion": "0.1.0",
  "manifestId": "jml.stash.release.0.1.0",
  "createdAt": "2026-07-02T00:00:00Z",
  "app": {
    "id": "BaronyModLoader",
    "version": "0.1.0"
  },
  "package": {
    "id": "jml.stash",
    "version": "0.1.0",
    "manifestPath": "mods/stash/bml-package.json",
    "checksum": {
      "algorithm": "sha256",
      "sha256": "package-manifest-sha256",
      "size": 12345
    },
    "archive": {
      "path": "framework/BaronyModLoader/fixtures/Stash-0.1.0.bmlpkg",
      "checksum": {
        "algorithm": "sha256",
        "sha256": "package-archive-sha256",
        "size": 67890
      },
      "status": "packed_fixture"
    },
    "installedPathShape": "<package-store>/jml.stash/0.1.0"
  },
  "runtime": {
    "contract": {
      "id": "bml-runtime-contract",
      "version": "0.1.0"
    },
    "runtimeVersion": "0.1.0",
    "requiredCapabilities": [
      "persistent_storage",
      "persistent_inventory",
      "void_chest_binding",
      "placement_lobby",
      "placement_shop",
      "multiplayer_version_metadata"
    ],
    "status": "installed_binary_hook_contract_verified"
  },
  "runtimeProvenance": {
    "runtimeStrategy": "installed-binary-hook",
    "storefront": "steam",
    "steam": {
      "appId": "371970",
      "buildId": "22630456"
    },
    "platform": "linux-x86_64",
    "gameVersion": "v5.0.2",
    "installedExecutable": {
      "pathHint": "/path/to/Steam/steamapps/common/Barony/barony.x86_64",
      "sha256": "installed-executable-sha256",
      "buildId": "installed-executable-build-id",
      "status": "verified_local_target"
    },
    "hookLibrary": {
      "path": "native/barony-modloader-hook/build/libbarony_bml.so",
      "checksum": {
        "algorithm": "sha256",
        "sha256": "hook-library-sha256",
        "size": 12345
      },
      "status": "built_available_verified"
    },
    "hookManifest": {
      "path": "native/barony-modloader-hook/manifests/steam-371970-22630456-linux.json",
      "checksum": {
        "algorithm": "sha256",
        "sha256": "hook-manifest-sha256",
        "size": 12345
      },
      "status": "present"
    },
    "runtimeInfo": {
      "path": "framework/BaronyModLoader/fixtures/runtime-info.installed-hook.stash.json",
      "checksum": {
        "algorithm": "sha256",
        "sha256": "runtime-info-sha256",
        "size": 12345
      },
      "status": "present"
    },
    "status": "provenance_verified"
  },
  "verification": {
    "packageArchiveInstallEnableDisable": "verified-by-cli-evidence",
    "runtimeManifestGeneration": "verified-by-cli-evidence",
    "installedHookStashBehavior": "verified-steam-linux-production-playable-bundle"
  },
  "rollback": {
    "disableCommand": "profile disable <profile-dir> --mod-id jml.stash",
    "notes": "Disabling removes Stash from profile activation without deleting unrelated Barony content or profile storage."
  }
}
```

Release manifest policy:

- It must record the installed PC executable target and BML hook/bootstrap artifacts used by the release.
- It must record the BaronyModLoader app/runtime versions and the runtime contract version.
- It must record the package archive checksum and the installed package path shape used by profile activation.
- For a `jml.stash` release, it must list only canonical Stash v0 capability ids: `persistent_storage`, `persistent_inventory`, `void_chest_binding`, `placement_lobby`, `placement_shop`, and `multiplayer_version_metadata`.
- It must distinguish package/archive/profile workflow verification, diagnostic runtime smokes such as `jml.windows_smoke` or another runtime-load smoke package requesting only `runtime_load_smoke`, and platform-specific installed-game Stash runtime verification such as the validated Steam/Linux production playable bundle.
- It must include rollback notes that do not require deleting unrelated Barony installs, content, or profiles.

## Profile activation record

The app should not mutate packages during activation. It writes a profile-specific activation record outside the package archive, for example:

```text
<profile>/BaronyModLoader/active-mods.json
<profile>/BaronyModLoader/runtime-manifest.json
<profile>/BaronyModLoader/logs/app-validation.log
```

The package remains immutable. The activation record captures selected versions, load order, resolved capabilities, checksum results, and the installed-executable hook runtime selected for launch.

## Validation stages

1. Parse `bml-package.json` and `checksums.json`.
2. Verify package id/version/format compatibility.
3. Verify archive paths are normalized and remain inside the package root.
4. Verify checksums and optional signatures.
5. Resolve dependencies, conflicts, and load order.
6. Match requested abstract engine capabilities to the selected registered runtime.
7. Verify selected runtime registration/provenance metadata if native framework support is required.
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
- clear installed-executable hook/runtime metadata pointing at BML-owned hook support;
- persistent storage and migration metadata for `void_chest_inventory`.

This keeps the first implementation focused while ensuring the package can live inside a real standalone BaronyModLoader ecosystem.
