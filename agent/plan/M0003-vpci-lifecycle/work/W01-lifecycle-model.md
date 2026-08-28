---
id: M0003-W01
milestone: M0003
status: Queued
area: lifecycle.contract
depends_on: [M0002]
updated: 2026-08-27
---

# Lifecycle Model and ABI 0.x

## Outcome

Specify and model one idempotent lifecycle state machine before transport or PCI
implementation. `metafluxd` remains the only authority that allocates and
publishes a generation/epoch; transport owners mirror committed state and retain
local tombstones. The complete ownership model is maintained in
[the control/data-plane architecture](../../../../docs/architecture/control-and-data-plane.md).

Each daemon process receives a non-reusable 128-bit `daemon_incarnation_id`,
separate from device UUID/generation/epoch. Every request carries request ID,
source, operation, expected UUID, expected generation, daemon incarnation, and
deadline. Replayed IDs are idempotent; duplicate, stale, or conflicting requests
never increment generation.

| Event | Valid source | Published intermediate states | Terminal state | Identity action |
| --- | --- | --- | --- | --- |
| Add | `ABSENT` | `PRESENT` | `ONLINE` or `LOST`/`ABSENT` | Publish a new generation before `ONLINE` |
| Remove | `ONLINE` or `LOST` | `QUIESCING -> DRAINING` | `ABSENT` | Retire generation; replacement gets a new one |
| Reset | `ONLINE` | `QUIESCING -> DRAINING -> RESETTING` | `ONLINE` or `LOST` | Consume one candidate; increment epoch on retirement; publish it only on success |
| Transport loss | Any live state | none required | `LOST` | Preserve current identity as a tombstone |
| Recover | `LOST` | `RESETTING` | `ONLINE` or `ABSENT` | Publish a new generation |

Add persists the generation high-water mark and stages immutable identity,
transport backing, and the exclusive worker lease before committing registry
visibility. Failure before commit destroys staged resources but consumes the
candidate; failure after commit makes that generation `LOST`.

Reset first allocates one never-reused candidate, then publishes `QUIESCING`,
rejects new work, drains and irrevocably retires the old generation, increments
epoch exactly once, and commits the candidate only after every owner is ready.
Failure before retirement may leave the old generation `ONLINE`; failure after
retirement ends in `LOST`. One accepted reset consumes at most one candidate and
a duplicate consumes none. Removal rejects new opens/submissions before removing
transport; old fd, VMA, DMA, queue, event, memory, executable, and handle objects
remain generation-bound tombstones returning `DEVICE_LOST`.

A public deadline means state reaches complete `ONLINE`, `LOST`, or `ABSENT`; it
does not promise physical cancellation. Non-cancellable work remains isolated
behind old-generation tombstones. Lifecycle/admin UAPI stays ABI 0.x until the
qualification workstream freezes it.

## Work

- [ ] Specify guards, owner, commit points, deadlines, terminal errors, and
  high-water persistence for every transition.
- [ ] Define request ID, incarnation, generation, epoch, idempotence, lease
  staging/revocation, worker death, and stale-completion rules.
- [ ] Normalize admin, vfio-user, QMP, disconnect, and restart sources.
- [ ] Generate positive, invalid, repeated, racing, and injected-failure model
  fixtures from one schema.

## Exit Gate

State-model evidence proves that no event sequence publishes two live owners,
reuses an old generation, increments identity for a duplicate request, or leaves
a half-online state.
