# Compiler

The compiler owns MetaFlux Kernel IR and Graph IR, common optimization passes,
cache-key construction, diagnostics, and backend-independent compilation
orchestration.

LLVM/MLIR lives in compiler services or workers, never in an application-side
provider. Ecosystem inputs such as PTX belong to their compatibility-layer
plugin, while target lowering belongs to the corresponding execution backend.
The compiler core must remain independent of CUDA, ROCm, Vulkan, and concrete
transport implementations.

Frontend and target selection are independent build roles. For example,
`plugins/compat/cuda/compiler/ptx` depends on compiler core to translate PTX into
Kernel IR. Planned target-lowering paths such as
`plugins/backend/cpu/compiler` and `plugins/backend/vulkan/compiler` will lower
the neutral IR for their targets; those directories are created when their
implementation begins. A daemon/compiler-worker package enables the frontends
and backends it serves without enabling application-side provider DSOs.
