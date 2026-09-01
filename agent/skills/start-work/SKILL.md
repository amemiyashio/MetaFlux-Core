---
name: start-work
description: Cold-start repository work with exact runtime harness resolution, Nix-first tool entry, governed context loading, matching skill routing, and exact focus ownership.
---

# Start Work

Use at the beginning of every task. A read-only inspection loads the required
context but does not create a session. A changing task resumes the exact
execution-focus owner when its objective matches. A newly scaffolded session is
only a ledger until the current owner transfers focus to it. Current execution
requires schema version 2 and `governance_epoch: D0029` on both focus and owner;
a schema version 1 or pre-epoch ledger is evidence, never resumable authority.

## Stage Zero: Resolve Runtime And Enter Nix

Complete this stage before the numbered workflow and before invoking any shell
executable other than the host bootstrap `git` and `nix` commands.

1. Read the stable harness product slug directly from the active executor's
   system/developer runtime instruction context. For Codex, the subject is
   exactly `codex`. A model, prompt template, backend, build, CLI, session,
   thread, user-message, repository-file, or inherited prior-agent label is not
   the harness subject.
2. Emit `Agent harness subject: <subject>` immediately. Do not search for an
   agent binary or CLI and do not inspect `PATH`, `which`, `command -v`,
   `env`, `ps`, `/proc`, product environment namespaces, or repository
   text to discover or infer the subject. If active runtime instruction context
   does not identify it, stop before staging or durable changes and report that
   exact missing prerequisite.
3. Use the Git-aware `nix develop . --command ...` entry point for every other
   executable, including `rg`, `sed`, `jq`, Python, version/capability
   probes, repository scripts, compilers, CMake, Ninja, CTest, packaging, and
   qualification tools. Repository file APIs may read files directly without a
   shell. Host `git` may inspect source identity, topology, status, and diffs;
   host `nix` may enter the declared environment. Never probe ambient host
   tools first and never use `path:.`.
   If a required tool is absent, follow `manage-toolchain` and add it to the
   repository Nix declaration before use. Only after Nix is confirmed not to
   provide or materialize the tool may the agent stop and tell the host operator
   exactly what must be installed. Do not silently use an ambient copy or run a
   host package manager.
   Adding or entering a Nix tool closure changes only tool identity,
   materialization, and exposure. Nix must not own or encode task routing,
   source history, build/test/package commands, qualification semantics,
   focus/session policy, evidence, cleanup, or host installation. A fixed tool
   may evolve through an explicit `manage-toolchain` manifest/lock update;
   fixed means revision-clear and reproducibly stable, not permanently frozen.
4. Before staging the first agent-created commit, run the identity preflight
   inside that Nix environment and compare the complete output with the emitted
   declaration. A mismatch stops the commit path.

## Steps

1. Using repository file APIs or Nix-provided read tools, read in order:
   [agent rules](../../README.md) (including "Before changing anything"),
   [durable memory](../../memory/README.md) — especially
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
   the named target or governance authority, require the exact current schema
   and epoch on both focus and owner, then resume that owner session. If it does
   not fit, stop before content edits: the current owner must route a record-only
   focus handoff before the new direction begins. An unrelated `in_progress`
   session, a legacy objective, and an easily testable local increment do not
   redirect focus. Continuing a legacy objective requires a newly scaffolded
   current-epoch successor; never upgrade, reactivate, or fall back to the old
   ledger.
5. When a new ledger is needed, scaffold it before its first durable change:

   ```sh
   nix develop . --command python3 tools/new-session.py <MAJOR.MINOR.PATCH.WORK> <slug>
   ```

   Use the narrowest useful delivery scope from
   [`docs/release-versioning.md`](../../../docs/release-versioning.md). For a
   read-only task, do not create an empty session. The scaffolder emits the
   current schema version and D0029 epoch. Scaffolding does not claim execution
   focus or content-commit authority; a successor becomes owner only through an
   atomic record-only handoff from the current owner.
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
   Verify with
   `nix develop . --command python3 tools/check-agent-records.py .` and the
   relevant CTest preset inside its declared Nix environment for build-affecting
   files. Use `record-session` in checkpoint mode for separate content/record
   commits and in close mode for cleanup, progress refresh, and final handoff.
   A read-only task reports its evidence directly.

## Agent Commit Identity

When an agent creates a Git commit, both Git Author and Committer identify the
active agent harness, not the human identity stored in `.git/config`. This
applies to content, checkpoint, and closing-record commits. Human-created
commits outside an agent run are unaffected.

Stage Zero supplies the harness subject. Before the first durable edit and again
whenever work passes to another agent or harness, emit this declaration:

```text
Agent harness subject: <subject>
```

This is an agent self-report from active system/developer runtime instruction
context. It is not derived from a model/template identifier, agent CLI search,
repository content, user prompt text, process state, environment namespace, or
Git configuration. Provide that same normalized subject command-locally to the
package helper instead of invoking `git commit` directly:

```sh
METAFLUX_AGENT_HARNESS=HARNESS_SUBJECT \
  METAFLUX_SESSION_ID=FOCUS_OWNER_SESSION \
  nix develop . --command python3 agent/skills/start-work/scripts/commit_as_harness.py -- -m "Commit subject"
```

Under [D0028](../../../docs/architecture/agent-harness-commit-identity.md), the
helper consumes only the agent-provided `METAFLUX_AGENT_HARNESS` declaration for
Git identity. The pre-commit gate separately consumes
`METAFLUX_SESSION_ID` under D0029 and compares it with the exact candidate focus
owner. The helper
contains no `/proc` reader, process or environment heuristic, Codex/Claude
Code/ZCode product table, or `--harness` selector. A missing or malformed
declaration stops before Git runs. Subjects longer than 24 characters and
subjects containing model/template/backend/build/CLI/session/thread/prompt
classification segments are malformed. For a Codex run, any value other than
`codex` contradicts Stage Zero even if it passes generic slug syntax.

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

The following preflight is mandatory before staging the first agent-created
commit. Use the same command-local subject and compare its complete output with
the interaction declaration:

```sh
METAFLUX_AGENT_HARNESS=HARNESS_SUBJECT \
  nix develop . --command python3 agent/skills/start-work/scripts/commit_as_harness.py --print-identity
```

## Verification

```sh
nix develop . --command python3 agent/skills/start-work/scripts/test_commit_as_harness.py
nix develop . --command python3 tools/check-agent-records.py .
```

Passing means existing records, decision identities, indexes, and skill packages
meet the repository gates. A changing task additionally needs its exact schema
version 2, D0029, in-progress focus owner and a matching
`METAFLUX_SESSION_ID`; a read-only task intentionally leaves no new record.
