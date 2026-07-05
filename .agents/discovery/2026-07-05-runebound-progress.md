# Discovery Progress: Runebound

## Goal
Discover the problem domain for Runebound: a Barony loot/progression mod concept aimed at making item discovery, progression, and run-to-run rewards feel more meaningful. Intended outcome is an approved problem frame and problem-derived capability needs for a later `/architecture` handoff, without prescribing UI, technical architecture, schemas, implementation phases, or code.

## Scope
### In Scope
- Barony players and co-op groups who care about loot progression, replayability, build identity, reward clarity, and item handoff/fit moments.
- Jobs, motivations, contexts, workarounds, frustrations, and desired outcomes around dungeon loot, item progression, and co-op coordination around drops.
- Trust, adoption, and interaction burden for installing and relying on a gameplay-altering mod, especially stability, compatibility, multiplayer matching, and Workshop clarity.
- Market/adjacent evidence from Barony, roguelikes, ARPGs, modding communities, and Path-of-Melvor-like loot systems.
- Problem-derived capability needs that later architecture work can satisfy.
- Optional game-design suggestion to preserve for later: OSRS-style material progression vocabulary — Bronze, Iron, Steel, Mithril, Adamantite, Runite — because it may create familiar fantasy and expand progression beyond base Barony.

### Out of Scope
- UI, screens, components, layouts, visual systems, or mocks.
- Technical architecture, system diagrams, service boundaries, data flow, or infrastructure.
- APIs, schemas, database models, events, queues, integration contracts, code, tests, migrations, or rollout plans.
- Final game balance numbers, affix tables, implementation phases, or native hook designs.
- Exact balance numbers, stat curves, drop rates, implementation mechanics, final affix tables, or whether the optional OSRS-style material ladder should replace existing Barony materials.

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
| 2026-07-05 | Discovery seed approved: Runebound should explore the problem behind a Barony loot/progression mod before solution design. | User confirmation in chat | Created progress document and began Phase 0 context gathering. |
| 2026-07-05 | Primary job/persona frame accepted enough to move forward: Runebound should create run-shaping item drops that support build pivots, discovery/surprise, strategic tradeoffs, power spikes, and co-op “you take this” moments. | User answers during jobs/personas phase | Enables ODI outcome prioritization. |
| 2026-07-05 | Updated the reusable `/discovery` prompt to collect ODI importance/satisfaction ratings through the `ask` tool when available. | User requested this method be written back into `/discovery`; edited `/home/jerry/.config/opencode/commands/discovery.md`. | Future discovery sessions should use structured `ask` prompts for outcome ratings. |
| 2026-07-05 | User completed ODI ratings through the `ask` tool. | `ask` tool response | Enables opportunity mapping and approval gate. |
| 2026-07-05 | Opportunity priority approved: Primary O6 meaningful tradeoffs, O1 plan-changing drops, O3 “this changes everything” moments; secondary O4 build pivots; O2 readability as guardrail; O5/O7/O8 lower-priority constraints or secondary outcomes. | User selected “Approve recommended priority” in `ask` tool. | Discovery can proceed to market/adoption/scope constraints. |
| 2026-07-05 | Hard constraints selected: stability/no crashes, preserve Barony feel, avoid inventory bloat, co-op compatibility, Workshop-friendly adoption. | User selected all listed hard constraints in `ask` tool. | Scope must treat stability, readability, and co-op/Workshop trust as constraints. |
| 2026-07-05 | Co-op should be equally central, not secondary. | User selected “Make co-op equally central” in `ask` tool. | Personas/scenarios/capability needs must include both solo run identity and co-op item handoff/fit. |
| 2026-07-05 | User introduced OSRS-style material tiers: Bronze, Iron, Steel, Mithril, Adamantite, Runite. | User scope-gate response. | Required clarification whether this was a problem-domain need, optional note, or hard product/content constraint. |
| 2026-07-05 | OSRS-style material vocabulary is a suggestion, not a hard constraint. | User corrected the earlier `ask` interpretation: “osrs materials is a suggestion, not a hard constraint.” | Discovery should preserve OSRS metals as an optional game-design note while keeping capability needs problem-derived. |
| 2026-07-05 | Corrected scope approved: co-op equally central; hard constraints are stability/no crashes, preserve Barony feel, avoid inventory bloat, co-op compatibility, and Workshop-friendly adoption; OSRS materials are an optional suggestion. | User selected “Approve corrected scope” in `ask` tool. | Discovery can proceed to final document synthesis. |
| 2026-07-05 | Co-op item-fit/handoff is a central constraint, not a top outcome. | User resolved O7 consistency check through `ask` tool. | Final document should support co-op as a first-class constraint while keeping top opportunity focused on meaningful tradeoffs and plan-changing drops. |
| 2026-07-05 | Final discovery approved complete. | User said “discovery is complete.” | Discovery phase is closed; next process question is defining a divergent solution-spiking step before architecture. |

## Open Questions
| Question | Owner | Needed For | Status |
|---|---|---|---|
| Who specifically has this problem? | User | Personas, jobs, adoption constraints | Answered: primary initial user is the user personally. |
| What triggered interest in Runebound now? | User | Evidence weighting and opportunity framing | Answered: user plays ARPGs/MMOs with better loot systems than Barony. |
| What does success look like if this problem is solved? | User | Desired outcomes and opportunity prioritization | Answered: varied/exciting items and runs that feel materially different based on drops, comparable in run-shaping impact to Slay the Spire relics or Binding of Isaac items. |
| What emotional reaction should a good item create? | User | Emotional jobs and success criteria | Answered: “holy shit, this changes everything” and “I’m clever for building around this.” |
| What should players want to say or coordinate around socially/co-op when a notable item drops? | User | Social jobs and co-op scenario framing | Answered: “you take this” is a great social outcome. |
| Should the prioritized underserved outcomes be O6, O1, and O3 as primary, with O4 as a secondary stretch and O2 as a readability/table-stakes guardrail? | User | Opportunity approval gate | Answered: approved recommended priority via `ask` tool. |
| Is the OSRS metals idea mainly about familiar fantasy/progression readability, replacing Barony’s material ladder, or both? | User | Scope approval and capability requirement wording | Answered: familiar fantasy and expanded progression are the motivation, but OSRS materials are a suggestion, not a hard constraint. |
| Is the corrected scope approved? | User | Scope approval gate | Answered: approved corrected scope via `ask` tool. |
| Should co-op item handoff be upgraded to a primary outcome after co-op was made central? | User | Consistency check before final synthesis | Answered: central constraint, not top outcome. |

## Evidence Ledger
| Claim | Evidence | Source | Confidence |
|---|---|---|---|
| Runebound discovery is about Barony loot/progression, not UI or architecture. | Seed interpretation confirmed by user. | User chat, 2026-07-05 | High |
| The current session should avoid prescribing implementation details during discovery. | `/discovery` command instructions explicitly constrain scope to problem domain. | User command, 2026-07-05 | High |
| Initial target user is the user as a Barony player seeking richer loot progression. | User answered “me” when asked who has the problem. | User chat, 2026-07-05 | High |
| Runebound is motivated by ARPG/MMO loot systems that make loot feel more varied and exciting than Barony’s current item progression. | User said they play other ARPGs/MMOs with better loot systems. | User chat, 2026-07-05 | High |
| Success means drops materially change how a run feels, similar to the run-shaping impact of Slay the Spire relics or Binding of Isaac items. | User explicitly named varied/exciting items and materially different runs, citing those games. | User chat, 2026-07-05 | High |
| Barony already has meaningful item properties, but its variability appears mostly bounded by item tier/type, quality, blessing/curse, durability, appraisal, and artifacts rather than per-instance build-defining modifier combinations. | Research found Barony item quality states, blessed/cursed states, appraisal, durability, and artifact behavior; user motivation implies these do not satisfy the desired ARPG/MMO loot feeling. | Web research: Barony wiki/search results, 2026-07-05; User chat, 2026-07-05 | Medium |
| Adjacent roguelike benchmarks create materially different runs through permanent or semi-permanent item/relic effects that force adaptation. | Slay the Spire relic research emphasized strategy-enabling permanent effects; Binding of Isaac research emphasized item synergies and unexpected combinations. | Web research: Slay the Spire relics and Binding of Isaac item synergies, 2026-07-05 | Medium |
| ARPG/MMO loot systems warn against solving “more exciting loot” by simply adding more drops or more stats. | Research found common complaints around excessive loot volume, affix bloat, unclear upgrades, inventory fatigue, and trade/crafting replacing the fantasy of finding good loot. | Web research: Diablo/Path of Exile/Last Epoch/MMO itemization complaints, 2026-07-05 | Medium |
| Existing Barony Workshop loot-adjacent mods appear to randomize item pools or add content, but that is not the same as run-shaping per-item identity. | Research found Item Randomizer-style mods and content mods; these broaden availability/content rather than clearly targeting build-defining drop identity. | Web research: Barony Steam Workshop modding, 2026-07-05 | Low |
| The functional job should emphasize build pivoting, discovery/surprise, strategic tradeoff, and occasional power fantasy from upgrades. | User selected these effects when asked what a good item drop should make the player do differently. | User chat, 2026-07-05 | High |
| Current workaround is switching Barony classes and races; desired emotional/social jobs are “this changes everything,” “I’m clever for building around this,” and co-op item handoff/fit moments. | User clarified current workaround and later selected emotional/social outcomes. | User chat, 2026-07-05 | High |
| Primary behavioral persona can be framed as a Run-Identity Seeker: a Barony player who wants drops to create tactical adaptation and run identity beyond class/race selection. | Derived from user’s stated trigger, desired success, functional jobs, emotional jobs, social job, and current workaround. | User chat + synthesis, 2026-07-05 | Medium |
| ODI ratings show strongest underserved outcomes around meaningful tradeoffs, plan-changing drops, and “this changes everything” moments. | Ratings: O6 10/3, O1 8/3, O3 8/3. | `ask` tool, 2026-07-05 | High |
| Build-pivot availability is underserved but less important than the top cluster. | Rating: O4 5/1. | `ask` tool, 2026-07-05 | High |
| Fast evaluation is important but mostly served today, so it is a guardrail rather than a primary opportunity. | Rating: O2 8/7. | `ask` tool, 2026-07-05 | High |
| Inventory burden, co-op handoff moments, and avoiding added complexity are lower-importance in this seed, though they remain constraints or secondary outcomes. | Ratings: O5 3/7, O7 3/5, O8 3/7. | `ask` tool, 2026-07-05 | High |
| Opportunity priority was explicitly approved by the user. | User selected “Approve recommended priority” for the outcome map. | `ask` tool, 2026-07-05 | High |
| Barony Workshop alternatives suggest whitespace between broad item-pool randomization/content expansion and targeted run-shaping item identity. | Item Randomizer-style mods broaden possible drops, while Wicked Rendition-style mods add content/classes; neither evidence source clearly addresses the approved top outcomes directly. | Web research: Barony Workshop mods, 2026-07-05 | Medium |
| Trust/adoption burden is material because gameplay mods can crash, break after game updates, conflict with other mods, or require all multiplayer participants to match mods. | Research found common Workshop mod risks around crashes, game updates, load/order conflicts, multiplayer synchronization, and troubleshooting. User previously encountered a Barony critical error during mod work. | Web research: Steam Workshop mod trust/adoption, 2026-07-05; user screenshot/context | High |
| The market/design evidence supports run-defining item effects but warns against affix bloat, unclear upgrades, and excess sorting. | Roguelike item design research emphasized run-defining items, tradeoffs, synergies, agency/randomness balance; ARPG research emphasized meaningful tradeoffs and loot clarity. | Web research: roguelike item design and ARPG itemization, 2026-07-05 | Medium |
| Co-op is equally central to Runebound’s problem frame. | User selected “Make co-op equally central,” revising the prior secondary-social assumption. | `ask` tool, 2026-07-05 | High |
| OSRS-style material vocabulary is an optional suggestion, not a hard product/content requirement. | User corrected the earlier interpretation and said “osrs materials is a suggestion, not a hard constraint.” | User correction, 2026-07-05 | High |
| Hard constraints are stability/no crashes, preserving Barony feel, avoiding inventory bloat, co-op compatibility, and Workshop-friendly adoption. | User selected all hard constraints offered in `ask` tool. | `ask` tool, 2026-07-05 | High |
| Corrected scope was explicitly approved. | User selected “Approve corrected scope” after the OSRS hard-constraint misclassification was fixed. | `ask` tool, 2026-07-05 | High |
| Co-op item handoff/build-fit should be treated as a central constraint rather than a top-ranked opportunity outcome. | User selected “Central constraint, not top outcome” when asked to resolve the tension between O7’s low rating and co-op being central. | `ask` tool, 2026-07-05 | High |
| Draft discovery document was written for review. | Created `.agents/discovery/2026-07-05-runebound.md` and verified the readable output. | Local file verification, 2026-07-05 | High |
| Workshop/playable claims for Runebound: Elixirs must stay fail-closed until live gates pass. | Architecture handoff standard says not to update playable or Workshop support claims until solo drop/use/save, multiplayer present-class pool, party-size gating, and mismatch rejection are verified against a live installed-executable path. | `.agents/architecture/2026-07-05-runebound-elixirs-architecture-plan.md:17-18`, `.agents/architecture/2026-07-05-runebound-elixirs-architecture-plan.md:207-213`, `.agents/architecture/2026-07-05-runebound-elixirs-architecture-plan.md:611-616` | High |

## Verification Standard
- Do not update Workshop/playable claims for Runebound: Elixirs until these live gates pass: solo elixir drop/use/save, multiplayer present-class drop pool, party-size drop eligibility, and multiplayer mismatch rejection.

## /architecture Handoff Readiness
- [x] Problem statement approved
- [x] Primary users and jobs approved
- [x] Underserved outcomes prioritized
- [x] Constraints and non-goals explicit
- [x] Capability requirements traced to jobs/outcomes
- [x] Open questions labeled as blocking or non-blocking
- [x] No UI, architecture, API, schema, or implementation plan prescribed
