# Runtime Lifecycle Coordinator

`metaflux::runtime::lifecycle::Coordinator` is the runtime-owned authority for
one logical device's generation and epoch lifecycle. It accepts normalized
requests from admin, memfd, cdev, vfio-user, QMP, disconnect, and restart
sources. A request is identified by its ID, source, operation, daemon
incarnation, expected generation/epoch, and deadline. Replays of an identical
request are idempotent; a reused ID with different fields is a conflict.

Transport implementations register at most one bounded `Mirror` for each of
`memfd`, `cdev`, and `vfio-user`. The coordinator invokes all registered mirrors
in the same prepare, quiesce, drain, commit sequence. The callbacks receive a
normalized `MirrorEvent` containing the old identity and any reserved candidate.
They do not receive a state-publish API, so no transport can create an
independent replacement generation. Abort and loss callbacks provide local
cleanup and tombstone notification after a failed or partial transaction.

Candidate generation and identity-record high-water marks are checked before
acceptance and are never reused, including when staging fails. Epoch is checked
before a retirement and advances only with the single authority commit. A
partial commit marks the candidate `Lost` instead of reopening the retired
generation. Transport loss preserves the current generation and epoch; recovery
uses a new candidate. Removed and replaced generations remain bounded
tombstones and resolve as `DeviceLost`.

This header is an in-process runtime contract. It intentionally contains no
M0110 descriptor, Linux UAPI, BAR, vfio-user wire, QMP, CUDA, or NVML type.
Those layers remain adapters and consume the coordinator without redefining its
state machine. The focused regression executable is
`runtime/core/tests/lifecycle.cpp`, registered as
`metaflux.unit.runtime-lifecycle`.
