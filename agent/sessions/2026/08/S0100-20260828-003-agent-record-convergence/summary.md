# Summary

Machine-enforced the four self-discipline gaps found in the agent/ design
review, converting the record loop's remaining manual steps into validator
rules and tooling. No product code changed.

The validator now requires index completeness (sessions, plan, and experience
README tables must exactly match disk), checks the new
`agent/memory/open-decisions.md` ledger (29 unresolved decisions across
M0100-M0130, per-milestone counts must match each plan's Decisions-to-Close),
requires a Distillation section in session summaries from 2026-08-28 onward,
and warns when an Active plan record is more than 14 days stale against the
latest checkpoint. `tools/new-session.py` scaffolds a validator-clean session
with automatic id allocation and index registration; it was dogfooded to create
this session, whose skeleton passed validation before any content was filled.

Failure paths were self-tested: removed index row and removed ledger row each
fail validation; a stale Active date warns without failing. Two implementation
bugs (anchored id patterns versus prefixed link targets, missing capture
group) were caught and fixed by those self-tests.

## Changed paths

- `tools/check-agent-records.py`: four new rules plus warnings output.
- `agent/memory/open-decisions.md`: new aggregated open-decisions ledger.
- `agent/memory/README.md`: ledger added to the memory index.
- `agent/templates/session-summary.md`: Distillation section.
- `agent/sessions/2026/08/S0100-20260828-001-spec-consistency/summary.md`
  and `agent/sessions/2026/08/S0100-20260828-002-layout-convergence/summary.md`:
  backfilled Distillation sections.
- `tools/new-session.py`: new session scaffolder.
- `tools/README.md`, `agent/README.md`: documentation of the new rules.

## Verification

| Command/gate | Result |
| --- | --- |
| `python3 tools/check-agent-records.py .` | ok (4 sessions, 37 events, 59 Markdown) |
| Negative: index row removed | Validation failed with 1 error |
| Negative: ledger row removed | Validation failed with 2 errors |
| Negative: Active date made stale | Warning emitted, exit 0 |
| Scaffold skeleton before fill-in | Validation ok (4 sessions, 37 events) |

## Decisions and experience

- No new DNNNN decisions: these are agent-workflow rules recorded in
  agent/README.md and enforced by the validator, not product decisions.

## Distillation

- Promoted: open-decision, distillation, staleness, and scaffolding rules ->
  memory, agent/README.md, and tools/README.md (session verification above).
- Session-only: none; these are repository-local governance rules rather than
  reusable engineering experience.

## Unresolved items

- None blocking. The first real consumer of the ledger is the next M0100
  decision closure, which must move its row out of open-decisions.md.

## Handoff

First command: `python3 tools/check-agent-records.py .`. Minimum reading:
[open decisions](../../../../memory/open-decisions.md) for what is still
undecided, then [current progress](../../../../progress/current.md).
