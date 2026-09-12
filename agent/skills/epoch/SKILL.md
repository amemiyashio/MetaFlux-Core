---
name: epoch
description: Govern an explicitly requested MetaFlux semantic cutover or route replan from an exact confirmed baseline, preserving accepted product work and publishing the verified next Epoch.
---

# Epoch

Role: governing parent, sole current-authority/Goal writer for this cutover.
Input: explicit user governance or route request at an exact baseline.
First action: choose one mode below; status or routine maintenance stays outside
governance. Read-only subagents may audit/review; no coding-worker dispatch.

- Route replan: read [proposal](references/proposal.md), audit the requested
  objective/DAG, then confirm its unchanged baseline. An already approved full
  implementation plan supplies confirmation. A no-op does not advance Epoch.
- Non-route governance: inspect the affected mechanism, its owners, current
  failures and intended observable correction. For Agent-harness work, trace
  first actions, required readings and transitions. Do not invent a new product
  objective or require a product-route DAG report.

Once confirmed, use [$prepare](../prepare/SKILL.md) skill:
begin → load preparation rules → prepared → load implementation rules.
Then read [cutover](references/cutover.md) before rewriting authority.
An unfinished pre-commit operation uses [$recover](../recover/SKILL.md) skill's
supersede after preserving unrelated edits.

Parent review and actual verification use their stage skills. Governance always
runs a complete dev build/full CTest once after the coherent diff and preflight.
Qualified activation automatically proceeds to guarded delivery and publication.
Report old/new Epoch, actual changes and evidence, and exact local/remote commit;
governance does not promote product capability or performance.
