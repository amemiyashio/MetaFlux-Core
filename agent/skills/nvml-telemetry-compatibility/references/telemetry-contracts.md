# Telemetry Contracts

Use a field table containing NVML API/field ID, source owner, unit, aggregation,
window, update cadence, maximum age, privilege, unsupported result, and
`nvidia-smi` presentation.

## Truthfulness rules

- Identity, quota, committed memory, process attachment, generation, and state
  come from the authoritative registry/shared snapshot.
- GPU utilization is a documented capacity-weighted value over the M0001
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
- Stale data handling is explicit: return the last sample with age metadata only
  where the API permits it, otherwise report a stable error or unsupported state.

## Setters

Setters are control-plane operations. Validate privilege and requested value,
send a request with idempotence identity, and return success only after the
effective snapshot reflects policy. A transport acknowledgement alone is not
success. Timeout and partial failure must not leave a falsely reported mode.
