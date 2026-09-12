# Implement A Kernel Operation

Read this when choosing the first edit; paths are repository-relative. Inspect
the current function and caller with `rg` before fixing a source location.

| Boundary | Source to inspect | Nearest evidence |
| --- | --- | --- |
| Userspace cdev request | `transports/cdev/client/src/cdev.c` | `transports/cdev/client/tests/cdev_test.c` |
| fd, VMA, ioctl, worker broker | `linux-kernel-drivers/core/metaflux_core_main.c` | `linux-kernel-drivers/tests/kselftest/cdev_qualification.c` |
| Generation guards | `linux-kernel-drivers/core/mf_cdev_generation.h` | `linux-kernel-drivers/tests/kunit/` |
| Worker mapping and lifetime | `transports/cdev/worker/src/worker.cpp` | worker lifecycle/rebind tests in the adjacent `tests/` |
| Guest PCI ring/IRQ consumer | `linux-kernel-drivers/pci/metaflux_pci_main.c` | guest/kernel evidence required by the assigned work item |

## Implement The Ownership Change

1. Follow one request with its actual fd/VMA/buffer to completion. Write the
   before/after result and name the first missing handler or reference edge.
2. Read [UAPI](uapi-compatibility.md) for changed public bytes and
   [lifetime](object-and-vma-lifetime.md) for changed owners. Name the authority,
   lock, generation, terminal state and finalizer for affected objects.
3. Implement the success path plus partial setup, interrupted wait and teardown
   outcomes that it can reach. For buffers use the full
   [DMA ledger](dma-pinning-ordering.md), including quota, dirtying and unwind.
   Model the relevant race before coding; do not add a local identity authority.
4. Build the affected target and use the nearest check to answer the particular
   lifetime/ABI uncertainty. Qualification models are useful but do not stand in
   for live ioctls, mappings or a supported target-kernel build.
5. Return the actual diff and observed result for parent review, then select
   the required final [kernel matrix](kernel-qualification.md) once.

Kernel-version differences use compile-probed shims against the pinned target,
not guesses from a broad version number. A shim's source home is created only
with real implementation, following
[Roadmap Homes](../../../../docs/roadmap.md#kernel-compatibility-shims).

For zero-overhead work, locate the actual enqueue, arm/recheck and completion
path. Move invariant setup out of it while preserving DMA/MMIO order and
bounded revocation. Trace absence of warm syscalls, allocations, global locks
and per-command interrupts before measuring the same path; model timing is not
a kernel data-plane latency result.
