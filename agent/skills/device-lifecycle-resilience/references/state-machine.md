# Lifecycle State Machine

## Identity

- Persistent UUID names the logical device across domains.
- Generation is a monotonically allocated device incarnation. Consumed
  candidates are never reused, even if staging fails.
- Epoch marks retirement/reset progress as defined by the canonical M0003 model.
- A non-reusable 128-bit daemon incarnation distinguishes requests across daemon
  restarts.
- Request ID plus source/operation/UUID/expected generation/incarnation makes
  retries idempotent and conflicts detectable.

## Canonical transitions

| Event | Valid source | Intermediate | Terminal | Identity rule |
| --- | --- | --- | --- | --- |
| Add | `ABSENT` | `PRESENT` | `ONLINE`, `LOST`, or `ABSENT` | stage a new generation before online commit |
| Remove | `ONLINE` or `LOST` | `QUIESCING -> DRAINING` | `ABSENT` | retire current generation; replacement is new |
| Reset | `ONLINE` | `QUIESCING -> DRAINING -> RESETTING` | `ONLINE` or `LOST` | consume at most one candidate; publish only on success |
| Transport loss | any live state | none required | `LOST` | preserve current identity as tombstone |
| Recover | `LOST` | `RESETTING` | `ONLINE` or `ABSENT` | publish a new generation |

## Commit rules

Add stages immutable identity, transport/presentation backing, backend instance,
and exclusive worker lease before registry visibility. Failure before commit
destroys staging but consumes the candidate; failure after commit marks that
generation lost.

Reset may preserve old online state if failure occurs before irreversible
retirement. After retirement, any failure ends lost. A duplicate accepted request
does not consume another candidate. Public deadline means a complete terminal
state is published; it does not promise physical cancellation of backend work.

Old objects retain their generation and fail deterministically. They never
resolve through a UUID or slot to a replacement generation.
