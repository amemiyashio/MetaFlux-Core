---
id: work-item-2.0.0.3
delivery: 2.0.0.3
milestone: milestone-2.0.0.0
status: Queued
area: vulkan.physical
depends_on: [milestone-0.1.0.0, milestone-0.1.3.0]
updated: 2026-09-05
---

# Physical Dual-Driver Vulkan Qualification

## Outcome

Produce the physical AMD + NVIDIA dual-driver evidence that the
milestone-0.1.3.0 work items explicitly deferred (decision-0040): capability
and memory matrices, execution equivalence, validation-layer soak,
driver-change and non-completing-submission faults, transport gates on
physical ICDs, and the external-memory extension freeze.

## Work

- [ ] Close the exact physical AMD and NVIDIA GPU, driver, ICD, PCIe, and
  host-role reference matrix for every deferred row.
- [ ] Run the dual-family capability, packed-layout, cache-key, and
  memory-tier matrices from milestone-0.1.3.1/0.1.3.2 on both physical
  drivers; archive device/driver identities per sample.
- [ ] Run kernel equivalence, stream/event dependency, and CUDA-observable
  parity from milestone-0.1.3.4 on both physical drivers, with the
  validation layer enabled for the declared soak duration and zero
  validation errors.
- [ ] Run the milestone-0.1.3.6 driver-change, non-completing-submission,
  vulkan-local-transport, and vulkan-guest-transport physical rows; schedule
  overhead, transfer-efficiency, and lifecycle bounds are measured on
  physical ICDs, not lavapipe or synthetic profiles.
- [ ] Freeze the external-memory extension (OPAQUE_FD/DMA_BUF promotion) only
  after both driver families pass their ownership, coherence, and teardown
  matrices with the declared evidence.
- [ ] Archive device, driver, ICD, command, raw-sample, and revision identity
  for every row; preserve skipped rows as failures to qualify.

## Exit Gate

Every deferred physical dual-driver row passes on both driver families with
archived identity: capability and memory matrices agree with the
host-independent fixtures, execution matches independent references under a
clean validation soak, the milestone-0.1.3.6 physical performance rows stay
within their declared bounds, and the external-memory extension is frozen
from its schema with dual-driver evidence.
