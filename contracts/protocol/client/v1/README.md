# Client Protocol v1

This directory owns provider/runtime negotiation and mapped-fastpath setup. The
protocol version is independent of the backend plugin ABI version.

Request and response objects are exactly 64-byte encoded buffers. Consumers must
use the supplied little-endian accessors; they never cast encoded bytes to native
records. Both messages carry magic, kind, header size, and total size. Reserved
bytes must be zero, unknown required capabilities fail negotiation, unknown
optional capabilities are ignored, and the highest overlapping version is chosen.

The successful response binds the selected protocol, shared-layout/ring ABI,
capabilities, and full `(daemon_incarnation, view_serial)` mapping incarnation.
Malformed, unsupported-version, unsupported-capability, and incompatible-view
outcomes are stable wire status values. New fields require a new sized message or
an explicitly negotiated capability; v1 reserved bytes never change meaning.

Control requests use bytes 12/14 for opcode/flags, 24 for request ID, 32/40 for
the full registry-view ID, 48 for object/context ID, and 56 for an operation
argument (size or generation). Responses use bytes 12/16 for status/flags, echo
the request and view IDs, then return object ID/generation at 48/56. Payload bytes
never enter this message. `PAYLOAD_FD` means exactly one SCM_RIGHTS descriptor in
the same seqpacket. The opcode defines its required descriptor type and flags:
host-memory, artifact, and argument-block payloads are memfds with the documented
size and seals, while host-address-space registration requires the documented
procfs descriptor identity and access mode. Absence, extras, truncation, or a
type-specific identity, access, size, or seal mismatch is rejected.

Capability bit 11 and control opcode 17 define `KERNEL_REQUEST_REGISTER`. A
client that depends on this path lists the capability as required, so an older
runtime rejects negotiation before request traffic begins. The operation carries
exactly one write-sealed payload FD, no other control flags, and the runtime
context ID at byte 48. Its payload is a 64-byte little-endian request header
followed immediately by the requested source bytes: magic at 0, version and
header size at 4/6, total size at 8, profile and operation at 16/20, operation
ABI version and Kernel IR schema version at 24/28, lifetime at 32, source offset
and size at 40/48, and zeroed reserved bytes elsewhere. The v1 baseline accepts
only the declared baseline profile and the closed
`MF_CLIENT_KERNEL_REQUEST_OPERATION_*_V1` set in `protocol.h`, with operation
ABI v1, Kernel IR schema v2, and `MODULE_LOAD` lifetime. Every newly accepted
operation receives a unique value and protocol validation coverage before a
provider may emit it. These values are ecosystem-neutral: the encoded request
contains no CUDA, PyTorch, MLIR, LLVM, Vulkan, target, or native-layout type.

The registered request becomes an immutable artifact. Its source bytes remain
valid through successful `MODULE_LOAD`; that operation validates and retains the
resulting canonical Kernel IR in the daemon-owned module. The client may release
the request artifact after module-load completion, while the module remains
valid until `MODULE_UNLOAD`. A request omitted from negotiation, malformed,
unknown, stale, or released before module load fails through the existing stable
capability, control, or ring status paths.

`MF_CLIENT_CAP_PROCESS_SNAPSHOT_V1` is requested only by observer sessions. Its
control response carries a fully sealed immutable payload FD. The payload has a
64-byte little-endian header followed by at most 64 fixed 128-byte process rows.
Rows bind PID plus process start time to an immutable device identity record and
generation, checked used-memory accounting, process kinds, and a bounded process
name. The canonical validator requires exact byte counts, zero reserved bytes,
known flags, unique PIDs, and rejects count or multiplication overflow.
Compute-session negotiation establishes credentials and accounting ownership but
does not by itself publish a process row when
`MF_CLIENT_CAP_LIVE_CONTEXT_ACCOUNTING_V1` is negotiated. Payload-free
`CONTEXT_ACQUIRE` and `CONTEXT_RELEASE` operations then maintain a checked
live-context count; snapshots publish a compute process only while at least one
context remains live. A new compute client that depends on this behavior lists
the capability as required, so a runtime without the behavior rejects the
negotiation instead of accepting a session with different publication semantics.

A v1 compute client that omits `MF_CLIENT_CAP_LIVE_CONTEXT_ACCOUNTING_V1` uses
the frozen legacy compatibility behavior: its process row is live from successful
negotiation until socket disconnect, and context-accounting control opcodes return
`MF_CLIENT_CONTROL_UNSUPPORTED` without closing the session. Observer sessions are
independent of this capability. Socket disconnect removes either kind of compute
session and its accounting regardless of its publication state.

`MF_CLIENT_CAP_COPY_REGION_V1` is an optional compute-session capability. When it
is negotiated, the client may submit COPY region argument blocks carrying
byte-granular source and destination offsets. A client requesting it still
negotiates successfully with an older runtime: absence of the bit preserves only
the zero-flag base-to-base COPY encoding, and the client must not publish a region
argument block or descriptor.

`MF_CLIENT_CAP_POLICY_SETTERS_V1` is optional for observer sessions. A negotiated
observer sends payload-free `DEVICE_SET_PERSISTENCE_MODE` or
`DEVICE_SET_COMPUTE_MODE` requests keyed by immutable `identity_record_id`. The
argument carries the canonical shared-device policy value. An `OK` response
returns that identity and the effective lifecycle sequence only after the daemon
has published and re-read the corresponding policy in the lifecycle fence.
Repeated requests for an already-effective value are idempotent and retain the
same lifecycle sequence. The peer must be root or the daemon owner; otherwise the
response is `NO_PERMISSION`.

Opcode 15 and capability bit 9 define `HOST_ADDRESS_SPACE_REGISTER` and
`DIRECT_HOST_COPY` for the milestone-0.1.0.0 direct-copy extension. A compute client that
negotiates the optional capability sends exactly one `PAYLOAD_FD | READ | WRITE`
registration for the runtime context, with a zero argument and an `O_RDWR`
descriptor opened from `/proc/self/mem`. The daemon accepts it only when its
device/inode identity matches `/proc/<SO_PEERCRED.pid>/mem`, its filesystem is
procfs, and no descriptor was previously registered for the session. The daemon
owns its received descriptor until disconnect. Failed or duplicate registration
does not replace the retained descriptor.

After successful registration, direct-host COPY descriptors carry exactly one
of `DIRECT_HOST_SOURCE` or `DIRECT_HOST_DESTINATION`. The target ID and argument
0 identify the device-memory object and generation; arguments 1, 2, and 3 carry
the host virtual address, device-memory byte offset, and nonzero byte count. The
daemon checks both address and object ranges and transfers directly with the
retained process-memory descriptor. A client that does not negotiate the bit,
cannot locally open its address-space descriptor, or receives an explicit
`UNSUPPORTED` or `INVALID_ARGUMENT` registration response continues to use
size-sealed host-memory payloads; no direct descriptor may be submitted in that
session. Transport, malformed-response, stale-view, permission, and internal
registration failures fail client initialization instead of silently selecting
the staged path.

Direct host addresses are meaningful only in the process address space bound by
the registered descriptor. The caller keeps a source range readable and
unchanged until its H2D command completes, and keeps a destination range writable
and unreused until its D2H command completes. The CUDA provider drains a direct
H2D command before returning from the copy call; asynchronous D2H completion is
consumed by the normal stream, event, or context synchronization boundary. The
daemon resolves each range when executing its one-shot descriptor and retains no
host address after that command completes. Unmapping or reusing a range before
completion violates this lifetime and a range that is invalid when executed
completes with `INVALID_ARGUMENT`; address reuse has no independent generation
protection.

Capability bit 10 and control opcode 16 define the local cdev binding extension.
A managed compute provider requests the bit before it opens a data cdev queue.
The successful `CDEV_BIND` request is payload-free, uses the runtime context ID
at byte 48, and carries the cdev device generation at byte 56. The daemon
accepts it only after the Unix control session and the caller-owned cdev queue
agree on the same registry view and generation, then binds the leased local
worker to that session. The Unix session remains the object-table control plane
while the cdev queue is the steady-state data plane. A provider commits one
transport for its initialization epoch: cdev absence or explicit ABI
incompatibility may select the pre-success memfd path, while permission,
malformed state, integrity, and policy errors remain terminal.

Host-memory registration retains a shared size-sealed memfd for the session.
Artifact and argument-block registration require write-sealed immutable memfds.
Artifact resolve returns another descriptor reference to the same immutable
content. Sender ownership is never transferred by SCM_RIGHTS; each receiver owns
and closes its installed descriptor.
