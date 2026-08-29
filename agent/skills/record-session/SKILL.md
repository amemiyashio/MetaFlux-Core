---
name: record-session
description: Record and close a repository work session by preserving durable outcomes, cleaning session-owned failed-route artifacts, and leaving a compact resume point.
---

# Record And Clean A Session

Use for any task that changes durable repository state. A session is a curated
work ledger and cleanup boundary, not a source snapshot, command transcript,
build cache, or evidence warehouse. Git owns recoverable source history.

## Steps

1. Scaffold before the first durable edit with
   `python3 tools/new-session.py <slug>`. Replace the objective immediately.
2. Record only material decisions, non-obvious commands, verification results,
   and findings needed to resume or reproduce the outcome. Omit routine command
   chatter and raw output that does not change a decision.
3. Keep durable source and configuration in Git. Do not copy the source tree,
   build directory, dependency store, downloaded packages, or ordinary logs
   into the session. Store a compact output only when an acceptance claim
   genuinely depends on its exact bytes and no canonical artifact owns it.
4. Before handoff, enumerate paths created by this session and classify them:
   durable Git content; promoted canonical evidence; or disposable work.
   Remove disposable build trees, duplicate source snapshots, failed-route
   files, temporary downloads, profiles, and logs. Preserve user changes and
   artifacts owned by other sessions or concurrent work.
5. If an abandoned route contains a reusable lesson, retain one concise
   `work_note` or promote a validated experience. Otherwise remove the route
   and omit its noise. Never rewrite completed historical sessions or
   checkpoints to make the old route appear successful.
6. Fill `summary.md`. `## Cleanup` names removed and intentionally retained
   artifacts (`none` is valid after inspection). `## Distillation` names the
   durable records that received reusable knowledge (`none` is valid).
7. Fill `session.json` from current facts: agents, honest milestone/work-item
   statuses, revisions from `git rev-parse`, and final status. Update the
   sessions index with a one-line outcome.
8. Refresh `progress/current.md` and add a compact checkpoint only for a
   material handoff boundary. When commits are part of the repository workflow,
   keep content and session/checkpoint records separate; the session is never a
   substitute for Git history.

## Cleanup Boundaries

- Resolve exact task-owned paths before deletion. Avoid broad globs and never
  clean the repository, home directory, Nix store, or shared cache as a generic
  session-finalization step.
- Host Nix GC is operator policy. A project session may report dead-store
  evidence, but it does not create GC roots, tune thresholds, or run GC unless
  that host operation is explicitly in scope.
- A failed test does not by itself make its source change disposable. Remove a
  route only after identifying why it is superseded and which retained change
  replaces it.

## Verification

```sh
python3 tools/check-agent-records.py .
```

The gate checks index completeness, cleanup and distillation sections,
checkpoint freshness, and status consistency. Inspect `git status` and the
task-owned work directories separately to prove cleanup actually happened.
