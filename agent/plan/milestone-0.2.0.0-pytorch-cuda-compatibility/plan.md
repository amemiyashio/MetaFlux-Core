---
id: milestone-0.2.0.0
delivery: 0.2.0.0
release: v0.2.0
status: Active
budgets: provisional
depends_on: [milestone-0.1.0.0, milestone-0.1.1.0, milestone-0.1.2.0, milestone-0.1.3.0]
areas: [compat.cuda, compiler, compiler.cpu, compiler.spirv, backend.cpu, backend.vulkan, compatibility]
updated: 2026-09-08
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
| Activation | The provider and five-stage probe exist, but the registered probe test uses a fake torch object. | First lane must provision the pinned stock client and exercise the stock daemon. |
| Implementation maturity | Provider-side cubin recognition and host-buffer tensor handlers demonstrate a prototype; deferred client cubins bypass daemon artifact registration and launch. | Move eager add through the neutral request into daemon CPU execution before broad surface or corpus work. |
| Release evidence | No checked-in real-client five-stage gate or daemon-backed operator corpus currently qualifies `v0.2.0`. | Treat earlier probe observations as diagnostic only and build release evidence in lane order. |

The readiness result is `not ready` for release and `ready for the first
vertical Iteration`: architecture owners exist, but activation and execution
evidence stop before the required daemon/backend path.

## Current Evidence

The repository contains the pinned baseline/frontier client manifest, a
five-stage probe, provider ABI and semantic tests, and a provider prototype that
parses client fatbins and recognizes selected PyTorch kernel names. The
prototype executes selected tensor operations in the application-side provider
by copying data to host buffers.

CTest currently runs the probe logic against a fake torch object; it does not
run pinned stock PyTorch or the claimed common-operator corpus. Deferred client
cubins bypass daemon artifact registration and module launch. Earlier manual
5/5 and 41/41 observations therefore establish prototype reach, not integrated
CPU-backend execution or milestone qualification.

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
baseline-required provider surface
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
| MLIR CUDA headline and compiler-first narrative | Rewrite | MLIR is internal mechanism, not the product objective. |
| Provider-local tensor results as qualifying evidence | Delete | They do not prove daemon submission or backend completion. |

Verification state: this decision is active as the epoch-0012 route. Technical
decisions listed below remain open until their own closure evidence exists.

## Scope

Included:

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
| [work-item-0.2.0.1](work/work-item-0.2.0.1-torch-client-bringup.md) | Active | Stock PyTorch CUDA five-stage baseline through daemon CPU execution |
| [work-item-0.2.0.2](work/work-item-0.2.0.2-torch-kernel-intake.md) | Queued | Complete CPU profile, neutral Kernel IR boundary, and versioned operator corpus |
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

1. Exact baseline-required CUDA Driver and profile-specific internal-table surface, with behavior provenance and stable unsupported-slot failures.
2. Minimal versioned ecosystem-neutral request schema and lifetime required for stock PyTorch eager add to become canonical Kernel IR.
3. Daemon CPU execution-mode surface and cache identity required for the stock PyTorch baseline.
4. Library-backed operator boundary, including whether matmul is supported without vendored cuBLAS execution.
5. Vulkan daemon routing shape and its qualification matrix.

Decisions 1-3 block integration of work-item-0.2.0.1. Decision 4 may wait until
work-item-0.2.0.2 but must close before its corpus freezes. Decision 5 may wait
until work-item-0.2.0.3 but must close before Vulkan route implementation.

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
