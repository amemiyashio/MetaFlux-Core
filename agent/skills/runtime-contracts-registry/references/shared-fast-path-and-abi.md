# Shared Fast Path and Backend ABI

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

Any read spanning view control plus per-device or telemetry records uses an
explicit acquire/read/acquire validation bracket. Close start is release-published
as one reader-indivisible `CLOSING` control state; consumers discard work when view
ID or generation changes or state is no longer `OPEN`. Per-record atomic loads
alone do not establish this cross-record lifetime guarantee.

A multi-field lifecycle fence has its own odd/even latch sequence. Writers publish
odd, update all fields, then release-store the next even value; readers require the
same even value before and after a full copy. The lifecycle sequence is payload,
not the latch, and cross-record control rechecks happen outside this stable-copy
loop.

Seqlock payload is not plain C/C++ memory: every concurrent field is represented by
naturally aligned 32/64-bit words and accessed through `mf_atomic_*` (relaxed only
inside the seqlock wrapper). The begin/end helpers provide compiler and hardware
barriers; subword values share an atomic containing word. Concurrent structure
copy or `memcpy` is forbidden, avoiding data-race UB even when a reader retries.
The independently atomic device-admission control is outside this payload and is
never overwritten by a whole-fence publication.

Multiword registry-view control follows the same atomic-payload seqlock rule. Its
independent view-admission control closes first and is never part of a control
copy, so readers reject close immediately even while the stable `CLOSING` payload
is being published.

Fence/control payload writers serialize through tagged `ViewPublisherControl` and
a bounded `ViewPublishRecord` initialized as `FREE -> INITIALIZING -> PREPARED ->
LINKING -> ACTIVE`. The record is the sole owner of immutable range/cursor/fence/
control targets. Recovery compares exact stable fence and control cursor, supplies
only missing markers/releases, and never replays a committed fence. There is no
mutable token position: head next is `max(range.begin, checked(cursor + 1))`, with
immutable retirement ledger proof for gaps. `RANGE_RESERVE` stable-commits high-
water/tail after range initialization; `RANGE_RETIRE` commits suffix disposition
before head advance. Normal and close kinds use their own exact validation.
Only quiescence/proven owner death permits raw-
write helping. A live-owner deadline terminal-closes independent view admission
and quarantines the mapping, which may retain odd payload but is never read/reused.
Two dedicated close records/tags and latch pairs are reserved at view creation for
the settled-writer `CLOSING`/`TERMINAL` branch.

Stateful admission uses one lock-free, generation-bound lease record referenced by
both view and device latches, not independent per-latch commits or a read-only
bracket. The atomic lease state arbitrates commit with close/loss and is helper-
recoverable through owner death and half-publication. Publication plans place a
release marker last and name idempotent cancel/complete/tombstone behavior. The
design must retain bounded close deadlines and the no-global-mutex steady-state
gate. Admission latches contain no mutable lease head/index. Once reservations are
blocked, lifecycle slow paths first complete a seq-cst quiescence handshake with
pre-registered attempt records, then scan the bounded central tagged table by full
view, identity, generation, and tag. An attempt publishes `ENTERING` before reading
`OPEN`; lease slots use `FREE -> INITIALIZING -> RESERVED`, and commit revalidates
the exact open tuple. Helpers do not interpret initializing payload. Hazard
publication followed by full record recheck prevents slot reuse from creating a
list-head ABA path.

Admission-relevant fence changes close the device validation generation into an
`UPDATING` state, resolve old-generation lease records, publish the stable payload,
then reopen only under the new generation. A final read-side recheck validates
lifetime; it is not an admission commit primitive or the read's policy
linearization point. Before the `UPDATING` CAS, the updater claims its record with
tagged `FREE -> INITIALIZING`, fills immutable recovery payload, release-publishes
`PREPARED`, then accepts irrevocable `LINKING`. The owner/helper links only that
exact tag to the latch, advances it to `ACTIVE`, and permits tagged reopen only
after `FENCE_PUBLISHED`. A merely prepared record is never applied. Proven owner
death permits helping; a deadline while the owner may execute closes/quarantines
the view until quiescence/death. Reuse follows tag/hazard no-ABA discipline.

Telemetry double buffering uses a dedicated 64-bit odd/even control latch around
atomic payload words for the full 64-bit no-wrap snapshot sequence, active bank,
and state. A tagged publisher control admits one `FREE -> INITIALIZING -> PREPARED
-> LINKING -> ACTIVE` publish record. Its owner CASes the expected even latch odd
before touching the inactive bank, writes bank/control payload through aligned
atomic-word accesses, then release-publishes the next even latch. Readers require
that same even latch around the selected bank. Proven owner death permits a helper
to finish/restore; a mere deadline closes and quarantines the view/banks until
quiescence/death, so a resumed writer never overlaps bank reuse. Both counters use
checked addition with terminal encodings reserved, avoiding slow-reader ABA and
any need for lock-free 128-bit CAS.

The client protocol owns provider/runtime negotiation and mapped fast-path setup.
The backend ABI independently owns daemon/worker-to-backend calls. A backend
entrypoint may expose sized C function tables and extension chains, but no C++
exception, STL object, compiler class, ecosystem handle, or ambiguous allocator
ownership crosses it.

Qualification includes C/C++ layout programs, encoded golden bytes, old/new
negotiation matrices, one-million-operation queue stress where required by the
active plan, shortened-counter exhaustion, death at every attempt/lease/update/
publish initialization and linkage step, live-owner deadline quarantine, tagged-
slot reuse, slow telemetry readers across repeated bank publication, multiprocess
death, and ABI symbol/closure inspection. Report a missing harness rather than
treating a fixture compile as runtime or compatibility evidence.

Primary repository sources: [runtime](../../../../runtime/README.md),
[control/data-plane ownership](../../../../docs/architecture/control-and-data-plane.md),
and [W0102](../../../plan/M0100-core-foundation/work/W0102-contracts-runtime.md).
