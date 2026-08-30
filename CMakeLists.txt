cmake_minimum_required(VERSION 3.25)

set(METAFLUX_VERSION_FILE "${CMAKE_CURRENT_LIST_DIR}/VERSION")
if(NOT EXISTS "${METAFLUX_VERSION_FILE}")
  message(FATAL_ERROR "MetaFlux version file is missing: ${METAFLUX_VERSION_FILE}")
endif()
file(READ "${METAFLUX_VERSION_FILE}" METAFLUX_PROJECT_VERSION)
string(STRIP "${METAFLUX_PROJECT_VERSION}" METAFLUX_PROJECT_VERSION)
if(
  NOT METAFLUX_PROJECT_VERSION
      MATCHES "^(0|[1-9][0-9]*)\\.(0|[1-9][0-9]*)\\.(0|[1-9][0-9]*)$"
)
  message(
    FATAL_ERROR
      "VERSION must contain exactly three numeric components without leading zeros; got '${METAFLUX_PROJECT_VERSION}'"
  )
endif()

project(
  MetaFluxCore
  VERSION "${METAFLUX_PROJECT_VERSION}"
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
  OR METAFLUX_BUILD_CPU_BACKEND_COMPILER
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
include(MetaFluxComponentGraph)

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
  OR METAFLUX_BUILD_CPU_BACKEND_COMPILER
  OR METAFLUX_BUILD_CPU_BACKEND_RUNTIME
)
  add_subdirectory(plugins)
endif()

if(METAFLUX_BUILD_DAEMON)
  add_subdirectory(services)
endif()

metaflux_write_component_graph("${CMAKE_BINARY_DIR}/metaflux-component-graph.json")

if(BUILD_TESTING AND METAFLUX_BUILD_TESTS)
  add_subdirectory(tests)
endif()

include(MetaFluxFormatting)
