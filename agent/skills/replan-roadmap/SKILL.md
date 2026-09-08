---
name: replan-roadmap
description: Explicitly replan the active MetaFlux product roadmap as an evidence-backed dependency DAG, then apply a confirmed semantic cutover through govern-epoch. Use only when the user invokes `$replan-roadmap` to replace the primary objective or reorder milestones, work items, decisions, and Iteration lanes; do not use for roadmap summaries, ordinary work-item edits, or implementation.
---

# Replan Roadmap

Use only when the user explicitly invokes `$replan-roadmap`. Treat ordinary
requests to view, explain, review, or summarize the roadmap as read-only project
inspection without this Skill.

Model the active route as a dependency DAG:

```text
user objective
  -> milestones
  -> work items
  -> open decisions and evidence prerequisites
  -> serial or parallel Iteration lanes
```

`agent/goal.json` remains the only active route state. Do not add a roadmap
database, proposal archive, progress diary, history ledger, or compatibility
view. Git is the old-state recovery mechanism.

## Input

The text after `$replan-roadmap` is the user-priority product objective. Prefer
that objective over the existing route while retaining verified product facts
and completed delivery evidence.

When no objective is supplied, perform a read-only audit, present at most three
distinct candidate objectives with observable success signals, and stop for a
selection. Do not write tracked files or infer a winner.

## Stage 1: Read-Only Proposal

1. Run `start-work` Stage Zero, resolve the full `HEAD`, require a clean supplied
   governance context, and read `agent/README.md`, durable memory,
   `agent/goal.json`, affected milestone/work-item Exit Gates, and matching
   domain Skills.
2. Use source and executable tests as implementation truth, then verified
   contracts and architecture, then plans and durable memory. Separate current
   implementation from intended architecture and release claims.
3. Compose `implementation-readiness` and classify architecture, activation,
   implementation maturity, and release evidence independently. A written
   design is not implementation evidence and a fake-client test is not a real
   client qualification gate.
4. Build a DAG whose nodes are milestones, work items, open decisions, evidence
   prerequisites, and bounded Iteration lanes. Use `depends_on`, `blocks`, and
   `evidenced_by` edges. Reject unresolved dependencies, duplicate node IDs,
   cycles, and a lane whose Exit Gate can pass without its prerequisites.
5. Classify each affected node as `keep`, `reorder`, `rewrite`, or `delete`.
   Completed milestones and work items remain closed. Active and Queued nodes
   may move in the candidate Epoch. Preserve a milestone or work-item ID when
   its delivery coordinate and observable output are unchanged.
6. Classify drift as objective drift, dependency/order drift, ownership or
   duplicate-authority drift, evidence/maturity drift, status drift, or
   scope/terminology drift. Promote facts to one canonical owner; do not turn
   transient execution detail into durable state.

The proposal output must contain:

- full baseline revision, active Epoch, and current objective;
- exactly one proposed primary objective and its observable success signal;
- an architecture/activation/implementation-maturity/release-evidence matrix;
- the proposed DAG, critical path, lane order, and every node disposition;
- decisions that must close before the first lane and decisions that may wait;
- exact affected files, residual searches, and regression commands; and
- whether the audit found a semantic change or a no-op.

The optional read-only guard validates the mechanical proposal invariants:

```sh
nix develop . --command python3 -B \
  agent/skills/replan-roadmap/scripts/check_route_proposal.py \
  PROPOSAL_JSON --root .
```

Keep proposal JSON outside the repository worktree. It is conversation or
temporary input, not project state.

## Confirmation Boundary

Stage 2 requires explicit user confirmation of the exact Stage 1 proposal. A
direct implementation request that already contains the complete proposal may
serve as that confirmation. General approval to plan does not authorize a later
or changed proposal.

Immediately before mutation, confirm that `HEAD`, the clean worktree, active
Epoch, and proposal baseline are unchanged. A mismatch invalidates the proposal;
discard it and rerun Stage 1. Use the guard with `--confirm` to check this
boundary. If the current DAG already satisfies the objective, report `no-op`,
do not invoke destructive governance, and do not advance the Epoch.

## Stage 2: Confirmed Cutover

1. Invoke `roast` over every affected claim. Promote valid product facts to one
   source, test, contract, architecture, plan, decision, constraint, or
   validated-experience owner. Discard duplicate and process-only material.
2. Do not close an unresolved technical decision merely because the route needs
   it. Put it before the dependent lane with an exact closure condition and
   retain it in the open-decision ledger until evidence exists that condition.
3. When objective, dependency, ownership, or milestone semantics change, invoke
   `govern-epoch`. It remains the sole destructive writer and Epoch publisher.
   Rewrite all affected current authority, advance the Epoch monotonically,
   reset Batch/Iteration state, and leave no old alias or dual route.
4. Do not modify product source, dispatch a worker, create an execution context,
   or implement a lane during replanning. Replanning only changes route,
   governance, tests for the workflow itself, and current canonical authority.
5. Run proposal self-tests, skill validation and routing, Agent state and its
   self-test, exact residual scans, the component graph, affected domain gates,
   a complete dev build, full CTest, and `git diff --check`.
6. Commit one atomic activation through the `start-work` commit helper. Resolve
   the resulting full object ID and publish exactly it through
   `push-repository` as required by `govern-epoch`.

## Task Stops

Use the `start-work` task-stop diagnostic contract. Missing target selection or
confirmation is owned by `user-or-application / preserve-and-report`. A changed
baseline is `roadmap-replan.proposal-invalidated`; rerun the read-only audit.
An invalid or cyclic graph and failed regression are
`verification.required-gate-failed` owned by `epoch-governor / fix-and-retry`.
Do not retain a failed proposal as repository state.

## Output

Report the old/new objective, old/new Epoch, final DAG and critical path, node
dispositions, decision timing, canonical roast promotions, discarded duplicate
count, affected owners, regression results, residual result, activation commit,
and push result. For a no-op, report the evidence and unchanged Epoch.
