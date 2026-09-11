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

set(cuda_pytorch_cpu_kernels "${CMAKE_CURRENT_BINARY_DIR}/pytorch-cuda-cpu-kernels.h")
file(
  WRITE "${cuda_pytorch_cpu_kernels}"
  "#ifndef METAFLUX_PYTORCH_CUDA_CPU_KERNELS_H\n#define METAFLUX_PYTORCH_CUDA_CPU_KERNELS_H\n\n"
)
function(metaflux_embed_pytorch_cpu_kernel symbol filename)
  set(source "${CMAKE_CURRENT_SOURCE_DIR}/profiles/pytorch-cuda-cpu-v1/${filename}")
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${source}")
  file(READ "${source}" ptx)
  string(REPLACE "\\" "\\\\" ptx "${ptx}")
  string(REPLACE "\"" "\\\"" ptx "${ptx}")
  string(REPLACE "\n" "\\n\"\n    \"" ptx "${ptx}")
  file(APPEND "${cuda_pytorch_cpu_kernels}" "static const char ${symbol}[] =\n    \"${ptx}\";\n\n")
endfunction()

metaflux_embed_pytorch_cpu_kernel(mf_pytorch_baseline_add_ptx add-i32.ptx)
metaflux_embed_pytorch_cpu_kernel(mf_pytorch_baseline_sub_ptx sub-i32.ptx)
metaflux_embed_pytorch_cpu_kernel(mf_pytorch_baseline_mul_ptx mul-i32.ptx)
metaflux_embed_pytorch_cpu_kernel(mf_pytorch_baseline_addf32_ptx add-f32.ptx)
metaflux_embed_pytorch_cpu_kernel(mf_pytorch_baseline_subf32_ptx sub-f32.ptx)
metaflux_embed_pytorch_cpu_kernel(mf_pytorch_baseline_mulf32_ptx mul-f32.ptx)
metaflux_embed_pytorch_cpu_kernel(mf_pytorch_baseline_divf32_ptx div-f32.ptx)
metaflux_embed_pytorch_cpu_kernel(mf_pytorch_baseline_neg_ptx neg-i32.ptx)
metaflux_embed_pytorch_cpu_kernel(mf_pytorch_baseline_fill_i32_ptx fill-i32.ptx)
metaflux_embed_pytorch_cpu_kernel(mf_pytorch_baseline_scadd_i32_ptx scalar-add-i32.ptx)
metaflux_embed_pytorch_cpu_kernel(mf_pytorch_baseline_scmul_i32_ptx scalar-mul-i32.ptx)
metaflux_embed_pytorch_cpu_kernel(mf_pytorch_baseline_alphaadd_i32_ptx alpha-add-i32.ptx)
metaflux_embed_pytorch_cpu_kernel(mf_pytorch_baseline_absi32_ptx abs-i32.ptx)
metaflux_embed_pytorch_cpu_kernel(mf_pytorch_baseline_absf32_ptx abs-f32.ptx)
metaflux_embed_pytorch_cpu_kernel(mf_pytorch_baseline_sqrtf32_ptx sqrt-f32.ptx)
metaflux_embed_pytorch_cpu_kernel(mf_pytorch_baseline_expf32_ptx exp-f32.ptx)
metaflux_embed_pytorch_cpu_kernel(mf_pytorch_baseline_eq_i32_ptx eq-i32.ptx)
metaflux_embed_pytorch_cpu_kernel(mf_pytorch_baseline_gt_i32_ptx gt-i32.ptx)
metaflux_embed_pytorch_cpu_kernel(mf_pytorch_baseline_lt_f32_ptx lt-f32.ptx)
metaflux_embed_pytorch_cpu_kernel(mf_pytorch_baseline_reluf32_ptx relu-f32.ptx)
metaflux_embed_pytorch_cpu_kernel(mf_pytorch_baseline_clamp_min_f32_ptx clamp-min-f32.ptx)
metaflux_embed_pytorch_cpu_kernel(mf_pytorch_baseline_cast_i32_f32_ptx cast-i32-f32.ptx)
metaflux_embed_pytorch_cpu_kernel(mf_pytorch_baseline_reduce_stub_ptx native-stub.ptx)
file(APPEND "${cuda_pytorch_cpu_kernels}" "#endif\n")

add_library(
  metaflux_cuda_provider
  SHARED
    src/provider.c
    src/provider_stubs.c
    src/provider_gap_stubs.c
    src/dispatch.c
    "${cuda_symbol_manifest}"
    "${cuda_managed_renames}"
    "${cuda_pytorch_cpu_kernels}"
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
if(METAFLUX_BUILD_CDEV_TRANSPORT)
  target_link_libraries(metaflux_cuda_provider PRIVATE MetaFlux::TransportCdevClient)
  target_compile_definitions(metaflux_cuda_provider PRIVATE METAFLUX_PROVIDER_CDEV=1)
else()
  target_compile_definitions(metaflux_cuda_provider PRIVATE METAFLUX_PROVIDER_CDEV=0)
endif()
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
