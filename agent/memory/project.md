---
status: Current
updated: 2026-08-27
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
- One authoritative logical-device view across compatibility providers,
  management interfaces, transports, and later device presentation.
- Ecosystems such as CUDA and future ROCm-facing interfaces are compatibility
  plugins; CPU, Vulkan, and later targets are execution backends.
- vPCI is presentation and transport, not the identity of the product and not a
  claim to implement a vendor-private kernel ABI.

The current repository is an engineering bootstrap of boundaries and build
fixtures, not a functional CUDA/NVML implementation. Canonical scope and
acceptance remain in the [repository overview](../../README.md),
[plugin ownership](../../plugins/README.md), and
[M0001](../plan/M0001-core-foundation/plan.md).
