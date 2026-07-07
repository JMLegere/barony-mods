# Project constraints

- Edit docs/memory separately from implementation: documentation-only passes must not change app source, tests, schemas, BDD files, or native hook code.
- BaronyModLoader v1 launches the active profile modlist. `launch-plan` and `launch` do not use `--package` as a launch target; `--package` is only an optional assertion that the package is already active in the profile.
- Pre-launch compatibility is app-owned. The app builds a modlist compatibility plan from enabled profile packages, then writes `runtime-manifest.json`, `active-mods.json`, and `validation-report.json` before launch.
- Compatibility means declared package metadata can be planned deterministically: required dependencies are present and version-satisfied, conflicts/exclusive capability owners do not overlap, required runtime capabilities are supported, and load-order edges are acyclic.
- Non-blocking compatibility warnings currently include optional missing package dependencies and missing non-required `loadAfter`/`loadBefore` hint targets. The app records warnings and uses deterministic fallback ordering when hints do not fully order the modlist.
- Blockers include required missing dependencies, unsatisfied required dependency versions, package conflicts, exclusive capability conflicts, unsupported required capabilities, invalid active package records, and load-order cycles.
- The engine runtime reads the app-written manifest and validates contract/capability support. It must not scan package stores, resolve dependencies/conflicts/load order, or repair the modlist at runtime.
- First scope is declarative planning, readable blockers/warnings, deterministic fallback ordering, installed-package launch artifacts, and BML-owned hook/runtime capabilities needed by Stash. A full automatic resolver that installs, disables, upgrades, or synthesizes compatibility packages is out of scope until later.
- Staging boundary: packages remain immutable inputs under the package store; profiles own active mod state and launch artifacts; runtime writes reports/state under profile-local BML paths and never back into package archives.
