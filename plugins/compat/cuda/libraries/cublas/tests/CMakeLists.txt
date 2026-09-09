add_library(metaflux_cublas_fake_cuda SHARED fake_cuda.c)
set_target_properties(
  metaflux_cublas_fake_cuda
  PROPERTIES OUTPUT_NAME cuda SOVERSION 1 VERSION 1.0.0
)
target_link_libraries(metaflux_cublas_fake_cuda PRIVATE MetaFlux::ClientProtocol)
metaflux_configure_target(metaflux_cublas_fake_cuda C)

add_executable(metaflux_cublas_provider_test provider_test.c)
target_link_libraries(metaflux_cublas_provider_test PRIVATE MetaFlux::CublasProvider)
metaflux_configure_target(metaflux_cublas_provider_test C)

add_test(
  NAME metaflux.unit.provider.cublas-semantics
  COMMAND
    "${CMAKE_COMMAND}" -E env
    "LD_LIBRARY_PATH=$<TARGET_FILE_DIR:metaflux_cublas_fake_cuda>"
    $<TARGET_FILE:metaflux_cublas_provider_test>
)
set_tests_properties(
  metaflux.unit.provider.cublas-semantics
  PROPERTIES LABELS "abi;cublas;provider;unit"
)

add_test(
  NAME metaflux.unit.provider.cublas-install-layout
  COMMAND
    "${CMAKE_COMMAND}"
    "-DINSTALL_SCRIPT=${CMAKE_CURRENT_BINARY_DIR}/../cmake_install.cmake"
    "-DINSTALL_LIBDIR=${CMAKE_INSTALL_LIBDIR}"
    "-DLIBRARY_STEM=libmetaflux-cublas-provider"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/check_install_layout.cmake"
)
set_tests_properties(
  metaflux.unit.provider.cublas-install-layout
  PROPERTIES LABELS "abi;cublas;packaging;provider"
)
