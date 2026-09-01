---
name: start-work
description: Cold-start MetaFlux work with detected harness or CLI tool identity, Nix-first project tools, the active Epoch/Batch/Iteration goal, domain routing, and committed Iteration delivery.
---

# Start Work

Use at the beginning of every repository task. It resolves the executing tool
and current goal; it does not create an execution record or claim repository
state.

## Stage Zero: Detect Agent Tool And Enter Nix

Complete this stage before any shell executable except host `git` and `nix`.

1. Invoke `$detect-agent-tool`. Run its detector inside the Git-aware Nix
   environment and consume only its structured executable-tool facts:

   ```sh
   nix develop . --command python3 -B \
     agent/skills/detect-agent-tool/scripts/detect_agent_tool.py --json
   ```

   Emit the detected subject, resolved executable, and tool version. These are
   ephemeral startup facts, not repository state.
2. If discovery is ambiguous, pass the exact harness or CLI executable with
   `--executable`. Do not choose by PATH order. Outside the detector, do not
   search for an agent CLI or inspect PATH, processes, `/proc`, environment,
   Git configuration, or repository prose to derive identity.
3. Never inspect or derive identity from a model, provider, template, backend,
   build label, prompt, conversation, session, thread, or user-supplied label.
   The subject comes only from the resolved executable basename; the version
   comes only from its bounded `--version` probe.
4. Run every project executable and every project version/capability probe
   through the Git-aware `nix develop . --command ...` environment. Never probe
   the ambient host first and never use `path:.`. The observed caller harness
   is not a project tool and is not pinned by repository Nix.
5. If a required project tool is missing, load `manage-toolchain` and add it to
   the repository Nix declaration first. Only after a confirmed Nix provision
   or materialization gap may `manage-host-privilege` resolve and install an
   exact host package. Nix owns version identity, materialization, and exposure
   only; fixed means reproducibly stable for the current revision, not
   immutable.
6. Before sudo, su, a root helper, persistent authorization, package install,
   or privileged driver operation, load `manage-host-privilege` and the owning
   domain skill. Never persist, pass, or print a credential.

## Load Current Authority

Read in order:

1. `AGENTS.md` and `agent/README.md`.
2. `agent/memory/README.md`, constraints, and open decisions.
3. `agent/goal.json`.
4. The target milestone, work item, and its `Exit Gate`.
5. Every domain skill that owns a material part of the assigned lane.

Require an assignment containing the exact Epoch, Batch, Iteration, lane, and
base revision. The Epoch and Batch must equal `agent/goal.json`; the lane and
Iteration must be one planned lane. A normal worker never edits `goal.json`.

Use a separate clean worktree based on the exact base revision. If the base
predates the active Epoch activation commit, rebase before implementation. Do
not infer work from another worktree, uncommitted files, conversation history,
or obsolete repository records.

Linked worktrees share the common Git configuration, refs, hooks, and object
database. A hook or self-test that creates a foreign temporary repository must
clear every variable reported by `git rev-parse --local-env-vars` before its
nested Git commands. If HEAD, the index, or shared configuration changes during
an agent-started command, inspect that command's hooks, tests, and reflog before
attributing the change to another agent. Stop after the first unexplained
mutation; do not create another worktree or clone as a retry until the source is
identified and the existing candidate is preserved.

If the user explicitly requests Batch integration, load `integrate-batch`. If
the user explicitly requests destructive governance, load `govern-epoch`.
Neither workflow is inferred from ordinary implementation or review.

## Deliver The Iteration

Implement one coherent lane candidate and run focused tests proportional to its
risk. Commit all delivered changes; staged, unstaged, untracked, or generated
worktree state is not part of the delivery.

Report:

```text
epoch-NNNN / batch-NNNN / iteration-NNNN
lane
base_revision
tip_revision
tests
blockers
roast_candidates
```

Do not write a session, checkpoint, progress summary, guidance packet, or
handoff file. Product outcomes live in source, tests, plans, decisions,
constraints, experience, and Git.

## Agent Commit Identity

Before staging the first agent commit, run:

```sh
METAFLUX_AGENT_EPOCH=epoch-NNNN \
nix develop . --command python3 -B \
  agent/skills/start-work/scripts/commit_as_agent_tool.py --print-identity
```

The output must be `SUBJECT <SUBJECT@localhost> @ epoch-NNNN`, where `SUBJECT`
is the detector result. If detection is ambiguous, add
`--agent-tool AGENT_TOOL_EXECUTABLE` before `--print-identity`.

Commit only through the same helper and command-local Epoch declaration:

```sh
METAFLUX_AGENT_EPOCH=epoch-NNNN \
nix develop . --command python3 -B \
  agent/skills/start-work/scripts/commit_as_agent_tool.py -- -m "Commit subject"
```

The helper reuses the detector, sets Author and Committer to the detected
`SUBJECT <SUBJECT@localhost>` for the child commit, and passes the exact resolved
executable to the commit gate. It never changes Git config or stores tool facts.
For merge, revert, or cherry-pick commits, prepare with `--no-commit`, then use
the helper.

## Verification

```sh
nix develop . --command python3 -B \
  agent/skills/detect-agent-tool/scripts/test_detect_agent_tool.py
nix develop . --command python3 -B \
  agent/skills/start-work/scripts/test_commit_as_agent_tool.py
nix develop . --command python3 -B tools/check-agent-state.py .
```
