---
name: roast
description: Explicitly classify and promote material project knowledge by semantic transformation depth while keeping evidence maturity, governance authority, and session-only disposition separate. Use only through `$roast` or an explicit repository workflow composition; do not use for code critique or ordinary summaries.
---

# roast

Use this skill only when explicitly invoked as `$roast` or when `start-work` or
`record-session` explicitly composes it at a material breakthrough, handoff, or
close. Read [the routing matrix](references/routing-matrix.md) when the input
mixes claim types or more than one durable owner appears plausible.

## Boundary

`roast` classifies and promotes project knowledge. It does not own Git commits,
session lifecycle, temporary guidance, decision closure, semantic-migration
authority, or technical domain meaning. Compose those responsibilities with
`record-session`, `session-guidance`, `close-decision`,
`govern-semantic-change`, and the matching domain skill.

Only a claim that this work unit materially creates or updates in one canonical
repository owner enters a roast bucket. A link to an already equivalent owner,
conversation text, routine command, duplicate source, raw log, or disposable
experiment produces no roast row. There is no roast archive.

## Workflow

1. Split the work unit into minimum independent claims. Separate observed fact,
   interpretation, decision, reusable method, current state, and unresolved
   choice.
2. Choose exactly one disposition for every material outcome:
   - promote it by creating or materially updating one canonical owner;
   - retain it under the independent `session-only` section with a bounded local
     reason; or
   - route an unresolved choice to its plan and the open-decisions ledger.
   Omit routine, duplicate, and disposable material through normal cleanup.
3. For each promoted claim, record its source, evidence, expected lifetime, and
   single canonical owner. Deduplicate before writing; an equivalent owner with
   no material change is omission, not promotion.
4. Assign exactly one semantic transformation depth:
   - `light roasts`: preserve equivalent meaning through faithful extraction,
     normalization, or merging into a newly created or updated owner;
   - `medium roasts`: perform bounded synthesis across non-equivalent claims
     inside existing terminology, constraints, ownership, and authority;
   - `dark roasts`: apply an already authorized project-governance
     reconstruction that changes canonical interpretation, terminology,
     normative rules, ownership/authority, or behavior across canonical owners.
5. Evaluate evidence maturity separately. A light result may be `Validated`; a
   medium result may remain `Candidate`. Reusable methods use the existing
   `ENNNN` lifecycle and reproducer rules.
6. Require every dark row to resolve to a `DNNNN`. If the result replaces
   established meaning, require the matching Active `SCNNNN` before promotion.
   A prospective dark result without authority remains an unresolved choice and
   cannot enter a terminal dark bucket.
7. Independently route any breaking semantic, identifier, constraint,
   record-contract, or authority replacement through `govern-semantic-change`,
   even when its claim transformation is light or medium.
8. Remove transient or duplicate material after promotion. Keep only the compact
   claim-to-owner, evidence identity, and independent local-retention reason in
   the session. `session-only` does not authorize artifact retention: when its
   claim depends on a retained local file, `record-session` must also list that
   file and its owner under `## Cleanup`.

## Output Contract

Use one lowercase `## roast` section with its three lowercase buckets in this
exact order, followed by the independent lowercase `## session-only` section:

```markdown
## roast

### light roasts

- CLAIM -> CANONICAL_PATH_OR_ID (EVIDENCE)

### medium roasts

- none.

### dark roasts

- CLAIM -> CANONICAL_PATH_OR_ID (EVIDENCE; authority: DNNNN, SCNNNN)

## session-only

- CLAIM - reason: RETENTION_REASON
```

Use `SC not required` beside the decision only for an additive dark result that
replaces no established meaning. Each container uses either one or more entries
or the sole line `- none.`. An active scaffold may instead use the sole line
`- TODO.`; terminal summaries contain no placeholder. A claim appears in only
one destination in the current summary and `session-only` never receives a
roast label.

## Verification

Verify that every promoted claim materially changed exactly one canonical
owner, every link and evidence identity resolves, evidence status matches its
reproducer, every dark row has the required authority, unresolved choices are
not disguised as session-only conclusions, every retained local artifact is
also accounted for by cleanup, and no legacy skill/schema or raw guidance
remains on a live surface. Run the relevant canonical and Agent-record gates
before checkpoint or close.
