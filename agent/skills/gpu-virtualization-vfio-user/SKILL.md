---
name: gpu-virtualization-vfio-user
description: Implement or review QEMU/KVM vfio-user negotiation, shared guest RAM, DMA map and unmap, IOVA epochs, ioeventfd or IRQ transport, reset, disconnect, and failure containment. Use for milestone-0.1.1.0 static guest and milestone-0.1.2.0 vfio-user lifecycle-adapter work. Do not use for PCI config-space layout, kernel cdev UAPI, or physical VFIO passthrough setup.
---

# GPU Virtualization with vfio-user

Own the guest/server transport and DMA lifetime. First identify whether the
requested change reaches the C++ server fixture or the live QEMU adapter using
the [implementation path](references/implementation-path.md). Trace the
missing guest-visible negotiation, mapping or submission through its real
consumer, then implement that path and its failure cleanup.

Analysis/review requests stay read-only; implementation steps apply to requested
changes. Use the assignment and Exit Gate through [$main](../main/SKILL.md) skill.
Reuse the pinned protocol matrix and select the needed readings below. Follow
[implementation guidance](../review/references/implementation-guidance.md);
a socket probe or regenerated region image alone does not deliver guest compute.

## Select The Work

| Task | Read before changing that boundary |
| --- | --- |
| Negotiation, regions, message parsing or capabilities | [Protocol](references/vfio-user-protocol.md) |
| Guest RAM, map/unmap, IOVA translation or callbacks | [DMA lifetime](references/guest-dma-lifetime.md) |
| Disconnect, reset or worker/QEMU failure | [Reset and failure](references/reset-and-failure.md) |
| Explain deployment choice or explicitly reassess it | [Virtualization models](references/virtualization-models.md) |

MetaFlux region/schema bytes have one canonical manifest; external vfio-user
framing stays a pinned adapter input. Validate ranges, permissions, fds, epochs
and quotas before side effects. Direct guest RAM needs mmap-capable backing;
the high-speed profile rejects socket-mediated region fallback.

Keep socket traffic on control/lifecycle operations. Steady-state submission
uses shared RAM and ioeventfd/MSI-X; implement and trace the real route without
provider syscalls/allocations, per-command interrupts or QEMU main-loop writes.
Message IDs are echoed sender values, may repeat concurrently, and are never
idempotence keys. `No_reply` suppresses a reply, not ordered side effects.

## Compose At The Crossing

[$pcie-vpci-device-model](../pcie-vpci-device-model/SKILL.md) skill owns PCI/BAR/MSI-X
presentation; [$linux-device-driver-uapi](../linux-device-driver-uapi/SKILL.md) skill
owns guest kernel mechanics. [$device-lifecycle-resilience](../device-lifecycle-resilience/SKILL.md) skill
alone allocates replacement identity. The static profile advertises neither
reset nor migration; coordinated reset advertises support only when its
authority-backed handler is ready.

Return the guest-observable behavior, ownership/unmap completion boundary and
scoped protocol/fault/real-guest evidence to parent review. Distinguish model,
live transport and guest compute results; do not repeat an unchanged matrix
instead of implementing its missing consumer.
