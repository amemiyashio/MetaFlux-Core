---
id: MXYZ0
delivery: X.Y.Z.0
release: vX.Y.Z
status: Draft
budgets: provisional
depends_on: []
areas: []
updated: YYYY-MM-DD
---

# MXYZ0: Milestone Title

Instance path: `agent/plan/MXYZ0-slug/plan.md`. Derive the compact ID from the
explicit delivery coordinate; do not allocate an unrelated serial.

## Outcome

State one observable product outcome and its user-visible boundary.

## Scope

### Included

- Item.

### Excluded

- Item and destination milestone where known.

## Locked boundaries

Link canonical contracts and architecture decisions. Record only milestone-specific
constraints here.

## Phases

| Phase | Exit gate | Status |
| --- | --- | --- |
| 0 | Measurable evidence | Draft |

## Acceptance evidence

- Correctness and ABI gate.
- Performance gate with measurement conditions. Numeric budgets carry the
  front-matter `budgets` status: `provisional` until the named measurement
  harness exists and a baseline is archived, `binding` afterward.
- Packaging and compatibility gate.

## Work index

| Work ID | Title | Status | Evidence |
| --- | --- | --- | --- |
| WXYZ1 | Title | Draft | Link |

## Risks and unresolved decisions

- Decision with owner and closure condition.

`Complete` requires every acceptance gate or an explicit superseding decision.
