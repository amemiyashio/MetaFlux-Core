---
id: milestone-0.2.0.0
delivery: 0.2.0.0
release: v0.2.0
status: Active
budgets: provisional
depends_on: [milestone-0.1.0.0, milestone-0.1.1.0, milestone-0.1.2.0, milestone-0.1.3.0]
areas: [compat.cuda, compiler, compiler.cpu, compiler.spirv, backend.cpu, backend.vulkan, compatibility]
updated: 2026-09-10
---

# milestone-0.2.0.0: PyTorch CUDA Transparent Compatibility Foundation

## Outcome

Make pinned stock PyTorch `2.11.0+cu126` use its normal `torch.cuda` path
through MetaFlux without modifying PyTorch source, wheel contents, or public
APIs. The first observable foundation is a checked-in five-stage real-client
gate whose eager add becomes a daemon submission and CPU-backend completion.
The route then broadens that foundation into a versioned CPU operator profile
and qualifies the same canonical Kernel IR corpus through Vulkan/SPIR-V.

Transparent use means stock source, wheel and application `torch.cuda` calls
remain unchanged after MetaFlux installation and process-level activation.
MetaFlux packaging, a launcher, and loader-environment activation are permitted.
The client retains its stock CUDA runtime; MetaFlux implements the reached
Driver/library compatibility surfaces and routes execution to its own backends.
CPU execution and physical AMD RADV/Vulkan execution require separate evidence.
MLIR remains an internal compiler-worker mechanism behind the neutral request
and Kernel IR boundaries; it is not the user-facing product objective.

This is a bounded compatibility profile, not a claim that every PyTorch
operation, CUDA library, cubin, or vendor kernel is supported. The milestone
keeps the `v0.2.0` delivery slot; native NixOS VM/package support remains
reserved for `v0.3.0`.

## Replan Readiness Basis

| Dimension | Current evidence | Route consequence |
| --- | --- | --- |
| Architecture | Provider, neutral protocol, daemon/compiler worker, canonical Kernel IR, CPU backend, and Vulkan backend owners exist. | Keep component boundaries and make stock PyTorch behavior the outer success signal. |
| Activation | Real-client runners provision the pinned wheel and activate the provider/daemon through test-owned loader and socket setup. | Preserve the stock profile; qualify a user-facing process entry in the Vulkan work item. |
| Implementation maturity | The baseline and a compiled CPU subset pass four execution modes. The versioned frontier corpus and surface/handle matrices exist; remaining positive rows use daemon-native CPU branches. | Finish the canonical Kernel IR/compiled CPU corpus before the dependent Vulkan lane; counts and remaining families live in work-item-0.2.0.2. |
| Release evidence | The CPU corpus is not frozen. The optional daemon Vulkan adapter has no qualified stock-client GPU path; standalone physical Vulkan fixtures do not supply that evidence. | Require physical AMD completion and lifecycle evidence for the same corpus, then qualify only explicitly declared application and release claims. |

The CPU-profile lane is active and ready for bounded semantic/compiled slices;
the stock baseline remains complete. GPU route architecture must close before
the qualifying adapter is implemented. Foundation completion and a stable
release remain pending their own gates; governance does not promote either.

## Current Evidence

The repository contains the pinned baseline/frontier client manifest, provider
ABI and semantic tests, and a checked-in five-stage stock-client gate. The gate
runs stock PyTorch `2.11.0+cu126`, records the exact direct provider/internal
table surface and neutral request, then verifies daemon-owned canonical Kernel
IR completion for eager int32 add through interpreter, cold JIT, warm JIT, and
AOT. The compiled modes require one shared versioned cache identity: cold JIT
compiles once, warm JIT performs a lookup-only hit after a cold seed, and AOT
first proves an unseeded stable unsupported result before the same PTX/KIR is
prewarmed and loaded without a runtime compiler request. The accepted path has
no provider-local tensor arithmetic or fabricated-success event.

The baseline gate deliberately rejects an unexpected internal-table surface,
unclassified reached slot, mismatched neutral request, or local semantic event.
The separate [CPU profile work item](work/work-item-0.2.0.2-torch-kernel-intake.md)
owns the current corpus summary and remaining execution work. Its versioned
surface matrix classifies implemented entries separately from typed stubs;
classification of the full pinned surface is not implementation of all CUDA.
The library boundary is closed by decision-0053, while corpus freeze is pending.
The [Vulkan work item](work/work-item-0.2.0.3-framework-qualification.md)
owns the unqualified adapter gaps and required real-device evidence. Standalone
GPU FMA, capability enumeration and green CPU tests do not qualify PyTorch GPU
execution. Recorded results apply only to their exact revision and tested tree.

## Evidence and Execution Boundary (decision-0044)

1. A baseline or corpus pass is acceptance evidence only when a checked-in gate
   runs the pinned real client against a stock daemon and records exact profile,
   source, tool, and backend identity. Fake probe self-tests validate probe
   control flow only.
2. The client route is CUDA/PyTorch profile input -> versioned
   ecosystem-neutral framework-kernel request -> verified canonical Kernel IR
   -> backend-owned lowering. PTX-bearing inputs continue through the PTX
   frontend; recognized cubin metadata may select a declared profile operation,
   but unrestricted SASS/cubin decoding is outside this milestone.
3. MLIR is an epoch-local compiler-worker implementation detail. It never
   crosses the provider, neutral protocol, durable Kernel IR, or backend C ABI.
   The CPU branch ends in LLVM IR/PIC ELF; the Vulkan branch lowers directly to
   target-constrained SPIR-V and never passes through LLVM IR.
4. The CUDA provider may present ABI and translate profile-specific client
   semantics, but tensor arithmetic and result materialization belong behind
   the neutral protocol in the daemon/backend execution path.
5. Reverse-engineered cudart internal tables are profile-specific observations,
   not CUDA Driver guarantees. Their entries require an explicit behavior
   classification and negative coverage.
6. Operator coverage is the exact checked-in corpus manifest, never an
   unqualified percentage or a claim of general PyTorch usability.

The accepted client routes have no provider-local tensor execution. Remaining
operation-specific daemon CPU branches are implementation evidence to converge
into canonical Kernel IR, not proof of generalized compiled or GPU execution.
Git retains discarded debugging chronology and unbound benchmark notes.

## Stock-PyTorch-First Sequencing (decision-0046)

decision-0046 makes the unmodified stock client the `v0.2.0` priority and
supersedes decision-0043's earlier client-contract/compiler-pipeline/framework-
qualification lane partition. The delivery coordinate and observable milestone
output remain stable, so milestone-0.2.0.0 and work-item-0.2.0.1 through
work-item-0.2.0.3 keep their IDs.

Rationale: the earlier ordering optimized internal compiler construction before
proving the shortest user-visible path. The repository already has enough
provider and backend structure to make one eager add cross the real boundary;
that vertical path exposes missing ABI, request, daemon, compiler, and cache
contracts with less speculative breadth.

The dependency DAG and critical path are:

```text
pytorch-v2.11.0 research readiness (satisfied for the completed baseline)
  -> baseline-required provider surface
  + minimal neutral request schema and lifetime
  + daemon CPU execution mode and cache identity
      -> work-item-0.2.0.1 stock five-stage baseline
          -> work-item-0.2.0.2 CPU profile and corpus
               + library-backed matmul boundary before corpus freeze
              -> work-item-0.2.0.3 Vulkan qualification
                   + Vulkan route decision before qualifying-route implementation
                       -> physical AMD add slice -> same corpus/lifecycle/activation
```

All three lanes are serial on this critical path. Bounded tests and contract
research inside a lane may run in parallel only when their outputs do not bypass
the lane's listed decisions or evidence prerequisites.

Current node dispositions are:

| Node | Disposition | Reason |
| --- | --- | --- |
| milestone-0.2.0.0 and three work-item IDs | Keep | Delivery coordinates and observable output are unchanged. |
| Pinned client surface, neutral request, Kernel IR, CPU, and Vulkan boundaries | Keep | decision-0044 remains the execution ownership authority. |
| Completed baseline and qualified surface/handle/library results | Keep | Reuse their bounded evidence; do not reopen completed deliveries. |
| Active CPU profile followed by queued Vulkan qualification | Keep | Canonical semantics precede reuse on the second backend. |
| Readiness, activation and GPU acceptance descriptions | Rewrite | Separate declared coverage, measured execution and broader application claims. |
| Duplicate counts and obsolete cross-milestone blocked state | Delete | The corpus/work-item and Goal own those facts. |

Verification state: this decision is retained by the active route.
decision-0047 adds only a research-readiness prerequisite before the first
lane; it does not change the product objective, work-item IDs, or lane order.
Technical decisions listed below remain open until their own closure evidence
exists.

## Scope

Included:

- Exact, on-demand PyTorch v2.11.0 reference-source materialization before
  first-lane implementation; the catalog and gitlink are research-only
  readiness inputs under decision-0047.
- Pinned stock PyTorch `2.11.0+cu126`, its normal `torch.cuda` API, and the
  exact five-stage gate: import, driver enumeration, runtime copy, artifact
  intake, and eager add.
- MetaFlux package, launcher, or loader-environment activation without PyTorch
  source, wheel, or API modification.
- The minimal Driver/internal-table surface needed by the baseline, followed by
  the complete pinned surface matrix and handle-negative profile.
- A versioned ecosystem-neutral request that the daemon/compiler worker
  validates into canonical Kernel IR.
- Daemon-routed CPU execution first and Vulkan execution second, with no
  provider-local tensor arithmetic or result materialization.
- A versioned CPU operator corpus, cache and stable error classification, then
  CPU/Vulkan differential evidence for the same corpus.
- Stream, event, allocator, synchronization, teardown, and daemon-loss behavior
  required by the pinned profile.
- Physical AMD RADV execution and a user-facing process activation path for the
  declared corpus. No per-kernel CPU fallback inside a Vulkan context.
- Release-facing baseline and frontier gap manifests.

Excluded:

- PyTorch source patches, wheel rewriting, or replacement `torch.cuda` APIs.
- Provider-local tensor arithmetic, fabricated daemon success, or result
  materialization as accepted execution.
- Vendor-private RM/UVM, NCCL, vendored cuBLAS/cuDNN execution, or unrestricted
  SASS/cubin execution.
- Physical dual-driver rows, which remain milestone-2.0.0.0 scope
  (decision-0040).
- Any relaxation of the frozen PTX 9.0/`sm_70` oracle without a compiler-epoch
  review.

Not yet declared or qualified: complete models, autograd/backward, optimizers,
mixed precision, general shape/broadcast/layout behavior, and
`torch.compile`/Triton execution. Their absence from the finite corpus is not a
test result for every possible call. The application-scope decision below must
close before adding such a claim. Pinned wheel dependencies do not themselves
establish support for their execution paths.

## Workstreams

| Workstream | Status | Deliverable |
| --- | --- | --- |
| [work-item-0.2.0.1](work/work-item-0.2.0.1-torch-client-bringup.md) | Complete | Stock PyTorch CUDA five-stage baseline through daemon CPU execution |
| [work-item-0.2.0.2](work/work-item-0.2.0.2-torch-kernel-intake.md) | Active | Complete CPU profile, neutral Kernel IR boundary, and versioned operator corpus |
| [work-item-0.2.0.3](work/work-item-0.2.0.3-framework-qualification.md) | Queued | PyTorch CUDA Vulkan and lifecycle qualification |

## Global Acceptance

- A checked-in gate runs pinned stock PyTorch `2.11.0+cu126` through all five
  baseline stages against a stock daemon with no PyTorch modification.
- Eager add and every accepted corpus operation produce daemon submissions and
  backend completions; the provider never computes or fabricates the result.
- The versioned corpus is bit-exact against the torch CPU reference through CPU
  and physical AMD Vulkan daemon routes; GPU records identify the actual device,
  enabled target, submissions and completions. CPU execution, software Vulkan
  or skipped hardware probes do not substitute for that GPU row.
- Backend selection is fixed before visible context resources succeed;
  unsupported GPU forms fail explicitly without per-kernel CPU substitution.
- The corpus passes through process-level activation and sustained execution;
  application claims outside it require separate declared scope and evidence.
- Provider surface, profile manifest, neutral request schema, Kernel IR schema,
  compiler pipeline, target environment, and cache identities advance together.
- Unsupported profile operations fail with stable classified errors and no
  wrong-data path.
- The cumulative milestone-0.1.x suites remain green with the framework client
  installed but idle.

## Decisions to Close

1. Vulkan daemon routing shape and its qualification matrix.
2. Exact application scope and acceptance for PyTorch CUDA claims beyond the finite corpus.

The routing decision closes before work-item-0.2.0.3 implements the qualifying
route, including reconciliation of the existing optional adapter. Its closure
must specify context/device selection, neutral lifetime, cache identity,
unsupported/loss behavior and a checked-in physical AMD qualification plan.
An opt-in prototype is evidence to evaluate, not an already closed decision.

Application scope closes before the first implementation or acceptance that
claims an application beyond the corpus; it does not block current bounded
CPU work or the first GPU slice. Pin the program/model and input/weight
identities, inference/backward/optimizer scope, dtype/shape/layout/batch,
framework compilation and library dependencies, correctness oracle, resource
limits and measured performance criteria. Do not invent a model selection.

## Resolved Decisions

### Neutral baseline kernel request and lifetime (decision-0048)

The first stock-PyTorch baseline uses protocol capability bit 11 and control
opcode 17, `KERNEL_REQUEST_REGISTER`, rather than the generic raw-artifact
registration path. The wire payload is a sealed, 64-byte v1 header followed by
source bytes. The completed baseline uses `ELEMENTWISE_ADD_I32` with
operation ABI v1, Kernel IR schema v2, and `MODULE_LOAD` lifetime. The current
closed operation set is owned by
[`protocol.h`](../../../contracts/protocol/client/v1/include/metaflux/client/protocol.h)
and its protocol tests; subsequent corpus rows do not create a private copy of
that enum in this plan. No CUDA,
PyTorch, MLIR, LLVM, Vulkan, target, or native-layout type crosses this wire
boundary.

The request artifact must remain live until `MODULE_LOAD` completes. Successful
module load parses and serializes the source into daemon-owned canonical Kernel
IR; the client may then release the artifact while the loaded module remains
valid until module unload. A runtime without the negotiated capability rejects
the session, malformed or raw-PTX request payloads are rejected, and an
artifact released before module load completes with the stable stale-handle
result.

Evidence is the encoded protocol contract and negative validation test in
`contracts/protocol/client/v1/`, the daemon lifecycle coverage in
`services/metafluxd/tests/client.c`, and the pinned stock PyTorch gate at
revision `f92304e55d9849de5c3694a4755dbcf22418b3e5`. That gate reports one
`baseline:elementwise-add-i32:1:2:module-load` request, daemon-owned canonical
module intake, CPU-backend completion, and no provider-local semantic event.

### Baseline Driver and internal-table surface (decision-0049)

The baseline ABI target remains the pinned R610 CUDA Driver 13.3 header
surface. The only profile-specific internal tables accepted for stock PyTorch
`2.11.0+cu126` are UUIDs `a094798c-2e74-2e74-93f2-0800200c0a66`,
`42d85a81-23f6-cb47-8298-f6e78a3aecdc`,
`c693336e-1121-df11-a8c3-68f355d89593`,
`263e8860-7cd2-6143-92f6-bbd5006dfa7e`,
and the remaining two UUIDs in the exact `BASELINE_INTERNAL_TABLES` set in the
stock gate. They are profile observations, not CUDA Driver API guarantees.

The c693 slot 0 status-success and slot 1 void-no-op behaviors are observed
from the pinned PyTorch `libcudart.so.12` image named in the provider source.
They are explicit MetaFlux-strengthened no-op behavior for this profile only.
Every unclassified reached table slot returns `CUDA_ERROR_NOT_SUPPORTED`; it
never returns a success value with unspecified caller-owned output. The
provider semantic test exercises unknown a094, c693, 42d8, and d408 slots, and
the real gate rejects unexpected table UUIDs, slot calls, or unclassified-slot
traces. The full table/symbol ABI remains governed by the frozen R610 header
manifest rather than these observations.

### CPU interpreter baseline and cache boundary (decision-0050)

The interpreter row of the stock five-stage baseline selects
`METAFLUX_CPU_EXECUTION_MODE=interpreter`.
`MODULE_LOAD` retains canonical Kernel IR and executes it in the daemon's CPU
interpreter. This mode never invokes the compiler worker, opens no mutable
or AOT compiled-artifact cache, and therefore has no compiled-artifact cache
identity. Its evidence reports those fields explicitly as
`not-applicable-in-interpreter-mode`, with zero compiler requests, cache hits,
and cache misses; the checked-in gate schema also binds the result to the exact
source revision and clean/dirty tree state.

This is a bounded baseline decision, not a cache-policy relaxation. Cold JIT,
warm JIT, and AOT retain their existing deterministic cache identities and
qualification rules. The checked-in baseline also qualifies eager add in these
compiled modes, and work-item-0.2.0.2 extends that evidence to its declared
compiled subset. Each additional row must bind compiler inputs, target/cache
identity and actual hit/miss evidence before joining that subset. Interpreter
non-use is never a substitute for compiled-mode cache identity.

### Automatic acceptance and advancement (decision-0052)

The [Agent execution authority](../../../docs/architecture/agent-execution.md#automatic-acceptance-and-advancement-decision-0052)
owns automatic acceptance, refined by decision-0054 for partial delivery and
evidence-bound recovery. Workflow governance does not broaden the PyTorch
compatibility claim. Decision-0053 below closes the bounded library interface;
Vulkan route qualification remains separate product work.

### Library-backed operator boundary (decision-0053)

Matmul is supported without vendored cuBLAS execution only through the exact
library calls reached by pinned stock PyTorch `2.11.0+cu126`. MetaFlux's C17
cuBLAS compatibility provider accepts host-pointer-mode float32
`cublasSgemm_v2` with `alpha=1`, `beta=0` or `beta=1`, valid column-major
leading dimensions, and `N`, `T`, or real-valued `C` transpose operands. This
covers the qualified `torch.matmul`, rectangular matmul, bias-free linear, and
`torch.addmm` cases. The cuBLASLt path is narrower: float32 compute and scale,
column-major `T/N` single-batch layouts with zero batch strides, `alpha=1`,
`beta=0`, identical C/D storage and layout, the bias epilogue, and the
provider-issued zero-workspace algorithm. It covers the qualified biased
linear case.

Both paths wrap the accepted call in the versioned neutral `MATMUL_F32`
request and invoke the Driver provider. The Driver validates device pointers,
matrix spans, launch geometry, and descriptor values before daemon submission;
the daemon-owned CPU matmul implementation alone reads inputs and materializes results. The library
provider performs no tensor arithmetic and does not load or execute an NVIDIA
cuBLAS implementation.

This is a pinned client profile, not a general cuBLAS compatibility claim.
Invalid handles and malformed objects return `CUBLAS_STATUS_NOT_INITIALIZED`
or `CUBLAS_STATUS_INVALID_VALUE`; valid but unqualified scalar modes, data
types, layouts, batching, algorithms, and epilogues return
`CUBLAS_STATUS_NOT_SUPPORTED` before submission. Any broader library-backed
operator must add an explicit compatibility surface, a versioned neutral
request/KIR operation, stable negative cases, and a real-client corpus row
before it is accepted.

Evidence is the provider semantic suite, including handle and negative-status
coverage, plus the checked-in interpreter corpus. Its five library-backed rows
produce bit-exact results for matmul, rectangular matmul, bias-free linear,
addmm, and biased linear; they record `sgemm-f32` or
`lt-matmul-bias-f32`, one daemon `matmul-f32` request, daemon CPU completion, and
zero provider-local execution. This closes the library boundary only; corpus
freeze, generalized lowering, compiled cache identity, and full CPU-profile
acceptance remain open in work-item-0.2.0.2.

### Evidence-bound transparent compatibility (decision-0055)

This governance decision refines decisions 0044/0046 without changing delivery
IDs, lane order or completed results. It narrows decision-0050's non-use claim
to interpreter mode and makes physical backend identity and process activation
explicit in the existing qualification deliverable.

Rationale: a finite client corpus, compilation counters, a standalone GPU
fixture and a working application establish different facts. A daemon can
compile a module and still execute an operation-specific CPU branch; a matching
tensor alone does not identify its executor.

Consequences:

- Manifest rows own declared coverage; the CPU work item owns the checked
  prose summary. Other indexes link to that owner instead of repeating counts.
- Full CPU acceptance requires per-request/module actual executor identity,
  canonical semantics and no daemon-native shortcut in generic interpreter or
  compiled modes. Existing
  result/cache counters remain useful bounded evidence, not this missing trace.
- Vulkan qualification requires physical AMD device identity and correlated
  submission/completion with fixed context backend and no CPU substitution.
  Optional adapters, model-only reports and skips never promote GPU maturity.
- Broader application scope remains an open decision until a program/model,
  execution modes and acceptance inputs are selected. The finite foundation
  does not promise universal PyTorch or joint CPU/GPU scheduling.
- Process activation for the declared corpus belongs to work-item-0.2.0.3;
  installed packages, upgrade/removal and released aliases belong to
  work-item-1.0.0.3. Physical Intel/NVIDIA/dual-driver gates remain v2 scope.

Verification state: this is an acceptance-policy cutover, not a product
implementation or a GPU qualification result. The readiness checker validates
declared counts and blocks completion inconsistent with an unfrozen corpus;
actual executor tracing and physical client gates remain product work.

## Definition of Done

milestone-0.2.0.0 is complete only when work-item-0.2.0.1 through
work-item-0.2.0.3 pass their Exit Gates from named revisions, the checked-in
stock-client and corpus gates pass through both daemon backends, unsupported
operations remain explicit, and the cumulative milestone-0.1.x regression is
green. Released artifacts follow the decision-0012 generic package policy.

## References

- [PyTorch compatibility roadmap](../pytorch-compatibility-roadmap.md)
- decision-0017 PTX 9.0/`sm_70` capability and semantic-oracle corpus
- decision-0040 physical-hardware boundary (milestone-2.0.0.0)
