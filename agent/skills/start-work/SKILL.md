---
name: start-work
description: Cold-start repository work so rules are read, the matching skill is followed, and any durable change gets a session before the first edit.
---

# Start Work

Use at the beginning of every task. A read-only inspection loads the required
context but does not create a session. Any task that will change repository
state scaffolds its session before the first edit so the record machinery can
cover the complete change.

## Steps

1. Read in order: [agent rules](../../README.md) (including "Before changing
   anything"), [durable memory](../../memory/README.md) — especially
   [constraints](../../memory/constraints.md) and the
   [open-decisions ledger](../../memory/open-decisions.md) for your area —
   then [current progress](../../progress/current.md) and the active
   milestone plan.
2. Check [skills](../README.md) for one matching the task (add-component,
   close-decision, record-session, ...) and follow it verbatim instead of
   improvising.
3. If your task resolves a ledger row, follow the `close-decision` skill; if
   it would relax anything in constraints, stop: that requires a recorded
   decision and a canonical source, not an edit.
4. If the task will change durable repository state, scaffold the session before
   the first file change:

   ```sh
   python3 tools/new-session.py <slug>
   ```

   For a read-only task, do not create an empty session.
5. Make the change and monitor the breakthrough trigger in `record-session`.
   When a coherent independently valuable stage passes its focused gates,
   checkpoint it before entering the next risk or scope phase; do not wait for
   the entire task to finish. The semantic trigger belongs to the agent, while
   the pre-commit hook only validates an attempted commit.
6. Verify with `python3 tools/check-agent-records.py .` and the relevant CTest
   preset for build-affecting files. Use `record-session` in checkpoint mode for
   separate content/record commits and in close mode for distillation, cleanup,
   progress refresh, and final handoff. A read-only task reports its evidence
   directly.

## Verification

```sh
python3 tools/check-agent-records.py .
```

Passing means existing records, decision identities, indexes, and skill packages
meet the repository gates. A changing task additionally needs its in-progress
session; a read-only task intentionally leaves no new record.
