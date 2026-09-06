---
id: work-item-0.2.0.1
delivery: 0.2.0.1
milestone: milestone-0.2.0.0
status: Draft
area: compat.cuda
depends_on: [work-item-0.1.0.4]
updated: 2026-09-06
---

# Torch Client Bring-Up

## Outcome

The pinned baseline PyTorch client enumerates the MetaFlux virtual device
through the stock provider and daemon: probe stages import,
driver-enumeration, and runtime-copy pass with no client-side patches.

## Work

- [x] Trace torch's cudart/ATen initialization against the provider and name
  the exact entry point answered `CUDA_ERROR_NOT_SUPPORTED` (driver error 36)
  in the 2026-09-06 baseline run.
- [ ] Implement the missing provider driver-API entries required for device
  enumeration and primary-context establishment, with negative fixtures for
  each and no regression in the existing ABI symbol gates.
- [ ] Drive the baseline profile probe through `driver-enumeration` with
  `--require-stage driver-enumeration`, then extend to `runtime-copy`
  (device-resident tensor allocation, host transfer, copy-back) through the
  daemon's memory path.
- [ ] Bind the required driver surface into the provider capability report,
  cache identity, and the compatibility probe manifest so artifacts miss when
  the surface changes.

## Measured progress (2026-09-07)

Root cause named: cudart 12.6 resolves its complete driver dispatch table in
one initialization sweep and then probes graph, user-object, capture, and
async-notification entry points; every unresolved or unimplemented probe
aborts `cudaGetDeviceCount` before `cuDeviceGetCount` is ever reached. The
provider advertised driver version 13030 while implementing a CUDA 12.0-era
surface — an over-claim that made cudart require the entire post-12.0 table.

Landed in this iteration:

- The advertised driver version drops to 12000, matching the implemented
  surface (ABI gate updated; the NVML compatibility target keeps its R610
  identity).
- 84 declared surface entries added to symbols.def/abi.h: the CUDA 12
  library/kernel family, graph-exec update family, stream capture and
  priority queries, context limits and cache config, device attribute and
  peer queries, profiler and export-table stubs, plus per-symbol gap thunks
  generated from the observed lookup trace (`provider_gap_stubs.c`).
- `cuDeviceGetAttribute` is functional for the sm_70 virtual-device identity;
  module function resolution is permissive (unknown names resolve to tokens
  and fail at launch, not at lookup).

Remaining blocker, named precisely: cudart requires the internal driver
export table `6bd5fb6c-5bf4-e74a-8987-d93912fd9df9` through
`cuGetExportTable`; the layout is undocumented and a null or zero-filled
table segfaults libcudart's reader, while NOT_FOUND aborts initialization
with error 500. Populating this table (RE of libcudart's reader, or
delegation to a real driver) is the next iteration's entire scope. Until it
closes, `cudaGetDeviceCount` returns 500 through the provider while direct
driver-API enumeration (cuInit/cuDeviceGetCount/cuDeviceGetName) works.

## Exit Gate

`pytorch_cuda_probe.py --profile baseline --require-stage runtime-copy`
passes against a stock daemon, the ABI/smoke gates stay green, and the traced
init sequence with the implemented entry points is recorded beside this work
item.
