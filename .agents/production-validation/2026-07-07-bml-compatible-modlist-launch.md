# Production Validation: BML Compatible Modlist Launch

Date: 2026-07-07
Branch: `bml-compatible-modlist-launch`

## Objective

Validate that the implemented BML compatible multi-mod launch flow works against the live installed Barony environment, not only unit/BDD fixtures.

## Runtime registration

Command run:

```sh
python framework/BaronyModLoader/app/barony_mod_loader.py runtime register \
  --registry ~/.local/share/BaronyModLoader/runtime-registry.json \
  --id barony-bml-runtime-stash-installed-binary-hook-steam-371970-22630456-linux-x86_64 \
  --runtime-strategy installed-binary-hook \
  --steam-build-id 22630456 \
  --steam-executable /home/jerry/.local/share/Steam/steamapps/common/Barony/barony.x86_64 \
  --hook-library native/barony-modloader-hook/build/libbarony_bml.so \
  --hook-manifest native/barony-modloader-hook/manifests/steam-371970-22630456-linux.json \
  --runtime-info framework/BaronyModLoader/fixtures/runtime-info.installed-hook.stash.json
```

Evidence:

- Command returned `status: registered`.
- Registry path: `/home/jerry/.local/share/BaronyModLoader/runtime-registry.json`.
- Runtime id: `barony-bml-runtime-stash-installed-binary-hook-steam-371970-22630456-linux-x86_64`.
- Steam executable: `/home/jerry/.local/share/Steam/steamapps/common/Barony/barony.x86_64`.
- Runtime capabilities include Stash and Runebound capability sets.

## Live profile state

Profile:

```text
/home/jerry/.local/share/BaronyModLoader/profiles/default
```

Active mods observed in both profile and active-mods artifact:

- `jml.runebound-elixirs` version `0.1.0`
- `jml.stash` version `0.1.0`

## Live multi-mod launch plan

Command run:

```sh
python framework/BaronyModLoader/app/barony_mod_loader.py launch-plan \
  ~/.local/share/BaronyModLoader/profiles/default \
  --runtime-info framework/BaronyModLoader/fixtures/runtime-info.installed-hook.stash.json \
  --out ~/.local/share/BaronyModLoader/profiles/default/BaronyModLoader/runtime-manifest.json
```

Evidence:

- Command returned `status: created`.
- Runtime manifest path: `/home/jerry/.local/share/BaronyModLoader/profiles/default/BaronyModLoader/runtime-manifest.json`.
- Active mods path: `/home/jerry/.local/share/BaronyModLoader/profiles/default/BaronyModLoader/active-mods.json`.
- Validation report path: `/home/jerry/.local/share/BaronyModLoader/profiles/default/BaronyModLoader/validation-report.json`.

Validation report evidence:

- `launchable: true`.
- `blockingIssues: []`.
- Enabled mod ids:
  - `jml.runebound-elixirs`
  - `jml.stash`
- Warnings were present and non-blocking:
  - `BML_MODLIST_DEPENDENCY_FACT_UNAVAILABLE` for current game-version fact availability.

Runtime manifest evidence:

- `mods[]` contains both enabled mods.
- `jml.runebound-elixirs` has `loadOrder: 0`.
- `jml.stash` is present in the same manifest.

## Registry-backed dry-run launch

Command run:

```sh
python framework/BaronyModLoader/app/barony_mod_loader.py launch \
  ~/.local/share/BaronyModLoader/profiles/default \
  --registry ~/.local/share/BaronyModLoader/runtime-registry.json \
  --runtime barony-bml-runtime-stash-installed-binary-hook-steam-371970-22630456-linux-x86_64 \
  --dry-run
```

Evidence:

- Command returned `status: dry-run`.
- Command selected the live Steam executable.
- Environment included:
  - `BML_PROFILE_DIR`
  - `BML_RUNTIME_MANIFEST`
  - `BML_RUNTIME_STRATEGY=installed-binary-hook`
  - `BML_LAUNCH_ADAPTER=linux-ld-preload`
  - `BML_HOOK_MANIFEST`
  - `BML_HOOK_LIBRARY`
  - `LD_PRELOAD`
- Dry-run referenced the same runtime manifest, active mods, and validation report paths.

## Live installed Barony launch

Command run:

```sh
python framework/BaronyModLoader/app/barony_mod_loader.py launch \
  ~/.local/share/BaronyModLoader/profiles/default \
  --registry ~/.local/share/BaronyModLoader/runtime-registry.json \
  --runtime barony-bml-runtime-stash-installed-binary-hook-steam-371970-22630456-linux-x86_64
```

Observed behavior:

- The command remained foreground-running long enough that the harness killed it after 60 seconds.
- This is compatible with launching the live installed game process.
- Current process check after timeout found no remaining Barony process.

Runtime evidence written by the live launch:

File:

```text
/home/jerry/.local/share/BaronyModLoader/profiles/default/BaronyModLoader/reports/runtime-load-report.json
```

Key evidence:

- `status: loaded`.
- `loadedMods[]` includes `jml.stash` with `status: loaded`.
- `loadedMods[]` includes `jml.runebound-elixirs` with `status: loaded`.
- `warnings: []`.
- `errors: []`.
- `reportedAt: 2026-07-07T17:02:59Z`.

Launcher log evidence:

File:

```text
/home/jerry/.local/share/BaronyModLoader/profiles/default/BaronyModLoader/logs/launcher-runtime.log
```

Key evidence:

- Live Barony executable started and initialized.
- Log includes Barony startup lines beginning at `[14-02-59]`.
- Log includes Barony version `v5.0.2`.

## Error conflict blocking validation

A temporary production-smoke profile was created with `jml.stash` plus a staged conflicting package `jml.conflict-stash` declaring a package conflict against `jml.stash`.

Command run:

```sh
python framework/BaronyModLoader/app/barony_mod_loader.py launch <temp-conflict-profile> \
  --registry ~/.local/share/BaronyModLoader/runtime-registry.json \
  --dry-run
```

Evidence:

- Command returned exit code `1`.
- Output included `BML_MODLIST_PACKAGE_CONFLICT` as an error-level blocker.
- Output ended with `FAILED: validation errors must be fixed before launch.`
- Conflict smoke assertion: `CONFLICT_BLOCKED True`.

## Result

Production validation passed for the implemented scope:

- Installed runtime registered for the current machine.
- Live default profile has a compatible Stash + Runebound active modlist.
- Live multi-mod launch plan is launchable.
- Warning-only dependency fact availability issues are visible and non-blocking.
- Registry-backed dry-run launch produces the correct launch environment and artifact paths.
- Live installed Barony launch consumed the runtime manifest and produced a runtime load report with both mods loaded.
- Error-level package conflict blocks launch before process start.
