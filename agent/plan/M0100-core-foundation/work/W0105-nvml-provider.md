---
id: W0105
delivery: 0.1.0.5
milestone: M0100
status: Active
area: compat.cuda.nvml
depends_on: [W0102, W0104]
updated: 2026-08-30
---

# NVML Provider and stock nvidia-smi

## Outcome

Expose the canonical logical-device inventory and processes through the target
NVML ABIs and supported stock `nvidia-smi` views. Default unfiltered CUDA/NVML
live identity and order agree for the same captured initial process-view revision;
CUDA-only filtering does not alter NVML inventory.

## Required Surface and Semantics

The provider exports the complete selected target-version symbol table and
implements init/shutdown/version strings; enumeration/index/UUID/BDF/name/compute
capability; quota/committed memory; 100 ms capacity-weighted GPU utilization;
compute processes; compute/persistence modes; and required field queries.

Unsupported physical telemetry returns `NVML_ERROR_NOT_SUPPORTED` and displays
`N/A`. Memory utilization is not synthesized from used/total. Hot getters read
the shared telemetry snapshot after a no-fallback lifecycle-fence check; setters
use the Unix control socket and return only after policy is effective in that
control fence.

## Work

- [x] Generate manifests and structure assertions for selected R535, R550, R570,
  R580, and R610 headers.
- [x] Implement the required function families, direct shared-page getters, and
  socket-based setters.
- [x] Validate zero/one/multiple devices; require default unfiltered CUDA/NVML
  count/order parity for the same initial revision, then map a filtered/reordered
  CUDA view to unchanged NVML rows by `(UUID, generation)` rather than ordinal or
  BDF alone.
- [x] Test stock `nvidia-smi -L`, summary, CSV, `compute-apps`, and required
  `-q/-x` combinations for every target version.

## Exit Gate

Supported stock tools show `MetaFlux Virtual Compute Device` with consistent
UUID/BDF/memory/process state, correct mode policy, and `N/A` for unsupported
physical fields. Hot getters meet the milestone latency budget.
