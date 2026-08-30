---
id: P20260831-049
status: Recorded
captured: 2026-08-31
milestone: M0130
workstream: W0135
branch: main
git_revision: 4959da6
workspace: portable and device-bound cache file publication is verified; pipeline integration remains open
---

# M0130 W0135 Vulkan Cache Filesystem

## Outcome

The filesystem publication stage is recorded at content revision `4959da6`.
`CacheFileStore` persists portable and device-bound entries under deterministic
short tokens while storing the complete canonical key in each envelope. It
writes a process-unique temporary file, flushes file and directory metadata,
and atomically renames the entry. Reads verify cache mode, complete key, length,
and payload digest; malformed, truncated, or mismatched files are removed before
returning `corrupt`. Device-bound entries have an explicit invalidation path.

This is host-independent cache storage evidence. It does not claim
cross-process stampede control, `CacheCatalog`/filesystem pin and quota
integration, opaque `VkPipelineCache` handling, pipeline creation, or warm-launch
tracing.

## Verification evidence

| Gate | Result |
|---|---|
| Filesystem cache round trip | Passed: atomic replacement, cross-instance read, and no temporary-file residue |
| Corruption recovery | Passed: truncated and equal-length payload mutations return `corrupt` and remove the entry |
| Device invalidation and input bounds | Passed: device-bound removal preserves portable entries and rejects invalid keys/oversized payloads |
| Full Vulkan CTest | Passed: 88/88 |
| Formatting and diff checks | Passed: `clang-format` and `git diff --check` |
| Agent records | Pending the separate record commit for this checkpoint |
| Content identity | Passed: `4959da6`, Agent Harness (codex) as Author and Committer |

## Boundary

W0135 remains Active. The next stage must connect file persistence to live
catalog references and quota/eviction, add cross-process single-key stampede
coordination, and bind device/driver invalidation to a real pipeline identity.
Pipeline creation and warm-launch no-compiler traces remain open.

## Cleanup

- Removed: none; temporary test directories self-cleaned and the external CMake
  build directory remains ignored and owned by the build workflow.
- Retained: CacheFileStore implementation/tests, W0135 plan stage, and this
  checkpoint.

## roast

### light roasts

- Atomic cache file publication -> `plugins/backend/vulkan/runtime/src/cache.cpp` (`4959da6`; full Vulkan CTest)
- Cache envelope corruption recovery -> `plugins/backend/vulkan/runtime/tests/cache_test.cpp` (`4959da6`; truncation and equal-length mutation regressions)
- Device-bound cache invalidation -> `plugins/backend/vulkan/runtime/include/metaflux/backend/vulkan_cache.hpp` (`4959da6`; device invalidation regression)

### medium roasts

- none.

### dark roasts

- none.

## session-only

- Host-independent cache storage - reason: live Vulkan pipeline, cross-process
  coordination, and physical driver qualification are outside this checkpoint.

## Handoff

Resume S0135 from this checkpoint with
`nix develop .#vulkan --command ctest --preset vulkan`, then read W0135 and the
Vulkan cache/device-loss reference before integrating catalog residency with the
filesystem layer.
