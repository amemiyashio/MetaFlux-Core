# Shared Telemetry Publication

Read for telemetry bank/control changes, publisher recovery and reader races.
[Record ownership](registry-records.md) defines the control and publisher records;
[admission/read brackets](registry-admission.md) defines the outer lifetime check.
NVML units, per-field errors and stock-tool presentation belong to
[$nvml-telemetry-compatibility](../../nvml-telemetry-compatibility/SKILL.md) skill.

Telemetry uses two banks and one tagged publisher owner. A writer claims its
publish record through `FREE -> INITIALIZING`, fills old/intended payload and
recovery data, release-publishes `PREPARED`, accepts `LINKING`, and establishes
exclusive `WRITING(record_tag)` ownership. It reacquires and compares the expected
even latch/control after ownership. A stale value before bank touch atomically
aborts that record, releases exact ownership, and retries with a fresh record; it
never leaves `WRITING` stuck. The active writer checked-adds the full
64-bit snapshot sequence and one complete 64-bit control-latch pair, CASes the
expected even latch to odd before touching the inactive bank, writes that bank and
control payload through aligned `mf_atomic_*` words, and release-publishes the next
even latch. It then marks the record `PUBLISHED`, releases exact ownership, and
marks terminal. Recovery that observes the exact target latch/control/bank supplies
only missing markers and never republishes. `PUBLISHED+WRITING(tag)` only releases
ownership; `ACTIVE+WRITING(tag)` with the unchanged old even latch aborts/releases
only after proven death, while a live timeout quarantines. A reader acquires an
even latch, copies the atomic control payload,
reads the selected atomic bank, and accepts only if a second acquire returns the
same even latch. The final sequence encoding and final latch pair are reserved;
inability to retain one complete publication starts view close before odd/bank
write. Only proven owner death permits a helper to finish or restore old control.
A mere deadline closes and quarantines the view/banks until owner quiescence/death,
so a resumed writer cannot corrupt a reused bank. Neither counter can wrap, two
live-view writers cannot overlap, and no lock-free 128-bit CAS is required.

## Affected scenarios

Select the scenarios for the mechanism being changed and the active work item's
required qualification; run them after the coherent implementation.

- Race loss/removal fence publication against telemetry bank publication, retry,
  and fallback; no call after observing the fence may recover stale `ONLINE`
  liveness from telemetry.
- Shorten telemetry latch and snapshot-sequence widths, pause a reader, publish
  through two bank cycles, and approach exhaustion. Require atomic bank/control
  payload words, identical even-latch bracketing, no mixed-bank acceptance, and
  view close before either terminal encoding or the inactive bank is consumed.
- Race two telemetry publishers through record initialization/linking and expected
  even-to-odd CAS; exactly one owns the bank. Two stale prepared targets must make
  the later owner abort/release before bank touch and retry. Kill its process at
  every store/even/marker/ownership-release boundary and reconcile without replay.
  For a live but expired owner, require view/bank quarantine with no helper write
  or reuse until owner quiescence/death.

Canonical source: [work-item-0.1.0.2](../../../plan/milestone-0.1.0.0-core-foundation/work/work-item-0.1.0.2-contracts-runtime.md).
