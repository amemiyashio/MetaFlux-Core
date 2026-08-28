# Vulkan Backend

The Vulkan backend is a planned v0.2 execution backend behind the same versioned
backend C ABI as the CPU backend. It owns Vulkan capability discovery, Kernel IR
to MLIR SPIR-V lowering, pipeline caching, memory-tier negotiation, submission,
and timeline completion.

Compiler and runtime implementations remain separate build targets. Vulkan
handles and types do not cross the backend ABI, and the CUDA compatibility layer
does not depend directly on this backend. Unsupported semantics produce explicit
capability diagnostics rather than silently switching an established context to
another backend.
