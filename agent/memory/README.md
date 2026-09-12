# MetaFlux Project Memory

Use these indexes to locate the owners relevant to the current task. Do not load
all product decisions for a read-only workflow or tool question:

1. [`project.md`](project.md): product direction and present maturity.
2. [`constraints.md`](constraints.md): durable technical and release limits.
3. [`component-map.md`](component-map.md): source ownership and dependency map.
4. [`decisions-index.md`](decisions-index.md): stable pointers to canonical
   decisions.
5. [`open-decisions.md`](open-decisions.md): aggregated unresolved decisions
   across all milestone plans; the entry point for "what is still undecided".
6. [`glossary.md`](glossary.md): shared terminology.

Memory summarizes stable context but does not replace source, tests,
[`contracts/`](../../contracts/README.md), verified
[`docs/architecture/`](../../docs/architecture/README.md), or approved
[`agent/plan/`](../plan/README.md) records. Update memory when those canonical
sources change; live task state is carried by the conversation and
`agent/goal.json`, and reusable procedures go in `experience/`.
