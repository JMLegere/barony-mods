# Solution Progress: BML GUI Mod Manager

## Goal
Choose a user-approved solution direction for moving BaronyModLoader from developer-oriented hook/CLI proof paths toward a real mod manager GUI and platform launcher shape. The intended decision outcome is a solution direction that can later feed `/architecture` without prescribing implementation details.

## Source Inputs
| Source | Path / Link | Status | Notes |
|---|---|---|---|
| User solution seed | Current conversation: BML GUI, `BaronyModLoader.exe`, Linux/Windows launcher structure | Approved | Primary source for the desired direction. |
| Runebound discovery | `.agents/discovery/2026-07-05-runebound.md` | Approved / secondary | Provides trust, adoption, stability, co-op, and Workshop-friendly constraints. |
| Runebound discovery progress | `.agents/discovery/2026-07-05-runebound-progress.md` | Approved / secondary | Confirms discovery completion and trust/adoption constraints. |
| BML framework README | `framework/BaronyModLoader/README.md` | Approved | Defines standalone loader app + engine runtime product split. |
| BML runtime contract | `framework/BaronyModLoader/loader-runtime-contract.md` | Approved | Defines app-owned package/profile/runtime manifest/launch/log responsibilities. |
| BML package format | `framework/BaronyModLoader/package-format.md` | Approved | Defines package/profile activation model that a GUI would expose. |
| Native hook state | `native/barony-modloader-hook/README.md`, `native/barony-modloader-hook/Makefile` | Approved | Shows Linux verified and Windows scaffold-only. |

## Scope
### In Scope
- Solution direction for a BML mod manager GUI.
- Relationship between GUI, platform launchers, profiles, packages, runtime manifests, reports, and hook payloads.
- Candidate directions, tradeoffs, assumptions, and decision criteria.
- Stable dev/product path direction replacing `.tmp` as the normal launch surface.

### Out of Scope
- Technical architecture diagrams.
- API contracts, schemas, database models, service boundaries, queues, deployment topology, or file-level plans.
- Implementation steps, code patches, tests, migrations, rollout plans, or project plans.
- Pixel-perfect UI, production visual design, or mockups.

## Phase Status
- [x] Input/source approval
- [x] Decision frame approval
- [x] Candidate solution set approval
- [x] Evidence and assumptions review
- [x] Probe/spike decision
- [x] Tradeoff comparison approval
- [x] User direction decision
- [ ] Final Solution Direction Brief approval
- [x] /architecture handoff readiness

## Candidate Directions
| ID | Direction | Status | Confidence | Notes |
|---|---|---|---|---|
| A | Wrapper GUI around existing CLI/dev flow | Rejected | Medium feasibility / Low direction fit | Too likely to preserve dev-flow problems. |
| B | Profile-first GUI launcher | Selected | Medium-High | User selected “B full send.” GUI owns profiles, package activation, runtime manifest generation, launch, and diagnostics while keeping native runtime separate. |
| C | Platform shell first, GUI later | Rejected standalone / preserved inside B | Medium | Product launcher naming is required, but shell-only is not enough. |
| D | Full mod manager ecosystem upfront | Rejected for first direction / preserved later | Low first-step fit | Too much scope before architecture/runtime maturity. |
| E | Hybrid staged GUI-first launcher | Preserved as architecture sequencing tactic | Medium-High | Architecture may stage B, but selected product direction remains B. |

## Evidence Ledger
| Claim | Evidence | Source | Confidence | Update Condition |
|---|---|---|---|---|
| BML is intended to be a standalone loader app paired with engine runtime. | README states standalone loader app discovers installs, manages profiles/packages, validates, launches, and owns logs. | `framework/BaronyModLoader/README.md` | High | Would change if product docs are superseded. |
| The app, not the runtime, owns profile activation and runtime manifest generation. | Runtime contract assigns package verification, profile activation, launch manifest writing, and launching to the app. | `framework/BaronyModLoader/loader-runtime-contract.md` | High | Would change if engine runtime becomes package scanner, currently disallowed. |
| GUI-first direction is user-selected. | User said “i want the mod loader to actually load a mod manager gui” and chose “B full send.” | User conversation | High | Would change if user revises selected direction. |
| Platform launcher naming matters. | User challenged internal Windows launcher naming with “BaronyModLoader.exe???” | User conversation | High | Would change only if user accepts internal/dev naming. |
| Linux is currently verified; Windows is scaffold-only. | Native README and Makefile state Linux verified, Windows scaffold/fail-closed. | `native/barony-modloader-hook/README.md`, `Makefile` | High | Would change after live Windows artifacts and verification. |
| `.tmp` should not be the normal launch path. | User explicitly rejected temp path as normal surface; `.tmp` contains validation artifacts. | User conversation and repo artifact usage | High | Would change only if user chooses a disposable dev-only tool. |
| GUI matters for trust/adoption, not just convenience. | Discovery says stability/no crashes, co-op compatibility, Workshop-friendly adoption, clear boundaries are hard constraints. | `.agents/discovery/2026-07-05-runebound.md` | Medium-High | Would change if BML GUI target becomes developer-only. |

## Assumption Ledger
| Assumption | Why It Matters | Test / Probe | Status | Decision Impact |
|---|---|---|---|---|
| The first GUI can manage local/dev packages before remote package browsing. | Keeps scope bounded while still satisfying real GUI direction. | Architecture should decide initial feature boundary. | Open for architecture | Remote package browsing should not be silently included in first architecture. |
| Profile-first GUI provides more value than launcher-only shell. | User specifically wants mod manager GUI, not just executable wrapper. | User direction decision. | Confirmed by user selection | Selected Direction B. |
| GUI technology choice can wait until architecture. | `/solution` should choose direction, not implementation framework. | Architecture tech comparison/probe if needed. | Accepted | Prevents premature Tauri/Electron/native decision. |
| Windows should be represented in product shape but not claimed playable. | Keeps `BaronyModLoader.exe` naming while preserving verified support truth. | Source evidence from native README/Makefile. | Supported | Architecture must keep Windows fail-closed until verified. |

## User Decision Log
| Date | Decision | Options Preserved / Rejected | Source | Impact |
|---|---|---|---|---|
| 2026-07-06 | Solution seed confirmed: move toward BML GUI and platform launcher structure. | Preserves candidate comparison before final direction. | User said “correct” and “continue.” | Allows candidate direction generation. |
| 2026-07-06 | Selected Direction B: Profile-first GUI launcher. | A rejected; C rejected standalone but preserved inside B; D rejected for first direction; E preserved as architecture sequencing tactic. | User said “B full send.” | Final brief drafted for approval. |

## Open Questions
| Question | Owner | Needed For | Status |
|---|---|---|---|
| Should the selected direction be profile-first GUI, launcher-shell-first, or staged hybrid? | User | Direction decision | Answered: B full send. |
| Should remote package browsing / Workshop integration be in first direction or deferred? | Architecture/User | Scope boundary | Deferred to architecture; not assumed in solution. |
| Should GUI tech choice be deferred to architecture? | User / architecture | Handoff boundary | Deferred to architecture. |

## /architecture Handoff Readiness
- [x] Approved discovery source cited
- [x] Target opportunity/problem explicit
- [x] Multiple candidate directions considered, or rationale given for why not
- [x] All viable directions preserved until user decision
- [x] User-selected direction recorded
- [x] Confidence and update condition stated
- [x] Non-goals and boundaries explicit
- [x] Architecture questions listed
- [x] No API/schema/database/file-level implementation plan prescribed
