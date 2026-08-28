option(METAFLUX_BUILD_RUNTIME_CORE "Build the ecosystem-neutral runtime core" ON)
option(METAFLUX_BUILD_CLIENT_FASTPATH "Build the application-side client fast path" ON)
option(METAFLUX_BUILD_CUDA_DRIVER_PROVIDER "Build the CUDA Driver ABI provider fixture" ON)
option(METAFLUX_BUILD_NVML_PROVIDER "Build the NVML provider fixture" ON)
option(METAFLUX_BUILD_CUDA_PTX_FRONTEND "Build the CUDA PTX compiler frontend fixture" ON)
option(METAFLUX_BUILD_DAEMON "Build the MetaFlux daemon fixture" ON)
option(METAFLUX_BUILD_COMPILER "Build the compiler core fixture" ON)
option(METAFLUX_BUILD_CPU_BACKEND_RUNTIME "Build the CPU backend runtime fixture" ON)
option(METAFLUX_BUILD_TESTS "Build MetaFlux tests when BUILD_TESTING is enabled" ON)

option(METAFLUX_ENABLE_WERROR "Treat project warnings as errors" OFF)
option(METAFLUX_ENABLE_SANITIZERS "Enable address and undefined behavior sanitizers" OFF)
option(METAFLUX_ENABLE_COVERAGE "Enable source coverage instrumentation" OFF)
option(METAFLUX_ENABLE_LTO "Enable interprocedural optimization for release builds" OFF)
option(METAFLUX_ENABLE_FRAME_POINTERS "Retain frame pointers for profiling" OFF)
option(METAFLUX_USE_LLD "Select LLD through the Clang compiler driver" ON)

if(METAFLUX_BUILD_DAEMON AND NOT METAFLUX_BUILD_RUNTIME_CORE)
  message(FATAL_ERROR "METAFLUX_BUILD_DAEMON requires METAFLUX_BUILD_RUNTIME_CORE")
endif()

if(METAFLUX_BUILD_DAEMON AND NOT METAFLUX_BUILD_COMPILER)
  message(FATAL_ERROR "METAFLUX_BUILD_DAEMON requires METAFLUX_BUILD_COMPILER")
endif()

if(METAFLUX_BUILD_CUDA_PTX_FRONTEND AND NOT METAFLUX_BUILD_COMPILER)
  message(FATAL_ERROR "METAFLUX_BUILD_CUDA_PTX_FRONTEND requires METAFLUX_BUILD_COMPILER")
endif()

if(
  (METAFLUX_BUILD_CUDA_DRIVER_PROVIDER OR METAFLUX_BUILD_NVML_PROVIDER)
  AND NOT METAFLUX_BUILD_CLIENT_FASTPATH
)
  message(FATAL_ERROR "CUDA/NVML providers require METAFLUX_BUILD_CLIENT_FASTPATH")
endif()

if(METAFLUX_ENABLE_SANITIZERS AND METAFLUX_ENABLE_COVERAGE)
  message(FATAL_ERROR "Sanitizer and coverage presets must use separate build trees")
endif()
