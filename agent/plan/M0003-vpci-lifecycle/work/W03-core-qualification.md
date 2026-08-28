---
id: M0003-W03
milestone: M0003
status: Queued
area: lifecycle.qualification
depends_on: [M0003-W02]
updated: 2026-08-28
---

# Core Lifecycle Qualification and Freeze

## Outcome

Qualify the lifecycle core independently of experimental bare-metal vPCI, then
freeze `mf_admin_lifecycle_v1` from one schema.

## Work

- [ ] Implement and qualify canonical MetaFlux udev/node policy for existing
  transports; the optional namespace launcher and NVIDIA-named aliases belong
  only to M0003-W04.
- [ ] Run 1,000 memfd, local-cdev, and guest-QMP reset/remove/add cycles under
  concurrent open, mmap, submit, and `nvidia-smi` activity.
- [ ] Run KUnit, kselftest, ABI fuzz, KASAN, KCSAN, lockdep, kmemleak, crash, and
  module-unload soak tests.
- [ ] Cover request replay/races, daemon/QEMU/server death, fd/VMA/DMA/queue/event
  tombstones, worker lease death/revocation, and non-cancellable old work.
- [ ] Freeze the lifecycle/admin extension only after the complete fault suite.

## Exit Gate

All three 1,000-cycle suites pass with no half-online state, stale-object reuse,
duplicate owner, leak, warning, or hung task. The generated ABI fixtures agree,
old objects deterministically return `DEVICE_LOST`, and M0002 remains green
without a data-plane ABI change.
