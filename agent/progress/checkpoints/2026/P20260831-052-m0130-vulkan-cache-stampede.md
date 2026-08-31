---
id: P20260831-052
status: Recorded
captured: 2026-08-31
milestone: M0130
workstream: W0135
branch: main
git_revision: 246f84b
workspace: cross-process single-key cache misses coalesce through a stable lock; pipeline-bound warm path remains open
---

# M0130 W0135 Vulkan Cache Miss Coalescing

## Outcome

The cross-process coordination stage is recorded at content revision `246f84b`.
`PersistentCacheRepository::lookup_or_publish` derives a stable lock file from
the cache entry token and holds an advisory `flock` across a second resident /
durable lookup, producer callback, and atomic publication. Waiters recheck after
the lock owner exits, so only a true miss invokes the producer. Lock timeout and
filesystem failure return `io-error`; a producer that declines work remains a
miss.

This is host-independent cache coordination evidence. It does not claim opaque
`VkPipelineCache` handling, pipeline creation, warm-launch compiler/allocation
exclusion, pipeline-bound device/driver invalidation, or physical qualification.

## Verification evidence

| Gate | Result |
|---|---|
| Cross-process single-key coordination | Passed: parent/child miss race invokes one producer and both callers receive a hit |
| Focused cache CTest | Passed: 5 repeat runs of `metaflux.backend.vulkan-cache-model` |
| Full Vulkan CTest | Passed: 88/88 |
| Formatting and diff checks | Passed: `clang-format` and `git diff --check` |
| Content identity | Passed: `246f84b`, Agent Harness (codex) as Author and Committer |
| Agent records | Pending the separate record commit for this checkpoint |

## Boundary

W0135 remains Active. The next stage must connect cache residency and
invalidation to a real pipeline identity, preserve live-reference pinning, and
measure warm launch without invoking MLIR, SPIR-V tools, validation,
shader-module/pipeline creation, or MetaFlux-owned allocation.

## Cleanup

- Removed: none; temporary cache directories, marker files, and lock files are
  self-cleaned with the test directory.
- Retained: lookup-or-publish implementation/tests, W0135 plan stage, and this
  checkpoint.

## roast

### light roasts

- Cross-process single-key miss coalescing -> `plugins/backend/vulkan/runtime/src/cache.cpp` (`246f84b`; parent/child regression)
- Producer-only-on-true-miss contract -> `plugins/backend/vulkan/runtime/tests/cache_test.cpp` (`246f84b`; five repeat runs)

### medium roasts

- none.

### dark roasts

- none.

## session-only

- Cross-process cache coordination - reason: pipeline-bound invalidation, Vulkan
  execution, warm-launch tracing, and physical driver qualification are outside
  this checkpoint.

## Handoff

Resume W0135 from this checkpoint with
`nix develop .#vulkan --command ctest --preset vulkan`, then read the W0135 plan
and Vulkan cache/device-loss reference before implementing pipeline-bound cache
invalidation.
