add_executable(
  metaflux_cuda_provider_semantics_test
  provider_test.c
  ../src/provider.c
  ../src/provider_stubs.c
  ../src/provider_gap_stubs.c
  ../src/dispatch.c
  "${cuda_managed_renames}"
)
target_compile_definitions(
  metaflux_cuda_provider_semantics_test
  PRIVATE METAFLUX_PROVIDER_TESTING=1
)
target_include_directories(
  metaflux_cuda_provider_semantics_test
  PRIVATE
    ../include
    "${CMAKE_CURRENT_BINARY_DIR}/.."
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../passthrough/src"
)
target_link_libraries(
  metaflux_cuda_provider_semantics_test
  PRIVATE MetaFlux::ClientFastpath MetaFlux::CudaPassthrough Threads::Threads
)
if(METAFLUX_BUILD_CDEV_TRANSPORT)
  target_link_libraries(metaflux_cuda_provider_semantics_test PRIVATE MetaFlux::TransportCdevClient)
  target_compile_definitions(metaflux_cuda_provider_semantics_test PRIVATE METAFLUX_PROVIDER_CDEV=1)
else()
  target_compile_definitions(metaflux_cuda_provider_semantics_test PRIVATE METAFLUX_PROVIDER_CDEV=0)
endif()
metaflux_configure_target(metaflux_cuda_provider_semantics_test C)

add_library(
  metaflux_cuda_provider_mode_fixture
  SHARED
    ../src/provider.c
    ../src/provider_stubs.c
    ../src/provider_gap_stubs.c
    ../src/dispatch.c
    "${cuda_managed_renames}"
)
target_compile_definitions(
  metaflux_cuda_provider_mode_fixture
  PRIVATE METAFLUX_PROVIDER_TESTING=1
)
target_include_directories(
  metaflux_cuda_provider_mode_fixture
  PRIVATE
    ../include
    "${CMAKE_CURRENT_BINARY_DIR}/.."
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../passthrough/src"
)
target_link_libraries(
  metaflux_cuda_provider_mode_fixture
  PRIVATE MetaFlux::ClientFastpath MetaFlux::CudaPassthrough Threads::Threads
)
if(METAFLUX_BUILD_CDEV_TRANSPORT)
  target_link_libraries(metaflux_cuda_provider_mode_fixture PRIVATE MetaFlux::TransportCdevClient)
  target_compile_definitions(metaflux_cuda_provider_mode_fixture PRIVATE METAFLUX_PROVIDER_CDEV=1)
else()
  target_compile_definitions(metaflux_cuda_provider_mode_fixture PRIVATE METAFLUX_PROVIDER_CDEV=0)
endif()
target_link_options(
  metaflux_cuda_provider_mode_fixture
  PRIVATE "LINKER:--build-id=sha1"
)
set_target_properties(
  metaflux_cuda_provider_mode_fixture
  PROPERTIES
    C_VISIBILITY_PRESET hidden
    OUTPUT_NAME metaflux-cuda-provider-mode-fixture
)
metaflux_configure_target(metaflux_cuda_provider_mode_fixture C)

add_test(
  NAME metaflux.unit.provider.cuda-semantics
  COMMAND metaflux_cuda_provider_semantics_test
)
set_tests_properties(
  metaflux.unit.provider.cuda-semantics
  PROPERTIES LABELS "abi;integration;provider" TIMEOUT 30
)

add_test(
  NAME metaflux.unit.provider.cuda-install-layout
  COMMAND
    "${CMAKE_COMMAND}"
    "-DINSTALL_SCRIPT=${CMAKE_CURRENT_BINARY_DIR}/../cmake_install.cmake"
    "-DINSTALL_LIBDIR=${CMAKE_INSTALL_LIBDIR}"
    "-DLIBRARY_STEM=libcuda"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/check_install_layout.cmake"
)
set_tests_properties(
  metaflux.unit.provider.cuda-install-layout
  PROPERTIES LABELS "abi;packaging;provider"
)

add_test(
  NAME metaflux.abi.provider.cuda-ptds-exports
  COMMAND
    "${CMAKE_COMMAND}"
    "-DREADELF=${CMAKE_READELF}"
    "-DPROVIDER=$<TARGET_FILE:metaflux_cuda_provider>"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/check_ptds_exports.cmake"
)
set_tests_properties(
  metaflux.abi.provider.cuda-ptds-exports
  PROPERTIES LABELS "abi;provider"
)

if(METAFLUX_NVIDIA_HEADER_DIR)
  add_executable(metaflux_cuda_ptds_header_smoke ptds_header_smoke.c)
  target_include_directories(
    metaflux_cuda_ptds_header_smoke
    SYSTEM PRIVATE "${METAFLUX_NVIDIA_HEADER_DIR}"
  )
  target_link_libraries(metaflux_cuda_ptds_header_smoke PRIVATE metaflux_cuda_provider)
  metaflux_configure_target(metaflux_cuda_ptds_header_smoke C)
  add_test(
    NAME metaflux.abi.provider.cuda-ptds-header
    COMMAND metaflux_cuda_ptds_header_smoke
  )
  set_tests_properties(
    metaflux.abi.provider.cuda-ptds-header
    PROPERTIES LABELS "abi;provider"
  )
endif()
