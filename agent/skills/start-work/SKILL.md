---
name: start-work
description: Cold-start one externally assigned MetaFlux Iteration with detected tool identity, a provisioned Git execution context, Nix-first project tools, bounded in-task subagents, and one committed delivery without allocating sibling lanes or source copies.
---

# Start Work

Use at the beginning of every repository task. It resolves the executing tool
and current goal; it does not create an execution record or claim repository
state, schedule another lane, or create an execution context.

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
2. Validate the current checkout before reading or running project tools:

   ```sh
   nix develop . --command python3 -B \
     agent/skills/start-work/scripts/check_execution_context.py --json
   ```

   The shared Git config key `metaflux.agentExecutionCommonDir` must contain
   the exact absolute `git-common-dir` provisioned by the user or application.
   The primary checkout and its registered linked worktrees share that value;
   a standalone clone or copied `.git` does not. An Agent never creates,
   changes, copies, or repairs this registration. A missing or mismatched value
   ends the work unit without a clone, worktree, directory, or config retry.
3. If discovery is ambiguous, pass the exact harness or CLI executable with
   `--executable`. Do not choose by PATH order. Outside the detector, do not
   search for an agent CLI or inspect PATH, processes, `/proc`, environment,
   Git configuration, or repository prose to derive identity.
4. Never inspect or derive identity from a model, provider, template, backend,
   build label, prompt, conversation, session, thread, or user-supplied label.
   The subject comes only from the resolved executable basename; the version
   comes only from its bounded `--version` probe.
5. Run every project executable and every project version/capability probe
   through the Git-aware `nix develop . --command ...` environment. Never probe
   the ambient host first and never use `path:.`. The observed caller harness
   is not a project tool and is not pinned by repository Nix.
6. If a required project tool is missing, load `manage-toolchain` and add it to
   the repository Nix declaration first. Only after a confirmed Nix provision
   or materialization gap may `manage-host-privilege` resolve and install an
   exact host package. Nix owns version identity, materialization, and exposure
   only; fixed means reproducibly stable for the current revision, not
   immutable.
7. Before sudo, su, a root helper, persistent authorization, package install,
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

Require the already supplied execution context to be a clean worktree based on
the exact base revision. If it is missing, dirty outside the assignment, or
based before the active Epoch activation commit, preserve it and report the
exact mismatch. Do not create, switch, move, replace, or delete a branch,
worktree, clone, task, thread, or chat to manufacture a compliant context. Do
not infer work from another worktree, uncommitted files, conversation history,
or obsolete repository records.

Re-run Stage Zero after a resumed or compacted task, after loading changed
`start-work` authority, and before further durable work when `HEAD`, the index,
or the execution path changed outside the current work unit. A context created
earlier by an Agent is not grandfathered: if its current registration fails,
stop there. Never fall back from a worktree, hook, index, or candidate-tree
failure to `git clone`, file-tree copying, or a sibling source directory.

## Assignment And Subagent Boundary

The user or application owns scheduling and supplies the task, execution
context, base revision, and assigned lane. `planned` sibling lanes describe the
Batch integration topology; they are not a queue for the current worker to
claim or dispatch. Existing parallel agents and worktrees are accepted as
external facts, not authorization to create more. After delivering the assigned
Iteration, report it and stop instead of selecting the next lane.

A standalone clone is never an Iteration execution context, even when it is
clean, local-only, based on the requested revision, or already contains a useful
candidate. Preserve unexpected content long enough to identify its exact Git
relationship, then let the user or application decide cleanup; do not continue
work there or create another copy.

Within the assigned Iteration, prefer bounded subagents over additional Git
branches when independent analysis materially improves speed or review quality.
Use them for read-only investigation, interface tracing, code review, test
selection, and failure triage. The parent Agent remains the sole durable writer,
test owner, and commit owner; subagents return findings or patch suggestions and
do not edit `goal.json`, claim sibling lanes, integrate, govern, or create
branches, worktrees, clones, tasks, threads, or chats.

If the available subagent mechanism inherently creates an independent task,
thread, branch, or worktree, treat it as external fan-out and require explicit
user or application authorization before invoking it. Concurrent durable edits
require execution contexts supplied by that external scheduler; otherwise the
parent applies changes sequentially in its assigned context.

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
  agent/skills/start-work/scripts/check_execution_context.py --json
nix develop . --command python3 -B \
  agent/skills/detect-agent-tool/scripts/test_detect_agent_tool.py
nix develop . --command python3 -B \
  agent/skills/start-work/scripts/test_commit_as_agent_tool.py
nix develop . --command python3 -B tools/check-agent-state.py .
```
