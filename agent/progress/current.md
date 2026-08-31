---
status: Active
updated: 2026-09-01
governance_epoch: D0029
focus_mode: governance
focus_owner: S0100-20260831-047-breaking-governance-epoch
milestone: M0110
workstream: W0112
checkpoint: P20260901-091
---

# Current Progress

## Execution Focus

D0029 and Active SC0007 are making execution-focus governance explicitly
breaking. The current owner is
`S0100-20260831-047-breaking-governance-epoch`; only this bounded migration may
change durable content until the destructive settlement is committed and
SC0007 is applied.

There is no compatibility, grandfather, fallback, or direct reactivation path
for pre-SC0007 sessions. All 82 schema version 1 ledgers and 72 transient
guidance files have been removed from the candidate current tree. Their compact
IDs resolve only through `agent/sessions/liquidated-v1.json`; continuing an old
objective requires current canonical files, a newly scaffolded epoch-bearing
successor, and an atomic focus handoff.

## Product Resume Target

Resume product work at [M0110](../plan/M0110-kernel-guest-transport/plan.md) /
[W0112](../plan/M0110-kernel-guest-transport/work/W0112-local-cdev.md), whose
canonical [Exit Gate](../plan/M0110-kernel-guest-transport/work/W0112-local-cdev.md#exit-gate)
requires live local-device CPU Add/Copy, replacement-generation isolation, and
kernel fault qualification through `/dev/metafluxctl` and `/dev/metafluxN`.

This target is dependency-valid: M0100 is Complete and M0110 is the first
incomplete milestone in the active chain. M0120 depends on M0110; M0130 depends
on M0110 and M0120. Recorded downstream work remains evidence, but it is not the
default scheduling authority while M0110 is incomplete.

## Current Boundary

- M0100 / `v0.1.0` is Complete with recorded CPU-backed CUDA/NVML Add/Copy,
  stock `nvidia-smi`, generic package matrices, reproducibility, sanitizer, and
  provenance evidence. Those claims remain revision-bound.
- W0112 has paired rings, queue/payload lifetime, bounded registered-memory and
  DMA mapping, backend operation leases, CPU Add/Copy dispatch, asynchronous
  completion, generation-bound references, daemon object-table activation, and
  queue-only region COPY stages recorded through P081.
- P089 records the runtime-owned immediate producer ingress at `6152efa`: admin
  reset, VFIO-user reset, disconnect, and daemon restart capture one authority
  snapshot; wrong-route QMP, malformed, unknown, and stale observations are
  covered. Actual producer call sites and live qualification remain open.
- W0113 has static host-independent vfio-user negotiation, PCI binding, and
  guest-ring stages. Real QEMU/libvfio-user DMA, BAR/MSI-X, guest Add/Copy, and
  fault qualification remain open.
- M0120 lifecycle/QMP/provider-view and M0130 Vulkan stages are recorded but do
  not satisfy their milestone Exit Gates and do not outrank M0110.

## Next Actions

1. Complete the candidate residual audit and all record, semantic-change,
   identity, hook, and Claude gateway regressions.
2. Commit the destructive liquidation, then mark SC0007 Applied against that
   exact effective revision.
3. Scaffold a current-epoch W0112 successor and atomically transfer product
   focus before resuming implementation.

## Blockers

- Live cdev qualification needs Linux 6.12 or 6.18 with module/device-node
  activation and the required privileges. This is an environment requirement,
  not a physical NVIDIA dependency.
- W0112 remains incomplete until live device-node Add/Copy and kernel fault
  evidence exist. Host-independent fixtures and mapped COPY alone do not close
  it.
- Product work remains paused until the liquidation commit is verified, SC0007
  is Applied, and the new epoch-bearing W0112 owner is installed atomically.
- M0120 and M0130 content work waits for an explicit focus handoff after the
  M0110 dependency boundary advances.

## Evidence Pointers

- [P091 destructive governance epoch](checkpoints/2026/P20260901-091-destructive-governance-epoch.md)
- [P090 execution-focus governance](checkpoints/2026/P20260831-090-execution-focus-governance.md)
- [P089 immediate producer ingress](checkpoints/2026/P20260831-089-m0120-producer-ingress.md)
- [P081 queue-only cdev region COPY](checkpoints/2026/P20260831-081-m0110-cdev-queue-only-region-copy.md)
- [P080 daemon object-table activation](checkpoints/2026/P20260831-080-m0110-cdev-daemon-object-activation.md)
- [P078 cdev worker lease](checkpoints/2026/P20260831-078-m0110-cdev-worker-lease.md)
- [P068 vfio-user negotiation](checkpoints/2026/P20260831-068-m0110-vfio-user-negotiation.md)

## Deferred Boundaries

- M0120: retain lifecycle, QMP socket/bridge, provider-view freeze, and
  qualification evidence; resume only after an explicit dependency-valid focus
  handoff.
- M0130: retain Vulkan capability, memory, queue, SPIR-V preflight/reflection,
  and cache evidence; actual Kernel IR lowering, pipelines, `vkQueueSubmit2`,
  backend composition, and driver-family qualification remain open.
- M1000 / `v1.0.0`: Intel x86_64 support and physical NVIDIA binding-performance
  qualification. Native NixOS VM/package qualification remains unallocated
  `v0.2.0` scope.

## Tool Boundary

Nix fixes and exposes declared tools only. Git owns source identity; CMake and
Ninja own builds; CTest and repository harnesses own verification; packaging
owns artifacts; focus owns scheduling and commit authority; sessions own compact
work evidence and cleanup; host operators own Nix-store retention.
