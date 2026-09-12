# Session Objects and Completion Lifetime

Use [server.cpp](../../../../services/metafluxd/src/server.cpp) as the first
implementation map: `Session`, `Object`, `resolve`, `add_module`, `release_object`,
`Session::control`, `handle_control_packet`, and `process_launch`. The
[service overview](../../../../services/metafluxd/README.md) explains negotiated
resource limits and drain behavior. Read the actual handler when its current
implementation and a prose summary disagree.

## Implement one complete transition

Follow the request's peer/session identity into object reservation and ownership.
For creation, identify the first externally visible success, then ensure every
earlier failure releases mappings, backend handles and reservations. For release,
find outstanding references and the operation that makes reuse legal. Current
object IDs are monotonic within a session; retained dead entries are not free
slots to reuse without a reviewed generation policy.

Kernel-request registration retains a sealed artifact and source range. Module
load retains canonical Kernel IR and prepared execution state; later artifact
release does not unload the module. Argument blocks refer to separately checked
memory objects. Reuse the canonical
[request-lifetime contract guide](../../runtime-contracts-registry/references/kernel-request-lifetime.md)
instead of declaring private versions or offsets here.

`process_launch` validates module and argument handles, argument header, buffer
access, offsets and alignment before entering the backend. Preserve these
checks when caching argument metadata; changing a mutable buffer reference is
not equivalent to reusing immutable module metadata. A concurrent or multi-stream
change needs an explicit owner for in-flight argument storage and completion.

## Cancellation is not immediate reclamation

Observe cooperative cancellation at existing dispatch/interpreter boundaries.
A compiled CTA already in its non-preemptible entry runs to return. Process
retirement can close admission and bound completion publication without freeing
that entry's module, buffers, telemetry or per-UID reservations early. Separate
socket retirement, queued-work cancellation, backend completion and final release.

Choose checks for the changed failure: stale/cross-type handles, failed creation,
release while work is live, full completion ring, peer disappearance, quota return
or repeated teardown. Existing sources include
[admission tests](../../../../services/metafluxd/tests/admission_test.c),
[process snapshots](../../../../services/metafluxd/tests/process_snapshot.c),
and [client integration](../../../../services/metafluxd/tests/client.c).
Compose the lifecycle expert when the change crosses the shared device authority;
a local lifetime repair does not reopen an already-closed device-model decision.
