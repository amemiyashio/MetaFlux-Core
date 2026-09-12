---
name: linux-device-driver-uapi
description: Implement or review Linux 6.12 or 6.18 character-device UAPI, ioctl and mmap behavior, kref and VMA lifetime, long-term page pinning, DMA, eventfd, MMIO barriers, teardown, and kernel qualification. Use for milestone-0.1.1.0 transport and milestone-0.1.2.0 kernel lifecycle-adapter work. Do not use for vfio-user wire protocol or PCI configuration-space design.
---

# Linux Device Driver UAPI

## Implementation Focus

For an implementation request, use the shared
[implementation guidance](../review/references/implementation-guidance.md).
Select the affected inputs and obligations below; broad qualification lists
do not make every invocation a new inventory or full-suite run.

Choose the affected userspace operation and follow its fd/VMA/buffer ownership
through the kernel to completion or teardown. Implement the operation and its
unwind together, reusing established UAPI projections and version shims.
Model the relevant race before coding; use targeted lifetime/fault checks during
repair and the required kernel matrix for final qualification, rather than
rebuilding every kernel after each local edit.

## Inputs

- The active milestone-0.1.1.0/milestone-0.1.2.0 work item, target Linux/Kbuild matrix, canonical
  transport-envelope schema manifest, Linux UAPI projection, and native/compat
  callers.
- Object ownership graph, fd/VMA/mapping/queue/eventfd/worker lifetimes, DMA
  directions, ordering protocol, quotas, and fault model affected by the task.
- Existing KUnit, userspace ABI, KASAN/KCSAN/lockdep/kmemleak, and teardown
  evidence.

Do not copy internal kernel structures into UAPI. Use fixed-width, explicitly
sized records and compile-probed compatibility shims for supported kernels.

## Routing

- Use [UAPI compatibility](references/uapi-compatibility.md) for cdev, ioctl,
  compat, extension, and mmap contracts.
- Use [object and VMA lifetime](references/object-and-vma-lifetime.md) for krefs,
  tombstones, close/remove races, and teardown.
- Use [DMA, pinning, and ordering](references/dma-pinning-ordering.md) for
  `FOLL_PIN`, SG/DMA ownership, eventfd, barriers, and unmap drain.
- Use [kernel qualification](references/kernel-qualification.md) for supported
  kernel builds, dynamic analysis, fuzzing, and evidence.
- Route vfio-user negotiation to `$gpu-virtualization-vfio-user`, PCI config and
  BAR/MSI-X presentation to `$pcie-vpci-device-model`, and cross-transport reset
  state to `$device-lifecycle-resilience`.

## Workflow

1. Draw the object/refcount graph and name the authority, lock, generation, and
   terminal state for every fd-, VMA-, mapping-, queue-, event-, and worker-owned
   object before changing code.
2. Update the zone-owned Linux UAPI definitions in the selected manifest closure.
   Definitions on the frozen milestone-0.1.1.0 base allowlist remain referenced exactly once
   by its base manifest; later lifecycle definitions are referenced by their own
   extension manifest and never retroactively added to the base. Generate the
   fixed-width size/version, flags, reserved-zero policy, extension namespace,
   limits, compat layouts, and byte fixtures; never make a kernel-private struct
   or handwritten duplicate the normative layout. Define exact errno and
   overflow behavior for every rejection.
3. Specify `open`, ioctl, `mmap`, `poll`/wait, eventfd registration, close,
   remove, daemon death, and module-unload transitions including concurrent
   interleavings.
4. For each userspace buffer, choose pin API, long-term/write flags, accounting,
   DMA direction, SG mapping, synchronization, dirtying, drain, unmap, and unwind
   order. Reject unsupported memory rather than downgrading silently.
5. Encode ring publication/consumption and doorbell/completion ordering with the
   project atomic and DMA/MMIO barrier contract. Separate polling from armed
   waits and avoid per-command interrupts.
6. Keep kernel-version differences in compile-probed compatibility shims. Create
   their source home only with real implementation, following
   [Roadmap Homes](../../../docs/roadmap.md#kernel-compatibility-shims).
   Never weaken ownership or ordering based on a version check alone.
7. Add fault injection and concurrent teardown tests before performance work.

## Output

Select the applicable outputs for the requested task:

- The canonical schema projection plus generated UAPI/errno/compat table,
  native/compat layout fixtures, and object/refcount state diagram.
- A pin/map/sync/drain/unmap/unpin ledger for each buffer class.
- Explicit ordering pairs for descriptor, doorbell, completion, timeline, and
  event notification.
- Kernel qualification results across the pinned matrix, with unresolved ABI
  decisions clearly marked as pre-freeze.

## Verification

- Build with each supported target Kbuild toolchain and `LLVM=1` where the
  target configuration supports it; run native and compat ABI/layout tests.
- Exercise short size, unknown extension/flag, reserved bits, nulls, overflow,
  bad offset/width, stale generation, permission, and interrupted wait cases.
- Verify the selected manifest references each definition in its own allowlist
  once, extension imports preserve the frozen base hash, and kernel, C17, C++20,
  native, and compat generated byte/offset fixtures agree.
- Race open/mmap/ioctl/poll/close/unmap/remove/worker death under KUnit, KASAN,
  KCSAN, lockdep, and kmemleak as applicable.
- Prove every page and DMA mapping unwinds exactly once on every injected failure
  and no successful unmap leaves a server/backend/callback reference.
- Measure the active cdev path only after tracing proves no warm enqueue syscall,
  allocation, or global lock.
