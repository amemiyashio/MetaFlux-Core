# metafluxd

`metafluxd` is the local registry/control authority and embeds one CPU data-plane
worker per negotiated session. It listens on Unix `SOCK_SEQPACKET`, accepts either
an explicitly bound path or systemd socket activation (`LISTEN_PID`, one
`LISTEN_FDS` descriptor at fd 3), and records each peer's kernel-provided
`SO_PEERCRED`. Standalone sockets are mode 0600; activated socket policy remains
owned by the service manager.

## Boundaries

| Crossing | Canonical owner | Service behavior |
| --- | --- | --- |
| Negotiation/control bytes | `contracts/protocol/client/v1` | Exact 64-byte packets; reserved and status validation |
| Registry/ring/argument mappings | `contracts/shared/device/v1` | Canonical offsets, lock-free atomics, sealed memfds |
| Client queue implementation | `runtime/client/fastpath` | Reused directly; no daemon-private ring copy |
| PTX translation | `MetaFlux::CudaPtxFrontend` | Parse and serialize once during module load |
| CPU compilation | `MetaFlux::CpuBackendCompiler` | Publish UID-isolated PIC ELF or administrator AOT content |
| CPU execution | `MetaFlux::CpuBackendRuntime` | Interpret canonical Kernel IR or call a validated, loaded PIC ELF entry |

One daemon incarnation publishes one canonical full registry view to every
connection. The daemon retains its original read/write mapping and seals the
memfd with `F_SEAL_FUTURE_WRITE`; client fastpath attaches the registry
`PROT_READ`. Submission and completion rings remain independent read/write
mappings per session. Host memory, device memory, artifacts, argument blocks,
and modules are generation checked before every operation, and object IDs are
not reused within a session. Daemon shutdown closes the shared view admission
before the authority mapping is reclaimed; a restarted daemon receives a new
incarnation.

Compute sessions negotiate process-publication semantics explicitly. Sessions
with `MF_CLIENT_CAP_LIVE_CONTEXT_ACCOUNTING_V1` are absent from process snapshots
until their first context acquire and disappear after their last context release.
Legacy v1 sessions without the capability remain published for their connection
lifetime; their context-accounting controls are unsupported but do not terminate
the session. Observer sessions do not require the capability. Pruning a dead
`(pid, start_time)` identity closes every corresponding session admission, removes
it from process snapshots, requests cooperative cancellation, and starts a fixed
100 ms monotonic completion-drain deadline.
Drain is progress-aware: an already-produced completion has priority over control
traffic and is published before retirement when the peer frees ring capacity;
the first subsequently consumed ring command receives the stale-handle result.
An idle pruned session retires immediately. The deadline never extends, so a peer
that leaves the completion ring full cannot extend the drain: at expiry the daemon
drops any undelivered completion or deferred control and closes the socket. Socket
HUP is the terminal outcome for inherited clients that do not receive a stale
response before that bounded retirement. Queued CTA work and interpreter rounds
observe cancellation before further dispatch. A native compiled CTA that already
entered its non-preemptible entry point runs to return; its session, objects,
telemetry allocation, and per-UID reservations remain quarantined until that
return and final session teardown. They are never reclaimed or reused solely
because the completion-drain deadline expired.

Mapped host-memory, artifact, and argument-block objects retain the local
256 MiB and 4096-object limits. A daemon-global authority additionally performs
atomic reserve/release by the `SO_PEERCRED` UID: at most 16 live sessions,
256 MiB, and 4096 mapped objects per UID, and at most 64 sessions, 1 GiB, and
16384 mapped objects for the daemon. Explicit release, disconnect, and failed
construction all return reservations. The session reservation is acquired before
a peer receives a worker thread or begins negotiation, transfers into the
established session, and is released by one RAII owner on every exit path. A peer
must send its complete negotiation packet within a fixed one-second monotonic
deadline; silent peers are closed without extending that deadline. Thus pending
handshakes participate in the same global and per-UID limits as established
sessions, and missing handshakes cannot retain capacity indefinitely.

The canonical telemetry row includes memory aggregated across all live compute
sessions. Data-plane work contributes measured monotonic compute intervals.
Successful copies also contribute memory intervals; successful launches do so
only when their prepared, structured Kernel IR contains a global load or store.
The authority independently unions compute and memory interval overlap with the
latest 100 ms single-device window. It never infers memory activity from allocated
or resident bytes. The listener refreshes the shared snapshot every 25 ms so an
idle device decays to zero without requiring another client operation.

The control socket materializes objects, resolves immutable artifacts, and
applies privileged registry-backed device-policy changes.
Allocation, copy, launch, events, synchronization, completion, and telemetry use
the shared rings/mappings after bootstrap. The test-only runtime host fixture is
not linked into this executable.

When `DIRECT_HOST_COPY` is negotiated, a compute session registers one validated
`O_RDWR` process-memory descriptor and direct COPY descriptors transfer between
that address space and daemon-owned device memory without per-copy host memfds.
The shutdown diagnostic reports direct and staged source/destination operation
and byte counters plus accepted address-space registrations. Acceptance and
performance runners require direct H2D and DtoH activity with all staged counters
at zero; older clients still use the staged compatibility path.

Observer sessions may also negotiate `MF_CLIENT_CAP_POLICY_SETTERS_V1` for the
two payload-free M0001 policy operations. The target is the immutable identity
record, and the argument is the canonical shared-device persistence or compute
mode value. The daemon accepts a setter only from root or its own effective UID,
serializes it with registry publication, preserves unknown policy bits, and sends
`OK` only after a stable lifecycle-fence read confirms the effective value and
sequence. Repeating an already-effective value is idempotent.

## CPU execution modes

`METAFLUX_CPU_EXECUTION_MODE` is parsed once before the listener opens. It is
unset by default (`interpreter`) and otherwise accepts exactly these values:

| Value | Module-load behavior |
| --- | --- |
| `interpreter` | Retain canonical Kernel IR and execute it in the embedded interpreter. |
| `cold-jit` | Load compatible administrator AOT first; otherwise require a mutable miss, compile once, atomically publish, validate, and load PIC ELF. |
| `warm-jit` | Load compatible administrator AOT first, then an existing mutable UID entry. A miss fails without invoking the compiler. |
| `aot` | Load only administrator-prewarmed AOT content. A miss is read-only and fails without invoking the compiler. |

Cold misses and explicit AOT prewarm reserve cache capacity in the parent, then
`posix_spawn` the current `metafluxd` image through its private compiler-worker
entry. The child alone runs MLIR/LLVM compilation. A versioned, length-bounded
socketpair protocol carries structured Kernel IR in and PIC ELF plus its digest
and parameter signature out; the parent validates the complete response and
retains publication authority. Worker process groups have a monotonic deadline,
parent-death kill, zero core limit, bounded CPU time, file size, descriptor
count, and a 4 GiB address-space limit in production builds. Sanitizer builds
omit only the address-space limit because the ASan shadow reservation exceeds
it. Exit, signal, timeout, malformed/truncated response, digest mismatch, and
worker exception paths return stable compiler diagnostics and always reap the
child. Warm-JIT and AOT lookup never spawn this worker.

D0019 intentionally keeps the required static MLIR/LLVM component closure in
the generic daemon image. The worker boundary isolates compiler execution,
resource limits, crashes, and timeout recovery in a distinct process; it is not
a claim that the parent ELF excludes those static dependencies.

Unknown values, aliases, and empty values are configuration errors. The default
cache roots are `/var/cache/metaflux/compiler` and `/var/lib/metaflux/aot`.
`METAFLUX_COMPILER_CACHE=/absolute/base` is the bounded development and test
override; it maps the mutable and AOT tiers to `/absolute/base/mutable` and
`/absolute/base/aot`. Relative paths, `..`, and `/` are rejected. Neither a
control packet nor a ring descriptor can select a cache root or UID.

CPU placement is refreshed only at a kernel boundary. The runtime intersects
the process affinity mask, online CPUs, and cgroup-v2
`cpuset.cpus.effective`; effective memory nodes likewise intersect online nodes
with `cpuset.mems.effective`. Auto mode selects one effective SMT sibling per
physical core, reserves one whole core when at least four are available, and
creates pinned per-NUMA pools whose shared queues permit only same-node CTA
stealing. `METAFLUX_CPU_PIN=<cpu>` selects one processing unit explicitly. If
that CPU later leaves the effective set, launches return the stable placement
loss result instead of silently moving work.

Isolated acceptance sandboxes may explicitly set
`METAFLUX_CPU_TOPOLOGY_ROOT=/absolute/fixture` to read CPU, node, and cgroup
topology from `fixture/{cpu,node,cgroup}` while retaining the process's real
`sched_getaffinity` mask. This is a test-fixture input, not a production fallback:
without the variable the daemon reads the host kernel interfaces and reports
missing or malformed topology as an initialization error. Empty, relative,
filesystem-root, and `..` paths are rejected before the listener opens.

Mutable lookup and publication always receive the `uid` captured by
`SO_PEERCRED`; the resulting path is
`mutable/users/<uid>/epoch-1/<prefix>/<digest>/`. A module retains both its
cache pin and loaded ELF handle until module unload or session teardown. The AOT
tier is shared across UIDs, installed read-only, and can be populated explicitly:

```sh
METAFLUX_COMPILER_CACHE=/absolute/base \
  metafluxd --prewarm-aot /absolute/path/kernel.ptx
```

A warm/AOT miss is reported as stable `MF_SHARED_NOT_SUPPORTED`; malformed
metadata or ELF is `MF_SHARED_MALFORMED`, quota exhaustion is
`MF_SHARED_RESOURCE_EXHAUSTED`, and I/O or loader infrastructure failures are
`MF_SHARED_SYSTEM_ERROR`. On clean shutdown the daemon prints mode, compiler
request, cache hit/miss, and loaded-module counters for qualification evidence.
