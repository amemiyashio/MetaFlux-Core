---
id: work-item-0.2.0.3
delivery: 0.2.0.3
milestone: milestone-0.2.0.0
status: Queued
area: compatibility
depends_on: [work-item-0.2.0.2, milestone-0.1.3.0]
updated: 2026-09-08
---

# PyTorch CUDA Vulkan Qualification

## Outcome

The pinned stock PyTorch CUDA profile is qualification-ready: required stream,
event, allocator, synchronization, teardown, and fault semantics pass, and the
same canonical Kernel IR corpus executes through independent CPU/LLVM and
Vulkan/SPIR-V daemon routes.

## Work

- [ ] Close the Vulkan daemon routing shape and qualification matrix before
  implementing this lane's framework route.
- [ ] Route the work-item-0.2.0.2 corpus through the Vulkan backend without
  changing its neutral request or canonical Kernel IR meaning.
- [ ] Lower accepted Kernel IR through the Vulkan backend's MLIR SPIR-V
  conversion with an explicit target environment, legality checks, reflection,
  and `spirv-val`; do not route Vulkan through LLVM IR.
- [ ] Qualify stream/event ordering, caching-allocator behavior, multi-stream
  concurrency, synchronization, deterministic teardown, and daemon loss
  mid-kernel.
- [ ] Run Vulkan results bit-exact against the CPU-backed run on the qualified
  RADV adapter using the same checked-in corpus manifest. Keep host measurement
  samples and topology/device fingerprints under `tmp/outputs/`.
- [ ] Publish checked-in baseline and frontier gap manifests with exact client,
  provider, daemon, backend, compiler, target-environment, and cache identities.
- [ ] Verify cumulative milestone-0.1.x regression and generic-package policy
  remain green with the framework client installed but idle.

## Exit Gate

The Vulkan route decision is closed; pinned stock PyTorch runs the same
canonical Kernel IR corpus bit-exact through CPU/LLVM and Vulkan/SPIR-V daemon
routes; all MLIR conversion leaves only target-legal operations; streams,
events, allocator, synchronization, teardown, and daemon-loss rows pass;
baseline/frontier gap manifests are current; and the full milestone-0.1.x
regression stays green.
