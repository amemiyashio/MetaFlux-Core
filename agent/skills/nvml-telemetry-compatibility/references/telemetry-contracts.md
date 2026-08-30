# Telemetry Contracts

Use a field table containing NVML API/field ID, source owner, unit, aggregation,
window, update cadence, maximum age, privilege, unsupported result, and
`nvidia-smi` presentation.

## Truthfulness rules

- Static device identity and canonical enumeration order come from the
  authoritative read-only registry view. Liveness, lifecycle state, epoch, quota,
  and effective policy come from the monotonic runtime lifecycle fence and never
  fall back to telemetry. Dynamic metric/counter fields come from their declared
  producers in one published snapshot sequence. A telemetry row references its
  immutable `identity_record_id` and observed lifecycle sequence; it does not
  duplicate UUID, BDF, generation, or lifecycle state. Persistent UUID/logical ID
  correlates replacement history, while live CUDA/NVML parity requires
  `(UUID, generation)` from an applicable common view revision.
- GPU utilization is a documented capacity-weighted value over the M0100
  100-millisecond window. Name numerator, denominator, idle behavior, saturation,
  and timestamp.
- Memory utilization is not derived from used/total unless the canonical
  contract explicitly defines that measurement. Unsupported physical sensors,
  thermals, clocks, power, fan, ECC, and link telemetry return
  `NVML_ERROR_NOT_SUPPORTED` and display `N/A`.
- Process rows use stable PID/start-time identity or another reuse-safe key,
  generation-bound accounting, and explicit visibility/permission behavior.
- Snapshot readers observe one published sequence. Use release/acquire or a
  validated sequence protocol; never combine fields from different epochs.
- A prior telemetry snapshot is eligible only under its field maximum-age rule and
  when its lifecycle sequence is not older than the fence observed at call entry.
  Race loss/removal with telemetry publication and retry; stale `ONLINE` liveness
  is always forbidden.
- Stale data handling is explicit: return the last sample with age metadata only
  where the API permits it, otherwise report a stable error or unsupported state.

## Setters

Setters are control-plane operations. Validate privilege and requested value,
send a request with idempotence identity, and return success only after the
monotonic control fence reflects policy. A transport acknowledgement or telemetry
bank alone is not success. Timeout and partial failure must not leave a falsely
reported mode, and getters never recover an older policy through telemetry
fallback.
