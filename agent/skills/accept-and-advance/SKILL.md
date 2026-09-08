---
name: accept-and-advance
description: Automatically accept one qualified committed MetaFlux Iteration delivery, compose Batch integration, advance the canonical lane and target state, publish the acceptance commit, and expose the next dependency-ready lane without waiting for another user message.
---

# Accept And Advance

Invoke this skill automatically in the same controlling turn after `start-work`
produces one committed Iteration delivery. This is the concrete transition
controller for decision-0052. A phrase such as "almost complete" is not
evidence: automatic acceptance starts only when the delivery contains exact
committed revisions, passing focused tests, no blockers, and the active
Epoch/Batch/Iteration/lane identity.

Ordinary status questions, uncommitted patches, design reviews, and partial test
results do not trigger this workflow. They remain in the current Iteration.

## Delivery Contract

Pass one transient JSON document from `tmp/work/` to the controller:

```json
{
  "schema_version": 1,
  "epoch": "epoch-NNNN",
  "batch": "batch-NNNN",
  "iteration": "iteration-NNNN",
  "lane": "lane-slug",
  "base_revision": "FULL_40_CHARACTER_GIT_OID",
  "tip_revision": "FULL_40_CHARACTER_GIT_OID",
  "tests": [
    {"command": "exact focused test command", "status": "passed"}
  ],
  "blockers": [],
  "roast_candidates": []
}
```

The document is transient execution input, never a tracked progress record.
Run the read-only preflight before reviewing or merging:

```sh
nix develop . --command python3 -B \
  agent/skills/accept-and-advance/scripts/accept_and_advance.py \
  check tmp/work/DELIVERY.json --root .
```

Preflight requires a clean current-main context, rejects an empty `base..tip`,
and verifies the goal identity, current target, dependency closure, full commit
objects, Epoch activation ancestry, test evidence, and blockers. It returns
`in-place` when the candidate is current `HEAD`, `merge` for a clean divergent
candidate, plus the next lane that would become dependency-ready.

## Acceptance

1. Read the target work item, its Exit Gate, the exact `base..tip` diff, and all
   materially affected domain skills. Reject drift or product semantics outside
   the assigned lane.
2. Invoke `integrate-batch` as the lower-level merge and verification workflow.
   For `in-place`, retain the candidate history. For `merge`, prepare
   `git merge --no-commit --no-ff TIP`; resolve only bounded composition gaps
   and preserve the candidate as a parent.
3. Run every reported focused command again, then the affected component gates
   and combined Batch regression. Reported `passed` values qualify entry to
   review; they never replace fresh integration evidence.
4. Invoke `roast`. Promote material claims to one canonical source, test,
   contract, plan, decision, constraint, or experience owner and discard
   duplicate process material.
5. After the candidate and combined tree pass, advance state:

```sh
nix develop . --command python3 -B \
  agent/skills/accept-and-advance/scripts/accept_and_advance.py \
  advance tmp/work/DELIVERY.json --root .
```

The controller marks the accepted lane and work item complete, activates the
first dependency-ready planned lane in Iteration order, updates `target`, and
marks the Batch integrated when no planned lane remains. It validates the new
Agent state and restores its own writes if that validation fails. Repeating an
already committed transition is a no-op.
6. Run the final state, routing, affected domain, component-graph, and combined
   gates. Commit the candidate plus acceptance transition through the
   `start-work` commit helper. Resolve the full acceptance commit and invoke
   `push-repository` for exactly that object.
7. When the controller reports a next lane, expose that exact
   Epoch/Batch/Iteration/lane/work-item tuple as the current route immediately.
   If the application has already supplied its worker context, invoke
   `start-work` in the same controlling turn. Creating a task, thread, branch,
   worktree, clone, or worker remains application-owned and is not inferred
   from the state transition.

## Failure And Repair

An invalid or empty candidate, failed focused or combined gate, unresolved
blocker, merge conflict that changes lane meaning, or state validation failure
leaves the lane planned and the target unchanged. Preserve the exact diagnostic
and continue a bounded repair Iteration on that same lane; do not skip forward,
mark partial progress, or manufacture a replacement execution context.

Use the `start-work` task-stop schema. Controller manifest failures are
`acceptance.delivery-invalid`; empty candidates are
`acceptance.candidate-empty`; mismatched or stale history is
`acceptance.candidate-invalid`; incomplete evidence is
`acceptance.not-qualified`; invalid integration context is
`acceptance.context-invalid`; and a failed post-transition state gate is
`acceptance.state-invalid`.

## Output

Report the accepted or rejected delivery identity, integration mode, exact
base/tip, freshly run tests, roast promotions, acceptance commit, resulting
Batch/lane/target state, push result, and next dependency-ready lane. Do not
create a progress diary, acceptance archive, or duplicate route database.
