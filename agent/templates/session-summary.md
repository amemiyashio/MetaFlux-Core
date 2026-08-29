---
id: SYYYYMMDD-NNN-slug
status: complete
fidelity: exact
started_at: YYYY-MM-DD
ended_at: YYYY-MM-DD
base_revision: commit or null
final_revision: commit or null
---

# Session Summary

Instance path: `agent/sessions/YYYY/MM/SYYYYMMDD-NNN-slug/summary.md`.

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

## Distillation

What this session promoted into durable records so reusable knowledge does not
stay trapped in cold storage. `none` is a valid answer; the validator requires
this section for sessions from 2026-08-28 onward.

- Distilled: none | memory/<file>, decisions DNNNN, experience ENNNN

## Unresolved items

- Work ID, blocker/risk, and next action.

## Handoff

Give the next agent a concrete first command and the minimum files to read. Link
the current progress record or checkpoint when the session changes durable state.

The summary is a curated outcome and resume point. It does not mirror Git,
archive a build directory, preserve routine command output, or retain failed
routes without a reusable lesson. Promote only validated, reusable project
knowledge to `memory/` or `experience/`.
