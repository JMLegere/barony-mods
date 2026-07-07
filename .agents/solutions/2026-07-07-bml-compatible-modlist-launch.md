# Solution Direction: BML Compatible Modlist Launch

Date: 2026-07-07
Status: Approved
Discovery Source: `.agents/discovery/2026-07-07-bml-profile-launch-requirements.md`
Progress Doc: `.agents/solutions/2026-07-07-bml-compatible-modlist-launch-progress.md`

## Executive Summary

The user-selected solution direction is **Direction B: Declarative Compatibility Contract**, with **Direction E: Progressive Hybrid Gate** preserved as architecture sequencing guidance. BML should treat package-declared compatibility facts as the primary source of pre-launch launchability truth: enabled mods, deterministic load order, issue severity, dependencies/conflicts/capability/runtime requirements at a direction level, and explainable error/warning outcomes. This was chosen over a minimum known-error gate because it better satisfies explainable compatibility; over curated modlists because it better fits “any compatible modlist”; and over a full automatic resolver because the first architecture pass should stage the contract rather than overbuild an ecosystem-scale solver. Confidence is **Medium-High**. Revisit if architecture finds package-declared compatibility facts cannot produce useful error/warning launch decisions without becoming a full resolver.

## Source Discovery

- Approved discovery artifact: `.agents/discovery/2026-07-07-bml-profile-launch-requirements.md`.
- Approved discovery control doc: `.agents/discovery/2026-07-07-bml-profile-launch-requirements-progress.md`.
- Target opportunity/problem: BML needs pre-launch compatible multi-mod launch confidence before starting Barony.
- Desired outcomes:
  - identify enabled mods before launch;
  - identify deterministic load order before launch;
  - determine whether the modlist has issues;
  - define compatibility as **no error-level issues**;
  - block launch on error-level issues;
  - allow launch with warning-level or lower-severity issues while keeping them visible/explainable;
  - explain non-launchable modlists.
- Hard constraints and non-goals inherited from discovery:
  - no current-session loaded-mod truth;
  - no post-launch diagnostics;
  - no diagnosing missing in-game mod effects;
  - no local developer/tester workflow expansion;
  - no session sharing/export;
  - no multiplayer synchronization;
  - no UI layouts, mocks, icons, screens, or component design;
  - no technical architecture, APIs, schemas, database models, implementation plans, code changes, tests, deployment, or rollout plans.

## Decision Frame

This brief resolves the solution-direction question:

> What direction should BML take to provide pre-launch compatible multi-mod launch confidence?

The selected direction is not an implementation plan. It intentionally does not decide:

- exact metadata fields;
- package manifest/schema shape;
- validation algorithm;
- GUI layout or interaction design;
- runtime manifest structure;
- file paths;
- database/storage model;
- code ownership boundaries;
- implementation order;
- tests or rollout.

Those belong to `/architecture` or later implementation planning.

## Candidate Directions Considered

### Direction A: Minimum Known-Error Gate

Concept: BML would provide deterministic enabled-mod and load-order visibility, block only known hard errors, and treat everything else as warning/unknown risk.

Why it might be right:

- Smallest direction that honors the approved launch gating rule.
- Honest about limited compatibility knowledge.
- High reversibility.

Why it might be wrong:

- Explanations may be weak or generic.
- Too many real incompatibilities may collapse into vague warnings.
- Under-serves the discovery requirement for clear non-launchable explanations.

Evidence:

- Discovery only requires blocking error-level issues, not solving every possible compatibility risk.
- But discovery also requires non-launchable explanations and visible issue severity.

Key assumptions:

- Known hard errors are enough to create initial trust.

Status: **Rejected as primary / fallback preserved.** It remains useful as fallback behavior when compatibility facts are incomplete.

### Direction B: Declarative Compatibility Contract

Concept: BML treats package-declared compatibility facts as the primary source of pre-launch truth. Packages declare the compatibility-relevant facts BML needs to determine launchability, issue severity, dependencies/conflicts, load-order needs, capability/runtime requirements, and explainable non-launchable outcomes.

Why it might be right:

- Strongest fit with the approved discovery outcome.
- Provides a path to explainable errors and warnings.
- Fits existing BML philosophy: packages are explicit, inspectable, validated input artifacts.
- Aligns with app-owned validation and launch responsibilities.
- Avoids relying only on curated presets or vague unknown-risk warnings.

Why it might be wrong:

- Can pressure architecture toward metadata/scope expansion.
- Depends on package authors or BML-owned package definitions being accurate enough.
- Could become too close to a full resolver if not staged carefully.

Evidence:

- `framework/BaronyModLoader/README.md` says the standalone app owns installation, packages, versions, profiles, runtime provenance, hook lifecycle, launch, validation, and diagnostics.
- `framework/BaronyModLoader/package-format.md` already frames packages as explicit and inspectable, with dependencies/conflicts, capabilities, activation records, validation stages, and stable human-readable errors.
- `framework/BaronyModLoader/loader-runtime-contract.md` says the app owns package installation, profile activation, dependency resolution, validation, launch, and logs.
- Discovery defines compatibility as no error-level issues and requires issue visibility/explanations before launch.

Key assumptions:

- Package-declared compatibility facts can provide useful pre-launch error/warning decisions.
- Architecture can stage the declarative contract without designing a full automatic resolver immediately.
- Warning visibility can be handled as a direction-level requirement without deciding UI in `/solution`.

Status: **Selected.**

### Direction C: Curated Compatible Modlists

Concept: BML would rely primarily on known-good modlists, known-bad combinations, and maintainer-approved load orders. Unknown combinations would remain warning-heavy or blocked depending on policy.

Why it might be right:

- Practical for a small early ecosystem.
- Useful when BML controls the first few packages.
- Can create confidence before rich compatibility metadata exists.

Why it might be wrong:

- Too narrow for “launch any compatible modlist.”
- Risks making compatibility feel permissioned rather than explainable.
- Does not create as strong a long-term foundation as package-declared compatibility facts.

Evidence:

- Current BML package ecosystem is small.
- Discovery asks for compatible modlists generally, not only curated presets.

Key assumptions:

- Early value comes more from known combinations than arbitrary composition.

Status: **Rejected as primary / optional aid preserved.** Curated known-good/bad knowledge may supplement B, but should not be the main direction.

### Direction D: Full Automatic Resolver

Concept: BML would aim upfront to automatically resolve dependencies, conflicts, load order, runtime capability compatibility, and launchability for arbitrary packages.

Why it might be right:

- Most complete long-term expression of mod manager behavior.
- Strong theoretical fit for arbitrary compatible modlists.

Why it might be wrong:

- Too broad and premature for the approved pass.
- Risks turning a focused launch-confidence requirement into full ecosystem architecture.
- May overfit to mod-manager ambition before enough real package cases exist.

Evidence:

- Adjacent mod ecosystems use dependency/conflict/load-order concepts.
- The approved discovery does not require an ecosystem-scale resolver immediately.

Key assumptions:

- The near-term ecosystem is complex enough to justify full resolver investment.

Status: **Rejected for now / long-term option preserved.**

### Direction E: Progressive Hybrid Gate

Concept: BML starts with deterministic profile launch, known-error gating, visible warnings, and clear non-launchable explanations while preserving an explicit path toward richer declarative compatibility metadata.

Why it might be right:

- Combines A’s honest narrowness with B’s long-term compatibility model.
- Prevents B from turning into D too early.
- Fits the broader selected BML profile-first GUI direction.

Why it might be wrong:

- As a standalone selected direction, it could dilute B into vague staging language.
- It must be framed as sequencing discipline, not a weaker product direction.

Evidence:

- Discovery requires launchability confidence, not full ecosystem solving.
- Existing BML docs support declarative package validation.

Key assumptions:

- The selected declarative contract can be staged.

Status: **Preserved as architecture sequencing guidance under B.**

## Evidence and Assumptions

### Evidence Ledger

| Claim | Evidence | Source | Confidence | Update Condition |
|---|---|---|---|---|
| The approved problem is pre-launch compatible multi-mod launch confidence, not post-launch diagnostics. | Discovery narrows scope to enabled mods, deterministic load order, compatibility, severity, explanations, and pre-launch confidence. | `.agents/discovery/2026-07-07-bml-profile-launch-requirements.md` | High | Change only if user reopens discovery scope. |
| Compatibility means no error-level issues. | Discovery states compatibility means exactly no error-level issues. | `.agents/discovery/2026-07-07-bml-profile-launch-requirements.md` | High | Change only if user changes gating policy. |
| Launch gating blocks errors and allows warnings/lower severity while making them visible/explainable. | Discovery states the exact launch gating policy. | `.agents/discovery/2026-07-07-bml-profile-launch-requirements.md` | High | Change only if user changes gating policy. |
| Existing BML product direction is profile-first GUI. | Prior solution selected profile-first GUI launcher. | `.agents/solutions/2026-07-06-bml-gui-mod-manager.md` | Medium-High | Change if user revises the broader BML direction. |
| Existing BML docs support a declarative package/validation direction. | README, package format, and runtime contract place package facts, profile activation, validation, launch, diagnostics, dependency/conflict ideas, and error models in the app/package domain. | `framework/BaronyModLoader/README.md`, `framework/BaronyModLoader/package-format.md`, `framework/BaronyModLoader/loader-runtime-contract.md` | High | Change if product docs are superseded or architecture finds the current model infeasible. |
| Adjacent mod manager evidence supports enabled/launched state separation, load order, dependencies, conflicts, profiles, and diagnostics. | Discovery evidence ledger summarizes r2modman/Vortex/Barony research. | `.agents/discovery/2026-07-07-bml-profile-launch-requirements-progress.md` | Medium | Change with stronger Barony-specific ecosystem evidence. |

### Assumption Ledger

| Assumption | Why It Matters | Status | Decision Impact |
|---|---|---|---|
| Package-declared compatibility facts can be the primary source of launchability truth. | This is the core bet in Direction B. | Open for architecture. | If false, selected direction collapses toward A/E with more known-error gating. |
| The declarative contract can be staged. | Prevents B from becoming full resolver scope. | Supported by preserving E as sequencing guidance. | Architecture must not overbuild D. |
| Warning visibility can be direction-level without UI design. | Keeps `/solution` from becoming `/mocks`. | Supported. | Architecture/mocks can later decide presentation. |
| Existing package docs are directionally trustworthy but not final architecture. | Prevents treating docs as schemas or implementation. | Supported. | Architecture must validate source/runtime details. |

## Probes / Spikes Performed or Deferred

No new probe was performed during this `/solution` pass.

Reason:

- The decision is direction-level.
- Existing discovery and BML source docs are sufficient to compare directions.
- The key remaining unknowns are architecture-level: minimum compatibility facts, evaluation boundary, staging, and exact representation.

Deferred architecture probes/questions if needed:

- What minimum package-declared compatibility facts are needed to produce useful error/warning decisions?
- Can existing package/profile/runtime validation surfaces support staged declarative compatibility without becoming a full resolver?
- What current package examples should architecture use to test the selected direction?

## Tradeoff Comparison

| Criterion | A: Minimum Known-Error Gate | B: Declarative Compatibility Contract | C: Curated Compatible Modlists | D: Full Automatic Resolver | E: Progressive Hybrid Gate |
|---|---|---|---|---|---|
| Outcome fit | Medium | Strong | Medium-low | Strong | Strong |
| Enabled mods/load order clarity | Strong | Strong | Strong | Strong | Strong |
| Non-launchable explanations | Weak-medium | Strong | Medium | Strong | Strong over time |
| Fits “any compatible modlist” | Medium | Strong | Weak-medium | Strong | Strong |
| First-scope size | Small | Medium | Small-medium | Very large | Medium |
| Risk of overbuilding | Low | Medium | Low | High | Medium-low |
| Risk of weak trust | Medium-high | Low-medium | Medium | Low | Low-medium |
| Fits current BML docs | Medium | Strong | Medium | Medium | Strong |
| Reversibility | High | Medium | Medium | Low | High-medium |
| Confidence | Medium | Medium-High | Low-Medium | Low | Medium-High |

Narrative:

- A wins only if minimizing scope matters more than strong explanations.
- B wins if the selected direction should make compatibility explainable through explicit package facts.
- C wins only if BML wants early curated confidence more than general modlist composition.
- D wins only if BML intentionally wants ecosystem-scale resolver scope now.
- E is best treated as sequencing guidance under B, not a separate final direction.

## User Direction Decision

### Selected Direction

**Direction B: Declarative Compatibility Contract.**

BML should treat package-declared compatibility facts as the primary source of pre-launch launchability truth for compatible multi-mod launch.

### Why This Direction

User rationale:

- User first expressed preference for B: “i like b.”
- User then approved the selection prompt: Direction B with Direction E preserved as sequencing guidance.

Supporting evidence:

- Direction B best matches the discovery requirement for explainable launchability.
- It fits the existing BML package/validation philosophy.
- It scales better than curated-only or minimum-known-error approaches.
- It avoids full automatic resolver scope when paired with E as sequencing guidance.

### Alternatives Preserved or Rejected

- **A Minimum Known-Error Gate**: rejected as primary; preserved as fallback for incomplete compatibility facts or early stages.
- **C Curated Compatible Modlists**: rejected as primary; preserved as optional aid for known-good/bad early package combinations.
- **D Full Automatic Resolver**: rejected for now; preserved as long-term option if ecosystem complexity demands it.
- **E Progressive Hybrid Gate**: preserved as architecture sequencing guidance under B.

### Confidence

**Medium-High.**

High confidence that B fits the approved discovery problem and existing BML direction. Medium uncertainty remains around how much declarative compatibility information is minimally needed and how architecture should stage it without becoming a full resolver.

### Update Condition

Revisit this selected direction if architecture finds that package-declared compatibility facts cannot produce useful pre-launch error/warning decisions without becoming a full automatic resolver, or if the user reopens discovery scope to include post-launch diagnostics, multiplayer synchronization, or session sharing.

## Boundaries and Non-goals

The selected direction does not include:

- post-launch loaded-mod truth;
- runtime/session diagnostics;
- missing-effect diagnosis;
- multiplayer synchronization;
- session sharing/export;
- visual mockups or UI layouts;
- exact metadata fields;
- schemas or API contracts;
- database/storage model;
- implementation steps;
- tests, migrations, deployment, or rollout.

Architecture should not silently add:

- full automatic resolver behavior as first scope;
- remote package ecosystem assumptions;
- multiplayer/session-sync requirements;
- post-launch truth or effect-diagnosis features;
- UI-specific warning/error presentation decisions.

Architecture should preserve:

- compatibility means **no error-level issues**;
- error-level issues block launch;
- warnings/lower-severity issues allow launch but remain visible/explainable;
- enabled mods and deterministic load order are part of pre-launch truth;
- package-declared facts are the primary source of compatibility truth;
- staging discipline from E.

## Risks and Open Questions

### Blocking Before Architecture

None. The user-selected solution direction is clear enough for `/architecture`.

### Architecture Handoff Questions

Architecture must decide, using source/runtime evidence:

- What minimum package-declared compatibility facts are needed for the first architecture pass?
- How should architecture distinguish error-level issues from warning-level or lower-severity issues?
- How should deterministic load order be derived or constrained without overbuilding a full resolver?
- How should incomplete or missing compatibility facts degrade: warning, error, or fallback?
- What package/runtime/app evidence should be used to decide when an issue is error-level?
- How should B be staged so it does not become Direction D upfront?
- What existing package examples, such as Stash and Runebound, should be used to validate the model?

### Later Questions

Deferred beyond this first architecture pass unless separately reopened:

- remote package registry or marketplace compatibility metadata;
- community-authored compatibility databases;
- sharing/exporting modlists;
- multiplayer synchronization;
- post-launch loaded-mod truth;
- runtime effect diagnostics;
- visual design for warning/error presentation.

## Architecture Handoff Contract

Architecture should use this brief to plan implementation, but must not re-open the approved problem framing or user-selected solution direction without new evidence or user approval.

Architecture must preserve:

- Direction B: Declarative Compatibility Contract as the selected solution direction.
- Direction E: Progressive Hybrid Gate as sequencing guidance.
- Discovery gating policy: errors block; warnings/lower severity allow but remain visible/explainable.
- Discovery compatibility definition: no error-level issues.
- The boundary that this is pre-launch compatibility confidence only.

Architecture must decide:

- the minimum first-pass compatibility facts;
- how those facts are evaluated;
- how severity is determined;
- how load order becomes deterministic;
- how incomplete facts degrade;
- how to stage B without designing D.

Architecture must not assume:

- full resolver scope is approved;
- curated modlists are the primary model;
- post-launch truth is part of this pass;
- multiplayer/session sync is part of this pass;
- UI design has been approved;
- schemas, APIs, or file-level plans are decided by this brief.

## Approval

User-approved direction:

- [x] Direction selected: B, Declarative Compatibility Contract
- [x] Sequencing guidance preserved: E, Progressive Hybrid Gate
- [x] Final Solution Direction Brief approved
- [ ] Needs revision

Approval notes:

- Approved by user for `/architecture` handoff on 2026-07-07.
