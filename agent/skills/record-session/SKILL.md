---
name: record-session
description: Checkpoint a verified stage breakthrough or close a repository work session while preserving durable outcomes, cleaning session-owned failed routes, and leaving a compact resume point.
---

# Checkpoint And Close A Session

Use during any task that changes durable repository state. It has two modes:

- **Checkpoint mode** preserves a verified stage breakthrough in Git while the
  session remains active.
- **Close mode** records the final outcome, cleans session-owned disposable
  work, and moves the session to a terminal status.

A session is a curated work ledger and cleanup boundary, not a source snapshot,
command transcript, build cache, or evidence warehouse. Git commits own
recoverable source history.

## Breakthrough Trigger

Create a checkpoint commit as soon as all of these are true:

1. The completed outcome or newly enforced invariant can be named in one
   sentence and has durable value on its own.
2. The relevant focused tests or verification gates pass for that outcome.
3. The diff is coherent enough to review, revert, and resume independently.
4. Continuing would enter a new risk/scope phase or make this boundary harder
   to recover from history.

Do not wait for the entire task or session to end. Elapsed time, file count,
routine formatting, generated output, exploratory edits, or a red test suite do
not constitute a breakthrough. Do not checkpoint unrelated user changes, and
honor an explicit user request to keep work uncommitted.

## Steps

1. Scaffold before the first durable edit with
   `python3 tools/new-session.py <MAJOR.MINOR.PATCH.WORK> <slug>`. Use the
   narrowest useful delivery scope and replace the objective immediately.
2. Record only material decisions, non-obvious commands, verification results,
   and findings needed to resume or reproduce the outcome. Omit routine command
   chatter and raw output that does not change a decision.
3. Keep durable source and configuration in Git. Do not copy the source tree,
   build directory, dependency store, downloaded packages, or ordinary logs
   into the session. Store a compact output only when an acceptance claim
   genuinely depends on its exact bytes and no canonical artifact owns it.
4. Before checkpoint or close, inspect the active session for ready guidance.
   Load [`session-guidance`](../session-guidance/SKILL.md) only when a packet
   exists or processing was explicitly requested. The session owner claims it,
   validates the proposal against current source and evidence, records any
   material disposition, and removes the raw packet and attachments. Persist a
   deferred item in the existing unresolved work or open-decision flow and
   record that target before removing its packet.
5. At a breakthrough trigger, stop expanding scope. Inspect `git status` and
   the diff, identify the exact files owned by the breakthrough, and run its
   focused verification. Stage only that coherent content and create an
   outcome-named Git commit. The pre-commit hook validates the attempt; it does
   not decide when a breakthrough exists or invoke `git commit` itself.
6. After the content commit, append its revision and verification result to the
   active session. Refresh `progress/current.md` and add a compact checkpoint
   only when the commit is also a material handoff boundary. Commit these
   session/checkpoint records separately from content. If work continues, keep
   `status: in_progress`, `ended_at: null`, and `final_revision: null`.
7. Before close or handoff, invoke `$roast` explicitly: split material outcomes
   into independent claims; route unresolved choices to their canonical owner;
   retain bounded local material under the independent `session-only`
   disposition; and classify each materially promoted claim under one canonical
   owner and roast depth. Keep only links and evidence identities in the
   session. Then enumerate paths created by this session and classify them:
   durable Git content; promoted canonical evidence; or disposable work.
   Remove disposable build trees, duplicate source snapshots, failed-route
   files, processed guidance packets, temporary downloads, profiles, and logs.
   Preserve user changes and artifacts owned by other sessions or concurrent
   work. A terminal session's guidance inbox must contain no file or symbolic
   link.
8. If an abandoned route contains a reusable lesson, retain one concise
   `work_note` or promote a validated experience. Otherwise remove the route
   and omit its noise. Never rewrite completed historical sessions or
   checkpoints to make an old route appear successful. A decision-authorized
   semantic synchronization follows `govern-semantic-change`, preserves factual
   evidence, and requires its exact protected paths in an Active SC already in
   `HEAD`.
9. In close mode, fill `summary.md`. `## Cleanup` names removed and intentionally
   retained artifacts (`none` is valid after inspection). Lowercase `## roast`
   contains the ordered `light roasts`, `medium roasts`, and `dark roasts`
   promotion maps; the following lowercase `## session-only` contains only
   bounded local-retention reasons. `none` is valid after classification, and
   neither section copies canonical content.
10. Fill `session.json` from current facts: agents, honest milestone/work-item
   statuses, the final content revision, end time, and terminal status. Update
   the sessions index with a one-line outcome, validate the records, and create
   the separate closing record commit.

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

The gate checks index completeness, cleanup, roast and session-only sections,
checkpoint freshness, and status consistency. Inspect `git status` and the
task-owned work directories separately to prove cleanup actually happened. A
checkpoint is complete only when the content commit exists and its active
session record is either committed separately or explicitly pending as the
next operation.
