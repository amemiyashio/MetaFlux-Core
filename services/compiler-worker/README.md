# Compiler Worker

Planned isolated LLVM/MLIR compilation service. It consumes versioned compiler
requests, populates the shared content-addressed cache, and keeps compiler
frameworks outside provider processes and runtime fast paths.
