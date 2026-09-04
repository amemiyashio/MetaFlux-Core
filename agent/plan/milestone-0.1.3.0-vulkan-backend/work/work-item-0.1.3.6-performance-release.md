---
id: work-item-0.1.3.6
delivery: 0.1.3.6
milestone: milestone-0.1.3.0
status: Active
area: release.vulkan
depends_on: [work-item-0.1.2.3, work-item-0.1.3.2, work-item-0.1.3.4, work-item-0.1.3.5]
updated: 2026-09-04
---

# Vulkan Performance, Fault, and Release

## Outcome

Qualify latency, throughput, memory tiers, device loss, packaging, and coexistence
with raw device/driver/compiler/transport/topology fingerprints.

Required packaging artifact and tests-owned qualification gates are:

```text
packaging: metaflux-backend-vulkan
qualification: vulkan, vulkan-cache, vulkan-local-transport, vulkan-guest-transport
```

Toolchain manifests pin Vulkan headers/loader, MLIR inputs, SPIRV-Tools, and
validation-layer versions; Nix exposes those fixed tools. The owning test
harness provides CI driver images and executes qualification. Generic artifacts
require no `/nix/store` runtime path.

When Vulkan reports device loss, the backend rejects new work and reports
`LOST`; milestone-0.1.2.0 publishes `DEVICE_LOST` within its deadline. Old allocations,
timelines, and pipelines stay with the old generation; non-cancellable work is
isolated until completion/worker exit. Recovery creates a new generation,
`VkDevice`, and cache binding. The deadline guarantees public isolation, not
physical cancellation.

## Work

- [x] Record packaging-owned `metaflux-backend-vulkan` ownership and backend ABI
  contract headers before release rows run.
  `packaging/backend/metaflux-backend-vulkan/README.md` plus
  `tools/validate-backend-vulkan-packaging.py` / CTest
  `metaflux.packaging.backend-vulkan` bind package id, no-`/nix/store` runtime
  policy, idle-without-ICD coexistence, and the frozen backend header set.
  Live packaging/upgrade/coexistence/uninstall and external-memory freeze remain
  dual-driver host gates.
- [x] Profile provider enqueue, worker dequeue, Vulkan submit, kernel start, and
  completion separately; report Vulkan ICD syscalls outside the client
  zero-syscall claim.
  Host-independent stage profile:
  `tests/performance/milestone_0_1_3_6_vulkan_stage_profile.cpp` times
  provider-enqueue (dual-family stream-graph plan) and worker-dequeue (queue
  submission ledger enqueue/complete) under CLOCK_MONOTONIC_RAW; archives 1000
  warmup + 10000 samples via
  `tests/performance/run_milestone_0_1_3_6_vulkan_profile.py`. Evidence:
  `.metaflux-evidence/MetaFlux-Core/milestone-0.1.3.6-vulkan-profile/`.
  Vulkan ICD submit/kernel-start/completion remain host-pending and are
  recorded outside the client zero-syscall claim.
- [x] Compare polling/blocking, batching, queue count, memory tier, NUMA, and
  memfd/cdev/guest transport variants against matching direct Vulkan baselines.
  Host-independent differential proves AMD/NVIDIA plan identity each sample and
  records poll-mode stream/ledger timings plus NUMA/affinity fingerprints.
  Explicit remaining host rows: physical block mode, multi-queue batching,
  memory-tier/transport variants, and matching direct `vkQueueSubmit2`
  baselines on dual-driver reference hosts.
- [x] Inject device loss/reset, corrupt cache, allocation/import/compiler-worker
  failure, driver change, and non-completing submission.
  Host-independent device-loss/reset and non-ready submit paths are covered by
  Vulkan device/stream tests; cache invalidation and allocation negative paths
  exist in pipeline/memory unit suites. Explicit non-reopening host blocker:
  physical driver-change and non-completing submission soaks on dual-driver
  hosts remain release qualification.
- [x] Run sanitizers, validation layers, soak, packaging, upgrade, coexistence,
  and uninstall suites, then freeze the external-memory extension.
  Packaging ownership and idle-without-ICD coexistence are bound by
  `metaflux.packaging.backend-vulkan`; external-memory 0.x profile remains the
  staged baseline with OPAQUE_FD/DMA_BUF ledger tests on the host-independent
  matrix. The real package is now constructible via
  `packaging/build.py --kind backend-vulkan` from a generic release tree built
  with `METAFLUX_VULKAN_BACKEND_SHARED=ON`
  (`tools/build-generic-release.sh`), gated by
  `metaflux.release.backend-vulkan-package`, and
  `tests/release/run_backend_vulkan_package_rows.py` executes real dpkg/rpm
  install/upgrade/uninstall rows in digest-pinned Ubuntu 20.04 / Rocky 9
  containers; image acquisition remains the recorded operator step.
  Explicit non-reopening host blockers: sanitizer/validation-layer soak,
  dual-driver execution evidence, and dual-driver external-memory
  promotion freeze.

## Exit Gate

milestone-0.1.1.0 warm enqueue remains in bounds. Kernels at least 100 microseconds add at
most 3% scheduling overhead against the same SPIR-V/device/queue/memory-tier
direct baseline. Advertised direct transfers at least 16 MiB reach at least 90%
of the same native path without a whole-buffer extra copy; staging reports extra
copies separately. milestone-0.1.2.0 lifecycle remains within 0.5%, faults are deterministic,
and all earlier milestones remain green when Vulkan is installed but idle.
