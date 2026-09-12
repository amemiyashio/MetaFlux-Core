# Canonical CPU Execution

Use this guide for stock-client coverage, interpreter/compiled launch work or
the module/cache boundary. Read the active
[CPU work item](../../../plan/milestone-0.2.0.0-pytorch-cuda-compatibility/work/work-item-0.2.0.2-torch-kernel-intake.md)
and affected row in the
[frontier corpus](../../../../tests/compatibility/pytorch_cuda_cpu_frontier_corpus_v1.json).
The finite frontier and its explicit gaps are current product evidence; they
are neither a universal PyTorch inventory nor a reason to rebuild a matrix.

## Locate the implementation

| Boundary | Source and first symbol |
| --- | --- |
| Stock request/module admission and completion correlation | [server.cpp](../../../../services/metafluxd/src/server.cpp), `Session::process_launch`, `MF_RING_OPCODE_MODULE_LOAD` |
| Interpreter/cold-JIT/warm-JIT/AOT preparation | [execution.cpp](../../../../services/metafluxd/src/execution.cpp), `CpuExecutionEngine::prepare`, `PreparedModule::launch` |
| Independent runtime KIR decoder and interpreter | [interpreter.cpp](../../../../plugins/backend/cpu/runtime/src/interpreter.cpp), `parse_kernel`, `execute_kernel_ir`, `execute_cta` |
| Object validation and generic compiled launch | [compiled_kernel.cpp](../../../../plugins/backend/cpu/runtime/src/compiled_kernel.cpp), `load_compiled_kernel`, `LoadedCompiledKernel::launch` |
| Persistent artifact lookup/acquisition | [pipeline.cpp](../../../../plugins/backend/cpu/compiler/src/pipeline.cpp), `lookup_cached_artifact`, `acquire_artifact`, `prewarm_aot` |
| CTA scheduling and placement refresh | [executor.cpp](../../../../plugins/backend/cpu/runtime/src/executor.cpp), `CpuExecutor::run_ctas`, `Impl::ensure_placement` |

Implement an admitted semantic family through these generic paths. Providers
normalize arguments and submit neutral work; they do not read tensor contents
or compute results. A daemon branch implementing one operation is not generic
interpreter/compiled coverage. For broader dtype, shape, layout or variable
arity, first establish the reusable representation with
[$ptx-simt-semantics](../../ptx-simt-semantics/SKILL.md) skill and the neutral contract
owner. Preserve the decision-0053 library boundary; do not silently broaden a
qualified shim or substitute library execution for an accepted generic row.

## Keep the execution modes truthful

Interpreter mode uses the canonical runtime decoder and records compiler/cache
non-use. Cold JIT compiles and loads an artifact; warm JIT resolves compatible
existing entries without a compiler request; AOT resolves the administrator
tier without runtime compilation. Bind each request/module to the executor
that actually completed it. One real client operation can emit multiple kernels;
match the complete observed source list and per-request evidence, not a count.

The helper ABI, parameter signature, target and FP policy connect compiler and
loader. Cache compatibility includes target triple, CPU/canonical features,
compiler epoch and pipeline, FP policy, helper/backend ABI, PGO identity and
kernel content; a changed input must miss rather than execute an old object.
For the current exponential helper, the DSO digest/function identity
is bound during preparation; generated artifacts have no undefined symbols or
dynamic dependencies. Preserve ELF class/machine/segment bounds, non-WX,
artifact digest, entry symbol and helper identity checks before exposing code.
The runtime target remains free of compiler-core, MLIR, LLVM and CUDA dependencies.

## Remove costs at the owning boundary

Trace the claimed warm call before selecting a lever. `execute_kernel_ir`
currently parses its canonical input on launch, while `LoadedCompiledKernel::launch`
constructs argument vectors before handing a callable to the executor. These
are concrete inspection points, not a claim that every launch mode is already
allocation-free. Separate prepared immutable kernel data from mutable arguments,
worker-local scratch and generation-bound state; retain cancellation and error
checks. Moving scratch to a shared module needs an explicit concurrency/lifetime
contract, not an unsynchronized mutable cache.

`CpuExecutor` already reuses scheduling storage and checks placement identity
before full rediscovery. Reuse that policy rather than disabling hotplug,
affinity/cpuset refresh or explicit-pin failure to improve a benchmark.
The [executor audit](../../../../tests/performance/cpu_executor_audit.cpp) exercises
an executor callable and compares allocation counts between windows; it does
not by itself prove zero allocation in compiled launch or stock-client execution.

## Qualify the changed boundary

Nearest owners are `metaflux.unit.cpu-interpreter`,
`metaflux.differential.cpu-ptx-corpus`,
`metaflux.integration.cpu-compiler-pipeline` and
`metaflux.differential.cpu-compiled-corpus`. Choose affected targets while editing;
formal performance acceptance retains differential correctness over the entire
advertised corpus. The real-client frontier runner additionally checks the
stock process, all required CPU modes, per-request execution and mode-specific
cache provenance. A standalone artifact test does not replace that claim.
Keep launch dimensions, argument types/counts, bounds, permissions, alignment,
overflow, FP environment, cancellation and shutdown negatives relevant to the
changed boundary. Report remaining work as an implementation limit, not a pass.
