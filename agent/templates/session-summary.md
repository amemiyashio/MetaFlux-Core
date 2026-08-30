---
id: SXYZW-YYYYMMDD-NNN-slug
delivery: X.Y.Z.W
status: complete
fidelity: exact
started_at: YYYY-MM-DD
ended_at: YYYY-MM-DD
base_revision: commit or null
final_revision: commit or null
---

An active session uses `status: in_progress` and `ended_at: null`. Set the end
date only when recording a terminal status.

# Session Summary

Instance path: `agent/sessions/YYYY/MM/SXYZW-YYYYMMDD-NNN-slug/summary.md`.

## Objective and outcome

State the project objective and resulting repository state.

## Durable changes

- `path`: concise reason.

## Verification

| Command/gate | Result |
| --- | --- |
| Command | Count or signal |

## Cleanup

List the session-owned disposable work inspected at handoff. Remove failed
routes, duplicate source snapshots, build trees, temporary downloads, and logs;
state `none` when inspection finds nothing. Name any retained non-Git artifact
and why it is still required.

- Removed: `path` - reason | none.
- Retained: `path` - owner and reason | none.

## Decisions and experience

- Canonical decision links.
- New or updated experience IDs.

## roast

Classify only materially promoted claims by semantic transformation depth. Each
bucket contains one or more `claim -> owner (evidence)` entries or the sole
value `- none.`. Active sessions may temporarily use the sole value `- TODO.`.

### light roasts

- <claim> -> <canonical path or ID> (<evidence>)

### medium roasts

- <claim> -> <canonical path or ID> (<evidence>)

### dark roasts

- <claim> -> <canonical path or ID> (<evidence>; authority: DNNNN, SCNNNN | SC not required)

## session-only

Record material resume or explanatory context that is intentionally not
promoted. Use one or more entries with a retention reason or the sole value
`- none.`. Active sessions may temporarily use the sole value `- TODO.`.

- <material fact> - reason: <why durable promotion is not justified>

## Unresolved items

- Work ID, blocker/risk, and next action.

## Handoff

Give the next agent a concrete first command and the minimum files to read. Link
the current progress record or checkpoint when the session changes durable state.

The summary is a curated outcome and resume point. It does not mirror Git,
archive a build directory, preserve routine command output, or retain failed
routes without a reusable lesson. Promote claims to their owning source,
contract, decision, memory, experience, plan, progress, or semantic-change
record. Keep only the classified compact promotion map and justified
session-only residue here.
