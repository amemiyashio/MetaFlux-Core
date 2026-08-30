# Project Knowledge Roast

| Field | Value |
| --- | --- |
| Status | Verified |
| Decision | D0026 |
| Applies to | Durable project-knowledge promotion, session summaries, and checkpoint handoff |

## Decision

MetaFlux names its durable project-knowledge refinement workflow `roast`.
Coffee roast levels are used only as an analogy for semantic transformation
depth: `light roasts`, `medium roasts`, and `dark roasts` do not rank evidence
strength, importance, quality, or retention time.

The workflow first splits material outcomes into independent claims and chooses
their disposition. A claim enters `roast` only when it is promoted to one
durable canonical owner. Material needed only to resume or explain the current
session is recorded under the independent `session-only` disposition, and
routine or disposable material is handled by session cleanup. Neither is a
fourth roast level.

## Transformation Depth

Each promoted claim receives exactly one final depth in the current summary:

| Depth | Semantic operation |
| --- | --- |
| `light roasts` | Faithful extraction, normalization, or merging of equivalent inputs into one newly created or materially updated canonical claim, without adding scope, causality, normative force, or a new conceptual relation. |
| `medium roasts` | Bounded comparison, synthesis, or generalization across non-equivalent claims while retaining existing terminology, ownership, constraints, and authority. The result states its boundary and either its evidence gap or reproduced evidence. |
| `dark roasts` | Authorized project-governance reconstruction whose acceptance changes canonical interpretation, terminology, a normative rule, ownership or authority, or behavior across canonical owners. Complexity alone never makes technical synthesis dark. |

Every dark roast resolves to a `DNNNN` decision. When it replaces established
meaning, it additionally follows D0025 through an Active `SCNNNN` before any
protected history changes. A prospective dark result is not promoted and does
not enter a terminal dark bucket until that authority exists; before then, the
underlying choice is routed to its plan and the open-decisions ledger.

D0025's trigger is independent of roast depth. Any change that replaces
established meaning, an identifier, a record contract, a constraint, or an
authority boundary requires its decision and SC even when the promoted claim is
light or medium. Conversely, an additive dark decision does not create an SC
when it replaces nothing. `roast` classifies the result; it never grants
migration authority.

A later work unit may classify the semantic transformation of its own new or
updated promotion differently as context changes. Roast depth is not a durable
owner status, identity, or successor chain; prior summaries remain historical
evidence and no roast archive is created.

## Evidence And Ownership

Evidence maturity remains independent. A light roast may be `Validated`; a
medium roast or an authorized dark roast may remain `Candidate` or pending
verification. Existing
source, contract, architecture, plan, memory, experience, progress, and
semantic-change ownership rules choose the single durable owner. Here durable
means that the claim leaves transient conversation, guidance, and session-local
disposition for the repository owner whose own lifecycle governs it; it does
not mean permanent or immutable. Sessions keep only the compact claim-to-owner
and evidence identity.

Promotion requires the work unit to create or materially update canonical
content, evidence, or status. Finding an already-owned equivalent claim without
such a change is duplicate omission, not a light roast row.

`session-only` retains a material claim only when it has current-session resume
or explanatory value and no durable promotion is justified. It records that
reason without a roast label and cannot carry an unapproved governance
conclusion. It does not authorize a local artifact to survive cleanup: when the
claim depends on a retained file, `record-session` also records that path, owner,
and retention reason under `## Cleanup`. Every material outcome is either promoted into exactly one roast
bucket, retained only under `session-only`, or routed as an unresolved choice to
its canonical plan/open-decision owner. It cannot appear in more than one of
those destinations. Disposable experiments, routine commands, raw logs,
duplicate source, and conversation text are removed or omitted through the
existing cleanup boundary.

## Invocation And Composition

The repository skill is named `roast` and is explicit-only as `$roast`.
Ordinary uses of the English verb, including requests to critique or "roast"
code, do not invoke this project-knowledge workflow. `start-work` and
`record-session` explicitly compose `$roast` at a material breakthrough,
handoff, or close.

`roast` owns claim splitting, disposition, transformation depth, canonical
promotion, and compact residue. `record-session` owns Git checkpoints, cleanup,
and session lifecycle; `session-guidance` owns transient advice;
`govern-semantic-change` owns breaking migration authority; domain skills own
technical meaning. There is no generic roast archive.

## Session Contract

Post-policy session summaries use one lowercase `## roast` section containing
the three ordered lowercase buckets, followed by an independent lowercase
`## session-only` section. Each bucket or disposition contains either one or
more entries or the sole value `- none.`. Active scaffolds may use `- TODO.`;
terminal summaries may not.

```markdown
## roast

### light roasts

- <claim> -> <canonical owner> (<evidence>)

### medium roasts

- none.

### dark roasts

- <claim> -> <canonical owner> (<evidence>; authority: DNNNN, SCNNNN | SC not required)

## session-only

- <claim> - reason: <retention reason>
```

D0026 replaces the pre-D0026 promotion model and requires a complete
D0025-governed semantic migration. Git and inventoried raw evidence retain the
earlier wording; no compatibility skill, alias, duplicate schema, or archive
remains on the current interface.
