# NVML Compatibility Provider

This directory builds the MetaFlux-managed `libnvidia-ml.so.1` compatibility
provider. It is installed below `${libdir}/metaflux/providers`, outside the
global vendor library lookup directory. A MetaFlux launcher or another explicit,
process-scoped selection mechanism must load this DSO; packaging must not add it
to `ldconfig` or replace a vendor-owned `libnvidia-ml.so` path.

The provider connects lazily on the first NVML initialization call. Its device
order remains the canonical registry order and is intentionally unaffected by
`CUDA_VISIBLE_DEVICES`. Unsupported physical telemetry remains `N/A` through
typed NVML errors rather than fabricated measurements.

`METAFLUX_MODE` accepts exactly `managed`, `passthrough`, or `auto`; an unset
value means `auto`. The value and parse result freeze at the first valid NVML
initialization attempt. `managed` uses only the MetaFlux session,
`passthrough` uses only one validated CUDA/NVML vendor pair, and `auto` may
transfer to that pair only after managed rollback proves the provider pristine.
There is no constructor-time selection.

All 131 entries in `symbols.def` use the frozen runtime decision, including
legacy/versioned aliases and `nvmlErrorString`. Passthrough initialization and
shutdown reference counts mirror vendor calls. The final successful shutdown
waits for active calls, releases NVML before CUDA, and a later initialization
reloads the same frozen pair selection. A forked child or stale fingerprint
returns `NVML_ERROR_GPU_IS_LOST`; a missing vendor entry returns
`NVML_ERROR_FUNCTION_NOT_FOUND`.

The CMake target pins the full driver, NVML, and CUDA Driver API versions exposed
to the selected stock-tool family. R610 is the default; qualification builds for
older families set the three `METAFLUX_NVML_TARGET_*` cache values from the
corresponding immutable tool/header manifest.

The selected R535 through R610 stock tools share one internal export-table UUID.
The provider completes that loader handshake with the pinned table size; private
slots without a real implementation return `NVML_ERROR_NOT_SUPPORTED`. Public
identity, memory, process, and telemetry calls remain the authoritative surface.

The public manifest includes the legacy names still requested by older stock
tools, including the pre-v2 driver-model, clocks-reason, remapped-row, fabric,
power-state, accounting, and application-clock queries. Unsupported scalar and
array queries validate their required inputs and return a typed
`NVML_ERROR_NOT_SUPPORTED`. Field-value batches instead return success after
marking every requested row `NVML_ERROR_NOT_SUPPORTED`, as required by NVML's
per-row status contract; no physical value is written. The three public PCI
info versions return the canonical registry domain, bus, device, function, and
both legacy and current BDF strings; unavailable vendor and subsystem IDs stay
zero.

| NVML field | Shared source | Unit | Aggregation/window | Update/age | Unsupported result |
| --- | --- | --- | --- | --- | --- |
| GPU utilization | telemetry `active_time_ns` | percent | union of measured execution intervals divided by one 100 ms single-device capacity window, floored and saturated at 100 | daemon publishes at most every 25 ms; idle windows decay to zero | lifecycle/telemetry error |
| Memory total/used/free | lifecycle quota plus telemetry used bytes | bytes | canonical device total and current process-authority aggregate | one telemetry snapshot sequence | lifecycle/telemetry error |
| Memory utilization | telemetry `memory_active_time_ns` | percent | union of successful COPY intervals and successful LAUNCH intervals whose prepared Kernel IR contains a global load/store, divided by the independent 100 ms window, floored and saturated at 100; never derived from byte occupancy | daemon publishes at most every 25 ms; idle windows decay to zero | lifecycle/telemetry error |
| Process memory | sealed process snapshot | bytes | checked sum by PID/start-time and device generation | monotonic snapshot revision | empty table is zero rows |
| Persistence mode | lifecycle fence `policy_bits[0]` | enum | effective daemon policy | getter reads one stable fence; setter uses the observer control socket | setter requires daemon-owner/root peer |
| Compute mode | lifecycle fence `policy_bits[2:1]` | enum | default/exclusive-thread/prohibited/exclusive-process | getter reads one stable fence; setter returns after fence publication and confirmation | setter requires daemon-owner/root peer |

The persistence and compute setters require negotiated
`MF_CLIENT_CAP_POLICY_SETTERS_V1`. They key requests by immutable identity record,
preserve unrelated policy bits, and accept success only when the response's
lifecycle sequence is visible in a stable fence with the requested value.
