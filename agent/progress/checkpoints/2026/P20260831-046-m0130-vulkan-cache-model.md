---
id: P20260831-046
status: Recorded
captured: 2026-08-31
milestone: M0130
workstream: W0135
branch: main
git_revision: 073376d
workspace: deterministic Vulkan cache identities and bounded in-memory catalog are tested; filesystem and pipeline warm path remain open
---

# M0130 W0135 Vulkan Cache Model

## Outcome

The first W0135 stage is recorded at content revision `073376d`.
`CacheIdentity` serializes deterministic portable and device-bound identities
from Kernel IR, target, compiler/lowering/tool epochs, FP and argument/backend
ABIs, specialization, and physical device/driver UUIDs. `CacheCatalog` models
bounded publication, hit/miss behavior, corruption removal for rebuild, live
reference pinning, and LRU quota eviction.

This is a host-independent catalog model. It does not claim atomic filesystem
publication, opaque `VkPipelineCache` data, pipeline creation, or warm-launch
trace qualification.

## Verification evidence

| Gate | Result |
|---|---|
| Cache key model | Passed: portable/device partitioning and field mutation misses |
| Cache catalog model | Passed: corruption rebuild, live pinning, LRU eviction, and quota exhaustion |
| Vulkan configure/build | Passed with C++20 using `nix develop .#vulkan --command cmake --preset vulkan` |
| Full Vulkan CTest | Passed: 87/87, including `metaflux.backend.vulkan-cache-model` |
| Repository gates | Passed: `python3 tools/check-agent-records.py .` and `git diff --check` |
| Content identity | Passed: `073376d`, Agent Harness (codex) as Author and Committer |

## Boundary

W0135 remains Active. Next actions are atomic filesystem publication, opaque
device cache handling, warm-launch trace assertions, and integration with the
future W0133/W0134 pipeline path. A model hit is not a Vulkan pipeline hit.

## Cleanup

- Removed: none; the external CMake build directory remains ignored and owned
  by the build workflow.
- Retained: W0135 cache model source, tests, plan update, and this checkpoint.

## roast

### light roasts

- Portable/device cache identity and catalog -> `plugins/backend/vulkan/runtime/src/cache.cpp` (content `073376d`; cache CTest)

### medium roasts

- none.

### dark roasts

- none.

## session-only

- Vulkan pipeline cache is not qualified on this host - reason: no physical ICD
  or pipeline creation path is present; this checkpoint records key/catalog
  behavior only.

## Handoff

Resume S0135 from this checkpoint and W0135. Run
`nix develop .#vulkan --command ctest --preset vulkan`, then read the Vulkan,
runtime-contracts, and cache-boundary guidance before adding filesystem or
pipeline behavior.
