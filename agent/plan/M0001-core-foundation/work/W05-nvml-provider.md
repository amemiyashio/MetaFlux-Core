---
id: M0001-W05
milestone: M0001
status: Queued
area: compat.cuda.nvml
depends_on: [M0001-W02, M0001-W04]
updated: 2026-08-27
---

# NVML Provider and stock nvidia-smi

## Outcome

Expose the same logical devices and processes through the target NVML ABIs and
supported stock `nvidia-smi` views.

## Required Surface and Semantics

The provider exports the complete selected target-version symbol table and
implements init/shutdown/version strings; enumeration/index/UUID/BDF/name/compute
capability; quota/committed memory; 100 ms capacity-weighted GPU utilization;
compute processes; compute/persistence modes; and required field queries.

Unsupported physical telemetry returns `NVML_ERROR_NOT_SUPPORTED` and displays
`N/A`. Memory utilization is not synthesized from used/total. Hot getters read
the shared snapshot; setters use the Unix control socket and return only after
policy is effective.

## Work

- [ ] Generate manifests and structure assertions for selected R535, R550, R570,
  R580, and R610 headers.
- [ ] Implement the required function families, direct shared-page getters, and
  socket-based setters.
- [ ] Validate zero/one/multiple devices and CUDA/NVML identity parity.
- [ ] Test stock `nvidia-smi -L`, summary, CSV, `compute-apps`, and required
  `-q/-x` combinations for every target version.

## Exit Gate

Supported stock tools show `MetaFlux Virtual Compute Device` with consistent
UUID/BDF/memory/process state, correct mode policy, and `N/A` for unsupported
physical fields. Hot getters meet the milestone latency budget.
