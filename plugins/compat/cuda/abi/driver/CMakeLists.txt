set(cuda_symbol_manifest "${CMAKE_CURRENT_SOURCE_DIR}/symbols.def")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${cuda_symbol_manifest}")
file(STRINGS "${cuda_symbol_manifest}" cuda_symbol_rows)
set(cuda_internal_exports "")
set(cuda_abi_exports "")
set(
  cuda_managed_renames
  "${CMAKE_CURRENT_BINARY_DIR}/managed-renames.h"
)
file(
  WRITE "${cuda_managed_renames}"
  "#ifndef METAFLUX_CUDA_MANAGED_RENAMES_H\n#define METAFLUX_CUDA_MANAGED_RENAMES_H\n"
)
foreach(symbol_row IN LISTS cuda_symbol_rows)
  if(symbol_row MATCHES "^MF_CUDA_INTERNAL\\(([A-Za-z0-9_]+)\\)")
    string(APPEND cuda_internal_exports "    ${CMAKE_MATCH_1};\n")
  elseif(symbol_row MATCHES "^MF_CUDA_SYMBOL\\(([A-Za-z0-9_]+),")
    string(APPEND cuda_abi_exports "    ${CMAKE_MATCH_1};\n")
    file(
      APPEND "${cuda_managed_renames}"
      "#define ${CMAKE_MATCH_1} mf_cuda_managed_${CMAKE_MATCH_1}\n"
    )
  endif()
endforeach()
file(APPEND "${cuda_managed_renames}" "#endif\n")

set(cuda_exports_map "${CMAKE_CURRENT_BINARY_DIR}/exports.map")
file(
  WRITE "${cuda_exports_map}"
  "METAFLUX_CUDA_PROVIDER_1 {\n  global:\n${cuda_internal_exports}};\n"
  "libcuda.so.1 {\n  global:\n${cuda_abi_exports}  local:\n    *;\n} METAFLUX_CUDA_PROVIDER_1;\n"
)

find_package(Threads REQUIRED)

add_library(
  metaflux_cuda_provider
  SHARED
    src/provider.c
    src/dispatch.c
    "${cuda_symbol_manifest}"
    "${cuda_managed_renames}"
)
add_library(MetaFlux::CudaProvider ALIAS metaflux_cuda_provider)

set_target_properties(
  metaflux_cuda_provider
  PROPERTIES
    C_VISIBILITY_PRESET hidden
    OUTPUT_NAME cuda
    SOVERSION 1
    VERSION 1.0.0
    VISIBILITY_INLINES_HIDDEN YES
)

target_include_directories(
  metaflux_cuda_provider
  PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
  PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
)
target_link_libraries(
  metaflux_cuda_provider
  PRIVATE MetaFlux::ClientFastpath MetaFlux::CudaPassthrough Threads::Threads
)
target_link_options(
  metaflux_cuda_provider
  PRIVATE
    "LINKER:--build-id=sha1"
    "LINKER:--version-script=${cuda_exports_map}"
    "LINKER:--no-undefined-version"
)
set_property(
  TARGET metaflux_cuda_provider
  APPEND
  PROPERTY LINK_DEPENDS "${cuda_exports_map};${cuda_symbol_manifest}"
)
metaflux_configure_target(metaflux_cuda_provider C)

if(BUILD_TESTING AND METAFLUX_BUILD_TESTS)
  add_subdirectory(tests)
endif()

install(
  TARGETS metaflux_cuda_provider
  LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}/metaflux/providers"
  COMPONENT Provider
)
