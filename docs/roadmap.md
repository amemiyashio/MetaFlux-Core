# Roadmap Homes

Planned components whose directories do not exist yet. Per repository policy,
a component directory is created only when its implementation work begins;
this file preserves the ownership boundaries until then. Milestone plans under
`agent/plan/` remain the canonical schedule.

## Vulkan Execution Backend (`plugins/backend/vulkan/`, M0130)

The Vulkan backend is the planned M0130 / `v0.1.3` execution backend behind the
same versioned backend C ABI as the CPU backend. It owns Vulkan capability
discovery, Kernel IR to MLIR SPIR-V lowering, pipeline caching, memory-tier
negotiation, submission, and timeline completion.

Compiler and runtime implementations remain separate build targets. Vulkan
handles and types do not cross the backend ABI, and the CUDA compatibility
layer does not depend directly on this backend. Unsupported semantics produce
explicit capability diagnostics rather than silently switching an established
context to another backend.

## vfio-user Service (`services/metaflux-vfio-userd/`, M0110)

Planned QEMU vfio-user PCI server and leased guest data-plane worker. One
process instance owns the backend instance and queue mappings for each accepted
generation, while `metafluxd` remains the registry, policy, and lease
authority. Its transport half follows the halves convention in
[`transports/README.md`](../transports/README.md).

## Compiler Worker (`services/compiler-worker/`, M0100)

Planned isolated LLVM/MLIR compilation service. It consumes versioned compiler
requests, populates the shared content-addressed cache, and keeps compiler
frameworks outside provider processes and runtime fast paths.
