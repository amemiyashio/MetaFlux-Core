# Failure Injection

## Stage matrix

Inject before and after every durable or externally visible side effect:

- request acceptance plus generation-candidate reservation and high-water
  persistence;
- registry staging/publication;
- transport socket/node/mapping setup;
- PCI function/config publication and driver bind;
- backend creation and resource import;
- worker lease stage/commit/revoke;
- admission stop, queue fence, drain, and the atomic old-generation
  retirement/epoch/candidate-install replacement transaction;
- QMP command send/reply/event and monitor disconnect;
- terminal-state publication, event notification, and cleanup.

For each point record expected state, current generation, whether the candidate is
consumed, whether replacement/retirement committed, the before/after epoch, live
owner count, accepted new work, old work isolation, resources to unwind,
tombstones retained, external event, public error, and retry behavior.
Pre-acceptance failure consumes no candidate. Every failure after accepted
reservation consumes it. Every failure before the identity transaction leaves
epoch and current generation unchanged; reset/recover commit old retirement,
epoch advancement, and candidate installation together. A later fault marks that
committed candidate `LOST`.

## Concurrency matrix

Race reset/remove/recover with duplicate and conflicting requests, transport
loss, daemon restart, worker/backend death, QEMU exit, cdev close/VMA fault, DMA
unmap, PCI config/rescan, provider calls, telemetry reads, queue submit, and late
completion. Use deterministic barriers/hooks to hit commit boundaries rather
than relying only on random timing.

For the runtime view gate, enumerate A partial publish followed by B's blocked
publish attempt, then A completion or abort/compensation/suffix-skip; A abort before
its first publish; B authority becoming `LOST` while A remains head; B admission/
read attempts; B's loss-publication deadline triggering view close; writer death;
and terminal close at every boundary. Also enumerate one shared admission record
through seq-cst attempt `ENTERING`, lease `FREE`/`INITIALIZING`/`RESERVED`,
`COMMITTING`, `COMMITTED`, and `PUBLISHED` versus device/view close; pause before
and after the OPEN reads/control CAS/quiescence scan; owner death between every
pair; descriptor/resource half-publication and helping; terminal attempt/lease-slot
reuse while a stale tagged owner/helper resumes; every
split point of an odd/even multi-field fence write; independent device-latch close
versus a stale fence writer; control-payload tear versus view-latch close; shortened
view-publish `FREE`/`INITIALIZING`/`PREPARED`/`LINKING`/`ACTIVE`, fence stable-even,
cursor, `PUBLISHED`, ownership release, and terminal; `RANGE_RESERVE` range-slot
initialization, stable high-water/tail, range marker/release, and caller acceptance;
`RANGE_RETIRE` whole/partial suffix disposition then head advance, including
abort-before-first and `max(begin, cursor + 1)` gap derivation; proven-dead
publisher recovery versus live expired publisher quarantine; zero/one/two ordinary
publish slots at tag exhaustion with two dedicated close records/tags/pairs;
high-water where a full range fits exactly or crosses reserved `MAX`, before and
after authority acceptance; tagged `OPEN -> UPDATING -> OPEN/CLOSED` quota/policy
update versus a stable reader and `RESERVED`/`COMMITTING` old-generation lease;
loss/close interrupting update `FREE`/`INITIALIZING`/`PREPARED`/`LINKING`/`ACTIVE`,
fence stable-even commit, cursor advance, `FENCE_PUBLISHED`, and stale reopen CAS;
updater proven death at each step versus a live expired updater requiring close/
quarantine; lifecycle-range/view-publish/attempt/lease/update/telemetry-publish
record reuse while a bounded central-table scanner pauses before/after hazard
publication; two telemetry publishers racing
two stale prepared targets, record link/ownership/even-to-odd, `ACTIVE+WRITING`
before odd, every bank/control/even/marker/release step, `PUBLISHED+WRITING`, a live
expired writer, shortened latch/sequence with a slow reader spanning
two bank cycles and exhaustion; shortened control latch with one/two complete pairs
left before close; close/new-view plus stale actor replay; and
reader view/device acquire, fence/telemetry read, close, and final recheck. A later
range never writes before the earlier range is closed, no range truncates or
consumes `MAX`, settled close publishes both control states while live-writer close
publishes terminal admission and quarantines without consuming the pairs, a blocked
loss never leaves an open admission latch,
policy reopens only after old leases resolve, dead updaters cannot strand
`UPDATING`, live expired owners cannot write into reused records/banks, exact target
fence/cursor or telemetry-even commits are marked rather than replayed, no mutable
per-latch index or record reuse creates ABA, telemetry never wraps or returns a
mixed bank, loss generation never decreases, no control/fence read tears or reopens
admission, and no stale actor/admission commits after close linearizes.

## Invariants

- At most one live generation and one worker lease per logical device.
- Generation high-water never decreases or reuses an accepted candidate. Epoch
  changes only at irreversible retirement, by exactly one per retired generation.
- Reset/recover never expose a state between old retirement and candidate
  installation. Every current `ONLINE` or `LOST` identity names one committed,
  not already-retired generation.
- No stale callback publishes completion, telemetry, IRQ, or state into a new
  generation.
- Every accepted request reaches complete `ONLINE`, `LOST`, or `ABSENT` by its
  public deadline.
- Cleanup is exactly once; retry is idempotent; uncertain external state is
  reconciled rather than assumed.
- Provider-view races never add an ordinal to initialized CUDA, never expose an
  addition inside the current NVML init epoch, and never create a second process
  `registry_view_id`.
- Closing or abandoning a view-range reservation either leaves authority unchanged
  before any publish, publishes a compensating/final authoritative fence, or
  terminal-closes the mapping. It never leaves a live mirror in an intermediate or
  stale state, and unused suffix values are permanently retired.
- Device/view close operates on one shared atomic lease state: it revokes, helps,
  drains/cancels, or tombstones by deadline despite owner death or half-publication.
  Every CAS includes the lease tag; slot reuse cannot admit a stale owner/helper.
  Every admission linearizes before that close or produces no valid side effect.
- Stateful admission publishes a seq-cst attempt before reading `OPEN`; close/update
  seq-cst transitions control, quiesces every old-tuple attempt, then scans READY
  leases. Initializing payload is never interpreted, and no late old lease commits.
- Lifecycle fence readers accept only a stable even odd/even-latch copy bracketed
  by matching open device/view generations; mixed-commit fields are impossible.
- Fence writers never modify the independent device-admission control, and a full
  never-reused view incarnation rejects every old-view token after renegotiation.
- Range allocation is all-or-nothing below reserved `MAX`: pre-accept exhaustion
  has no side effect; post-accept/external-event exhaustion atomically closes view.
- `RANGE_RESERVE` stable-commits high-water/tail to one initialized whole range and
  reconciles a lagging marker without a gap. `RANGE_RETIRE` commits immutable whole/
  partial suffix disposition before head advance; cursor records only real fences,
  and range begin plus the retirement ledger derives every sequence jump.
- View publication has one tagged owner and one cursor source; its record alone owns
  token/range/cursor/fence/control targets. Recovery marks exact commits without
  replay. Settled close reserves two records/tags/pairs for stable control states;
  a live-owner deadline reaches terminal admission and quarantines odd payload/
  storage instead of stealing the writer.
- Every admission-relevant fence update uses tagged `UPDATING`, resolves the prior
  validation generation's leases, and only then reopens under the new quota/policy.
  Its record moves `FREE -> INITIALIZING -> PREPARED -> LINKING -> ACTIVE`, refers
  to the sole owning view-publish record, and reconciles fence commit, cursor,
  marker, and reopen without replay. Loss/close wins with monotonic `CLOSED`; stale
  reopen fails. Proven death may help; a live-owner deadline closes/quarantines.
- Admission latches contain no mutable lease head/index. Bounded central-table scans
  publish hazards and revalidate full tag/state, so lifecycle-range/view-publish/
  attempt/lease/update/telemetry-publish record reuse cannot create a second ABA
  path. Initializing/linking/active slots are not physically reused until owner
  quiescence or proven death.
- Telemetry latch and snapshot sequence never wrap; atomic control/bank words plus
  identical even-latch bracketing prevent a slow reader from accepting fields
  across bank reuse. One tagged publisher owns the bank; exact even commit is marked
  rather than replayed. Proven death permits recovery, while a live-owner deadline
  closes/quarantines without bank write/reuse. Exhaustion closes before odd/bank.
- View creation reserves two close records/tags and complete latch pairs. A settled
  publisher uses them for stable `CLOSING`/`TERMINAL`; a possibly executing expired
  publisher consumes neither and reaches terminal `ViewAdmissionControl`/
  quarantine by deadline. No live/reusable view remains intermediate.
