# Client Fast Path

This C17 component is the application-side owner of local daemon bootstrap,
mapped registry reads, and bounded MPMC submission/completion rings. It has no C++,
compiler, backend, or daemon link dependency.

`mf_client_session_connect_default_v1` resolves `METAFLUX_SOCKET`, then
`$XDG_RUNTIME_DIR/metafluxd.sock`, then `/run/user/<uid>/metafluxd.sock`. A
successful 64-byte negotiation receives exactly three SCM_RIGHTS descriptors in
registry/submission/completion order and attaches all three to the negotiated full
view ID. The session owns its socket, duplicated FDs, and mappings. Borrow APIs do
not transfer ownership. Registry attach is read-only; identity, fences, and
telemetry remain daemon-owned, while the two MPMC ring mappings remain writable.
Compute-session bootstrap requires `MF_CLIENT_CAP_LIVE_CONTEXT_ACCOUNTING_V1`, so
the CUDA provider never enters a session whose process-publication semantics are
the legacy connection-lifetime behavior. Observer bootstrap remains independent
of that capability. Compute bootstrap requests `MF_CLIENT_CAP_COPY_REGION_V1`
and `MF_CLIENT_CAP_DIRECT_HOST_COPY_V1` as optional. Callers inspect
`negotiated_capabilities` before publishing either extension. A runtime without
those bits retains the zero-flag base-to-base COPY path and size-sealed host
payloads.
Close shuts down the socket first so the daemon can release session state, then
unmaps completion, submission, and registry state. The canonical registry view
itself remains live for other sessions until daemon shutdown.

Registry attach validates one nonzero `process_view_revision` in both the
registry header and view-control record, then captures that revision in the
process-local handle. `mf_client_registry_process_view_revision_v1` returns the
captured value; it does not reread mutable provider state. A provider's
membership snapshot therefore remains tied to its initialization epoch and the
exact view revision it joined. A later mapping or a later zero-to-one provider
initialization may capture a newer revision, while an already initialized
provider keeps its prior membership and ordinal set.

Control calls are serialized request/response pairs. Payload helpers retain their
own mapping and FD after a send; immutable payloads are write-sealed, while
writable host payloads remain shared and size-sealed. Ring submission is
independent of the control socket and remains the warm path. A control helper
return of `MF_SHARED_SUCCESS` reports a valid transport response; the operation
outcome is the stable `MF_CLIENT_CONTROL_*` status in response byte 12. An
SCM_RIGHTS descriptor returned through `out_received_payload_fd` is newly owned by
the caller.

The batch ring entry point reserves the complete descriptor range with one
producer-cursor CAS before publishing any slot. Capacity failure therefore
leaves the producer cursor and every descriptor unchanged, even when other
producers race with the caller; a successful batch wakes consumers once.

## Descriptor arguments

| Opcode | `target_id` | `arguments[0..3]` |
| --- | --- | --- |
| `MEMORY_ALLOC` | context | bytes, alignment, 0, 0 |
| `MEMORY_FREE` | memory | generation, 0, 0, 0 |
| `MODULE_LOAD` | artifact | generation, 0, 0, 0 |
| `MODULE_UNLOAD` | module | generation, 0, 0, 0 |
| `COPY` (`flags == 0`) | destination memory | destination generation, source ID, source generation, bytes |
| `COPY` (`REGION_ARGUMENT_BLOCK`) | copy argument block | argument-block generation, 0, 0, 0 |
| `COPY` (`DIRECT_HOST_SOURCE/DESTINATION`) | device memory | device generation, host virtual address, device byte offset, bytes |
| `LAUNCH` | module | module generation, kernel ID, argument-block ID, argument-block generation |
| `EVENT_RECORD/WAIT` | event | event generation, timeline, 0, 0 |
| `QUEUE_SYNCHRONIZE/CANCEL` | queue | timeout, 0, 0, 0 |
| `COMPLETION` | result object | status, result generation, timeline, detail |

Every descriptor is also bound to the ring's full view ID and queue ID/generation.
Object IDs are meaningful only with their returned generation and that view.
The copy-region argument block is immutable and contains exactly a writable
destination `BUFFER`, a readable source `BUFFER`, and a nonzero `U64` byte count.
Its buffer values are byte offsets; checked ranges must fit both objects. The
zero-flag COPY encoding remains the base-to-base compatibility path.
The direct-host helper accepts exactly one direction flag and rejects zero or
overflowing host/device ranges before publishing. The client must first register
one `O_RDWR` `/proc/self/mem` descriptor through
`HOST_ADDRESS_SPACE_REGISTER`; the daemon retains its SCM_RIGHTS duplicate for
the session.
