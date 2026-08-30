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
   then the [semantic-change index](../../semantic-changes/README.md), [current
   progress](../../progress/current.md), and the active milestone plan. Load
   only an Active SC or an Applied SC relevant to the task.
2. Check [skills](../README.md) for one matching the task (add-component,
   close-decision, record-session, ...) and follow it verbatim instead of
   improvising.
3. If the task replaces established semantics, identifiers, constraints,
   record shape, or authority, follow `govern-semantic-change` before the first
   affected edit. If it resolves a ledger row, compose `close-decision`. A
   compatible implementation correction does not create an SC.
4. If the task will change durable repository state, scaffold the session before
   the first file change:

   ```sh
   python3 tools/new-session.py <MAJOR.MINOR.PATCH.WORK> <slug>
   ```

   Use the narrowest useful delivery scope from
   [`docs/release-versioning.md`](../../../docs/release-versioning.md). For a
   read-only task, do not create an empty session.
5. When resuming an active session, inspect only whether its `guidance/` inbox
   contains a ready packet. Do not load guidance as routine session context. If
   one exists, or the user explicitly requests guidance publication or
   processing, load and follow
   [`session-guidance`](../session-guidance/SKILL.md) before the next coherent
   work unit. The session owner validates the packet and assigns its
   disposition; a specialist acting as its author does not edit product source.
6. Make the change and monitor both the breakthrough trigger in `record-session`
   and the guidance control boundary. Recheck for ready guidance after a
   specialist or colleague completion notice and before beginning the next
   coherent work unit; do not interrupt a long-running command solely to poll.
   When a coherent independently valuable stage passes its focused gates,
   checkpoint it before entering the next risk or scope phase; do not wait for
   the entire task to finish. The semantic trigger belongs to the agent, while
   the pre-commit hook only validates an attempted commit.
7. Before checkpoint or close, use `distill-project-knowledge` to route each
   valuable claim to one canonical owner or mark it session-only with a reason.
   Verify with `python3 tools/check-agent-records.py .` and the relevant CTest
   preset for build-affecting files. Use `record-session` in checkpoint mode for
   separate content/record commits and in close mode for cleanup, progress
   refresh, and final handoff. A read-only task reports its evidence directly.

## Verification

```sh
python3 tools/check-agent-records.py .
```

Passing means existing records, decision identities, indexes, and skill packages
meet the repository gates. A changing task additionally needs its in-progress
session; a read-only task intentionally leaves no new record.
