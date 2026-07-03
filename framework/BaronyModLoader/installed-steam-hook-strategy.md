# Installed PC executable hook strategy

## Decision

BaronyModLoader v1 should target installed PC copies of Barony through a BML-owned hook/bootstrap runtime. Source-built Barony runtimes are out of scope for the main v1 plan; old source patches may remain as semantic references, but not as a supported runtime strategy.

Jeremy's current Steam install reports:

```text
Steam app id: 371970
Steam build id: 22630456
Installed executable: /home/jerry/.local/share/Steam/steamapps/common/Barony/barony.x86_64
Detected game version string: v5.0.2
```

## PC storefront and platform target

The product target is the PC storefront/platform matrix Jeremy identified from Barony's purchase page:

| Storefront | PC platforms in scope |
| --- | --- |
| Steam | Windows, macOS, Linux |
| Epic Games Store | Windows, macOS |
| GOG / DRM-free | Windows, macOS, Linux |
| Humble Bundle | Windows, macOS, Linux |

Nintendo Switch is intentionally out of scope for this native PC mod-loader plan.

Steam/Linux is the first concrete verification target because that is the installed executable currently available on this workstation. The app/runtime contracts should be written so additional PC storefronts and operating systems can be added without reintroducing a source-build runtime path.

The deleted prototype source checkout was:

```text
origin: https://github.com/TurningWheel/Barony
commit: 962a5ce36d10207beef7d8673876e0cebf8e76e4
```

That checkout is not proven to match Steam build `22630456`, and should not be used as the basis for claiming Steam-current compatibility.

## Why the source-built sidecar is not v1-clean

A sidecar runtime built from stale public source can load the Steam asset directory and still be gameplay-incompatible with the installed Steam game. It can diverge in:

- save format and save metadata;
- entity ids, sprite ids, item ids, map markers, and script hooks;
- map/data parsing behavior;
- multiplayer and lobby protocol behavior;
- bugfixes and balance logic;
- DLC/content expectations;
- Steam/runtime integrations.

Therefore, BML should not equate "runs with Steam assets" with "works with the Steam version."

## Target v1 runtime model

The v1 Steam-current model should be:

```text
BaronyModLoader app
  detects Steam install/build
  validates package/profile
  prepares BML profile state + manifest
  launches the installed Steam executable through a controlled hook/bootstrap path

Installed Steam Barony executable
  remains the base runtime
  is not overwritten in-place
  receives BML bootstrap/hook library at process start

BML native hook/bootstrap library
  loads in the Steam process
  verifies executable identity/build/provenance
  discovers safe hook points for the active build
  installs only the narrow hooks needed by Stash v1
  writes runtime-load-report and diagnostics

Stash package
  remains declarative from the app perspective
  activates only when hook/runtime capabilities are verified
```

## Linux implementation and current status

The first Linux proof was dynamic loader injection with a no-op process. That `/usr/bin/true` smoke is historical first-stage evidence only: it proved the hook library could load and write `BaronyModLoader/reports/runtime-load-report.json`, but it is no longer the current gameplay-status boundary.

```text
LD_PRELOAD=native/barony-modloader-hook/build/libbarony_bml.so \
BML_RUNTIME_MANIFEST=<profile>/BaronyModLoader/runtime-manifest.json \
BML_PROFILE_DIR=<profile> \
SteamAppId=371970 \
SteamGameId=371970 \
/usr/bin/true
```

Current Steam/Linux status for build `22630456` uses the installed executable at `/home/jerry/.local/share/Steam/steamapps/common/Barony/barony.x86_64` with the same BML hook environment plus `BML_HOOK_MANIFEST` and `BML_HOOK_LIBRARY`. Current live evidence resolves 30/30 required symbols, loads a validated `jml.stash` runtime manifest, and installs the production Stash bundle by default without `BML_STASH_ENABLE_EXPERIMENTAL_PLAYABLE`.

The verified player-facing live boundary is the Start Map lobby access point: the live quickstart evidence records the placed chest/lid and Jeremy confirmed the in-game `Open stash` prompt. Fake-provider evidence covers generated-shop placement logic, shared inventory/spell binding, multiplayer metadata/client guard reporting, scoped prompt replacement, and production hook install reports. Live gameplay still needs separate verification for real generated-shop placement, cross-run persistence, player-cast spell-created Void Chests, save/resume, disabled behavior, and multiplayer mismatch rejection.

## Hook-loader architecture

```text
framework/BaronyModLoader/app/
  detects Steam install/build
  validates package/profile
  selects runtime strategy: installed-binary-hook
  writes manifest and launch report paths
  launches stock Steam executable with BML hook environment

native/barony-modloader-hook/
  src/bootstrap.cpp              # library constructor, manifest path discovery, report setup
  src/provenance.cpp             # executable/build/version/hash checks
  src/symbols.cpp                # symbol/signature resolution for supported Steam builds
  src/hooks.cpp                  # hook installation/removal primitives
  src/stash_inventory.cpp        # persistent inventory binding hooks
  src/stash_placement.cpp        # lobby/shop placement hooks
  src/stash_reports.cpp          # runtime-load-report + diagnostics
  manifests/steam-371970-*.json  # build-specific symbol/signature maps
```

## Guardrails

The app must refuse to describe a runtime as Steam-compatible unless it records:

```json
{
  "runtimeStrategy": "installed-binary-hook",
  "steamAppId": "371970",
  "steamBuildId": "22630456",
  "steamExecutable": "/home/jerry/.local/share/Steam/steamapps/common/Barony/barony.x86_64",
  "steamExecutableSha256": "...",
  "gameVersionString": "v5.0.2",
  "hookLibrary": "native/barony-modloader-hook/build/libbarony_bml.so",
  "hookLibrarySha256": "...",
  "hookManifest": "/path/to/steam-371970-22630456.json",
  "capabilities": [
    "persistent_storage",
    "persistent_inventory",
    "void_chest_binding",
    "placement_lobby",
    "placement_shop",
    "multiplayer_version_metadata"
  ]
}
```

Source-built runtimes are not a v1 runtime strategy. If the old source patches are retained, they should be labeled as:

```json
{
  "runtimeStrategy": "semantic-source-reference",
  "sourceOrigin": "...",
  "sourceCommit": "...",
  "steamCurrentRuntime": false
}
```

The launcher must not treat source-derived artifacts as supported runtime entries.

## MVP spike sequence

1. **Stop relying on `/tmp/barony-src` source builds.** Completed locally: the prototype source/build directories were deleted.
2. **Add app-side installed-executable runtime metadata and guardrails.** The runtime registry should model BML-owned hook/bootstrap artifacts for supported PC storefront/build/platform combinations.
3. **Build a no-op preload hook library.** First proof: the installed Steam executable starts with `libbarony_bml.so` loaded and writes a BML runtime-load-report without gameplay hooks.
4. **Add executable provenance checks.** Hash and version-string checks should fail closed on unsupported Steam builds.
5. **Add one harmless hook/symbol probe.** Verify the hook runtime can locate a stable symbol or signature in build `22630456` without changing gameplay.
6. **Port Stash one hook at a time.** Start with diagnostics-only lobby/shop observations, then placement, then inventory binding/persistence.

## Non-goals for the hook MVP

- Do not overwrite the Steam executable.
- Do not patch bytes on disk.
- Do not load arbitrary third-party native mods into the game process in v1.
- Do not pretend stale source rebuilds are Steam-compatible.
- Do not continue Stash gameplay verification unless the installed executable, hook library, hook manifest, and symbol map all match a supported PC build.
