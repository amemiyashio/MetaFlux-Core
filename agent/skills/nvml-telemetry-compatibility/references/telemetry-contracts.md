# Telemetry Contracts

Use a field table containing NVML API/field ID, source owner, unit, aggregation,
window, update cadence, maximum age, privilege, unsupported result, and
`nvidia-smi` presentation.

## First source path

Start with one requested field or setter in
[provider.c](../../../../plugins/compat/cuda/management/nvml/src/provider.c).
Read the [current field table](../../../../plugins/compat/cuda/management/nvml/README.md)
for its exact source, units and window. Change only the affected producer and
mapping; do not turn the whole table into a prerequisite for an unrelated query.

| Requested behavior | Trace |
| --- | --- |
| GPU or memory utilization | `nvmlDeviceGetUtilizationRates` → `mf_nvml_read_telemetry_locked` → `mf_client_registry_read_telemetry_v1` in [fastpath.c](../../../../runtime/client/fastpath/src/fastpath.c) |
| Real measured work/window publication | `RegistryAuthority::record_work`, `publish_locked`, `refresh_telemetry` in [server.cpp](../../../../services/metafluxd/src/server.cpp) |
| Memory total/used/free | `nvmlDeviceGetMemoryInfo` and `_v2`, lifecycle quota plus the published telemetry source |
| Compute process rows | `nvmlDeviceGetComputeRunningProcesses*` → `mf_client_process_snapshot_fetch_v1`/`fill_v1` in [fastpath.c](../../../../runtime/client/fastpath/src/fastpath.c) |
| Persistence/compute setters | `nvmlDeviceSetPersistenceMode`/`nvmlDeviceSetComputeMode` → `mf_nvml_set_policy_locked` and the effective lifecycle fence |

Use [$runtime-contracts-registry](../../runtime-contracts-registry/SKILL.md) skill
when a snapshot, shared reader bracket or producer contract changes. This NVML
topic owns field meaning, unit conversion, caller output and error mapping.
Use the existing measured producer when present; add missing measurements in
their owning layer before claiming a new telemetry capability.

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
- GPU utilization is a documented capacity-weighted value over the milestone-0.1.0.0
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

For process reporting, distinguish a negotiated compute session from a live
CUDA context. The [client contract](../../../../contracts/protocol/client/v1/README.md)
owns context-acquire/release accounting and sealed snapshot lifetime. Preserve
checked memory sums, PID/start-time identity, device generation, revision and
count/fill validation instead of deriving process presence from DSO loading.

Check the changed getter with publication/loss races and the changed setter with
permission, timeout and effective-fence confirmation. Preserve distinct per-row
statuses in field batches; a successful batch call does not mean every field is
supported. Select actual stock-tool checks through
[qualification](nvidia-smi-qualification.md).
