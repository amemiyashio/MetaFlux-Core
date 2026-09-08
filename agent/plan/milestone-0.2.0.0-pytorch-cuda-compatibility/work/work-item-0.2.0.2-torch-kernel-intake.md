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

Third operation (2026-09-08, continued): int32 subtract is live through
the same neutral request path. torch lowers `a - b` to the add kernel
with alpha = -1, so the add adapter now routes alpha 1 to the add PTX and
alpha -1 to a new sub_u32 PTX (OPERATION_ELEMENTWISE_SUB_I32_V1 = 3,
accepted by the wire validation alongside add and mul). A critical
launch-binding rule was decoded and fixed: the daemon binds every launch
to the most recently materialized kernel-request artifact for the
module, so an operation switch must re-register — the materializer now
tracks the last operation per module and re-registers on a switch
(replacing the module/artifact ids in place; superseded daemon objects
leak until context teardown). Verified bit-exact: add, sub, mul, and
re-switching add/sub/mul in one process all return exact results.

Remaining: float/i64 variants of add/sub/mul (each needs its own PTX and
dtype discrimination), gt/ge routing, and the torch.cuda._sleep bundled
smoke kernel (the probe's artifact-intake stage currently fails cleanly
on it — pre-existing on this architecture).

F32 variants and GPU backend slice plan (2026-09-09): float32 add and
mul joined the kernel-request path — OPERATION_ELEMENTWISE_ADD_F32_V1 = 4
and MUL_F32_V1 = 5, with add_f32/mul_f32 PTX artifacts (add.rn.f32 /
mul.rn.f32 over the same linear-index copy shape) and provider adapters
keyed on the mangled dtype suffix (CUDAFunctor_addIfE / MulFunctorIfE;
alpha 1.0 only for f32 add). Verified bit-exact: [2.0, -0.25, 2.125,
3.5] and [0.75, -4.5, -3.125, -2.0]. div stays a clean error (div.rn.f32
is not yet in the PTX frontend or the CPU interpreter opcode table).

GPU backend minimal vertical slice (plan, next front): register a
passthrough backend under plugins/backend/ per mf_backend_api_v1 that
forwards accepted Kernel Requests to the real GPU driver — the daemon
hands the Kernel Request PTX payload to cuModuleLoadData/cuLaunchKernel
on a real CUDA context (via a forwarding libcuda session) instead of the
CPU pipeline. Gate: with no GPU visible the backend returns its declared
NOT_SUPPORTED; with a GPU present the same stock-baseline suite must
pass bit-exact on the device. Decode prerequisites: the Kernel Request
artifact must carry the launch dimensions already present in the
argument block, and the passthrough layer needs the real-driver session
bootstrap (a pinned forwarding libcuda, mirroring toolchains' ICD
recipe). This is tracked here as the next decode front after the
multi-operation corpus freezes.

div.rn.f32 full stack (2026-09-09): div.rn.f32 joined the PTX frontend
form table (parser dispatch, manifest form id div-rn-f32 with spelling
div.rn.f32 and kir_op div_rn_f32), the Kernel IR opcode DivRnF32, the
CPU interpreter (div_rn IEEE division), and the CPU compiler's LLVM
emitters (fdiv in both the JIT and AOT pipelines); positive-fp-forms
gained the div fixture and the forms manifest grew to 32 entries with
the pinned counts and hash updated. ELEMENTWISE_DIV_F32_V1 = 6 joined
the kernel request operations, and the provider routes the torch
DivFunctorIfE binary shape through the daemon. The GPU passthrough
backend skeleton landed under plugins/backend/gpu: a
mf_backend_api_v1 backend whose driver probe dlopens the real driver
(METAFLUX_GPU_PASSTHROUGH_DRIVER, default libcuda.so.1), enumerates its
devices, loads the Kernel Request PTX via cuModuleLoadData, and
forwards launches via cuLaunchKernel; without a usable driver it
enumerates zero devices and stays inert. The component follows
backend-runtime rules (backend-plugin-api + dl only).

Verification: fdiv bit-exact ([3.0, -1.125, -3.125, -0.125]); ptx-parser,
both corpus differentials, stock-baseline four modes all pass; CTest
154/154.

neg-i32 adapter and remaining daemon_launch binding issue (2026-09-09):
the neg adapter (neg_kernel_cuda, two-pointer unary shape) correctly
matches, materializes OPERATION_ELEMENTWISE_NEG_I32_V1 = 7, and reaches
the daemon launch. The remaining "invalid argument" is in the
daemon_launch path after the goto — not in the adapter itself. Debug
traces confirm the adapter fires, element_count reads correctly, and
the materialize succeeds; the failure is in the argument block
validation, argument cache acquire, or the daemon-side execution. The
neg PTX was widened to four parameters (destination, input, unused,
count) to match the argument block entry count. Next: trace each
daemon_launch validation step for the neg launch to isolate the exact
rejecting check.

## Exit Gate

The complete surface/status matrix and handle-negative suite pass; the neutral
request and Kernel IR contracts cover the frozen versioned corpus; the library
boundary is closed; provider-local tensor execution is absent; every accepted
operation lowers through the backend-owned MLIR/LLVM path and passes bit-exact
through the stock daemon and CPU backend; cold/warm and many-kernel cache tests
pass; and every unsupported operation returns its declared error without wrong
data.
