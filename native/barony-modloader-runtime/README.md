# BaronyModLoader source patch references

This directory stores historical/semantic Barony source patch artifacts for the BaronyModLoader runtime handshake and Stash hook design. They are not the supported v1 runtime path. BML v1 targets installed PC executables through a BML-owned hook/bootstrap runtime.

## Use of these references

Use these patch artifacts to understand source-level intent: manifest loading, Void Chest inventory binding, persistent Stash state, lobby/shop placement, save metadata, and diagnostics. Do not apply them as the v1 runtime and do not treat a source-built Barony executable as storefront-current compatibility evidence.

Historical application command, retained only for review/reference:

```sh
git apply /path/to/barony-mods/native/barony-modloader-runtime/patches/0001-bml-runtime-handshake.patch
git apply /path/to/barony-mods/native/barony-modloader-runtime/patches/0002-bml-stash-runtime.patch
git apply /path/to/barony-mods/native/barony-modloader-runtime/patches/0003-bml-stash-diagnostics.patch
```

The unified diff uses paths relative to the Barony source root:

- `src/barony_mod_loader.hpp`
- `src/barony_mod_loader.cpp`
- `src/CMakeLists.txt`
- `src/game.cpp`

## What the patches demonstrated

`0001-bml-runtime-handshake.patch` demonstrated a small `BaronyModLoader` runtime helper and startup manifest validation flow in source form.

The patched binary supports:

- `--bml-runtime-info`: print runtime info JSON to stdout and exit.
- `--bml-runtime-info=<path>`: write runtime info JSON to a file and exit.
- `--bml-runtime-manifest <path>`: load and validate a runtime manifest before SDL initialization.
- `--bml-runtime-manifest=<path>`: same manifest load path using `=` form.
- `BML_RUNTIME_MANIFEST`: environment fallback when no manifest argument is supplied.
- `--bml-profile-root <path>` / `--bml-profile-root=<path>`: root BML reports/state under the selected profile instead of Barony's default output directory.
- `runtime-load-report.json`: written with `loaded` or `failed` status and stable error codes (`BML_OK`, `BML_MANIFEST_READ_FAILED`, `BML_CONTRACT_MISSING`, `BML_CONTRACT_UNSUPPORTED`, `BML_CAPABILITY_MISSING`, `BML_REPORT_WRITE_FAILED`).

The runtime info and manifest validation use the P2 contract `bml-runtime-contract` version `0.1.0` and the six canonical capability IDs:

- `persistent_storage`
- `persistent_inventory`
- `void_chest_binding`
- `placement_lobby`
- `placement_shop`
- `multiplayer_version_metadata`

`0002-bml-stash-runtime.patch` implements the first Stash gameplay hooks: profile-scoped persistent inventory storage, Void Chest inventory binding, lobby/shop access-point hooks, and save metadata compatibility fields.

`0003-bml-stash-diagnostics.patch` adds lightweight profile-scoped diagnostics (`BaronyModLoader/state/stash-diagnostics.jsonl`) and log lines for runtime load, Stash inventory saves, and Stash access point placement. This exists to make in-game smoke tests observable without attaching a debugger.

The manifest validator is intentionally narrow for v1: it verifies the runtime contract id/version and the presence of all six canonical capability strings, then writes a load report before the normal game starts.
