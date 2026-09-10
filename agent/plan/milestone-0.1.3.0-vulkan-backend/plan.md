---
id: milestone-0.1.3.0
delivery: 0.1.3.0
release: v0.1.3
status: Complete
depends_on: [milestone-0.1.1.0, milestone-0.1.2.0]
areas: [backend.vulkan, compiler.spirv]
updated: 2026-09-08
---

# milestone-0.1.3.0: Vulkan Execution Backend

## Outcome

Add a Linux Vulkan 1.3 compute backend behind `mf_backend_api_v1` without
changing provider, transport, or lifecycle hot-path contracts. CUDA and NVML
remain the application-facing compatibility APIs. milestone-0.1.1.0 owns the data plane and
milestone-0.1.2.0 owns lifecycle; this milestone consumes both contracts.

Deliver compute-only discovery/capability reporting, target-constrained Kernel IR
to MLIR SPIR-V lowering, packed buffer-device-address arguments, queue batching,
timeline completion, truthful memory/import tiers, portable and device-bound
caches, a host-independent two-family target matrix, direct AMD/RADV component
execution and deterministic lifecycle models. Physical dual-driver execution
and fault soak are retained by work-item-2.0.0.3 under decision-0040.

Development may begin after [milestone-0.1.1.0](../milestone-0.1.1.0-kernel-guest-transport/plan.md)
stabilizes the backend and transport ABIs. Release requires the
[milestone-0.1.2.0](../milestone-0.1.2.0-vpci-lifecycle/plan.md) core Definition of
Done, not promotion of its experimental software root. Ownership remains as
defined in [the architecture record](../../../docs/architecture/control-and-data-plane.md).

## Locked Boundaries

- Backend implementation is C++20 over the Vulkan C API. Only the stable C
  backend ABI and sized extension records cross components; no Vulkan handle,
  C++ ABI/type, exception, allocator ownership, or STL object crosses it.
- One logical context selects CPU or Vulkan, and memfd/cdev/guest transport,
  before module loading/allocation and first visible success. Neither selection
  changes during that context or after a fault.
- Unsupported CUDA semantics return an explicit capability diagnostic. There is
  no per-kernel CPU fallback, implicit migration/copy of device-local state, or
  weakened FP/barrier/memory-order/subgroup behavior.
- The compiler uses shared MLIR dialects then the MLIR SPIR-V dialect with an
  exact target environment. It does not translate LLVM IR to SPIR-V. Cache-hit
  launch never invokes MLIR, SPIRV-Tools, or `spirv-val`.
- Single physical device, compute queues/buffers, static workgroup memory, and
  timeline semaphores are included. Graphics/images/textures, sparse resources,
  device groups, ray tracing, and presentation are excluded.
- CUDA-warp assumptions, unproved subgroup vote/shuffle/reconvergence, dynamic
  parallelism, unsupported atomic scopes, cross-backend residency/coherence,
  cubin/SASS/fatbin/`nvdisasm`, SR-IOV/P2P/ATS/PASID/PRI paths are excluded.

## Execution Contract

```text
PTX
  -> Kernel IR
  -> shared MLIR arith/scf/memref/vector/gpu dialects
  -> MLIR SPIR-V dialect with explicit spirv.target_env
  -> SPIR-V validation and reflection
  -> VkShaderModule/VkPipeline cache
  -> packed buffer-device-address argument block
  -> vkQueueSubmit2 batch
  -> timeline semaphore
  -> milestone-0.1.1.0 completion timeline
```

CTA maps to Workgroup, thread/block/grid indices map to Vulkan builtins, static
shared memory maps to Workgroup storage, and barriers encode explicit execution
and memory scopes/semantics. Pointers are validated generation-bound device
addresses. Subgroup size is never assumed to be 32.

Streams remain FIFO; event record/wait uses explicit timeline dependencies;
copy/launch visibility uses Synchronization2; default and per-thread default
stream behavior follows the CUDA provider. Batching may preserve, never alter,
these observable dependencies.

## Workstreams

| Workstream | Deliverable |
| --- | --- |
| [work-item-0.1.3.1](work/work-item-0.1.3.1-capability-abi.md) | Vulkan target, capability/argument/memory ABI 0.x, and benchmark fixtures |
| [work-item-0.1.3.2](work/work-item-0.1.3.2-device-memory.md) | Device/queue bring-up, memory tiers, synchronization, and lifecycle hooks |
| [work-item-0.1.3.3](work/work-item-0.1.3.3-spirv-lowering.md) | Target-constrained lowering, validation, reflection, and semantic diagnostics |
| [work-item-0.1.3.4](work/work-item-0.1.3.4-execution-streams.md) | Pipeline execution and CUDA-observable stream/event behavior |
| [work-item-0.1.3.5](work/work-item-0.1.3.5-cache-warm-path.md) | Portable/device caches and compile-free warm launch |
| [work-item-0.1.3.6](work/work-item-0.1.3.6-performance-release.md) | Performance, faults, packaging, and release qualification |

## Memory and Cache Boundaries

The backend advertises three independent memory tiers: compatible OPAQUE_FD or
DMA-BUF plus external synchronization; capability-proven external host memory;
and explicit host-visible staging/device-local copy. Only staging is required for
baseline completion. memfd, arbitrary guest RAM, and host pointers are not called
DMA-BUF or zero-copy. Direct import is an optional, capability-gated promotion.

Portable caches store canonical SPIR-V, reflection, and argument metadata keyed
by Kernel IR/schema, compiler/lowering epochs, tool/pass versions, exact target
digest, FP/argument/specialization state, and backend ABI. Device-bound pipeline
caches add vendor/device, device and driver identities/versions, and pipeline
cache UUID. Publication is atomic; referenced pipelines cannot be evicted;
corrupt/incompatible entries are removed and rebuilt.

## Milestone Acceptance

Correctness:

- The host-independent two-family target/model matrix agrees with independent
  CPU results for its declared forms; direct AMD/RADV component fixtures prove
  their named physical execution rows. Physical two-driver agreement remains
  work-item-2.0.0.3, not a result inferred from synthetic target identities.
- Model import/staging and lifecycle tests preserve generation, permissions,
  ordering and unregister rules without a transport UAPI change. Physical guest
  import and cross-transport GPU qualification remain work-item-2.0.0.3.
- Neutral graph/visibility/loss models pass their declared fixtures. Actual
  stock PyTorch default/PTDS translation and daemon GPU lifecycle are
  work-item-0.2.0.3 acceptance, not inferred from the graph planner. Unsupported
  semantics continue to require explicit failures.
- Target environment, actual enabled features/limits, FP behavior, and cache keys
  remain mutually consistent.

Warm path and cache:

- Warm provider enqueue retains milestone-0.1.1.0 bounds.
- Warm launch invokes no MLIR/SPIR-V compiler or validator and creates no shader
  module, pipeline, Vulkan allocation, or MetaFlux-owned heap object.
- Vulkan ICD submit syscalls are measured separately and are not hidden in the
  client zero-syscall claim.
- Driver/device/target/argument/pipeline UUID changes cause a cache miss.

Throughput and memory (provisional targets, not promoted by model tests):

- Kernels lasting at least 100 microseconds add at most 3% scheduling overhead
  relative to identical SPIR-V, device, queue, memory-tier, and direct-Vulkan
  baseline.
- Advertised direct transfers of at least 16 MiB reach at least 90% of that
  path's native baseline without a whole-buffer extra copy. Staging reports its
  extra copies and uses a separate baseline.
- milestone-0.1.2.0 lifecycle core remains inside its 0.5% steady-state budget. Experimental
  vroot is measured only when separately enabled.

Fault and release (model and named component scope):

- Declared loss/teardown models and named component fixtures enforce `LOST`
  and resource isolation. Stock client propagation belongs to work-item-0.2.0.3;
  physical reset/hotplug and cross-driver qualification belong to
  work-item-2.0.0.3. The deadline promises isolation, not physical cancellation.
- Cache corruption and compiler rejection have deterministic component tests;
  simulated import failures and driver identity changes establish model/key
  behavior. Physical import and driver-change recovery remain v2 qualification.
- milestone-0.1.0.0, milestone-0.1.1.0, and milestone-0.1.2.0 core remain green with Vulkan installed but idle.
- Generic packages require no `/nix/store` runtime path.

## Decision Closure

Capability and target serialization, packed arguments, memory tiers, FP and
unsupported-semantic policy, cache residency, queue topology, and device-loss
isolation closed with work-item-0.1.3.1 through work-item-0.1.3.6 evidence.
Physical dual-family capability and external-memory qualification remain owned
by milestone-2.0.0.0 (decision-0040).

## Deferred Cubin Research

Cubin remains an unnumbered, non-blocking future input:

```text
standalone fixed-SM cubin
  -> bounded structured ELF parser
  -> isolated, version-locked nvdisasm --emit-json worker
  -> schema and operand validation
  -> deterministic allowlisted SASS IR
  -> Kernel IR
  -> native CUDA / interpreter / CPU JIT / Vulkan differential corpus
```

The worker must be low privilege with CPU, RSS, file-size, output, fd,
temporary-directory, and wall-time limits. It rejects unknown opcodes, schema
drift, unresolved relocations/control flow, indirect calls, unsupported barriers,
and reconvergence uncertainty. It adds no runtime UAPI and blocks neither milestone-0.1.3.0
nor later semantic work.

## Definition of Done

milestone-0.1.3.0 retains completion for the milestone-0.1.2.0 regression,
host-independent target/stream/memory/cache/loss contract matrix, named direct
AMD/RADV component execution and warm-path measurements, and generic packaging
rows recorded by its work items. Synthetic driver identities are model inputs,
not two physical driver runs. Physical dual-driver, external-memory promotion
and driver-loss soak remain work-item-2.0.0.3. Stock PyTorch fixed-backend
integration and application lifecycle remain work-item-0.2.0.3.

work-item-0.1.3.1 through work-item-0.1.3.5 may proceed after milestone-0.1.1.0. Vulkan-specific release
integration waits for the milestone-0.1.2.0 core DoD and all gates above. Deferred cubin
research blocks neither path.

The owning work items retain the bounded component evidence and physical FMA
measurements. They do not establish a PyTorch GPU route or promote the
provisional performance budgets. Decision-0040 owns the physical expansion;
decision-0055 owns the framework qualification boundary.

## References

- MLIR SPIR-V dialect: <https://mlir.llvm.org/docs/Dialects/SPIR-V/>
- Vulkan SPIR-V environment:
  <https://docs.vulkan.org/spec/latest/appendices/spirvenv.html>
- Vulkan buffer device address:
  <https://docs.vulkan.org/guide/latest/buffer_device_address.html>
- Vulkan external memory:
  <https://docs.vulkan.org/spec/latest/chapters/memory.html>
- Vulkan synchronization:
  <https://docs.vulkan.org/guide/latest/synchronization_examples.html>
