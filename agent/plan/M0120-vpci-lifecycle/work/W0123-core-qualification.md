---
id: W0123
delivery: 0.1.2.3
milestone: M0120
status: Active
area: lifecycle.qualification
depends_on: [W0122]
updated: 2026-08-31
---

# Core Lifecycle Qualification and Freeze

## Outcome

Qualify the lifecycle core independently of experimental bare-metal vPCI, then
freeze `mf_admin_lifecycle_v1` from one schema.

## Work

- [x] Add a host-independent Coordinator qualification fixture covering 1,000
  reset/remove/add cycles. It retains and resolves all 2,000 retired
  generations as `DEVICE_LOST`, checks the three mirror callback streams, and
  verifies replay/conflict behavior after the run. The static authority bounds
  are 4,096 request records and 2,048 tombstones; no eviction is introduced.
- [x] Serialize the Coordinator's public control-plane operations with one
  reentrant authority mutex and qualify concurrent duplicate replay alongside
  open, mmap, submit, and telemetry-style observations. The fixture proves one
  accepted replay, idempotent duplicates, legal intermediate states, and final
  generation/tombstone invariants without changing the transport ABI.
- [ ] Implement and qualify canonical MetaFlux udev/node policy for existing
  transports; the optional namespace launcher and NVIDIA-named aliases belong
  only to W0124.
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
old objects deterministically return `DEVICE_LOST`, and M0110 remains green
without a data-plane ABI change.
