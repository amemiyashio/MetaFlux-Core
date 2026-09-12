---
name: linux-device-driver-uapi
description: Implement or review Linux 6.12 or 6.18 character-device UAPI, ioctl and mmap behavior, kref and VMA lifetime, long-term page pinning, DMA, eventfd, MMIO barriers, teardown, and kernel qualification. Use for milestone-0.1.1.0 transport and milestone-0.1.2.0 kernel lifecycle-adapter work. Do not use for vfio-user wire protocol or PCI configuration-space design.
---

# Linux Device Driver UAPI

Own Linux cdev/UAPI and kernel buffer lifetimes. First trace the affected
userspace operation in `transports/cdev/client/src/cdev.c` to its handler in
`linux-kernel-drivers/core/metaflux_core_main.c`. Name the fd, VMA or pinned
buffer whose behavior changes; implement its successful path and unwind
together. A close is not proof that its VMAs or callbacks are gone.

Analysis/review requests stay read-only; implementation steps apply to requested
changes. Use the assignment and Exit Gate through [$main](../main/SKILL.md) skill.
Reuse the frozen schema and target-kernel matrix. Select the relevant readings
below before editing; [implementation guidance](../review/references/implementation-guidance.md)
keeps focused checks tied to an actual implementation uncertainty.

## Select The Work

| Task | Read before changing that boundary |
| --- | --- |
| Trace a cdev/worker operation or choose source/tests | [Implementation path](references/implementation-path.md) |
| ioctl, compat, mmap or generated public bytes | [UAPI compatibility](references/uapi-compatibility.md) |
| fd/VMA references, revoke, remove or finalization | [Object lifetime](references/object-and-vma-lifetime.md) |
| Pin/map/unmap, DMA, ring or notification ordering | [DMA and ordering](references/dma-pinning-ordering.md) |
| Kernel version/shim changes or final qualification | [Kernel qualification](references/kernel-qualification.md) |

Keep fixed-width UAPI definitions in `contracts/uapi/linux/`; generate all
consumers from the owning manifest. Later extensions import the frozen base;
never add an extension definition retroactively to its base allowlist.
Probe the exact target kernel's APIs for shims and build with its Kbuild using
Nix-provided tools. Privileged live work uses
[$manage-host-privilege](../manage-host-privilege/SKILL.md) skill.

Implement ring publication and armed waits without per-command interrupts.
Preserve the warm path's no-syscall/allocation/global-lock design and every
pin/map/drain/unmap/unpin ownership edge; performance work retains correctness.

## Compose At The Crossing

Use [$runtime-contracts-registry](../runtime-contracts-registry/SKILL.md) skill
for shared contract changes, [$device-lifecycle-resilience](../device-lifecycle-resilience/SKILL.md) skill
for authoritative generation transitions, [$pcie-vpci-device-model](../pcie-vpci-device-model/SKILL.md) skill
for config/BAR/MSI-X presentation, and
[$gpu-virtualization-vfio-user](../gpu-virtualization-vfio-user/SKILL.md) skill
for the vfio-user wire. Kernel code consumes these contracts; it implements
neither CUDA semantics nor tensor execution.

Return the changed operation, exact lifetime/errno boundary and affected
native/compat or race evidence to parent review. Select the relevant final
matrix once; a module load or one happy ioctl does not qualify the UAPI.
