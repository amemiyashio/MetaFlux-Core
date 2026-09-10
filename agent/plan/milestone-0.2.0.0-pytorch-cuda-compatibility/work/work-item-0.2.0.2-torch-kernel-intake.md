---
id: work-item-0.2.0.2
delivery: 0.2.0.2
milestone: milestone-0.2.0.0
status: Active
area: compiler-cpu
depends_on: [work-item-0.2.0.1]
updated: 2026-09-11
---

# PyTorch CUDA CPU Profile

## Outcome

The proven stock-client vertical slice expands into a versioned CPU execution
profile: the required Driver/internal-table surface and handle semantics are
complete, neutral framework requests verify as canonical Kernel IR, and the
declared operator corpus executes through the backend-owned MLIR/LLVM CPU path
with stable cache and error behavior.

## Current Implementation

The checked-in [frontier corpus](../../../../tests/compatibility/pytorch_cuda_cpu_frontier_corpus_v1.json)
runs pinned stock PyTorch `2.11.0+cu126` through its normal `torch.cuda` API.
This is the sole current prose summary of its counts; the manifest owns the
rows. `tools/check-pytorch-cuda-readiness.py` derives and checks this table.
Counts describe declared coverage, not a percentage of PyTorch support or a
fresh run. Actual results remain bound to the revision, tree and invocation
reported by the real-client gate.

The baseline, frontier and concat harnesses select the lowest effective CPU
before starting their stock client processes, report that placement and restore
the caller's affinity on exit. This bounds the test environment after an
observed stock-client import-time `ApproximateClock` non-monotonic counter
assertion; it neither retries a failed import nor qualifies unrestricted
multi-CPU placement. The observed assertion alone does not identify its
hardware cause.

| Corpus metric | Count |
| --- | --- |
| Supported cases | 41 |
| Compiled cases | 22 |
| Unique compiled PTX sources | 21 |
| Cases outside compiled subset | 19 |
| Classified gaps | 1 |

Every supported interpreter-mode case records its neutral request, daemon
module intake and CPU completion, and rejects provider-local execution.
Interpreter-mode success includes operation-specific daemon CPU execution;
it does not prove that every row has generic Kernel IR interpreter semantics.
The corpus remains `frontier-not-frozen`, with `exit_gate_complete: false`.

The compiled subset includes arithmetic, fill, scalar/alpha operations,
absolute value, square root, comparisons, int32-to-float32 cast, ReLU and clamp.
The two clamp inputs share one kernel. Profile-owned PTX lives beside the
PyTorch CPU profile and is generated into the C17 provider at configure time.

The compiled `abs.s32` exposed an incorrect signed-mask formula in both the
scalar and SIMD LLVM emitters. The lowering now implements
`(value xor sign) - sign`, and the independent interpreter/compiled corpus
checks both `abs(-3) == 3` and the PTX two's-complement `INT_MIN` result.

The many-kernel gate binds every result to an exact `mf-cache-v1` identity.
Cold JIT compiles the unique inputs, loads every compiled case, and records the
expected same-source hit for the two clamp cases. Warm JIT seeds those
identities and then loads every compiled case without a compiler request. AOT
first proves stable unsupported misses, prewarms the unique inputs, and then
loads every compiled case without runtime compilation. The complete
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
- [ ] Freeze and qualify the existing versioned real-client CPU corpus covering supported operation
  categories, exact inputs and outputs, module volume, repeated function
  resolution, cold/warm cache behavior, and classified unsupported operations.
- [ ] Lower verified Kernel IR through an explicitly legal backend-owned MLIR
  conversion to LLVM dialect, LLVM IR, validated PIC ELF, and CPU execution.
- [ ] Bind surface, client profile, request schema, Kernel IR, canonical
  pipeline, target triple, CPU features, FP policy, helper/backend ABI, compiler
  epoch, and content hash into cache identity.
- [ ] Prove every accepted corpus operation with correlated request/module,
  daemon submission and actual CPU executor identity. Compiled rows must execute
  their tensor semantics through the loaded compiled entry, with no
  operation-specific daemon-native shortcut; compilation counters alone do not
  prove that execution took place.

## Remaining Work

Rows outside the manifest's compiled subset cover reductions, strided copy,
concat, arange, exponential, int32 minimum clamping, library-backed
matrix/linear, softmax and sigmoid. They cross the
neutral daemon boundary but use operation-specific daemon CPU branches.
Promote their actual semantics into canonical Kernel IR and the compiled
pipeline; a copy-shaped placeholder associated with a native operation is not
that operation's semantic implementation. Cast is already in the compiled
subset and is not remaining work.

For each bounded slice, select an explicit remaining operation family, define
its dtype/shape/layout and error boundary, review neutral request lifetime and
Kernel IR meaning, then prove all four CPU modes against an independent oracle.
Keep the library boundary in decision-0053; close each broader configuration
with positive and negative evidence before including it. Freeze only when all
accepted rows satisfy the same semantic and cache-identity contracts.

`sigmoid-f64` is the current classified gap inside this finite corpus. It is
not an inventory of every unsupported PyTorch operation. FP16/BF16, broader
shapes/layouts, autograd/backward, optimizers, complete models and
`torch.compile`/Triton have no qualification claim here. Application selection
and its closure timing belong to decision-0055 in the milestone plan.

## Exit Gate

The complete surface/status matrix and handle-negative suite pass; the neutral
request and Kernel IR contracts cover the frozen versioned corpus; the library
boundary is closed; provider-local tensor execution is absent; every accepted
operation has canonical semantics, lowers through the backend-owned MLIR/LLVM
path and passes bit-exact through the stock daemon and CPU backend with
per-request/module actual executor identity and no daemon-native tensor
shortcut in the generic interpreter or compiled modes; cold/warm and many-kernel cache tests
pass; and every unsupported operation returns its declared error without wrong
data.
