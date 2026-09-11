include(MetaFluxGeneratedFile)
set(nvml_symbol_manifest "${CMAKE_CURRENT_SOURCE_DIR}/symbols.def")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${nvml_symbol_manifest}")
set(
  METAFLUX_NVML_TARGET_DRIVER_VERSION
  "610.43.02"
  CACHE STRING
  "Driver version reported by the selected stock NVML compatibility target"
)
set(
  METAFLUX_NVML_TARGET_INTERFACE_VERSION
  "13.610.43.02"
  CACHE STRING
  "Full NVML library version reported by the selected compatibility target"
)
set(
  METAFLUX_NVML_TARGET_CUDA_DRIVER_VERSION
  "13030"
  CACHE STRING
  "CUDA Driver API integer reported by the selected NVML compatibility target"
)
if(
  METAFLUX_NVML_TARGET_DRIVER_VERSION STREQUAL ""
  OR METAFLUX_NVML_TARGET_INTERFACE_VERSION STREQUAL ""
  OR NOT METAFLUX_NVML_TARGET_CUDA_DRIVER_VERSION MATCHES "^[0-9]+$"
)
  message(FATAL_ERROR "NVML target version strings and CUDA Driver API integer must be pinned")
endif()
file(STRINGS "${nvml_symbol_manifest}" nvml_symbol_rows)
set(nvml_internal_exports "")
set(nvml_abi_exports "")
set(
  nvml_managed_renames
  "${CMAKE_CURRENT_BINARY_DIR}/managed-renames.h"
)
set(
  nvml_managed_renames_content
  "#ifndef METAFLUX_NVML_MANAGED_RENAMES_H\n#define METAFLUX_NVML_MANAGED_RENAMES_H\n"
)
foreach(symbol_row IN LISTS nvml_symbol_rows)
  if(symbol_row MATCHES "^MF_NVML_INTERNAL\\(([A-Za-z0-9_]+)\\)")
    string(APPEND nvml_internal_exports "    ${CMAKE_MATCH_1};\n")
  elseif(symbol_row MATCHES "^MF_NVML_SYMBOL\\(([A-Za-z0-9_]+),")
    string(APPEND nvml_abi_exports "    ${CMAKE_MATCH_1};\n")
    string(
      APPEND nvml_managed_renames_content
      "#define ${CMAKE_MATCH_1} mf_nvml_managed_${CMAKE_MATCH_1}\n"
    )
  endif()
endforeach()
string(APPEND nvml_managed_renames_content "#endif\n")
metaflux_write_if_different("${nvml_managed_renames}" "${nvml_managed_renames_content}")

set(nvml_exports_map "${CMAKE_CURRENT_BINARY_DIR}/exports.map")
string(
  CONCAT nvml_exports_content
  "METAFLUX_NVML_PROVIDER_1 {\n  global:\n${nvml_internal_exports}};\n"
  "libnvidia-ml.so.1 {\n  global:\n${nvml_abi_exports}  local:\n    *;\n} METAFLUX_NVML_PROVIDER_1;\n"
)
metaflux_write_if_different("${nvml_exports_map}" "${nvml_exports_content}")

find_package(Threads REQUIRED)

add_library(
  metaflux_nvml_provider
  SHARED
    src/provider.c
    src/dispatch.c
    "${nvml_symbol_manifest}"
    "${nvml_managed_renames}"
)
add_library(MetaFlux::NvmlProvider ALIAS metaflux_nvml_provider)

target_compile_definitions(
  metaflux_nvml_provider
  PRIVATE
    "MF_NVML_TARGET_DRIVER_VERSION=\"${METAFLUX_NVML_TARGET_DRIVER_VERSION}\""
    "MF_NVML_TARGET_INTERFACE_VERSION=\"${METAFLUX_NVML_TARGET_INTERFACE_VERSION}\""
    "MF_NVML_TARGET_CUDA_DRIVER_VERSION=${METAFLUX_NVML_TARGET_CUDA_DRIVER_VERSION}"
)

set_target_properties(
  metaflux_nvml_provider
  PROPERTIES
    C_VISIBILITY_PRESET hidden
    OUTPUT_NAME nvidia-ml
    SOVERSION 1
    VERSION 1.0.0
    VISIBILITY_INLINES_HIDDEN YES
)

target_include_directories(
  metaflux_nvml_provider
  PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
  PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
)
target_link_libraries(
  metaflux_nvml_provider
  PRIVATE MetaFlux::ClientFastpath MetaFlux::CudaPassthrough Threads::Threads
)
target_link_options(
  metaflux_nvml_provider
  PRIVATE
    "LINKER:--build-id=sha1"
    "LINKER:--version-script=${nvml_exports_map}"
    "LINKER:--no-undefined-version"
)
set_property(
  TARGET metaflux_nvml_provider
  APPEND
  PROPERTY LINK_DEPENDS "${nvml_exports_map};${nvml_symbol_manifest}"
)
metaflux_configure_target(metaflux_nvml_provider C)

if(BUILD_TESTING AND METAFLUX_BUILD_TESTS)
  add_subdirectory(tests)
endif()

install(
  TARGETS metaflux_nvml_provider
  LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}/metaflux/providers"
  COMPONENT Provider
)
