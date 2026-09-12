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
   A no-op reports the existing exact acceptance and publication state without
   beginning another operation. For a new candidate, begin the Batch request
   with `checks` omitted so the main controller supplies its fixed final
   metadata plan. Product checks belong only in step 3's integration plan;
   they are rejected in a Batch request.
2. Review the exact base..tip scope and current Exit Gate with the owning
   domain skills. Reject drift. Keep a current-HEAD candidate in place; prepare
   a divergent candidate with `git merge --no-commit --no-ff TIP`. Never accept
   an obscured stale ancestor as a fresh candidate.
   Compare the delivered behavior with its briefing using
   [implementation guidance](../main/references/implementation-guidance.md).
   Review actual admitted callers and fresh/reused state before accepting a
   dead-code or fallback-removal claim. Passing existing corpus rows alone
   does not show that an unrepresented reachable path remains correct.
3. Resolve only bounded composition defects. Select fresh covering checks on
   the actual merged tree, without repeating a focused CTest gate also selected
   by that phase's full suite. Promote material knowledge
   using [$main](../main/SKILL.md) skill's shared reference before the final combined evaluation.
   A worker receipt is entry evidence, not a replacement for integration checks.
   Choose checks for the merged behavior and shared risks; a candidate's broad
   test list is not automatically the right integration plan. Preserve required
   modes and prerequisites, and proceed to acceptance after that plan passes.
   Run `scripts/batch.py load-rules DELIVERY --root .` and read the emitted
   bodies. Then run `scripts/batch.py verify DELIVERY --root . --checks PLAN.json
   --summary 'Actual parent review'`. The wrapper derives the integration base
   from the checked delivery, validates context/rules and preflights coverage
   before executing any long check; do not hand-assemble a second baseline.
4. Supply that current integration receipt to `scripts/batch.py advance`.
   This is the sole daily Goal/work-item writer. It prepares a recoverable
   transaction under the shared Git lock; failed validation restores only
   writes that still belong to that transaction.
5. A `slice` preserves the planned lane, Active work item, and target, assigning
   the current lane the Batch's maximum Iteration plus one. A `work-item`
   acceptance requires its whole Exit Gate, completes it, and selects the first
   array-ordered planned lane whose dependencies are integrated. Exhaustion
   does not wrap. Closing a Batch does not close a milestone or create an Epoch.
6. Run `scripts/batch.py check-metadata --root .`. Its exact transaction proof
   limits the post-integration delta to the recorded Goal/work-item acceptance
   changes, including modes and blobs. Reload rules through [$main](../main/SKILL.md) skill,
   review the current advanced tree, and evaluate the controller's fixed final
   metadata/state/routing checks. That proof preserves the fresh integration evidence without a third
   product-suite run. A product or other semantic change invalidates it and
   requires repair and fresh integration verification. Then commit the
   exact acceptance tree, staging its reviewed paths before delivery through
   [$main](../main/SKILL.md) skill's helper. A staging-only rejection preserves the
   receipt: stage the reported paths and retry delivery without new evaluation
   or transaction rollback. Its Git trailers bind
   the candidate, delivery identity, acceptance kind, and receipt digest.
7. Publish that exact commit through [$main](../main/SKILL.md) skill's shared transport. Resume failed
   publication without accepting again. Return the next exact tuple and base;
   [$main](../main/SKILL.md) skill continues only in an already supplied matching execution context.

The delivery remains transient. Accepted product facts belong in their unique
source/test/contract/plan/decision owner; Git preserves prior states.
