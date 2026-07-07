# Discovery Progress: BML Profile Launch Requirements

## Goal
Define the problem-domain requirements for BaronyModLoader compatible multi-mod launch: what makes a modlist launchable, what issue severity blocks launch, what users need to know before launch, and what capability needs architecture must later satisfy. Corrected scope: this pass is pre-launch multi-mod compatibility only, not current-session truth, post-launch diagnosis, or local dev/test workflow discovery.

## Scope
### In Scope
Compatible multi-mod launch requirements: enabled mods, deterministic load order, compatibility, issue severity, non-launchable explanations, and pre-launch launchability confidence.
### Out of Scope
Current-session loaded-mod truth, post-launch diagnostics, diagnosing missing in-game mod effects, local BML user launch-confidence framing, local mod developer/tester workflows, session sharing/export codes, multiplayer synchronization, UI layouts, mocks, icons, screen/component design, technical architecture, APIs, schemas, implementation plans, code changes, tests, deployment, and rollout plans.

## Phase Status
- [x] Seed approval
- [x] Jobs and users
- [x] Opportunity approval
- [x] Personas and scenarios
- [x] Outcomes and constraints
- [x] Scope approval
- [x] Trust/adoption/interaction burden
- [x] Market and evidence review
- [x] Final discovery approval
- [x] /architecture handoff readiness

## Decision Log
| Date | Decision | Source | Impact |
|---|---|---|---|
| 2026-07-07 | Discovery seed approved: BML profile/launch requirements around active mod intent, launch target clarity, and trust that the intended mod loads. | User confirmation in chat | Start problem-domain discovery and maintain this control document. |
| 2026-07-07 | Seed answers approved: primary users are Barony mod users plus mod developers/testers; trigger is launch-confidence failure; success means auditable intended-vs-actual mod launch state. | User confirmation: “correct” | Proceed to Jobs and Users / JTBD discovery. |
| 2026-07-07 | User added explicit desired capability: launch any number of mods in a modlist. Translate as problem-domain need for trustworthy multi-mod composition, not an architecture design yet. | User interjection | Discovery scope must include modlist composition, compatibility/conflict confidence, and multi-mod launch intent. |
| 2026-07-07 | Multi-mod scope approved as “launch any compatible modlist, with clear explanation when the list is not launchable.” | User confirmation | Outcome discovery must include compatibility, conflicts, dependencies, load order, and diagnostic explainability. |
| 2026-07-07 | Core functional job accepted: assemble a compatible Barony modlist and launch/test the exact intended session. No current workaround exists. Most painful problem is not knowing which mods are loaded in the current session, especially during mod testing. | User confirmation | Persona/scenario discovery should center on mod testing and session-state confidence. |
| 2026-07-07 | Personas/scenario approved with addition: include a BML user as a relevant persona, not only the mod developer/tester. | User confirmation | Persona set should include Mod Tester/Developer, BML User/Player, and Shared Modlist/Multiplayer user. |
| 2026-07-07 | ODI ratings collected. Highest underserved outcomes: classify missing in-game mod effect (Essential/Not served), know exactly which mods are loaded (High/Not served), know pre-launch modlist (High/Poorly served), and explain non-launchable modlists (High/Poorly served). | User ratings via ask tool | Opportunity approval should prioritize launch/session observability and diagnostic classification over sharing/export and vanilla/BML separation. |
| 2026-07-07 | User clarified multi-mod loading itself must be part of the approved opportunity set, not merely a diagnostic edge case. | User statement: “also multi-mod loading” | Opportunity prioritization must include compatible multi-mod launch as a core underserved outcome. |
| 2026-07-07 | Opportunity set approved: compatible multi-mod loading, current-session loaded-mod truth, missing-effect diagnosis, pre-launch modlist certainty, and non-launchable modlist explanation. | User confirmation: “yes” | Continue to market/trust/scope discovery around multi-mod launch confidence. |
| 2026-07-07 | Launch gating policy approved: errors block launch; warnings and lower-severity issues allow launch. | User answer | Compatibility and capability requirements need severity classification. |
| 2026-07-07 | Compatibility definition approved: a modlist is compatible if it has no error-level issues. | User answer | Warning-level issues may remain launchable but must be visible/explainable. |
| 2026-07-07 | Session sharing/export is out of scope for now. | User answer | Discovery should treat sharing/export/multiplayer as non-goals or secondary future opportunities. |
| 2026-07-07 | Scope correction: multi-mod launch support is in scope for the current pass, not merely eventual/future. | User correction | Discovery must treat compatible multi-mod launch as a current-pass requirement. |
| 2026-07-07 | Intentional scope correction approved: current-session truth, post-launch missing-effect diagnosis, local BML user launch-confidence framing, and local mod developer/tester workflows are out of scope. Current pass focuses on pre-launch compatible multi-mod launch requirements. | User correction and confirmation | Rewrite scope around launchable modlists, load order, and issue severity only. |

## Open Questions
| Question | Owner | Needed For | Status |
|---|---|---|---|
| Who has this profile/launch confidence problem? | User | Jobs/personas | Superseded by intentional scope narrowing; broader BML user confidence framing is out of scope. |
| What triggered the interest in this requirements discovery? | User | Seed context and scope | Superseded by intentional scope narrowing; original Stash/Runebound issue remains evidence but current pass focuses pre-launch multi-mod launchability. |
| What does success look like if this is solved? | User | Outcomes and opportunity approval | Superseded by intentional scope narrowing; success now means compatible multi-mod modlists are launchable pre-launch with enabled mods, load order, and issues visible. |
| What does “any number of mods in a modlist” mean in practice: literally unbounded, or any compatible set with clear conflict/dependency/load-order constraints? | User | Multi-mod outcomes and constraints | Answer approved: launch any compatible modlist, with clear explanation when the list is not launchable. |
| Who besides the mod developer/tester has this same no-workaround launch-state problem today? | User | Persona prioritization | Superseded by intentional scope narrowing; local BML user confidence and local dev/test workflows are out of scope. |
| What interaction burden is acceptable for resolving modlist issues before launch? | User | Trust/adoption and constraints | Answer approved: prevent launch on errors; allow launch when only warnings or lower-severity issues exist. |
| What counts as compatibility for a BML modlist? | User | Capability requirements and constraints | Answer approved: no error-level issues. |
| What should be explicitly out of scope for this discovery? | User | Scope approval | Answer approved: session sharing/export, multiplayer sync, current-session truth, missing-effect diagnosis, local BML user confidence, and local dev/test workflows are out of scope. |

## Evidence Ledger
| Claim | Evidence | Source | Confidence |
|---|---|---|---|
| Users can experience confusion about which mod BML launch actually targets when multiple packages are active or selected. | Recent live issue: Stash expected but Runebound-only launch artifacts were generated while both packages were active. | User report and inspected live profile artifacts during session | High |
| Discovery should stay problem-domain only and defer UI/architecture/code decisions. | User-provided /discovery instructions. | Current user message | High |
| Mod managers commonly separate installed mods, enabled profile mods, and the launch/deployment step; confusion arises when those states are not clearly aligned. | Vortex research: installed vs enabled vs deployed are distinct; mods appearing installed but not in-game usually means disabled or not deployed. | Web search results citing Nexus/Vortex support-style sources and Reddit/user language | Medium |
| r2modman sets user expectations that a selected profile plus “Start modded” launches all mods enabled for that profile, while “Start Vanilla” is clearly separate. | r2modman research: profiles store installed mods/settings, profile export/import, “Start modded” and “Start Vanilla” separation. | Web search results citing r2modman/Thunderstore-adjacent sources | Medium |
| The user wants BML to support launching any compatible modlist, with clear explanation when the list is not launchable. | Direct user statement and approved wording. | Current chat | High |
| Barony’s native modding flow is manual and load-order sensitive, and advanced mods often require duplicate installs/custom executables. | Barony modding research: manual Custom Content loading, load order, duplicate install workaround, update breakage, multiplayer mismatch risk. | Web search results citing Steam/community Barony modding sources | Medium |
| Classifying why a mod effect is missing is the most underserved outcome. | User rated “Minimize time to classify a missing in-game mod effect as profile, launch, runtime, compatibility, or mod behavior” as Essential / Not served. | ODI rating | High |
| Knowing exactly which mods are loaded in the current session is highly underserved. | User rated this outcome High / Not served. | ODI rating | High |
| Pre-launch certainty and non-launchable explanations are important and poorly served. | User rated both outcomes High / Poorly served. | ODI rating | High |
| Compatible multi-mod loading is a core opportunity, not a secondary profile-management feature. | User clarified “also multi-mod loading” after seeing the initial opportunity set. | Current chat | High |
| Vanilla-vs-BML launch separation matters but is comparatively better served. | User rated this High / Mostly served. | ODI rating | High |
| There is currently no acceptable workaround for knowing which BML mods are loaded in the current session during mod testing. | User statement: “there is no workaround” and “most painful is not knowing which mods are loaded in my current session and mod testing.” | Current chat | High |
| Adjacent mod ecosystems treat current-session logs/loaded-plugin lists as important diagnostics. | BepInEx research: console/log output identifies loaded plugins; mod managers often provide access to profile logs. | Web search results citing BepInEx/r2modman/Thunderstore-adjacent sources | Medium |
| Multi-mod Barony sessions add multiplayer/load-order risk. | Barony multiplayer research: exact same mods and load order are required to avoid join/desync/crash issues. | Web search results citing Barony community troubleshooting sources | Medium |
| General mod manager compatibility work centers on dependencies, conflicts, load order, profiles, modlists, and diagnostics. | Market research summary across Thunderstore/r2modman/Vortex-style tooling. | Web search results | Medium |
| The minimum trust proof for launch is enabled mods, their load order, and whether there are any issues. | User answer to trust question: “enabled mods and their load order, and whether there are any issues.” | Current chat | High |
| Launch should be prevented only for error-level issues; warning-level or lower issues should allow launch. | User answer: “prevent launch if there are errors, allow if only warnings or below.” | Current chat | High |
| A compatible BML modlist is one with no error-level issues. | User answer: “no errors.” | Current chat | High |
| Session sharing is out of scope for now. | User answer: “keep session sharing out of scope for now.” | Current chat | High |
| Compatible multi-mod launch is in scope for the current pass, not deferred future scope. | User correction to scope summary. | Current chat | High |
| Current-session loaded-mod truth and missing-effect diagnosis are intentionally out of scope for this discovery pass. | User explicitly moved local BML user confidence, local dev/test workflows, and missing-effect diagnosis out of scope, then confirmed this was intentional. | Current chat | High |

## /architecture Handoff Readiness
- [x] Problem statement approved
- [x] Primary users and jobs approved
- [x] Underserved outcomes prioritized
- [x] Constraints and non-goals explicit
- [x] Capability requirements traced to jobs/outcomes
- [x] Open questions labeled as blocking or non-blocking
- [x] No UI, architecture, API, schema, or implementation plan prescribed
