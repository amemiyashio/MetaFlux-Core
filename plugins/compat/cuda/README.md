# CUDA Compatibility Layer

This compatibility-layer plugin lets unmodified applications use the CUDA
Driver API and NVML while MetaFlux owns managed execution. It is not an NVIDIA
hardware emulator and does not implement the private RM/UVM ABI.

Implementation is organized by responsibility when each area gains real source:

- `abi/driver/`: CUDA Driver API provider and exported ABI.
- `management/nvml/`: NVML provider used by stock `nvidia-smi`.
- `compiler/ptx/`: supported PTX input parsing and verification.
- `semantics/`: CUDA-visible behavior translated to core contracts.
- `passthrough/`: recursion-safe dispatch to vendor libraries.
- `generated/`: manifest-owned export tables and ABI fixtures.
- `tests/`: component-local CUDA/NVML tests.

The provider and client fast path stay C17 and avoid LLVM/MLIR and the C++
runtime in the application process. Compiler-side code may use C++20 but remains
an independently buildable target and Nix derivation.
