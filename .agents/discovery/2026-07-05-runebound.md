# Discovery: Runebound

## Problem Statement

Barony already has replayability through dungeon generation, class/race choice, appraisal, item quality, blessings/curses, and artifacts, but its ordinary item drops do not reliably create the feeling that a run has developed a distinct tactical identity. The initial target user is a Barony player who also plays ARPGs/MMOs/roguelites and wants drops to create moments like “holy shit, this changes everything” and “I’m clever for building around this.” The underserved opportunity is to make item discovery produce meaningful tradeoffs and plan-changing decisions without creating ARPG-style loot bloat, unreadable stat clutter, crashes, or Workshop/co-op adoption risk.

## Domain Boundary

### In Problem Domain

- Barony players and co-op groups who care about loot progression, replayability, build identity, reward clarity, and item handoff/fit moments.
- Jobs, motivations, contexts, workarounds, frustrations, and desired outcomes around dungeon loot, item progression, and co-op coordination around drops.
- Trust, adoption, and interaction burden for installing and relying on a gameplay-altering mod.
- Market/adjacent evidence from Barony, roguelikes, ARPGs, modding communities, and Path-of-Melvor-like loot systems.
- Problem-derived capability needs that later `/architecture` work can satisfy.
- Optional game-design suggestion for later: OSRS-style material vocabulary — Bronze, Iron, Steel, Mithril, Adamantite, Runite — as a possible way to create familiar fantasy and expanded progression beyond base Barony.

### Out of Problem Domain

- UI, screens, components, layouts, visual systems, or mocks.
- Technical architecture, system diagrams, service boundaries, data flow, infrastructure, APIs, schemas, databases, event contracts, queues, code, tests, deployment, migrations, or rollout plans.
- Final balance numbers, stat curves, drop rates, affix tables, implementation phases, native hook designs, or whether optional OSRS-style metals should replace existing Barony materials.

## Users & Personas

### Primary Persona: The Run-Identity Seeker

**Goals**
- End goal: have Barony runs feel materially different because of what drops during the run, not only because of starting class/race.
- Experience goal: feel surprise, tactical tension, and cleverness when adapting to a notable item.
- Life/identity goal: enjoy the expressive mastery of “I built around what the dungeon gave me.”

**Context**
- Plays Barony enough that class/race switching is the current workaround for variety.
- Has reference points from ARPGs, MMOs, Slay the Spire relics, and Binding of Isaac item synergies.
- Encounters the problem mid-run when loot drops but does not force a meaningful reconsideration of plan, role, weapon path, or co-op allocation.

**Behaviors**
- Switches classes/races to create variety.
- Notices when other games make itemization more exciting or run-defining.
- Wants powerful upgrades sometimes, but specifically approved meaningful tradeoffs and plan-changing surprise as the stronger opportunity.

**Pain Points**
- Loot can feel like incremental replacement rather than run identity.
- Drops do not often create “this changes everything” moments.
- Current variety leans heavily on pre-run choice rather than in-run discovery.
- More loot is not enough if it creates sorting, complexity, or unclear upgrade evaluation.

### Secondary Persona: The Co-op Build-Fit Partner

**Goals**
- Recognize when a drop is better for another player.
- Create “you take this” moments where item fit, role, and build direction matter.
- Trust that everyone in the session can understand, use, and depend on the modded loot system.

**Context**
- Plays in co-op sessions where gameplay-altering mods require shared trust and compatibility.
- Needs item changes to preserve Barony’s feel and avoid slowing down co-op decision-making.

**Pain Points**
- Co-op item handoff is a central constraint, but not the top opportunity outcome.
- Multiplayer mod mismatch, instability, and unclear item behavior would undermine adoption.

### Key Scenarios

**Scenario 1: The run-changing solo drop**

A player starts a Barony run with a familiar class/race combination. Mid-run, they find a notable item. Instead of asking only “is this number higher?”, they pause because the item creates a meaningful tradeoff: keeping the current setup is safe, but switching could reshape the rest of the run. The player feels both surprise and agency: “holy shit, this changes everything,” followed by “I’m clever for building around this.”

**Scenario 2: The co-op handoff**

A co-op group finds a notable item. One player could use it, but another player’s current role/build context makes the fit more interesting. The item prompts a quick social decision: “you take this.” The item does not need to be the top opportunity by itself, but the mod must preserve the conditions for this kind of trustable co-op handoff.

**Scenario 3: The bad version to avoid**

The dungeon drops many slightly different items. Players spend more time comparing marginal modifiers than playing. The mod creates clutter, confusion, or crashes. Even if the system is technically deeper, it fails the actual job because the itemization becomes homework rather than run identity.

## Jobs to Be Done

### Functional Jobs

- Create distinct run identity from discovered items.
- Make notable drops change the player’s plan for the run.
- Create meaningful tradeoffs between current setup and item-driven direction changes.
- Enable occasional power fantasy when a drop is a true upgrade, without making every notable item pure upside.
- Support co-op handoff decisions when an item is better for another player’s build/context.

### Emotional Jobs

- Feel “holy shit, this changes everything.”
- Feel clever for building around what dropped.
- Feel discovery/surprise rather than just incremental optimization.
- Sometimes feel powerful/lucky when an item is a genuine upgrade.

### Social Jobs

- Enable “you take this” co-op moments.
- Let players recognize item-fit for each other without bogging the group down.
- Preserve trust that everyone’s modded session is stable and compatible.

### Related Jobs

- Understand whether an item is worth building around.
- Preserve Barony’s feel while expanding item progression.
- Avoid importing ARPG/MMO baggage such as excessive loot volume, affix bloat, and inventory sorting fatigue.

### Current Workarounds

- Switch Barony classes/races to create run variety.
- Play other ARPGs/MMOs/roguelites that provide more exciting loot systems.
- Use or consider Barony Workshop mods that randomize item pools or add content, though these appear adjacent rather than directly solving run-shaping item identity.

## Desired Outcomes

ODI scale collected through the `ask` tool using ordinal mappings: Essential=10, High=8, Medium=5, Low=3, Not important=1; Already great=10, Mostly served=7, Mixed=5, Poorly served=3, Not served=1.

| ID | Outcome Statement | Importance | Current Satisfaction | Opportunity Read |
|---|---|---:|---:|---|
| O6 | Maximize meaningful tradeoffs between keeping the current setup and switching to a new item-driven direction. | 10 | 3 | Primary underserved outcome |
| O1 | Maximize the likelihood that a notable item drop changes the player’s plan for the run. | 8 | 3 | Primary underserved outcome |
| O3 | Maximize the frequency of “this changes everything” moments without making them routine. | 8 | 3 | Primary underserved outcome |
| O4 | Maximize the number of viable build pivots available from item drops during a run. | 5 | 1 | Secondary/stretch opportunity |
| O2 | Minimize the time it takes to understand whether a dropped item is worth building around. | 8 | 7 | Important guardrail/table-stakes, not primary whitespace |
| O7 | Maximize co-op moments where one player recognizes an item is better for another player’s build. | 3 | 5 | Central co-op constraint, not top-ranked outcome |
| O5 | Minimize inventory-sorting burden caused by evaluating many similar items. | 3 | 7 | Constraint/avoidance requirement |
| O8 | Minimize the risk that added loot complexity makes Barony harder to read, slower, or more tedious. | 3 | 7 | Constraint/avoidance requirement |

### Approved Opportunity Priority

Primary opportunity:
1. Meaningful tradeoffs.
2. Plan-changing drops.
3. “This changes everything” moments.

Secondary/stretch:
- More viable build pivots from item drops.

Guardrails and constraints:
- Fast readability.
- Low inventory burden.
- Preserve Barony feel.
- Stability/no crashes.
- Co-op compatibility and Workshop-friendly adoption.

## Opportunity Map

```mermaid
quadrantChart
  title Runebound Outcome Opportunity Map
  x-axis Low current satisfaction --> High current satisfaction
  y-axis Low importance --> High importance
  quadrant-1 Table stakes / maintain
  quadrant-2 Well-served high importance
  quadrant-3 Low priority
  quadrant-4 Underserved opportunity
  O6 Meaningful tradeoffs: [0.30, 1.00]
  O1 Plan-changing drops: [0.30, 0.80]
  O3 Changes-everything moments: [0.30, 0.80]
  O4 Build pivots: [0.10, 0.50]
  O2 Quick evaluation: [0.70, 0.80]
  O7 Co-op handoff: [0.50, 0.30]
  O5 Low sorting burden: [0.70, 0.30]
  O8 Low complexity burden: [0.70, 0.30]
```

## Trust, Adoption & Interaction Burden

### Trust Prerequisites

- The mod must not make Barony fail to launch or corrupt runs.
- Players must trust that the mod preserves Barony’s feel rather than turning it into Diablo/Path of Exile.
- Co-op participants must trust that the same loot rules and item behavior are present for the group.
- Workshop subscribers must understand what kind of gameplay change they are installing.

### Adoption Costs and Blockers

- Gameplay mods can create crashes, break after game updates, conflict with other mods, or create multiplayer mismatch.
- The user already encountered a Barony critical-error launch failure during mod work, making stability a concrete adoption concern rather than abstract polish.
- More item depth can create evaluation burden if it produces many similar items or unclear effects.

### Interaction Burden Constraints

- Runebound should not turn loot into spreadsheet-like sorting.
- Co-op item decisions should remain quick enough not to stall play.
- Notable items need to be understandable enough to support build decisions, even if the exact implementation is deferred.

### Proof / Control / Transparency / Reversibility Needs

- Players need confidence that the mod is compatible with the current game version.
- Players need clear boundaries around what the mod changes.
- Co-op groups need compatibility clarity.
- Future architecture should preserve fail-closed behavior and avoid overclaiming unsupported runtime behavior.

## Market Landscape

### Existing / Adjacent Solutions

| Solution / Reference | What It Serves | Gap Relative to Runebound |
|---|---|---|
| Base Barony itemization | Item tiers/materials, quality, blessing/curse, appraisal, artifacts, durability | Variation exists, but ordinary drops do not reliably create run-shaping identity or meaningful build tradeoffs. |
| Barony Item Randomizer-style mods | Broader item pools, unusual/random loot availability | Randomization broadens what can appear but does not necessarily create readable, meaningful, tradeoff-driven item identity. |
| Barony content/class expansion mods such as Wicked Rendition-style mods | More classes, areas, enemies, and items | Adds content/replayability, but not necessarily the specific job of item drops changing a run’s plan. |
| Slay the Spire relics | Permanent run-shaping effects that force adaptation | Strong benchmark for “run identity from rewards,” but card/deck context differs from Barony. |
| Binding of Isaac item system | Item synergies and surprising combinations | Strong benchmark for discovery/surprise, but can create chaos that may not preserve Barony feel. |
| ARPG/MMO loot systems | Excitement, rarity, affixes, long-term chase | Useful inspiration, but common failure modes include affix bloat, junk loot, unclear upgrades, and sorting fatigue. |

### Table Stakes

- Stability and version compatibility.
- Clear enough item evaluation.
- Preservation of Barony feel.
- Co-op compatibility/trust.
- Avoidance of inventory bloat.

### Differentiator / Whitespace

Runebound’s whitespace is not “more random loot” or “more items.” It is **Barony-native, run-shaping item identity**: notable drops that create meaningful tradeoffs, change plans, and occasionally create “this changes everything” moments while remaining stable, readable, and co-op-compatible.

## Constraints, Assumptions & Non-Goals

### Hard Constraints

- Stability / no crashes.
- Preserve Barony feel.
- Avoid inventory bloat.
- Co-op compatibility.
- Workshop-friendly adoption.

### Explicit Non-Goals

- Do not prescribe UI, architecture, APIs, schemas, code, tests, deployment, rollout, or native-hook design in discovery.
- Do not prescribe final affix tables, balance numbers, stat curves, drop rates, or exact item mechanics.
- Do not make OSRS-style materials a hard requirement; preserve them as an optional suggestion for later game-design work.
- Do not solve the problem by simply increasing drop volume.

### Assumptions

| Assumption | Risk | Status |
|---|---|---|
| The initial target user can stand in for a broader “Run-Identity Seeker” persona. | Medium | Accepted as starting point; needs more external player validation later. |
| Base Barony’s existing item variation is insufficient for the desired run-shaping feeling. | Medium | Supported by user input and research, but should be validated with more Barony player evidence. |
| Co-op should be central as a constraint, not the top-ranked outcome. | Low | Explicitly resolved by user. |
| ARPG/MMO systems are inspiration, not a template to copy. | Low | Supported by user goals and research warnings. |

## Problem-Derived Capability Requirements

These describe what the eventual solution must make possible, not how to build it.

| ID | Capability Need | Traced Job/Outcome | Evidence | Priority | Notes |
|---|---|---|---|---|---|
| CR1 | Make notable item drops capable of changing a player’s plan for the current run. | O1, O3, functional job: create run identity from discovered items | User approved plan-changing drops and “this changes everything” as primary opportunities. | High | Must not require a specific implementation model. |
| CR2 | Make item-driven direction changes involve meaningful tradeoffs rather than only obvious upgrades. | O6, emotional job: feel clever for building around this | O6 rated Essential/poorly served and approved as top opportunity. | High | Occasional power fantasy is allowed, but pure upside cannot be the only mode. |
| CR3 | Make the player able to recognize when an item is potentially build-defining quickly enough to keep play moving. | O2, guardrail: fast readability | O2 rated High/mostly served; approved as guardrail. | High | This is a readability/interaction burden need, not a UI prescription. |
| CR4 | Make item variety support multiple viable build pivots over time without requiring every run to pivot. | O4, functional job: build pivot | O4 is underserved but medium importance; approved as secondary/stretch. | Medium | Should support the possibility of pivots, not force constant churn. |
| CR5 | Make co-op item-fit decisions possible and trustworthy without making co-op the top opportunity. | Social job: “you take this”; co-op central constraint; O7 | User said “you take this” is a great social outcome and later resolved co-op as central constraint, not top outcome. | Medium-High | Must respect compatibility and shared understanding. |
| CR6 | Make the loot experience preserve Barony’s feel while expanding item progression fantasy. | Constraint: preserve Barony feel; optional OSRS material suggestion | User selected preserve Barony feel and suggested OSRS metals for familiar fantasy/expanded progression. | High | OSRS metals remain optional suggestion, not hard requirement. |
| CR7 | Make the loot system avoid inventory bloat and excessive evaluation burden. | O5/O8 constraints; ARPG failure-mode evidence | User selected avoid inventory bloat; research warns against affix bloat and sorting fatigue. | High | This is an avoidance capability/constraint. |
| CR8 | Make adoption safe enough for Workshop and co-op users to trust the mod. | Trust/adoption burden | User selected stability/no crashes, co-op compatibility, Workshop-friendly adoption; prior critical-error context makes this concrete. | High | Future architecture must treat stability and version compatibility as first-class. |

### Quality Gate Check

- Every capability traces to jobs/outcomes/constraints above.
- No capability prescribes UI, architecture, API, schema, file plan, code, tests, deployment, or rollout.
- OSRS materials are preserved only as an optional game-design input, not a hard requirement.
- Co-op is reflected as a central constraint/capability need without contradicting the approved opportunity ranking.

## Open Questions

| Question | Blocking for `/architecture`? | Notes |
|---|---|---|
| How broad is the target beyond the user personally — other Barony players, Workshop users, co-op groups, ARPG/MMO players? | Non-blocking initially | Architecture can start from the approved persona, but future validation should broaden evidence. |
| Which Barony item categories should be eligible for run-shaping identity? | Blocking for design/architecture detail | Discovery should not answer this; architecture/game design need it later. |
| How should optional OSRS-style materials relate to existing Barony material progression? | Non-blocking for problem frame; blocking for later game design | Explicitly optional suggestion, not a hard constraint. |
| What proof is enough to claim a gameplay-altering mod is safe for Workshop/co-op users? | Blocking for architecture/validation | Stability and compatibility are hard constraints. |

## Handoff to /architecture

### Approved Problem Frame

Runebound should make Barony item drops create run identity, meaningful tradeoffs, and plan-changing surprise while preserving Barony feel and avoiding loot bloat. Co-op is central as a trust/compatibility and item-handoff constraint, not the top-ranked opportunity outcome.

### Approved Capability Requirements

- CR1 through CR8 above.

### Constraints Architecture Must Respect

- Stability / no crashes.
- Preserve Barony feel.
- Avoid inventory bloat.
- Co-op compatibility.
- Workshop-friendly adoption.
- Do not overclaim unverified gameplay support.
- Keep OSRS-style materials as optional game-design input, not a hard requirement.

### Evidence Architecture Should Review

- Barony current item model and modding/runtime constraints.
- Existing Workshop approaches such as item randomizers and content/class expansion mods.
- Prior Runebound scaffold work and BML runtime constraints.
- Stability and launch-failure history from the Stash/Runebound mod work.

### Optional /mocks Inputs

- OSRS-style material vocabulary as a possible flavor/progression reference: Bronze, Iron, Steel, Mithril, Adamantite, Runite.
- “Holy shit, this changes everything” item reveal moments.
- Co-op “you take this” item handoff moments.

### Blocking Open Questions

- Which item categories and gameplay surfaces are eligible for run-shaping item identity?
- What validation standard proves the mod is safe enough for Workshop/co-op use?

### Non-Blocking Open Questions

- How much broader Workshop/player validation is needed beyond the user?
- Whether OSRS-style material vocabulary should be used, adapted, or discarded in later game design.

---

## Appendix A: Market Research

- Barony base itemization uses item materials/tiering, quality/durability, blessed/cursed states, appraisal, and artifacts. Source direction: [Barony Wiki - Items](https://baronygame.fandom.com/wiki/Items), [Barony Wiki - Appraisal](https://baronygame.fandom.com/wiki/Appraisal).
- Barony Workshop alternatives include Item Randomizer-style mods that broaden common loot pools and content/class expansion mods such as Wicked Rendition-style mods. Search evidence indicated these serve adjacent variety/content jobs but not necessarily the approved run-shaping tradeoff job. Source direction: [Steam Workshop - Barony](https://steamcommunity.com/app/371970/workshop/).
- Gameplay mods commonly carry adoption risks: crashes, game-update breakage, conflicts, multiplayer synchronization, and troubleshooting difficulty. Source direction: [Steam Workshop documentation](https://partner.steamgames.com/doc/features/workshop), plus web research into Workshop mod compatibility/player complaints.

## Appendix B: JTBD Research

User language captured during discovery:

- “me” — initial target user.
- “i play other arpgs and mmos that have better loot systems.”
- “varied and exciting items and runs that feel materially different based on the items that are dropped.”
- “like slay the spire relics, or binding of isaac.”
- Desired functional effects: “build pivot, discovery/surprise, and strategic tradeoff, sometimes power fantasy if the item is an upgrade.”
- Desired emotional jobs: “holy shit, this changes everything” and “i’m clever for building around this.”
- Desired social/co-op job: “you take this.”
- Current workaround: switching classes and races.

## Appendix C: Outcome Analysis

ODI ratings were collected through the `ask` tool after the `/discovery` command prompt was updated to prefer structured `ask` prompts for outcome ratings when available.

Raw mapped ratings:

- O1: High importance / Poorly served = 8 / 3.
- O2: High importance / Mostly served = 8 / 7.
- O3: High importance / Poorly served = 8 / 3.
- O4: Medium importance / Not served = 5 / 1.
- O5: Low importance / Mostly served = 3 / 7.
- O6: Essential importance / Poorly served = 10 / 3.
- O7: Low importance / Mixed = 3 / 5; later clarified as central constraint, not top outcome.
- O8: Low importance / Mostly served = 3 / 7.

Approved prioritization:

- Primary: O6, O1, O3.
- Secondary/stretch: O4.
- Guardrail/table-stakes: O2.
- Lower-priority constraints/secondary outcomes: O5, O7, O8, with O7 later clarified as central constraint.

## Appendix D: Competitive Analysis

- **Base Barony**: strong roguelike/co-op foundation and item properties; gap is ordinary drops not reliably creating run-shaping identity.
- **Item Randomizer-style Barony mods**: increase item availability/randomness; risk is chaos/game-breaking without meaningful tradeoff structure.
- **Content/class expansion Barony mods**: increase replayability through more content; different job from making item drops reshape current-run decisions.
- **Slay the Spire**: relics are a strong benchmark for persistent run-shaping reward effects. Source direction: [Slay the Spire Wiki - Relics](https://slay-the-spire.fandom.com/wiki/Relics).
- **Binding of Isaac**: item synergies are a strong benchmark for surprising combinatorial run identity. Source direction: [Binding of Isaac: Rebirth Wiki - Items](https://bindingofisaacrebirth.wiki.gg/wiki/Items).
- **ARPG/MMO loot systems**: provide excitement and progression fantasy, but common failure modes are affix bloat, junk drops, unclear upgrades, and sorting fatigue.

## Appendix E: Trust, Adoption & Interaction Burden Evidence

- User selected hard constraints: stability/no crashes, preserve Barony feel, avoid inventory bloat, co-op compatibility, Workshop-friendly adoption.
- User previously encountered a Barony critical-error launch failure during mod work, making stability a concrete adoption concern.
- Workshop/gameplay mod research shows recurring adoption risks: crashes, conflicts, game-update breakage, multiplayer mod mismatch, and unclear compatibility.
- Co-op is central as a compatibility and handoff constraint; final consistency check resolved it as “central constraint, not top outcome.”
