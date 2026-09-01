# Static vfio-user Transport

The guest half is a C17 encoder for the generated MetaFlux message, DMA map, and
DMA unmap records. The server half is a C++20 `SOCK_SEQPACKET` adapter that
negotiates a static Type-0 profile, reports BAR0 (64 KiB), BAR2 (4 KiB), and BAR4
(4 KiB with two MSI-X vectors), and owns a generation/epoch-bound DMA mapping
ledger.

The guest ring adapter now attaches the paired shared-memory submission and
completion queues through `mf_vfio_user_guest_ring_attach_v0`. It reuses the
generated `mf_ring_header_v1` layout and fastpath atomic publication, checks
queue IDs, registry-view identity, generation, and equal capacity, and exposes
payload-bound checks plus completion polling/waits. A successful submission
invokes the injected BAR2 doorbell callback; backpressure and malformed input
never publish a doorbell. The callback is a host-independent seam for the
future kernel MMIO path, not physical NVIDIA or QEMU qualification.

The server keeps vfio-user framing at the boundary and never treats its message
IDs as unique. IDs are echoed on every reply, may be reused, and `No_reply`
suppresses only the reply. DMA maps require page-aligned, mmap-capable file
descriptors and reject overflow, overlap, stale generation/epoch, and permission
errors. Unmap removes the mapping before release; this fixture has no in-flight
backend references, so a successful reply is the zero-reference point.

milestone-0.1.1.0 deliberately advertises neither reset nor migration. A reset or doorbell
message received on the control socket returns `MF_SHARED_NOT_SUPPORTED`; the
steady-state doorbell/ring path and `metaflux_pci.ko` remain the next work-item-0.1.1.3
implementation stage.

The C++ server can register its `metaflux::runtime::lifecycle::Mirror` with the
milestone-0.1.2.0 coordinator. Lifecycle quiesce rejects new control work, reset commits
only after the DMA mapping ledger is empty, and commit advances the server's
device generation and mapping epoch together. Map and unmap requests carrying a
retired pair return `MF_SHARED_STALE_HANDLE`; work after a lifecycle loss
returns `MF_SHARED_DEVICE_LOST`. Socket disconnects mark the local server lost;
the owning coordinator remains responsible for submitting the normalized loss
request. The mirror does not add a reset wire message or alter the generated
vfio-user profile. After an EOF or socket error, a caller that owns the
coordinator can use `mark_lost_and_submit` with a captured `Disconnect` event;
the server marks its local state lost first, then routes the event through the
stateless ingress. The caller still supplies the logical device, daemon
incarnation, identity, generation, epoch, and request ID. The three-argument
`process_once` overload combines EOF/error detection with that handoff; normal
messages retain the original `ServerResult` and do not submit a lifecycle event.

`server/qmp_lifecycle.hpp` provides the cold-control QMP command/event
correlation fixture. It permits one pending command, requires the matching
`device-added` or `device-deleted` event, and emits a normalized request only
after that match. A failed remove emits the existing QMP transport-loss
operation so the coordinator can publish `LOST`; a failed add emits no device
request. `QmpSocket` now supplies the bounded live Unix-stream layer: it owns a
connected `SOCK_STREAM` fd, sends one command object with a non-zero numeric ID,
frames one top-level JSON object up to 64 KiB, and classifies QMP greetings,
replies, errors, device events, closure, and malformed input. It does not own
lifecycle identity, generation/epoch allocation, command retry, or event
publication. `QmpLifecycleAdapter::receive_and_submit` composes one socket
receive with correlation and the stateless lifecycle ingress, returning a
separate `{transport, lifecycle}` outcome. Unrelated or wrong-kind events keep
the adapter pending; malformed, timeout, and closed-socket results never mutate
coordinator state. The `QmpResult` reports correlation status while
`ResultDetails` reports the coordinator's authoritative outcome. Production
QEMU producer wiring and qualification remain open.

The work-item-0.1.1.4 bounded fault matrix covers short and flag-invalid packets, payload
size mismatches, stale exact unmaps, DMA address overflow, duplicate ranges,
and a valid message after malformed input. These faults retain the configuring
state or return a stable shared status; they do not reuse a retired mapping.
Socket disconnect, live backend-reference draining, MSI-X behavior, and the
native/compat protocol matrix remain open qualification gates.
