---
id: work-item-0.2.0.2
delivery: 0.2.0.2
milestone: milestone-0.2.0.0
status: Active
area: compiler-cpu
depends_on: [work-item-0.2.0.1]
updated: 2026-09-09
---

# PyTorch CUDA CPU Profile

## Outcome

The proven stock-client vertical slice expands into a versioned CPU execution
profile: the required Driver/internal-table surface and handle semantics are
complete, neutral framework requests verify as canonical Kernel IR, and the
declared operator corpus executes through the backend-owned MLIR/LLVM CPU path
with stable cache and error behavior.

## Current Implementation

The checked-in `frontier-not-frozen` corpus runs pinned stock PyTorch
`2.11.0+cu126` through its normal `torch.cuda` API. Its current interpreter
matrix contains 38 supported cases and one classified gap
(`sigmoid-f64`). Every supported case records its neutral request, daemon
module intake and CPU-backend completion, and every case rejects
provider-local semantic execution.

A compiled subset now covers 21 real-client cases across 20 unique
profile-owned PTX sources: integer and float arithmetic, negation, fill,
scalar and alpha arithmetic, integer and float absolute value, square root,
comparisons, ReLU, and two clamp inputs sharing one kernel. These sources live
beside the PyTorch CUDA CPU profile and are generated into the C17 provider at
configure time; the provider no longer owns a duplicate block of embedded PTX
definitions.

The compiled `abs.s32` exposed an incorrect signed-mask formula in both the
scalar and SIMD LLVM emitters. The lowering now implements
`(value xor sign) - sign`, and the independent interpreter/compiled corpus
checks both `abs(-3) == 3` and the PTX two's-complement `INT_MIN` result.

The many-kernel gate binds every result to an exact `mf-cache-v1` identity.
Cold JIT compiles 20 unique inputs, loads 21 modules, and records the expected
single same-source hit for the two clamp cases. Warm JIT seeds the same 20
identities and then loads all 21 modules without a compiler request. AOT first
proves 21 stable unsupported misses, prewarms the 20 unique inputs, and then
loads all 21 modules without runtime compilation. The complete
Driver/internal-table surface and handle-negative matrices remain qualified,
and decision-0053 remains the closed library boundary.

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
- [x] Close the library-backed operator boundary in decision-0053: pinned
  float32 SGEMM and single-batch cuBLASLt bias-linear calls translate to the
  neutral matmul request and execute only in the daemon; broader calls fail
  with typed statuses before submission.
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

## Remaining Work

The remaining 17 supported interpreter cases are reductions, cast, strided
copy, concat, the five library-backed matrix/linear cases, two softmax shapes,
and sigmoid. They already cross the neutral daemon boundary, but they still
use operation-specific daemon execution rather than the general
Kernel IR -> MLIR -> LLVM compiled path. The next slices must promote those
semantics into the compiled pipeline, extend stable negative coverage, and
freeze the corpus only after every accepted row passes interpreter, cold JIT,
warm JIT, and AOT with the same cache-identity contract.

## Exit Gate

The complete surface/status matrix and handle-negative suite pass; the neutral
request and Kernel IR contracts cover the frozen versioned corpus; the library
boundary is closed; provider-local tensor execution is absent; every accepted
operation lowers through the backend-owned MLIR/LLVM path and passes bit-exact
through the stock daemon and CPU backend; cold/warm and many-kernel cache tests
pass; and every unsupported operation returns its declared error without wrong
data.
