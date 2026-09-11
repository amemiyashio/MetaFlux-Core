---
name: batch
description: Check or automatically accept an exact MetaFlux Iteration delivery, integrate and verify the combined tree, accept a slice or whole work item, publish its state commit, and expose the next dependency-ready assignment.
---

# Batch

Invoke automatically from [$main](../main/SKILL.md) skill after an exact qualified Iteration delivery.
`$batch check` and requests to inspect a delivery are read-only. Uncommitted
patches, completion language, and a reported passed string do not authorize a
state transition. Bootstrap and diagnostics are owned by [$main](../main/SKILL.md) skill.

## Acceptance

1. Require the supplied current-main integration context and delivery v2.
   Run `scripts/batch.py check DELIVERY --root .`. Check existing validated
   acceptance history before new-candidate identity to make retries idempotent.
2. Review the exact base..tip scope and current Exit Gate with the owning
   domain skills. Reject drift. Keep a current-HEAD candidate in place; prepare
   a divergent candidate with `git merge --no-commit --no-ff TIP`. Never accept
   an obscured stale ancestor as a fresh candidate.
3. Resolve only bounded composition defects. Run fresh focused, affected,
   and combined checks on the actual merged tree. Promote material knowledge
   using [$main](../main/SKILL.md) skill's shared reference before the final combined evaluation.
   A worker receipt is entry evidence, not a replacement for integration checks.
4. Supply that current integration receipt to `scripts/batch.py advance`.
   This is the sole daily Goal/work-item writer. It prepares a recoverable
   transaction under the shared Git lock; failed validation restores only
   writes that still belong to that transaction.
5. A `slice` preserves the planned lane, Active work item, and target, assigning
   the current lane the Batch's maximum Iteration plus one. A `work-item`
   acceptance requires its whole Exit Gate, completes it, and selects the first
   array-ordered planned lane whose dependencies are integrated. Exhaustion
   does not wrap. Closing a Batch does not close a milestone or create an Epoch.
6. Evaluate final state/routing and affected combined gates, then commit the
   exact staged acceptance tree through [$main](../main/SKILL.md) skill's helper. Its Git trailers bind
   the candidate, delivery identity, acceptance kind, and receipt digest.
7. Publish that exact commit through [$main](../main/SKILL.md) skill's shared transport. Resume failed
   publication without accepting again. Return the next exact tuple and base;
   [$main](../main/SKILL.md) skill continues only in an already supplied matching execution context.

The delivery remains transient. Accepted product facts belong in their unique
source/test/contract/plan/decision owner; Git preserves prior states.
