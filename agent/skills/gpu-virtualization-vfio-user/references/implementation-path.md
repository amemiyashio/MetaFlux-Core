# Implement The Guest Transport Path

Read this before selecting a transport source change. All paths are
repository-relative; confirm the actual call site with `rg`.

| Consumer or mechanism | Implementation | Evidence entry |
| --- | --- | --- |
| C17 guest encoder and ring | `transports/vfio-user/guest/src/guest.c` | guest `tests/guest_test.c`, `ring_backend_test.c` |
| C++ server framing and DMA ledger | `transports/vfio-user/server/src/server.cpp` | server `tests/server_test.cpp`, `fault_matrix_test.cpp` |
| Server MSI-X and QMP | server `src/msix.cpp`, `qmp_socket.cpp`, `qmp_lifecycle.cpp` | corresponding server tests |
| Live pinned QEMU/libvfio-user adapter | `transports/vfio-user/live/src/metaflux_vfu_live_server.c` | live `tests/run_vfio_user_live_bringup.py` |
| Guest kernel BAR/ring/IRQ | `linux-kernel-drivers/pci/metaflux_pci_main.c` | assigned guest/kernel qualification |

## Choose A Reachable Slice

Identify the active consumer first: the C++ protocol fixture and the C live
adapter are distinct paths. A change to one does not prove the other uses it.
Trace one request from that consumer through negotiation, mapping, descriptor
lookup and completion; choose the first missing behavior in the assigned scope.

Reuse the exact QEMU/libvfio-user/profile inputs. Standard protocol negotiation
and MetaFlux readiness are separate: accepted direct maps need mmap-capable fds,
file-backed guest RAM with `share=on`, valid ranges and permissions. BAR0 is
ready only after the required region/IRQ/memory profile is accepted.

Implement the operation with its fd/map/reference cleanup. For unmap, remove
lookup before drain and acknowledge only after queue/backend/callback/host
references are gone; deadline failure reaches loss and closes without false
success. Keep generation and IOVA epoch checks on actual descriptor lookup.

A guest-compute change follows the shared ring and actual backend completion,
not a new control-socket doorbell. Preserve the no-socket/QEMU-main-loop/
provider-syscall/allocation/per-command-interrupt warm path. Distinguish a
transport fixture, live guest execution and any physical backend claim.

## Select Evidence For The Change

Use affected protocol/DMA/IRQ tests during implementation. Final qualification
covers the assigned pinned pairs and requires the affected positive, malformed,
message-ID reuse, `No_reply`, unmap/deadline and stale-completion cases. Rebuild
the canonical manifest projections and byte fixtures when their inputs change.
Model tests alone do not establish unmodified guest identity and Add/Copy.

For a new reset/loss path read [reset and failure](reset-and-failure.md) and
compose the lifecycle owner; static terminal loss and coordinated replacement
remain different supported profiles.
