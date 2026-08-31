---
id: P20260831-060
status: Recorded
captured: 2026-08-31
milestone: M0130
workstream: W0132
branch: main
git_revision: 8afbe2a
workspace: private Vulkan device context and generation-bound timeline smoke
---

# M0130 W0132 Vulkan Device-Context Checkpoint

## Outcome

W0132 now binds the existing successful capability profile to a private Vulkan
1.3 instance, an exact physical-device identity, one compute queue, and a
timeline semaphore at content revision `8afbe2a`. The context rechecks the
profile's API and driver versions, UUID, queue family/count, subgroup and
workgroup limits, memory totals, and required timeline/Synchronization2/
buffer-device-address feature chain before creating the logical device.

Generation-checked empty `vkQueueSubmit2` signals, bounded waits, counter polls,
stale-generation checks, monotonic timeline validation, timeout mapping, and
healthy reset are covered by a focused CTest. Vulkan handles stay source-local
C++ state and no stable backend C ABI record changed.

## Verification evidence

| Gate | Result |
|---|---|
| Vulkan device-context focused CTest | Passed: `metaflux.backend.vulkan-device` 1/1 in the lean shell's no-device-safe path |
| Explicit RADV device smoke | Passed: `nix develop .#vulkan-runtime` with `VK_ICD_FILENAMES=.../radeon_icd.x86_64.json`, queue-family=0, timeline=2 |
| Full lean Vulkan CTest | Passed: 89/89, including capability, memory, target, stream, cache, and device tests |
| Build and warning checks | Passed: Vulkan configure/build and clang-format checks |
| Record validation | Passed: Agent records before the separate checkpoint commit |
| Content identity | Passed: `8afbe2a`, Agent Harness (codex) as Author and Committer |

## Boundary

This is a device bring-up and timeline-adapter checkpoint. Physical
`VkDeviceMemory`, mapped flush/invalidate, external-memory/semaphore import,
command-buffer/pipeline ownership, M0110 completion composition, device-loss
fault injection, and two-driver-family qualification remain open. The AMD/RADV
run is local provisioning/single-driver smoke evidence only; it does not close
physical NVIDIA, performance, or release gates.

## Cleanup

- Removed: none; no build tree, source snapshot, downloaded package, or ordinary
  log was copied into Agent records.
- Retained: source-local context, focused test, plan boundary, and compact
  checkpoint/session records; the external build directory remains owned by
  CMake/CTest.

## roast

### light roasts

- Profile-bound Vulkan device context -> `plugins/backend/vulkan/runtime/src/vulkan_device.cpp` (`8afbe2a`; lean build and explicit RADV smoke)
- Generation-checked timeline operations -> `plugins/backend/vulkan/runtime/tests/vulkan_device_test.cpp` (`8afbe2a`; focused CTest)
- W0132 bring-up boundary -> `plugins/backend/vulkan/README.md` (`8afbe2a`; explicit no-ABI/no-qualification statement)

### medium roasts

- none.

### dark roasts

- none.

## session-only

- AMD RADV queue-family/timeline output - reason: current-host observation is useful for smoke reproduction but is not portable, dual-driver, physical NVIDIA, or release qualification evidence

## Handoff

Resume from `8afbe2a` and P060. Use `nix develop .#vulkan-runtime` with an
explicit ICD for local device smoke; next W0132 work must attach the context to
the staging ledger and real allocation/flush/invalidate operations while
preserving generation and external-handle ownership rules.
