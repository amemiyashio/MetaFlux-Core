# Roadmap Homes

Milestone plans under `agent/plan/` are the canonical schedule. Every component
needed by the active route already has an owner and source home, so no new
top-level component directory is currently reserved here.

## Active v0.2 Route

milestone-0.2.0.0 reuses the established component boundaries while making the
stock client, rather than an internal compiler mechanism, the observable goal:

```text
references/catalog/pytorch/pytorch-v2.11.0.json
  -> exact on-demand PyTorch v2.11.0 gitlink readiness
  -> lane-stock-pytorch-cuda-baseline

stock PyTorch 2.11.0+cu126 normal torch.cuda path
  -> import -> driver enumeration -> runtime copy -> artifact intake -> eager add
  -> plugins/compat/cuda provider and profile translation
  -> versioned ecosystem-neutral client request
  -> services/metafluxd private compiler-worker mode
  -> compiler/core canonical Kernel IR
       |-> plugins/backend/cpu/compiler MLIR -> LLVM -> PIC ELF
       `-> plugins/backend/vulkan/compiler MLIR -> SPIR-V
```

The first lane must correlate eager add with daemon submission and CPU-backend
completion; provider-local tensor arithmetic is not acceptance evidence. The
reference entry is an implementation-readiness prerequisite only and is not
a product, build, qualification, or release input. The
two target branches are independent after canonical Kernel IR, and Vulkan never
routes through LLVM IR. MLIR remains inside compiler-worker and backend compiler
components, and no MLIR, CUDA, PyTorch, LLVM, or Vulkan type crosses the neutral
protocol or backend C ABI. A general cubin/SASS compiler, framework-specific
backend, or separate compiler service is not implied by this route. New
component homes are added only after a milestone allocates work that cannot fit
an existing owner.
