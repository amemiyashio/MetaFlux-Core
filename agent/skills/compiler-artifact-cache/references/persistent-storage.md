# Persistent Artifact Transactions

Start at `PersistentArtifactCache` in
[`artifact_cache.hpp`](../../../../compiler/core/include/metaflux/compiler/artifact_cache.hpp)
and its implementation in
[`artifact_cache.cpp`](../../../../compiler/core/src/artifact_cache.cpp).
Read the relevant transition below; storage repair does not require reopening
every target feature or compiler pipeline decision.

## Reservation and competing publishers

Mutable entries are under `users/<uid>/epoch-<epoch>/<prefix>/<digest>`;
administrator AOT uses its separate epoch root. Cache state holds the global
lock, per-UID/key locks and reservation records. Private state directories/files
are validated for type, ownership and permissions with no-follow opens.
Do not replace a checked object with an unchecked path or silently share mutable
content across peer UIDs.

`reserve` acquires the bounded per-key lock before `State::mutex`: the current
publisher needs that mutex to release its lock. Preserve the monotonic timeout
and optional request deadline; this API has no stop-token parameter. Waiting
on this lock is distinct from cancelling an already spawned compiler worker.
After locking, inspect again: `EntryAvailable` tells the caller to lookup the
other publisher's result instead of compiling it again.

Global accounting includes live cross-process reservation records and existing
entries. Enforce maximum entry, per-UID/global byte quotas and reserved free
space; retain saturating arithmetic and avoid double-counting reclaimed bytes
after sampling filesystem space. Candidate-scan I/O failure is not empty usage.
Eviction uses last-use order with deterministic tie breaks and skips live pins;
all-pinned exhaustion returns a stable quota error. Cancel failed compilation's
reservation and reconcile abandoned records without removing a live holder.

## Publication and crash boundaries

`publish_entry` writes the artifact and complete metadata into a sibling private
temporary directory. Preserve file fsync, metadata fsync, directory fsync,
rename and parent-directory fsync. AOT files/directories become read-only before
publication. A same-key competing publication succeeds only for equivalent
validated content; it must not overwrite different bytes or descriptors.

Faults before rename leave no committed partial entry. A fault reported after
rename can leave a wholly visible, valid entry; inspect that state before retry
instead of assuming every I/O error means absence. `reconcile` removes stale
mutable temporaries/reservations and invalid unpinned content. It neither creates
nor mutates the administrator AOT tree. `install_aot` has its own publication
path independent of mutable-state locks and quota reservations.

## Lookup, corruption and lifetime

`lookup` validates key/metadata/epoch, artifact length and digest, plus the
optional backend validator. A mutable hit retains an fd-backed shared `flock`
pin while last-use metadata is atomically refreshed. Eviction/invalidation needs
the exclusive entry lock; do not erase live code to recover quota or corruption.
The consuming module retains its pin and loaded-code handle until its lifetime
ends. Backend signature/helper mismatch handling releases its own pin before
requesting mutable invalidation; another reader may still keep the entry live.

Runtime lookup preserves AOT content even if invalid and proceeds according to
the declared mode/tier policy; explicit administrator repair is separate.
`invalidate` rejects the administrator tier. Report miss, metadata mismatch,
quota and I/O errors distinctly rather than turning corruption into execution
or repeatedly overwriting the same entry.

## Focused evidence

The existing
[artifact_cache_test.cpp](../../../../compiler/core/tests/artifact_cache_test.cpp)
contains isolation/corruption/epoch cases, each publication fault boundary,
quota/free-space accounting, lock-path validation, cross-instance and forked
reservation/pin tests, live-holder timeout/recovery and read-only AOT precedence.
Choose the changed transition, then preserve the owning
`metaflux.unit.artifact-cache` coverage in the formal plan. Keep true process
tests for process-shared lock/pin behavior; same-object mocks are insufficient.
Backend pipeline checks additionally prove executable validation, failed-compile
reservation cleanup and actual cold/warm/AOT behavior. They do not replace the
stock client or physical-device evidence required by the active Exit Gate.
