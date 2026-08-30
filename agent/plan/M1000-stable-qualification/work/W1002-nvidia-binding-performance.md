---
id: W1002
delivery: 1.0.0.2
milestone: M1000
status: Queued
area: performance.binding
depends_on: [M0100, W1001]
updated: 2026-08-30
---

# Physical NVIDIA Binding Performance

## Outcome

Produce the complete physical NVIDIA evidence needed to promote the declared
performance budgets to binding for `v1.0.0`. The physical device is the native
and passthrough reference, not a new MetaFlux execution backend.

## Work

- [ ] Close the exact NVIDIA GPU, driver, PCIe, NUMA, and AMD/Intel host-role
  reference matrix.
- [ ] Run schema-v2 same-path native H2D and D2H baselines with complete GPU,
  driver, library, benchmark, command, placement, and raw-sample identity.
- [ ] Run managed-path, passthrough-loss, stock `nvidia-smi` interference,
  launch, copy, event, initialization, and hot-getter measurements with the
  declared warm-up, sample, and stopping rules.
- [ ] Require every strict binding field and audit to complete; preserve skipped
  or incomplete rows as failures to qualify rather than provisional passes.
- [ ] Compare AMD and Intel reference roles as separate artifacts and promote
  M1000 budgets to binding only after every required row passes.

## Exit Gate

The strict binding harness exits successfully for every approved host/device
row, H2D and D2H each meet their declared same-path native threshold,
passthrough and stock-tool interference meet their budgets, and the canonical
M1000 plan records `budgets: binding` with the exact evidence revisions.
