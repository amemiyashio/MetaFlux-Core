---
id: P20260827-001
status: Recorded
captured: 2026-08-27
milestone: M0001
workstream: M0001-W01
branch: main
git_revision: null
workspace: all files untracked
---

# Engineering Bootstrap Baseline

Milestone: [M0001](../../../plan/M0001-core-foundation/plan.md). Workstream:
[M0001-W01](../../../plan/M0001-core-foundation/work/W01-build-toolchain.md). The
repository has no Git revision; all repository files were untracked when this
checkpoint was recorded.

## Snapshot

The monorepo scaffold, Nix environment, compiler epoch descriptor, CMake target
graph, initial contract boundaries, provider fixtures, runtime/compiler/backend
fixtures, package outputs, and cross-component qualification tests are present.
They establish ownership and build constraints but do not implement the M0001
vertical slice.

The tracked collaboration layer contains stable M0001-M0004 plans, durable
memory, three validated experience records, current progress, this checkpoint,
templates, and the reconstructed bootstrap session. Architecture and public
contracts remain authoritative outside `agent/`.

## Verification evidence

| Gate | Result |
| --- | --- |
| Full development CTest | 14/14 passed |
| Full ASan CTest | 14/14 passed |
| Provider preset CTest | 9/9 passed |
| CUDA-only CTest | 5/5 passed |
| NVML-only CTest | 5/5 passed |
| Agent records | Project-only records, references, event types, and output hashes passed |
| `nix flake check path:. -L` | Passed, including `agent-records` |
| Nix packages | `runtime`, `provider`, `daemon`, and `toolchain` built |

Provider qualification included independent bootstrap, SONAME, `DT_NEEDED`, and
versioned export checks plus dual-provider co-loading. Nix qualification included
the provider release closure, runtime install consumer, LLVM/MLIR toolchain SDK
consumer/RUNPATH, formatting, and the shared test graph.

## Durable outcomes

- Compiler epoch 1 has one JSON source consumed by CMake and Nix.
- Client protocol and backend plugin ABI have independent version lines.
- Providers do not expose the statically embedded client fast-path symbol.
- Compatibility frontends and execution backends are independently selectable.
- Agent records have a separate Nix fileset/check and do not invalidate runtime,
  provider, daemon, or toolchain package sources.
- Architecture intent remains in plans and proposed records; fixtures are not
  represented as completed product functionality.

## Resume notes

1. Inspect `git status` before changing files; no Git revision reconstructs this
   snapshot.
2. Use `nix develop path:.` while the flake remains untracked.
3. Read [current progress](../../current.md), then the relevant part of M0001.
4. Re-run the affected narrow gate and full `nix flake check path:.` before the
   next checkpoint.

Related validated experience: [E0001](../../../experience/E0001-nix-untracked-flake.md),
[E0002](../../../experience/E0002-provider-closure-symbol-gates.md), and
[E0003](../../../experience/E0003-llvm-mlir-sdk-outputs-runpath.md).
