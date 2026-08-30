---
name: distill-project-knowledge
description: Distill valuable project experience, handoffs, reasoning, failures, and verified breakthroughs into one authoritative durable owner with evidence-aware status. Use at material handoffs, checkpoints, session closure, or when reusable knowledge would otherwise remain transient; do not create a generic archive.
---

# Distill Project Knowledge

Use this skill when a work unit contains conclusions worth surviving the
session. Read [the routing matrix](references/routing-matrix.md) when more than
one durable owner is plausible or the input mixes facts, decisions, methods,
and current state.

## Boundary

This skill classifies and promotes knowledge. It does not own Git commits,
session lifecycle, temporary guidance, decision closure, or technical domain
meaning. Compose with `record-session`, `session-guidance`, `close-decision`,
`govern-semantic-change`, and domain skills at their existing boundaries.

Do not promote conversation transcripts, routine commands, duplicate source,
raw logs, personal preference, or an unbounded collection of thoughts.

## Workflow

1. Break the input into minimum independent claims. Separate observed fact,
   interpretation, decision, reusable method, current state, and unresolved
   question rather than promoting a mixed paragraph.
2. For each claim, name its source, evidence, expected lifetime, and existing
   canonical owner. Deduplicate against that owner before writing anything.
3. Route the claim through the matrix. Promote it to exactly one authoritative
   location; every other record keeps only a compact link, revision, evidence
   identity, or disposition.
4. Keep reusable but unverified engineering knowledge as an `ENNNN Candidate`.
   Only reproducible evidence can make it `Validated`; a plausible explanation
   or one unreproducible success is not validation.
5. Route a replacement of established meaning to `govern-semantic-change`.
   Route unresolved tradeoffs to the owning plan and open-decisions ledger.
   Dispose raw guidance through `session-guidance` before promoting an adopted
   or adapted material outcome.
6. Remove transient and duplicate material after promotion. In the session
   summary, record only the promotion map and any intentionally session-only
   claim with its reason.

## Breakthroughs

When a claim is independently valuable, coherently reviewable, and supported by
passing focused gates, it is a stage breakthrough. Promote its knowledge first,
then use `record-session` to create the Git checkpoint before entering the next
risk or scope phase.

## Output

Produce a compact map in this form:

```text
Promoted: CLAIM -> CANONICAL_PATH_OR_ID (EVIDENCE)
Session-only: CLAIM - RETENTION_REASON | none
```

There is no `agent/distillations/` store. If no claim meets promotion criteria,
record `Promoted: none` after classification rather than using `none` as a
shortcut for skipping it.

## Verification

Check that every promoted claim has one owner, links resolve, experience status
matches its evidence, raw guidance and disposable artifacts leave at their
normal boundaries, and the relevant canonical and Agent-record gates pass.
