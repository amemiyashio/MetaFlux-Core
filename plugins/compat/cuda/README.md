# CUDA Compatibility Layer

This compatibility-layer plugin lets unmodified applications use the CUDA
Driver API and NVML while MetaFlux owns managed execution. It is not an NVIDIA
hardware emulator and does not implement the private RM/UVM ABI.

Implementation is organized by responsibility:

- `abi/driver/`: CUDA Driver API provider and exported ABI.
- `management/nvml/`: NVML provider used by stock `nvidia-smi`.
- `compiler/ptx/`: supported PTX input parsing and verification.
- `libraries/cublas/`: bounded cuBLAS/cuBLASLt ABI translation into daemon-owned
  execution through the Driver provider.
- `passthrough/`: recursion-safe dispatch to vendor libraries.

CUDA-visible behavior, ABI manifests, and component tests live with their owning
provider or frontend. Configure-time export maps and embedded profile headers
belong in the build tree; cross-component qualification lives under the
repository's `tests/`.

The provider and client fast path stay C17 and avoid LLVM/MLIR and the C++
runtime in the application process. Compiler-side code may use C++20 but remains
an independently buildable target and Nix derivation.
