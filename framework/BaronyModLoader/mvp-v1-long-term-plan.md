# BaronyModLoader MVP v1.0 Plan

## Decision

Build the long-term architecture now, but only implement the runtime modules needed for the Stash reference mod.

BaronyModLoader v1.0 should be:

- a standalone PC app/launcher;
- a profile and package manager for installed PC copies of Barony;
- a strategy-aware registry for BML-owned hook/bootstrap runtimes;
- an installed-executable hook path for Steam/Epic/GOG/Humble desktop builds, with Steam/Linux as the first verified target;
- a schema-first package/runtime contract;
- a working Stash mod proving persistent Void Chest storage, lobby/shop access, and compatibility diagnostics.

BaronyModLoader v1.0 should not be:

- an in-place patcher for retail executables;
- a source-built Barony runtime strategy;
- an arbitrary native plugin loader;
- a broad scripting framework;
- a one-off Stash-only launcher with framework language painted on afterward.

## Long-term path, MVP surface

The long-term shape is inspired by tModLoader/SKSE/Fabric-style separation, but narrowed to installed PC executables:

```text
Installed PC Barony copy
  stock executable remains untouched
  maps/data/assets/libs remain the canonical owned game install
  first verified target: Steam/Linux build 22630456
  product target: Steam/Epic/GOG/Humble desktop builds

BaronyModLoader app
  detects storefront/build/executable provenance
  manages profiles/packages/hook runtimes
  validates before launch
  writes runtime manifests
  launches the installed executable with BML hook/bootstrap environment

BML hook/bootstrap runtime
  BML-owned native library loaded into the installed game process
  versioned per OS/store/build/symbol map
  reads the app-written manifest
  activates only validated capabilities

Stash package
  declarative package proving the first BML modules
  no arbitrary code execution
```

The MVP implements only enough of that architecture to make Stash work correctly and robustly.

## Quality bar

The MVP must stay simple, but not sloppy.

Code quality rules:

1. **No Stash special cases in the app.**
   - The app may use Stash fixtures and examples.
   - Production app logic should reason about packages, capabilities, profiles, and runtimes.

2. **No broad plugin runtime in v1.**
   - No arbitrary `.so`/DLL loading.
   - No Lua/WASM runtime.
   - No global event bus.

3. **Small engine hook surface.**
   - Existing Barony files should contain thin calls into BML runtime modules.
   - BML logic should live in isolated `bml`/`modding` runtime files.

4. **Fail closed before gameplay.**
   - Missing runtime capability blocks Stash.
   - Mismatched Steam build/runtime blocks launch.
   - Storage/schema incompatibility blocks or quarantines Stash state rather than corrupting inventory.

5. **Every artifact is inspectable.**
   - Runtime info, runtime manifest, load report, package manifest, and profile state are JSON/TOML artifacts with stable fields.

6. **Vanilla Barony remains recoverable.**
   - Do not overwrite stock Steam `barony.x86_64`.
   - Do not require deleting Steam files to disable BML.

7. **Persistent state is profile-scoped.**
   - Stash state belongs to the BML profile/state root, not the package archive and not arbitrary Steam install paths.

## MVP v1.0 deliverables

### 1. App: Steam-aware install discovery

Current status: implemented as `steam detect`.

MVP completion requirements:

- detect Steam Barony app id `371970`;
- read `appmanifest_371970.acf`;
- record Steam build id;
- record install path, executable path, and asset root;
- mark stock executable as `stockExecutablePatched=false`;
- create a Steam-backed profile with `profile create --steam`.

Acceptance command:

```sh
python framework/BaronyModLoader/app/barony_mod_loader.py steam detect
python framework/BaronyModLoader/app/barony_mod_loader.py profile create .tmp/bml-steam-profile --id steam-default --steam --runtime-info framework/BaronyModLoader/fixtures/runtime-info.stash.json
```

### 2. App: package manager and profile activation

Current status: implemented for local Stash package flow.

MVP completion requirements:

- validate `mods/stash/bml-package.json`;
- pack Stash into deterministic `.bmlpkg`;
- install package into a package store;
- enable/disable package in a profile;
- inspect active profile state;
- never mutate the package archive during activation.

Acceptance commands:

```sh
python framework/BaronyModLoader/app/barony_mod_loader.py package validate mods/stash
python framework/BaronyModLoader/app/barony_mod_loader.py package pack mods/stash --out .tmp/Stash-0.1.0.bmlpkg
python framework/BaronyModLoader/app/barony_mod_loader.py package install .tmp/Stash-0.1.0.bmlpkg --store .tmp/bml-package-store
python framework/BaronyModLoader/app/barony_mod_loader.py profile enable .tmp/bml-steam-profile --package .tmp/bml-package-store/jml.stash/0.1.0
python framework/BaronyModLoader/app/barony_mod_loader.py profile inspect .tmp/bml-steam-profile
```

### 3. App: runtime registry

Current status: implemented in the app CLI for registration, listing, inspection, checksum capture, Steam build matching, and package capability selection, but the current implementation is still executable-centric and must be reworked for installed-executable hook runtimes.

Purpose: bridge the current gap between installed PC game detection and actually launching a BML hook/bootstrap runtime against that installed executable.

MVP implementation:

```text
BaronyModLoader runtime register
BaronyModLoader runtime list
BaronyModLoader runtime inspect
```

Runtime registry fields:

```json
{
  "id": "steam-linux-371970-22630456-hook-dev",
  "runtimeStrategy": "installed-binary-hook",
  "storefront": "steam",
  "steamAppId": "371970",
  "steamBuildId": "22630456",
  "platform": "linux-x86_64",
  "runtimeVersion": "0.1.0",
  "runtimeContract": "bml-runtime-contract@0.1.0",
  "steamExecutable": "/home/jerry/.local/share/Steam/steamapps/common/Barony/barony.x86_64",
  "steamExecutableSha256": "...",
  "steamExecutableBuildId": "58089d84bce3afb48d5b19df032f7aa89d81b69a",
  "gameVersionString": "v5.0.2",
  "hookLibrary": "/path/to/libbarony_bml.so",
  "hookLibrarySha256": "...",
  "hookManifest": "native/barony-modloader-hook/manifests/steam-371970-22630456-linux.json",
  "runtimeInfo": "/path/to/runtime-info.installed-hook.json",
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

Acceptance criteria:

- runtime registration fails if the installed executable is missing;
- runtime registration fails if the hook library is missing;
- runtime registration fails if runtime-info or hook manifest is missing or invalid;
- runtime registration records executable and hook-library SHA-256;
- runtime selection requires matching storefront/build/provenance and required Stash capabilities;
- app explains missing runtime with a clear action: register a BML hook runtime for the detected installed PC build.

### 4. App: real launch command

Current status: implemented in the app CLI for registered runtime selection, manifest/active-mods/log artifact creation, dry-run diagnostics, and process execution against a shim runtime; installed-executable hook launch verification is still pending.

MVP implementation:

```text
BaronyModLoader launch <profile-dir> --package <installed-package> [--runtime <runtime-id>] -- [barony args]
```

Launch flow:

1. load profile;
2. detect/verify installed PC game still matches recorded storefront/build/provenance;
3. load installed Stash package;
4. select compatible registered hook runtime;
5. validate package against runtime capabilities;
6. validate hook library, hook manifest, and symbol map provenance;
7. write `runtime-manifest.json`;
8. write `active-mods.json`;
9. create log/state dirs;
10. launch the installed game executable with hook environment;
11. capture stdout/stderr to app logs;
12. read runtime-load-report if written.

Launch environment for the first Steam/Linux target:

```text
cwd=<Steam Barony install>
SteamAppId=371970
SteamGameId=371970
BML_PROFILE_DIR=<profile dir>
BML_RUNTIME_MANIFEST=<profile>/BaronyModLoader/runtime-manifest.json
LD_PRELOAD=<path/to/libbarony_bml.so>
LD_LIBRARY_PATH=<Steam Barony install>:$LD_LIBRARY_PATH
```

The stock installed executable should not receive BML-only command-line arguments unless the hook/bootstrap design explicitly proves they are safe. Prefer environment variables and manifest paths for the no-op hook MVP.

Acceptance criteria:

- launch refuses unsupported installed executable provenance;
- launch refuses missing or mismatched hook library/manifest/symbol map;
- launch refuses runtime missing any Stash-required capability;
- launch writes manifest and logs even on failure;
- launch has a dry-run mode that clearly prints the installed executable path and hook environment.

### 5. Runtime: clean BML hook module structure

Current status: source patch artifacts exist as semantic references; the supported v1 native runtime should live in a new installed-executable hook module tree.

MVP runtime layout target:

```text
native/barony-modloader-hook/
  CMakeLists.txt
  src/bootstrap.cpp              # library constructor, manifest/env discovery, report setup
  src/provenance.cpp             # executable/build/version/hash checks
  src/symbols.cpp                # symbol/signature resolution for supported PC builds
  src/hooks.cpp                  # hook installation/removal primitives
  src/stash_inventory.cpp        # persistent inventory binding hooks
  src/stash_placement.cpp        # lobby/shop placement hooks
  src/stash_reports.cpp          # runtime-load-report + diagnostics
  manifests/steam-371970-22630456-linux.json
```

The old Barony source patch files should remain reference material only:

```text
native/barony-modloader-runtime/stash-source-map.toml
native/barony-modloader-runtime/stash-verification-plan.toml
native/barony-modloader-runtime/patches/*.patch
```

Acceptance criteria:

- BML hook logic is isolated in `native/barony-modloader-hook/`;
- hook installation is explicit and build/provenance-gated;
- runtime compiles without Stash package hardcoding in the launcher;
- disabling Stash returns behavior to vanilla-compatible paths.

### 6. Runtime: app-to-engine manifest loading

Current status: handshake patch artifact exists.

MVP requirements:

- parse `--bml-runtime-manifest`;
- fallback to `BML_RUNTIME_MANIFEST` env var;
- load manifest JSON;
- validate contract id/version;
- validate required capabilities;
- reject unsafe/malformed manifest before gameplay;
- write `runtime-load-report.json` with stable errors.

Acceptance criteria:

- launching without manifest behaves like vanilla or clearly reports BML disabled;
- malformed manifest writes a failed load report;
- Stash manifest only loads when all required capabilities are supported.

### 7. Runtime: Stash-required capabilities

Current status: source patch artifact exists; playable verification pending.

MVP capability modules:

```text
persistent_storage
persistent_inventory
void_chest_binding
placement_lobby
placement_shop
multiplayer_version_metadata
```

Stash behavior requirements:

- Stash maps to Barony's existing `void_chest_inventory` concept;
- every Void Chest access path opens the same persistent inventory while Stash is active;
- spell-created Void Chests use the same persistent inventory;
- permanent Void Chest appears near lobby assistive items;
- permanent Void Chest appears in each generated shop where a safe tile exists;
- inventory persists across save/load and run boundaries;
- state is host-authoritative in multiplayer;
- incompatible clients/saves fail before state divergence.

Acceptance criteria:

- item placed in Stash remains after save/resume;
- item placed in Stash remains after death/new run/relaunch according to profile scope;
- lobby/shop/spell-created Void Chests open the same inventory;
- removing/disabling Stash prevents Stash hooks from activating;
- runtime reports storage/placement/multiplayer errors clearly.

### 8. Build: installed-executable hook bootstrap

Current status: planned. The previous local `/tmp/barony-bml-build/barony` source-build smoke path was deleted and is not valid Steam-current evidence.

MVP requirements:

- build a BML-owned native hook/bootstrap library for the first Steam/Linux target;
- launch the installed Steam executable with that hook loaded;
- record hook runtime info and provenance;
- register the hook runtime with the BML app;
- keep stock game executables untouched;
- use the installed game directory as asset/data root when launching.

Acceptance criteria:

- hook library exists and has recorded SHA-256;
- runtime-info advertises the installed-executable hook strategy;
- registered runtime matches Steam build id `22630456` and executable provenance;
- BML launcher can start the installed Steam executable with hook environment in dry-run and no-op load modes.

### 9. Verification: user-layer Stash proof

MVP cannot be called done until user-layer behavior is verified in a playable Barony session.

Required evidence:

- screenshot or log proving launcher selected Steam install and BML runtime;
- runtime-load-report showing Stash loaded;
- lobby Void Chest observed;
- shop Void Chest observed;
- Stash item persisted after save/resume;
- Stash item persisted after run boundary/relaunch;
- disabled Stash does not activate hooks;
- mismatch path produces clear block/failure.

## Recommended phase order

### Phase A: contracts and docs rebaseline

1. remove source-build as a supported v1 runtime strategy;
2. document the PC storefront/platform target matrix;
3. update schemas/fixtures for installed-executable hook provenance;
4. commit and push.

Exit criteria: docs and contracts describe the same installed PC executable hook strategy.

### Phase B: app launcher strategy support

1. extend Steam detection with executable provenance;
2. add installed-hook runtime registry fields;
3. implement launch dry-run for `LD_PRELOAD`/hook environment;
4. verify against a fake/small hook artifact first;
5. commit and push.

Exit criteria: app can select a registered installed-hook runtime and construct the correct launch command without needing Stash behavior yet.

### Phase C: no-op native hook proof

1. create `native/barony-modloader-hook/`;
2. compile `libbarony_bml.so`;
3. load inside the installed Steam/Linux executable;
4. write canonical runtime-load-report and diagnostics;
5. fail closed on provenance mismatch.

Exit criteria: installed Steam executable starts with the hook loaded and writes BML reports without gameplay hooks.

### Phase D: symbol map and harmless probe

1. build symbol/provenance manifest for Steam/Linux build `22630456`;
2. resolve one harmless exported symbol/global;
3. verify PIE relocation handling;
4. emit diagnostics without mutating gameplay state.

Exit criteria: hook runtime can safely recognize the installed build it supports.

### Phase E: Stash behavior verification

1. launch through BML profile;
2. verify lobby/shop placements;
3. verify persistent inventory;
4. verify save/run/relaunch boundaries;
5. verify disable/mismatch failure paths;
6. update release manifest from pending to verified.

Exit criteria: Stash works from the player perspective on the first supported PC target.

## v1.0 done definition

BaronyModLoader v1.0 is done when:

- supported PC install detection works for the v1 target set or unsupported PC storefront/platform combinations are explicitly blocked with clear diagnostics;
- Stash package validates/packs/installs/enables/disables;
- an installed-executable BML hook runtime is registered and selected by build/provenance/capability compatibility;
- `launch` starts the installed game executable with the selected hook runtime;
- runtime loads the manifest and writes canonical reports;
- Stash behavior works in-game on the verified v1 target;
- stock Barony executables remain untouched;
- all failure modes produce clear diagnostics;
- repo is clean, pushed, and includes verification evidence.

## Explicit post-v1 scope

Do not add these until Stash v1 works:

- GUI app;
- broad plugin SDK;
- Lua/WASM scripting;
- source-built Barony runtime support;
- Workshop publishing automation;
- multiple gameplay mods;
- hot reload;
- multiplayer convenience features beyond compatibility blocking;
- automated on-disk binary patching.

## Immediate next step

Implement the app-side installed-executable hook registry and launch dry-run first.

That is the smallest next step that preserves the long-term architecture and moves from package planning toward an actual PC game launcher without forcing us to solve native detouring and in-game Stash behavior in the same commit.
