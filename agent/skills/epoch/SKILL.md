---
name: epoch
description: Explicitly audit and replan MetaFlux goals or govern a repository-wide semantic cutover, preserving two-stage proposal confirmation, exact baselines, canonical knowledge, and atomic Epoch publication.
---

# Epoch

Use on an explicit route-replanning or governance request, including when [$main](../main/SKILL.md) skill
composes this skill for that request. Ordinary status, maintenance, or an
automatically completed Batch does not request another Epoch. Bootstrap and
diagnostics are owned by [$main](../main/SKILL.md) skill; the application supplies the execution context.

## Read-Only Proposal

For replanning, prefer the user's supplied objective. With none, present at
most three evidence-backed candidates and wait for a selection. Non-route
governance audits need no invented product objective.

Read current source/tests, verified contracts and architecture, plans, then
memory. Use [$main](../main/SKILL.md) skill's readiness reference. Model the route as a dependency DAG of
milestones, work items, decisions, evidence/reference prerequisites, and lanes.
Report exact baseline/Epoch, one objective and observable success, the four
readiness axes, critical path, lane order, keep/reorder/rewrite/delete decisions,
immediate and deferred decisions, affected owners, residual scans, and tests.

Use `scripts/check_route_proposal.py` for route proposals. Confirmation binds
that exact proposal, clean context, HEAD, and active Epoch. A complete plan
already explicitly approved for implementation supplies that confirmation.
Changed evidence invalidates the proposal; re-audit rather than applying it.
A semantic no-op leaves the Epoch unchanged.

For Agent-harness governance, trace how `agent/` entry instructions, controller
output and domain guidance affect the next implementation action. Prefer a
specific correction demonstrated by a realistic task over more generic rules,
new reports or extra gates. Use
[implementation guidance](../main/references/implementation-guidance.md) to
review whether the resulting flow leads to useful implementation and timely
delivery. Exercise changed guidance independently with representative scenarios;
do not present a governance publication as product capability or performance.

## Confirmed Governance

The governing parent is the sole current-authority and Goal writer for the
cutover. Read-only subagents may independently inventory semantics and review
regression. No worker dispatch or execution-context creation is implied.

When a pre-commit operation is already active, use the `supersede` command in
[$main](../main/SKILL.md) skill for this explicitly confirmed governance request.
First preserve unrelated product changes in Git and exclude them from the
governance tree. Check exact recovery inputs, reload rules and prepare again;
do not mark the old operation complete or delete its state to unlock governance.
Restore preserved candidate contents after publication without claiming their
acceptance or reusing older-Epoch verification.

1. Promote useful knowledge to one canonical owner using [$main](../main/SKILL.md) skill's reference;
   discard duplicate process material. Record the governing decision.
2. Rewrite every affected current authority, skill, checker, caller, test,
   link, and term. Remove superseded entrypoints in the same candidate; Git
   preserves old states without aliases, compatibility readers, or an archive.
3. Advance the Epoch monotonically and restart Batch/Iteration numbering.
   Retain verified completed work, stable delivery IDs, and dependency-valid
   targets. Planning never closes an evidence-bound technical decision.
4. Preflight one complete regression plan after the coherent governance diff
   and parent review. Run the complete dev build and full CTest once in that
   evaluation. Registered state/self-tests, routing, reference, commit and
   component/domain gates are covered by that suite; add required unregistered
   skill validation and residual scans separately. Preserve distinct execution
   modes and explicit environment skips. A skipped environment is not product
   qualification. Repair a failure in this same unit and invalidate changed
   content's evidence; never reuse another phase's receipt as a test cache.
5. Commit the fully verified activation with [$main](../main/SKILL.md) skill's guarded helper and publish
   its exact revision. A push failure preserves the activation for publication
   recovery. Older-base work is revalidated against the active Epoch before
   later acceptance; existing branches remain intact.

Keep product implementation separate from this explicit governance scope.
Report old/new Epoch, concrete authority changes, validation, and exact commit
and publication result, without a governance diary.
