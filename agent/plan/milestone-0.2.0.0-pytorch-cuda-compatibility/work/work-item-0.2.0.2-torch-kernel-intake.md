---
id: work-item-0.2.0.2
delivery: 0.2.0.2
milestone: milestone-0.2.0.0
status: Active
area: compiler-cpu
depends_on: [work-item-0.2.0.1]
updated: 2026-09-08
---

# PyTorch CUDA CPU Profile

## Outcome

The proven stock-client vertical slice expands into a versioned CPU execution
profile: the required Driver/internal-table surface and handle semantics are
complete, neutral framework requests verify as canonical Kernel IR, and the
declared operator corpus executes through the backend-owned MLIR/LLVM CPU path
with stable cache and error behavior.

## Current Implementation

The first CPU-profile slice keeps the pinned stock PyTorch application and its
normal `torch.cuda` API unchanged while running eager `torch.add(int32)` through
all four daemon CPU execution modes. Interpreter retains canonical Kernel IR
without compiler/cache use; cold JIT compiles exactly once; warm JIT reuses that
identity with a lookup-only hit; and AOT first proves a stable unsupported miss,
then loads an administrator-prewarmed artifact without a runtime compiler
request. Cold JIT, warm JIT, and AOT bind to the same `mf-cache-v1` identity.

The gate continues to require the exact direct Driver/internal-table surface,
the v1 neutral request and Kernel IR v2 boundary, daemon module ownership,
bit-exact result bytes, and zero provider-local semantic events. This closes the
compiled-mode uncertainty for the baseline operation only. The complete surface
and handle matrices, generalized request, library boundary, and versioned
multi-operation corpus remain open below.

## Work

- [x] Generate one versioned surface matrix from pinned headers, provider
  exports, typed stubs, and profile-specific internal-table observations.
  Classify every entry as implemented, typed-stubbed, observed, or
  MetaFlux-strengthened and preserve stable failure for unsupported entries.
- [x] Add focused handle tests for repeated live lookup, invalid module,
  destroyed module, stale generation, cross-module name collision, duplicate
  teardown, and capacity reuse.
- [ ] Generalize the lane-1 request without adding CUDA, PyTorch, MLIR, LLVM, or
  target-specific types to the wire; validate each accepted request into
  versioned canonical Kernel IR in the daemon/compiler worker.
- [ ] Close the library-backed operator boundary, including matmul behavior when
  vendored cuBLAS execution remains excluded, before freezing the corpus.
- [ ] Publish a versioned real-client CPU corpus covering supported operation
  categories, exact inputs and outputs, module volume, repeated function
  resolution, cold/warm cache behavior, and classified unsupported operations.
- [ ] Lower verified Kernel IR through an explicitly legal backend-owned MLIR
  conversion to LLVM dialect, LLVM IR, validated PIC ELF, and CPU execution.
- [ ] Bind surface, client profile, request schema, Kernel IR, canonical
  pipeline, target triple, CPU features, FP policy, helper/backend ABI, compiler
  epoch, and content hash into cache identity.
- [ ] Prove every accepted corpus operation with correlated daemon submission
  and CPU-backend completion evidence.

## Exit Gate

The complete surface/status matrix and handle-negative suite pass; the neutral
request and Kernel IR contracts cover the frozen versioned corpus; the library
boundary is closed; provider-local tensor execution is absent; every accepted
operation lowers through the backend-owned MLIR/LLVM path and passes bit-exact
through the stock daemon and CPU backend; cold/warm and many-kernel cache tests
pass; and every unsupported operation returns its declared error without wrong
data.
