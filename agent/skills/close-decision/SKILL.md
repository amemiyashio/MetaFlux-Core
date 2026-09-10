---
name: close-decision
description: Resolve one row of the open-decisions ledger into a durable decision record without leaving drift behind.
---

# Close a Decision

Use when resolving any row of
[the open-decisions ledger](../../memory/open-decisions.md). The validator
counts each plan's open items against ledger rows, so a half-closed decision
fails validation. Follow every step.

## Steps

1. Assemble the evidence the row's closure condition names. If the condition
   says "only after <workstream> evidence", that evidence must already exist.
2. Write the decision into its canonical source: the owning milestone plan
   (edit its "Decisions to Close" item to state the resolution) or a
   `docs/architecture/` record. A decision without a canonical source cannot
   be closed.
3. Add the next `decision-NNNN` row to
   [decisions-index](../../memory/decisions-index.md) pointing at that
   canonical source with an honest source status; the index never promotes
   `Proposed` material to `Verified`.
4. Remove the row from the ledger. If only part of the item closed (see the
   glibc baseline under decision-0009), rewrite the row to the remaining open scope
   instead of deleting it, and keep the plan item count matching.
5. Promote the consequence into `memory/` when it is a durable constraint.
   Keep implementation work in the assigned Iteration and return its exact
   base/tip revisions; only `batch` may update delivery-time
   `agent/goal.json`.

## Verification

```sh
nix develop . --command python3 tools/check-agent-state.py .
```

The validator proves structural identity: every numbered open plan decision
matches exactly one ledger `Decision` cell, duplicates fail, decision-index IDs
are unique, and every `decision-NNNN` reference resolves to the index. It does
not prove the technical outcome; the new index row and amended plan must still
link the canonical rationale and qualification evidence.
