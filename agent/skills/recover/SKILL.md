---
name: recover
description: Diagnose and recover one interrupted MetaFlux operation, amend an omitted same-task path, preserve pending acceptance edits, or restore publication from an exact committed revision.
---

# Recover

Role: parent responsible for the current failure or interrupted operation.
First action: inspect the current action card and raw failing check/log.
Read [diagnostics](references/diagnostics.md); for state repair read
[recovery protocol](references/recovery.md).

A running verification uses its existing session. A concrete failed cause stays
in the current scope: repair it, then load implementation/review/verification
modules as directed. Repeating unchanged evidence does not establish progress.
A staging-only delivery rejection goes directly back to [$deliver](../deliver/SKILL.md) skill.

Use `main.py resume` for interrupted work, not missing git add.
Lost pre-commit state needs new review and actual checks. A committed operation
recovers from its exact Git workflow record; `resume --revision FULL_COMMIT`
restores publication/handoff without new implementation, acceptance or tests.

Use `rescope` for necessary omitted companion paths within the existing task.
An explicitly confirmed Epoch may `supersede` an uncommitted operation after
preserving unrelated edits. Neither command grants authority or accepts work.
Never edit receipts or remove state to manufacture recovery.

Completion: the concrete cause is resolved and the action card names the valid
next step, or the diagnostic identifies the exact external responsibility and
required input. Preserve raw child output and useful independent in-scope work.
