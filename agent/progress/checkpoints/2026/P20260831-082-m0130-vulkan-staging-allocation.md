---
id: P20260831-082
status: Recorded
captured: 2026-08-31
milestone: M0130
workstream: W0132
branch: main
git_revision: a9ba189f65da06d4c1ba936c4372160d50498aae
workspace: physical Vulkan host-visible staging allocation
---

# M0130 W0132 Physical Staging Allocation Checkpoint

## Outcome

The Vulkan runtime now has a source-local host-visible staging adapter. It
creates a generation-local `VkBuffer`, selects a compatible host-visible memory
type while preferring host-coherent memory, allocates and binds
`VkDeviceMemory`, maps it, and guards flush/invalidate ranges. The stable
`mf_backend_api_v1` ABI is unchanged.

## Verification evidence

| Gate | Result |
|---|---|
| Changed-file format check | Passed with the pinned `.#vulkan` shell |
| Vulkan build | Passed: backend and `metaflux_vulkan_device_test` targets built |
| Full Vulkan runtime CTest | Passed: 90/90 under `.#vulkan-runtime` on AMD/RADV |
| Physical device staging test | Passed: allocation, mapping, coherent visibility calls, and range rejection |
| Diff checks | Passed: `git diff --check` |
| Content identity | `a9ba189`; Agent Harness (codex) is Author and Committer |

## Boundary

This checkpoint proves physical host-visible staging allocation and mapping on
the current AMD/RADV host only. Backend admission, device-local copy and
suballocation, external-handle import, timeline/device-loss integration,
non-coherent-only physical coverage, a second driver family, and physical
NVIDIA qualification remain open under W0132/W0134/M1000.

## Cleanup

- Removed: none.
- Retained: no test logs or generated snapshots; the external build directory
  remains under its existing build owner.

## roast

### light roasts

- Physical Vulkan host-visible staging allocation and mapping ->
  `plugins/backend/vulkan/runtime/src/vulkan_staging.cpp` (`a9ba189`, 90/90
  runtime CTest)

### medium roasts

- W0132 physical staging adapter boundary ->
  `agent/plan/M0130-vulkan-backend/work/W0132-device-memory.md` (P082)

### dark roasts

- none.

## session-only

- AMD/RADV is the only physical Vulkan host exercised. No non-coherent-only
  hardware fixture or second driver family was available; those qualification
  claims remain out of this checkpoint.

## Handoff

Resume W0132 from `a9ba189` and P082. Read the W0132 plan, Vulkan README, the
source-local device/staging adapter, and the host-independent visibility ledger.
The next bounded slice is backend admission plus a device-local copy path; keep
the stable C ABI and the physical AMD/RADV evidence boundary intact.
