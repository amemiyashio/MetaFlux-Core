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
sessions were destructively settled and any session missing the exact epoch can
never own focus. There is no compatibility parser, grandfather handoff,
in-place legacy upgrade, fallback owner, prior-file execution path, or legacy
close route. Every continuation of an old objective starts from current
canonical repository files in a newly scaffolded epoch-bearing successor.

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
epoch, and updates the focus atomically. A current-epoch non-owner may
terminally close its own ledger through the exact record-only path, but it
cannot commit content or be installed directly as successor. Liquidated and
other pre-epoch IDs have no close path.

## Legacy Epoch Liquidation

SC0007 settled all 82 schema version 1 sessions before product focus resumed.
Their event logs, notes, detailed summaries, outputs, and transient guidance
are absent from the current tree. The machine-checked
`agent/sessions/liquidated-v1.json` manifest retains only canonical session IDs,
settlement counts, the source Git revision, and identities of existing
medium/dark owners so durable references resolve as tombstones. It is not a
resume surface and carries no objective, result, next action, evidence detail,
or content authority. Once this manifest exists, the validator rejects every
schema version 1 session directory and every live/tombstoned ID overlap.

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
  lost content authority and were liquidated; a handoff can name only a newly
  scaffolded current-schema successor.
- Parallel investigation and guidance remain possible; durable source changes
  serialize through the focus owner.
- Product scheduling follows milestone dependencies instead of available local
  hardware or the easiest passing fixture.
- Session ownership is explicit at commit time; the focus owner controls
  content and handoff, while a current-epoch non-owner can only close its exact
  record ledger.
- Focus changes are reviewable repository state, not a process-local selector
  or an inference from paths, Git identity, timestamps, or running processes.
- Preserving canonical medium/dark owners and earlier Git commits is knowledge
  and history retention, not workflow compatibility.

## Verification State

Revision `47d5735cb363129ac5877d084d0af1c4326c5421` remains the factual
schema version 1 / SC0006 baseline. Active SC0007 owns the breaking schema
version 2 migration and destructive liquidation. The candidate validator and
scaffolder require the exact D0029 epoch; commit and Claude gateways reject
legacy owners; the strict tombstone rejects legacy-directory reintroduction;
and the Agent-record self-test passes 194 cases. Exact inventory, medium/dark
owner audit, 330 tracked-file removals, 72 transient guidance removals, and 20
checkpoint-link migrations are present in the candidate tree. Residual search,
candidate commit verification, SC0007 application, and the current-epoch W0112
handoff remain before product work resumes.
