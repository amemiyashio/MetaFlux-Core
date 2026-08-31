---
id: P20260831-051
status: Recorded
captured: 2026-08-31
milestone: M0130
workstream: W0135
branch: main
git_revision: e1288c1
workspace: process-local cache repository integrates durable files with bounded catalog residency; cross-process pipeline work remains open
---

# M0130 W0135 Vulkan Cache Catalog Integration

## Outcome

The process-local cache integration is recorded at content revision `e1288c1`.
`PersistentCacheRepository` composes `CacheFileStore` persistence with
`CacheCatalog` residency. Publish admission checks pinned entries and quota
before writing a file; resident-first lookup returns a hit without I/O, while a
validated disk hit hydrates the bounded catalog and participates in LRU
eviction. Pin/unpin and device invalidation are serialized at the repository
boundary.

This is host-independent persistence and residency evidence. It does not claim
cross-process single-key stampede control, opaque `VkPipelineCache` handling,
pipeline creation, warm-launch compiler exclusion, or physical driver
qualification.

## Verification evidence

| Gate | Result |
|---|---|
| Persistent repository lifecycle | Passed: publish admission, resident-first lookup, disk hydration, pin protection, quota, and device invalidation |
| Focused cache CTest | Passed: `metaflux.backend.vulkan-cache-model` |
| Full Vulkan CTest | Passed: 88/88 |
| Formatting and diff checks | Passed: `clang-format` and `git diff --check` |
| Content identity | Passed: `e1288c1`, Agent Harness (codex) as Author and Committer |
| Agent records | Pending the separate record commit for this checkpoint |

## Boundary

W0135 remains Active. The next stage must coordinate concurrent processes for a
single cache key, then bind residency and invalidation to a real Vulkan pipeline
identity without evicting live references. Warm launch must separately prove no
compiler, validator, shader-module, pipeline, or MetaFlux-owned allocation.

## Cleanup

- Removed: none; temporary cache directories are self-cleaned by the regression.
- Retained: persistent repository implementation/tests, W0135 plan stage, and
  this checkpoint.

## roast

### light roasts

- Persistent cache repository boundary -> `plugins/backend/vulkan/runtime/src/cache.cpp` (`e1288c1`; full Vulkan CTest)
- Disk-hit hydration with LRU residency -> `plugins/backend/vulkan/runtime/tests/cache_test.cpp` (`e1288c1`; cross-instance and eviction regression)
- Pinned publication and device invalidation -> `plugins/backend/vulkan/runtime/include/metaflux/backend/vulkan_cache.hpp` (`e1288c1`; quota/pin regression)

### medium roasts

- none.

### dark roasts

- none.

## session-only

- Process-local cache integration - reason: cross-process stampede, pipeline
  ownership, warm-launch tracing, and physical driver qualification are outside
  this checkpoint.

## Handoff

Resume W0135 from this checkpoint with
`nix develop .#vulkan --command ctest --preset vulkan`, then read the W0135 plan
and Vulkan cache/device-loss reference before implementing cross-process
coordination.
