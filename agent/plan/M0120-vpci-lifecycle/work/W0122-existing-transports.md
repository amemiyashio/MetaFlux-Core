---
id: W0122
delivery: 0.1.2.2
milestone: M0120
status: Active
area: lifecycle.transports
depends_on: [W0121]
updated: 2026-08-31
---

# Lifecycle on Existing Transports

## Outcome

Apply one generation transaction to the M0100 memfd fallback, M0110 local cdev,
and M0110 guest vfio-user paths without changing the M0110 descriptor, base
ioctl/mmap UAPI, BAR layout, or steady-state ring.

`metaflux_core.ko`, `metaflux_pci.ko`, and `metaflux-vfio-userd` mirror the
committed lifecycle and own local reference safety. The guest worker uses the
private `metafluxd`/`metaflux-vfio-userd` control channel; `/dev/metafluxctl`
remains the local broker.

Admin reset, `VFIO_USER_DEVICE_RESET`, QMP remove/add, server/QEMU disconnect,
guest unload/reload, and daemon restart enter one request normalizer. No source
may publish a generation independently. Every add/reset stages an exclusive
worker lease before registry commit; drain/remove revokes new attachment before
retirement. Brokers reject a second binding, stale daemon incarnation, or
completion after revocation.

The cold-control QMP adapter correlates commands/events with lifecycle request
IDs. Remove publishes `QUIESCING` before `device_del` and waits for the matching
device-deleted event; QMP failure leaves a still-present function rejecting work
in `LOST`. Add stages server/socket and an unpublished generation before
`device_add`; BAR0 remains not-ready until commit. If QMP succeeds but commit
fails, the adapter removes the staged function, destroys staging, and leaves the
candidate consumed without advancing epoch because it never became a committed
generation. Partial success never restores a retired generation to `ONLINE`.

CUDA and NVML retain the M0100 provider-view epoch and shared
`registry_view_id`. Removal immediately makes old handles return `DEVICE_LOST`.
Later additions appear only after a later NVML zero-to-one init epoch or in a new
process, never as a new ordinal in an initialized CUDA process. Default
unfiltered membership/order is identical only when both providers captured the
same process-view revision; after NVML reinitializes while CUDA remains initialized,
their count/order may diverge. Common live incarnations match by
`(UUID, generation)`, never UUID/BDF alone. `CUDA_VISIBLE_DEVICES` affects CUDA
only.

## Work

- [x] Implement the runtime-owned coordinator transactions and bounded transport
  mirror callback boundary. The coordinator is the only component that commits
  generation/epoch state; callbacks receive a normalized event and cannot
  publish a replacement independently.
- [x] Exercise identical prepare, quiesce, drain, commit, abort, tombstone,
  `LOST`, and `ABSENT` paths with the memfd, cdev, and guest vfio-user mirror
  fixture registrations.
- [x] Connect the concrete C++ cdev worker and vfio-user server mirrors to the
  coordinator. Their steady-state protocol records remain unchanged; cdev
  drains published descriptors and rejects retired generations, while vfio-user
  gates DMA work and rejects stale generation/epoch pairs.
- [x] Connect the memfd fallback worker mirror to the coordinator. The existing
  C17 fast path remains the client half; the C++ adapter rejects stale
  generations, blocks submissions while quiescing, drains in-flight work, and
  retains a local lost/absent tombstone without allocating replacement identity.
- [x] Add the fixed external-event request normalizer used by transport and
  control adapters. It maps admin, VFIO-user, QMP, disconnect, and restart
  events to existing lifecycle requests; producer call-site integration remains
  open.
- [x] Add the vfio-user QMP command/event correlation fixture. It keeps one
  pending command, requires a matching add/delete event, and emits QMP loss on
  failed removal without changing the transport wire contract. The bounded
  `QmpSocket` Unix-stream adapter now supplies live command/event framing and
  classification; lifecycle producer wiring remains open.
- [x] Provide a QMP completion-to-ingress helper that snapshots the correlated
  event, maps failed remove to `QmpFailure`, and calls
  `submit_external_event`; callers can now feed it replies from `QmpSocket`,
  while non-QMP producer wiring remains open.
- [x] Provide a vfio-user EOF/error handoff that marks the local server lost and
  submits a captured `Disconnect` event through `submit_external_event`; the
  caller still owns lifecycle tuple capture and event-request allocation.
- [x] Bind the vfio-user `process_once` EOF/error result to that handoff through
  an overload that leaves ordinary message results unchanged; live QMP command
  transport is now covered by the bounded `QmpSocket`; restart producer wiring
  remains open.
- [x] Bind source metadata capture to an authority snapshot helper. QMP commands
  and vfio-user `process_once` can now capture logical device, daemon incarnation,
  identity record, generation, epoch, and deadline at observation time; the
  explicit event overload remains for callers that already own a captured tuple.
- [x] Provide a `QmpSocket`-to-lifecycle completion bridge. One receive returns
  separate transport and lifecycle outcomes, submits only a correlated reply
  through `QmpLifecycleAdapter` and `Coordinator`, keeps wrong-kind events
  pending, and leaves malformed/timeout/closed socket errors outside authority
  state mutation.
- [x] Capture and validate the provider view revision at Registry attach. The
  header and view-control revision must be nonzero and identical; CUDA and NVML
  retain the captured revision for their initialization epoch, keep membership
  frozen while initialized, and accept a newer revision only after a later
  zero-to-one initialization. Regressions cover before/during/after replacement
  plus malformed revision metadata.
- [ ] Integrate every reset/disconnect/restart source and inject failure at each
  staging, commit, DMA, completion, and teardown step.
- [x] Verify provider enumeration freeze before, during, and after replacement.

## Implemented stage

`metaflux::runtime::lifecycle::Coordinator` in
`runtime/core/include/metaflux/runtime/lifecycle.hpp` owns one logical-device
state, generation and identity high-water marks, the retirement epoch, bounded
request replay records, and immutable generation tombstones. Requests bind the
expected identity-record ID together with generation and epoch. `Mirror` callbacks
are a C++-only runtime-core boundary: transport code supplies a context and
prepare/quiesce/drain/commit/abort/loss hooks, while the coordinator supplies
the normalized request and candidate identity. The boundary deliberately does
not include M0110 descriptors, ioctl/mmap records, BAR definitions, QMP types,
or provider APIs.

Accepted add/reset/recover requests reserve a candidate before callbacks; a
failed pre-commit stage consumes that candidate but leaves the old identity and
epoch unchanged. A successful replacement retires the old generation and
increments epoch once. A partial commit never reopens the old generation: the
candidate becomes `LOST`. Transport loss preserves the current generation and
epoch as a lost tombstone. Remove retires the current generation to `ABSENT`,
and stale generation resolution returns `DeviceLost`.

The focused unit gate is
`metaflux.unit.runtime-lifecycle`. It covers all three mirror registrations,
duplicate/conflicting request IDs, stale generations, pre-commit candidate
consumption, transport-loss recovery, partial commit, remove/add, and checked
generation/epoch exhaustion. The memfd worker gate
`metaflux.transport.memfd-worker` adds failed-drain recovery, stale-generation
submission rejection, transport-loss recovery, and remove/add tombstone checks.
This stage is a preparation for the real adapter and qualification work below;
it does not claim those gates complete. The cdev and vfio-user transport tests
additionally exercise reset/loss callbacks and retired-generation rejection at
their protocol boundaries.

The live-control stage adds `metaflux::transport::vfio_user::QmpSocket`, a
host-independent Unix-stream adapter. It bounds each top-level JSON object to
64 KiB, rejects invalid command IDs and argument objects before sending,
classifies QMP greeting/reply/error/device events, and returns stable results
for timeout, closure, malformed input, and unexpected lifecycle messages. The
adapter is deliberately transport-only: it does not allocate lifecycle
identity, mutate generation/epoch state, retry commands, or submit events.
`QmpLifecycleAdapter` remains the composition point for correlation and
`Coordinator` remains the authority for state transitions. Its focused test
uses `socketpair` plus a pathname Unix listener and is included in the Vulkan
CTest preset. `QmpLifecycleAdapter::receive_and_submit` now composes one
socket receive with that correlation and ingress, reports a separate
`QmpLifecycleSocketOutcome`, preserves pending state for an unrelated or
wrong-kind event, and does not submit malformed, timeout, or closed-socket
results.

The provider-view stage now makes `process_view_revision` an explicit captured
part of the client Registry handle. Attach rejects zero or mismatched header and
view-control revisions. CUDA and NVML store the revision observed at their
zero-to-one initialization boundary; count and ordinal queries continue to use
that initialized membership while the backing fixture is changed, and a later
initialization captures the newer revision. This stage is intentionally
host-independent and does not claim live QMP, kernel, or physical-driver
qualification.

## Exit Gate

Local and guest CPU Add/Copy recover only through a new generation; old objects
return `DEVICE_LOST`; every failure reaches complete `ONLINE`, `LOST`, or
`ABSENT`; no transport bypasses the coordinator.
