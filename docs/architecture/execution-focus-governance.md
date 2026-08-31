# Execution Focus Governance

| Field | Value |
| --- | --- |
| Status | Verified decision; SC0007 migration Active |
| Decision | D0029 |
| Classification | Breaking (destructive) governance |
| Governance epoch | D0029 |
| Compatibility | None for schema 1 focus or pre-epoch session authority |
| Applies to | Default work selection, session ownership, and durable commit authorization |

## Decision

MetaFlux has one machine-readable execution focus at a time. The focus names
one owning `in_progress` session and either one product Exit Gate or one bounded
decision-authorized governance migration. A durable content commit declares its
exact owner through `METAFLUX_SESSION_ID`; the candidate focus and candidate
session record must agree. An unrelated active session never supplies commit
authority.

The current focus contract is schema version 2 and declares
`governance_epoch: D0029`. Its owner is a schema version 2 session declaring
the same epoch. This is a breaking governance boundary: schema version 1
sessions and any session missing the exact epoch are factual records only and
can never own focus. There is no compatibility parser, grandfather handoff,
in-place legacy upgrade, fallback owner, or prior-file execution path. Every
continuation of a legacy objective starts from the current repository rules in
a newly scaffolded epoch-bearing successor.

A product focus resolves one Active milestone, one Active work item belonging
to that milestone, and that work item's canonical `## Exit Gate`. Every
milestone dependency must be Complete. A governance focus resolves one decision
and one Active semantic change whose migration session is the focus owner; it
also names the dependency-valid product target that resumes after application.
There is no generic maintenance bypass.

## Handoff And Closure

Ordinary content and checkpoint commits keep the same focus owner. A focus
handoff is a record-only commit that terminally closes the old owner, installs
one newly scaffolded schema version 2 `in_progress` owner with the exact D0029
epoch, and updates the focus atomically. A legacy or non-owner active session
may preserve its existing facts until liquidation and use an exact record-only
terminal close, but it may not add product, tooling, plan, or governance
content and may not be installed directly as successor.

## Legacy Epoch Liquidation

SC0007 makes settlement mandatory before product focus resumes. Every schema
version 1 session directory is removed from the current tree, including its
event log, notes, detailed summary, outputs, and transient guidance. A compact
machine settlement manifest may retain only canonical session IDs, the source
Git revision, and the fact of liquidation so durable references remain
resolvable as tombstones. It is not a resume surface and carries no objective,
result, next action, evidence detail, or content authority.

The `$roast` settlement keeps only already promoted `medium roasts` and
`dark roasts` in their existing canonical owners. It creates no roast archive
and retains no `light roasts` or `session-only` details. Git preserves the old
bytes at their original revisions for repository history; the current
engineering version neither loads those bytes as task context nor provides a
compatibility route to execute them.

`agent/progress/current.md` is the compact human resume projection of the focus:
it names the same target, the current boundary, at most three next actions, and
explicit blockers. Historical checkpoints retain evidence; they do not compete
as current scheduling candidates. A checkpoint remains justified by an
independently valuable verified outcome, but local testability alone does not
change the execution focus.

## Consequences

- Existing schema version 1 and pre-epoch `in_progress` sessions permanently
  lose content authority and are liquidated before SC0007 applies; a handoff can
  name only a newly scaffolded current-schema successor.
- Parallel investigation and guidance remain possible; durable source changes
  serialize through the focus owner.
- Product scheduling follows milestone dependencies instead of available local
  hardware or the easiest passing fixture.
- Session ownership is explicit at commit time, while closing records retain a
  narrow path that cannot carry content.
- Focus changes are reviewable repository state, not a process-local selector
  or an inference from paths, Git identity, timestamps, or running processes.
- Preserving canonical medium/dark owners and earlier Git commits is knowledge
  and history retention, not workflow compatibility.

## Verification State

Revision `47d5735cb363129ac5877d084d0af1c4326c5421` remains the factual
schema version 1 / SC0006 baseline. Active SC0007 owns the breaking schema
version 2 migration and legacy liquidation. The candidate validator and
scaffolder now require the exact D0029 epoch, legacy owners are rejected, and
the Agent-record self-test passes 189 cases. Candidate commit authorization,
the Claude bridge, exact liquidation inventory, medium/dark owner audit,
session-detail deletion, residual search, and final product handoff remain
required before SC0007 can become Applied.
