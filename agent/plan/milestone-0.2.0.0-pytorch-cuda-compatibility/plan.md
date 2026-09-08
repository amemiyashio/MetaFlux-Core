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

# milestone-0.2.0.0: MLIR CUDA and PyTorch CUDA Foundation

## Outcome

Make the pinned PyTorch CUDA baseline usable for an explicit, versioned
operation corpus through a clear MLIR CUDA route. The CUDA compatibility
provider presents the client surface and translates recognized profile input
into an ecosystem-neutral request. The daemon/compiler worker validates that
request into canonical Kernel IR; backend-owned MLIR pipelines lower the same
semantics to CPU/LLVM first and Vulkan/SPIR-V second.

This is a bounded compatibility profile, not a claim that every PyTorch
operation, CUDA library, cubin, or vendor kernel is supported. The milestone
occupies the `v0.2.0` delivery slot; native NixOS VM/package support remains
reserved for `v0.3.0`.

## Current Evidence

The repository contains the pinned baseline/frontier client manifest, a
five-stage probe, provider ABI and semantics tests, and a provider prototype
that parses client fatbins and recognizes selected PyTorch kernel names. The
prototype executes selected tensor operations in the application-side provider
by copying data to host buffers.

CTest currently runs the probe logic against a fake torch object; it does not
run the pinned real client or the claimed common-operator corpus. Deferred
client cubins also bypass daemon artifact registration and module launch.
Therefore earlier manual 5/5 and 41/41 observations establish a useful
prototype, but not integrated CPU-backend execution or milestone qualification.

## Evidence and Execution Boundary (decision-0044)

decision-0044 makes source/test truth explicit after the prototype work crossed
work-item and component boundaries:

1. A baseline or corpus pass is acceptance evidence only when a checked-in gate
   runs the pinned real client against a stock daemon and records exact profile,
   source, tool, and backend identity. Fake probe self-tests validate probe
   control flow only.
2. The selected route is CUDA/PyTorch client profile -> versioned
   ecosystem-neutral framework-kernel request -> verified canonical Kernel IR
   -> backend-owned MLIR lowering. PTX-bearing inputs continue through the PTX
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
not a release contract or a qualified backend. Git retains the discarded
debugging chronology and unbound benchmark notes.

## Foundation-First Sequencing (decision-0043)

Work proceeds through three ordered boundaries:

1. close the pinned CUDA/PyTorch client contract and its real-client evidence;
2. establish the neutral framework request, canonical Kernel IR boundary, and
   backend-owned MLIR-to-CPU execution path;
3. qualify PyTorch CUDA lifecycle semantics and route the same Kernel IR corpus
   through the independent Vulkan/SPIR-V branch.

milestone-1.0.0.0 remains queued until all three boundaries pass.

## Scope

Included:

- The exact Driver API and profile-specific internal surface required by the
  pinned cudart/ATen client, with normative, observed, and
  MetaFlux-strengthened behavior kept distinct.
- Staged checked-in real-client gates: import, driver enumeration, and runtime
  copy close the client contract; artifact intake and eager add close with
  neutral CPU execution.
- A versioned, ecosystem-neutral framework-kernel request that the
  daemon/compiler worker validates into canonical Kernel IR.
- Backend-owned MLIR conversion with explicit legality: CPU lowers through the
  LLVM dialect to PIC ELF, while Vulkan lowers directly to validated SPIR-V.
- Daemon-routed CPU and Vulkan execution with bit-exact differential evidence.
- Framework stream, event, allocator, synchronization, teardown, and daemon-loss
  behavior required by the pinned profile.
- Release-facing baseline and frontier gap manifests.

Excluded:

- Provider-local tensor arithmetic as an accepted execution backend.
- Vendor-private RM/UVM, NCCL, vendored cuBLAS/cuDNN execution, or unrestricted
  SASS/cubin execution.
- Physical dual-driver rows, which remain milestone-2.0.0.0 scope
  (decision-0040).
- Any relaxation of the frozen PTX 9.0/`sm_70` oracle without a compiler-epoch
  review.

## Workstreams

| Workstream | Status | Deliverable |
| --- | --- | --- |
| [work-item-0.2.0.1](work/work-item-0.2.0.1-torch-client-bringup.md) | Active | Pinned client contract and real-client gate |
| [work-item-0.2.0.2](work/work-item-0.2.0.2-torch-kernel-intake.md) | Active | Kernel IR and MLIR CPU pipeline |
| [work-item-0.2.0.3](work/work-item-0.2.0.3-framework-qualification.md) | Draft | PyTorch CUDA multi-backend qualification |

## Global Acceptance

- A checked-in gate runs the pinned real client through all five baseline stages
  against a stock daemon with no client patch.
- The versioned eager corpus is bit-exact against the torch CPU reference
  through actual daemon submissions on CPU and Vulkan.
- Provider surface, profile manifest, neutral request schema, Kernel IR schema,
  compiler pipeline, target environment, and cache identities advance together.
- Unsupported profile operations fail with stable classified errors and no
  wrong-data path.
- The cumulative milestone-0.1.x suites remain green with the framework client
  installed but idle.

## Decisions to Close

1. Exact versioned ecosystem-neutral framework-kernel request schema and
   lifetime used to translate the pinned client profile into canonical Kernel
   IR without CUDA, PyTorch, MLIR, or target-specific types crossing the wire.
2. Exact provider Driver API and profile-specific internal-table surface required
   by pinned cudart/ATen, with behavior provenance.
3. Library-backed operator boundary, including whether matmul is supported
   without vendored cuBLAS execution.
4. Daemon execution-mode surface for framework clients and its cache identity.
5. Vulkan daemon routing shape and its qualification matrix.

## Definition of Done

milestone-0.2.0.0 is complete only when work-item-0.2.0.1 through
work-item-0.2.0.3 pass their Exit Gates from named revisions, the checked-in
real-client and corpus gates pass through both daemon backends, unsupported
operations remain explicit, and the cumulative milestone-0.1.x regression is
green. Released artifacts follow the decision-0012 generic package policy.

## References

- [PyTorch compatibility roadmap](../pytorch-compatibility-roadmap.md)
- decision-0017 PTX 9.0/`sm_70` capability and semantic-oracle corpus
- decision-0040 physical-hardware boundary (milestone-2.0.0.0)
