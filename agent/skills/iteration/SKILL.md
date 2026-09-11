---
name: iteration
description: Implement and deliver one bounded MetaFlux product Iteration assigned by the application, with a parent briefing, domain skills, parent review, actual checks, and an exact committed candidate for batch acceptance.
---

# Iteration

Enter through [$main](../main/SKILL.md) skill Bootstrap. Require an application-supplied registered
worker context at an exact base and the active Epoch/Batch/Iteration/lane tuple.
The lane remains planned and its work item Active until [$batch](../batch/SKILL.md) skill accepts it.
Do not create an execution context or select a sibling lane.

1. Read the work item's Exit Gate, current source/tests, and domain skills.
   Use [$main](../main/SKILL.md) skill's readiness reference to separate implementation prerequisites
   from later qualification requirements. Choose one independently verifiable
   slice within the assigned lane; its objective and checks precede edits.
   Trace affected manifests, summaries and test consumers into the declared
   scope. Use the controller's `rescope` through [$main](../main/SKILL.md) skill
   for necessary omitted companion files, with fresh rules, preparation,
   review and verification; this alone does not change the assignment.
2. For declared reference prerequisites, run `references/tools/reference.py`
   materialize and verify through Nix. Treat that exact detached source as
   research only. No undeclared reference operation is implied.
3. Brief bounded coding subagents with exact identity/base, allowed and
   forbidden paths, domain constraints, objective, checks, and drift surfaces.
   The parent reviews the returned diff in conversation before another
   dispatch, evaluation, or commit. Subagents do not change Goal, integrate,
   govern, commit, push, or create contexts.
4. Run [$main](../main/SKILL.md) skill's evaluation against the actual candidate tree and commit through
   the shared guarded helper. Changed code invalidates corresponding review
   and verification evidence. Failed checks stay in the current unaccepted
   Iteration and produce a repaired exact tip.
5. Emit delivery schema v2 under `agent/tmp/main/` and return it to [$batch](../batch/SKILL.md) skill.
   It contains exact epoch/batch/iteration/lane and base/tip, `acceptance_kind`
   (`slice` or `work-item`), `slice_objective`, actual test results,
   `verification_receipt`, empty blockers, and material `knowledge_candidates`.
   A work-item claim needs its whole current Exit Gate, not a partial test pass.
6. The controlling parent invokes [$batch](../batch/SKILL.md) skill immediately. The worker does not push
   its candidate or manufacture the next assignment.

Use [$main](../main/SKILL.md) skill's diagnostic contract. Preserve supplied edits
and distinguish an omitted file declaration from a changed execution baseline
or assignment. Only the latter needs an exact updated application context.
Linked-worktree and fixture Git commands clear Git local environment variables;
an unexpected HEAD/index change is traced to its invoking hook/test and reflog.
