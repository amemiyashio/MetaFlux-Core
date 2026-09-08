---
name: integrate-batch
description: Review, merge, and verify one qualified MetaFlux Batch candidate from exact committed Iteration revisions when composed by accept-and-advance or explicitly requested for integration repair; do not own automatic routing or Goal advancement.
---

# Integrate Batch

`accept-and-advance` invokes this lower-level skill after its controller accepts
an exact committed candidate whose dependencies and reported focused gates are
ready. A user may explicitly invoke `$integrate-batch` to inspect or repair
integration mechanics. This skill never self-routes from completion language,
updates `agent/goal.json`, selects the next lane, or creates an acceptance
commit. The application supplies the integration context and candidate
revisions; this skill does not create either.

## Inputs

Require the supplied integration context to already be a clean worktree at the
current main revision and require one entry per candidate:

```text
lane
epoch-NNNN / batch-NNNN / iteration-NNNN
base_revision
tip_revision
tests
roast_candidates
```

Accept only committed Git objects. Reject staged, unstaged, untracked, inferred,
or path-only delivery. Resolve each revision to a full hash; require base to be
an ancestor of tip and the current Epoch activation commit to be an ancestor of
base. Epoch, Batch, lane, and Iteration must match `agent/goal.json`.

A planned lane without a supplied committed candidate is a missing input, not
authorization to create a worker, subtask, branch, worktree, clone, thread, or
chat. Report the exact missing delivery and leave its lane state unchanged.
Bounded read-only subagents may review independent `base..tip` ranges, test
evidence, or dependency ordering when useful. The integrator remains the sole
merge owner; `accept-and-advance` remains the `goal.json` owner. Subagents do
not edit authority or create execution contexts. A subagent mechanism that
creates an independent execution context still requires explicit user or
application authorization.

Run the ancestry gate for every delivery before merge preparation:

```sh
nix develop . --command python3 tools/check-agent-state.py . \
  --integration-base BASE_REVISION --integration-tip TIP_REVISION
```

## Integration

1. Read the goal, target Exit Gate, lane dependencies, and every materially
   touched domain skill.
2. Inventory each exact `base..tip` range and reject unrelated scope,
   incompatible product semantics, generated residue, or missing verification.
3. Order candidates topologically by lane dependencies. When `TIP` equals the
   integration context's `HEAD`, retain that linear candidate history and
   prepare an in-place acceptance commit. When `TIP` diverges from `HEAD`,
   prepare `git merge --no-commit --no-ff TIP` so the original lane history
   remains a parent of the integration commit. Reject a non-HEAD ancestor as a
   stale delivery rather than silently accepting an obscured candidate.
4. Resolve bounded merge and composition gaps in the merge. If resolution would
   redefine the lane outcome, abort that merge and request a new Iteration.
5. Run focused tests before committing each non-final merge. Keep all integration
   commits in the supplied integration context; do not create another branch or
   worktree, and leave main unchanged until promotion.
6. Run the combined Batch regression on the final in-place candidate or
   prepared merge, then return the exact tested revisions and evidence to
   `accept-and-advance`. That controller owns `roast`, Goal/work-item state
   advancement, the final acceptance commit, exact push, and next-lane
   selection.

If any focused or combined gate fails, do not publish the merge or goal state.
Fix a bounded integration defect in the same Batch, or return the exact defect
for a new Iteration. Do not create an integration report, checkpoint, or Batch
history file.

## Task Stops

Use the `start-work` task-stop contract. A missing integration context is
`integration.context-invalid` with `user-or-application /
preserve-and-report`.
A planned lane without an exact committed delivery is
`integration.candidate-missing` with the same external responsibility; it is
never a dispatch signal. Revision ancestry errors come from the state checker
unchanged. A focused or combined regression failure is
`verification.required-gate-failed`; the Batch integrator may repair only a
bounded integration defect, otherwise it preserves Goal state and requests a
new committed Iteration.

## Output

Report accepted/rejected Iterations, merge revisions, focused and combined test
results, and the exact tested integration tip to `accept-and-advance`.
