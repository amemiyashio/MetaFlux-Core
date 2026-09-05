---
status: Current
updated: 2026-09-06
---

# MetaFlux Project

MetaFlux is a low-overhead compatibility and execution substrate for unmodified
accelerator applications. It presents familiar ecosystem interfaces while
routing managed execution through an ecosystem-neutral runtime and selectable
execution backends.

The product direction is:

- Transparent, Wine-style compatibility rather than a new required application
  API.
- Minimum initialization and steady-state overhead; warm paths avoid compiler
  frameworks, allocation, global locks, and unnecessary RPC.
- One authoritative logical-device view within each managed domain across
  compatibility providers, management interfaces, transports, and later device
  presentation; cross-vendor coexistence uses explicit loader namespaces
  (work-item-0.1.0.6) rather than merged enumeration.
- Ecosystems such as CUDA and future ROCm-facing interfaces are compatibility
  plugins; CPU, Vulkan, and later targets are execution backends.
- vPCI is presentation and transport, not the identity of the product and not a
  claim to implement a vendor-private kernel ABI. Presentation may adopt a
  synthetic vendor identity as a compatibility disguise (decision-0008); the canonical
  device identity remains the persistent MetaFlux UUID.

Product releases use standard three-part SemVer from the repository-root
[`VERSION`](../../VERSION). Delivery planning uses the related four-part
`MAJOR.MINOR.PATCH.WORK` coordinate and derived M/W/S identifiers described in
the [release and delivery identity policy](../../docs/release-versioning.md).

The `v0.1.x` product line is complete through `v0.1.3`: the CPU-backed
CUDA/NVML core foundation (milestone-0.1.0.0), local cdev and guest transport
(milestone-0.1.1.0), lifecycle and experimental vPCI presentation
(milestone-0.1.2.0), and the Vulkan execution backend (milestone-0.1.3.0) are
all delivered with evidence recorded beside their work items. The current
target is milestone-1.0.0.0 / `v1.0.0`, the stable compatibility release with
provisional budgets; Intel x86_64 qualification and physical NVIDIA
binding-performance promotion belong to milestone-2.0.0.0 / `v2.0.0`
(decision-0040). Native NixOS VM/package qualification remains the unallocated
`v0.2.0` support expansion. Canonical scope and acceptance
remain in the [repository overview](../../README.md),
[plugin ownership](../../plugins/README.md), and
[milestone-0.1.0.0](../plan/milestone-0.1.0.0-core-foundation/plan.md); recorded evidence is valid only
for the Git revision and invocation it names.
