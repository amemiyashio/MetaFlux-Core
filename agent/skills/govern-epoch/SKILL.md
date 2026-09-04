---
name: govern-epoch
description: Apply an explicitly requested destructive MetaFlux Epoch cutover in a user-supplied governance context, rewriting current authority directly without dispatching workers or creating branches, worktrees, tasks, or threads.
---

# Govern Epoch

Use only when the user explicitly invokes `$govern-epoch` or directly requests
destructive Epoch governance. Do not infer it from ordinary refactoring,
integration conflict, review findings, or product implementation.

## Entry Boundary

Require the user or application to supply a clean dedicated governance worktree
at current main. Do not create, switch, move, replace, or delete a branch,
worktree, clone, task, thread, or chat to establish that context. Read the
active goal, decision index, constraints, plans, skills, gates, and every
affected canonical surface. Confirm concrete governance drift such as duplicate
authority, ambiguous terminology, incompatible rules, stale process machinery,
or process work displacing product progress.

Use bounded read-only subagents for independent semantic inventory, residual
search, or regression review when that improves coverage. Coding subagents are
not the default on a governance cutover: the governing Agent remains the sole
writer of current authority and the sole commit owner. Subagents do not rewrite
authority, advance the Epoch, edit `goal.json`, or create execution contexts.
If the available subagent mechanism creates an independent task, thread,
branch, or worktree, require explicit user or application authorization before
invoking it.

Choose the next monotonic `epoch-NNNN`. Do not publish it yet. Existing branches
and worktrees remain untouched, but no older-base candidate may integrate after
the new Epoch is published without rebase and revalidation.

## Direct Cutover

1. Invoke `roast` over affected old material. Promote every valuable claim to
   exactly one current canonical owner; discard duplicate, local-only, routine,
   and failed-route detail.
2. Record or update the governing `decision-NNNN` in current authority.
3. Rewrite all affected rules, identifiers, schemas, plans, skills, tools,
   tests, links, and memory directly. Delete obsolete surfaces in the same
   candidate tree.
4. Provide no alias, dual-write, compatibility parser, legacy lookup, migration
   ledger, historical synchronization record, or grandfather path. Git is the
   sole old-state recovery mechanism.
5. Set `agent/goal.json.epoch` to the candidate Epoch, reset Batch and Iteration
   numbering, preserve the dependency-valid product target, and define the next
   bounded lanes.
6. Run exact residual scans, the candidate state checker and its self-tests,
   skill validation/routing, commit identity gates, component graph, relevant
   domain tests, full CTest, and `git diff --check`.

Any failure means the candidate Epoch is not published. Repair it in the same
governance work unit and repeat the full regression. Only a completely passing
tree is committed atomically through the `start-work` helper using the candidate
Epoch declaration. After that activation commit is on disk, resolve its full
object ID with `git rev-parse HEAD` and invoke `push-repository` to push exactly
that OID to `refs/heads/main`. A push failure leaves the local Epoch commit
intact and stops through the `git-publish.*` contract; do not retry unchanged
transport evidence.

## Task Stops

Use the `start-work` task-stop contract. Missing explicit destructive authority
or a missing/dirty supplied governance context is
`epoch-governance.explicit-request-required` or
`epoch-governance.context-invalid` with
`user-or-application / preserve-and-report`; the Agent does not manufacture a
context. Residual old authority or regression failure is
`verification.required-gate-failed` with
`epoch-governor / fix-and-retry`; keep the published Epoch unchanged and resume
only after the complete candidate regression passes.

## Output

Report the old/new Epoch, governing decision, deleted authority, canonical roast
promotions, regression results, residual-scan result, and exact activation
revision. Do not create a governance summary or compatibility artifact.
