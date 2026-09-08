---
id: work-item-0.2.0.3
delivery: 0.2.0.3
milestone: milestone-0.2.0.0
status: Draft
area: compatibility
depends_on: [work-item-0.2.0.2, milestone-0.1.3.0]
updated: 2026-09-08
---

# PyTorch CUDA Multi-Backend Qualification

## Outcome

The pinned PyTorch CUDA profile is qualification-ready: required stream, event,
allocator, teardown, and fault semantics pass, and the same canonical Kernel IR
corpus executes through independent CPU/LLVM and Vulkan/SPIR-V MLIR branches.

## Work

- [ ] Qualify the torch-relied runtime semantics through the provider:
  stream/event ordering, the caching allocator's allocation pattern,
  multi-stream concurrency, synchronization, and deterministic teardown, with
  fault rows for daemon death mid-kernel.
- [ ] Close the daemon execution-mode and Vulkan routing decisions; implement
  routing so a framework launch can select the Vulkan backend, gated by the
  backend's existing qualification suites.
- [ ] Lower the accepted canonical Kernel IR through the Vulkan backend's MLIR
  SPIR-V conversion with an explicit target environment, legality checks,
  reflection, and `spirv-val`; do not route Vulkan through LLVM IR.
- [ ] Run the Vulkan-backed eager corpus bit-exact against the CPU-backed run
  on the RADV adapter using the same checked-in corpus manifest and archive
  both sample sets with topology/device fingerprints under `tmp/outputs/`.
- [ ] Publish checked-in baseline and frontier gap manifests with exact client,
  provider, daemon, backend, compiler, and cache identities.
- [ ] Verify the cumulative 0.1.x regression and generic-package policy stay
  green with the framework client installed but idle.

## Exit Gate

The pinned real client runs the same canonical Kernel IR corpus bit-exact
through CPU/LLVM and Vulkan/SPIR-V daemon routes; all MLIR conversion leaves
only target-legal operations; streams, events, allocator, teardown, and fault
rows pass; baseline/frontier gap manifests are published; and the full
milestone-0.1.x regression stays green.
