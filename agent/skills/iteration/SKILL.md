---
name: iteration
description: Implement one application-assigned MetaFlux product slice and deliver its exact reviewed and verified candidate to the controlling Batch integrator.
---

# Iteration

Role: candidate-delivery parent. Your bounded coding subagents implement source;
the controlling parent holding an integration context owns subsequent acceptance.
Input: exact application-supplied Epoch/Batch/Iteration/lane and base, active work
item, Exit Gate and user objective. Never allocate a context or sibling lane.

First action: read the assigned Exit Gate and relevant source/owners, then use
[$prepare](../prepare/SKILL.md) skill: begin → load preparation rules → prepared
→ load implementation rules. A declared reference prerequisite is materialized
and verified through `references/tools/reference.py` only on demand.

Implement a coherent observable behavior family under the owning domain skills
and [implementation guidance](../review/references/implementation-guidance.md).
Use a self-contained coding briefing with objective, identity/base, scope,
boundaries, checks and drift surfaces. Parent review accepts the returned diff
before another coding dispatch; subagents never edit Goal, accept, govern,
commit, push or create contexts.

Focused checks resolve implementation uncertainty. After the coherent behavior
is ready, load [$review](../review/SKILL.md) skill, obtain parent approval, then
[$verify](../verify/SKILL.md) skill executes the actual candidate plan.
[$deliver](../deliver/SKILL.md) skill commits and emits
[delivery schema v2](references/delivery.md) from the actual receipt.

The controlling parent immediately passes the exact candidate to
[$batch](../batch/SKILL.md) skill using its matching integration context.
If that context is absent, emit the exact application request; do not fabricate
one. The candidate stays unaccepted and the worker never pushes. Failed checks
stay in this bounded Iteration for repair.
