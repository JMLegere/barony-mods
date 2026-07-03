# BaronyModLoader native hook runtime

This directory is the v1 runtime authority for the installed-executable path.

BML v1 targets the installed PC game executable instead of building or patching an open-source Barony fork. The app launches the stock executable and injects a platform-specific BML hook library. On Linux the verified local implementation path is `LD_PRELOAD`; macOS and Windows still need platform-specific launch adapters before they can be claimed as verified.

Current state:

- `src/bml_hook.c` builds a Linux ELF shared object with a constructor and exported `bml_hook_init` symbol.
- `build/libbarony_bml.so` is produced by the local `Makefile`.
- `manifests/steam-371970-22630456-linux.json` mirrors the local verified Steam/Linux Barony executable identity, required symbol probe targets, and fail-closed Stash hook targets passed as `BML_HOOK_MANIFEST`; the current native hook validates the path is readable and uses compiled-in tables for probing/analysis.
- The hook validates launch inputs and writes `BaronyModLoader/reports/runtime-load-report.json` under `BML_PROFILE_DIR`; executable provenance is still enforced by the app/runtime registry before launch.
- The hook resolves required installed-binary symbols with `dlsym(RTLD_DEFAULT, mangledSymbol)` and writes `BaronyModLoader/reports/symbol-probe-report.json`.
- Stash gameplay hooks are grouped behind the abstract `linux-x86_64-direct-stash-detour` backend. The current backend mode remains `analyze-only`: it keeps data symbols `ready`, runs the same conservative decoder used by the detour self-test to classify function prologues as `ready` or `blocked`, supports ordinary and REX-prefixed push/pop, register-only, non-RIP-memory, RIP-relative ModRM, and terminal-return-at-patch-boundary windows, relocated short/near relative control-flow windows, and common two-byte/multi-byte prologue instructions (`movzx`, multi-byte NOP, `pxor`, `lea`), records `patchWindowBytes` for decoder-safe targets, and fails closed for active Stash profiles until gameplay hooks are actually installed and verified. A separate `BML_DETOUR_SELF_TEST=1` path verifies only the narrow Linux x86_64 absolute-jump detour substrate against the fake test symbol provider, including near-target trampoline allocation and RIP-relative displacement relocation. `BML_STASH_INSTALL_ADD_ITEM_PASSTHROUGH=1` is an opt-in single-target bridge that installs a pass-through detour for `Entity::addItemToVoidChestServer` and writes `BaronyModLoader/reports/stash-detour-install-report.json`. `BML_STASH_INSTALL_CORE_PASSTHROUGH=1` expands that bridge to the core Void Chest lifecycle pass-through set (`Entity::getChestInventoryList`, `Entity::addItemToVoidChestServer`, `Entity::removeItemFromVoidChestServer`, `Entity::closeChest`, and `Entity::closeChestServer`) and writes `BaronyModLoader/reports/stash-core-detour-install-report.json`; both modes deliberately leave the Stash backend fail-closed and do not claim playable Stash behavior.

Build and test:

```sh
make -C native/barony-modloader-hook clean all
make -C native/barony-modloader-hook test
make -C native/barony-modloader-hook static-readiness
```

The test target builds `build/libbarony_bml.so` plus a fake Barony symbol provider, creates a temporary profile/runtime manifest, preloads both libraries into `/usr/bin/true`, and validates the JSON runtime load, symbol probe, Stash hook, generic detour self-test, opt-in Stash target detour self-test, opt-in Stash add-item pass-through install, and opt-in Stash core pass-through install reports with Python. It does not launch Barony or prove gameplay behavior. The `static-readiness` target runs `tools/analyze_stash_targets.py` against the installed Steam/Linux executable and emits a static prologue-readiness report; that report is target evidence only, not a gameplay claim.

Manual smoke shape:

```sh
BML_PROFILE_DIR=/path/to/profile \
BML_RUNTIME_MANIFEST=/path/to/profile/BaronyModLoader/runtime-manifest.json \
BML_HOOK_MANIFEST=/path/to/native/barony-modloader-hook/manifests/steam-371970-22630456-linux.json \
BML_HOOK_LIBRARY=/path/to/native/barony-modloader-hook/build/libbarony_bml.so \
LD_PRELOAD=/path/to/native/barony-modloader-hook/build/libbarony_bml.so \
/usr/bin/true
```

Set `BML_DETOUR_SELF_TEST=1` with the fake symbol provider preloaded to write `BaronyModLoader/reports/detour-self-test-report.json`. That report proves the fixture replacement ran and called through the executable trampoline to the original fake function. Set `BML_STASH_DETOUR_SELF_TEST=1` with the same fake provider to write `BaronyModLoader/reports/stash-detour-self-test-report.json`, proving the exact `Entity::addItemToVoidChestServer` symbol can be patched, routed through BML replacement code, and called through to its original fixture implementation. Set `BML_STASH_INSTALL_ADD_ITEM_PASSTHROUGH=1` without the Stash detour self-test to write `BaronyModLoader/reports/stash-detour-install-report.json`, proving the same target can be installed as a process-lifetime pass-through bridge while the runtime still fails closed for the rest of the Stash hook set. Set `BML_STASH_INSTALL_CORE_PASSTHROUGH=1` by itself to write `BaronyModLoader/reports/stash-core-detour-install-report.json`, proving the core Void Chest lifecycle targets can all be installed as pass-through detours while `runtime-load-report.json` still contains `BML_STASH_HOOKS_NOT_INSTALLED`. These reports are not Stash gameplay claims.

Current non-goals:

- Do not claim playable Stash behavior: the verified native detour coverage is still limited to fake-symbol self-tests, analyze-only target readiness reporting, one opt-in `Entity::addItemToVoidChestServer` pass-through install report, and one opt-in core Void Chest lifecycle pass-through install report; no behavior-changing Stash gameplay detour has been installed in Barony or in-game verified yet.
- Do not treat `native/barony-modloader-runtime/patches/` as the runtime path. Those source-fork patches are semantic reference only.
- Do not claim Windows or macOS native injection support yet.
