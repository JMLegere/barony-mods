# Solution Progress: BML Compatible Modlist Launch

## Goal
Choose a user-approved solution direction for satisfying the approved BML pre-launch compatible multi-mod launch requirements: enabled mods, deterministic load order, issue visibility, compatibility defined as no error-level issues, and launch gating that blocks only errors while preserving warning visibility. The intended decision outcome is a Solution Direction Brief that can feed `/architecture` without prescribing implementation details.

## Source Inputs
| Source | Path / Link | Status | Notes |
|---|---|---|---|
| BML profile launch requirements discovery | `.agents/discovery/2026-07-07-bml-profile-launch-requirements.md` | Approved / authoritative | Defines narrow pre-launch compatible multi-mod launch requirements. |
| BML profile launch requirements progress | `.agents/discovery/2026-07-07-bml-profile-launch-requirements-progress.md` | Approved / authoritative | Confirms final discovery approval and architecture handoff readiness. |
| Prior BML GUI solution | `.agents/solutions/2026-07-06-bml-gui-mod-manager.md` | Context only | Establishes GUI-first/profile-first BML product direction; not a substitute for this narrower solution decision. |
| Prior BML GUI solution progress | `.agents/solutions/2026-07-06-bml-gui-mod-manager-progress.md` | Context only | Records selected profile-first GUI direction and deferred architecture questions. |
| Related Runebound discovery | `.agents/discovery/2026-07-05-runebound.md` | Context only | Provides trust/adoption background around stability, co-op compatibility, and Workshop-friendly behavior. |
| Related Runebound architecture plan | `.agents/architecture/2026-07-05-runebound-elixirs-architecture-plan.md` | Context only | Provides existing BML implementation-context signals; not authoritative for this solution direction. |

## Scope
### In Scope
- Solution directions for how BML should provide pre-launch compatible multi-mod launch confidence.
- Candidate direction concepts, tradeoffs, assumptions, and risks.
- Deciding whether a probe/spike is needed before `/architecture`.
- User-selected direction and architecture handoff constraints.

### Out of Scope
- Problem rediscovery beyond reopening `/discovery` if needed.
- Visual mockups, UI layouts, screen designs, icons, or production interaction design.
- Technical architecture diagrams, API contracts, schemas, database models, service boundaries, queues, deployment topology, or file-level plans.
- Implementation steps, code patches, tests, migrations, rollout plans, or project plans.
- Current-session loaded-mod truth, post-launch diagnostics, missing-effect diagnosis, session sharing/export, and multiplayer synchronization unless the user reopens scope.

## Phase Status
- [x] Input/source approval
- [x] Decision frame approval
- [x] Candidate solution set approval
- [x] Evidence and assumptions review
- [x] Probe/spike decision
- [x] Tradeoff comparison approval
- [x] User direction decision
- [x] Final Solution Direction Brief approval
- [x] /architecture handoff readiness

## Candidate Directions
| ID | Direction | Status | Confidence | Notes |
|---|---|---|---|---|
| A | Minimum Known-Error Gate | Rejected as primary / fallback preserved | Medium | Too thin as the main direction; may under-serve explanation quality, but useful fallback for incomplete compatibility facts. |
| B | Declarative Compatibility Contract | Selected | Medium-High | User selected this direction; strong fit with BML package docs and app-owned validation responsibilities. |
| C | Curated Compatible Modlists | Rejected as primary / optional aid preserved | Low-Medium | Useful early for known packages, but weaker fit for “any compatible modlist.” |
| D | Full Automatic Resolver | Rejected for now / long-term option preserved | Low | Too broad and premature as first direction. |
| E | Progressive Hybrid Gate | Preserved as sequencing guidance under B | Medium-High | Architecture should stage B rather than turning it into a full resolver upfront. |

## Evidence Ledger
| Claim | Evidence | Source | Confidence | Update Condition |
|---|---|---|---|---|
| The approved problem is pre-launch compatible multi-mod launch confidence, not post-launch diagnostics. | Discovery explicitly narrows scope to enabled mods, deterministic load order, compatibility, severity, explanations, and pre-launch confidence. | `.agents/discovery/2026-07-07-bml-profile-launch-requirements.md` | High | Would change only if user reopens discovery scope. |
| Compatibility is defined as no error-level issues. | Discovery states compatibility means exactly no error-level issues. | `.agents/discovery/2026-07-07-bml-profile-launch-requirements.md` | High | Would change only if user changes gating policy. |
| Launch gating blocks errors and allows warnings/lower severity while making them visible/explainable. | Discovery states exact launch gating policy. | `.agents/discovery/2026-07-07-bml-profile-launch-requirements.md` | High | Would change only if user changes gating policy. |
| Existing BML product direction is profile-first GUI. | Prior solution selected profile-first GUI launcher as BML product direction. | `.agents/solutions/2026-07-06-bml-gui-mod-manager.md` | Medium-High | Would change if user revises the broader BML direction. |
| Adjacent mod manager evidence supports enabled/launched state separation, load order, dependencies, conflicts, profiles, and diagnostics. | Discovery evidence ledger summarizes r2modman/Vortex/Barony research. | `.agents/discovery/2026-07-07-bml-profile-launch-requirements-progress.md` | Medium | Would change with stronger Barony-specific ecosystem evidence. |
| Existing BML source docs already support a declarative package/validation direction. | README says the app owns installation, packages, versions, profiles, runtime provenance, hook lifecycle, launch, validation, and diagnostics; package format defines explicit dependencies/conflicts, capabilities, activation records, validation stages, and stable error model. | `framework/BaronyModLoader/README.md`, `framework/BaronyModLoader/package-format.md`, `framework/BaronyModLoader/loader-runtime-contract.md` | High | Would change only if product docs are superseded or architecture finds current docs infeasible. |

## Assumption Ledger
| Assumption | Why It Matters | Test / Probe | Status | Decision Impact |
|---|---|---|---|---|
| BML can make a useful direction decision before choosing exact metadata, schema, or validation architecture. | Keeps `/solution` from jumping to architecture. | Source docs already establish declarative package/validation intent; no additional probe needed now. | Supported | Allows direction selection without implementation detail. |
| A useful first direction may be staged, not full ecosystem-grade dependency solving. | Prevents scope explosion. | Preserve E as sequencing variant under or adjacent to B. | Supported | If B is selected, architecture should stage the contract rather than assume full resolver scope. |
| Warning visibility can be direction-level without deciding UI. | Keeps launch gating requirement implementation-neutral. | Translate warning visibility into architecture handoff questions. | Supported | Architecture must decide expression, not `/solution`. |
| Package-declared compatibility facts can be the primary source of launchability truth. | This is the central bet in Direction B. | Architecture should validate current package model and identify minimum declarative facts needed. | Open for architecture | If false, B collapses toward A/E with more manual/known-error gating. |

## User Decision Log
| Date | Decision | Options Preserved / Rejected | Source | Impact |
|---|---|---|---|---|
| 2026-07-07 | Solution seed confirmed: decide a solution direction for BML pre-launch compatible multi-mod launch confidence. | Candidate directions not generated yet. | User answered “yes” to seed framing. | Allows decision frame and candidate direction generation. |
| 2026-07-07 | Decision frame approved: evaluate directions for pre-launch compatible multi-mod launch confidence without deciding architecture, UI, schemas, APIs, or implementation. | Candidate directions preserved for comparison. | User said “decision frame looks right.” | Allows candidate option-space approval gate. |
| 2026-07-07 | Candidate option space accepted with preference for Direction B: Declarative Compatibility Contract. | A/C/D/E preserved for comparison; B is not yet final until tradeoff decision is confirmed. | User said “i like b.” | Allows evidence/probe/tradeoff review focused on B while preserving alternatives. |
| 2026-07-07 | Selected Direction B: Declarative Compatibility Contract, with Direction E preserved as architecture sequencing guidance. | A rejected as primary but fallback preserved; C rejected as primary but optional aid preserved; D rejected for now but long-term option preserved; E preserved as sequencing guidance under B. | User answered “yes” to selection prompt. | Allows final Solution Direction Brief drafting. |

## Open Questions
| Question | Owner | Needed For | Status |
|---|---|---|---|
| Does the decision frame correctly separate solution direction from architecture and implementation? | User | Decision frame approval | Answered: approved. |
| Are there candidate directions missing before comparison? | User | Candidate set approval | Answered: no missing direction stated; user prefers B. |
| Is a read-only probe needed before choosing direction? | User / assistant | Probe decision | Recommendation: no new probe before direction decision; existing source docs are sufficient for direction-level choice. |

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
