# Roadmap Homes

Milestone plans under `agent/plan/` are the canonical schedule. Every component
needed by the active route already has an owner and source home, so no new
top-level component directory is currently reserved here.

## Active v0.2 Route

milestone-0.2.0.0 reuses the established component boundaries:

```text
CUDA/PyTorch client
  -> plugins/compat/cuda provider and profile translation
  -> versioned ecosystem-neutral client request
  -> services/metafluxd private compiler-worker mode
  -> compiler/core canonical Kernel IR
       |-> plugins/backend/cpu/compiler MLIR -> LLVM -> PIC ELF
       `-> plugins/backend/vulkan/compiler MLIR -> SPIR-V
```

The two target branches are independent after the canonical Kernel IR boundary;
Vulkan never routes through LLVM IR. MLIR remains inside compiler-worker and
backend compiler components, and no MLIR, CUDA, PyTorch, LLVM, or Vulkan type
crosses the neutral protocol or backend C ABI. A general cubin/SASS compiler,
framework-specific backend, or separate compiler service is not implied by this
route. New component homes are added here only after a milestone allocates work
that cannot fit an existing owner.
