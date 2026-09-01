---
id: work-item-0.1.1.2
delivery: 0.1.1.2
milestone: milestone-0.1.1.0
status: Active
area: transport.cdev
depends_on: [work-item-0.1.1.1]
updated: 2026-09-01
---

# Local cdev Vertical Slice

## Outcome

Execute unmodified CPU-backed CUDA Add/Copy through canonical cdev mappings with
one leased local worker and no PCI module.

```text
application -> CUDA/NVML provider -> /dev/metafluxN mmap
            -> shared ring -> leased local worker -> mf_backend_api_v1 -> CPU
metafluxd -> /dev/metafluxctl lease/broker -> local worker
```

The worker may be embedded in `metafluxd`, but queue/backend ownership remains
explicit. The authoritative ownership rule is maintained in
[the architecture record](../../../../docs/architecture/control-and-data-plane.md).

The milestone-0.1.0.0 memfd path is local-only fallback. Before any visible object succeeds,
cdev may fall back only for `ENOENT`, `ENODEV`, or explicit ABI incompatibility.
Permission, malformed state, integrity, or policy rejection never falls back.
Transport commits for the provider initialization epoch; a later provider joins
the same mode/transport/`registry_view_id`.

Each stream owns one SPSC descriptor lane. Same-stream host threads serialize
with a robust userspace owner word; independent streams share no producer lock.
Descriptors are 64 bytes with slot sequence, opcode, flags, generation, timeline,
handle, offset, and length. Cursors/timeline/armed state use separate cache lines.
Owner death marks that queue generation `LOST`; incomplete slots are not reused.
Empty-to-nonempty uses arm/recheck/wake. Active queues poll atomically; blocking
queues arm/recheck before futex/eventfd. Every handle/range/permission/generation
and arithmetic operation is validated.

## Implemented stage

- [x] Add C17 client and C++20 worker halves with a paired submission/completion
  ring projection and generation validation.
- [x] Build `metaflux_core.ko` through target Kbuild; register `/dev/metafluxctl`
  and `/dev/metaflux0`, negotiate the fixed candidate UAPI, map the paired rings,
  and enforce one worker lease per generation.
- [x] Add `CdevWorkerSession` to activate a generation/view-bound worker lease,
  negotiate and validate the current view on every control fd before taking the
  lease, validate the exact page-aligned paired-ring mapping, and close the
  leased control fd after unmapping. The
  kernel control fops expose the queue mmap at offset zero and allow the leased
  control fd to map the exact payload arena after its data-file owner allocates
  it; payload ownership remains with the data fd.
- [x] Add `CdevObjectTableResolver` as the daemon-facing region COPY adapter.
  It validates the exact argument block, resolves generation- and
  permission-bound object views, checks both ranges before importing, and
  balances destination cleanup when source resolution fails. The CPU backend
  regression exercises two imported subranges through the real worker.
- [x] Keep cdev fallback limited to `ENOENT`/`ENODEV`/explicit ABI incompatibility
  in the userspace client; permission and malformed states remain visible.
- [x] Allocate one generation-bound, page-aligned driver payload arena through
  `MF_UAPI_IOCTL_MEMORY_ALLOC`, expose it through the generated payload mmap
  offset, and retain an offline tombstone until the final VMA closes.
- [x] Attach one complete caller-owned eventfd pair to either the data queue or
  worker lease, retain kernel `eventfd_ctx` references, and reject a second owner
  for the generation with `-EBUSY`.
- [x] Register a bounded table of up to four caller-owned ranges, each with
  `FOLL_LONGTERM`/`FOLL_WRITE` pinning, normal memlock accounting, an independent
  SG table, partial-pin unwind, dirty unpin, owner-close or explicit unregister
  revocation, and direction-aware `dma_map_sg`/`dma_unmap_sg` lifetime through
  the data cdev DMA device. Enforce unique generation-bound handles and a 256
  MiB aggregate quota; reject a standalone cdev without a DMA mask or parent
  master before pinning, while backend in-flight device references remain open.
- [x] Add a worker-side `mf_backend_api_v1` COPY dispatch seam with sized-table,
  capability, handle, offset, and backend-status validation. An unbound worker
  retains the local fixture copy path; a malformed bound API returns
  `MF_SHARED_NOT_SUPPORTED` without fallback. Registered-memory backend import
  and asynchronous generation ownership remain open.
- [x] Expose a backend-agnostic `CdevCopyResolver` for
  `MF_RING_COPY_FLAG_REGION_ARGUMENT_BLOCK_V1`. The resolver maps argument-block
  object references to independent backend memory handles and ranges, with an
  explicit retain/release pair for each returned handle; the worker validates
  the result, retains both references, and holds them through synchronous copy
  or asynchronous completion/backpressure while also holding the operation
  lease. The cdev descriptor carries the argument-block ID in `target_id` and
  its generation in `arguments[0]`; the resolver-side
  `CdevBackendMemoryImporter` seam now converts a generation-checked,
  caller-owned registered range into that referenced backend handle. The
  daemon object-table adapter and physical-device qualification remain open.
- [x] Require a synchronous backend-operation lease for every bound COPY or
  LAUNCH. The worker holds the lease across resolver access and the backend ABI
  call, surfaces lease rejection in the completion status, and releases it only
  after the synchronous call returns. Nonzero event completion retains the
  lease, exact backend binding, and resolved memory references until observed
  completion; physical DMA qualification remains separate work.
- [x] When a pending asynchronous operation's lifecycle mirror receives
  transport loss, invoke the generation-bound backend `cancel_queue` only when
  `MF_BACKEND_CAP_CANCELLATION` is advertised. A successful cancellation emits
  `MF_SHARED_DEVICE_LOST` through the normal completion path and preserves the
  lease/reference/backpressure ordering; backends without cancellation retain
  their event contract.
- [x] Allow a bound direct COPY payload handle to carry a complete
  `CdevBackendMemoryReference`; retain it before backend dispatch and release it
  after synchronous completion or asynchronous completion/cancellation and
  completion-ring backpressure. Partial callback sets are rejected; existing
  no-reference fixtures remain valid.
- [x] Reset and Remove quiesce reject a pending asynchronous operation when its
  backend lacks cancellation capability; supported cancellation is processed by
  lifecycle drain before the submission ring is considered empty.
- [x] Expose the CPU backend's transport-facing COPY and synchronous launch
  subset (instance, context, queue, caller-owned host-memory import, canonical
  KIR module load/unload, and memory-handle argument blocks). The CPU backend
  executes the Add fixture through `mf_backend_api_v1.submit` with strict
  dimensions, ranges, ownership, and typed status mapping.
- [x] Add cdev launch descriptor helpers and a backend-agnostic,
  generation-bound `CdevLaunchResolver`. The resolver translates module and
  argument-block object references into payload-relative backend argument bytes;
  the worker validates the range and 2D launch shape, then invokes the backend
  submit ABI. The real CPU backend Add fixture now passes through this cdev
  worker path; production backend memory import and physical DMA remain open.
- [x] Bind provider cdev initialization to the matching Unix daemon session,
  registry view, and queue generation through the decision-0030 capability and control
  handshake. The provider keeps Unix object/control operations and routes COPY
  plus primary-entry LAUNCH through the leased cdev worker after binding; the
  daemon resolves its object table into CPU backend handles with explicit
  operation references. Live device-node qualification and kernel registered
  memory/DMA import remain separate gates.
- [x] Bind every worker backend binding to its device generation. After a
  lifecycle commit, a valid old binding is rejected for new COPY/LAUNCH work as
  stale until the daemon installs a binding for the committed generation;
  pending operations retain their captured binding through drain.
- [x] Enforce the worker-side generation-bound replacement boundary. Rebind is
  accepted only while the lifecycle mirror is online at the committed
  generation; lifecycle commit clears the previous current binding, and an
  optional owner retire callback waits for pending backend, lease, and memory
  references to drain. A pending binding may be rebound to the same owner
  without duplicate retirement.
- [x] Activate the daemon's authoritative object table for region COPY in the
  embedded CPU worker path. Each daemon session owns a CPU backend
  instance/context/queue, the cdev resolver validates object identity,
  permissions, and ranges, and each object is bound once to a full-range
  backend memory handle before COPY. Operation references are retained and
  released around the backend call; retired object handles drain before backend
  reclamation, and the public completion and copy-accounting records remain
  unchanged.
- [x] Permit a queue-only worker view for daemon-owned region COPY. The worker
  requires a payload arena for direct COPY and LAUNCH, while a bound
  object-table resolver may consume the leased queue without mapping the
  caller-owned payload arena.
- [x] Retain an offline queue mapping as a VMA tombstone after module teardown
  and reclaim its backing under the cdev lock when the final queue VMA closes.
- [x] Mark the current generation offline and wake waiters when the queue owner
  or worker lease closes, preventing new users from reusing that tombstone.
- [x] Track queue backing with distinct root, owner, lease, VMA, and active
  wait/poll `kref` references so the final reference performs reclamation.
- [x] Track payload backing with distinct root, owner, VMA, and active allocation
  operation `kref` references so owner or module close cannot reclaim an arena
  while `MEMORY_ALLOC` or an existing VMA still observes it.
- [x] Add `MF_UAPI_IOCTL_MEMORY_QUERY` on the negotiated worker lease so the
  worker obtains the current payload handle, generation, exact page-aligned
  size, and mmap offset without a local size convention; mmap rechecks the
  owner and exact length under the cdev lock.
- [x] Serialize all cdev ioctl, mmap, and poll reads of mutable per-file
  negotiation, lease, queue, and registered-memory authorization state under
  `mf_cdev_lock`, preserving errno and resource-unwind behavior across close
  and teardown races.
- [x] Add a live cdev qualification executable covering the generated ioctl and
  mmap ABI, eventfd-backed lease, payload query, long-term registered-memory
  pin/unregister, malformed and stale requests, unknown ioctl rejection, and
  owner-close VMA tombstones. It is wired into CTest with explicit skip codes
  when the device node, lease, or DMA target is unavailable.

## Remaining work

- [ ] Complete daemon-controlled generation replacement and backend reference
  drain beyond the queue and payload kref/tombstone graphs. Embedded daemon
  object handles now have persistent operation-reference draining; the payload
  and queue VMA tombstones, owner-death transition, eventfd references, and
  bounded registered-memory lifetime are implemented for the current fixture.
  Worker-side backend binding generation isolation and owner-retire drain are
  now enforced; daemon/provider-side rebinding and physical replacement drain
  remain open.
- [ ] Connect the daemon object table and leased worker/backend binding to the
  live cdev registered-memory handles and prove Add/Copy through the mapped
  payload arena. The source-level daemon lease/object-table binding and CPU
  backend COPY/LAUNCH adapter are now connected under `0404481`; live daemon
  use of the lease/query path, kernel DMA-backed references, generation
  replacement, and physical device qualification remain open.
- [ ] Test open/mmap/process/daemon death, stale generation, counter wrap, and
  teardown with KUnit, KASAN, KCSAN, lockdep, and kmemleak. The userspace live
  ABI harness is now present; kernel-configured sanitizer and fault-injection
  runs remain unexecuted on the current host. After `batch-0001` integrates its
  three live-path Iterations, the integrator advances `agent/goal.json` to
  `batch-0002` for the Linux 6.12/6.18 fault and sanitizer qualification matrix.

## Exit Gate

The implemented stage is not the work-item-0.1.1.2 exit gate yet. Closure still requires
unmodified Add/Copy through `/dev/metafluxN`, an uncontended active enqueue with
no syscall, allocation, or global lock, and fd/VMA tombstones that remain safe
after daemon death. Contention and owner-death slow paths must be bounded
separately.
