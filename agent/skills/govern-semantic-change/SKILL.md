---
name: govern-semantic-change
description: Govern a decision-authorized breaking change to repository semantics, identifiers, constraints, record shapes, or authority by synchronizing every affected current and historical record, protecting factual evidence, and handing the change to active sessions. Do not use for compatible implementation edits or ordinary decision closure.
---

# Govern Semantic Change

Use this skill only after the replacement meaning is approved by the user or a
canonical `DNNNN` decision. If the choice is still open, resolve it through the
existing decision workflow first. The matching domain skill continues to own
technical meaning; this skill owns migration completeness and handoff.

Read [the record and migration protocol](references/protocol.md) before creating
or changing an `SCNNNN` record.

## Trigger Boundary

Use an SC when an existing identifier, durable constraint, record contract,
ownership boundary, compatibility promise, or interpretation must be replaced
and mixed old/new meaning would mislead later work. A normal additive feature,
compatible implementation correction, local refactor, or raw expert suggestion
does not create an SC.

Compose with `close-decision` when an open ledger row is being resolved,
`session-guidance` for affected active sessions, the relevant domain skill for
technical semantics, and `record-session` for verified Git checkpoints.

## Workflow

1. Identify the canonical decision and the exact old/new semantic replacement.
   Create the next monotonic `SCNNNN` record as `Active` before touching a
   protected historical surface.
2. Search the whole tracked repository for affected current and historical
   surfaces. Put every affected file as one exact path in the migration
   inventory. Group findings only in explanatory prose or verification output,
   never in a Surface row.
3. Mark factual evidence as locked. Preserve timestamps, commands, output,
   counts, revisions, hashes, and observed facts. Correct current interpretation
   by marking an invalid old conclusion `invalidated` or `pending
   re-verification`; never manufacture a new pass.
4. Publish one transient guidance packet to every affected `in_progress`
   session except the migration owner. Track publication and later disposition
   in the SC, but leave record edits, event recording, and packet removal to the
   target owner. Do not list another owner's active-session files as Pending
   inventory surfaces merely to represent the handoff.
5. Update the canonical owner first, then every affected current and historical
   record in one semantic migration. Git keeps the prior form. Old wording may
   remain only as registered retained evidence or an explicit compatibility
   surface.
6. Run focused behavior checks, Agent gates, exact residual searches, and a diff
   audit of locked evidence. Resolve every `Pending` inventory row with concrete
   proof.
7. Commit the coherent migration, then set the SC to `Applied` with that content
   revision. Use `record-session` for the separate record checkpoint. A later
   replacement preserves the old SC and marks it `Superseded`.

## Historical Edit Gate

A terminal session or recorded checkpoint is editable only when its exact path
appears as a `Historical` row in an `Active` SC already committed to `HEAD`, tied
to a resolvable decision and an in-progress migration session. Add the row in a
separate authorization commit before the edit. The edit hook and pre-commit gate
reject broader, stale, or same-commit exceptions.

## Output

Leave one coherent current checkout, one durable SC reminder, compact guidance
dispositions, and Git commits that preserve the prior state. Do not create a
worktree snapshot, advice archive, generic migration log, or duplicate source of
technical truth.

## Verification

At minimum run:

```sh
python3 tools/check-agent-records.py .
python3 tools/test-check-agent-records.py
```

Also run relevant domain tests, Skill routing gates when descriptions or routing
change, exact old-marker searches named by the SC, and `git diff --check`.
