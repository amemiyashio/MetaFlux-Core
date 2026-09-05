---
id: work-item-0.3.0.1
delivery: 0.3.0.1
milestone: milestone-0.3.0.0
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

- [ ] Trace torch's cudart/ATen initialization against the provider and name
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

## Exit Gate

`pytorch_cuda_probe.py --profile baseline --require-stage runtime-copy`
passes against a stock daemon, the ABI/smoke gates stay green, and the traced
init sequence with the implemented entry points is recorded beside this work
item.
