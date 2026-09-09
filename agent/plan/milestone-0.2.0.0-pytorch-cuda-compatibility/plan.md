---
id: milestone-0.2.0.0
delivery: 0.2.0.0
release: v0.2.0
status: Active
budgets: provisional
depends_on: [milestone-0.1.0.0, milestone-0.1.1.0, milestone-0.1.2.0, milestone-0.1.3.0]
areas: [compat.cuda, compiler, compiler.cpu, compiler.spirv, backend.cpu, backend.vulkan, compatibility]
updated: 2026-09-09
---

# milestone-0.2.0.0: PyTorch CUDA Transparent Compatibility Foundation

## Outcome

Make pinned stock PyTorch `2.11.0+cu126` use its normal `torch.cuda` path
through MetaFlux without modifying PyTorch source, wheel contents, or public
APIs. The first observable foundation is a checked-in five-stage real-client
gate whose eager add becomes a daemon submission and CPU-backend completion.
The route then broadens that foundation into a versioned CPU operator profile
and qualifies the same canonical Kernel IR corpus through Vulkan/SPIR-V.

MetaFlux packaging, a launcher, and loader-environment activation are permitted.
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
| Activation | The checked-in gate provisions pinned stock PyTorch `2.11.0+cu126` and reaches all five stages against the stock daemon. | Preserve this exact profile as the CPU-profile and Vulkan qualification input. |
| Implementation maturity | The eager int32 add crosses a capability-gated neutral request into daemon-owned Kernel IR and passes through interpreter, MLIR/LLVM cold JIT, warm JIT, and administrator-prewarmed AOT. Deferred profile operations return classified unsupported errors rather than bypassing the daemon. | Broaden only through the versioned corpus and its daemon-owned routes. |
| Release evidence | One real-client operation has passed all four CPU execution modes with exact compiler/cache counters, stable cache identity, and an explicit AOT-miss error, but no versioned CPU corpus, complete surface/handle matrix, Vulkan differential gate, or current gap manifests qualify `v0.2.0`. | Treat this as the first CPU-profile slice, not a release. |

The readiness result is `not ready` for release and `ready for the CPU-profile
lane`: its three baseline decisions have real evidence and the baseline is
integrated, while the broad corpus and Vulkan work remain in the following
lanes.

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

The same gate deliberately rejects an unexpected internal-table surface,
unclassified reached slot, mismatched neutral request, or local semantic event.
It is limited to the named profile and one operation: no common-operator corpus,
complete surface/handle profile, or Vulkan route is yet qualified. That
distinction keeps the accepted slice from overstating milestone completion.

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

The provider-local semantic router remains implementation evidence to migrate,
not a release contract or a qualified backend. Git retains discarded debugging
chronology and unbound benchmark notes.

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
pytorch-v2.11.0 exact reference gitlink readiness
  -> baseline-required provider surface
  + minimal neutral request schema and lifetime
  + daemon CPU execution mode and cache identity
      -> work-item-0.2.0.1 stock five-stage baseline
          -> work-item-0.2.0.2 CPU profile and corpus
               + library-backed matmul boundary before corpus freeze
              -> work-item-0.2.0.3 Vulkan qualification
                   + Vulkan route decision before implementation
```

All three lanes are serial on this critical path. Bounded tests and contract
research inside a lane may run in parallel only when their outputs do not bypass
the lane's listed decisions or evidence prerequisites.

Node disposition from epoch-0011 is:

| Node | Disposition | Reason |
| --- | --- | --- |
| milestone-0.2.0.0 and three work-item IDs | Keep | Delivery coordinates and observable output are unchanged. |
| Pinned client surface, neutral request, Kernel IR, CPU, and Vulkan boundaries | Keep | decision-0044 remains the execution ownership authority. |
| Full surface matrix and handle-negative expansion | Reorder | Move after the minimal stock-client vertical baseline. |
| Eager add and minimal CPU execution | Reorder | Pull into the first lane as the earliest user-visible success. |
| Compiler-first headline and narrative | Rewrite | MLIR is internal mechanism, not the product objective. |
| Provider-local tensor results as qualifying evidence | Delete | They do not prove daemon submission or backend completion. |

Verification state: this decision is retained by the epoch-0015 route.
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
  and Vulkan daemon routes.
- Provider surface, profile manifest, neutral request schema, Kernel IR schema,
  compiler pipeline, target environment, and cache identities advance together.
- Unsupported profile operations fail with stable classified errors and no
  wrong-data path.
- The cumulative milestone-0.1.x suites remain green with the framework client
  installed but idle.

## Decisions to Close

1. Vulkan daemon routing shape and its qualification matrix.

This decision may wait until work-item-0.2.0.3 but must close before Vulkan
route implementation.

## Resolved Decisions

### Neutral baseline kernel request and lifetime (decision-0048)

The first stock-PyTorch baseline uses protocol capability bit 11 and control
opcode 17, `KERNEL_REQUEST_REGISTER`, rather than the generic raw-artifact
registration path. The wire payload is a sealed, 64-byte v1 header followed by
source bytes. Its only accepted baseline profile is `ELEMENTWISE_ADD_I32` with
operation ABI v1, Kernel IR schema v2, and `MODULE_LOAD` lifetime. No CUDA,
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

The stock five-stage baseline selects `METAFLUX_CPU_EXECUTION_MODE=interpreter`.
`MODULE_LOAD` retains canonical Kernel IR and executes it in the daemon's CPU
interpreter. This profile never invokes the compiler worker, opens no mutable
or AOT compiled-artifact cache, and therefore has no compiled-artifact cache
identity. Its evidence reports those fields explicitly as
`not-applicable-in-interpreter-mode`, with zero compiler requests, cache hits,
and cache misses; the checked-in gate schema also binds the result to the exact
source revision and clean/dirty tree state.

This is a bounded baseline decision, not a cache-policy relaxation. Cold JIT,
warm JIT, and AOT retain their existing deterministic cache identities and
qualification rules. Any future stock-PyTorch compiled path must promote its
compiler inputs, target/cache identity, and cache hit/miss evidence in
work-item-0.2.0.2 before it joins the versioned CPU corpus.

### Automatic acceptance and advancement (decision-0052)

The controlling parent automatically invokes `accept-and-advance` after a
dependency-ready worker emits a non-empty committed base/tip range, passing
focused-test evidence, and no blockers. A second user message is not a
prerequisite. The controller rejects mismatched or stale history, composes
explicit-only `integrate-batch` for merge and combined verification, invokes
`roast`, advances Goal/work-item state, and pushes only the resulting acceptance
commit.

Goal schema v3 binds every lane directly to its work item. On acceptance the
controller completes the current lane/work item, activates and targets the
first dependency-ready planned lane in Iteration order, or closes the Batch.
Current-HEAD candidates retain linear history; divergent candidates use a
prepared non-fast-forward merge; older ancestors remain stale. State validation
is transactional for controller-owned writes, and committed replay is a no-op.

Evidence is the controller behavior test, bilingual routing corpus, schema-v3
state checker, and the epoch-0015 full regression. Decision-0052 replaces the
policy-only automatic trigger in decision-0051 without broadening the PyTorch
compatibility claim; decision-0053 below subsequently closes the library
boundary while Vulkan routing remains open.

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
the daemon CPU backend alone reads inputs and materializes results. The library
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
`lt-matmul-bias-f32`, one daemon `matmul-f32` request, backend completion, and
zero provider-local execution. This closes the library boundary only; corpus
freeze, generalized lowering, compiled cache identity, and full CPU-profile
acceptance remain open in work-item-0.2.0.2.

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
