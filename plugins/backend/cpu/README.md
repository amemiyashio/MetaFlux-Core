# CPU Backend

The CPU backend is the reference execution backend for the first vertical slice.
It implements the versioned backend C ABI, Kernel IR interpretation, CTA
scheduling, and MLIR/LLVM lowering to PIC ELF.

Compiler-side and runtime-side code remain separate build targets even though
they share this ownership directory. The bootstrap currently contains only the
runtime-side C ABI fixture. The interpreter is the correctness oracle; JIT and
AOT reuse the same lowering pipeline and content-addressed cache. No CUDA or
other compatibility-layer type crosses the backend boundary.
