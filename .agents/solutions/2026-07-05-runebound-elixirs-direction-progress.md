# Solution Progress: Runebound Elixirs Direction

## Goal
Choose a solution direction for Runebound: how Barony item drops should create run identity through meaningful tradeoffs, plan-changing drops, and occasional “this changes everything” moments while preserving Barony feel, avoiding loot bloat, staying stable, and remaining co-op/Workshop-friendly.

## Source Inputs
| Source | Path / Link | Status | Notes |
|---|---|---|---|
| Approved discovery artifact | `.agents/discovery/2026-07-05-runebound.md` | Approved source | Discovery says final discovery is complete and approved; use as authoritative problem/opportunity frame. |
| Discovery control document | `.agents/discovery/2026-07-05-runebound-progress.md` | Approved source | Records seed, scope, ODI outcome ratings, constraints, OSRS correction, and final discovery approval. |
| Project config | `barony-mods.toml` | Context only | Confirms this repo is for Barony Workshop mods, app id `371970`, hidden default Workshop visibility, and install paths. |
| BML / Stash memory | `~/.memory/2026-07-05-[fact]-barony-modloader-v1-state.md` | Feasibility context only | Use only where current Barony/BML runtime feasibility changes the solution-direction decision; do not turn into architecture. |
| Current user confirmation | Chat, 2026-07-05: “correct seed” | Approved | Confirms the inferred Runebound solution question and source scope are correct. |

## Scope
### In Scope
Solution-direction questions being evaluated:
- Which loot-system concept best serves the approved Runebound opportunity.
- How candidate directions differ on outcome fit, player value, feasibility risk, readability burden, co-op fit, Workshop trust, reversibility, and scope appetite.
- Which assumptions must be tested before architecture.
- Whether a bounded read-only probe is needed before choosing a direction.
- What architecture must preserve once the user selects a direction.

### Out of Scope
Problem discovery, visual mockups, technical architecture, API contracts, schemas, database models, service boundaries, queues, deployment topology, implementation steps, code patches, tests, migrations, rollout plans, final balance numbers, stat curves, drop rates, affix tables, native hook designs, and file-level plans.

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
| A | Curated Run-Defining Notables | Preserved; strong core candidate | High | Best direct gameplay fit if authored notables are genuinely Barony-native, tradeoff-heavy, and memorable. |
| B | Deterministic Affix Overlay | Preserved; future/conditional candidate | Low-Medium | Existing scaffold points here, but repo evidence says live gameplay is unverified and current examples risk stat-affix bloat. |
| C | Material / Progression Ladder Expansion | Preserved; support/flavor candidate | Medium | Useful readability/progression vocabulary, but weak as the primary answer unless materials create tradeoffs rather than linear upgrades. |
| D | Synergy Archetype Items | Survey-suggested outcome emphasis | Medium | User answers prioritized synergy archetypes, combo recognition, and trying new builds; broad opaque synergy remains risky and not selected. |
| E | Staged Hybrid: Notables First, Affixes Later | Workflowz evidence-leading posture | High | Staged posture remains the strongest evidence read for preserving options while proving fantasy, but the user has not selected it. |
| F | Static Content-Pack Expansion | Preserved; fallback/envelope candidate | Medium | Safest adoption shape, but only fits if static items are notable-grade rather than more content volume. |
| G | Synergy-Driven Bargain Notables | Survey-suggested hybrid, not selected | Medium | Survey signal: drops should ask players to take bargains, recognize combos, and try new builds; this suggests a D/E/A hybrid but needs tradeoff clarification before selection. |
| H | Equipment-Bound Sustained Effects | Newly added candidate | Medium | Armour/weapons grant always-on spell-like/passive effects while worn; strong Barony-native bridge between relics, gear, spells, and build identity. |
| I | Artifact / Unique Item Expansion | Newly added candidate | Medium-High | Expand Barony-style artifacts toward PoE-style uniques: fixed-name, fixed-identity, authored gear with distinctive effects and tradeoffs; likely strongest umbrella for A/H/G if kept Barony-native. |
| J | Ring-Centric Build System / Ten-Ring Mode | Newly added candidate | Medium | Add many authored rings and expand characters to wear many rings, turning rings into the primary build-composition surface; high synergy potential but high UI/balance/feel risk. |
| K | Relics (Cursed Equippables) | Mutually exclusive implementation branch | Medium-High | Relics are a new cursed equippable item type that cannot be freely unequipped and provides strong passive rule changes; best object/loot identity, higher equipment lifecycle risk. |
| L | Permanent Bargain Elixirs | Selected first feature | High | Elixirs are a new class-bound consumable type that grants permanent or run-permanent tradeoff effects when used; they may be semi-common because the tradeoff, not rarity alone, controls power and commitment. |

### Candidate Detail Notes

#### A. Curated Run-Defining Notables
- Concept: a small set of hand-authored notable drops with strong identities, clear upside/downside tradeoffs, and enough power to reshape a run.
- Serves: meaningful tradeoffs, plan-changing drops, quick recognition, and co-op handoff moments.
- Intentionally does not do: flood ordinary loot with many small modifiers or require every item to become build-defining.
- Might be right: strongest readability-to-impact ratio; closest to Slay the Spire relic clarity while preserving low inventory burden.
- Might be wrong: limited authored pool could become familiar quickly; may not deliver broad long-term loot variety by itself.
- Evidence: discovery prioritized O6/O1/O3 and warned against affix bloat; Barony already has artifacts as a familiar “special item” precedent.
- Key assumptions: a small curated pool can create enough run identity; hand-authored effects can feel Barony-native rather than arbitrary.
- Primary risk category: player value / scope appetite.

#### B. Deterministic Affix Overlay
- Concept: ordinary Barony items can roll deterministic, Barony-native modifiers that create tradeoffs, identity, and build pivots.
- Serves: broad item variety, replayability, and the fantasy that many drops can surprise the player.
- Intentionally does not do: rely only on rare uniques or fixed authored content.
- Might be right: best match for ARPG/MMO loot inspiration and long-run variety if kept readable.
- Might be wrong: highest risk of affix bloat, unclear upgrades, inventory sorting fatigue, persistence complexity, and runtime feasibility problems.
- Evidence: user’s ARPG/MMO reference supports the desire; discovery and memory both warn that dynamic per-instance identity likely needs significant runtime capability.
- Key assumptions: affixes can stay sparse/readable; Barony/BML can support item metadata, persistence, display, stat/effect hooks, and deterministic rolls safely enough.
- Primary risk category: feasibility / interaction burden.

#### C. Material / Progression Ladder Expansion
- Concept: expand item progression fantasy through recognizable material tiers or material-like item identities, optionally using OSRS-style vocabulary as inspiration.
- Serves: familiar fantasy, readability, and expanded progression beyond base Barony materials.
- Intentionally does not do: make OSRS metals mandatory or solve run identity only through bigger numbers.
- Might be right: very readable, Workshop-explainable, and easier for players to evaluate quickly.
- Might be wrong: may become incremental tier replacement rather than “this changes everything” unless paired with deeper effects or tradeoffs.
- Evidence: user introduced OSRS metals as optional inspiration; discovery says readability is a guardrail and primary opportunity is tradeoff/plan change, not tier count.
- Key assumptions: material identity can carry meaningful tradeoffs, not only linear upgrades; expanded progression can remain Barony-native.
- Primary risk category: outcome fit.

#### D. Synergy Archetype Items
- Concept: items are designed around build vectors that interact with player choices, other items, or playstyle patterns to create emergent run identity.
- Serves: Binding of Isaac-style surprise, cleverness, and co-op “this is better for you” fit.
- Intentionally does not do: make every item independently run-defining without context.
- Might be right: strongest path to “I’m clever for building around this” if synergies are discoverable.
- Might be wrong: complexity can become opaque, balance-heavy, and hard to communicate without UI/tooltip burden.
- Evidence: discovery explicitly cites Isaac synergies and clever build-around moments; constraints warn against added complexity and slow evaluation.
- Key assumptions: synergies can be made legible enough in Barony’s interaction model; co-op groups can recognize fit quickly.
- Primary risk category: usability / complexity.

#### E. Staged Hybrid: Notables First, Affixes Later
- Concept: begin with curated run-defining notables to prove the core fantasy and readability, while preserving deterministic affixes/material expansion as later options after feasibility learning.
- Serves: immediate run identity, controlled scope, stability trust, and learning before deeper runtime investment.
- Intentionally does not do: commit the first direction to full ARPG affixes or a pure content pack.
- Might be right: preserves the strongest options while reducing the biggest early risks; creates a concrete architecture handoff without overcommitting implementation detail.
- Might be wrong: if the user wants broad randomized loot fantasy immediately, this may feel too conservative.
- Evidence: discovery’s top outcomes favor notable plan-changing moments; BML memory suggests dynamic affix systems likely carry meaningful feasibility risk.
- Key assumptions: a first solution direction can be staged without diluting the long-term Runebound fantasy; architecture can preserve future affix/material paths.
- Primary risk category: viability / option preservation.

#### F. Static Content-Pack Expansion
- Concept: add more authored items or item variants with minimal dynamic behavior, prioritizing compatibility and Workshop clarity.
- Serves: low-risk novelty, content breadth, and simple installation/adoption.
- Intentionally does not do: build a dynamic item identity system or guarantee run-changing drops.
- Might be right: safest first release shape if runtime risk dominates all other criteria.
- Might be wrong: directly risks missing the approved opportunity by becoming “more content” rather than plan-changing run identity.
- Evidence: discovery rejects “more loot” as sufficient and identifies content expansion mods as adjacent rather than directly solving the top outcomes.
- Key assumptions: authored static items can still be made distinctive enough; safety/compatibility may be worth weaker outcome fit.
- Primary risk category: outcome fit.

#### H. Equipment-Bound Sustained Effects
- Concept: weapons and armour grant permanent sustain-spell-like effects while worn, plus other persistent equipment-bound effects.
- Serves: run identity, build pivots, Barony-native spell/equipment fantasy, co-op handoff, and quick “this changes my plan” recognition.
- Intentionally does not do: create a broad stat-affix soup or explicit deckbuilder-style archetype taxonomy.
- Might be right: it translates Slay-the-Spire-like relic pressure into Barony’s own grammar: equipment, spells, artifacts, blessings/curses, and survival resources.
- Might be wrong: it may require significant runtime support for equipped-state effects, stacking rules, persistence, display/status clarity, and co-op synchronization.
- Evidence: user introduced the direction; Barony’s public copy emphasizes spells/equipment, deep RPG systems, loot/stats to analyze and synergize, and resourcefulness.
- Key assumptions: always-on equipment effects can stay readable and not become passive clutter; sustain-like effects can be Barony-native rather than abstract build tags.
- Primary risk category: feasibility / usability.

#### I. Artifact / Unique Item Expansion
- Concept: expand Barony's artifact-like item space toward PoE-style Unique Items: named, recognizable, authored items on specific gear bases with fixed identity, distinctive effects, and deliberate tradeoffs.
- Serves: run identity, quick recognition, plan-changing drops, co-op “who should take this?” moments, and long-term mod identity without relying on broad random affix soup.
- Intentionally does not do: copy PoE’s economy-scale unique catalog, tier complexity, trading assumptions, or hundreds-of-items endgame; uniqueness should be in gameplay rules and Barony fantasy, not item-count bloat.
- Might be right: this may be the cleanest synthesis so far: A’s curated notables, H’s equipment-bound sustained effects, and G’s bargain/combo play can all live inside a Barony artifact/unique-item frame.
- Might be wrong: if every unique requires bespoke runtime behavior, the technical surface may become broader than affixes; if effects are too subtle, it becomes flavor-only content; if too many are added, it becomes a lookup-table burden.
- Evidence: user raised artifact expansion and PoE-style Unique Items; PoE Wiki describes unique items as specific-name items with predetermined modifiers, unique artwork, and special build-enabling gameplay; Barony already has artifact vocabulary and public positioning around spells/equipment, deep loot/stat synergy, surprises, and resourcefulness.
- Key assumptions: a small authored set of unique/artifact-grade items can deliver enough variety; players will evaluate them quickly by name/effect; architecture can support a constrained effect catalog rather than unlimited bespoke scripting.
- Primary risk category: scope / runtime surface.

#### J. Ring-Centric Build System / Ten-Ring Mode
- Concept: create a large ring pool and let characters wear many rings, potentially up to ten, making rings the main modular build surface for Runebound.
- Serves: combo recognition, build pivots, loot excitement, and a highly legible item family where small effects can stack into a run identity.
- Intentionally does not do: make every weapon/armor drop unique or require affixes on all ordinary gear.
- Might be right: rings are fantasy-native, compact, easy to theme, and naturally suited to passive/sustained effects; “ten rings” is a memorable mod hook that players can understand and talk about.
- Might be wrong: ten simultaneous ring effects can explode cognitive load, break Barony balance, fight the existing equipment UI/feel, and make the correct play become inventory/ring micromanagement rather than dungeon adaptation.
- Evidence: user proposed “a bunch of rings” plus “wear 10 rings”; discovery favors plan-changing drops, combo recognition, low inventory burden, and Barony feel.
- Key assumptions: Barony/BML can support extra ring slots cleanly; rings can stay readable as a small number of strong decisions rather than ten passive stat crumbs; the concept reads as delightful rather than gimmicky.
- Primary risk category: usability / Barony feel / UI feasibility.

#### K. Relics (Cursed Equippables)
- Concept: relics are a new type of cursed equippable item that function like Slay the Spire relics: persistent passive/run-rule effects that become part of the character once equipped and cannot be freely unequipped.
- Serves: “this changes everything” moments, irreversible or semi-irreversible bargains, build identity, combo recognition, and reduced inventory churn.
- Intentionally does not do: require every ordinary equipment slot to carry build identity, or make players constantly swap passive bonuses for marginal gains.
- Might be right: binding/cursing directly solves the “relic” problem in Barony language: the item is not just gear, it is a dungeon bargain with consequences.
- Might be wrong: forced inability to unequip can become anti-fun if the choice was unclear, accidental, or too punishing; it also needs very strong UI/readability and a release-valve policy.
- Evidence: user clarified the cursed equippables are a new item type called relics, and also preserved elixirs as the consumable branch; discovery prioritizes meaningful tradeoffs and “this changes everything” while warning against frustration, bloat, and Barony-feel violations.
- Key assumptions: binding can be opt-in/readable enough to feel like a bargain rather than a trap; relic effects can be powerful without creating unwinnable runs; co-op groups can understand who should accept a cursed equippable.
- Primary risk category: consent / frustration / balance.

#### L. Permanent Bargain Elixirs
- Concept: elixirs are a new class-bound consumable or accept-on-use item type that grant permanent or run-permanent bargain effects once used.
- Serves: run-shaping decisions, low inventory burden, clear commitment, class-specific build identity, party-size-sensitive co-op tuning, and Slay-the-Spire-like passive effects without requiring extra equipment slots or constant swapping.
- Intentionally does not do: make elixirs another gear slot to optimize every floor, rely on normal cursed-equipment behavior, or make broad random affixes the first feature.
- Might be right: elixirs are readable as “drink/accept this and your run changes”; class-binding keeps the candidate pool relevant to the current character, party-size binding lets co-op-specific bargains appear only when they make sense, and semi-common drops can work because each elixir carries a tradeoff rather than pure upside.
- Might be wrong: class-binding and party-size binding require architecture to know present player classes and player count, especially in co-op; permanent consumables need especially clear previews and post-use visibility; too many semi-common permanent passives can still become mental overhead.
- Evidence: user selected elixirs as the first Runebound feature, then refined that elixirs can be semi-common if they are tradeoffs, should be class-bound, and should include player-count-bound drop eligibility for some elixirs.
- Key assumptions: permanent elixir effects can be tracked and communicated clearly after consumption; players understand drinking/using the elixir as consent; class detection, co-op present-class pools, and party-size eligibility can be implemented safely; elixir count/effect strength stay bounded enough to avoid passive clutter.
- Primary risk category: permanence clarity / class-state detection / effect tracking.

## Workflowz Research Synthesis

Parallel lenses run:
- game-design / outcome fit;
- technical feasibility;
- co-op / Workshop adoption;
- scope / reversibility;
- adversarial critique.

Consensus read:
- **E wins as the strongest direction posture** if the first bet is “prove run-defining item identity safely before broadening the system.”
- **A is the best core gameplay content inside E**: curated, readable, tradeoff-heavy notables are the cleanest match for the approved O6/O1/O3 opportunity.
- **B remains valuable but conditional**: the current repo scaffold is historical broad-item-shaped, but live gameplay is not verified and the existing examples are mostly stat bonuses, not proven run-defining tradeoffs.
- **C should remain optional support**: material vocabulary can help readability/fantasy but should not lead unless it creates meaningful choices, not just tier progression.
- **D is a strong later/complementary layer**: synergy can create cleverness and co-op fit, but should not lead before readability and runtime-surface risk are controlled.
- **F is a safety envelope, not a product answer by itself**: static content works only if it carries notable-grade identity.

Probe decision:
- No code/runtime probe is needed before a direction decision **if the user selects E/A**.
- A runtime feasibility probe becomes blocking **before selecting B or broad D as the primary direction**, because the current repo says Runebound dynamic item behavior is scaffold-only / not installed / not live verified.
- A lightweight concept probe would improve confidence for E/A but is not required before the user direction decision; it can be an architecture/game-design handoff question unless the user wants to test item concepts now.

## Evidence Ledger
| Claim | Evidence | Source | Confidence | Update Condition |
|---|---|---|---|---|
| The authoritative discovery source is `.agents/discovery/2026-07-05-runebound.md`. | Discovery progress says final discovery is approved complete; user confirmed the inferred seed as correct. | `.agents/discovery/2026-07-05-runebound-progress.md`; user chat | High | Changes only if user supplies a different source artifact or reopens discovery. |
| Primary opportunity is meaningful tradeoffs, plan-changing drops, and “this changes everything” moments. | ODI outcomes O6/O1/O3 were approved as primary. | `.agents/discovery/2026-07-05-runebound.md` | High | Changes if user reopens discovery outcome priority. |
| Fast readability, low inventory burden, Barony feel, stability, co-op compatibility, and Workshop adoption are constraints/guardrails. | Discovery records hard constraints and guardrails selected/approved by user. | `.agents/discovery/2026-07-05-runebound.md`; progress decision log | High | Changes if user explicitly changes constraints. |
| OSRS-style materials are optional game-design inspiration, not a hard requirement. | User corrected the earlier interpretation during discovery; final discovery preserves OSRS metals as optional. | `.agents/discovery/2026-07-05-runebound-progress.md` | High | Changes only if user promotes them to a selected solution direction later. |
| Co-op item handoff is central as a compatibility and social constraint, not the top opportunity outcome. | User resolved consistency check as “central constraint, not top outcome.” | `.agents/discovery/2026-07-05-runebound.md` | High | Changes if user decides co-op should become the primary solution driver. |
| Runtime feasibility may be decisive for solution direction, but architecture details are not yet in scope. | BML/Stash memory says dynamic randomized affix/legendary systems likely need item metadata, deterministic roll hooks, persistence, display/name/tooltip hooks, stat hooks, and eventually combat/effect hooks. | `~/.memory/2026-07-05-[fact]-barony-modloader-v1-state.md` | Medium | Must be verified against current repo/source if used to reject or prefer a direction. |
| Earlier Runebound scaffold was broad-loot shaped, but the current implementation has cut over to elixirs. | The package now uses `jml.runebound-elixirs`, elixir-specific capabilities, `modules.runeboundElixirs`, `elixir-catalog.json`, and `elixir-drop-tables.json`; the older broad-loot direction remains future/non-first-scope context only. | `mods/runebound-elixirs/bml-package.json`; `mods/runebound-elixirs/content/data/bml/elixir-catalog.json`; `mods/runebound-elixirs/content/data/bml/elixir-drop-tables.json` | High | Changes only if architecture is explicitly reopened to make broad loot the first implementation scope. |
| E/A are consensus-favored across workflowz lenses, but E is not user-selected yet. | Five parallel lenses converged on staged notables-first as strongest posture, with curated notables as the core gameplay bet; adversarial critique warned not to treat E as automatic. | Workflowz fan-out, artifact `artifact://5` | High | Changes if user values broad randomized loot immediately, or if concept/runtime probes favor B/D/C. |
| Static content is safe only when it is notable-grade. | Research lenses agreed F is adoption-friendly but misses the approved opportunity if it becomes content volume rather than run-defining tradeoff items. | Workflowz fan-out, artifact `artifact://5`; discovery anti-bloat constraints | High | Changes if static authored examples demonstrate plan-changing tradeoffs at low burden. |
| Equipment-bound sustained effects are a newly surfaced candidate that may fit Barony better than abstract synergy language. | User suggested armour/weapons with permanent sustain spells when worn and similar effects; Barony public materials emphasize spells, equipment, RPG systems, loot/stats to synergize, and resourcefulness. | User chat; Barony official site; Steam store page | Medium | Changes if technical/runtime evidence shows equipped-state effects are too risky or if concept examples become passive clutter rather than run-shaping decisions. |
| Slay-the-Spire-like archetype/synergy fantasy partially fits Barony’s public vision if translated into Barony-native resourcefulness rather than deck-builder rails. | Official site/store describe Barony as a first-person roguelike RPG with co-op, adaptability/resourcefulness, complex/deep RPG systems, loot/stats to analyze and synergize, unique classes/playstyles, brutal dungeons, and surprises. | `https://www.baronygame.com/`; Steam store page for app `371970`; user question | Medium | Changes if direct Barony player/community evidence rejects relic/archetype framing or if concepts feel non-Barony in play. |
| PoE-style Unique Items are relevant inspiration if translated into Barony-scale artifacts, not copied as an economy-scale item system. | PoE Wiki describes unique items as named items with predetermined modifiers and special build-enabling gameplay; the user asked whether Runebound could expand artifacts in that direction. | User chat; `https://www.poewiki.net/wiki/Unique_item`; Barony official site; Steam store page | Medium-High | Changes if unique examples cannot stay readable, Barony-native, or feasible within a constrained runtime effect catalog. |
| A ring-centric system could deliver synergy and build identity through a constrained item family. | The user proposed many rings and ten wearable ring slots; rings are a recognizable fantasy equipment type suited to passive or sustained effects. | User chat; approved discovery outcomes O6/O1/O3 and guardrails | Medium | Changes if extra ring slots are infeasible, UI-hostile, or create inventory/balance burden that violates discovery constraints. |
| Bound cursed relics are a strong candidate for translating Slay the Spire relic pressure into Barony. | The user proposed cursed items that cannot be unequipped and function like Slay the Spire relics; Barony already has curse-adjacent fantasy language, and discovery prioritizes meaningful tradeoffs and run-changing drops. | User chat; approved discovery outcomes O6/O1/O3 and guardrails | Medium-High | Changes if binding feels punitive, accidental, or UI-hostile, or if co-op/Workshop safety makes permanent effects too risky. |
| Naming now splits by mechanism: “relics” are cursed equippables, while “elixirs” are consumable bargain effects. | User clarified “the cursed equippables are a new item type called relics,” after saying elixirs make sense for the consumable bargain branch. | User chat | High | Changes only if the user renames either branch later. |
| Permanent bargain elixirs may be a stronger mechanism than cursed equippables for low inventory burden. | User suggested consumables with permanent effects like elixirs; this preserves commitment while avoiding extra equipment slots. | User chat; approved discovery guardrails | Medium-High | Changes if permanent effect tracking is unclear, if Barony feel requires visible equipment anchors, or if runtime persistence is too risky. |
| Implementation-wise, broad affixes are not the lowest-risk starting point despite historical scaffold pressure. | Current first-scope files declare elixir metadata, drop generation, consumption, active-effect state/application, display rendering, and multiplayer metadata; package/workshop copy remains hidden/evidence-gated rather than claiming full Workshop playability. | `mods/runebound-elixirs/bml-package.json`; `mods/runebound-elixirs/workshop.toml`; `native/barony-modloader-hook/manifests/steam-371970-22630456-linux.json` | High | Changes if future installed-executable hooks are identified, installed, and verified for a broader loot path. |
| Implementation-wise, elixirs appear narrower than relics as a first vertical slice, but weaker as visible loot objects. | Elixirs avoid extra equipment slots and per-item unequip rules, but still need persistent effect tracking; relics preserve object identity but require equip/bind/remove lifecycle and likely more UI/equipment integration. | Current candidate comparison; approved discovery constraints; repo hook evidence | Medium-High | Changes if Barony already exposes an easy cursed-equipment lifecycle hook or if persistent player-effect tracking proves harder than equipment binding. |
| Relics and elixirs should be treated as mutually exclusive first implementation formats, not parallel first-scope systems. | User explicitly narrowed the fork: “i think we do relics or elixirs, not both.” | User chat | High | Changes only if the user later decides both should coexist after the first implementation. |
| The underlying effects can mostly be shared between relics and elixirs; the main implementation difference is lifecycle. | A damage/resource/status rule can be expressed as a reusable authored effect, while relics wrap it in equip/bind/remove lifecycle and elixirs wrap it in consume/apply/persist lifecycle. | Candidate comparison; repo evidence that current affix path is broader than either branch | Medium-High | Changes if some desired effects intrinsically require a visible item anchor or an on-consume-only trigger. |
| Elixirs are the selected first Runebound feature. | User said “we should call them elixirs. lets proceed with elixirs as the first feature for runebound.” | User chat | High | Changes only if the user explicitly reopens direction selection. |
| Elixirs do not need to be rare if they are real tradeoffs. | User said elixirs could be semi-common if they are trade-offs. | User chat | High | Changes if playtests show semi-common permanent effects create clutter, snowballing, or decision fatigue. |
| Elixir pools should be class-bound. | User said effects should be class-bound: players only get elixirs bound to their class; in multiplayer, all class-bound items for all present classes can be found. | User chat | High | Changes if class detection/drop context proves infeasible or if co-op class pools create too much off-person burden. |
| Some elixirs should be player-count-bound for drop eligibility. | User said drop eligibility should also be player-count bound and some elixirs should only drop in certain party sizes. | User chat | High | Changes if party-size-aware drops prove infeasible or if dynamic join/leave behavior makes eligibility too confusing. |
| Runebound: Elixirs is selected as the first feature and now uses `modules.runeboundElixirs`. | Architecture recommended a clean cutover to an elixir module and capability set; the current package follows that shape. | `.agents/architecture/2026-07-05-runebound-elixirs-architecture-plan.md:50-69`; `mods/runebound-elixirs/bml-package.json` | High | Changes only if architecture is explicitly reopened to add broader loot systems to first scope. |
| `POTION_EMPTY` is the recommended first physical carrier for elixirs. | Architecture found `POTION_EMPTY` exists as a potion item, vanilla use is inert/hint-only, and using it lets unsupported runtimes fail closed instead of introducing a new Barony item enum first. | `.agents/architecture/2026-07-05-runebound-elixirs-architecture-plan.md:20-23`, `.agents/architecture/2026-07-05-runebound-elixirs-architecture-plan.md:90-97`, `.agents/architecture/2026-07-05-runebound-elixirs-architecture-plan.md:613-614` | High | Changes if live implementation proves another existing carrier is safer or more Barony-native. |
| Active consumed elixir effects need BML sidecar state rather than `Stat::attributes`. | Architecture found Barony player save/load skips `attributes`, so permanent/run-permanent consumed effects must live in BML-owned sidecar state and be restored by the runtime. | `.agents/architecture/2026-07-05-runebound-elixirs-architecture-plan.md:23-24`, `.agents/architecture/2026-07-05-runebound-elixirs-architecture-plan.md:123-125`, `.agents/architecture/2026-07-05-runebound-elixirs-architecture-plan.md:615-616` | High | Changes only if a verified Barony-native persisted player-state surface is identified. |

## Assumption Ledger
| Assumption | Why It Matters | Test / Probe | Status | Decision Impact |
|---|---|---|---|---|
| A solution direction can be chosen before full architecture if tradeoffs are kept implementation-neutral. | `/solution` must converge enough for `/architecture` without prescribing implementation. | User approval gates and final brief quality check. | Active | If false, reopen discovery or run a bounded probe before direction decision. |
| At least three viable loot-system directions exist and should be compared before convergence. | Prevents assumption collapse into “affixes” or “materials” as the default answer. | Candidate set review. | Active | If fewer are viable, document why and preserve the evidence. |
| Technical feasibility evidence may affect direction, but should not become architecture prematurely. | Dynamic affixes/effects may vary greatly in runtime risk. | Optional read-only repo/source probe if candidate comparison depends on it. | Active | Could shift confidence or recommend staged direction. |
| The first Runebound bet should probably privilege run identity over content breadth. | Discovery prioritizes tradeoffs/plan-changing drops over “more items.” | Compare against candidate set. | Hypothesis | Could rule against content-pack-only directions unless user values low-risk first release more. |
| Notables-first is a faithful first test of Runebound’s long-term fantasy. | If false, E could be a safe slice that does not satisfy the ARPG/MMO-inspired loot-variety desire. | Optional no-code concept probe comparing notable, affix, material, and synergy examples on O6/O1/O3/O2/O5/O7. | Active | If curated notables underperform while constrained affixes/synergies score better, reconsider B/D or a different hybrid. |
| Existing historical broad-item scaffold should inform feasibility, not decide product direction. | Repo path-dependence could bias the solution toward B even if current affix content misses tradeoff/run-identity outcomes. | Ask whether B would still lead if the scaffold did not exist; compare against discovery outcomes. | Active | Prevents accidental scaffold-driven architecture. |
| Co-op and Workshop trust are veto constraints even where ODI importance looked lower. | User later made stability/co-op/Workshop hard constraints; dynamic systems can fail adoption even when game-design upside is high. | Keep runtime proof and player-facing claim discipline as architecture handoff requirements. | Active | B/D cannot lead publicly without stronger proof; E/A can proceed with narrower claims. |
| Equipment-bound sustained effects can carry the synergy fantasy without explicit archetype rails. | If true, H may be a better Barony-native expression of the survey signal than broad D or abstract G. | Compare H examples against A/B/D/G on readability, plan-change, co-op handoff, and runtime risk. | Active | Could move H into the leading option set before final direction selection. |
| Slay-the-Spire inspiration must be translated, not copied. | Barony is embodied first-person dungeon survival/co-op, not a deckbuilder; archetypes should emerge from gear/resource/playstyle choices rather than explicit deck rails. | Concept probe: compare “relic-like rule change” concepts against Barony-native classes, survival pressures, resources, weapons, spells, and co-op handoff. | Active | If concepts feel like external card-game logic, narrow back to Barony artifacts/blessings/curses/equipment tradeoffs. |
| Artifact/unique expansion can serve as an umbrella direction over notables, sustained effects, and bargain synergies. | If true, I may resolve the naming/framing problem: the solution is not “deckbuilder archetypes” or “stat affixes,” but a Barony artifact expansion with unique equipment rules. | Compare I against A/H/G/B on outcome fit, readability, co-op handoff, scope, and runtime feasibility. | Active | Could become the leading direction framing while preserving H as a mechanism and A/G as design principles. |
| Ten-ring mode is memorable but must prove it does not violate low inventory burden. | The concept’s appeal comes from combinatorics, but the approved discovery explicitly warns against slow evaluation and inventory pressure. | Concept comparison: compare 2 strong ring slots, 4 curated ring slots, and 10-ring mode against readability, combo depth, UI impact, and co-op handoff. | Active | Could become a playful subdirection of artifact uniques, or be narrowed to a smaller ring expansion if ten slots is too chaotic. |
| Cursed equippables need an explicit consent and release-valve model. | A non-unequippable item can create great commitment, but accidental or unclear binding would violate the low-friction/readability guardrails. | Concept comparison: opt-in equip binding, pickup binding, curse-removal escape, shrine/vendor release, or limited cursed-equippable slots. | Active | Could make K a strong branch, or demote it if permanent equipment binding is too hostile. |
| Permanent bargain elixirs require persistent effect visibility after the item is gone. | If the elixir is consumed, the player still needs to know what permanent rules are active; otherwise readability and trust degrade. | Concept comparison: permanent status panel, character-sheet elixir list, floor-start reminder, or visible buff icons as candidate UX requirements for architecture. | Active | Could make L leading if tracking is feasible; could push back toward cursed equippables if consumed effects are too invisible. |
| The first implementation should probably prove a small shared effect catalog, not a general item-randomization engine. | Elixirs and relics can share authored effect definitions; affixes require broader per-item metadata, roll, rendering, save, combat, and multiplayer surfaces. | Architecture handoff should compare a narrow effect-catalog runtime against the current historical broad-item-shaped scaffold. | Active | If true, L/K become safer implementation-first branches than B/J; if false, existing affix infrastructure may regain priority. |
| Effects should be modeled as format-agnostic where possible, with relic/elixir as the delivery wrapper. | This lets the solution decision compare lifecycle and UI cost without redesigning every effect twice. | Architecture handoff should preserve shared effect definitions while choosing exactly one first delivery format. | Active | If true, selecting relics vs elixirs is mostly an activation/presentation/persistence decision rather than an effect-design decision. |
| Semi-common elixirs must stay tradeoff-bearing, not pure upgrades. | Frequency can increase only if the decision remains meaningful and does not create automatic pickup/consume behavior. | Architecture/game-design handoff should require each elixir to express both gain and cost in catalog data. | Active | If tradeoffs are weak, elixirs must become rarer or more constrained. |
| Class-bound elixir drops require reliable present-class awareness. | Solo should only surface elixirs bound to the player's class; multiplayer should include pools for all present classes so drops can support co-op handoff without irrelevant global clutter. | Architecture must identify class/source-of-truth and host-authoritative party composition before implementing drop generation. | Active | If class detection is unreliable, class-binding becomes a blocking implementation risk. |
| Party-size eligibility should initially govern drop generation, not retroactive use, unless an effect explicitly depends on current allies. | The user framed player count as drop eligibility; making already-dropped elixirs become unusable after disconnects or party changes would create avoidable frustration. | Architecture handoff should model party-size constraints on catalog/drop tables and separately model effect conditions when needed. | Active | If user wants party-size-bound use eligibility too, add explicit use-time validation and messages. |

## User Decision Log
| Date | Decision | Options Preserved / Rejected | Source | Impact |
|---|---|---|---|---|
| 2026-07-05 | Confirmed the inferred `/solution` seed for Runebound as correct. | No solution directions generated or rejected yet. | User chat: “correct seed” | Authorizes progress doc creation and decision-frame synthesis. |
| 2026-07-05 | Approved the Phase 0 decision frame. | Draft candidate directions generated; none rejected or selected yet. | User chat: “correct” | Authorizes candidate-set approval gate. |
| 2026-07-05 | Approved candidate option space by instructing “continue.” | Directions A-F preserved for evidence/assumption comparison; none rejected or selected yet. | User chat: “continue” | Authorizes evidence, assumptions, and tradeoff review. |
| 2026-07-05 | Ran workflowz parallel solution research across outcome-fit, feasibility, adoption, scope, and adversarial lenses. | Directions A-F preserved; E/A evidence-leading; B/D conditional; C support; F fallback/envelope. | Workflowz fan-out, artifact `artifact://5`; repo reads | Enables tradeoff comparison and user direction decision. |
| 2026-07-05 | Deterministic ask result suggested, but did not select, a D/E/A hybrid: Synergy-Driven Bargain Notables. | Preserved: all directions remain available. No direction is user-selected yet; user corrected “hold on, i did not select a direction yet.” | `ask` tool answers plus user correction | Next step is explain what the survey suggested and clarify tradeoffs before any final direction brief. |
| 2026-07-05 | Added Equipment-Bound Sustained Effects as candidate H. | No direction selected; H now competes with A-G as a possible Barony-native expression of run-shaping item identity. | User suggested “armour and weapons have a permanent sustain spell when worn, and other effects like this.” | Requires tradeoff comparison against notables, affixes, and synergy-archetype framing before final decision. |
| 2026-07-05 | Added Artifact / Unique Item Expansion as candidate I. | No direction selected; I now competes with A-H and may act as an umbrella for curated notables, equipment-bound effects, and bargain synergies. | User asked “we could expand the artifacts? what about the direction of PoE-style 'Unique Items'?” plus PoE unique-item source read. | Requires updated tradeoff comparison; do not treat this as selected until user explicitly chooses it. |
| 2026-07-05 | Narrowed the implementation fork to relics or elixirs, not both. | Effects can mostly be shared conceptually, but only one first delivery format should be selected. | User said “i think we do relics or elixirs, not both” and asked whether the effects could be the same in either format. | Requires feasibility recommendation between K and L before final direction selection. |
| 2026-07-05 | Added Ring-Centric Build System / Ten-Ring Mode as candidate J. | No direction selected; J competes with A-I and may be a subdirection of artifact/unique expansion if rings become the primary unique-item category. | User asked “what if we made a bunch of rings, and made characters able to wear 10 rings?” | Requires tradeoff comparison, especially readability, UI feasibility, Barony feel, and balance burden. |
| 2026-07-05 | Adopted “relics” for the cursed-equippable branch and updated candidate K to Relics (Cursed Equippables). | No direction selected; K competes with A-J/L as the equipment-bound version of the bargain fantasy. | User clarified “the cursed equippables are a new item type called relics.” | Requires updated tradeoff comparison focused on consent, release valves, UI clarity, co-op handoff, and balance. |
| 2026-07-05 | Adopted “elixirs” for the consumable bargain branch and updated candidate L to Permanent Bargain Elixirs. | No final direction selected; L competes with K as the consumable version of the same permanent bargain fantasy. | User said “if elixirs make sense for the bargains, lets call them elixirs,” then clarified the branch as “a new type of consumable.” | Requires updated comparison of elixirs vs cursed equippables, especially consent, effect visibility, Barony feel, and persistence. |
| 2026-07-05 | Selected Permanent Bargain Elixirs as Runebound’s first feature. | Relics, rings, affixes, and broader unique/artifact expansion are preserved as non-first-scope alternatives or future directions, but first implementation should be elixirs. | User said “we should call them elixirs. lets proceed with elixirs as the first feature for runebound.” | Authorizes `/architecture` planning for elixirs. |
| 2026-07-05 | Refined elixir drop model: semi-common, class-bound, and co-op present-class pooled. | Elixirs need not be rare if tradeoff-bearing; solo drops should match the player's class; multiplayer drops may include elixirs for all classes present in the party. | User said “i don't even think the elixirs need to be rare if they are trade-offs... effects should be class-bound... in multiplayer, all class-bound items for all present classes can be found.” | Architecture must add class/source-of-truth and present-party-class pool design to implementation planning. |
| 2026-07-05 | Added player-count-bound drop eligibility for elixirs. | Some elixirs should only drop at certain party sizes; this augments class-bound pooling and semi-common tradeoff frequency. | User said “drop eligibility should also be player-count bound. some elixirs should only drop in certain party sizes.” | Architecture must add party-size constraints to elixir catalog/drop tables and decide dynamic join/leave behavior. |

## Open Questions
| Question | Owner | Needed For | Status |
|---|---|---|---|
| Which candidate solution directions are the right option space to compare? | User + agent | Candidate set approval | Answered: directions A-F approved for comparison. |
| Does dynamic per-item identity require a feasibility probe before choosing direction? | Agent proposes; user decides | Probe/spike decision | Answered conditionally: not needed before choosing E/A; required before selecting B or broad D as primary. |
| Which item categories and gameplay surfaces are eligible for run-shaping identity? | Later `/architecture` / game-design step | Architecture handoff | Answered for first feature: architecture recommends `POTION_EMPTY` carrier elixirs with BML sidecar item/effect state; relics/rings/affixes remain future/non-first-scope. |
| What validation standard proves the mod is safe enough for Workshop/co-op use? | Later `/architecture` | Architecture handoff | Answered for claim discipline: do not update Workshop/playable claims until solo drop/use/save, multiplayer present-class pool, party-size gating, and mismatch rejection live gates pass. |

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
