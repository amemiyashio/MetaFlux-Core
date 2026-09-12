# Shared Fast Path and Backend ABI

Read for mapped layouts, queue ordering, negotiation or backend calls. Start at
[client fastpath.c](../../../../runtime/client/fastpath/src/fastpath.c), its
[layout/queue contract](../../../../runtime/client/fastpath/README.md), or the
[backend API](../../../../contracts/plugin/backend/v1/include/metaflux/backend/api.h).

Mapped records use fixed-width integers, byte arrays, offsets, generation-bound
handles, and explicit padding. They contain no pointers, `size_t`, native enums,
language booleans, C++ objects, or language-specific atomic types. C and C++ use
the project atomic wrapper over aligned lock-free operations; builds reject a
hidden `libatomic` dependency.

Queue specifications name the single publication owner, per-slot sequence and
wrap arithmetic, release/acquire edges, cache-line ownership, armed/sleeping
handshake, and transport-specific wake primitive. Apply the syscall limit from
the exact active work item and distinguish a polling active queue from a worker
that has entered a blocking wait.

Seqlock payload is not plain C/C++ memory: every concurrent field is represented by
naturally aligned 32/64-bit words and accessed through `mf_atomic_*` (relaxed only
inside the seqlock wrapper). The begin/end helpers provide compiler and hardware
barriers; subword values share an atomic containing word. Concurrent structure
copy or `memcpy` is forbidden, avoiding data-race UB even when a reader retries.
The independently atomic device-admission control is outside this payload and is
never overwritten by a whole-fence publication.

The client protocol owns provider/runtime negotiation and mapped fast-path setup.
The backend ABI independently owns daemon/worker-to-backend calls. A backend
entrypoint may expose sized C function tables and extension chains, but no C++
exception, STL object, compiler class, ecosystem handle, or ambiguous allocator
ownership crosses it.

Version negotiation specifies struct_size, capabilities, extension rules,
reserved-field handling, downgrade behavior and allocator/lifetime ownership.
Keep client-protocol and backend-ABI versions independent. The C17 application
fast path stays free of C++/LLVM/backend objects.

For cross-record stable copies and shared leases read
[admission](registry-admission.md); for fence/range writers read
[publication](registry-publication.md); for banks and publisher death read
[telemetry](registry-telemetry.md). Those references retain the full tagged
state machines, terminal capacity and live-owner quarantine rules.

## Affected scenarios

Select the scenarios for the mechanism being changed and the active work item's
required qualification; run them after the coherent implementation.

- Exercise compatible, older, newer, truncated, unknown-extension, malformed,
  and unsupported-capability negotiation without reinterpreting native structs.
- Stress queue wrap, false sharing, lost wakeups, process death, and concurrent
  producers/consumers under the active work item's syscall and ordering gates.
- Include C/C++ layout programs, encoded golden bytes, shortened counters,
  multiprocess death and ABI symbol/closure checks for the affected boundary.
  Run one-million-operation queue stress when required by the active plan.
  A fixture compile does not establish runtime or compatibility behavior.

Primary repository sources: [runtime](../../../../runtime/README.md),
[control/data-plane ownership](../../../../docs/architecture/control-and-data-plane.md),
and [work-item-0.1.0.2](../../../plan/milestone-0.1.0.0-core-foundation/work/work-item-0.1.0.2-contracts-runtime.md).
