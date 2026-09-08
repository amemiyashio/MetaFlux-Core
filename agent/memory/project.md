---
status: Current
updated: 2026-09-08
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
target is milestone-0.2.0.0 / `v0.2.0`, which builds a PyTorch CUDA transparent
compatibility foundation in three ordered boundaries: pinned stock PyTorch
through the five-stage daemon/CPU baseline, a complete CPU execution profile,
then Vulkan/SPIR-V and lifecycle qualification (decisions 0044 and 0046).
milestone-1.0.0.0 / `v1.0.0` remains queued until this foundation closes.

Current v0.2 maturity is a provider-side compatibility prototype, not qualified
daemon/backend execution (decision-0044). The repository has a five-stage probe,
pinned client manifests, and selected PyTorch kernel-name handlers, but CTest
exercises the probe with a fake client and no checked-in real-client operator
corpus exists. Epoch-0013 preserves the shortest observable stock-client
path: eager add must move through a minimal versioned neutral request into
canonical Kernel IR and the daemon CPU backend before the surface and corpus
broaden. Its first implementation lane now has a research-only readiness
prerequisite: the exact PyTorch v2.11.0 gitlink and catalog entry under
`references/` must be materialized and verified before source inspection.
This reference is not product, build, qualification, or release evidence
(decision-0047). MLIR remains internal to backend compilation; Vulkan
qualification then reuses the same Kernel IR corpus (decision-0046).
Intel x86_64 qualification and physical NVIDIA
binding-performance promotion belong to milestone-2.0.0.0 / `v2.0.0`
(decision-0040), and native NixOS VM/package qualification remains the
unallocated `v0.3.0` support expansion. Canonical scope and acceptance
remain in the [repository overview](../../README.md),
[plugin ownership](../../plugins/README.md), and
[milestone-0.1.0.0](../plan/milestone-0.1.0.0-core-foundation/plan.md); recorded evidence is valid only
for the Git revision and invocation it names.
