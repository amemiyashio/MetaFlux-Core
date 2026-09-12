---
name: batch
description: Integrate and accept an exact qualified MetaFlux candidate with fresh combined evidence, advance only its accepted product state, and publish the acceptance commit.
---

# Batch

Role: controlling parent with an application-supplied integration context.
Input: exact committed delivery v2 and current-main base matching that assignment.
First action: `scripts/batch.py check DELIVERY --root .` through clean Nix.
A validated prior acceptance is a no-op; report its exact publication state.
Read-only checks create no operation. Ambient patches are never delivery inputs.

For a new delivery, use [$prepare](../prepare/SKILL.md) skill:
begin a Batch request with checks omitted → load preparation rules → prepared
→ load implementation rules. The fixed final plan is acceptance
metadata/Agent-state/routing; product checks belong to integration.

Read [integration](references/integration.md) before merge or verification.
Keep a current-HEAD candidate in place; compose only the exact supplied tip.
Fresh integration receipt → `batch.py advance` prepares the sole
Goal/work-item transaction → `batch.py check-metadata` proves its exact delta.
Then load review rules, review the advanced tree, load verification rules and
execute the fixed final plan. Semantic changes require fresh integration.

Slice acceptance preserves lane/target/work item and allocates the Batch maximum
Iteration plus one. Whole-work-item acceptance needs the complete Exit Gate.
Next work uses dependency readiness and lane order. Batch exhaustion never
completes its milestone or creates an Epoch.

Stage reviewed acceptance paths and use [$deliver](../deliver/SKILL.md) skill.
A staging-only rejection preserves evidence. Then [$publish](../publish/SKILL.md) skill publishes the exact acceptance commit. Retry publication of that commit
without accepting again; handoff names the next exact assignment/base.
