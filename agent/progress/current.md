---
status: Active
updated: 2026-09-01
governance_epoch: D0029
focus_mode: product
focus_owner: S0112-20260901-005-m0110-w0112-post-privilege-governance
milestone: M0110
workstream: W0112
checkpoint: P20260901-102
---

# Current Progress

## Execution Focus

M0110/W0112 product work is active under
`S0112-20260901-005-m0110-w0112-post-privilege-governance`. SC0009 is Applied at
`c4cedfe`; D0032 makes `manage-host-privilege` the sole sudo/su, root-helper,
host-authorization, post-Nix package, privileged driver, and revocation owner.
Nix remains first and limited to clear, stable, evolvable tool identity,
materialization, and exposure. Product commands and evidence remain with their
domain workflows.

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
  6.18 Kbuild remain green. P099 adds the source-level provider cdev-first
  handshake, same-session/view/generation binding, daemon cdev worker pump,
  and CPU backend COPY/primary-entry LAUNCH resolution with explicit memory
  references. P100 adds an executable live cdev qualification gate for the
  generated ioctl/mmap ABI, eventfd lease, payload query, registered-memory
  unregister, malformed/stale requests, and owner-close VMA tombstones. The
  full suite now has 86 tests; 85 execute successfully and this live gate is
  explicitly skipped while `/dev/metaflux0` is absent. These are source and
  fixture closures, not live device proof.
- The live product path remains unqualified: `/dev/metafluxctl` and
  `/dev/metaflux0` are absent on this host. The provider and daemon now have a
  single initialization epoch and a cdev data-plane route under D0030, while
  kernel registered-memory/DMA import, replacement-generation isolation, and
  fd/VMA tombstone behavior still require an activated device node.
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

1. Activate the current `metaflux_core.ko` and `/dev/metafluxctl` plus
   `/dev/metafluxN` through `$manage-host-privilege` and the W0112 workflow.
2. Run the live cdev qualification for CPU Add/Copy and registered-memory/DMA
   import, preserving exact device/view/generation binding.
3. Complete replacement-generation isolation and the Linux 6.12/6.18 fault,
   sanitizer, lockdep, and kmemleak matrix required by the Exit Gate.

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

- [P102 host privilege escalation](checkpoints/2026/P20260901-102-host-privilege-escalation.md)
- [P101 agent startup resolution](checkpoints/2026/P20260901-101-agent-startup-resolution.md)
- [P100 live cdev qualification harness](checkpoints/2026/P20260901-100-m0110-cdev-live-qualification.md)
- [P093 daemon cdev backend reference drain](checkpoints/2026/P20260901-093-m0110-daemon-cdev-reference-drain.md)
- [P094 current cdev worker payload mapping](checkpoints/2026/P20260901-094-m0110-cdev-worker-payload-mapping.md)
- [P095 lease-bound cdev payload query](checkpoints/2026/P20260901-095-m0110-cdev-payload-query.md)
- [P096 manifest closure after cdev payload query](checkpoints/2026/P20260901-096-m0110-cdev-payload-query-closure.md)
- [P097 serialized cdev payload query lease checks](checkpoints/2026/P20260901-097-m0110-cdev-payload-query-lock.md)
- [P098 serialized cdev per-file state checks](checkpoints/2026/P20260901-098-m0110-cdev-per-file-state-lock.md)
- [P099 cdev provider and daemon worker binding](checkpoints/2026/P20260901-099-m0110-cdev-provider-daemon-binding.md)
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

D0031 requires Git-aware Nix-first executable resolution. Missing tools enter
the repository declaration first; `manage-toolchain` must prove a Nix gap
before D0032 permits host package escalation. `manage-host-privilege` owns every
sudo/su, root-helper, persistent-grant, privileged driver, and revocation
operation without owning the consuming workflow. Nix owns only clear,
reproducible, stable tool-version identity, materialization, and exposure. A
fixed version describes the current revision, not permanent immutability; later
governed manifest/lock updates may evolve it.
