# CUDA Driver Compatibility Provider

This directory builds the MetaFlux-managed `libcuda.so.1` compatibility provider.
It is installed below `${libdir}/metaflux/providers`, outside the global vendor
library lookup directory. A MetaFlux launcher or another explicit, process-scoped
selection mechanism must load this DSO; packaging must not add it to `ldconfig`
or replace a vendor-owned `libcuda.so` path.

The provider connects lazily on the first `cuInit` call. `METAFLUX_SOCKET` may
select a daemon socket explicitly; otherwise the client fastpath uses its
documented runtime-directory lookup. `CUDA_VISIBLE_DEVICES` is captured once at
successful initialization and only filters/reorders the CUDA ordinal view.
The default ABI target is the pinned R610 CUDA Driver 13.3 surface; known but
unsupported entry points return stable CUDA errors rather than implying backend
capability.

`METAFLUX_MODE` accepts exactly `managed`, `passthrough`, or `auto`; an unset
value means `auto`. The value and parse result freeze at the first valid
`cuInit(0)` attempt. `managed` uses only the MetaFlux session, `passthrough`
uses only one validated CUDA/NVML vendor pair, and `auto` tries the managed
transaction first. Auto mode enters the vendor stack only after managed
rollback and a pristine-state check both succeed. There is no constructor-time
selection.

All 77 entries in `symbols.def` use the frozen runtime decision. In passthrough
mode each wrapper resolves the same exact, versioned symbol from the selected
pair, including aliases, PTDS/PTSZ forms, and the vendor's own
`cuGetProcAddress` implementation. A forked child or a changed vendor/config,
namespace, or driver-version fingerprint returns `CUDA_ERROR_DEINITIALIZED`;
missing vendor symbols return the corresponding stable CUDA provider error.

Managed launch, asynchronous copy, event-record, and stream-wait calls enqueue
into a fixed 128-entry completion ledger and return after the ring accepts the
descriptor. Completions are consumed in request FIFO order. Event query polls
and returns `CUDA_ERROR_NOT_READY` while its record is outstanding; event,
stream, and context synchronization block only through their captured request
boundary and report the oldest related deferred error. When direct host copy is
negotiated, the provider registers one `O_RDWR` `/proc/self/mem` descriptor at
cold start and submits host virtual addresses without per-copy payload objects.
Asynchronous HtoD drains before returning so the source bytes are fixed; DtoH
finishes at its stream synchronization boundary. Older runtimes retain the
size-sealed memfd path, where HtoD bytes are copied into provider-owned storage
before return and DtoH destinations change when completion is consumed. Module,
memory, and context teardown drain pending references before releasing the
referenced object. A local `/proc/self/mem` open failure or explicit registration
`UNSUPPORTED`/`INVALID_ARGUMENT` response selects that staged path. Transport,
malformed-response, stale-view, permission, and internal registration failures
fail managed initialization; `auto` may select the vendor pair only after rollback
proves the managed provider pristine. Stream and event destroy invalidate their
public handle immediately;
their generation-tagged ledger references remain valid until completion and are
still observable at the owning context boundary. Request and timeline
identifiers never wrap; exhaustion returns a stable allocation error. The fixed
deferred-error table preserves the oldest failures when saturated without
turning application failures into a transport failure.

Successful managed context creation acquires one daemon process-view reference,
and final context destruction releases exactly one reference after its pending
work and owned objects drain. Primary-context retains share that single
reference, use a checked retain count, and release it only when the last retain
is released. Primary-context reset drains and invalidates owned state without
dropping the primary record, its retain count, or the daemon reference. A
negotiated client connection without a live CUDA context therefore does not
appear in the process snapshot, while disconnect remains the crash cleanup
boundary for any unreleased references.

The R610 PTDS/PTSZ stream-query, stream-synchronize, and event-record aliases
are exported explicitly. Null-stream work submitted through a PTDS/PTSZ entry
uses a generation-tagged default-stream boundary owned by the calling thread and
context generation; it neither waits for nor consumes another thread's deferred
null-stream errors.

The managed Add launch path also keeps a fixed 64-entry immutable argument-block
cache. An exact byte-for-byte match within the same context and module generation
reuses the registered object, so a repeated warm launch performs no control-socket
transaction. Cache-full misses use completion-owned one-shot blocks. Cached blocks
are released after pending references drain at memory, module, context, or provider
teardown; completion processing releases only one-shot blocks.

Each live context owns one recursive provider gate. Initialized launch, Copy,
event-record, and stream-wait paths acquire only their current context gate, so
different contexts can reserve request IDs and submit concurrently to the MPMC
ring. Same-context serialization preserves CUDA stream order and object lifetime.
The export dispatcher reads an atomic runtime/PID snapshot and enters these calls
without its process-wide gate; warm paths therefore acquire neither the dispatch
gate nor `mf_cuda_global.lock` and perform no provider heap allocation. A first
launch or interior-Copy cache miss may still perform its documented serialized
control transaction; repeated matching submissions reuse fixed-capacity cache
entries. Direct host copies use only the fixed pending ledger; fallback host
copies additionally retain their memfd mappings there.

The fixed 128-slot pending ledger uses a monotonically tagged state word plus
atomic request/context keys. A submitter publishes immutable pending data before
the `SUBMITTED` state. Any context may claim a matching completion as
`COMPLETING`, write its payload, and release-publish `COMPLETION_READY`; only the
owning context finalizes ready requests in increasing request-ID order. Slot reuse
increments the tag and stops at its terminal value, preventing stale CAS
operations from acting on a new request. Capacity and request-ID exhaustion are
reported without publishing a partial command.

Cold lifecycle and teardown paths take the process state gate and then every
context gate in index order before mutating shared object tables. Cold completion
waits release that ownership in bounded slices, and teardown defers unmap until
every mapping borrower returns. Observation-only callback reentry is supported;
a callback may not start a nested destructive object or queue transaction.

The provider semantics test snapshots test-only counters around all managed
launch, synchronous/asynchronous Copy, event-record, and stream-wait aliases. It
requires zero dispatch-gate acquisitions, zero state-gate acquisitions, zero
provider heap-allocation attempts, and one context-gate acquisition per call,
then repeats mixed submissions from four threads sharing one context. Long
control transactions and lifecycle operations remain cold paths.

Each Add argument block carries the caller's exact x/y grid and block dimensions;
the daemon no longer derives launch shape from the scalar element count. The
compiler-epoch-1 PTX subset fixes both z dimensions at one and rejects other z
values or dynamic shared memory before enqueue. With no peer/import capability,
every device endpoint used by launch, copy, or free must belong to the current
context. Generation-bound launch arguments preserve validated, four-byte-aligned
interior device-pointer offsets.
