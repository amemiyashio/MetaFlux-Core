include(MetaFluxGeneratedFile)
set(cublas_exports_map "${CMAKE_CURRENT_BINARY_DIR}/exports.map")
string(
  CONCAT cublas_exports_content
  "METAFLUX_CUBLAS_PROVIDER_1 {\n  global:\n    mf_cublas_provider_bootstrap_abi_version;\n};\n"
  "libcublas.so.12 {\n  global:\n"
  "    cublasCreate_v2;\n    cublasDestroy_v2;\n"
  "    cublasGetVersion_v2;\n    cublasGetProperty;\n    cublasGetCudartVersion;\n"
  "    cublasSetWorkspace_v2;\n    cublasSetStream_v2;\n    cublasGetStream_v2;\n"
  "    cublasGetPointerMode_v2;\n    cublasSetPointerMode_v2;\n"
  "    cublasGetMathMode;\n    cublasSetMathMode;\n"
  "    cublasGetStatusName;\n    cublasGetStatusString;\n"
  "    cublasSgemm_v2;\n  local:\n    *;\n} METAFLUX_CUBLAS_PROVIDER_1;\n"
  "libcublasLt.so.12 {\n  global:\n"
  "    cublasLtMatmulDescCreate;\n    cublasLtMatmulDescDestroy;\n"
  "    cublasLtMatmulDescSetAttribute;\n"
  "    cublasLtMatrixLayoutCreate;\n    cublasLtMatrixLayoutDestroy;\n"
  "    cublasLtMatrixLayoutSetAttribute;\n"
  "    cublasLtMatmulPreferenceCreate;\n    cublasLtMatmulPreferenceDestroy;\n"
  "    cublasLtMatmulPreferenceSetAttribute;\n"
  "    cublasLtMatmulAlgoGetHeuristic;\n    cublasLtMatmul;\n"
  "} METAFLUX_CUBLAS_PROVIDER_1;\n"
)
metaflux_write_if_different("${cublas_exports_map}" "${cublas_exports_content}")

find_package(Threads REQUIRED)

add_library(metaflux_cublas_provider SHARED src/provider.c)
add_library(MetaFlux::CublasProvider ALIAS metaflux_cublas_provider)

set_target_properties(
  metaflux_cublas_provider
  PROPERTIES
    C_VISIBILITY_PRESET hidden
    OUTPUT_NAME metaflux-cublas-provider
    SOVERSION 1
    VERSION 1.0.0
    VISIBILITY_INLINES_HIDDEN YES
)

target_include_directories(
  metaflux_cublas_provider
  PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)
target_link_libraries(
  metaflux_cublas_provider
  PRIVATE MetaFlux::ClientProtocol Threads::Threads "${CMAKE_DL_LIBS}"
)
target_link_options(
  metaflux_cublas_provider
  PRIVATE
    "LINKER:--build-id=sha1"
    "LINKER:--version-script=${cublas_exports_map}"
    "LINKER:--no-undefined-version"
)
set_property(TARGET metaflux_cublas_provider APPEND PROPERTY LINK_DEPENDS "${cublas_exports_map}")
metaflux_configure_target(metaflux_cublas_provider C)

if(BUILD_TESTING AND METAFLUX_BUILD_TESTS)
  add_subdirectory(tests)
endif()

install(
  TARGETS metaflux_cublas_provider
  LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}/metaflux/providers"
  COMPONENT Provider
)
