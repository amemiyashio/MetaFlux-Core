cmake_minimum_required(VERSION 3.25)

project(
  MetaFluxCore
  VERSION 0.1.0
  DESCRIPTION "MetaFlux compatibility and execution core"
  LANGUAGES C
)

include(CTest)
include(GNUInstallDirs)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(MetaFluxOptions)
include(MetaFluxSourcePolicy)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

if(
  METAFLUX_BUILD_RUNTIME_CORE
  OR METAFLUX_BUILD_DAEMON
  OR METAFLUX_BUILD_COMPILER
  OR METAFLUX_BUILD_CPU_BACKEND_RUNTIME
  OR METAFLUX_BUILD_CUDA_PTX_FRONTEND
)
  enable_language(CXX)
  set(CMAKE_CXX_STANDARD 20)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
  set(CMAKE_CXX_EXTENSIONS OFF)
endif()

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(MetaFluxTargets)
include(MetaFluxBuildManifest)

add_subdirectory(contracts)

if(
  METAFLUX_BUILD_RUNTIME_CORE
  OR METAFLUX_BUILD_CLIENT_FASTPATH
  OR METAFLUX_BUILD_CUDA_DRIVER_PROVIDER
  OR METAFLUX_BUILD_NVML_PROVIDER
  OR METAFLUX_BUILD_DAEMON
)
  add_subdirectory(runtime)
endif()

if(METAFLUX_BUILD_COMPILER)
  add_subdirectory(compiler)
endif()

if(
  METAFLUX_BUILD_CUDA_DRIVER_PROVIDER
  OR METAFLUX_BUILD_NVML_PROVIDER
  OR METAFLUX_BUILD_CUDA_PTX_FRONTEND
  OR METAFLUX_BUILD_CPU_BACKEND_RUNTIME
)
  add_subdirectory(plugins)
endif()

if(METAFLUX_BUILD_DAEMON)
  add_subdirectory(services)
endif()

if(BUILD_TESTING AND METAFLUX_BUILD_TESTS)
  add_subdirectory(tests)
endif()

include(MetaFluxFormatting)
