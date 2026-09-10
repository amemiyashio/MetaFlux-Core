---
name: main
description: Control MetaFlux repository work through evidence-bound stages, dispatch epoch/batch/iteration or specialist skills, and recover exact delivery/publication state. Use as the repository entrypoint, including read-only status, readiness, identity, maintenance, and publication diagnostics.
---

# Main

The controlling Agent owns task interpretation and calls the next skill. The
controller scripts validate transitions; they do not run an Agent, allocate an
execution context, or confer user authorization. Read the current request and
retain its limits, including a request to keep work local.

## Bootstrap

Before repository executables other than host Git/Nix, enter the Git-aware Nix
environment. Shell grammar runs inside its bash. Consume only the harness name
already emitted in the conversation; never infer identity from processes,
PATH, a model, or repository prose.

```sh
nix develop . --command python3 -B agent/skills/main/scripts/detect_agent_tool.py --agent-tool HARNESS_NAME --json
nix develop . --command python3 -B agent/skills/main/scripts/check_git_topology.py --json
```

Read `agent/README.md`, memory, `agent/goal.json`, the relevant product Exit Gate,
and the domain skills owning the requested change. Nix supplies tools; CMake,
Ninja, Kbuild, CTest, and packaging keep their existing ownership. Provision
missing tools through `manage-toolchain`; confirmed host gaps and every
privileged action additionally compose `manage-host-privilege`.

## Dispatch

- Inspection, status, readiness, and identity requests stay read-only. Use
  [readiness](references/evidence-framework.md) for evidence classification.
  A question during active work does not replace its state or trigger a commit.
- Maintenance uses the main preparation, implementation, parent review,
  evaluation, guarded commit, and publication stages. It has no invented
  product Iteration identity and leaves Goal state unchanged.
- An assigned product candidate uses `iteration`; its exact committed delivery
  goes automatically to `batch` in the same controlling turn.
- An explicit route or governance request uses `epoch`. Discovering drift in
  ordinary work is evidence to report, not a new governance authorization.
- Standalone knowledge promotion uses [knowledge promotion](references/knowledge-promotion.md)
  within the requested scope. Identity and publication diagnostics use the
  shared helpers without manufacturing a delivery.

Read [the executable interface](references/controller.md) before mutating
controller state. `$main status` maps to `inspect`; `$main resume` revalidates
Git and evidence before selecting another operation. The script exposes
`inspect`, `begin`, `step`, and `resume`.

The controller reports stage, evidence, next operation, and delivery target.
Preparation, implementation, review, evaluation, delivery, publication, and
handoff are stages, not new numbered execution levels. A required check that
fails returns to bounded repair. A declared optional environment skip is not
evidence for a product requirement that needed that environment.

## Shared Delivery Boundary

The parent reviews returned diffs against its briefing before verification or
another coding dispatch. Coding subagents never change Goal, accept, govern,
commit, publish, or create execution contexts. The application supplies worker
and integration contexts; do not manufacture a branch, worktree, clone, task,
thread, or chat to satisfy a gate.

Commit only through `scripts/commit_as_agent_tool.py`, with the conversation
subject and exact expected HEAD/tree. The helper checks the active verification
receipt and transaction before invoking the candidate-tree commit gate. It
never modifies Git identity configuration.

Maintenance, Batch acceptance, and Epoch activation publish their exact guarded
commit automatically unless the current user request limits publication. An
ordinary worker candidate goes to its controlling Batch instead of pushing.
Use [publication](references/publication.md); do not force, broaden the refspec,
or read private-key bytes. A publication failure preserves that commit and
resumes publication only. Next work starts only after exact publication and a
matching application-supplied context.

## State And Recovery

`agent/goal.json` owns product route and accepted progress. Ignored
`agent/tmp/main/` owns only the current worktree's running state, verification
receipts, and pending transaction. Build and product-test artifacts stay under
root `tmp/`. No temporary state is authorization or product completion proof.

Use the common Git lock and expected HEAD/tree checks for shared mutations.
Recheck the received scope and Git facts after interruption. If pre-commit
state is lost, review and evaluate again; do not guess an accepted transition.
If a validated acceptance commit exists, recover it from Git before attempting
another acceptance. Preserve external edits detected during recovery.

## Task-Stop Diagnostics

The shared `tools/agent_diagnostics.py` contract is the sole diagnostic shape:
`code`, `source`, `summary`, `evidence[]`, `responsibility`, `disposition`,
`required_action`, `resume_when`, and an optional exact `retry_command`.
Responsibility is `current-agent`, `user-or-application`, `batch-integrator`,
`epoch-governor`, or `host-operator`. Disposition is `fix-and-retry`,
`stop-and-report`, or `preserve-and-report`.

Preserve child diagnostics and raw product output. A current-agent repair
retries only after evidence or prerequisites change. External responsibility
does not authorize scheduling, privilege, cleanup, or an unchanged retry.
Diagnostics are output and current-operation recovery data, never a ledger.
