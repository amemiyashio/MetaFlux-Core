# Lifecycle State Machine

## Identity

- Persistent UUID names the logical device across domains.
- Generation is a monotonically reserved device incarnation. Acceptance of a
  nonduplicate add/reset/recover request durably consumes exactly one candidate;
  staging and pre-transaction failures never make it reusable. Numeric high-water
  exhaustion rejects the request before acceptance and consumes nothing.
- Epoch is a committed retirement counter, not an allocatable candidate. It
  advances exactly once in the atomic identity transaction that retires the old
  generation and, for reset/recover, installs its replacement. It remains
  unchanged on every failure before that transaction.
- A non-reusable 128-bit daemon incarnation distinguishes requests across daemon
  restarts.
- Request ID plus source/operation/UUID/expected generation/incarnation makes
  retries idempotent and conflicts detectable.

## Canonical transitions

| Event | Valid source | Intermediate | Terminal | Identity rule |
| --- | --- | --- | --- | --- |
| Add | `ABSENT` | `PRESENT` | `ONLINE`, `LOST`, or `ABSENT` | reserve a candidate; add commit installs it `ONLINE` without epoch change |
| Remove | `ONLINE` or `LOST` | `QUIESCING -> DRAINING` | `ABSENT` | retire current generation and advance epoch once; replacement is new |
| Reset | `ONLINE` | `QUIESCING -> DRAINING -> RESETTING` | `ONLINE` or `LOST` | reserve one candidate; atomic replacement retires old, advances epoch once, and installs candidate `ONLINE` |
| Transport loss | any live state | none required | `LOST` | preserve current identity as tombstone |
| Recover | `LOST` | `RESETTING` | `ONLINE` or `LOST` | reserve one candidate; atomic replacement retires old `LOST`, advances epoch once, and installs candidate `ONLINE` |

## Commit rules

Add stages immutable identity, transport/presentation backing, backend instance,
and exclusive worker lease before registry visibility. Failure before commit
destroys staging but consumes the candidate; failure after commit marks that
generation lost.

Reset acceptance is recorded only with successful durable reservation of its one
generation candidate. A failure before acceptance consumes none; any later
failure consumes that candidate. Reset may preserve the old online state if
failure occurs before replacement commit, and epoch remains unchanged. After all
replacement owners stage, one atomic identity transaction retires the old
generation, advances epoch once, and installs the candidate as current `ONLINE`.
A later fault marks that committed candidate `LOST` and cannot restore the old
generation. Replaying the same accepted request returns its recorded outcome and
consumes neither another candidate nor another epoch increment. Public deadline
means a complete terminal state is published; it does not promise physical
cancellation of backend work.

Normal remove uses the same retirement/epoch commit and reaches `ABSENT` without
reserving a replacement candidate. A later add reserves its own never-reused
candidate; duplicate remove or add requests repeat the recorded outcome without
another retirement or reservation.

Checked epoch capacity is an acceptance guard for remove/reset/recover, evaluated
before candidate reservation or any intermediate state. At exhaustion the request
is rejected with the stable overflow/resource error, consumes no candidate, and
leaves state/identity/epoch unchanged. The sole authority serializes the reserved
retirement right, so capacity cannot fail after acceptance; every accepted remove
still commits `ABSENT`, and epoch never wraps.

Transport loss alone does not retire identity or advance epoch. An accepted
recover reserves one candidate and stages every replacement owner while the old
`LOST` generation remains current. Failure before replacement commit consumes the
candidate, leaves epoch unchanged, and remains `LOST`. The atomic replacement
transaction retires that old generation, advances epoch once, and installs the
candidate `ONLINE`; any later fault marks the committed candidate `LOST`. Thus a
current `LOST` state always names a committed, not already-retired generation.

Old objects retain their generation and fail deterministically. They never
resolve through a UUID or slot to a replacement generation.

## Provider view freeze

CUDA and NVML in one process join one `registry_view_id`; neither provider keeps
an independently mutable registry copy. Removal or loss updates the frozen entry
immediately so old handles return `DEVICE_LOST`. A later addition never enters an
initialized CUDA ordinal set. NVML can observe it only after a matching shutdown
followed by a later zero-to-one initialization epoch, or in a new process.
Default unfiltered membership/order parity applies only when both providers
captured the same process-view revision. If NVML captures a later revision while
CUDA remains initialized, counts/order may diverge. Persistent UUID/logical ID
still correlates the device, but only `(UUID, generation)` identifies a common
live incarnation; stable BDF does not. CUDA filtering may only filter or reorder
CUDA's captured ordinal set.
