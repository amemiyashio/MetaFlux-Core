---
status: Active
updated: 2026-09-01
governance_epoch: D0029
focus_mode: product
focus_owner: S0112-20260901-001-m0110-w0112-current-epoch
milestone: M0110
workstream: W0112
checkpoint: P20260901-098
---

# Current Progress

## Execution Focus

SC0007 is Applied at effective revision
`97248239d507028309df07aaa4d1f462388cf4b6`. D0029 execution governance is
explicitly breaking and destructive. The sole product owner is
`S0112-20260901-001-m0110-w0112-current-epoch`, a schema version 2 session that
declares the current D0029 epoch.

There is no compatibility, grandfather, fallback, upgrade, or direct
reactivation path for pre-SC0007 sessions. All 82 schema version 1 ledgers and
72 transient guidance files are absent from the current tree. Their IDs resolve
only through `agent/sessions/liquidated-v1.json` as administrative tombstones.
Work proceeds from current canonical repository files only.

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
  queue-only region COPY stages recorded through P081. The current epoch also
  binds embedded daemon memory objects to persistent CPU backend handles with
  explicit operation-reference draining. P094 adds same-fd current cdev view /
  generation discovery and lease-bound mapping of the data-owner payload;
  P095 adds the canonical lease-bound payload query, so worker mapping no longer
  depends on a local size convention; daemon-side live lease/import remains
  open. P096 confirms the base/lifecycle manifest closure and full 85-test
  suite after that UAPI update. P097 serializes query lease-state checks with
  cdev release under the same lock. P098 extends that invariant to every cdev
  ioctl, mmap, and poll read of mutable per-file negotiation, lease, queue, and
  registered-memory authorization state; the full 85-test suite and Linux
  6.18 Kbuild remain green.
- The live product path is still absent: `Session::serve()` starts only the
  Unix-ring `EmbeddedCpuWorker`; the cdev client region descriptors refer to a
  daemon object table that the standalone cdev client does not create or bind,
  and the CPU backend currently imports host virtual ranges rather than kernel
  registered-memory handles. D0030 now closes the provider cdev selection and
  M0100 fallback boundary: cdev is first, its queue binds to the same Unix
  session/view/generation, and memfd is a pre-success fallback only for
  `ENOENT`, `ENODEV`, or explicit ABI incompatibility. Provider wiring remains
  open until that binding is implemented.
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

1. Connect the current daemon object table and leased worker/backend binding to
   live cdev registered-memory handles using the current W0112 source and tests;
   the worker-side payload mapping slice is complete.
2. Prove unmodified Add/Copy through `/dev/metafluxN`, including
   replacement-generation isolation and fd/VMA tombstone behavior, then
   execute the W0112 fault matrix with KUnit, KASAN, KCSAN, lockdep, and
   kmemleak on Linux 6.12 and 6.18.

## Blockers

- Live cdev qualification needs Linux 6.12 or 6.18 with module/device-node
  activation and the required privileges. This is an environment requirement,
  not a physical NVIDIA dependency.
- W0112 remains incomplete until live device-node Add/Copy and kernel fault
  evidence exist. Host-independent fixtures and mapped COPY alone do not close
  it.
- M0120 and M0130 content work waits for an explicit focus handoff after the
  M0110 dependency boundary advances.

## Evidence Pointers

- [P093 daemon cdev backend reference drain](checkpoints/2026/P20260901-093-m0110-daemon-cdev-reference-drain.md)
- [P094 current cdev worker payload mapping](checkpoints/2026/P20260901-094-m0110-cdev-worker-payload-mapping.md)
- [P095 lease-bound cdev payload query](checkpoints/2026/P20260901-095-m0110-cdev-payload-query.md)
- [P096 manifest closure after cdev payload query](checkpoints/2026/P20260901-096-m0110-cdev-payload-query-closure.md)
- [P097 serialized cdev payload query lease checks](checkpoints/2026/P20260901-097-m0110-cdev-payload-query-lock.md)
- [P098 serialized cdev per-file state checks](checkpoints/2026/P20260901-098-m0110-cdev-per-file-state-lock.md)
- [P092 breaking governance applied](checkpoints/2026/P20260901-092-breaking-governance-applied.md)
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
