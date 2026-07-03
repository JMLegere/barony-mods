# BaronyModLoader native hook runtime

This directory is the v1 runtime authority for the installed-executable path.

BML v1 targets the installed PC game executable instead of building or patching an open-source Barony fork. The app launches the stock executable and injects a platform-specific BML hook library. On Linux the verified local implementation path is `LD_PRELOAD`; macOS and Windows still need platform-specific launch adapters before they can be claimed as verified.

Current state:

- `src/bml_hook.c` builds a Linux ELF shared object with a constructor and exported `bml_hook_init` symbol.
- `build/libbarony_bml.so` is produced by the local `Makefile`.
- `manifests/steam-371970-22630456-linux.json` mirrors the local verified Steam/Linux Barony executable identity, required symbol probe targets, and fail-closed Stash hook targets passed as `BML_HOOK_MANIFEST`; the current native hook validates the path is readable and uses compiled-in tables for probing/analysis.
- The hook validates launch inputs and writes `BaronyModLoader/reports/runtime-load-report.json` under `BML_PROFILE_DIR`; executable provenance is still enforced by the app/runtime registry before launch.
- The hook resolves required installed-binary symbols with `dlsym(RTLD_DEFAULT, mangledSymbol)` and writes `BaronyModLoader/reports/symbol-probe-report.json`.
- Stash gameplay hooks are grouped behind the abstract `linux-x86_64-direct-stash-detour` backend. The default backend mode remains `analyze-only`: it keeps data symbols `ready`, classifies function prologues with the conservative decoder, records `patchWindowBytes`, and fails closed for active Stash profiles until gameplay hooks are installed and verified. Opt-in modes now cover the generic detour self-test, the single `Entity::addItemToVoidChestServer` bridge, the seven-target core inventory lifecycle pass-through bridge (`Entity::getChestInventoryList`, `Entity::addItemToChest`, `Entity::getItemFromChest`, `Entity::addItemToVoidChestServer`, `Entity::removeItemFromVoidChestServer`, `Entity::closeChest`, and `Entity::closeChestServer`), and an experimental fake-provider-only state-backed core behavior self-test (`BML_STASH_ENABLE_EXPERIMENTAL_CORE_BEHAVIOR=1` plus `BML_STASH_CORE_BEHAVIOR_SELF_TEST=1`) that writes `BaronyModLoader/reports/stash-core-behavior-report.json` while still leaving runtime load fail-closed.

Build and test:

```sh
make -C native/barony-modloader-hook clean all
make -C native/barony-modloader-hook test
make -C native/barony-modloader-hook static-readiness
```

The test target builds `build/libbarony_bml.so` plus a fake Barony symbol provider, creates temporary profile/runtime manifests, preloads both libraries into `/usr/bin/true`, and validates the JSON runtime load, symbol probe, Stash hook, generic detour self-test, opt-in Stash target detour self-test, opt-in Stash add-item pass-through install, opt-in Stash core pass-through install, and experimental fake-provider state-backed core behavior reports with Python. It does not launch Barony or prove in-game Stash behavior. The `static-readiness` target runs `tools/analyze_stash_targets.py` against the local Steam/Linux executable and reports static prologue readiness only.

Manual smoke shape:

```sh
BML_PROFILE_DIR=/path/to/profile \
BML_RUNTIME_MANIFEST=/path/to/profile/BaronyModLoader/runtime-manifest.json \
BML_HOOK_MANIFEST=/path/to/native/barony-modloader-hook/manifests/steam-371970-22630456-linux.json \
BML_HOOK_LIBRARY=/path/to/native/barony-modloader-hook/build/libbarony_bml.so \
LD_PRELOAD=/path/to/native/barony-modloader-hook/build/libbarony_bml.so \
/usr/bin/true
```

Set `BML_DETOUR_SELF_TEST=1` with the fake symbol provider preloaded to write `BaronyModLoader/reports/detour-self-test-report.json`. Set `BML_STASH_DETOUR_SELF_TEST=1` to verify the exact `Entity::addItemToVoidChestServer` fake-symbol detour and trampoline call-through. Set `BML_STASH_INSTALL_ADD_ITEM_PASSTHROUGH=1` to install the process-lifetime add-item pass-through report. Set `BML_STASH_INSTALL_CORE_PASSTHROUGH=1` to install and report the seven-target core inventory lifecycle pass-through set. Set `BML_STASH_ENABLE_EXPERIMENTAL_CORE_BEHAVIOR=1` with `BML_STASH_CORE_BEHAVIOR_SELF_TEST=1` under the fake provider to validate state-backed load/add/remove/save over `stats[0]->void_chest_inventory`, including generic `addItemToChest` and `getItemFromChest` mutations. These opt-in paths are still fail-closed reports, not playable in-game Stash verification.

Current non-goals:

- Do not claim playable Stash behavior: verified native detour coverage now includes fake-symbol self-tests, analyze-only target readiness reporting, opt-in add-item/core pass-through install reports, and a fake-provider-only state-backed core behavior self-test that exercises both Void Chest server mutation and generic chest add/get mutation. The installed Steam game has not yet had the complete behavior-changing Stash gameplay hook set installed and verified in-game.
- Do not treat `native/barony-modloader-runtime/patches/` as the runtime path. Those source-fork patches are semantic reference only.
- Do not claim Windows or macOS native injection support yet.
