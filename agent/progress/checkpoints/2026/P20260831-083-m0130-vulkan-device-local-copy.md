---
id: P20260831-083
status: Recorded
captured: 2026-08-31
milestone: M0130
workstream: W0132
branch: main
git_revision: d050de277837a0ab3bfae2d6327d0310d54194c0
workspace: physical Vulkan Tier 3 device-local copy
---

# M0130 W0132 Device-Local Copy Checkpoint

## Outcome

The Vulkan runtime now pairs the host-visible staging allocation with a
device-local transfer buffer. A source-local command pool records
`vkCmdCopyBuffer` in both directions, and `VulkanDeviceContext::submit_commands`
submits each copy with a generation-checked timeline wait/signal. A 4096-byte
pattern survives an upload and download round trip on the current AMD/RADV host.
The stable `mf_backend_api_v1` ABI is unchanged.

## Verification evidence

| Gate | Result |
|---|---|
| Changed-file format check | Passed with the pinned `.#vulkan` shell |
| Vulkan build | Passed: backend and `metaflux_vulkan_device_test` targets built |
| Physical Tier 3 copy test | Passed: byte-for-byte 4096-byte host/device/host round trip; timeline reached 4 |
| Full Vulkan runtime CTest | Passed: 90/90 under `.#vulkan-runtime` on AMD/RADV |
| Diff checks | Passed: `git diff --check` |
| Content identity | `d050de2`; Agent Harness (codex) is Author and Committer |

## Boundary

This checkpoint proves one physical Tier 3 staging-to-device-local copy path on
AMD/RADV. Backend admission, reusable suballocation, external-handle import,
command-buffer/pipeline composition, cross-process synchronization,
device-loss drain, non-coherent-only physical coverage, second-driver
qualification, and physical NVIDIA qualification remain open.

## Cleanup

- Removed: none.
- Retained: no test logs or generated snapshots; the external build directory
  remains under its existing build owner.

## roast

### light roasts

- Physical Vulkan host/device copy round trip ->
  `plugins/backend/vulkan/runtime/src/vulkan_device_copy.cpp` (`d050de2`,
  direct test and full 90/90 CTest)

### medium roasts

- W0132 Tier 3 device-local copy boundary ->
  `agent/plan/M0130-vulkan-backend/work/W0132-device-memory.md` (P083)

### dark roasts

- none.

## session-only

- AMD/RADV-only physical Vulkan evidence - reason: no second driver family or
  NVIDIA qualification host is available in the current environment.

## Handoff

Resume W0132 from `d050de2` and P083. Read the W0132 plan, Vulkan README,
source-local device/staging/copy adapters, and visibility/queue-submission
ledgers. The next bounded slice is backend admission and ledger composition;
preserve the stable C ABI and keep AMD/RADV evidence separate from v1.0 gates.
