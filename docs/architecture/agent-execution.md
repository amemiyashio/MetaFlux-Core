---
status: Verified
decision: decision-0033
updated: 2026-09-10
---

# Goal-First Multi-Agent Execution

## Decision

MetaFlux uses Epoch, Batch, and Iteration to coordinate product work. Decision-0054
consolidates the operational interface while retaining product delivery IDs and
the application ownership of execution contexts. Goal records product intent
and accepted progress; activity and identity never confer repository authority.

## Four Workflow Skills (decision-0054)

The four entries are `main`, `epoch`, `batch`, and `iteration`. Main bootstraps
identity and Nix, interprets the current request, and controls preparation,
implementation, review, evaluation, delivery, publication, and handoff. These
stages have no numbers. Its scripts validate events and evidence; the parent
Agent calls the appropriate skill. Scripts create no agent, branch, worktree,
clone, task, or thread.

Epoch is one effective objective, route, and governance regime. Batch is a
bounded set of dependent work in that Epoch. Iteration is one bounded, reviewed,
verified delivery and may cover part of a work item. Milestones and work items
remain product targets; lanes describe DAG work lines rather than another
execution level.

The consolidation removes duplicate entrypoints and makes shared identity,
readiness, knowledge promotion, commit, and publication capabilities main's
internal operations. Eleven domain skills and four utility skills retain their
existing product/tool ownership. Packages remain flat and the routing policy
is owned once by `tools/check-agent-state.py`.

Rationale: separate overlapping entrypoints allowed contradictory dispatch and
progress rules. One controller makes the next required action explicit, while
four operation owners preserve meaningful write boundaries. This governance
changes workflow capability only and does not promote product maturity.

Verification: four-entry discovery/routing; controller, proposal, acceptance,
commit and publication behavioral tests; authority/temporary-state checks;
component graph; complete dev build and CTest. Failure leaves the candidate
Epoch unpublished.

## Rule Loading Boundary

The mandatory-read rule is enforced inside the existing main stages. Main
binds actual rule-body emission to the request, scope, base, HEAD, and current
file versions before preparation allows implementation. Review and verification
carry that certificate with the candidate content and check plan. Candidate
validation uses its own tree; integration requires fresh rule loading and
parent review. Publication reloads at the committed HEAD. The certificate is
evidence of body emission, not comprehension or user authorization. Required
references inside skills still need to be read by the Agent. The exact
preparation and reloading protocol is owned by
[main's controller](../../agent/skills/main/references/controller.md).

The project PreToolUse hook supplies missing rule bodies to the current context
and denies the current covered write until preparation succeeds. The
user/application activates trust in its exact definition; a tracked hook file
does not establish host trust. This tool boundary does not sandbox arbitrary
shell execution. [Tool Hooks](../../agent/skills/main/references/tool-hooks.md) owns coverage and activation.
These checks implement mandatory reading without adding workflow entries,
changing Goal/Epoch, or treating temporary receipts as product authority.

## State Model

Goal schema v4 in `agent/goal.json` is the sole active product-route and accepted
progress authority. It contains Epoch, Batch, target, objective, reference
prerequisites, and lanes; no process stage or history arrays. Lane states are
`planned`, `integrated`, and `deferred`; Batch states are `open` and `integrated`.

Ignored `agent/tmp/main/` contains only the current worktree's operation state,
rule/context evidence, verification receipts, delivery envelopes, and pending
transaction. It supplies neither authorization nor product-completion evidence.
Authority scanning skips this directory, and staged contents are rejected even
when force-added.
Build and product-test artifacts remain under root `tmp/`. Git preserves prior
committed facts; no process archive or duplicate route database exists.

## Worker Contract

Main keeps inspection read-only, including status questions during active work.
Maintenance uses its review/evaluation/commit/publication stages without a
product tuple or Goal change. Product work enters iteration with an exact base
and full `epoch-NNNN / batch-NNNN / iteration-NNNN` plus lane assignment from the
application. The parent briefs bounded coding subagents and reviews returned
diffs against the briefing and domain boundaries before further dispatch.
Coding subagents do not change Goal, accept, govern, commit, publish, or allocate
contexts. Parent semantic review remains necessary alongside machine checks.

Delivery schema v2 names exact base/tip and the product tuple, `acceptance_kind`
(`slice` or `work-item`), `slice_objective`, actual checks,
`verification_receipt`, `exit_gate`, blockers, and `knowledge_candidates`.
Reported passed strings without executed receipts are rejected.

## Automatic Acceptance And Advancement (decision-0052)

Main immediately invokes batch after a qualified committed candidate. Batch
checks existing validated acceptance history before current Iteration identity
so a replay cannot advance twice. A current-HEAD candidate is accepted in place;
a divergent candidate needs its exact prepared merge and fresh integration
verification. A stale non-HEAD ancestor is not a new candidate.

Candidate and integration receipts independently bind the baseline, tested
content, loaded rule versions, toolchain inputs, reviewed check plan, actual
results, and output digests. Changed inputs invalidate affected evidence.
Optional skips are not passes for required Exit Gates. Knowledge promotion
happens before final evaluation so its changes also receive review and
verification.

A slice retains its planned lane, Active work item, and target, and assigns that
lane the Batch's maximum existing Iteration plus one. Exhaustion never wraps.
Whole-work-item acceptance requires the current complete Exit Gate, completes
the lane/work item, and selects the first array-ordered planned lane whose
dependencies are integrated. Iteration numbers carry no priority. Batch closure
does not complete a milestone or begin an Epoch.

Batch alone makes daily accepted-state changes. Its pending transaction retains
exact before/after blob IDs and modes for each affected authority. Final
evaluation covers the advanced tree before one acceptance commit.

## Commit Boundary

Main's shared helper requires expected HEAD, exact staged tree, operation kind,
and a current verification receipt with its reviewed rule certificate.
Pre-commit calls the same `commit_guard` before and after candidate-tree checks
and requires all four inputs, including full expected HEAD/tree IDs. Direct
Git invocation does not substitute for those inputs. Candidate-tree hooks load
helpers from that same tree and remain side-effect-free in the invoking
repository. Linked worktree fixtures clear Git local environment variables.
Unexpected changes are attributed to exact reflog/hook/test evidence before
another actor is blamed.

Workflow commit trailers bind the operation, tree, verification digest, and
publication policy. Acceptance additionally binds delivery identity, candidate,
kind, integration receipt, and authority blob transition. Replays verify actual
Git state rather than trusting a trailer label alone. Decision-0034 owns
conversation-derived Author/Committer identity; Epoch is not a Git identity or
environment declaration.

## Publication And Recovery

Maintenance, Batch acceptance, and Epoch activation automatically publish their
exact guarded commit unless the user limits publication. A worker candidate goes
to Batch. Main's governed transport validates canonical main, Nix Git/OpenSSH,
the external public fingerprint, and a narrow exact refspec without force.

Remote revision equality proves publication. A different remote revision must
be ancestry-checked; if it contains the target, that delivery is published, but
new work waits for an application-supplied current base. Divergence preserves
the local commit and reports the required context. Publication failure resumes
publication of the same commit instead of repeating acceptance.

The common Git lock serializes transitions, and expected input checks detect
concurrent changes. Pre-commit recovery restores only transaction-owned bytes
and modes, preserving external edits. Lost pre-commit state requires renewed
review and evaluation. Post-commit recovery validates Git records and restores
only publication/handoff. Temporary flags never create authorization.

Main continues next work when the application already supplied a matching
context. Otherwise it emits the precise tuple, lane, full base, objective, and
checks required for allocation; scripts do not allocate that context.

## Task-Stop Contract

[main](../../agent/skills/main/SKILL.md#task-stop-diagnostics) owns the diagnostic
shape rendered by `tools/agent_diagnostics.py`. Preserve child causes and raw
tool output. External responsibility does not authorize scheduling, privilege,
cleanup, source copies, or unchanged retries. Diagnostics are current-operation
output, not product authority or historical records.

## Route Replanning (decision-0045)

Epoch is explicit-only. Stage 1 is read-only and reports the exact revision and
Epoch, objective and observable success, architecture/activation/maturity/release
matrix, DAG/critical path/lane order, node dispositions, decision timing,
affected owners, residual scans, and regression plan. Without a supplied focus,
offer at most three candidates and wait for selection.

Stage 2 requires confirmation of the unchanged proposal. A complete approved
implementation plan supplies that confirmation. Recheck HEAD, worktree, and
active Epoch; changed input invalidates the proposal. No-op leaves Epoch
unchanged. Completed milestones/work items stay closed. Active/Queued nodes may
move; unchanged delivery coordinates and outputs retain their IDs. Planning
does not close evidence-bound technical decisions or implement product code.

## Epoch Governance

The governing parent rewrites affected current authority, removes obsolete
semantics, promotes reusable facts, advances Epoch, and resets Batch/Iteration
numbering while retaining completed product facts. No aliases, compatibility
parsers, dual writes, or migration ledgers remain. The next Epoch is committed
and published only after complete regression. Older-base product work needs a
current application context and renewed verification before acceptance.

## Knowledge Promotion

Promote each measured reusable claim to one source, test, contract, plan,
decision, constraint, glossary, or validated experience owner. Discard duplicate
process material. Knowledge promotion is a normal shared step with no separate
entrypoint, grading vocabulary, or archive. Standalone promotion stays within
the user's requested scope.
