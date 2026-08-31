# Execution Focus Governance

| Field | Value |
| --- | --- |
| Status | Verified |
| Decision | D0029 |
| Applies to | Default work selection, session ownership, and durable commit authorization |

## Decision

MetaFlux has one machine-readable execution focus at a time. The focus names
one owning `in_progress` session and either one product Exit Gate or one bounded
decision-authorized governance migration. A durable content commit declares its
exact owner through `METAFLUX_SESSION_ID`; the candidate focus and candidate
session record must agree. An unrelated active session never supplies commit
authority.

A product focus resolves one Active milestone, one Active work item belonging
to that milestone, and that work item's canonical `## Exit Gate`. Every
milestone dependency must be Complete. A governance focus resolves one decision
and one Active semantic change whose migration session is the focus owner; it
also names the dependency-valid product target that resumes after application.
There is no generic maintenance bypass.

## Handoff And Closure

Ordinary content and checkpoint commits keep the same focus owner. A focus
handoff is a record-only commit that terminally closes the old owner, installs
one new `in_progress` owner, and updates the focus atomically. An active session
that is not the focus owner may preserve its existing work and use an exact
record-only terminal close, but it may not add product, tooling, plan, or
governance content.

`agent/progress/current.md` is the compact human resume projection of the focus:
it names the same target, the current boundary, at most three next actions, and
explicit blockers. Historical checkpoints retain evidence; they do not compete
as current scheduling candidates. A checkpoint remains justified by an
independently valuable verified outcome, but local testability alone does not
change the execution focus.

## Consequences

- Existing `in_progress` sessions remain factual records but lose content
  authority until an explicit focus handoff names them.
- Parallel investigation and guidance remain possible; durable source changes
  serialize through the focus owner.
- Product scheduling follows milestone dependencies instead of available local
  hardware or the easiest passing fixture.
- Session ownership is explicit at commit time, while closing records retain a
  narrow path that cannot carry content.
- Focus changes are reviewable repository state, not a process-local selector
  or an inference from paths, Git identity, timestamps, or running processes.

## Verification State

Behavior revision `47d5735cb363129ac5877d084d0af1c4326c5421` installs the
focus schema and dependency/Exit-Gate validator, exact candidate-owner and
record-only handoff gate, Claude focus guard, compact progress projection, and
workflow/tool routing. The candidate pre-commit passed with the new focus in
the staged tree; the Agent-record self-test passed 184 cases; the focused
architecture CTest selection passed 7/7; current-authority residual searches
found no global session-coverage rule; and the SC0006 Historical inventory
remained byte-for-byte unchanged.

SC0006 owns the final migration inventory, effective revision, and atomic
handoff from the governance owner to the dependency-valid W0112 product owner.
