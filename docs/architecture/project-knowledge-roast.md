# Project Knowledge Roast

| Field | Value |
| --- | --- |
| Status | Proposed |
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
| `light roasts` | Faithful extraction, normalization, or deduplication of equivalent claims without adding scope, causality, normative force, or a new conceptual relation. |
| `medium roasts` | Bounded comparison, synthesis, or generalization across non-equivalent claims while retaining existing terminology, ownership, constraints, and authority. The result states its boundary and evidence gap. |
| `dark roasts` | Governance-level reconstruction that creates or reframes terminology, a normative rule, an authority relationship, a conceptual model, or a cross-scope consequence. |

Every dark roast resolves to a `DNNNN` decision. When it replaces established
meaning, it additionally follows D0025 through an Active `SCNNNN` before any
protected history changes. `roast` classifies the result; it never authorizes a
semantic replacement by itself.

A later session may roast a claim again when new evidence or context changes
the transformation. The newer result points to its canonical successor instead
of duplicating the same claim across current buckets.

## Evidence And Ownership

Evidence maturity remains independent. A light roast may be `Validated`; a
medium or dark roast may remain `Candidate` or pending verification. Existing
source, contract, architecture, plan, memory, experience, progress, and
semantic-change ownership rules choose the single durable owner. Sessions keep
only the compact claim-to-owner and evidence identity.

`session-only` retains a material claim only when it has current-session resume
or explanatory value and no durable promotion is justified. It records that
reason without a roast label. Disposable experiments, routine commands, raw
logs, duplicate source, and conversation text are removed or omitted through
the existing cleanup boundary.

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

- none.

## session-only

- <claim> - reason: <retention reason>
```

D0026 replaces the former Distillation promotion model and requires a complete
D0025-governed semantic migration. Git and inventoried raw evidence retain the
earlier wording; no compatibility skill, alias, duplicate schema, or archive
remains on the current interface.
