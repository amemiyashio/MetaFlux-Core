# Candidate Delivery

An Iteration delivery has exactly these schema-v2 fields:

```json
{
  "schema_version": 2,
  "epoch": "epoch-NNNN",
  "batch": "batch-NNNN",
  "iteration": "iteration-NNNN",
  "lane": "lane-slug",
  "base_revision": "FULL_BASE",
  "tip_revision": "FULL_TIP",
  "acceptance_kind": "slice",
  "slice_objective": "The observed bounded result",
  "tests": [{"command": ["PROGRAM", "ARG"], "status": "passed"}],
  "verification_receipt": {},
  "exit_gate": null,
  "blockers": [],
  "knowledge_candidates": []
}
```

Replace the empty receipt with the actual candidate receipt. Tests mirror its
executed argv and status (`passed` or `optional-skip`). A `work-item` delivery
sets `exit_gate` to `{ "digest": "SHA256_OF_EXIT_GATE_JSON_STRING", "checks":
["CHECK_ID"] }`. Hash the exact trimmed body under the current `## Exit Gate`
using `workflow_state.digest()`. The mapped checks must pass in both candidate
and integration phases. The parent reviews their full semantic coverage.
