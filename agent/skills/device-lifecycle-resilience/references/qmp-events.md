# QMP Events

## Control model

QMP is an asynchronous adapter, not the lifecycle authority. Pin the exact QEMU
schema for the support matrix and model greeting/capability negotiation,
command IDs, immediate replies, later events, disconnect, timeout, and explicit
state queries.

## Correlation rules

- Give every outbound command a unique QMP ID and map it to the lifecycle request
  ID, expected UUID/generation, operation, and deadline.
- Treat command acceptance and device transition completion as separate facts.
  Do not publish `ONLINE` or `ABSENT` from an immediate success reply alone.
- Match asynchronous device add/delete/reset/error events using stable QEMU
  device identity plus the expected lifecycle request. Reject stale events from a
  prior daemon incarnation or generation.
- Event ordering relative to replies or other events must follow the pinned QMP
  contract. Where no total order is guaranteed, make handlers commutative or
  reconcile using QMP queries and guest/transport state.
- On monitor disconnect, mark in-flight command outcomes unknown, stop new
  publication, reconnect/renegotiate, query current state, and drive one
  idempotent reconciliation path.

## Test cases

Cover success reply followed by delayed event, event before local observation,
duplicate event, unrelated event, missing event, command error, QEMU shutdown,
monitor reconnect, device ID reuse attempt, and timeout racing completion. Store
sanitized QMP transcripts as fixtures with exact QEMU version/schema.

Primary sources:

- [QMP reference](https://www.qemu.org/docs/master/interop/qemu-qmp-ref.html)
- [QMP specification](https://www.qemu.org/docs/master/interop/qmp-spec.html)
