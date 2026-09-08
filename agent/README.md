# MetaFlux Agent Context

MetaFlux uses a goal-first, training-shaped execution loop. Repository state
describes product intent and accepted integration only; it does not track agent
identity, conversations, worktrees, or activity.

## Read Order

1. Follow [`start-work`](skills/start-work/SKILL.md) Stage Zero and consume
   [`detect-agent-tool`](skills/detect-agent-tool/SKILL.md) conversation-emitted
   harness-name facts.
2. Read [`memory/README.md`](memory/README.md), especially constraints and open
   decisions.
3. Read [`goal.json`](goal.json).
4. Read the target milestone, work item, and its `Exit Gate`.
5. Load every domain skill that owns a material part of the assigned lane.

Current source and passing tests outrank verified architecture, approved plans,
memory, and validated experience in that order. Proposed architecture and
queued plans describe intent, not implemented behavior.

## Execution Scale

- **Epoch** (`epoch-0011`): one repository-wide semantic governance regime.
  Only explicit `govern-epoch` may advance it, after a full current-authority
  rewrite and passing regression.
- **Batch** (`batch-0001`): one bounded collection of parallel product lanes
  integrated and tested together. Batch numbering restarts in a new Epoch.
- **Iteration** (`iteration-0001`): one lane candidate or one integration repair
  that produces a new, independently testable Git state. Iteration numbering
  restarts in a new Batch.

The full identity is always written as
`epoch-NNNN / batch-NNNN / iteration-NNNN` (currently
`epoch-0011 / batch-0001 / iteration-0001`). A worker delivers exact base and tip
revisions. Product source and test mutation prefers a parent briefing, a bounded
coding subagent, and parent review against drift. Only an explicitly created
integration agent changes `goal.json`.
Failed candidates and superseded Batch state are not archived in the current
tree; Git and the originating conversation retain their evidence.

## Repository Roles

| Path | Authority |
| --- | --- |
| `goal.json` | Current Epoch, Batch, target, lanes, dependencies, and acceptance |
| `plan/` | Product milestones, work items, dependencies, and Exit Gates |
| `memory/` | Compact stable constraints, decisions, ownership, and terminology |
| `experience/` | Reusable engineering methods with reproducible evidence |
| `skills/` | Task-specific operating instructions |

There are no session, focus, checkpoint, guidance, semantic-change, or roast
record directories. Git history is the sole prior-state recovery mechanism.

## Stable Identifiers

Repository-owned record identifiers use complete lowercase words:

- milestone: `milestone-MAJOR.MINOR.PATCH.0`
- work item: `work-item-MAJOR.MINOR.PATCH.WORK`
- decision: `decision-NNNN`
- experience: `experience-NNNN`
- Epoch, Batch, Iteration: `epoch-NNNN`, `batch-NNNN`, `iteration-NNNN`
- lane: a descriptive lowercase slug beginning with `lane-`

Product SemVer, ABI/UAPI/schema versions, compiler epochs, external driver
families such as R535, and tool diagnostics such as E402 are independent
technical namespaces.

## Delivery Loop

1. Establish a Batch objective and preallocate one Iteration per parallel lane.
2. Run each Iteration in a separate worktree from an exact base revision.
3. Brief coding subagents from loaded authority, review their product-source
   diffs in conversation against the briefing goal, and only then start the
   next dispatch or deliver committed base/tip revisions, tests, blockers, and
   roast candidates.
4. In a separate user-requested integration run, invoke `integrate-batch` to
   review, merge, repair bounded integration gaps, and run combined regression.
5. Update lane and Batch status only after acceptance passes. Open the next
   Batch in the same integration commit when the next bounded cohort is known.
6. If governance drift is detected, leave the Batch state unchanged and invoke
   `govern-epoch` explicitly. Publish a new Epoch only after full regression.

At integration and governance boundaries, `roast` promotes each valuable claim
to one canonical owner. Routine commands, duplicate prose, failed routes, local
host facts, and conversation detail are discarded.
