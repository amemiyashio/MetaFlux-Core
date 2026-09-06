---
id: work-item-0.2.0.3
delivery: 0.2.0.3
milestone: milestone-0.2.0.0
status: Draft
area: compatibility
depends_on: [work-item-0.2.0.2]
updated: 2026-09-06
---

# Framework Qualification and Execution Routing

## Outcome

Framework clients are a qualified, released capability: stream/event and
allocator semantics torch relies on pass, the daemon routes framework clients
to a qualified execution backend beyond CPU (Vulkan rows included), and both
client profiles carry release-facing evidence.

## Work

- [ ] Qualify the torch-relied runtime semantics through the provider:
  stream/event ordering, the caching allocator's allocation pattern,
  multi-stream concurrency, synchronization, and deterministic teardown, with
  fault rows for daemon death mid-kernel.
- [ ] Close decision items 3-4: the daemon execution-mode surface for
  framework clients and the Vulkan daemon-routing shape; implement the
  routing so a framework launch can select the Vulkan backend, gated by the
  backend's existing qualification suites.
- [ ] Run the Vulkan-backed eager corpus bit-exact against the CPU-backed run
  on the RADV adapter and archive both sample sets with topology/device
  fingerprints under `tmp/outputs/`.
- [ ] Record the frontier profile gap list as release-facing documentation and
  keep the roadmap's probe/promotion boundary current.
- [ ] Verify the cumulative 0.1.x regression and generic-package policy stay
  green with the framework client installed but idle.

## Exit Gate

The baseline client runs the eager corpus bit-exact through the daemon on
both qualified execution backends (CPU and Vulkan where its rows are closed),
streams/events/allocator and fault rows pass, the frontier gap list is
published, and the full 0.1.x regression stays green.
