---
id: M0004-W05
milestone: M0004
status: Queued
area: backend.vulkan.cache
depends_on: [M0004-W03, M0004-W04]
updated: 2026-08-27
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

- [ ] Implement both caches, live-reference pinning, atomic publication,
  stampede control, quota/eviction, corruption recovery, and device/driver
  invalidation.
- [ ] Verify every key mutation causes a miss and provisional extension revisions
  deterministically invalidate incompatible entries.
- [ ] Prove warm launch invokes no MLIR/SPIR-V compiler or validator, creates no
  shader module/pipeline/Vulkan allocation, and performs no MetaFlux-owned heap
  allocation.

## Exit Gate

Corrupt/truncated caches recover; all identity/target/ABI mutations miss; warm
traces contain no compiler, validation, shader-module, or pipeline creation.
