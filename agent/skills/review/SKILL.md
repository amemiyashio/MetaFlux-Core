---
name: review
description: Review a bounded MetaFlux implementation against its parent briefing and actual reachable behavior, select a covering verification plan, and promote material knowledge when in scope.
---

# Review

Role: controlling parent, distinct from its coding subagents.
Input: completed coherent diff, briefing, source boundaries and proposed checks.
First action: `main.py load-rules --for review` through the clean Nix entry;
read its complete modules before semantic review.

Use [implementation guidance](references/implementation-guidance.md) to compare
observable before/after behavior, actual callers and fresh/reused state.
A green existing corpus alone does not establish a removed path was unreachable.
Review unsupported cases and real client fallbacks against their owning contract.
State whether the briefing goal was achieved before another coding dispatch.

Choose covering checks for this exact phase. Preserve required modes, fixtures,
build prerequisites and Exit Gate claims. Read [$verify](../verify/SKILL.md) skill
when selecting or changing that plan. Batch integration reads its own workflow
and derives the exact candidate baseline; final Batch review is metadata only.

For Batch acceptance, Epoch governance or explicit knowledge work, read
[knowledge promotion](references/knowledge-promotion.md) and update the unique
owner before final review. Do not produce a review archive or progress diary.

After parent approval, run
`main.py step review --payload-json '{"summary":"Actual semantic conclusion"}'`.
Then follow the verification action card and load its rules before evaluation.
Changed content requires repair/review; rule loading alone does not renew evidence.
