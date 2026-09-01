---
name: gpu-virtualization-vfio-user
description: Design or review QEMU/KVM vfio-user negotiation, shared guest RAM, DMA map and unmap, IOVA epochs, ioeventfd or IRQ transport, reset, disconnect, and failure containment. Use for milestone-0.1.1.0 static guest and milestone-0.1.2.0 vfio-user lifecycle-adapter work. Do not use for PCI config-space layout, kernel cdev UAPI, or physical VFIO passthrough setup.
---

# GPU Virtualization with vfio-user

## Inputs

- The active milestone-0.1.1.0/milestone-0.1.2.0 work item, canonical transport-envelope manifest, and
  exact QEMU, libvfio-user, protocol, host kernel, and guest kernel matrix.
- Server/device feature set, region and IRQ descriptors, shared-memory backend,
  DMA map/unmap records, worker lease, mapping/generation epochs, and reset/fault
  behavior affected by the task.
- Traces showing socket control traffic, region access, ioeventfd/MSI-X, DMA
  lifetime, and steady-state submissions.

Treat the QEMU/server boundary as trusted only within the plan's deployment
model. Validate every guest-derived range, flag, offset, width, epoch, and quota.

## Routing

- Use [virtualization models](references/virtualization-models.md) to choose and
  explain vfio-user versus passthrough, mdev, or SR-IOV.
- Use [vfio-user protocol](references/vfio-user-protocol.md) for negotiation,
  regions, DMA, IRQ, and socket-message semantics.
- Use [guest DMA lifetime](references/guest-dma-lifetime.md) for shared guest RAM,
  map/unmap, IOVA validation, references, and drain.
- Use [reset and failure](references/reset-and-failure.md) for disconnect,
  terminal reset, worker/QEMU loss, and milestone-0.1.2.0 recovery boundaries.
- Route host/guest kernel UAPI to `$linux-device-driver-uapi`, PCI config/BAR/
  MSI-X presentation to `$pcie-vpci-device-model`, and coordinated generation
  transitions to `$device-lifecycle-resilience`.

## Workflow

1. Freeze the QEMU/libvfio-user/protocol support matrix and state why vfio-user
   is the selected milestone-0.1.1.0 model. Keep other virtualization models out of the
   implementation path unless a later decision changes scope.
2. Define standard vfio-user connection/feature negotiation separately from the
   MetaFlux readiness profile. In milestone-0.1.1.0 advertise neither migration nor reset;
   direct mapping is accepted only when each required DMA map supplies a valid
   mmap-capable fd. Record region/IRQ inventory and exact rejection behavior
   before marking BAR0 ready.
3. Require file-backed shared guest RAM with `share=on` and mmap-capable DMA-map
   fds for the high-speed profile. Reject socket-mediated region fallback during
   negotiation.
4. Build one mapping ledger keyed by IOVA range, permissions, file/offset,
   mapping epoch, generation, references, and revocation state. Validate every
   descriptor translation against it.
5. Keep negotiation, DMA map/unmap, device state, and recovery on the socket;
   move steady-state descriptors/data to shared memory and notifications to
   ioeventfd/MSI-X.
6. Remove mappings from lookup before drain and acknowledge unmap only after all
   queue/backend/callback/host references are gone. Deadline failure transitions
   to lost and closes the connection without false success.
7. Model reset/disconnect and stale completion fencing. milestone-0.1.1.0 does not advertise
   the reset command; a guest/QEMU reset observation is terminal loss, and a
   defensively received reset command never receives success. Only milestone-0.1.2.0
   lifecycle may advertise coordinated reset and publish a replacement generation.
8. Treat vfio-user message IDs as sender-owned values echoed in replies: they may
   be reused concurrently, receivers assume no uniqueness, and they are never
   duplicate-detection or idempotence keys. Process client commands in receive
   order, honor `No_reply` without skipping side effects, and do not infer a total
   order across the two command directions.

## Output

Return or implement:

- A model-selection rationale and exact negotiation/capability matrix, including
  reset/migration advertisement bits and unsupported-command results.
- Generated MetaFlux region/capability projections traced to the canonical
  transport-envelope manifest; vfio-user host-native framing remains an adapter.
- A protocol state machine and DMA/IOVA ownership ledger.
- Reset/disconnect/failure transitions with socket and guest-observable results.
- Trace-backed evidence that warm commands avoid vfio-user socket traffic and
  QEMU main-loop region writes.

## Verification

- Test compatible, missing, unknown, malformed, reordered, and interrupted
  negotiation/messages against every pinned QEMU/libvfio-user pair. Include
  concurrent message-ID reuse, `No_reply` followed by a replied command, and
  interleaved opposite-direction traffic without treating IDs as duplicates.
- Regenerate server/guest region, BAR, capability, and golden-byte projections
  from a clean checkout; fail on duplicate manifest imports or a layer-local
  normative layout.
- Exercise DMA overlap, overflow, width, flags, fd/offset, permission, epoch,
  stale generation, partial map, concurrent unmap, and deadline failure.
- Inject QEMU, server, daemon, worker, and backend loss plus guest reset while
  descriptors and callbacks are in flight; prove no stale completion reaches a
  replacement generation.
- Trace that active Add/Copy emits no socket message, QEMU main-loop write,
  provider syscall/allocation, or per-command interrupt.
- Qualify identity and Add/Copy end to end in the guest; protocol unit tests alone
  do not establish GPU virtualization readiness.
