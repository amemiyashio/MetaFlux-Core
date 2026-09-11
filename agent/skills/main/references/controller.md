# Controller Interface

Run each application command through the clean Git-aware Nix entry in
[$main](../SKILL.md#bootstrap) skill. The executable is
`agent/skills/main/scripts/main.py --root .`; its operations are `inspect`,
`begin REQUEST.json` or `begin --request-json JSON`, `load-rules [--skill NAME]`,
`step EVENT [--payload PAYLOAD.json | --payload-json JSON]`, and
`resume [--revision FULL_COMMIT]`, `rescope`, and `supersede`. Inline JSON lets bootstrap proceed before
repository writes are permitted. If input files are used, they live under ignored
`agent/tmp/main/`. Inspect reads without creating state. A read-only request
never replaces an active operation. The Agent interprets natural language;
the script validates the selected workflow, rather than guessing user intent.

## Request

Request schema 1 has `kind` (`read-only`, `maintenance`, `iteration`, `batch`,
or `epoch`), `objective`, exact `base_revision`, repository-relative
`allowed_paths`, `checks`, and `publication` (`auto` by default or `local` when
the user limits publication). A trailing slash allows a subtree; other scope
paths match exactly. Epoch requests additionally quote the already received
`confirmation`. Product requests include `assignment` with exact `epoch`,
`batch`, `iteration`, and `lane`. Temporary fields record the conversation;
they never replace actual user authorization or application context.

Optional `skills` names additional required skills. The controller always requires its
own rules, the selected workflow, and the domain skills derived from the
declared paths. Additional skills supplement those owners. A wider scope may
require several domain skills; no new workflow entry or product identity is
created by this selection.

Checks are objects with unique `id`, non-empty `argv` arrays, and optionally
`optional_skip_reason` for an environment-dependent exit 77. The parent selects
checks before evaluation. Required checks must return zero. Receipt creation
executes the commands and retains their raw output and digest.

## Rule Loading Before Mutation

Begin the bounded request first, then run `load-rules` and read its output in the
executing context before `step prepared`. The loader emits the complete bodies
of `AGENTS.md`, [$main](../SKILL.md) skill, this controller reference, and the required
workflow/domain `SKILL.md` files. Follow each skill's routing instructions for
additional references; the loader does not replace that reading.

The current certificate under `agent/tmp/main/rules.json` binds the actual file
modes and blob IDs, required skills, workflow, full HEAD, request/run identity,
and operation base. It is issued after body emission and a version recheck.
This is evidence of emitted instructions, not proof of comprehension or user
authorization. Editing a receipt or declaring that rules were read supplies
neither current bodies nor matching verification evidence.

Begin captures the existing worktree content. Preparation rejects any changes
made after that snapshot and before `prepared`; preserve those changes and use
the controller's recovery diagnostics to obtain a fresh review. Existing edits
remain subject to the bounded scope and parent review. Rule files changed
during authorized implementation must be loaded again before review and
evaluation. Review, evaluate, deliver, and publish each require current rules.
After a successful commit, reload at the committed HEAD before publication;
the pre-commit certificate belongs to the prior HEAD.

The project `.codex/hooks.json` PreToolUse entry enforces this preparation
boundary for covered tool calls. When rules are missing or stale, it supplies
their actual bodies to the current context and denies that attempted write;
loading the bodies does not execute or approve the write. The user/application
must activate trust for the exact hook definition. This host trust is separate
from repository workflow evidence. Coverage and activation belong to
[Tool Hooks](tool-hooks.md); the hook is not an
arbitrary-shell sandbox.

## Events

| Event | Required stage | Effect |
| --- | --- | --- |
| `prepared` | preparation | Requires current loaded rules and unchanged begin snapshot; confirms bounded scope and enters implementation |
| `review` | implementation/review/evaluation/delivery | Payload `summary` binds parent review, current rules, and the check plan to current content; enters evaluation |
| `evaluate` | evaluation | Executes checks; passing receipts enter delivery; failure returns to current implementation |
| `deliver` | delivery | Payload `agent_tool` and `message`; requires exact staged reviewed content; guarded commit then publication or handoff |
| `publish` | publication | Calls governed exact-commit transport; failure preserves this stage and commit |
| `handoff` | handoff | Payload `assignment_request` records the current handoff text; completes this operation |
| `repair` | any pre-commit working stage | Invalidates evidence and returns to bounded implementation; optional payload `checks` revises the check plan within the same scope |

The state response contains stage, evidence, next operation, and delivery
target; `load-rules` also emits the required bodies before that response.
When a failed check needs a different execution plan, `repair` may replace
`checks` while retaining every check ID and its required/optional boundary.
It preserves the objective, paths, baseline, confirmation, and publication.
The new request identity invalidates the old rule certificate; load rules,
review the revised coverage, and execute all checks before delivery. A repair
never converts old logs into passing evidence or revises a committed operation.
The parent provides its user-facing explanation and invokes the next
skill. Handoff is not permission to create a task or source context: emit the
exact Epoch/Batch/Iteration/lane, full base, objective, Exit Gate, and required
checks, then reuse a matching context if the application supplied one.

## Scope Amendment And Governance Replacement

`inspect` emits `evidence.recovery_token`, binding the whole current operation,
HEAD, worktree file blobs/modes, toolchain inputs, and staged entries. Inspect
does not write a Git tree or change the operation. Under the common Git lock:

```sh
python3 -B agent/skills/main/scripts/main.py rescope --expected-state TOKEN --paths-json '["existing/path", "necessary/companion.md"]' --reason 'Explain its dependency on the existing task'
python3 -B agent/skills/main/scripts/main.py supersede --expected-state TOKEN --request-json EPOCH_REQUEST_JSON --reason 'Quote the explicit governance request and explain preservation of unfinished work'
```

Run these through the clean Nix entry. `rescope` adds paths while retaining
every declared path and all other request fields: objective, kind, assignment,
base, checks, extra skills, confirmation and publication. The parent determines
whether the added files implement existing user intent. An omission in that
declaration alone needs no new application context. Reject path traversal,
Git/temporary-state targets, escaped symlinks, and transferred Goal ownership.
Already present out-of-scope edits remain visible: the revised scope must cover
all candidate changes, and preparation/review still assess them.

`supersede` accepts only a confirmed Epoch request at the current base. It does
not infer confirmation from its reason or token. Preserve unrelated product
edits with Git before replacement, keep them outside the governance request and
tested tree, and retain their exact stash object until restoration is verified
by file mode/blob and staged/unstaged disposition. The controller changes no
product files, index, branches, execution context or acceptance state. After the
cutover, preserved older-Epoch work needs a current assignment and fresh review
and execution evidence before acceptance.

Both commands require an intact original request, an unchanged exact inspection
token, no committed delivery, and no pending acceptance transaction. Recover a
pending transaction first; recover a committed delivery through `resume`.
They validate the new request and recheck the token before atomically replacing
current state. Success returns to preparation with a new request identity and
current snapshot, removes rule/context bindings, and discards review/receipt
references. Reload all newly required skill bodies, prepare, review and execute
the checks again. Interruption after state replacement still leaves old evidence
bound to another request. Replaying a stale token fails without replacing state.
There is no history array or retained authority for the superseded operation.

## Verification And Acceptance

`workflow_state.review()` binds the semantic review to content, check plan,
and the current rule-loading certificate.
`evaluate()` executes a reviewed plan and produces schema-1 receipts under
`agent/tmp/main/receipts/`. Candidate receipts use kind `iteration`, integration
receipts use `integration`, and final acceptance receipts use `batch`.
Maintenance and governance use `maintenance` and `epoch` respectively. Receipts
bind actual content, toolchain input identity, HEAD, exact base, reviewed plan
and rule versions, executed commands, return codes, and log hashes. Staging
unchanged content does not invalidate evidence; changing mode, content, HEAD,
or toolchain does. Validation of a committed candidate checks its rule files
against that candidate tree, not the integrator's current working files.

An Iteration delivery has exactly these schema-v2 fields:

```json
{
  "schema_version": 2,
  "epoch": "epoch-NNNN",
  "batch": "batch-NNNN",
  "iteration": "iteration-NNNN",
  "lane": "lane-slug",
  "base_revision": "FULL_BASE",
  "tip_revision": "FULL_TIP",
  "acceptance_kind": "slice",
  "slice_objective": "The observed bounded result",
  "tests": [{"command": ["PROGRAM", "ARG"], "status": "passed"}],
  "verification_receipt": {},
  "exit_gate": null,
  "blockers": [],
  "knowledge_candidates": []
}
```

Replace the empty receipt with the actual candidate receipt. Tests mirror its
executed argv and status (`passed` or `optional-skip`). A `work-item` delivery
sets `exit_gate` to `{ "digest": "SHA256_OF_EXIT_GATE_JSON_STRING", "checks":
["CHECK_ID"] }`. Hash the exact trimmed body under the current `## Exit Gate`
using `workflow_state.digest()`. The mapped checks must pass in both candidate
and integration phases. The parent reviews their full semantic coverage.

[$batch](../../batch/SKILL.md) skill first runs `batch.py check DELIVERY`. After an exact prepared merge or
in-place candidate and knowledge promotion, load the integration's current
rules, perform a fresh parent review, and generate an `integration` receipt
against the delivery's base and actual integration content. Candidate review
and rule evidence do not substitute for this integration review. Pass it to
`batch.py advance DELIVERY --receipt RECEIPT`. This writes only the recoverable
Goal/work-item transition. Then controller review/evaluate with kind `batch` covers
the final acceptance tree before deliver. The acceptance trailer includes the
transaction's exact before/after authority blobs and delivery identity.

## Recovery

Resume rechecks request identity and exact context. An unrelated HEAD change
invalidates an unfinished proposal/assignment. A committed matching operation
restores publication or handoff; `resume --revision` supports lost temporary
state by validating the exact Git workflow record. Pre-commit cache loss
requires a new scope review and actual checks. A pending acceptance rollback
restores only its owned file and index changes; other edits remain intact.

Temporary JSON is not an authorization store. If user scope changed, re-read it
before any transition. The shared lock and expected HEAD/tree guard concurrent
workflow actors. The helper accepts only `-m MESSAGE` or `-F FILE`, expected
HEAD/tree, `--kind`, `--receipt`, and the conversation-emitted `--agent-tool`.
Pre-commit requires the helper's full expected HEAD/tree, receipt, and kind,
and invokes the same `commit_guard` before and after candidate-tree checks.
The guard also requires the current controller operation in delivery, its exact
request and verification receipt, and the matching currently loaded rules.
Resuming or replacing an operation invalidates an old helper invocation even
when HEAD and content have not changed. Lost pre-commit state requires a new
[$main](../SKILL.md) skill request, rule loading, review, and evaluation. Historical candidate receipt
validation remains independent of the integrator's current operation.
Missing input and stale content or rules fail the guard. It preserves
candidate-hook diagnostics and verifies the committed tree.
