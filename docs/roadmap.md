# Roadmap Homes

Planned components whose directories do not exist yet. Per repository policy,
a component directory is created only when its implementation work begins;
this file preserves the ownership boundaries until then. Milestone plans under
`agent/plan/` remain the canonical schedule.

## Vulkan Execution Backend (`plugins/backend/vulkan/`, milestone-0.1.3.0)

The Vulkan backend is the planned milestone-0.1.3.0 / `v0.1.3` execution backend behind the
same versioned backend C ABI as the CPU backend. It owns Vulkan capability
discovery, Kernel IR to MLIR SPIR-V lowering, pipeline caching, memory-tier
negotiation, submission, and timeline completion.

Compiler and runtime implementations remain separate build targets. Vulkan
handles and types do not cross the backend ABI, and the CUDA compatibility
layer does not depend directly on this backend. Unsupported semantics produce
explicit capability diagnostics rather than silently switching an established
context to another backend.

## vfio-user Service (`services/metaflux-vfio-userd/`, milestone-0.1.1.0)

`metaflux-vfio-userd` now provides the bounded Unix `SOCK_SEQPACKET` service
entrypoint for the generated vfio-user control envelope. It accepts one static
guest connection at a time, delegates negotiation, GET_INFO, DMA map/unmap, and
terminal-loss handling to the transport server adapter, and closes its socket
without deleting a replacement path owned by another process. The current
entrypoint remains a control-plane service: backend lease binding, guest PCI
data-plane rings, and libvfio-user/QEMU qualification remain separate 0.x gates.
One process instance is intended to own the backend instance and queue mappings
for each accepted generation, while `metafluxd` remains the registry, policy,
and lease authority. Its transport half follows the halves convention in
[`transports/README.md`](../transports/README.md).

## Compiler Worker (`services/compiler-worker/`, milestone-0.1.0.0)

Planned isolated LLVM/MLIR compilation service. It consumes versioned compiler
requests, populates the shared content-addressed cache, and keeps compiler
frameworks outside provider processes and runtime fast paths.
