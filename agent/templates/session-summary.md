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

## Changed paths

- `path`: concise reason.

## Verification

| Command/gate | Result |
| --- | --- |
| Command | Count or signal |

## Decisions and experience

- Canonical decision links.
- New or updated experience IDs.

## Unresolved items

- Work ID, blocker/risk, and next action.

## Handoff

Give the next agent a concrete first command and the minimum files to read. Link
the current progress record or checkpoint when the session changes durable state.

The summary belongs inside a MetaFlux project work record containing objectives,
decisions, tool calls/results, commands, outputs, and notes. It must not contain
conversation transcripts, personal context, or work from another repository.
Promote only validated, reusable project knowledge to `memory/` or
`experience/`.
