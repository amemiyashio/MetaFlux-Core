# Runtime

The runtime source boundary owns ecosystem-neutral mechanisms and types for
device, registry, execution, memory, queue, lifecycle, and scheduling behavior.
At run time, the authoritative registry, policy, generation, and worker lease
belong to the `metafluxd` instance. The runtime also owns the application-side
fast path used by compatibility providers.

The core must not contain CUDA, NVML, PTX, HIP, HSA, or AMD SMI types. Client
code remains small, allocation-free on steady-state operations, and independent
of LLVM/MLIR and concrete execution backends. Cross-component communication uses
the versioned contracts in `contracts/`.

The application-side chain is `compat provider -> client protocol -> runtime ->
backend plugin ABI`. Client protocol versions are independent of backend ABI
versions; changing an execution backend does not force a provider ABI change.

Provider DSOs may each statically embed stateless fast-path code for inlining and
closure isolation. Mutable mode, registry-view, queue, and generation state must
live in the one negotiated shared mapping, never in per-DSO globals. CUDA and NVML
therefore join the same process view without requiring a public helper DSO or
duplicating mutable state. The shared `registry_view_id` fixes selection policy and
default order and is a never-reused mapping incarnation; CUDA and NVML may capture
different generation-bound membership revisions when their permitted initialization
epochs differ. Same-revision,
unfiltered membership must agree, while cross-revision live matching uses
`(UUID, generation)` and does not require equal count or ordinal order. One
runtime-owned view-global FIFO publication gate serializes lifecycle fence updates,
range retirement, and mapping-terminal close; layer adapters emit events rather
than writing view rows independently. Calls bracket fence/telemetry reads with
stable view/fence snapshots plus independent view/device validation rechecks.
Fence/control writes use one tagged view publisher and recoverable publication
record. Whole-range reservation commits high-water/tail together; suffix retirement
commits before head advance. There is no token position duplicate: the actual-
fence cursor, immutable range begin, and retirement ledger derive the next value.
Proven-dead writers can be reconciled exactly, while a live expired writer terminal-closes admission and
quarantines its mapping instead of being unsafely preempted. View creation reserves
the close records/tags and latch pairs needed by the settled-writer close branch.
Stateful admission instead uses tagged generation-bound lease records shared by
the validation latches and serialized with policy update, close, and loss, so
neither stale policy, slot ABA, partial latch commit, nor a dead publisher can
admit late work. The latches contain no mutable lease index: bounded lifecycle
slow paths scan one central tagged table with hazard/revalidation. Admission-
relevant updates publish a helper-recoverable update record before entering
`UPDATING`; seq-cst admission-attempt quiescence prevents a late old-generation
lease from escaping the central scan. Telemetry uses one tagged publisher, atomic
bank/control payload words, and a dedicated 64-bit no-wrap odd/even latch. Exact
commit recovery never replays a bank, and a live expired publisher quarantines the
mapping before bank reuse can alias a slow reader.
