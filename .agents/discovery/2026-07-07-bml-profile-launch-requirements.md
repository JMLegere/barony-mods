# BML Profile Launch Requirements Discovery

## Summary

This discovery defines the problem-domain requirements for BaronyModLoader pre-launch compatible multi-mod launch. The narrow goal is that a user can assemble a BML modlist, understand the enabled mods and deterministic load order before launch, see whether any issues exist, and launch only when the modlist is compatible.

For this pass, compatibility means exactly: **no error-level issues**.

Launch gating is exactly: **error-level issues block launch; warnings and lower-severity issues allow launch but must be visible/explainable**.

This document is ready for /architecture handoff only as a requirements artifact. It does not prescribe UI, architecture, APIs, schemas, implementation plans, code changes, tests, deployment, or rollout.

## Problem Statement

BML needs a clear pre-launch requirement model for launching multiple mods together. The core user problem is not general profile management or post-launch diagnosis; it is knowing whether the intended enabled modlist is launchable before starting Barony.

A launchable multi-mod BML experience requires enough pre-launch truth to answer:

- Which mods are enabled for this launch?
- In what deterministic order will those mods load?
- Are there any issues in the modlist?
- Are any issues severe enough to make the modlist incompatible?
- If the modlist is not launchable, what error-level issue explains the block?
- If the modlist is launchable with warnings or lower-severity issues, what should the user know before choosing to proceed?

The approved scope is compatible multi-mod launch requirements only. Broader trust, diagnostics, sharing, multiplayer, and workflow topics are intentionally excluded from this pass.

## Scope

### In Scope

- Pre-launch compatible multi-mod launch requirements.
- Enabled mods selected for the launch.
- Deterministic load order for enabled mods.
- Compatibility defined as **no error-level issues**.
- Issue severity classification for launch gating.
- Blocking explanations for non-launchable modlists.
- Visibility and explanation of warning-level and lower-severity issues before launch.
- Pre-launch launchability confidence for the exact intended modlist.

### Out of Scope

The following topics are explicit non-goals and must not enter /architecture from this discovery pass:

- Current-session loaded-mod truth.
- Post-launch diagnostics.
- Diagnosing missing in-game mod effects.
- Local BML user launch-confidence framing beyond the narrowed pre-launch compatible multi-mod requirement.
- Local mod developer/tester workflows.
- Session sharing or export codes.
- Multiplayer synchronization.
- Vanilla-vs-BML launch separation beyond any already-existing product behavior.
- UI layouts, mocks, icons, screens, or component design.
- Technical architecture.
- APIs.
- Schemas.
- Implementation plans.
- Code changes.
- Tests.
- Deployment.
- Rollout plans.

## Users and Jobs

Only the users and jobs needed for this narrowed pass are included.

### Primary User: BML Modlist Launcher

A user who wants to launch Barony through BML with more than one enabled mod.

Job:

- Assemble an intended set of enabled BML mods and determine before launch whether that exact modlist is compatible and launchable.

Needs:

- See the enabled mods that define the launch intent.
- See the deterministic load order that defines how the modlist will be applied.
- Know whether the modlist has issues.
- Know whether any issue is error-level.
- Be prevented from launching only when error-level issues exist.
- Be able to proceed when only warnings or lower-severity issues exist, with those issues visible and explainable.

### Secondary Context: Mod Testing Use Case

Mod testing was part of the evidence trail, but local developer/tester workflow discovery is out of scope for this final pass. It remains relevant only as evidence that pre-launch enabled-mod truth, load order, and issue visibility matter.

Job within this pass only:

- Before launch, confirm that the intended compatible multi-mod set is the launch target.

## Outcomes

The requirements should enable these outcomes:

1. A user can identify the exact enabled modlist before launch.
2. A user can identify the deterministic load order before launch.
3. A user can tell whether the modlist is compatible before launch.
4. A compatible modlist is launchable when it has **no error-level issues**.
5. A modlist with one or more error-level issues is not launchable.
6. A non-launchable modlist has a visible explanation tied to its error-level issue or issues.
7. A modlist with only warning-level or lower-severity issues remains launchable.
8. Warning-level and lower-severity issues are visible and explainable before launch.
9. The pre-launch state distinguishes launch-blocking issues from non-blocking issues.
10. /architecture can proceed from these requirements without importing excluded topics.

## Constraints and Gating Policy

### Compatibility Definition

Compatibility means exactly: **no error-level issues**.

### Launch Gating Policy

Launch gating is exactly: **error-level issues block launch; warnings and lower-severity issues allow launch but must be visible/explainable**.

### Required Constraint Boundaries

- The pre-launch model must be about the intended enabled modlist, not runtime or post-launch truth.
- The pre-launch model must include deterministic load order as part of launch confidence.
- Severity must distinguish at least error-level issues from warnings and lower-severity issues.
- Error-level issues are the only approved launch-blocking severity in this discovery pass.
- Warning-level and lower-severity issues must not silently disappear; they must remain visible and explainable even though they do not block launch.
- Non-launchable explanations must be tied to error-level issues, not generic failure language.
- Requirements must remain problem-domain requirements and must not prescribe UI, architecture, APIs, schemas, or implementation details.

## Capability Requirements

Each requirement below traces to the approved narrow scope, the approved gating policy, or the evidence ledger in the progress document.

| ID | Capability requirement | Trace |
|---|---|---|
| CR-1 | Represent the exact enabled mods that make up the intended pre-launch BML modlist. | Narrow scope: enabled mods; evidence: minimum trust proof includes enabled mods. |
| CR-2 | Represent the deterministic load order for the enabled pre-launch modlist. | Narrow scope: deterministic load order; evidence: minimum trust proof includes load order and Barony modding is load-order sensitive. |
| CR-3 | Determine whether the intended pre-launch modlist has any issues. | Narrow scope: issue visibility; evidence: minimum trust proof includes whether there are any issues. |
| CR-4 | Classify issues by severity so error-level issues are distinguishable from warnings and lower-severity issues. | Approved gating policy and compatibility definition. |
| CR-5 | Define compatibility as **no error-level issues**. | Approved compatibility definition. |
| CR-6 | Block launch when one or more error-level issues exist. | Approved launch gating policy. |
| CR-7 | Allow launch when issues are only warning-level or lower severity. | Approved launch gating policy. |
| CR-8 | Keep warning-level and lower-severity issues visible and explainable before launch. | Approved launch gating policy. |
| CR-9 | Explain why a modlist is not launchable using the relevant error-level issue or issues. | Narrow scope: non-launchable explanations; evidence: non-launchable explanations are important and poorly served. |
| CR-10 | Provide enough pre-launch launchability confidence for the intended compatible multi-mod list without relying on post-launch/session diagnostics. | Narrow scope correction; evidence: compatible multi-mod launch is current-pass scope and current-session truth is out of scope. |
| CR-11 | Avoid importing excluded domains into this pass when translating requirements to /architecture. | Approved scope correction and explicit non-goals. |

## Evidence and Confidence

### High Confidence

- The user approved compatible multi-mod loading as a core opportunity, not a secondary or future feature.
- The user corrected the scope so compatible multi-mod launch is in scope for the current pass.
- The user approved the compatibility definition: **no error-level issues**.
- The user approved the launch gating policy: **error-level issues block launch; warnings and lower-severity issues allow launch but must be visible/explainable**.
- The user approved session sharing/export as out of scope for now.
- The user intentionally moved current-session loaded-mod truth, post-launch missing-effect diagnosis, local BML user confidence framing, and local mod developer/tester workflows out of scope.
- The user identified the minimum trust proof for launch as enabled mods, load order, and whether there are any issues.

### Medium Confidence

- Adjacent mod manager and Barony modding research supports the importance of installed/enabled/launched state separation, load order, dependencies, conflicts, profiles, and diagnostics.
- Barony modding context suggests load order and exact mod alignment are important, but this discovery uses that only as evidence for pre-launch enabled-mod/load-order/issue requirements.

### Evidence Not Carried Into Scope

Some evidence in the progress ledger concerns current-session truth, missing-effect diagnosis, local tester workflows, sharing/export, multiplayer, and post-launch diagnostics. Those are intentionally not converted into requirements here because the approved final scope is pre-launch compatible multi-mod launch only.

## Open Questions

No blocking open questions remain for /architecture handoff of this narrowed pass.

Non-blocking questions intentionally deferred because they belong outside this discovery scope:

- How should current-session loaded-mod truth be surfaced after launch?
- How should missing in-game mod effects be diagnosed?
- What local mod developer/tester workflows should BML optimize?
- How should local BML user confidence be framed beyond compatible multi-mod pre-launch requirements?
- Should session sharing/export be supported later?
- Should multiplayer synchronization or exact shared load-order validation be supported later?
- How should UI, architecture, APIs, schemas, code, tests, deployment, or rollout be designed?

## Architecture Handoff Notes

This discovery is ready for /architecture handoff because:

- The problem statement is narrowed to pre-launch compatible multi-mod launch requirements.
- The in-scope and out-of-scope boundaries are explicit.
- Compatibility is defined as **no error-level issues**.
- Launch gating is defined as **error-level issues block launch; warnings and lower-severity issues allow launch but must be visible/explainable**.
- Capability requirements trace to the approved narrow scope, gating policy, and evidence ledger.
- Open questions are non-blocking and explicitly deferred outside this pass.
- No UI, architecture, API, schema, implementation plan, code change, test, deployment, or rollout prescription is included.

/architecture should treat this as the requirements boundary and should not expand into the excluded topics without a separate discovery decision.
