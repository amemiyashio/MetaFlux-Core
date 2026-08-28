# Reset and Hotplug

## Reset

Distinguish bus/device reset requests, vfio-user device reset, lifecycle reset,
and capability-level FLR. Do not advertise FLR until its exact config capability
and semantics exist. In M0002, reset fences work and ends in `LOST`; recovery
requires a fresh static instance.

In M0003 coordinated reset, PCI presentation mirrors lifecycle states:

```text
ONLINE -> QUIESCING -> DRAINING -> RESETTING -> ONLINE(new generation) | LOST
```

Mask notifications and reject new accesses before old resources retire. A new
generation becomes visible only after config/presentation, transport, registry,
and worker owners are staged and the authority commits it.

## Remove and re-add

- Remove live lookup/admission first, then unregister the current `pci_dev` and
  emit coherent sysfs/uevents.
- Old config callbacks, BAR mappings, IRQ/eventfd paths, cdev fds, VMAs, DMA
  mappings, and handles remain old-generation tombstones.
- Re-add allocates a never-reused generation and repeats identity/binding setup.
  Avoid duplicate BDF/function publication under concurrent rescan.
- Idempotent duplicate requests do not allocate identity or emit contradictory
  events.

## Qualification

Race reset/remove/re-add against config reads/writes, `lspci`, sysfs rescan,
open/mmap/submit, IRQ delivery, driver probe/remove, server disconnect, and
module unload. Assert one live function/owner, consistent UUID/BDF/generation,
no use-after-free or hung task, and no stale completion after replacement.

QMP event sequencing is owned by `$device-lifecycle-resilience`.
