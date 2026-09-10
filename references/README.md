# Reference Sources

This directory is MetaFlux-Core's catalog of upstream material used during
implementation research. Git source entries are submodule gitlinks: the Core
repository records only an upstream URL and exact commit, while the source
bytes remain in the upstream repository.

A normal clone does not download reference sources. Materialize only the entry
needed for the current investigation:

```sh
nix develop . --command python3 -B references/tools/reference.py list
nix develop . --command python3 -B references/tools/reference.py materialize pytorch-v2.11.0
nix develop . --command python3 -B references/tools/reference.py verify pytorch-v2.11.0
```

Materialized sources appear under `references/sources/` as detached submodule
worktrees and remain outside the Core repository history. They are
reference-only: no MetaFlux build, test, package, release, or product
capability depends on their presence. Product facts derived from inspection
must be promoted to the owning source, test, contract, decision, constraint,
or plan before they are relied upon.

## Layout

| Path | Role |
| --- | --- |
| `catalog/` | Versioned reference manifests and their schema |
| `sources/` | Exact upstream gitlinks; source worktrees appear only on demand |
| `notes/` | Small path, symbol, and call-graph observations |
| `tools/reference.py` | Catalog listing, status, materialization, and verification |

The catalog does not replace `toolchains/`. If an upstream input becomes part
of a build or qualification gate, its identity must be promoted into the
canonical Core toolchain manifest.

## Reference-Only Source Catalog (decision-0047)

MetaFlux keeps reference source identity inside this repository so the active
route can point to one catalog entry without a second roadmap or external
state database. The catalog owns research provenance only. Git owns the Core
tree and each exact submodule gitlink; `toolchains/` continues to own every
external input consumed by a build or qualification gate.

A reference prerequisite establishes implementation readiness, not product
maturity or release evidence. `agent/goal.json` may require an entry before a
lane begins, but lane acceptance still comes from Core source and owning
tests. Notes remain non-normative until knowledge promotion promotes a relied-upon claim to
one canonical Core owner.

A reference update changes the smallest manifest and matching gitlink in one
commit. It never vendors upstream bytes, adds a default recursive clone,
rewrites the upstream worktree, or turns its availability into a runtime or
release dependency.
