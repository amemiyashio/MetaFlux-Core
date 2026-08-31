---
id: P20260831-067
status: Recorded
captured: 2026-08-31
milestone: M0110
workstream: W0113
branch: main
git_revision: 9a5cb6b
workspace: host-independent vfio-user guest paired-ring adapter
---

# M0110 W0113 Guest Ring Checkpoint

## Outcome

The vfio-user guest half now attaches the generated submission and completion
ring mappings through the existing client fastpath. Attachment checks the fixed
queue IDs, registry-view identity, queue generation, and equal capacity. The
guest exposes payload bounds, submission/completion waits, completion polling,
and a BAR2 doorbell callback that fires only after a successful submission.

## Verification evidence

| Gate | Result |
|---|---|
| Focused guest CTest | Passed: `metaflux.transport.vfio-user-guest` |
| Full development CTest | Passed: 84/84 |
| Component graph and transport schema | Passed in the full development preset |
| Build and diff checks | Passed: CMake build and `git diff --check` |
| Content identity | Passed: `9a5cb6b`, Agent Harness (codex) as Author and Committer |
| Agent records before checkpoint record | Passed: 58 sessions / 390 events / 335 Markdown files |

## Boundary

This proves the host-independent paired-ring and publication-callback seam. It
does not prove live QEMU/libvfio-user transport, BAR2 MMIO, MSI-X delivery,
generation-bound DMA lifetime, Add/Copy execution through the guest, or physical
NVIDIA/CUDA qualification.

## Cleanup

- Removed: no session-owned disposable artifacts.
- Retained: the guest adapter, focused regression, and compact evidence records.

## roast

### light roasts

- Guest paired-ring attachment and success-only doorbell publication ->
  `transports/vfio-user/guest/src/guest.c` (`9a5cb6b`; focused/full CTest)

### medium roasts

- W0113 host-independent guest data-plane boundary ->
  `agent/plan/M0110-kernel-guest-transport/work/W0113-static-vfio-user.md`
  (P067; live transport and PCI qualification remain open)

### dark roasts

- none.

## session-only

- Memfd-backed local ring fixture - reason: validates queue and callback
  semantics without representing physical-device or QEMU qualification.

## Handoff

Resume W0113 from `9a5cb6b`. Keep the generated ring layout authoritative while
connecting the adapter to the pinned QEMU/libvfio-user BAR2/MSI-X path, then
bind generation-bound DMA and Add/Copy fault handling.
