---
name: start-work
description: Cold-start repository work so rules and execution focus are read, the matching skill is followed, and durable changes use the exact focus owner.
---

# Start Work

Use at the beginning of every task. A read-only inspection loads the required
context but does not create a session. A changing task resumes the exact
execution-focus owner when its objective matches. A newly scaffolded session is
only a ledger until the current owner transfers focus to it.

## Steps

1. Read in order: [agent rules](../../README.md) (including "Before changing
   anything"), [durable memory](../../memory/README.md) — especially
   [constraints](../../memory/constraints.md) and the
   [open-decisions ledger](../../memory/open-decisions.md) for your area —
   then the machine [execution focus](../../progress/focus.json), the
   [semantic-change index](../../semantic-changes/README.md), [current
   progress](../../progress/current.md), and the focused milestone/work item and
   its Exit Gate. Load only an Active SC or an Applied SC relevant to the task.
2. Check [skills](../README.md) for one matching the task (add-component,
   close-decision, record-session, ...) and follow it verbatim instead of
   improvising.
3. If the task replaces established semantics, identifiers, constraints,
   record shape, or authority, follow `govern-semantic-change` before the first
   affected edit. If it resolves a ledger row, compose `close-decision`. A
   compatible implementation correction does not create an SC.
4. Compare the requested durable outcome with `progress/focus.json`. If it fits
   the named target or governance authority, resume the exact owner session. If
   it does not fit, stop before content edits: the current owner must route a
   record-only focus handoff before the new direction begins. An unrelated
   `in_progress` session and an easily testable local increment do not redirect
   focus.
5. When a new ledger is needed, scaffold it before its first durable change:

   ```sh
   python3 tools/new-session.py <MAJOR.MINOR.PATCH.WORK> <slug>
   ```

   Use the narrowest useful delivery scope from
   [`docs/release-versioning.md`](../../../docs/release-versioning.md). For a
   read-only task, do not create an empty session. Scaffolding does not claim
   execution focus or content-commit authority; a successor becomes owner only
   through an atomic record-only handoff from the current owner.
6. When resuming an active session, inspect only whether its `guidance/` inbox
   contains a ready packet. Do not load guidance as routine session context. If
   one exists, or the user explicitly requests guidance publication or
   processing, load and follow
   [`session-guidance`](../session-guidance/SKILL.md) before the next coherent
   work unit. The session owner validates the packet and assigns its
   disposition; a specialist acting as its author does not edit product source.
7. After a collaborator or subagent reports durable source or record changes,
   load and follow
   [`converge-project-changes`](../converge-project-changes/SKILL.md) before the
   next coherent work unit. Review the exact delivered batch, not every ambient
   worktree change. The current integration session repairs only its owned
   compatible gaps; another active owner receives `session-guidance`, and a
   breaking replacement follows `govern-semantic-change`.
8. Make the change and monitor both the breakthrough trigger in `record-session`
   and the guidance control boundary. Recheck for ready guidance after a
   specialist or colleague completion notice and before beginning the next
   coherent work unit; do not interrupt a long-running command solely to poll.
   When a coherent independently valuable stage passes its focused gates,
   checkpoint it before entering the next risk or scope phase; do not wait for
   the entire task to finish. The semantic trigger belongs to the agent, while
   the pre-commit hook only validates an attempted commit.
9. Before checkpoint or close, invoke `$roast` explicitly. Route each material
   outcome to a canonical unresolved owner, the independent `session-only`
   disposition, or one materially updated canonical owner and roast depth.
   Verify with `python3 tools/check-agent-records.py .` and the relevant CTest
   preset for build-affecting files. Use `record-session` in checkpoint mode for
   separate content/record commits and in close mode for cleanup, progress
   refresh, and final handoff. A read-only task reports its evidence directly.

## Agent Commit Identity

When an agent creates a Git commit, both Git Author and Committer identify the
active agent harness, not the human identity stored in `.git/config`. This
applies to content, checkpoint, and closing-record commits. Human-created
commits outside an agent run are unaffected.

Before the first agent-created commit, read the harness subject from the active
agent runtime context and emit this declaration in the interaction:

```text
Agent harness subject: <subject>
```

This is an agent self-report. Do not infer it from `/proc`, process names,
product-specific environment namespaces, repository contents, or the user's Git
configuration. Repeat the declaration whenever work passes to another agent or
harness. Then provide that same normalized subject command-locally to the
package helper instead of invoking `git commit` directly:

```sh
METAFLUX_AGENT_HARNESS=HARNESS_SUBJECT \
  METAFLUX_SESSION_ID=FOCUS_OWNER_SESSION \
  python3 agent/skills/start-work/scripts/commit_as_harness.py -- -m "Commit subject"
```

Under [D0028](../../../docs/architecture/agent-harness-commit-identity.md), the
helper consumes only the agent-provided `METAFLUX_AGENT_HARNESS` declaration for
Git identity. The pre-commit gate separately consumes
`METAFLUX_SESSION_ID` under D0029 and compares it with the exact candidate focus
owner. The helper
contains no `/proc` reader, process or environment heuristic, Codex/Claude
Code/ZCode product table, or `--harness` selector. A missing or malformed
declaration stops before Git runs.

The normalized subject generates both roles as `Agent Harness (<subject>)
<<subject>@localhost>`. The declaration is a provenance label supplied by the
agent after reading its harness context; it is not an authentication credential
or a preferred identity to choose ad hoc. The helper never falls back to a
repository user's identity. It sets the result only for the child `git commit`,
leaves local and global Git configuration unchanged, and rejects `--author`,
`--amend`, and message-reuse options that could carry another commit's
authorship into the new commit.

When an agent operation would normally create a merge, revert, or cherry-pick
commit, first prepare the result without committing (`git merge --no-commit
--no-ff`, `git revert --no-commit`, or `git cherry-pick --no-commit`), then
create the commit through this helper. A fast-forward creates no new commit and
therefore retains the existing commit's original identity.

After each commit, verify the recorded identity before reporting its revision:

```sh
git show -s --format='Author: %an <%ae>%nCommitter: %cn <%ce>' HEAD
```

To validate the declaration before staging or committing, use the same
command-local subject:

```sh
METAFLUX_AGENT_HARNESS=HARNESS_SUBJECT \
  python3 agent/skills/start-work/scripts/commit_as_harness.py --print-identity
```

## Verification

```sh
python3 agent/skills/start-work/scripts/test_commit_as_harness.py
python3 tools/check-agent-records.py .
```

Passing means existing records, decision identities, indexes, and skill packages
meet the repository gates. A changing task additionally needs its exact
in-progress focus owner and a matching `METAFLUX_SESSION_ID`; a read-only task
intentionally leaves no new record.
