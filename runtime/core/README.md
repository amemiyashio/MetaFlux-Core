# Runtime Lifecycle Coordinator

`metaflux::runtime::lifecycle::Coordinator` is the runtime-owned authority for
one logical device's generation and epoch lifecycle. It accepts normalized
requests from admin, memfd, cdev, vfio-user, QMP, disconnect, and restart
sources. A request is identified by its ID, source, operation, daemon
incarnation, expected identity-record generation/epoch tuple, and deadline.
Replays of an identical request are idempotent; a reused ID with different
fields is a conflict.

`lifecycle_normalizer.hpp` defines the fixed external-event vocabulary and maps
admin add/remove/reset, VFIO-user reset, QMP add/remove, disconnect, and daemon
restart to the existing `Request` source/operation pair. It validates the
request envelope and returns an unsupported result for unknown event kinds. The
normalizer has no state and does not reserve candidates, advance epochs, or
publish lifecycle state. `submit_external_event` is the canonical ingress that
normalizes an event and submits only the resulting request to the Coordinator;
malformed or unknown events stop before authority state is changed.
`capture_external_event` copies the authority snapshot tuple at the producer
observation point, while the producer still supplies the request ID, event kind,
and optional deadline. A later authority advance therefore leaves the captured
event stale and visible to normal request validation instead of silently
rebinding it.

Immediate producers use `capture_and_submit_external_event`. It captures the
tuple and submits it through the same ingress in one producer call for admin
reset, VFIO-user reset, disconnect, and daemon restart. Other event kinds return
`Unsupported` before authority mutation. Delayed QMP completions continue to use
the pre-captured event overload so command correlation retains the
observation-time identity. A concurrent authority advance is reported as
`Stale`; the helper never recaptures against the replacement generation.

`ProducerIngress` owns request-ID sequencing for a coordinator-owned producer
boundary. Its `capture` method records the authority tuple once and is passed to
correlated QMP or transport completion adapters; `submit_immediate` is reserved
for reset, loss, and restart events that have no delayed completion. IDs never
wrap: exhaustion returns an invalid event and does not reuse an earlier request.

Transport implementations register at most one bounded `Mirror` for each of
`memfd`, `cdev`, and `vfio-user`. The coordinator invokes all registered mirrors
in the same prepare, quiesce, drain, commit sequence. The callbacks receive a
normalized `MirrorEvent` containing the old identity and any reserved candidate.
They do not receive a state-publish API, so no transport can create an
independent replacement generation. Abort and loss callbacks provide local
cleanup and tombstone notification after a failed or partial transaction.

Mirror ownership is explicit: an adapter that owns a mirror context must call
`unregister_mirror` while the coordinator is still alive and before destroying
that context. The coordinator serializes removal with lifecycle submission and
compacts the bounded mirror table, so later transactions cannot call a stale
owner.

Candidate generation and identity-record high-water marks are checked before
acceptance and are never reused, including when staging fails. Epoch is checked
before a retirement and advances only with the single authority commit. A
partial replacement commit marks the candidate `Lost` instead of reopening the
retired generation. An accepted remove reserves its retirement epoch and
tombstone before mirror callbacks; if a mirror fails during prepare, drain, or
commit, the authority still retires the old identity to `Absent` and publishes a
loss fence to every mirror. Transport loss preserves the current generation and
epoch; recovery uses a new candidate. Removed and replaced generations remain
bounded tombstones and resolve as `DeviceLost`.

The bounded replay and tombstone tables are sized for the work-item-0.1.2.3 qualification
envelope: 4,096 request records and 2,048 immutable tombstones. A 1,000-cycle
reset/remove/add run consumes 3,000 request records, 2,000 tombstones, and
2,000 generation/identity candidates. Capacity exhaustion returns
`ResourceExhausted`; there is no eviction or implicit garbage collection that
could make a replay or an old object ambiguous.

The Coordinator serializes its public control-plane operations with one
reentrant authority mutex. This protects lifecycle state, replay records, and
tombstones when reset/remove/add requests race with read-only open, mmap, and
telemetry observations or duplicate submit replays. Mirror callbacks run inside
the same authority transaction and may take a consistent snapshot; transport
payloads and provider fence payloads retain their own synchronization owners.

Telemetry publication is bound to the same authority. A producer row must carry
the current identity and exact stable lifecycle sequence while device admission
is `OPEN` and the fence is `ONLINE`. The runtime checks that tuple before taking
the telemetry latch, again while the latch is odd, and once more after staging
the inactive bank. An old or future sequence returns `RETRY`; a closed or lost
device returns `DEVICE_LOST`; malformed identity data returns
`INVALID_ARGUMENT`. Recovery validates a marker-complete target bank against
the current fences before promoting it, so an owner-death race cannot make stale
telemetry `READY`. Readers still perform the final fence recheck described by
the shared telemetry contract.

This header is an in-process runtime contract. It intentionally contains no
milestone-0.1.1.0 descriptor, Linux UAPI, BAR, vfio-user wire, QMP, CUDA, or NVML type.
Those layers remain adapters and consume the coordinator without redefining its
state machine. The focused regression executable is
`runtime/core/tests/lifecycle.cpp`, registered as
`metaflux.unit.runtime-lifecycle`.
