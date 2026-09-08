---
name: start-work
description: Cold-start one externally assigned MetaFlux Iteration with the conversation-emitted harness name, existing Git topology, Nix-first project tools, parent briefing, bounded coding subagents for product source CRUD, parent review against drift, and one committed delivery without allocating sibling lanes or source copies.
---

# Start Work

Use at the beginning of every repository task. It resolves the executing tool
and current goal; it does not create an execution record or claim repository
state, schedule another lane, or create an execution context.

## Stage Zero: Detect Agent Tool And Enter Nix

Complete this stage before any shell executable except host `git` and `nix`.

1. Invoke `$detect-agent-tool`. Pass the harness name already emitted in this
   conversation through the Git-aware Nix environment and consume only its
   normalized subject:

   ```sh
   nix develop . --command python3 -B \
     agent/skills/detect-agent-tool/scripts/detect_agent_tool.py \
     --agent-tool HARNESS_NAME --json
   ```

   Emit the declared subject. These are ephemeral startup facts, not
   repository state.
2. Validate the current checkout from Git's existing topology before reading or
   running project tools:

   ```sh
   nix develop . --command python3 -B \
     agent/skills/start-work/scripts/check_git_topology.py --json
   ```

   Accept the current primary checkout or an existing registered linked
   worktree. Reject an independent repository whose remote or creation reflog
   shows a local Git source. Do not add, remove, or rewrite remotes or reflogs to
   alter this result. This check reads Git facts only and creates no repository,
   worktree, Agent, or execution identity.
3. If no harness name was declared, pass the name already shown in this
   conversation with `--agent-tool`. Do not choose by PATH order and do not
   inspect PATH, processes, `/proc`, executables, Git configuration, or
   repository prose to derive identity.
4. Never inspect or derive identity from a model, provider, template, backend,
   build label, prompt, session, thread, or a user-supplied label that was not
   the harness name already emitted in this conversation. The subject comes
   only from that declared name.
5. Run every project executable and every project version/capability probe
   through a Nix shell, the first-choice execution environment:
   `nix develop . --command ...` for repository workflows, or the preferred
   single-tool form `nix shell .#<tool-output> --command TOOL ...` when only
   one named tool output is required. Shell grammar for repository work runs
   under the Nix-provided bash (`nix develop . --command bash -c '...'`);
   the ambient host shell never executes repository tools. Never probe the
   ambient host first and never use `path:.`. The observed caller harness
   is not a project tool and is not pinned by repository Nix.
   In-repository workspace scratch belongs only under `tmp/` at the repository
   root (decision-0042): CMake trees in `tmp/build/<preset>`, dumps in
   `tmp/outputs/`, retained work in `tmp/work/`. Do not write `build/`,
   `.cache/`, `outputs/`, `../.metaflux-build`, or `../.metaflux-evidence`.
   Installed compiler and AOT caches stay at `/var/cache/metaflux/compiler`
   and `/var/lib/metaflux/aot`.
6. If a required project tool is missing, load `manage-toolchain` and add it to
   the repository Nix declaration first. Only after a confirmed Nix provision
   or materialization gap may `manage-host-privilege` resolve and install an
   exact host package. Nix owns version identity, materialization, and exposure
   only; fixed means reproducibly stable for the current revision, not
   immutable.
7. Before sudo, su, a root helper, persistent authorization, package install,
   or privileged driver operation, load `manage-host-privilege` and the owning
   domain skill. Never persist, pass, or print a credential.

## Task-Stop Diagnostics

This section is the sole Agent-facing contract for a condition that blocks the
next task phase. Apply it when a repository gate fails, a loaded Skill's hard
prohibition is reached, or a required verification cannot advance. Do not turn
routine compiler output into a new diagnostic stream: preserve the raw output
and add one task-stop diagnostic per distinct root cause.

Every diagnostic contains:

```text
code
source
summary
evidence[]
responsibility
disposition
required_action
resume_when
retry_command (only when an exact retry is valid)
```

Use a readable dotted code such as `git-topology.local-clone`; it is a
diagnostic classification, not an identity, record, or sequence. Responsibility
is exactly `current-agent`, `user-or-application`, `batch-integrator`,
`epoch-governor`, or `host-operator`. Disposition is exactly
`fix-and-retry`, `stop-and-report`, or `preserve-and-report`.

Default CLI rendering is one human-readable `ERROR [code]` block on stderr.
Governed CLIs accept `--diagnostic-format human|json`; JSON failures use the
same fields in one versioned error envelope while successful interfaces and
exit codes remain unchanged. If a child gate already emitted this contract,
propagate its code, evidence, responsibility, action, and resume condition
instead of replacing it with a generic rejection.

For `current-agent / fix-and-retry`, change only the assigned candidate and
retry only after evidence or a prerequisite changed. For any external
responsibility, preserve relevant state, perform no implicit scheduling,
privilege expansion, branch/worktree/clone/task/thread creation, or cleanup,
and stop after reporting the exact requested action. An unchanged repeated
diagnostic is not a retry signal: fix its cause or escalate to its declared
responsibility.

When a mandatory build or test fails, retain its command and raw output, then
emit `verification.required-gate-failed` with the exact command, return code,
bounded actionable evidence, the authority allowed to repair it, and the
condition that the same command passes. Diagnostics remain conversation and
command output only; never persist an error ledger, counter, or Goal field.

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

## Reference Prerequisites

After validating the supplied clean execution context, inspect the
`references` array in `agent/goal.json`. For each entry whose `required_by`
contains the assigned lane, materialize and verify that exact catalog entry:

```sh
nix develop . --command python3 -B \
  references/tools/reference.py materialize ENTRY
nix develop . --command python3 -B \
  references/tools/reference.py verify ENTRY
```

Do not materialize entries for other lanes. Materialization populates the
declared submodule worktree under `references/sources/`; it does not create a
clone, branch, worktree, execution context, or build input. Require the exact
manifest revision and URL, a detached HEAD, and a clean reference worktree.
Treat it as read-only and promote every relied-upon product conclusion to its
canonical Core source, test, contract, decision, constraint, or plan.

An undeclared entry, URL or revision mismatch, dirty checkout, or failed
materialization is `reference.prerequisite-invalid` with
`current-agent / fix-and-retry`. Preserve the raw tool output and resume only
after the same `materialize` and `verify` commands pass. A lane with no declared
reference prerequisite performs no reference operation.

## Assignment And Subagent Boundary

The execution controller owns scheduling, worktree, and lane assignment.
`planned` sibling lanes describe the Batch integration topology; they are not a
queue for the current worker to claim or dispatch. Existing parallel agents and
worktrees are accepted as external facts, not authorization to create more. After
delivering the assigned Iteration, report it and stop instead of selecting the
next lane.

A standalone local clone is never an Iteration execution context, even when it
is clean, based on the requested revision, or already contains a useful
candidate. Preserve unexpected content long enough to identify its exact Git
relationship, then let the user or application decide cleanup; do not continue
work there, remove its provenance, or create another copy.

Within the assigned Iteration, prefer bounded coding subagents over additional
Git branches when independent analysis or source mutation materially improves
speed or review quality. Coding subagents perform product source and test lookup,
add, delete, and modify operations. Do not use extra Git branches for subagent
work.

Before dispatching any coding subagent, the parent MUST write a self-contained
briefing drawn exclusively from already-loaded authority. The briefing must
include: epoch/batch/iteration identity, lane, base revision, allowed paths,
forbidden paths, Exit Gate or acceptance criterion, domain-skill constraints that
apply, the exact operation requested, completion criteria, and drift surfaces
that must not change. The parent must not ask the subagent to invent missing
identity, lane, or goal.json edits.

Coding subagents MAY edit product source and tests inside allowed paths. They
MUST NOT edit `agent/goal.json`, claim sibling lanes, integrate, govern, push,
or create branches, worktrees, clones, tasks, threads, or chats. Read-only
subagents remain allowed for inventory, residual search, and failure triage;
they are not the default for coding work.

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

After a dependency-ready committed Iteration delivery, the execution controller
loads `integrate-batch` automatically. If the user explicitly requests
destructive governance, load `govern-epoch`. Ordinary Iteration delivery does
not load `push-repository`. After an Epoch activation or automatic
Batch-integration commit is on disk, that parent loads `push-repository` with
that commit's full object ID.
Any other external Git push still requires an explicit user or application
request and one full committed object ID.

## Deliver The Iteration

After coding subagents return, the parent reviews diffs and conclusions in
conversation against the briefing and product boundaries. The review must
confirm that the dispatched briefing goal is met and that the result has not
drifted outside allowed paths, forbidden paths, or named drift surfaces.
Reject or re-brief on drift or an unmet briefing goal. Do not start the next
coding-subagent dispatch, the next lane slice, or another Iteration cycle until
that review accepts the current dispatch. Do not write a session, roast,
checkpoint, progress, or review archive. Parent remains sole owner of
verification commands and the start-work commit helper. Parent remains sole
owner of `goal.json`; workers never edit it. Only after review may the parent
run tests and commit via the commit helper. The execution controller then
automatically runs `integrate-batch` before scheduling another lane. Do not
push an ordinary Iteration commit.

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
nix develop . --command python3 -B \
  agent/skills/start-work/scripts/commit_as_agent_tool.py --print-identity
```

The output must be `SUBJECT <SUBJECT@localhost>`, where `SUBJECT` is the
declared harness name. If no name is declared, add
`--agent-tool HARNESS_NAME` before `--print-identity`.

Epoch is repository goal state, not Git identity or command environment. Read
its single active value from `agent/goal.json`; never duplicate it in Author,
Committer, an environment declaration, Git config, or another identity record.

Commit only through the same helper:

```sh
nix develop . --command python3 -B \
  agent/skills/start-work/scripts/commit_as_agent_tool.py -- -m "Commit subject"
```

The helper reuses the detector, sets Author and Committer to the declared
`SUBJECT <SUBJECT@localhost>` for the child commit, and passes that name to
the commit gate. It never changes Git config, stores tool facts, or executes
the harness.
For merge, revert, or cherry-pick commits, prepare with `--no-commit`, then use
the helper.

## Verification

```sh
nix develop . --command python3 -B \
  agent/skills/start-work/scripts/check_git_topology.py --json
nix develop . --command python3 -B \
  agent/skills/detect-agent-tool/scripts/test_detect_agent_tool.py
nix develop . --command python3 -B \
  agent/skills/start-work/scripts/test_commit_as_agent_tool.py
nix develop . --command python3 -B tools/test-agent-diagnostics.py
nix develop . --command python3 -B references/tools/test_reference.py
nix develop . --command python3 -B references/tools/reference.py verify
nix develop . --command python3 -B tools/check-agent-state.py .
```
