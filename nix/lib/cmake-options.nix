{ lib }:
{
  runtimeCore ? false,
  clientFastpath ? false,
  cudaDriverProvider ? false,
  nvmlProvider ? false,
  cudaPtxFrontend ? false,
  daemon ? false,
  compiler ? false,
  cpuBackendRuntime ? false,
  tests ? false,
  werror ? true,
  sanitizers ? false,
  coverage ? false,
  lto ? false,
  framePointers ? false,
  useLld ? true,
}:
let
  cmakeBool = enabled: if enabled then "ON" else "OFF";
in
[
  "-DMETAFLUX_BUILD_RUNTIME_CORE=${cmakeBool runtimeCore}"
  "-DMETAFLUX_BUILD_CLIENT_FASTPATH=${cmakeBool clientFastpath}"
  "-DMETAFLUX_BUILD_CUDA_DRIVER_PROVIDER=${cmakeBool cudaDriverProvider}"
  "-DMETAFLUX_BUILD_NVML_PROVIDER=${cmakeBool nvmlProvider}"
  "-DMETAFLUX_BUILD_CUDA_PTX_FRONTEND=${cmakeBool cudaPtxFrontend}"
  "-DMETAFLUX_BUILD_DAEMON=${cmakeBool daemon}"
  "-DMETAFLUX_BUILD_COMPILER=${cmakeBool compiler}"
  "-DMETAFLUX_BUILD_CPU_BACKEND_RUNTIME=${cmakeBool cpuBackendRuntime}"
  "-DMETAFLUX_BUILD_TESTS=${cmakeBool tests}"
  "-DMETAFLUX_ENABLE_WERROR=${cmakeBool werror}"
  "-DMETAFLUX_ENABLE_SANITIZERS=${cmakeBool sanitizers}"
  "-DMETAFLUX_ENABLE_COVERAGE=${cmakeBool coverage}"
  "-DMETAFLUX_ENABLE_LTO=${cmakeBool lto}"
  "-DMETAFLUX_ENABLE_FRAME_POINTERS=${cmakeBool framePointers}"
  "-DMETAFLUX_USE_LLD=${cmakeBool useLld}"
]
