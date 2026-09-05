---
id: work-item-0.1.3.5
delivery: 0.1.3.5
milestone: milestone-0.1.3.0
status: Complete
area: backend.vulkan.cache
depends_on: [work-item-0.1.3.3, work-item-0.1.3.4]
updated: 2026-09-06
---

# Vulkan Cache and Warm Path

## Outcome

Publish portable canonical SPIR-V/reflection/argument metadata and separate
device-bound live/opaque pipeline caches while keeping warm launch free of
compiler, validator, module/pipeline creation, and MetaFlux-owned allocation.

Portable keys cover Kernel IR/schema; compiler and Vulkan-lowering epochs; MLIR
pipeline and SPIRV-Tools; exact target/feature/property digest; FP mode; argument
ABI; specialization constants; and backend ABI. Device-bound storage contains
live `VkPipeline` objects and opaque `VkPipelineCache` data. Its keys additionally
cover vendor/device, device UUID, driver UUID/ID/version, and pipeline-cache UUID.

Live executables pin pipelines; LRU eviction touches only unreferenced entries.
Publication is atomic, concurrent misses for one key coalesce, and corrupt,
truncated, incompatible, or rejected opaque data is removed and rebuilt.
`FAIL_ON_PIPELINE_COMPILE_REQUIRED` plus creation feedback proves the warm path
where available; other platforms use traces and a separate p99 creation budget.

## Work

- [x] Add deterministic portable/device cache identities and a bounded catalog
  model. Key mutations miss, corrupt unpinned entries are removed for rebuild,
  live references pin entries against LRU eviction, and a full pinned catalog
  reports quota exhaustion.
- [x] Add host-independent filesystem persistence for portable and device-bound
  entries. The file envelope carries the complete key, cache mode, payload size,
  and digest; publish uses a process-unique temporary file, `fsync`, and atomic
  rename, while malformed, truncated, or mismatched entries are removed and
  device-bound entries can be explicitly invalidated.
- [x] Integrate `CacheFileStore` with `CacheCatalog` for process-local
  residency, validated hydration, live-reference pinning, and quota/eviction.
- [x] Add cross-process single-key stampede control. `lookup_or_publish` uses a
  stable per-key advisory lock, rechecks resident and durable stores after
  acquisition, and invokes the producer only for a true miss.
- [x] Add a host-independent pipeline binding boundary. A validated resident
  device-bound hit is pinned under one generation-scoped binding; duplicate or
  stale generations are rejected and release is required before device
  invalidation can remove the entry. Actual `VkPipeline` creation remains open.
- [x] Verify every portable and device-bound identity field causes a miss for
  its changed key while unchanged key partitions remain hits. Provisional
  extension revisions still require their producer and release evidence.
- [x] Add a host-independent warm-launch trace admission contract. It accepts
  only cache lookup, pipeline binding, argument binding, and submit in order,
  and rejects compiler, validator, shader-module, pipeline, Vulkan-allocation,
  and MetaFlux-allocation events. Physical trace capture is exercised by the
  Vulkan pipeline qualification test.
- [x] Prove warm launch invokes no MLIR/SPIR-V compiler or validator, creates no
  shader module/pipeline/Vulkan allocation, and performs no MetaFlux-owned heap
  allocation. The physical warm test starts from an exported device pipeline
  cache, hydrates a device-bound repository hit, reuses the already-created
  `VulkanComputePipeline`, submits through the real queue, waits for completion,
  validates the four-event trace, and observes zero host allocations during
  warm submission. The source boundary contains no compiler, validator, or
  Vulkan object creation call.

## Exit Gate

Corrupt/truncated caches recover; all identity/target/ABI mutations miss; warm
traces contain no compiler, validation, shader-module, or pipeline creation.
The physical cache-hit qualification is `metaflux.backend.vulkan-pipeline` and
the host-independent trace matrix is `metaflux.backend.vulkan-cache-model`.
