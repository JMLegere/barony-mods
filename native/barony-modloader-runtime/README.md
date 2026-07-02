# BaronyModLoader runtime handshake patch

This directory stores the P2 Barony source patch artifact for the BaronyModLoader v1.0 runtime handshake. It is a source patch kept in this repository for review; it has not been applied to `/tmp/barony-src` by this repo.

## Apply the patch

From a clean Barony source checkout at the revision used for `/tmp/barony-src`:

```sh
git apply /path/to/barony-mods/native/barony-modloader-runtime/patches/0001-bml-runtime-handshake.patch
```

The unified diff uses paths relative to the Barony source root:

- `src/barony_mod_loader.hpp`
- `src/barony_mod_loader.cpp`
- `src/CMakeLists.txt`
- `src/game.cpp`

## What P2 adds

P2 is handshake only. It adds a small `BaronyModLoader` runtime helper and wires it into Barony startup before SDL initialization.

The patched binary supports:

- `--bml-runtime-info`: print runtime info JSON to stdout and exit.
- `--bml-runtime-info=<path>`: write runtime info JSON to a file and exit.
- `--bml-runtime-manifest <path>`: load and validate a runtime manifest before SDL initialization.
- `--bml-runtime-manifest=<path>`: same manifest load path using `=` form.
- `BML_RUNTIME_MANIFEST`: environment fallback when no manifest argument is supplied.
- `runtime-load-report.json`: written with `loaded` or `failed` status and stable error codes (`BML_OK`, `BML_MANIFEST_READ_FAILED`, `BML_CONTRACT_MISSING`, `BML_CONTRACT_UNSUPPORTED`, `BML_CAPABILITY_MISSING`, `BML_REPORT_WRITE_FAILED`).

The runtime info and manifest validation use the P2 contract `bml-runtime-contract` version `0.1.0` and the six canonical capability IDs:

- `persistent_storage`
- `persistent_inventory`
- `void_chest_binding`
- `placement_lobby`
- `placement_shop`
- `multiplayer_version_metadata`

## What P2 does not add

P2 does not implement Stash gameplay behavior. It does not persist items, bind the live Void Chest inventory to profile storage, place lobby/shop chests, change multiplayer compatibility checks, patch savegame item serialization, or alter Barony gameplay loops. Those are later gameplay-hook phases after the app/runtime manifest handshake is established.

The manifest validator is intentionally narrow for P2: it verifies the runtime contract id/version and the presence of all six canonical capability strings, then writes a load report before the normal game starts.
