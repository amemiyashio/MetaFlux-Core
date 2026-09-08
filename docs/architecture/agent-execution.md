---
status: Verified
decision: decision-0033
updated: 2026-09-08
---

# Goal-First Multi-Agent Execution

## Decision

MetaFlux coordinates repository work through Epoch, Batch, and Iteration. The
current tree stores only the active goal and canonical product knowledge. Agent
identity, activity, conversation detail, handoffs, and historical progress are
not repository authorities.

This decision supersedes the session/focus authority of decision-0029, the
semantic-change ledger of decision-0025, and the record-shaped roast contract
of decision-0026. Earlier detailed records have no current-tree compatibility
or lookup surface. Git history preserves their factual capture state.

## Rationale

The superseded model made repository bookkeeping an execution objective: stale
owners blocked unrelated work, repeated ledgers duplicated canonical facts, and
handoff mechanics could consume more effort than the product Exit Gate. The new
model keeps durable state proportional to the product decision being made and
leaves live scheduling to the application that already owns agents and threads.

## Consequences

- Workers cannot claim repository authority through activity metadata; a
  committed Iteration and its tests are the candidate unit.
- Parallel work is accepted by an automatically invoked integration stage against
  exact revisions, so a delivery cannot silently replace main state.
- Governance is intentionally destructive and atomic. A failed Epoch candidate
  remains unpublished and is repaired in place.
- Old execution detail has no current-tree lookup path. Valuable knowledge must
  live in its product, test, plan, decision, constraint, or experience owner.

## State Model

`agent/goal.json` is the only execution-state document.

- An Epoch is a repository-wide semantic regime and advances only through an
  explicitly requested destructive `govern-epoch` run.
- A Batch is a bounded set of lanes accepted together. The execution controller
  automatically invokes integration after a dependency-ready qualified delivery;
  only that stage may change Batch or lane state.
- An Iteration is one committed candidate for a lane or one committed repair
  candidate. It is identified by its full Epoch/Batch/Iteration triple and
  exact base/tip revisions.
- A reference prerequisite maps one catalog entry under `references/` to the
  lanes that require it for implementation research. It is readiness state,
  not progress, product capability, or release evidence (decision-0047).

Lane state is `planned`, `integrated`, or `deferred`. Batch state is `open` or
`integrated`. There is no persisted in-progress state: the application and
conversation own live scheduling.

## Worker Contract

A worker starts from an exact revision in a separate worktree, reads the active
goal and domain authority, and changes only its assigned lane. Product source
and test lookup, add, delete, and modify prefer a parent-written briefing, a
bounded coding subagent, and parent review of returned diffs against that
briefing and the product boundary. The parent does not start the next
coding-subagent dispatch or Iteration cycle until that review accepts the
dispatched briefing goal without drift. The parent owns verification commands,
the commit helper, and never lets a coding subagent edit `goal.json`,
integrate, govern, push, or create an execution context. Review stays in
conversation; there is no review archive. Its delivery is:

```text
epoch: epoch-NNNN
batch: batch-NNNN
iteration: iteration-NNNN
lane: lane-slug
base_revision: full Git revision
tip_revision: full Git revision
tests: exact commands and results
blockers: bounded unresolved conditions
roast_candidates: material claims only
```

The delivery must resolve to committed Git objects. Dirty, staged, untracked,
or inferred worktree state is not accepted.

## Automatic Batch Integration (decision-0051)

After a worker delivers exact committed revisions and its lane dependencies and
focused gates are ready, the execution controller automatically invokes
`integrate-batch`. No second user message is a state-transition prerequisite.
The integrator verifies that each base is at or after the activation commit of
the current Epoch, reviews `base..tip`, and orders candidates by `depends_on`.

When a candidate tip is the integration context's current `HEAD`, its linear
history is already present and the integrator creates an in-place acceptance
commit. A divergent candidate is retained through a non-fast-forward merge. A
candidate that is merely an older ancestor of `HEAD` is stale and is rejected
until it is rebased and revalidated. Bounded conflict and composition repairs
belong to the relevant merge; a change to the lane's intended product semantics
returns as a new Iteration.

Each lane receives focused verification. The combined tree then receives the
Batch regression. Only a passing combined tree may mark lanes or the Batch
integrated, and that state change is included in the final acceptance commit
rather than a standalone record commit. Failure leaves main and `goal.json`
unchanged. After the acceptance commit is on disk, the integrating parent
pushes that full object ID through `push-repository`.

## Task-Stop Contract

A task-stopping gate or Skill prohibition reports bounded evidence, the
authority responsible for the next action, and the exact condition for
resuming. A child gate's structured cause remains visible through callers;
required product verification keeps its original command output and receives
one actionable wrapper per distinct root cause. External responsibility never
authorizes an Agent to create another execution context, widen privilege, or
retry unchanged evidence.

[`start-work`](../../agent/skills/start-work/SKILL.md#task-stop-diagnostics) is
the operational authority and `tools/agent_diagnostics.py` is its shared
renderer. Diagnostics are command/conversation output only and never become
Goal state, Agent identity, a numbered record, or an archive.

## Route Replanning (decision-0045)

`replan-roadmap` is an explicit-only, two-stage workflow. Stage 1 reads the
current repository and proposes one evidence-backed dependency DAG without
tracked writes. It reports the baseline revision and Epoch, primary objective
and observable success, four-axis readiness, critical path, lane order, node
dispositions, decision timing, affected owners, residual scans, and regression
plan. With no supplied objective it offers at most three candidates and waits
for selection.

Stage 2 starts only after explicit confirmation of that exact proposal. It
rechecks `HEAD`, worktree cleanliness, active Epoch, and proposal baseline; any
change invalidates the proposal and returns to Stage 1. A semantic no-op does
not advance the Epoch. A semantic route change composes `roast` and
`govern-epoch`; the latter remains the sole destructive authority writer and
Epoch publisher.

The route is a DAG from user objective through milestones, work items, open
decisions, active reference inputs, and evidence prerequisites to Iteration
lanes. Completed milestones and work items remain closed. Active and Queued
nodes may be reordered in a new Epoch. Existing IDs remain when delivery
coordinates and observable outputs are unchanged. Planning does not close
evidence-bound technical decisions, modify product source, dispatch workers, or
create execution contexts.
`agent/goal.json` remains the only active route state; proposals and prior
routes are not stored as ledgers, databases, or compatibility views.

decision-0045 adopts this protocol because objective changes otherwise mix
read-only diagnosis with destructive governance and encourage duplicate route
authority. Verification is the proposal-guard self-test, explicit bilingual
routing, Agent-state DAG validation, residual scans, and the full governance
regression.

## Epoch Governance

`govern-epoch` is explicit-only. It applies when duplicate authority, ambiguous
terminology, incompatible rules, or process work displaces product progress.
The governance run promotes valuable knowledge, rewrites every affected current
authority, removes obsolete surfaces, and stages the next monotonic Epoch.

The new Epoch is not published until the state checker, skill gates, architecture
graph, relevant domain tests, and full regression pass. A failed candidate is
fixed in the same governance work unit under the old published Epoch. After the
activation commit is on disk, the governing parent pushes that full object ID
through `push-repository`. After publication, older-base work must rerun
`start-work`, rebase onto the new Epoch, and repeat verification before
integration. Existing branches are not deleted.

## Knowledge Promotion

`roast` is an ephemeral disposition step. Light, medium, and dark describe the
semantic transformation required to update one canonical owner; they are not
durable record statuses. Source, tests, contracts, plans, decisions,
constraints, and validated experience are valid owners. Duplicate, routine,
local-only, and failed-route material is discarded. No roast archive is kept.

## Commit Boundary

Decision-0034 supersedes the fixed identity portion of this decision. The
[`agent-tool detection boundary`](agent-tool-detection.md) derives Author and
Committer from the harness name already emitted in the current conversation.
The active Epoch is represented
once in `agent/goal.json` and is not part of commit identity or command
environment. The Epoch/Batch/Iteration topology and candidate-tree commit gate
defined here remain authoritative.
