---
name: roast
description: Explicitly classify valuable MetaFlux knowledge and promote each material claim to one canonical owner without creating a knowledge or execution archive.
---

# Roast

Use only when explicitly invoked or when `integrate-batch` or `govern-epoch`
composes it. Roast is an in-memory disposition step, not a record type.

## Workflow

1. Split the candidate into independent facts, interpretations, decisions,
   reusable methods, current product state, and unresolved choices.
2. Discard routine commands, duplicate meaning, conversation detail, local-only
   host facts, and failed routes that do not prevent a repeatable engineering
   error.
3. For each valuable claim, choose exactly one canonical owner:
   source, test, contract, plan, decision, constraint, glossary/component map,
   or experience.
4. Update that owner materially. A link to already equivalent content is not a
   promotion.
5. Classify the transformation independently of evidence maturity:
   - `light`: faithful extraction or normalization with equivalent meaning;
   - `medium`: bounded synthesis within existing authority and terminology;
   - `dark`: an explicitly authorized reconstruction of project-level meaning,
     terminology, ownership, or normative behavior.
6. A dark promotion requires a current `decision-NNNN`. A breaking replacement
   also requires explicit `govern-epoch`; no semantic-change ledger is created.
7. Route unresolved choices to the owning plan and open-decisions table. Do not
   disguise them as conclusions.

## Result

Return a compact list of `depth / claim / canonical owner / evidence` and a
short discarded-material count. Do not create roast files, summaries,
checkpoints, or compatibility records. Integration and governance commits carry
the actual canonical-owner changes; Git preserves their prior form.

Verify that every promoted claim changed exactly one owner, links resolve,
experience status matches reproducible evidence, and no transient input remains
in the current tree.

## Task Stops

Use the `start-work` task-stop contract. Implicit standalone use is
`roast.explicit-request-required` with
`user-or-application / stop-and-report`. A dark promotion without a current
decision or a breaking replacement without explicit Epoch governance is
`roast.authority-missing` with the corresponding decision or Epoch authority;
leave canonical owners unchanged until that authority exists.
