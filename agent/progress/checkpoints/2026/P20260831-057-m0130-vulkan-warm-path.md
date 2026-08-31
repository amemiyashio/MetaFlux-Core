---
id: P20260831-057
status: Recorded
captured: 2026-08-31
milestone: M0130
workstream: W0135
branch: main
git_revision: 5b1a304
workspace: cache identity mutation and host-independent warm trace admission are verified; physical warm launch remains open
---

# M0130 W0135 Vulkan Warm-Path Admission Checkpoint

## Outcome

The W0135 cache contract now has a complete identity mutation matrix and a
host-independent warm-launch admission validator at content revision `5b1a304`.
Each portable and device-bound identity field is mutated independently and the
changed key misses while unchanged partitions continue to hit. An accepted
warm trace is exactly cache lookup, pipeline binding, argument binding, and
submit; compiler, validator, shader-module, pipeline, Vulkan-allocation, and
MetaFlux-allocation events are rejected.

This is a deterministic contract/fixture checkpoint. It does not claim an ICD
trace, real `VkPipeline` creation, compiler-free execution on a physical device,
or driver-family qualification.

## Verification evidence

| Gate | Result |
|---|---|
| Identity mutation matrix | Passed: all portable and device-bound key fields produce deterministic misses |
| Warm trace admission | Passed: exact four-event sequence accepted; missing, reordered, unknown, duplicate, and six forbidden events rejected |
| Focused cache CTest | Passed: `metaflux.backend.vulkan-cache-model` |
| Full Vulkan CTest | Passed: 88/88 |
| Formatting and diff checks | Passed: `clang-format` and `git diff --check` |
| Content identity | Passed: `5b1a304`, Agent Harness (codex) as Author and Committer |
| Agent records | Pending the separate record commit for this checkpoint |

## Boundary

W0135 remains Active. The next stage must collect a real Vulkan ICD warm trace
around a validated resident pipeline, prove no compiler/validator/module/
pipeline/allocation calls, and qualify the device/driver matrix. The current
host-independent validator is not that evidence.

## Cleanup

- Removed: none; temporary cache directories self-cleaned by the regression.
- Retained: identity and trace tests, plan updates, and this checkpoint in Git;
  no build, download, source snapshot, or ordinary log artifact was retained.

## roast

### light roasts

- Cache identity mutation miss matrix -> `plugins/backend/vulkan/runtime/tests/cache_test.cpp` (`5b1a304`; full Vulkan CTest 88/88)
- Warm-launch event allowlist -> `plugins/backend/vulkan/runtime/src/cache.cpp` (`5b1a304`; forbidden-event regression)
- Warm-path admission boundary -> `plugins/backend/vulkan/README.md` (`5b1a304`; exact sequence contract)

### medium roasts

- none.

### dark roasts

- none.

## session-only

- Physical ICD trace and driver qualification - reason: no physical Vulkan device
  evidence is available in this host-independent stage

## Handoff

Resume W0135 with:

```sh
nix develop .#vulkan --command ctest --preset vulkan
```

Read W0135 and the cache/device-loss reference before implementing the real
pipeline trace boundary.
