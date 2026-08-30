---
status: Current
updated: 2026-08-30
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
  (W0106) rather than merged enumeration.
- Ecosystems such as CUDA and future ROCm-facing interfaces are compatibility
  plugins; CPU, Vulkan, and later targets are execution backends.
- vPCI is presentation and transport, not the identity of the product and not a
  claim to implement a vendor-private kernel ABI. Presentation may adopt a
  synthetic vendor identity as a compatibility disguise (D0008); the canonical
  device identity remains the persistent MetaFlux UUID.

Product releases use standard three-part SemVer from the repository-root
[`VERSION`](../../VERSION). Delivery planning uses the related four-part
`MAJOR.MINOR.PATCH.WORK` coordinate and derived M/W/S identifiers described in
the [release and delivery identity policy](../../docs/release-versioning.md).

The current product line is M0100 / `v0.1.0`: the CPU-backed CUDA/NVML core
foundation and its four-distribution generic release qualification. Intel host
qualification, physical NVIDIA binding-performance promotion, and native NixOS
VM/package qualification are the `v0.2.0` support expansion, not M0100 exit
gates. Canonical scope and acceptance remain in the
[repository overview](../../README.md), [plugin ownership](../../plugins/README.md),
and [M0100](../plan/M0100-core-foundation/plan.md); recorded evidence is valid
only for the Git revision and invocation it names.
