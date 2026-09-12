# MetaFlux Agent Context

All work enters through [$main](skills/main/SKILL.md) skill, which preserves the user
scope, bootstraps Nix and conversation-derived identity, and selects the next
operation. Read-only questions preserve an active task. Maintenance has no
invented product Iteration and leaves Goal unchanged.

## Read Order

1. Read the short [$main](skills/main/SKILL.md) skill entry and [goal.json](goal.json).
2. Inspect the current action card; read its complete modules and relevant
   [memory](memory/README.md)/authority owners. Product work also reads the assigned work item and Exit Gate.
3. Before each action, load its indicated stage. Parent review uses
   `load-rules --for review`; review includes the immediately following verification contract.
   Other stages load on demand, not as a global reading checklist.

Source and measured tests outrank verified architecture, approved plans, memory,
and validated experience. Proposed material describes intent, not capability.

## Implementation Direction

For a request to advance the product, identify the next observable behavior and
its missing implementation owner, then deliver that coherent change. Reuse
established profiles and contracts. Inspection and focused checks resolve
specific uncertainties; the formal plan validates the completed slice. Passing
unchanged tests, growing case counts or deleting code does not alone establish
product progress. The shared [implementation guidance](skills/review/references/implementation-guidance.md)
owns selection, source-path review, zero-overhead evidence and verification use.
It preserves explicit read-only, benchmark and maintenance requests.

Controller inspection points to the current implementation objective, an
existing verification session, a concrete failed cause, or the next delivery
action as appropriate. Temporary verification status does not grant acceptance.

## Execution Scale

| Concept | Meaning |
| --- | --- |
| Epoch | One effective objective, route, and governance regime; explicit semantic changes advance it |
| Batch | A bounded collection of work with dependencies inside an Epoch |
| Iteration | One bounded, reviewed, verified delivery; it may complete only a slice of a work item |

Use `epoch-NNNN / batch-NNNN / iteration-NNNN` for product assignments.
Milestones and work items are product delivery coordinates, and lanes are work
lines in the dependency DAG. They add no execution hierarchy. Iteration numbers
identify deliveries, not scheduling priority. The current values live in Goal.

## Repository Roles

| Path | Authority |
| --- | --- |
| `goal.json` | Sole active product route and accepted progress, schema v4 |
| `plan/` | Milestone/work-item deliverables, dependencies, and Exit Gates |
| `memory/` | Stable constraints, decisions, ownership, and terminology |
| `experience/` | Reusable methods with reproducible evidence |
| `skills/` | Control, orchestration, stage, domain and utility packages; each owns its instructions/helpers |
| `lib/` | Shared state, locks, rule selection and verification mechanisms; no second route authority |
| `tmp/main/` | Git-ignored current operation, receipts, and pending transaction; no authorization or product proof |
| `../tmp/` | Build and product-test artifacts |
| `../references/` | Research-only upstream gitlinks and non-normative notes |

Git retains committed prior states. There is no duplicate roadmap, process
archive, progress diary, or numbered review record.

Choose coherent behavior slices and preflight verification coverage before
long runs. The [verification scheduling decision](../docs/architecture/agent-execution.md#verification-scheduling-decision-0059)
and [controller protocol](skills/verify/references/verification.md)
own this boundary; current attempts and logs stay in ignored `tmp/main/`.

## Delivery Loop

[$iteration](skills/iteration/SKILL.md) skill delivers an exact candidate in the context
supplied by the application. The parent reviews bounded coding-subagent diffs
before further dispatch, evaluation, or commit. A qualified delivery immediately
enters [$batch](skills/batch/SKILL.md) skill for composition and actual integration tests.

A slice preserves its lane, Active work item, and target, allocating the Batch's
maximum Iteration plus one. Whole-work-item acceptance requires the complete
Exit Gate before closing it. The acceptance controller selects the first dependency-ready lane in
array order. Batch completion does not complete a milestone or create an Epoch.

Maintenance, acceptance, and Epoch activation automatically publish their exact
guarded commit unless the user limits publication. Worker candidates go to
[$batch](skills/batch/SKILL.md) skill. Publication recovery retains the same commit. Next work continues only
in a matching application-supplied context; otherwise [$main](skills/main/SKILL.md) skill emits the exact
assignment request.

[$epoch](skills/epoch/SKILL.md) skill owns explicit replanning and governance. A complete
approved implementation plan supplies confirmation for its unchanged baseline.
A semantic no-op leaves Epoch unchanged. Knowledge promotion is a shared
internal operation that updates one canonical owner per useful claim.

## Stable Identifiers

Use `milestone-MAJOR.MINOR.PATCH.0`, `work-item-MAJOR.MINOR.PATCH.WORK`,
`decision-NNNN`, `experience-NNNN`, and descriptive `lane-` slugs. Product
SemVer, protocol versions, device epochs, and compiler epochs are independent
namespaces; see the [glossary](memory/glossary.md).
