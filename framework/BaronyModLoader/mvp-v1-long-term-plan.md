# BaronyModLoader MVP v1.0 Plan

## Decision

Build the long-term architecture now, but only implement the runtime modules needed for the Stash reference mod.

BaronyModLoader v1.0 should be:

- a standalone app/launcher;
- a Steam-aware profile and package manager;
- a selector for version-matched BaronyModLoader-enabled runtimes;
- a narrow Barony engine runtime sidecar/fork with upstreamable hook points;
- a schema-first package/runtime contract;
- a working Stash mod proving persistent Void Chest storage, lobby/shop access, and compatibility diagnostics.

BaronyModLoader v1.0 should not be:

- an in-place patcher for the stock Steam executable;
- a retail binary patching system;
- an arbitrary native plugin loader;
- a broad scripting framework;
- a one-off Stash-only fork with framework language painted on afterward.

## Long-term path, MVP surface

The long-term shape is inspired by tModLoader/SKSE/Fabric-style separation:

```text
Steam Barony install
  stock barony.x86_64 remains untouched
  maps/data/assets/libs remain the canonical owned game install

BaronyModLoader app
  detects Steam install/build
  manages profiles/packages/runtimes
  validates before launch
  writes runtime manifests
  launches a selected BML runtime

BML runtime sidecar
  built from Barony source plus isolated BML runtime modules
  version-matched to the Steam build/source target
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

Current status: implemented in the app CLI for registration, listing, inspection, checksum capture, Steam build matching, and package capability selection.

Purpose: bridge the current gap between Steam install detection and actually launching a BML-enabled runtime.

MVP implementation:

```text
BaronyModLoader runtime register
BaronyModLoader runtime list
BaronyModLoader runtime inspect
```

Runtime registry fields:

```json
{
  "id": "bml-runtime-steam-371970-22630456-linux-x86_64",
  "steamAppId": "371970",
  "steamBuildId": "22630456",
  "platform": "linux-x86_64",
  "runtimeVersion": "0.1.0",
  "runtimeContract": "bml-runtime-contract@0.1.0",
  "executable": "/path/to/barony-bml.x86_64",
  "runtimeInfo": "/path/to/runtime-info.json",
  "sha256": "...",
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

- runtime registration fails if executable is missing;
- runtime registration fails if runtime-info is missing or invalid;
- runtime registration records executable SHA-256;
- runtime selection requires matching Steam build id and required Stash capabilities;
- app explains missing runtime with a clear action: build/register BML runtime for the detected Steam build.

### 4. App: real launch command

Current status: implemented in the app CLI for registered runtime selection, manifest/active-mods/log artifact creation, dry-run diagnostics, and process execution against a shim runtime; playable Barony runtime verification is still pending.

MVP implementation:

```text
BaronyModLoader launch <profile-dir> --package <installed-package> [--runtime <runtime-id>] -- [barony args]
```

Launch flow:

1. load profile;
2. detect/verify Steam install still matches recorded app/build;
3. load installed Stash package;
4. select compatible registered runtime;
5. validate package against runtime capabilities;
6. write `runtime-manifest.json`;
7. write `active-mods.json`;
8. create log/state dirs;
9. launch runtime executable with Steam install as working directory;
10. capture stdout/stderr to app logs;
11. read runtime-load-report if written.

Launch environment:

```text
cwd=<Steam Barony install>
SteamAppId=371970
SteamGameId=371970
BML_PROFILE_DIR=<profile dir>
BML_RUNTIME_MANIFEST=<profile>/BaronyModLoader/runtime-manifest.json
LD_LIBRARY_PATH=<Steam Barony install>:$LD_LIBRARY_PATH
```

Runtime arguments:

```text
--bml-runtime-manifest <profile>/BaronyModLoader/runtime-manifest.json
--bml-profile-root <profile>
```

Acceptance criteria:

- launch refuses stock Steam executable for Stash;
- launch refuses stale runtime build id;
- launch refuses runtime missing any Stash-required capability;
- launch writes manifest and logs even on failure;
- launch has a dry-run mode for diagnostics.

### 5. Runtime: clean BML module structure in Barony source

Current status: patch artifacts exist, but should evolve toward isolated runtime module files.

MVP runtime layout target:

```text
src/bml/
  bml_runtime.hpp
  bml_runtime.cpp
  bml_manifest.hpp
  bml_manifest.cpp
  bml_reports.hpp
  bml_reports.cpp
  bml_storage.hpp
  bml_storage.cpp
  bml_inventory.hpp
  bml_inventory.cpp
  bml_void_chest.hpp
  bml_void_chest.cpp
  bml_placement.hpp
  bml_placement.cpp
  bml_multiplayer.hpp
  bml_multiplayer.cpp
```

Existing Barony files should have minimal hook calls only:

```text
src/game.cpp          startup args, runtime initialization/report path
src/actchest.cpp      Void Chest inventory binding and save-on-close boundary
src/maps.cpp          lobby/shop placement hooks
src/scores.cpp/.hpp   save/load metadata boundaries if needed
src/CMakeLists.txt    include BML runtime sources
```

Acceptance criteria:

- BML logic is isolated from large Barony functions where possible;
- call sites are thin and readable;
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

### 8. Build: Steam-backed runtime build

Current status: completed locally for the current workstation after installing build dependencies. The patched Barony source builds at `/tmp/barony-bml-build/barony`, writes `/tmp/barony-bml-build/runtime-info.json`, registers against Steam build id `22630456`, and can be executed through the BML launcher in `--bml-runtime-info` startup mode. This is local build evidence, not a distributable runtime release.

MVP requirements:

- build a BML-enabled Barony executable from source plus BML runtime modules;
- record runtime info sidecar;
- register the runtime with BML app;
- keep stock Steam executable untouched;
- use Steam install as asset/data root when launching.

Acceptance criteria:

- runtime executable exists and has recorded SHA-256;
- runtime-info advertises required capabilities;
- registered runtime matches Steam build id `22630456` or explicitly documents the source/build compatibility mapping;
- BML launcher can start the runtime.

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

### Phase A: app launcher completion

1. implement runtime registry;
2. implement launch dry-run;
3. implement launch command process execution;
4. verify against a fake/small runtime executable first;
5. commit and push.

Exit criteria: app can select a registered runtime and construct/execute the correct launch command without needing Stash behavior yet.

### Phase B: runtime code quality pass

1. convert patch artifacts into clean BML runtime module files;
2. keep hook call sites small;
3. standardize `--bml-runtime-manifest` and report paths;
4. ensure runtime can run vanilla when no manifest is provided;
5. commit and push.

Exit criteria: native patch still applies, but BML code is isolated and maintainable.

### Phase C: build Steam-backed runtime

1. install/provide build dependencies;
2. configure CMake build;
3. compile BML runtime;
4. generate runtime-info sidecar;
5. register runtime in BML app;
6. commit build docs/scripts, not local binaries unless intentionally releasing.

Exit criteria: app can launch a real BML-enabled Barony executable.

### Phase D: Stash behavior verification

1. launch through BML profile;
2. verify lobby/shop placements;
3. verify persistent inventory;
4. verify save/run/relaunch boundaries;
5. verify disable/mismatch failure paths;
6. update release manifest from pending to verified.

Exit criteria: Stash works from the player perspective.

## v1.0 done definition

BaronyModLoader v1.0 is done when:

- Steam install detection works;
- Stash package validates/packs/installs/enables/disables;
- a BML runtime executable is registered and selected by build/capability compatibility;
- `launch` starts the selected runtime against the Steam install;
- runtime loads the manifest and writes reports;
- Stash behavior works in-game;
- stock Steam Barony remains untouched;
- all failure modes produce clear diagnostics;
- repo is clean, pushed, and includes verification evidence.

## Explicit post-v1 scope

Do not add these until Stash v1 works:

- GUI app;
- broad plugin SDK;
- Lua/WASM scripting;
- Workshop publishing automation;
- multiple gameplay mods;
- hot reload;
- multiplayer convenience features beyond compatibility blocking;
- automated binary patching.

## Immediate next step

Implement the app-side runtime registry and launch dry-run first.

That is the smallest next step that preserves the long-term architecture and moves from package planning toward an actual game launcher without forcing us to solve the native build and in-game Stash behavior in the same commit.
