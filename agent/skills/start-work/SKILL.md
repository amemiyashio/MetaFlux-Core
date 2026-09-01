---
name: start-work
description: Cold-start MetaFlux work with exact Codex identity, Nix-first tools, the active Epoch/Batch/Iteration goal, domain routing, and committed Iteration delivery.
---

# Start Work

Use at the beginning of every repository task. It resolves the runtime and
current goal; it does not create an execution record or claim repository state.

## Stage Zero: Resolve Runtime And Enter Nix

Complete this stage before any shell executable except host `git` and `nix`.

1. Read the stable harness product slug from active system/developer runtime
   instructions. For Codex it is exactly `codex`. Emit:

   ```text
   Agent harness subject: codex
   ```

2. Do not search for an agent CLI and do not inspect PATH, environment,
   processes, `/proc`, Git config, repository text, model names, templates,
   backends, builds, prompts, threads, or prior labels to infer the harness.
3. Run every other executable and every version/capability probe through the
   Git-aware `nix develop . --command ...` environment. Never probe the ambient
   host first and never use `path:.`.
4. If a required tool is missing, load `manage-toolchain` and add it to the
   repository Nix declaration first. Only after a confirmed Nix provision or
   materialization gap may `manage-host-privilege` resolve and install an exact
   host package. Nix owns version identity, materialization, and exposure only;
   fixed means reproducibly stable for the current revision, not immutable.
5. Before sudo, su, a root helper, persistent authorization, package install,
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
METAFLUX_AGENT_HARNESS=codex \
METAFLUX_AGENT_EPOCH=epoch-NNNN \
nix develop . --command python3 \
  agent/skills/start-work/scripts/commit_as_harness.py --print-identity
```

The complete output must be `codex <codex@localhost> @ epoch-NNNN`. Commit only
through the same helper and command-local declarations:

```sh
METAFLUX_AGENT_HARNESS=codex \
METAFLUX_AGENT_EPOCH=epoch-NNNN \
nix develop . --command python3 \
  agent/skills/start-work/scripts/commit_as_harness.py -- -m "Commit subject"
```

The helper sets Author and Committer to `codex <codex@localhost>` for the child
commit only. It never changes Git config or derives identity from runtime state.
For merge, revert, or cherry-pick commits, prepare with `--no-commit`, then use
the helper.

## Verification

```sh
nix develop . --command python3 \
  agent/skills/start-work/scripts/test_commit_as_harness.py
nix develop . --command python3 tools/check-agent-state.py .
```
