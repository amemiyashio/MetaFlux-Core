---
id: W0136
delivery: 0.1.3.6
milestone: M0130
status: Queued
area: release.vulkan
depends_on: [W0123, W0132, W0134, W0135]
updated: 2026-08-30
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
`LOST`; M0120 publishes `DEVICE_LOST` within its deadline. Old allocations,
timelines, and pipelines stay with the old generation; non-cancellable work is
isolated until completion/worker exit. Recovery creates a new generation,
`VkDevice`, and cache binding. The deadline guarantees public isolation, not
physical cancellation.

## Work

- [ ] Profile provider enqueue, worker dequeue, Vulkan submit, kernel start, and
  completion separately; report Vulkan ICD syscalls outside the client
  zero-syscall claim.
- [ ] Compare polling/blocking, batching, queue count, memory tier, NUMA, and
  memfd/cdev/guest transport variants against matching direct Vulkan baselines.
- [ ] Inject device loss/reset, corrupt cache, allocation/import/compiler-worker
  failure, driver change, and non-completing submission.
- [ ] Run sanitizers, validation layers, soak, packaging, upgrade, coexistence,
  and uninstall suites, then freeze the external-memory extension.

## Exit Gate

M0110 warm enqueue remains in bounds. Kernels at least 100 microseconds add at
most 3% scheduling overhead against the same SPIR-V/device/queue/memory-tier
direct baseline. Advertised direct transfers at least 16 MiB reach at least 90%
of the same native path without a whole-buffer extra copy; staging reports extra
copies separately. M0120 lifecycle remains within 0.5%, faults are deterministic,
and all earlier milestones remain green when Vulkan is installed but idle.
