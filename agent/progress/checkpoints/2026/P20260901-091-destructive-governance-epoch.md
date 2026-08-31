---
id: P20260901-091
status: Recorded
captured: 2026-09-01
milestone: M0110
workstream: W0112
branch: main
git_revision: 47bab198be8241c6c01a501056da5e74ca8d6522
workspace: destructive D0029 governance epoch
---

# Destructive Governance Epoch Checkpoint

## Outcome

D0029 now has one executable schema: focus and owner are schema version 2 and
declare `governance_epoch: D0029`. Revision `8247105` performed the exact atomic
cutover; revision `47bab19` removed the one-time cutover permit. Pre-commit and
the repository-local Claude bridge reject schema version 1 focus or owners,
mismatched epochs, and legacy non-owner close attempts. Continuing an old
objective requires a newly scaffolded current-epoch successor.

## Verification evidence

| Gate | Result |
| --- | --- |
| Agent records | Passed with 83 sessions, 452 events, and 411 Markdown files |
| Agent-record self-test | Passed 189 cases, including strict focus/owner schema and epoch failures in pre-commit and Claude |
| Semantic-change edit gate | Passed 21/21 cases |
| Commit identity | Passed 7/7 cases; `8247105` and `47bab19` record Agent Harness (codex) as Author and Committer |
| Session-start bridge | Emits schema version 2, D0029, and the explicit prohibition on legacy resumption |

## Boundary

This checkpoint proves the execution cutover, not legacy-data liquidation.
SC0007 must still commit an exact protected-file inventory, verify that every
retained medium/dark claim already resolves to one canonical owner, install the
compact non-executable tombstone manifest, delete every schema version 1 detail
directory and transient guidance packet, and pass a residual search before
product focus resumes.

## Cleanup

- Removed: task-generated Python bytecode and temporary self-test output.
- Retained temporarily: schema version 1 session directories and 35 untracked
  guidance inboxes solely as inputs to the exact SC0007 liquidation inventory.

## roast

### light roasts

- none.

### medium roasts

- none.

### dark roasts

- Destructive D0029 execution epoch and strict schema version 2 authority ->
  `docs/architecture/execution-focus-governance.md` (revisions `8247105` and
  `47bab19`; authority: D0029, SC0007)

## session-only

- none.

## Handoff

Use the current SC0007 governance owner to generate and commit the exact legacy
liquidation inventory. Do not delete protected session files before that permit
is present in `HEAD`, and do not turn old roast entries into a new archive.
