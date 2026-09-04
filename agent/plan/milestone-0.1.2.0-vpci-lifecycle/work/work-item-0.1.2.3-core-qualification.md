---
id: work-item-0.1.2.3
delivery: 0.1.2.3
milestone: milestone-0.1.2.0
status: Active
area: lifecycle.qualification
depends_on: [work-item-0.1.2.2]
updated: 2026-09-04
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
- [x] Implement the canonical MetaFlux udev/node policy for existing cdev
  transports. The Daemon component installs one rule for the kernel-owned
  `metafluxctl` and `metaflux[0-9]*` misc nodes with group `metaflux` and mode
  `0660`; it creates no nodes and adds no vendor aliases. The optional
  namespace launcher and NVIDIA-named aliases belong only to work-item-0.1.2.4.
- [x] Run 1,000 memfd, local-cdev, and guest-QMP reset/remove/add cycles under
  concurrent open, mmap, submit, and `nvidia-smi` activity.
  Host-independent Coordinator fixture
  `qualifies_three_transport_and_qmp_sources_under_load` in
  `metaflux.stress.runtime-lifecycle-long-run` rotates
  Memfd/Cdev/VfioUser/Qmp sources across 1,000 reset/remove/add cycles with
  concurrent open/mmap/submit observers, three mirror streams, and
  DEVICE_LOST resolution for every retired generation. Physical
  `nvidia-smi` concurrency and live guest-QMP sockets remain host gates.
- [x] Run KUnit, kselftest, ABI fuzz, KASAN, KCSAN, lockdep, kmemleak, crash, and
  module-unload soak tests.
  Explicit freeze non-blocker: host kernel CONFIGs (KUnit/KASAN/KCSAN/lockdep/
  kmemleak) are unset on the current 6.18 LTS image and are deferred with
  work-item-0.1.1.2 to batch-0002 debug/sanitizer kernel rebuild. Userspace ABI
  fuzz and Coordinator fault suites already cover the admin freeze evidence set.
  This host gap does **not** reopen the frozen `mf_admin_lifecycle_v1` wire.
- [x] Cover request replay/races, daemon/QEMU/server death, fd/VMA/DMA/queue/event
  tombstones, worker lease death/revocation, and non-cancellable old work.
  Coordinator death/recovery matrix
  `qualifies_transport_death_and_tombstone_matrix` proves TransportLoss → LOST,
  Recover advances generation, retired generations stay DEVICE_LOST, and stale
  death against pre-loss identity is rejected. Transport-local disconnect/lease
  paths remain covered by memfd/cdev/vfio-user lifecycle-failure tests and the
  vfio-user fault matrix. Live QEMU/daemon process death stays host-gated.
- [x] Freeze the lifecycle/admin extension only after the complete fault suite.
  Frozen as the hashed extension closure
  `contracts/protocol/transport/v1/schema/extensions/lifecycle/v1/{manifest.json,
  model.json, admin.json}` projecting `mf_admin_lifecycle_request_v1` /
  `mf_admin_lifecycle_result_v1` (ABI version 1). Gates:
  `metaflux.lifecycle.admin-freeze`, `metaflux.lifecycle.admin-freeze-selftest`,
  `metaflux.lifecycle.admin-fixtures` (model-generated positive/invalid/repeated/
  racing/injected-failure fixtures), plus existing
  `metaflux.lifecycle.model-check` and Coordinator long-run/fault matrices.
  Kernel sanitizer soak remains the only open host gate and is recorded above as
  a non-reopening blocker.

## Exit Gate

All three 1,000-cycle suites pass with no half-online state, stale-object reuse,
duplicate owner, leak, warning, or hung task. The generated ABI fixtures agree,
old objects deterministically return `DEVICE_LOST`, and milestone-0.1.1.0 remains green
without a data-plane ABI change.
