---
status: Verified
decision: decision-0033
updated: 2026-09-04
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
- Parallel work is accepted only by a separate integration agent against exact
  revisions, so a delivery cannot silently replace main state.
- Governance is intentionally destructive and atomic. A failed Epoch candidate
  remains unpublished and is repaired in place.
- Old execution detail has no current-tree lookup path. Valuable knowledge must
  live in its product, test, plan, decision, constraint, or experience owner.

## State Model

`agent/goal.json` is the only execution-state document.

- An Epoch is a repository-wide semantic regime and advances only through an
  explicitly requested destructive `govern-epoch` run.
- A Batch is a bounded set of lanes accepted together. Only an explicit
  integration run may change Batch or lane state.
- An Iteration is one committed candidate for a lane or one committed repair
  candidate. It is identified by its full Epoch/Batch/Iteration triple and
  exact base/tip revisions.

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

## Integration Contract

The user explicitly creates an integration agent and supplies exact deliveries.
The integrator verifies that each base is at or after the activation commit of
the current Epoch, reviews `base..tip`, and orders lanes by `depends_on`.
Accepted lane history is retained through non-fast-forward merges. Bounded
conflict and composition repairs belong to the relevant merge; a change to the
lane's intended product semantics returns as a new Iteration.

Each lane receives focused verification. The combined tree then receives the
Batch regression. Only a passing combined tree may mark lanes or the Batch
integrated, and that state change is included in the final product merge rather
than a standalone record commit. Failure leaves main and `goal.json` unchanged.
After the integration commit is on disk, the integrating parent pushes that
full object ID through `push-repository`.

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
Committer from bounded executable evidence. The active Epoch is represented
once in `agent/goal.json` and is not part of commit identity or command
environment. The Epoch/Batch/Iteration topology and candidate-tree commit gate
defined here remain authoritative.
