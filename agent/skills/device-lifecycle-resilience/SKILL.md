---
name: device-lifecycle-resilience
description: Design or review generation and epoch state machines across cdev, vfio-user, vPCI, registry, workers, QMP events, reset, remove, re-add, tombstones, idempotence, deadlines, and fault injection. Use for M0003 lifecycle resilience. Do not use for layer-local wire layouts or target compiler/runtime behavior.
---

# Device Lifecycle Resilience

## Inputs

- The active M0003 work item and canonical lifecycle model, registry authority,
  daemon incarnation, generation/epoch persistence, and deadline policy.
- Layer adapters for cdev, vfio-user, PCI/vroot, QMP, worker leases, providers,
  and backend resources affected by the transition.
- Event traces, request IDs, fault points, old-object/tombstone behavior, and the
  qualification kernel/QEMU matrix.

The daemon lifecycle authority alone allocates and publishes generations/epochs.
Transport and presentation layers mirror committed state and retain local
tombstones; they do not invent replacement identity.

## Routing

- Use [state machine](references/state-machine.md) for guards, commit points,
  identity, idempotence, deadlines, and tombstones.
- Use [QMP events](references/qmp-events.md) for command/event correlation,
  asynchronous completion, disconnect, and reconciliation.
- Use [failure injection](references/failure-injection.md) for stage-by-stage and
  racing faults.
- Use [qualification](references/qualification.md) for model, integration, stress,
  and release evidence.
- Route layer-local mechanics to `$linux-device-driver-uapi`,
  `$gpu-virtualization-vfio-user`, or `$pcie-vpci-device-model`; route Vulkan
  device loss to `$vulkan-spirv-compute` as a paired target adapter.

## Workflow

1. Write the transition schema first: source/event, valid source states, guards,
   request identity, owner, staged resources, irreversible retirement point,
   published intermediate states, terminal states, errors, and deadline.
2. Bind every request to request ID, source, operation, UUID, expected generation,
   daemon incarnation, and deadline. Define duplicate, stale, and conflicting
   behavior before side effects.
3. Allocate candidates monotonically and persist the high-water mark before
   publication. Never reuse a consumed generation or epoch after failure.
4. Stage identity, transport, presentation, backend, and exclusive worker lease
   off-registry. Publish one committed state only when every owner is ready.
5. On retirement, reject new work first; fence queues/completions; drain to
   deadline; then retain old fd/VMA/DMA/queue/event/memory/module/pipeline/handle
   objects as generation-bound tombstones.
6. Normalize admin, vfio-user, QMP, disconnect, daemon restart, worker death, and
   backend/device loss into the same authority model. Reconcile missed external
   events by querying state, not by guessing.
7. Generate positive, invalid, duplicate, stale, reordered, racing, timeout, and
   injected-failure tests from the transition schema.

## Output

Return or implement:

- A complete transition/ownership/commit table and machine-checkable model
  update where the workstream provides one.
- Per-layer prepare, commit, abort, revoke, drain, tombstone, and reconcile
  obligations with exact public errors/events.
- A fault matrix covering every side-effect boundary and late callback.
- Qualification evidence proving single ownership, monotonic identity,
  idempotence, bounded terminal states, and old-work isolation.

## Verification

- Model-check or exhaustively generate bounded event sequences before transport
  implementation; assert one live owner, no identity reuse, and no half-online
  terminal state.
- Test duplicate/stale/conflicting requests, daemon incarnation changes,
  persistence failure, missed/reordered QMP events, deadline expiry, and restart
  reconciliation.
- Inject failure before and after every allocation, publish, lease, drain,
  transport, PCI, backend, and registry commit point; verify exact unwind or lost
  behavior.
- Run reset/remove/re-add under concurrent open, mmap, DMA, submit, completion,
  telemetry, config access, QMP, daemon/server/worker loss, and module lifecycle.
- Keep performance and 1,000-cycle promotion claims tied to archived M0003
  evidence; a state-model unit test is necessary but not release qualification.
