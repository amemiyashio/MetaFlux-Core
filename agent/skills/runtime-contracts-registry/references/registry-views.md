# Registry Views

Read for identity, membership, provider join/freeze or fresh-view creation.
For record fields and reclamation use [registry records](registry-records.md);
for stateful side effects use [admission](registry-admission.md).

The daemon owns logical-device identity, policy, generation, and worker leases.
A process negotiates one shared `registry_view_id`; provider DSOs join that view
instead of keeping mutable per-DSO registry copies. The shared ID fixes selection
policy and default order; it does not claim that providers initialized at
different times always have identical membership.

Keep these views distinct:

- the authoritative registry and its stable default order;
- the process selection view shared by participating providers;
- provider membership snapshots, each labeled by provider, initialization epoch,
  and `process_view_revision`;
- CUDA's initialized ordinal set, which `CUDA_VISIBLE_DEVICES` may filter or
  reorder without changing NVML;
- NVML's zero-to-one initialization epoch;
- immutable per-generation identity records, each named by a never-reused
  `identity_record_id`;
- a separately published, monotonically sequenced lifecycle fence used for
  liveness, admission, state, epoch, effective quota, and policy;
- generation/sequence-tagged telemetry snapshots, which reference identity
  records and never duplicate UUID, BDF, generation, or lifecycle state fields.

Default unfiltered CUDA and NVML live membership/order must agree when both
providers captured the same `process_view_revision`. After CUDA filtering, or
when provider initialization revisions differ, count and ordinal parity are not
required. UUID or logical ID correlates a persistent device across replacement,
but live-incarnation parity requires `(UUID, generation)` from the same applicable
view revision; a stable BDF is not an incarnation key.

Loss marks the captured `identity_record_id` and generation lost immediately. A
re-add creates a new identity record and generation. It never enters an initialized
CUDA ordinal set and may appear in NVML only after the contract's next allowed
initialization epoch. An old CUDA entry and a new NVML entry can therefore share
UUID/BDF while intentionally having no live-incarnation parity.

The full `registry_view_id` is a never-reused mapping incarnation minted from a
fresh daemon incarnation and checked per-daemon serial. New mappings never reuse
the old full ID even if storage/address is reused. Tokens, handles, memberships,
leases, and reader snapshots bind it; view-ID/serial exhaustion rejects creation,
so counter restart in a fresh view cannot form cross-view ABA.

## Affected scenarios

Select the scenarios for the mechanism being changed and the active work item's
required qualification; run them after the coherent implementation.

- Stress provider join/freeze, `CUDA_VISIBLE_DEVICES` isolation, zero/one/many
  devices, loss/addition, generation replacement, identity/generation/epoch
  exhaustion without wrap, ordered view-global lifecycle-sequence allocation,
  abandoned-writer recovery, terminal exhaustion, same-revision parity,
  cross-revision divergence, and stale handles.
- Close a view, negotiate a fresh mapping, and replay stale tokens, handles, leases,
  and reader snapshots. The never-reused full view incarnation must reject all of
  them even if gate and lifecycle counters restart.

Canonical source: [work-item-0.1.0.2](../../../plan/milestone-0.1.0.0-core-foundation/work/work-item-0.1.0.2-contracts-runtime.md).
