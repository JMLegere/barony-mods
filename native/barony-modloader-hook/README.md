# BaronyModLoader native hook runtime

This directory is the v1 runtime authority for the installed-executable path.

BML v1 targets the installed PC game executable instead of building or patching an open-source Barony fork. The app launches the stock executable and injects a platform-specific BML hook library. On Linux the verified local implementation path is `LD_PRELOAD`; macOS and Windows still need platform-specific launch adapters before they can be claimed as verified.

Current state:

- `src/bml_hook.c` builds a Linux ELF shared object with a constructor and exported `bml_hook_init` symbol.
- `build/libbarony_bml.so` is produced by the local `Makefile`.
- `manifests/steam-371970-22630456-linux.json` pins the local verified Steam/Linux Barony executable identity consumed as `BML_HOOK_MANIFEST`.
- The hook only validates launch inputs and writes `BaronyModLoader/reports/runtime-load-report.json` under `BML_PROFILE_DIR`.

Build and test:

```sh
make -C native/barony-modloader-hook clean all
make -C native/barony-modloader-hook test
```

The test target builds `build/libbarony_bml.so`, creates a temporary profile/runtime manifest, preloads the library into `/usr/bin/true`, and validates the JSON runtime-load report with Python. It does not launch Barony.

Manual smoke shape:

```sh
BML_PROFILE_DIR=/path/to/profile \
BML_RUNTIME_MANIFEST=/path/to/profile/BaronyModLoader/runtime-manifest.json \
BML_HOOK_MANIFEST=/path/to/native/barony-modloader-hook/manifests/steam-371970-22630456-linux.json \
BML_HOOK_LIBRARY=/path/to/native/barony-modloader-hook/build/libbarony_bml.so \
LD_PRELOAD=/path/to/native/barony-modloader-hook/build/libbarony_bml.so \
/usr/bin/true
```

Current non-goals:

- Do not patch Barony symbols, resolve gameplay addresses, mutate gameplay state, or implement Stash behavior in this slice.
- Do not treat `native/barony-modloader-runtime/patches/` as the runtime path. Those source-fork patches are semantic reference only.
- Do not claim Windows or macOS native injection support yet.
