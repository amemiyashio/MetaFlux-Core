---
name: start-work
description: Cold-start any task in this repository so rules are read, a session exists, and the matching skill is followed before the first change.
status: Active
---

# Start Work

Use at the beginning of every task, before the first file change. It exists
because the repository's machinery only protects agents that onboard through
it.

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
4. Scaffold the session before changing anything outside `agent/`:

   ```sh
   python3 tools/new-session.py <slug>
   ```

5. Make the change; verify with `python3 tools/check-agent-records.py .` and
   the relevant CTest preset for build-affecting files. The pre-commit hook
   enforces both records validity and session coverage.
6. Finish through the `record-session` skill: distillation, decision closure,
   progress refresh, checkpoint, separate content and record commits.

## Verification

```sh
python3 tools/check-agent-records.py .
```

Passing means the session exists, the ledger agrees with the plans, and the
indexes are complete — the on-ramp was used.
