# Static vfio-user Transport

The guest half is a C17 encoder for the generated MetaFlux message, DMA map, and
DMA unmap records. The server half is a C++20 `SOCK_SEQPACKET` adapter that
negotiates a static Type-0 profile, reports BAR0 (64 KiB), BAR2 (4 KiB), and BAR4
(4 KiB with two MSI-X vectors), and owns a generation/epoch-bound DMA mapping
ledger.

The server keeps vfio-user framing at the boundary and never treats its message
IDs as unique. IDs are echoed on every reply, may be reused, and `No_reply`
suppresses only the reply. DMA maps require page-aligned, mmap-capable file
descriptors and reject overflow, overlap, stale generation/epoch, and permission
errors. Unmap removes the mapping before release; this fixture has no in-flight
backend references, so a successful reply is the zero-reference point.

M0110 deliberately advertises neither reset nor migration. A reset or doorbell
message received on the control socket returns `MF_SHARED_NOT_SUPPORTED`; the
steady-state doorbell/ring path and `metaflux_pci.ko` remain the next W0113
implementation stage.
