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

Compatibility harness daemons bind their Unix sockets inside the harness
scratch directory. `sun_path` holds only 108 bytes, and nested `nix develop`
TMPDIR hierarchies push TMPDIR-scoped socket paths past that limit: at three
levels of nesting the stock-baseline and concat daemons failed listener setup
and exited 1 before socket creation (a proven 131-byte socket path reproduces
the failure) while the frontier suite, whose socket directory was already
`/tmp`-anchored, passed. Every compatibility harness therefore anchors its
scratch directory at `/tmp`, which keeps socket paths short at any nesting
depth.

The sum-i64 case (torch int32 sum promoted to its int64 accumulator output)
now executes canonically: the provider normalizes the ReduceOpIl config
(int64-accumulator functor reads `num_inputs` at byte 20), binds the
`reduce-sum-i64.ptx` profile artifact, and the daemon runs it through the
generic executor in interpreter mode. The daemon silently returns
`MF_SHARED_MALFORMED` from the module-load path when profile PTX violates the
Kernel IR v2 dialect: unsupported forms such as `mov.u64`, `add.u64` with a
32-bit operand, and register reuse (the IR is single-assignment) all fail
`compiler::ptx::parse` before module preparation, with no diagnostic. The
sum-i64 artifact sums the six 8-byte source elements from their low words:
a wrapping u32 sum plus a wrap-carry count minus a negative-element count
yields the exact high word for any sign mix.

sum-i64 is now a compiled-subset row. Its application issues two baseline
requests — the int32-to-int64 cast-copy companion and the reduction — and the
corpus compiled declaration therefore carries a source list: one source per
baseline request. The `cast-copy-i64.ptx` artifact widens each loaded int32
word to a sign-extended int64 pair with dialect-legal forms (`sub.u32`
produces the 0xFFFFFFFF high half), and the daemon excludes the cast-copy
operation from its native branch so both requests execute canonically with
per-request executor evidence in every mode. The compiled runner resolves a
case's sources from either a string or a list, requires no more sources than
requests, and the AOT-miss check accepts a non-empty prefix of the expected
request and library-call sequences because a multi-request application aborts
at its first failed module load; the miss-run cache-miss expectation is the
attempted module-load count derived from that observed prefix evidence.

The arange-i64 case is likewise a compiled-subset row. torch emits one
`elementwise_kernel_with_index` request carrying the element count and an
int64 {start, step} functor, and the artifact evaluates the progression for
the pinned zero-start unit-step shape: each thread stores its global index
sign-extended into the destination int64 element. The Kernel IR dialect has
no 64-bit multiply form, so the provider rejects other start or step values
for the pinned artifact, mirroring the reduction extent gates; generalizing
the progression needs a dialect extension slice.

The two softmax cases joined the compiled subset together with the dialect
form they need: the Kernel IR had no float select, so `selp.f32` (f32, f32,
pred) is now an advertised form across the parser, serializer, interpreter
and CPU compiler with its sealed manifest fixture. Both artifacts pin the
observed shapes (two-by-three row softmax and two-by-three-by-two dimension
softmax) because the dialect cannot divide general thread indices; the
provider rejects other shape parameters, and each thread re-evaluates the
full three-element slice maximum, exponential sum in index order and its own
guarded quotient, reproducing the daemon-native arithmetic bit for bit.

| Corpus metric | Count |
| --- | --- |
| Supported cases | 41 |
| Compiled cases | 41 |
| Unique compiled PTX sources | 41 |
| Cases outside compiled subset | 0 |
| Classified gaps | 2 |

Every supported interpreter-mode case records its neutral request, daemon
module intake and CPU completion, and rejects provider-local execution.
Interpreter-mode success includes operation-specific daemon CPU execution;
it does not prove that every row has generic Kernel IR interpreter semantics.
The corpus remains `frontier-not-frozen`, with `exit_gate_complete: false`.

The compiled subset includes arithmetic, fill, scalar/alpha operations,
absolute value, square root, exponential, sigmoid, comparisons, int32-to-float32
cast, ReLU, clamp, the pinned six-element int32 sum, signed int32 clamp-min,
signed int32 max/min reductions, the pinned strided contiguous copy, the
four-element float32 sum/mean reductions, and the pinned 2x2, 2x3-by-3x4
matmul, 2x3-by-3x2 transposed-weight linear, 2x2 beta=1 addmm, and 2x3-by-3x2
transposed-weight bias-linear float32 shapes.
The two float32 clamp inputs share one kernel. Profile-owned PTX
lives beside the PyTorch CPU profile and is generated into the C17 provider at
configure time.

The matmul profile promotes the observed stock cuBLAS SGEMM request for a
2x2, a 2x3-by-3x4 non-transposed shape, a 2x3-by-3x2 transposed-weight linear
shape, and the fixed beta=1 addmm shape into four fixed unrolled Kernel IR
artifacts. The cuBLASLt bias-linear request adds a fifth artifact with an
explicit bias buffer and epilogue identity. The provider keeps the neutral
request and materializes the exact shape/variant; a warm launch does no
registration work when its module/operation/variant cache identity matches,
including distinct identities for addmm and bias-linear. Other shapes,
transposes, beta values and bias epilogues remain on the closed library-backed
boundary. The artifacts use the same column-major request contract as the
existing cuBLAS adapter, emit one daemon submission, and are exercised by the
generic interpreter, cold JIT, warm JIT and AOT executors.
This is a fixed-shape CPU slice and does not claim general GEMM or Vulkan
execution.

The provider's warm materialization identity is now observable without adding
work to the normal path: when diagnostic tracing is enabled, a repeated launch
emits `MF_PYTORCH_BASELINE_WARM_HIT` only after the module's operation and
variant identities match. The real-client frontier gate binds that count to
`launches - module_loads` for every supported row, so a second registration
cannot be hidden behind a successful result. Tracing is disabled by default;
the identity check remains the existing early-return path and performs no
daemon registration or tensor execution.

The concat-u32 slice adds the pinned two-source contiguous `torch.cat` row.
The provider decodes the source metadata and submits destination, two source
buffers, lengths, and total count as a neutral six-parameter request. The
daemon's operation-specific concat branch is bypassed; a dedicated unrolled
PTX artifact copies the twelve words through the generic CPU interpreter or
compiled executor. This is a fixed two-input, six-element CPU claim and does
not generalize variable arity, non-contiguous inputs, or Vulkan execution.
Provider admission requires exactly two sources with six elements each before
module materialization. Other lengths or source counts receive
`CUDA_ERROR_NOT_SUPPORTED` as a MetaFlux profile limit. The standalone stock
concat gate checks int32 and float32 results, repeated launches, and rejection
of `4+4`, unequal lengths, an invalid second-source length, and three sources
without module registration or daemon tensor execution.

The strided-contiguous-copy-u32 slice promotes the pinned 4x6 int32 transpose
materialization into a compiled profile. The provider carries the decoded
two-dimensional size and byte-stride descriptor through the neutral request,
but admits only the observed 4x6 source iteration layout (`sizes=4,6`, output
byte strides `4,16`, input byte strides `24,4`, 24 elements). The profile artifact performs the
transpose through the compiled entry; other layouts are rejected before module
materialization rather than receiving fixed-shape data. The daemon has no
operation-specific strided-copy tensor branch for this row.

The exponential slice carries actual `ExpF32` semantics through canonical
Kernel IR, the compiler-worker protocol and the generic CPU executors. Its
finite-result accuracy bound is 2 ULP against an independent mathematical
reference; it makes no correctly-rounded claim. The edge fixture supplies 23
independent references covering signed zero, subnormals, overflow, underflow,
infinities and NaN. Interpreter, cold JIT, warm JIT and AOT agree bit-for-bit
under one bound CPU math-helper identity. The helper is scalar; this slice
establishes no SIMD or throughput improvement.

The sigmoid-f32 slice carries the pinned stock client's `torch.sigmoid` result
through profile-owned PTX composing the existing subtraction, exponential,
addition and division Kernel IR operations. Checked-in expected bit outputs
for the pinned finite cases agree across interpreter, cold JIT, warm JIT and AOT;
the provider only normalizes the two buffer arguments and typed zero/one
scalars, submits the request, and records no provider-local tensor execution.
Cold JIT has one compiler miss, warm JIT has one cache hit with no compiler
request, and AOT prewarms the same cache identity while a separate unsupported
probe remains a stable miss. This is a CPU-profile slice and does not qualify
physical AMD GPU execution or the complete corpus.

The float32 reduction slice adds the pinned four-element `torch.sum` and
`torch.mean` rows to the compiled subset. The provider keeps the neutral
destination/source/count request and rejects other reduction extents until
Kernel IR v2 has a bounded loop form. The daemon bypasses its operation-specific
float32 reduction branch for these two operation IDs, so the checked-in
unrolled PTX is parsed, verified, and executed by the generic CPU interpreter
or compiled CPU artifact. The store is predicated to one logical lane and the
mean divisor is carried as a neutral u32 scalar. This slice establishes no
general reduction-shape or Vulkan qualification claim.

The integer reduction slice adds the pinned six-element
`torch.sum(i32, dtype=int32)` row. The provider accepts only that source
extent, rejects other extents before module materialization, and sends a
neutral destination/source/unused/count request to the daemon. Operation 19
now bypasses the daemon-native reduction branch; its unrolled `u32` bit-sum is
parsed, verified, and executed through the generic CPU interpreter, JIT, or AOT
artifact. Other integer reductions remain outside this compiled claim, and this
slice does not freeze a general reduction corpus or qualify Vulkan execution.

The signed int32 clamp-min slice adds the pinned `torch.relu` row. The provider
normalizes its destination, source, neutral scalar, element count, and signed
floor into a versioned request, and operation 35 bypasses the daemon's native
clamp branch. Its dedicated PTX uses a signed compare and select, is parsed and
verified as canonical Kernel IR, and executes through the generic CPU
interpreter, JIT, or AOT artifact. This remains a six-element pinned shape
claim and does not generalize clamp coverage or qualify Vulkan execution.

The signed int32 max/min slice adds the pinned six-element `torch.max(i32)`
and `torch.min(i32)` rows. The provider accepts only that source extent with
one destination element, selects dedicated signed compare/select PTX, and
operation IDs 22 and 23 bypass the daemon-native reduction branch. Each
artifact is parsed and verified as canonical Kernel IR and executes through
the generic CPU interpreter, JIT, or AOT artifact with exact results `100`
and `-999`; this remains a fixed-shape CPU claim and does not generalize
reduction coverage or qualify Vulkan execution.

Compiled entry helper ABI v3 passes a runtime-bound math function pointer.
The actual implementation identity participates in both the cache fingerprint
and the checked object identity, while generated objects retain strict
undefined-symbol and dependency rejection. Helper binding and identity hashing
occur during preparation/loading, with no added warm-launch lookup or hashing.
The real-client gate enables `METAFLUX_TRACE_EXECUTION=1` and correlates client
PID, session, request, module generation, protocol operation and actual executor
after successful generic CPU execution. Compiled rows reject missing, duplicate,
wrong-operation or wrong-mode completion records. This optional qualification trace is disabled normally;
daemon-native branches supply no generic-executor completion evidence.

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
  neutral matmul request; the observed 2x2 and 2x3-by-3x4 SGEMM rows now use
  the canonical compiled profiles while broader calls remain library-backed
  or fail with typed statuses before submission.
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

The fixed matrix/linear rows are now all represented by canonical Kernel IR
artifacts; there are no supported rows outside the compiled subset. The two
remaining classified gaps are the explicit float64 sigmoid and exponential
probes, which still return stable unsupported errors. Broader shapes,
transposes, beta values and bias configurations remain outside this finite
profile and require their own positive and negative evidence.

For each bounded slice, select an explicit remaining operation family, define
its dtype/shape/layout and error boundary, review neutral request lifetime and
Kernel IR meaning, then prove all four CPU modes against an independent oracle.
Keep the library boundary in decision-0053; close each broader configuration
with positive and negative evidence before including it. Freeze only when all
accepted rows satisfy the same semantic and cache-identity contracts.

`sigmoid-f64` and `exp-f64` are the classified gaps inside this finite corpus.
The exponential adapter recognizes only the pinned float32 unary signature;
float64 is rejected before a daemon module or request is materialized. This is
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
