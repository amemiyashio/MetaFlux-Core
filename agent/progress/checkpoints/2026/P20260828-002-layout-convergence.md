---
id: P20260828-002
status: Recorded
captured: 2026-08-28
milestone: M0001
workstream: M0001-W01
branch: main
git_revision: cca908221cb5b265ae0944a3e87598b1f5a70956
workspace: layout convergence committed; this checkpoint and its session record are committed afterward
---

# Repository Layout Convergence

Milestone: [M0001](../../../plan/M0001-core-foundation/plan.md). Workstream:
[M0001-W01](../../../plan/M0001-core-foundation/work/W01-build-toolchain.md).
This checkpoint records the state at layout revision
`cca908221cb5b265ae0944a3e87598b1f5a70956`.

## Snapshot

The 2026-08-28 layering review found the dependency direction sound and
modularity high, with one undecided placement (transport halves) and one
unenforced rule (dependency legality). Convergence applied five adjustments
modelled on Mesa/Gallium, Wine's `dlls/<name>/unix/` split, and crosvm
rutabaga_gfx:

| Adjustment | Records |
| --- | --- |
| Transport one-directory two-halves convention (C17 client / C++20 worker, no shared headers) | D0010 |
| Machine-checked component graph: role whitelist, C-to-CXX language wall, no client-to-daemon links, run as CTest `metaflux.architecture.component-graph` | D0011 |
| Unit tests beside owners enforced (`runtime/core/tests/`); `tests/` keeps cross-component qualification | [tests](../../../../tests/README.md) |
| Placeholder directories pruned; ownership in `docs/roadmap.md` | [roadmap](../../../../docs/roadmap.md) |
| Repository layout record (Verified): taxonomy, dependency map, placement table | [repo layout](../../../../docs/architecture/repo-layout.md) |

The component graph at dev configures 9 components with 9 dependency edges,
all legal. The check's failure path was verified by injecting an illegal
compat-provider to backend-runtime edge (two errors, nonzero exit).

## Verification evidence

| Gate | Result |
| --- | --- |
| dev / release / asan CTest | 15/15 passed each |
| provider preset CTest | 10/10 passed |
| cuda-only / nvml-only CTest | 6/6 passed each |
| `metaflux.architecture.component-graph` | Passed in every preset |
| Agent records (`tools/check-agent-records.py`) | Passed |

No source, contract header, or provider closure semantic changed; the
ELF/SONAME gates and prior P20260827-001 build evidence remain standing.

## Resume notes

1. Read [current progress](../../current.md), then
   [repo layout](../../../../docs/architecture/repo-layout.md) before placing
   new code.
2. The next material boundary remains M0001-W01: sysroot, CUDA/NVML header
   acquisition, the LLVM 22 patchset, and reference-host baselines that
   promote the provisional budgets to binding.
3. When the first transport implementation begins, create
   `transports/<name>/client/` and `transports/<name>/worker/` per D0010 and
   register both roles so the graph check covers them.
4. Re-run the full preset matrix and `nix flake check path:.` before the next
   checkpoint.

Related work record:
[S20260828-002](../../../sessions/2026/08/S20260828-002-layout-convergence/summary.md).
