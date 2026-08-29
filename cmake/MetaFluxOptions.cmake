option(METAFLUX_BUILD_RUNTIME_CORE "Build the ecosystem-neutral runtime core" ON)
option(METAFLUX_BUILD_CLIENT_FASTPATH "Build the application-side client fast path" ON)
option(METAFLUX_BUILD_CUDA_DRIVER_PROVIDER "Build the CUDA Driver ABI provider fixture" ON)
option(METAFLUX_BUILD_NVML_PROVIDER "Build the NVML provider fixture" ON)
option(METAFLUX_BUILD_CUDA_PTX_FRONTEND "Build the CUDA PTX compiler frontend fixture" ON)
option(METAFLUX_BUILD_DAEMON "Build the MetaFlux daemon fixture" ON)
option(METAFLUX_BUILD_COMPILER "Build the compiler core fixture" ON)
option(METAFLUX_BUILD_CPU_BACKEND_COMPILER "Build the CPU backend compiler" ON)
option(METAFLUX_BUILD_CPU_BACKEND_RUNTIME "Build the CPU backend runtime fixture" ON)
option(METAFLUX_BUILD_TESTS "Build MetaFlux tests when BUILD_TESTING is enabled" ON)

option(METAFLUX_ENABLE_WERROR "Treat project warnings as errors" OFF)
option(METAFLUX_ENABLE_SANITIZERS "Enable address and undefined behavior sanitizers" OFF)
option(METAFLUX_ENABLE_COVERAGE "Enable source coverage instrumentation" OFF)
option(METAFLUX_ENABLE_LTO "Enable interprocedural optimization for release builds" OFF)
option(METAFLUX_ENABLE_FRAME_POINTERS "Retain frame pointers for profiling" OFF)
option(METAFLUX_USE_LLD "Select LLD through the Clang compiler driver" ON)
option(
  METAFLUX_COMPILER_LINK_SHARED_LLVM
  "Link compiler workers to the private shared MLIR/LLVM libraries"
  OFF
)

set(
  METAFLUX_PGO_MODE
  "OFF"
  CACHE STRING
  "Project PGO mode: OFF, GENERATE, or USE"
)
set_property(CACHE METAFLUX_PGO_MODE PROPERTY STRINGS OFF GENERATE USE)
string(TOUPPER "${METAFLUX_PGO_MODE}" METAFLUX_PGO_MODE)
set(
  METAFLUX_PGO_RAW_PATTERN
  "${CMAKE_BINARY_DIR}/pgo/%m-%p.profraw"
  CACHE STRING
  "Clang instrumentation output pattern used in GENERATE mode"
)
set(
  METAFLUX_PGO_PROFILE
  ""
  CACHE STRING
  "Merged llvm-profdata input used in USE mode"
)

if(NOT METAFLUX_PGO_MODE MATCHES "^(OFF|GENERATE|USE)$")
  message(FATAL_ERROR "METAFLUX_PGO_MODE must be OFF, GENERATE, or USE")
endif()
if(
  NOT METAFLUX_PGO_MODE STREQUAL "OFF"
  AND (METAFLUX_ENABLE_SANITIZERS OR METAFLUX_ENABLE_COVERAGE)
)
  message(FATAL_ERROR "PGO qualification uses a dedicated non-sanitizer, non-coverage build")
endif()
if(METAFLUX_PGO_MODE STREQUAL "GENERATE")
  if(NOT METAFLUX_PGO_RAW_PATTERN MATCHES "%m")
    message(FATAL_ERROR "METAFLUX_PGO_RAW_PATTERN must contain %m to isolate binary signatures")
  endif()
  if(NOT IS_ABSOLUTE "${METAFLUX_PGO_RAW_PATTERN}")
    message(FATAL_ERROR "METAFLUX_PGO_RAW_PATTERN must be absolute")
  endif()
  if(NOT METAFLUX_PGO_PROFILE STREQUAL "")
    message(FATAL_ERROR "METAFLUX_PGO_PROFILE is valid only in USE mode")
  endif()
  get_filename_component(metaflux_pgo_raw_directory "${METAFLUX_PGO_RAW_PATTERN}" DIRECTORY)
  file(MAKE_DIRECTORY "${metaflux_pgo_raw_directory}")
elseif(METAFLUX_PGO_MODE STREQUAL "USE")
  if(METAFLUX_PGO_PROFILE STREQUAL "" OR NOT EXISTS "${METAFLUX_PGO_PROFILE}")
    message(FATAL_ERROR "METAFLUX_PGO_MODE=USE requires an existing METAFLUX_PGO_PROFILE")
  endif()
  if(NOT IS_ABSOLUTE "${METAFLUX_PGO_PROFILE}")
    message(FATAL_ERROR "METAFLUX_PGO_PROFILE must be absolute")
  endif()
elseif(NOT METAFLUX_PGO_PROFILE STREQUAL "")
  message(FATAL_ERROR "METAFLUX_PGO_PROFILE is valid only in USE mode")
endif()

set(
  METAFLUX_NVIDIA_HEADER_DIR
  ""
  CACHE PATH
  "Directory containing the frozen official cuda.h and nvml.h used by compatibility tests"
)
if(METAFLUX_BUILD_DAEMON AND NOT METAFLUX_BUILD_RUNTIME_CORE)
  message(FATAL_ERROR "METAFLUX_BUILD_DAEMON requires METAFLUX_BUILD_RUNTIME_CORE")
endif()

if(METAFLUX_BUILD_DAEMON AND NOT METAFLUX_BUILD_COMPILER)
  message(FATAL_ERROR "METAFLUX_BUILD_DAEMON requires METAFLUX_BUILD_COMPILER")
endif()

if(METAFLUX_BUILD_CUDA_PTX_FRONTEND AND NOT METAFLUX_BUILD_COMPILER)
  message(FATAL_ERROR "METAFLUX_BUILD_CUDA_PTX_FRONTEND requires METAFLUX_BUILD_COMPILER")
endif()

if(METAFLUX_BUILD_CPU_BACKEND_COMPILER AND NOT METAFLUX_BUILD_COMPILER)
  message(FATAL_ERROR "METAFLUX_BUILD_CPU_BACKEND_COMPILER requires METAFLUX_BUILD_COMPILER")
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
