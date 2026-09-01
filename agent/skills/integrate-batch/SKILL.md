---
name: integrate-batch
description: Integrate one explicitly requested MetaFlux Batch from exact committed Iteration revisions, preserve lane history, run focused and combined tests, and update goal state only on success.
---

# Integrate Batch

Use only when the user explicitly creates an integration run or invokes
`$integrate-batch`. Ordinary worker completion does not trigger it.

## Inputs

Require a clean integration worktree at the current main revision and one entry
per candidate:

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
3. Order candidates topologically by lane dependencies. Prepare each merge with
   `git merge --no-commit --no-ff TIP` so the original lane history remains a
   parent of the integration commit.
4. Resolve bounded merge and composition gaps in the merge. If resolution would
   redefine the lane outcome, abort that merge and request a new Iteration.
5. Run focused tests before committing each non-final merge. Keep all integration
   commits on the isolated integration branch; main remains unchanged.
6. For the final merge, update accepted lane states and Batch state in
   `agent/goal.json`, invoke `roast`, and apply any canonical promotions. Run the
   combined Batch regression before creating the final merge commit.
7. Commit through the `start-work` helper with the current Epoch. Promote the
   tested integration tip to main only by an exact fast-forward or explicit
   user-directed merge.

If any focused or combined gate fails, do not publish the merge or goal state.
Fix a bounded integration defect in the same Batch, or return the exact defect
for a new Iteration. Do not create an integration report, checkpoint, or Batch
history file.

## Output

Report accepted/rejected Iterations, merge revisions, focused and combined test
results, promoted roast claims, the resulting goal state, and the exact tested
integration tip.
