---
id: P20260831-043
status: Recorded
captured: 2026-08-31
milestone: M0130
workstream: W0132
branch: main
git_revision: 67eaf28
workspace: generation-bound staging suballocation and timeline admission model are tested; physical Vulkan allocation and direct-import qualification remain open
---

# M0130 W0132 Vulkan Memory Model

## Outcome

The first W0132 stage is recorded at content revision `67eaf28`. The Vulkan
runtime now has a `StagingLedger` that derives its budget from the W0131
capability profile, performs aligned first-fit suballocation, prevents overlap,
and retains generation-bound release/validation. `TimelineGate` provides
monotonic submit, completion, and wait admission with stale-generation and
future-value rejection.

This is a host-independent ownership model. It does not claim physical
`VkDeviceMemory`, non-coherent flush/invalidate, external-handle import,
cross-process semaphore visibility, or driver-family qualification.

## Verification evidence

| Gate | Result |
|---|---|
| Vulkan memory model | Passed: staging budget, alignment, non-overlap, release/reuse, generation, and timeline fixtures |
| Vulkan configure/build | Passed with C++20 using `nix develop .#vulkan --command cmake --preset vulkan` |
| Full Vulkan CTest | Passed: 84/84, including `metaflux.backend.vulkan-memory` |
| Repository gates | Passed: `python3 tools/check-agent-records.py .` and `git diff --check` |
| Content identity | Passed: `67eaf28`, Agent Harness (codex) as Author and Committer |

## Boundary

W0132 remains Active. The next stages must wire actual instance/device/queue
allocation on a qualified ICD, add non-coherent synchronization and lifecycle
drain, and measure each direct-import tier before advertising it. The staging
model alone is not a device-memory or release qualification.

## Cleanup

- Removed: none; the external CMake build directory remains ignored and owned
  by the build workflow.
- Retained: W0132 runtime source, tests, plan update, and this checkpoint.

## roast

### light roasts

- Staging allocation and generation ownership -> `plugins/backend/vulkan/runtime/src/memory.cpp` (content `67eaf28`; memory CTest)
- Timeline admission -> `plugins/backend/vulkan/runtime/include/metaflux/backend/vulkan_memory.hpp` (content `67eaf28`; timeline regression)

### medium roasts

- none.

### dark roasts

- none.

## session-only

- Physical Vulkan memory is not qualified on this host - reason: no discoverable
  AMD ICD exists, so this checkpoint retains only host-independent model evidence.

## Handoff

Resume S0132 from this checkpoint and W0132. Run
`nix develop .#vulkan --command ctest --preset vulkan`, then inspect the
capability profile before adding actual Vulkan allocation or synchronization.
