# vfio-user Protocol

## Ownership and framing

The external vfio-user wire format comes only from the pinned QEMU/libvfio-user
revision. It is host-native adapter input and is not copied into the MetaFlux
schema. MetaFlux regions, capabilities, BAR records, and extension bytes come
from the M0002 root manifest at
`contracts/protocol/transport/v1/schema/manifest.json`; server-local structs are
not normative.

## Connection state

Model `Disconnected -> Negotiating -> Configuring -> Running -> Quiescing ->
Lost/Closed`. Record which side owns the transition, which messages are legal,
and which resources may exist in each state. Do not expose the PCI function as
ready before required features and memory behavior are accepted.

## Negotiation checklist

- protocol major/minor and capabilities;
- maximum message/fd counts and reply/error behavior;
- region inventory, sizes, flags, mmap capability, sparse areas, and access size;
- IRQ sets, vector count, eventfd masks, and notification semantics;
- DMA map/unmap flags, fd/offset rules, address width, dirty/migration capability;
- reset and migration advertisement bits; M0002 advertises neither;
- shared-memory prerequisite and rejection of slow region fallback;
- unknown command/capability handling and clean rollback after partial setup.

Every message parser validates header size, command, flags, request/reply role,
payload length, fd count, integer overflow, range, and connection state. Treat
received fds as resources requiring close on every error path.

The 16-bit message ID belongs to its sender, may be reused even while another
message with the same ID is outstanding, and is echoed in the reply. The receiver
must not assume uniqueness or use it to reject, deduplicate, or make an operation
idempotent. Client-to-server commands execute in receive order. `No_reply`
suppresses only the reply, not validation or side effects; processing a later
replied command therefore follows processing earlier `No_reply` commands. The
optional reverse command channel has its own order, so unrelated traffic in
opposite directions may interleave without establishing a global order.

## Data/control separation

The socket carries negotiation, DMA lifecycle, device state, and failure control.
BAR2 ioeventfd and shared rings carry submission; MSI-X/eventfds carry armed
completion/fatal notifications. Tracing must prove no steady-state command uses a
socket message or QEMU main-loop region write.

Primary source: [QEMU vfio-user protocol](https://www.qemu.org/docs/master/interop/vfio-user.html).
Pin the exact QEMU/libvfio-user revisions because master documentation moves.
