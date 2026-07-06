# Solution Direction: BML GUI Mod Manager

Date: 2026-07-06
Status: Proposed
Discovery Source: `.agents/discovery/2026-07-05-runebound.md` plus BML framework docs
Progress Doc: `.agents/solutions/2026-07-06-bml-gui-mod-manager-progress.md`

## Executive Summary

The user-selected direction is **Direction B: Profile-first GUI launcher**. BaronyModLoader should become a real user-facing mod manager GUI, not a terminal command, temporary profile, or thin wrapper around dev scripts. The GUI should own Barony install discovery, profiles, enabled mods, launch readiness, runtime manifest generation, launch, and readable diagnostics while keeping engine behavior in the platform-specific BML hook/runtime payload. Confidence is **Medium-High** because this direction matches the existing BML product docs and the user’s explicit preference; it would change if architecture proves that a GUI-first launcher cannot safely manage the current runtime/profile flow without excessive implementation risk.

## Source Discovery

- Approved discovery artifact: `.agents/discovery/2026-07-05-runebound.md`.
- Discovery relevance: Runebound discovery is not a GUI discovery, but it establishes trust/adoption constraints: stability, no crashes, co-op compatibility, Workshop-friendly adoption, clear mod boundaries, and low interaction burden.
- Primary BML source docs:
  - `framework/BaronyModLoader/README.md`
  - `framework/BaronyModLoader/loader-runtime-contract.md`
  - `framework/BaronyModLoader/package-format.md`
  - `native/barony-modloader-hook/README.md`
  - `native/barony-modloader-hook/Makefile`
- User source inputs:
  - “i want the mod loader to actually load a mod manager gui”
  - “BaronyModLoader.exe???”
  - Approval of platform launcher structure:
    ```text
    BaronyModLoader/
    ├── linux/
    │   └── BaronyModLoader
    └── windows/
        └── BaronyModLoader.exe
    ```
  - Rejection of `.tmp/runebound-elixir-live-grant/` as the normal launch path.
  - Direction choice: “B full send.”

## Decision Frame

This brief resolves the solution-direction question:

> Should BML become a GUI-first mod manager/launcher app that manages profiles, mods, launch readiness, runtime manifests, and diagnostics before launching modded Barony?

It intentionally does not resolve:

- GUI technology/framework.
- API contracts, schemas, persistence model, service boundaries, or file-level implementation.
- Exact screen layouts or production visual design.
- Detailed Linux/Windows injection implementation.
- Remote mod marketplace, Workshop integration, or public package index design.

## Candidate Directions Considered

### Direction A: Thin GUI Wrapper Around Existing CLI/Dev Flow

Concept: Add a GUI that mostly shells out to the existing Python CLI and current hook launch commands.

Why it might be right:
- Fastest route to a visible UI.
- Reuses current CLI behavior.
- Lowest immediate uncertainty.

Why it might be wrong:
- Preserves developer-shaped internals as product behavior.
- Risks making `.tmp`, manual path wiring, and CLI assumptions official.
- Does not satisfy the user’s desire for an actual mod manager GUI.

Evidence:
- Current BML docs expose Python CLI examples.
- Existing CLI can validate packages, create profiles, install packages, and dry-run launches.

Key assumptions:
- Convenience is more important than product shape.

Status: Rejected for first direction. Useful only as a tactical bridge if architecture needs temporary CLI reuse behind a proper GUI boundary.

### Direction B: Profile-first GUI Launcher

Concept: Make the GUI the primary BML app surface. It manages Barony install detection, profiles, enabled mods, launch readiness, runtime manifest generation, launch, and report viewing. Native hooks remain platform-specific runtime payloads behind the GUI.

Why it might be right:
- Matches the BML docs: standalone app owns profiles, packages, validation, launch, and logs; engine runtime owns gameplay.
- Directly solves the `.tmp` and env-var launch problem.
- Gives users a trustworthy place to enable/disable mods and see compatibility/readiness before launch.
- Preserves platform-specific product launcher names: `BaronyModLoader` and `BaronyModLoader.exe`.

Why it might be wrong:
- Larger scope than a thin wrapper.
- Requires architecture discipline to separate app GUI, package/profile state, platform launcher, and engine runtime.
- Needs later architecture decisions on GUI tech, persistence, launcher packaging, and validation flow.

Evidence:
- `framework/BaronyModLoader/README.md` defines a standalone loader app that discovers installs, manages profiles/packages, validates, launches, and owns logs.
- `framework/BaronyModLoader/loader-runtime-contract.md` says the app writes runtime manifests and launches Barony with hook environment.
- User explicitly wants the mod loader to load a mod manager GUI.
- User explicitly selected “B full send.”

Key assumptions:
- A local/profile/package-focused GUI is enough for the first product direction.
- Remote package browsing and full ecosystem features can be deferred without undermining the GUI-first direction.
- GUI technology choice can be deferred to `/architecture`.

Status: **Selected**.

### Direction C: Platform Launcher Shell First, GUI Later

Concept: Establish correct platform launcher artifacts first, such as `BaronyModLoader` and `BaronyModLoader.exe`, while deferring real mod manager UX.

Why it might be right:
- Directly addresses product naming and launcher layout.
- Establishes cross-platform product boundary.
- Reduces immediate GUI scope.

Why it might be wrong:
- User asked for a mod manager GUI, not only a renamed launcher.
- Could become another command wrapper with better branding.
- Does not solve mod enable/disable, profile, or readiness UX soon enough.

Evidence:
- Native repo already has Linux verified and Windows scaffold-only boundaries.
- User wants platform-specific folders and launchers.

Status: Rejected as standalone direction. Preserved as a requirement within Direction B.

### Direction D: Full Mod Manager Ecosystem Upfront

Concept: Build BML as a full ecosystem manager immediately: package browser, dependency resolver, profile editor, conflict UI, diagnostics, remote package source, Workshop-style metadata, runtime registry, and multiple platform release flows.

Why it might be right:
- Most complete expression of a mod manager.
- Aligns with long-term ecosystem ambitions.

Why it might be wrong:
- Too broad before architecture and runtime maturity.
- High scope risk.
- Could distract from proving the first trustworthy GUI launch loop.

Evidence:
- BML docs anticipate package stores, dependencies, profiles, runtime registry, and diagnostics.

Status: Rejected for first direction. Preserved as a later expansion path after the profile-first GUI is real.

### Direction E: Hybrid Staged GUI-first Launcher

Concept: Select the GUI-first direction but stage implementation around local/dev packages, stable profiles, launch readiness, and reports before richer ecosystem features.

Why it might be right:
- Combines B’s product direction with stronger scope control.
- Keeps `.tmp` as test evidence only.
- Preserves future ecosystem path.

Why it might be wrong:
- User selected B “full send,” implying the solution direction should not be diluted into a half-GUI launcher.

Status: Preserved as a possible architecture sequencing tactic, but not the selected solution direction. The selected direction is B; architecture may still stage delivery without changing the product direction.

## Evidence and Assumptions

### Evidence Ledger

| Claim | Evidence | Source | Confidence | Update Condition |
|---|---|---|---|---|
| BML is intended to be a standalone loader app paired with engine runtime. | README defines standalone loader app and engine runtime halves. | `framework/BaronyModLoader/README.md` | High | Would change if product docs are superseded. |
| The app owns package/profile activation and runtime manifest generation. | Runtime contract assigns install discovery, package verification, profile activation, manifest writing, launch, and logs to the app. | `framework/BaronyModLoader/loader-runtime-contract.md` | High | Would change if runtime is redesigned to scan packages directly, currently disallowed. |
| GUI-first direction is a user requirement, not only agent preference. | User said the mod loader should “actually load a mod manager gui” and selected “B full send.” | User conversation | High | Would change only if user revises the selected direction. |
| Platform-specific launcher naming matters. | User challenged internal `bml-win-launcher.exe` naming with “BaronyModLoader.exe???” | User conversation | High | Would change only if user chooses internal/dev naming. |
| Linux is verified now; Windows must remain product-shaped but fail-closed until verified. | Native README and Makefile state Linux is verified and Windows is scaffold-only/fail-closed. | `native/barony-modloader-hook/README.md`, `Makefile` | High | Would change after live Windows artifacts and verification. |
| `.tmp` should not be the normal launch path. | User questioned temp usage; `.tmp` contains validation scratch artifacts. | User conversation and repo evidence | High | Would change only for disposable test runs. |
| GUI supports trust/adoption, not just convenience. | Runebound discovery stresses stability/no crashes, co-op compatibility, Workshop-friendly adoption, clear boundaries, and low interaction burden. | `.agents/discovery/2026-07-05-runebound.md` | Medium-High | Would change if BML GUI is scoped as developer-only. |

### Assumption Ledger

| Assumption | Why It Matters | Status | Decision Impact |
|---|---|---|---|
| First GUI can focus on local/dev packages, profiles, launch readiness, and reports before remote package browsing. | Keeps first direction practical while still being a real mod manager. | Open for architecture | Architecture must decide initial feature boundary without reducing product direction to a thin wrapper. |
| GUI technology choice can be deferred to architecture. | `/solution` should choose product direction, not implementation framework. | Accepted for now | Architecture must compare GUI tech using repo/runtime constraints. |
| Windows should be represented in product structure but not claimed playable yet. | Preserves user-facing `BaronyModLoader.exe` direction without false support claims. | Supported by repo evidence | Architecture must keep Windows fail-closed until verified. |
| Existing CLI/package/profile logic can inform the GUI but should not define the user experience. | Avoids wrapping dev commands as product. | Supported by selected direction | Architecture may reuse logic internally but must design around the GUI as primary app surface. |

## Probes / Spikes Performed or Deferred

No probe was performed during `/solution`.

Reason:
- The decision is direction-level, and the decisive evidence is already available: BML docs define a standalone app, user explicitly wants a GUI, and user selected Direction B.
- A GUI technology probe would be architecture work unless it changes whether Direction B is viable at all.

Deferred probe candidates for `/architecture` if needed:
- Verify the best GUI tech can launch the installed Barony process with the required environment on Linux.
- Verify packaged launcher behavior can keep hook/runtime paths stable after relocation.
- Verify Windows launcher can remain fail-closed while presenting user-readable GUI status.

## Tradeoff Comparison

| Criterion | A: Thin GUI wrapper | B: Profile-first GUI launcher | C: Launcher shell first | D: Full ecosystem upfront | E: Staged GUI-first hybrid |
|---|---|---|---|---|---|
| Outcome fit | Weak-Medium | Strong | Medium | Strong but oversized | Strong |
| User value | Quick convenience | Real mod manager | Better launch naming | Broad future value | Real mod manager with scope control |
| Feasibility | Highest | Plausible | High | Lowest | Plausible |
| Scope/appetite fit | Small but underpowered | Medium/Large | Medium but incomplete | Too large | Medium |
| Reversibility | Medium | Medium | High | Low | Medium-High |
| Key risk | Freezes dev flow as product | Scope/control | Delays GUI value | Scope explosion | Ambiguous between direction and sequencing |
| Confidence | Medium feasibility, Low direction fit | Medium-High | Medium | Low first-step fit | Medium-High |

Narrative:
- A wins only if speed matters more than product shape.
- C wins only if launcher packaging is more urgent than mod manager UX.
- D wins only if the project wants to invest in the entire ecosystem now.
- E is a strong staged delivery tactic, but user selected B as the direction.
- B best matches the user’s stated intent and the BML docs: the GUI becomes the actual mod manager/launcher surface, not a decoration over dev commands.

## User Direction Decision

### Selected Direction

**Direction B: Profile-first GUI launcher.**

BaronyModLoader should become a GUI-first app that manages profiles, enabled mods, launch readiness, runtime manifest generation, modded Barony launch, and diagnostics. Platform-specific launchers and hook payloads sit behind that product surface.

### Why This Direction

User rationale:
- The mod loader should actually load a mod manager GUI.
- The user-facing Windows launcher should be `BaronyModLoader.exe`, not an internal `bml-win-launcher.exe`-style artifact.
- The current `.tmp` launch path is unacceptable as a normal product path.
- The user explicitly selected “B full send.”

Supporting evidence:
- BML docs already define the standalone app as the owner of profiles, package activation, launch configuration, validation, and logs.
- Runtime contract already requires app-written runtime manifests and app-owned launching.
- Runebound trust/adoption constraints make a GUI valuable because players need clear compatibility, failure, and mod-state feedback.

### Alternatives Preserved or Rejected

- A Thin GUI wrapper: rejected as too likely to preserve dev-flow problems.
- C Platform launcher shell first: rejected as standalone direction, preserved as a requirement within B.
- D Full ecosystem upfront: rejected for first direction as scope-heavy, preserved as long-term expansion.
- E Hybrid staged GUI-first: preserved as possible architecture sequencing, but not the product direction. The selected direction remains B.

### Confidence

**Medium-High.**

High confidence that B matches the user’s intent and existing BML docs. Medium uncertainty remains around GUI tech, packaging, and how much existing CLI/profile logic should be reused internally.

### Update Condition

Change or revisit this direction if architecture finds that:
- A profile-first GUI cannot reliably launch Barony with hook environment on the target OS.
- Packaging a GUI launcher breaks the runtime/provenance contract.
- The selected GUI approach cannot present fail-closed runtime status clearly.
- User revises scope toward developer-only tools or full ecosystem manager upfront.

## Boundaries and Non-goals

The selected direction does not include:

- Remote mod marketplace or package registry as a first-direction requirement.
- Full Workshop publishing UI.
- Arbitrary native plugin loading.
- Lua/WASM/general scripting runtime.
- Pixel-perfect visual mocks.
- GUI technology selection.
- API/schema/database/file-level implementation.
- Claiming Windows playable support before live Windows verification.

Architecture should not silently add:
- A full package ecosystem.
- A public mod browser.
- General native plugin support.
- Runtime package scanning inside the game process.
- In-place patching of the retail executable.

## Risks and Open Questions

### Blocking Before Architecture

None. The selected direction is clear enough for architecture.

### Architecture Handoff Questions

Architecture must decide:
- What GUI technology best fits Linux now and Windows later?
- How does the GUI discover Barony installs and supported runtimes?
- How does the GUI represent profiles, enabled mods, launch readiness, and reports?
- How does the GUI generate runtime manifests without letting the engine runtime scan arbitrary packages?
- How does the product layout separate stable profiles from disposable `.tmp` evidence?
- How do `BaronyModLoader` and `BaronyModLoader.exe` relate to the GUI app and platform hook payloads?
- How does Windows remain visible in product shape while fail-closed until verified?
- What is the minimum first GUI surface that still counts as a real mod manager, not a wrapper?

### Later Questions

- Remote package browsing.
- Workshop publishing integration.
- Dependency/conflict visualization beyond first local packages.
- Multiplayer compatibility UX beyond core readiness/status.
- Visual design/mocks.

## Architecture Handoff Contract

Architecture should use this brief to plan implementation, but must not re-open the approved problem framing or user-selected solution direction without new evidence or user approval.

Architecture must preserve:
- GUI-first BaronyModLoader as the primary user surface.
- Profile-first mod activation and launch readiness.
- Stable user/development profile paths, not `.tmp` as normal launch path.
- Platform-specific launcher naming and product shape:
  ```text
  BaronyModLoader/
  ├── linux/
  │   └── BaronyModLoader
  └── windows/
      └── BaronyModLoader.exe
  ```
- App/runtime separation: GUI/app owns package/profile/manifest/launch/logs; engine runtime owns gameplay hooks.
- Linux verified now, Windows visible but fail-closed until live verification.
- Fail-closed compatibility and clear diagnostics.

Architecture must decide:
- GUI tech and packaging.
- First GUI scope boundary.
- Profile/package store mechanics.
- Launch flow and report collection.
- Platform launcher/runtime payload layout.

Architecture must not assume:
- The first GUI includes a remote package marketplace.
- Windows is playable just because `BaronyModLoader.exe` is part of the product shape.
- Existing CLI/dev commands are the final UX.
- `.tmp` is acceptable as a normal profile location.
- The engine runtime should scan mods directly.

## Approval

User-approved direction:
- [ ] Approved
- [ ] Needs revision

Approval notes:
