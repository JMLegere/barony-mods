# BaronyModLoader native hook runtime

This directory is the v1 runtime authority for the installed-executable path.

BML v1 targets the installed PC game executable instead of building or patching an open-source Barony fork. The app launches the stock executable and injects a platform-specific BML hook library. On Linux the first implementation path is `LD_PRELOAD`; macOS and Windows need platform-specific launch adapters before they can be claimed as verified.

Current state:

- `manifests/steam-371970-22630456-linux.json` pins the local verified Steam/Linux Barony executable identity.
- `libbarony_bml.so` is not implemented yet.
- The app-side registry and launch dry-run can be validated with a fake hook library file, but that does not prove in-game behavior.

Non-goal for v1 runtime authority:

- Do not treat `native/barony-modloader-runtime/patches/` as the runtime path. Those source-fork patches are semantic reference only.
