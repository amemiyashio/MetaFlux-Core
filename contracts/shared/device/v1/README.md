# Shared Device Layout v1

This directory is the canonical owner of the mmap-visible milestone-0.1.0.0 registry, device,
telemetry, handle, and memfd-ring layouts. Consumers include
`metaflux/shared/device.h`; they do not reproduce field lists or offsets.

All records use fixed-width integers, byte arrays, offsets, and explicit 64-byte
alignment. The mapped representation contains no pointers, `size_t`, native enum,
language boolean, C/C++ atomic object, or packed atomic. Concurrent 32/64-bit words
are accessed only through `metaflux/shared/atomic.h`, which requires lock-free
aligned compiler atomics and therefore never adds `libatomic`.

## Canonical layout

| Record | Size | Alignment | Ownership |
| --- | ---: | ---: | --- |
| `mf_shared_registry_header_v1` | 128 | 64 | Segment sizes, counts, offsets, full view ID |
| `mf_shared_registry_extension_header_v1` | 256 | 64 | Recovery capacities and canonical absolute offsets |
| `mf_virtual_device_identity_v1` | 256 | 64 | Immutable identity and committed generation |
| `mf_view_admission_control_v1` | 64 | 64 | Atomic view validation generation/state |
| `mf_registry_view_control_v1` | 128 | 64 | Odd/even control latch, revision, gate, range cursors |
| `mf_device_admission_control_v1` | 64 | 64 | Atomic device validation generation/state/update tag |
| `mf_virtual_device_lifecycle_fence_v1` | 64 | 64 | Odd/even lifecycle fence payload |
| `mf_telemetry_control_v1` | 64 | 64 | Odd/even double-bank selection |
| `mf_virtual_device_telemetry_v1` | 128 | 64 | Identity-keyed metrics row |
| `mf_view_publisher_control_v1` | 64 | 64 | Tagged exclusive view-publish owner |
| `mf_telemetry_publisher_control_v1` | 64 | 64 | Tagged exclusive telemetry-publish owner |
| `mf_admission_attempt_record_v1` | 128 | 64 | Pre-registered reservation quiescence attempt |
| `mf_admission_lease_record_v1` | 192 | 64 | View/device-generation-bound admission commit |
| `mf_device_validation_update_record_v1` | 192 | 64 | Exact tagged policy-update recovery link |
| `mf_view_publish_record_v1` | 256 | 64 | FIFO fence/control publication recovery plan |
| `mf_lifecycle_range_record_v1` | 192 | 64 | Immutable range and retired-suffix ledger |
| `mf_telemetry_publish_record_v1` | 256 | 64 | Double-bank publication recovery plan |
| `mf_generation_handle_v1` | 64 | 64 | Full-view and generation-bound object handle |
| `mf_argument_block_header_v1` | 64 | 64 | Immutable argument entry segment header |
| `mf_argument_entry_v1` | 32 | 8 | Scalar or generation-bound buffer argument |
| `mf_ring_header_v1` | 256 | 64 | Four isolated metadata/cursor/wait cache lines |
| `mf_ring_descriptor_v1` | 64 | 64 | Per-slot sequence and command payload |

Ring attach validates the complete metadata cache line and snapshots its capacity,
view ID, queue ID, and queue generation into the process-local client handle.
It also requires grow, shrink, and seal seals on the backing memfd and rejects
nonzero v1 metadata flags.
Subsequent submit, consume, wait, and queue-control operations use that trusted
snapshot; only cursors, wait controls, descriptor sequences, and descriptor
payloads remain live shared state. A peer-side metadata change therefore cannot
change an attached handle's bounds or identity.

`mf_registry_view_id_v1` is the pair `(daemon_incarnation, view_serial)` and is
never reused. The device admission tuple packs a 24-bit checked generation, an
8-bit state, and a 32-bit update tag into one lock-free 64-bit word. The terminal
generation is permanently reserved; normal transitions stop one value earlier.
The bounded recovery tables use the same rule for 24-bit record tags and slots.
Each record's tag, state, and state-specific auxiliary tag/slot are compared in
one 64-bit atomic word, so a stale owner cannot act on a reclaimed physical slot.
`mf_shared_checked_add_u64_below_terminal_v1` performs whole-range allocation
without consuming the terminal encoding or partially advancing a caller-owned
counter.

The v1 legacy prefix ends immediately after telemetry bank 1. A mapping with
`header.flags == 0` ends there and remains attachable. A mapping with
`MF_SHARED_REGISTRY_FLAG_RECOVERY_TABLES_V1` places the extension header at that
exact boundary, followed by the two publisher controls and the attempt, lease,
device-update, lifecycle-range, view-publish, and telemetry-publish tables. Both
runtime and client recompute every offset from the device count and reject
unknown flags, non-canonical offsets, capacity changes, misalignment, or checked
arithmetic overflow.

Each telemetry row carries independent `active_time_ns` and
`memory_active_time_ns` measurements. Both are monotonic-duration totals for the
publisher's sampling window; neither memory activity nor utilization is inferred
from `memory_used_bytes`. The final seven words are reserved and remain zero.

The lifecycle fence's `policy_bits` word canonically stores persistence in bit 0
and the two-bit compute mode in bits 1-2. Compute values are default, exclusive
thread, prohibited, and exclusive process in numeric order 0-3. Writers preserve
unknown bits, publish policy through the device-validation update protocol, and
acknowledge a setter only after a stable fence read confirms the requested value.

milestone-0.1.0.0 fixes table capacities at 16 attempts, 32 leases, 16 device updates, 16
lifecycle ranges, 18 view publications, and 9 telemetry publications. The last
two view-publication slots are reserved for closing and terminal publication;
the last telemetry-publication slot is reserved for terminal publication. Those
reserved records use the permanent terminal tag and are never admitted to the
ordinary reuse scan.

`mf_owner_identity_v1` stores a Linux PID together with `/proc/PID/stat` field 22
(process start-time ticks). Recovery compares both values. A deadline expiring
while that exact owner can still run quarantines the view; only confirmed owner
death permits compensation or tombstoning.

The ring is a bounded multi-producer/multi-consumer queue. Producer and consumer
cursors occupy different cache lines and are claimed with lock-free compare-and-
exchange tickets. A producer owns exactly one slot after its cursor CAS, stores
the payload, and release-stores the ticket's published sequence. A consumer owns
exactly one published slot after its cursor CAS, reads the payload after the
sequence acquire, and release-stores the ticket plus capacity as the next reusable
sequence. Sequence comparisons use modulo-64-bit distance with capacity bounded
below half the counter range, so ordinary cursor wrap does not alias a live slot.

The wait line stores producer and consumer waiter counts plus futex sequences and
doorbell counters. A waiter increments its count before the final readiness check;
a publisher that observes a nonzero count advances the corresponding futex
sequence and wakes one peer. The recheck closes the publish-before-sleep race, and
each successful publish or consume performs at most one wake syscall.

Argument blocks are immutable sealed memfds. A `U32` or `U64` entry carries its
scalar in `value` and has zero flags/object fields. A `BUFFER` entry carries an
object ID, object generation, byte offset, and explicit read/write access. The
header fixes entry count/size and exact total size. With header flags zero, all
four reserved words remain zero and legacy callers retain their original
behavior. The
`MF_ARGUMENT_BLOCK_FLAG_LAUNCH_DIMENSIONS_XY_V1` flag assigns the four words, in
order, to `grid_x`, `grid_y`, `block_x`, and `block_y`; each is a nonzero
`uint32_t` value. The CPU execution subset fixes both z dimensions at one, so
ecosystem adapters reject any other z value before registering the block.
The `MF_ARGUMENT_BLOCK_FLAG_COPY_REGION_V1` flag keeps all reserved words zero
and permits byte-aligned `BUFFER` offsets. Its exact three-entry shape is a
writable destination buffer, a readable source buffer, and a nonzero `U64` byte
count. Unknown kinds, flag combinations, invalid launch dimensions, unaligned
non-copy offsets, and trailing bytes are malformed.

A COPY descriptor with exactly one of
`MF_RING_COPY_FLAG_DIRECT_HOST_SOURCE_V1` or
`MF_RING_COPY_FLAG_DIRECT_HOST_DESTINATION_V1` targets device memory directly.
Argument 0 is its generation; arguments 1, 2, and 3 are the host virtual
address, device byte offset, and nonzero byte count. These flags are mutually
exclusive with each other and with the copy-region argument-block flag.
