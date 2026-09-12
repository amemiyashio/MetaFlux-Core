# nvidia-smi Qualification

Qualify stock binaries, not a reimplemented parser. Record tool build/version,
selected NVML target, locale, invocation, exit status, stdout/stderr digest, and
the registry fixture used.

Start from the requested view in the
[stock-tool harness](../../../../tests/compatibility/run_nvidia_smi_acceptance.py).
Use [provider test registration](../../../../plugins/compat/cuda/management/nvml/tests/CMakeLists.txt)
for the affected semantics, policy-setter or CUDA/NVML mode check. Implement the
missing producer/mapping before broad target qualification. The views below
define the supported-tool qualification matrix; select the required subset for
a bounded behavior change and the complete matrix when that is the assignment.

## Required views

- `nvidia-smi -L` for zero, one, and multiple devices.
- Default summary output.
- The approved CSV query set with and without headers/units.
- `--query-compute-apps` or the selected release's equivalent process view.
- Required `-q` and XML `-x` combinations, including unsupported fields.

## Assertions

- NVML device count/order and immutable identity match the membership revision it
  captured at initialization. The default, unfiltered CUDA view uses the same
  live membership/order only when it captured the same process-view revision;
  dynamic NVML fields come from one internally consistent published telemetry
  sequence, not from a presumed CUDA telemetry snapshot.
- With `CUDA_VISIBLE_DEVICES` filtering/reordering active, NVML count/order stay
  canonical and unchanged. Join common live CUDA/NVML incarnations by
  `(UUID, generation)` rather than ordinal or BDF alone. If NVML captured a later
  lifecycle revision than initialized CUDA, count/order may intentionally differ.
- Unsupported physical fields remain structurally present where the stock tool
  expects them and render as `N/A`; no invented zero is accepted as evidence.
- XML remains well formed and stable enough for the selected compatibility
  contract. CSV escaping, units, ordering, and per-field errors are exact.
- Concurrent init/shutdown and snapshot publication do not crash, hang, mix
  generations, or produce an out-of-range process row.

Measure 1 Hz polling impact and hot getter latency only with pinned affinity,
warm-up, sample count, and raw distributions. The active milestone owns the
threshold and whether it is provisional or binding.
