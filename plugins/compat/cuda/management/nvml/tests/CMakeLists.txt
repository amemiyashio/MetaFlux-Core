find_package(Threads REQUIRED)

add_executable(
  metaflux_nvml_provider_semantics_test
  provider_test.c
  ../src/provider.c
  ../src/dispatch.c
  "${nvml_managed_renames}"
)
target_compile_definitions(
  metaflux_nvml_provider_semantics_test
  PRIVATE METAFLUX_PROVIDER_TESTING=1
)
target_include_directories(
  metaflux_nvml_provider_semantics_test
  PRIVATE
    ../include
    "${CMAKE_CURRENT_BINARY_DIR}/.."
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../passthrough/src"
)
target_link_libraries(
  metaflux_nvml_provider_semantics_test
  PRIVATE MetaFlux::ClientFastpath MetaFlux::CudaPassthrough Threads::Threads
)
metaflux_configure_target(metaflux_nvml_provider_semantics_test C)

add_library(
  metaflux_nvml_provider_mode_fixture
  SHARED
    ../src/provider.c
    ../src/dispatch.c
    "${nvml_managed_renames}"
)
target_compile_definitions(
  metaflux_nvml_provider_mode_fixture
  PRIVATE METAFLUX_PROVIDER_TESTING=1
)
target_include_directories(
  metaflux_nvml_provider_mode_fixture
  PRIVATE
    ../include
    "${CMAKE_CURRENT_BINARY_DIR}/.."
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../passthrough/src"
)
target_link_libraries(
  metaflux_nvml_provider_mode_fixture
  PRIVATE MetaFlux::ClientFastpath MetaFlux::CudaPassthrough Threads::Threads
)
target_link_options(
  metaflux_nvml_provider_mode_fixture
  PRIVATE "LINKER:--build-id=sha1"
)
set_target_properties(
  metaflux_nvml_provider_mode_fixture
  PROPERTIES
    C_VISIBILITY_PRESET hidden
    OUTPUT_NAME metaflux-nvml-provider-mode-fixture
)
metaflux_configure_target(metaflux_nvml_provider_mode_fixture C)

add_executable(metaflux_cuda_nvml_provider_mode_test provider_mode_test.c)
add_dependencies(
  metaflux_cuda_nvml_provider_mode_test
  metaflux_cuda_provider_mode_fixture
  metaflux_nvml_provider_mode_fixture
  metaflux_passthrough_cuda_good
  metaflux_passthrough_nvml_good
)
target_compile_definitions(
  metaflux_cuda_nvml_provider_mode_test
  PRIVATE METAFLUX_PROVIDER_TESTING=1
)
target_include_directories(
  metaflux_cuda_nvml_provider_mode_test
  PRIVATE
    ../../../abi/driver/include
    ../include
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../passthrough/src"
)
target_link_libraries(
  metaflux_cuda_nvml_provider_mode_test
  PRIVATE MetaFlux::ClientFastpath MetaFlux::CudaPassthrough ${CMAKE_DL_LIBS}
)
metaflux_configure_target(metaflux_cuda_nvml_provider_mode_test C)

add_test(
  NAME metaflux.unit.provider.nvml-semantics
  COMMAND metaflux_nvml_provider_semantics_test
)
set_tests_properties(
  metaflux.unit.provider.nvml-semantics
  PROPERTIES LABELS "abi;unit;provider"
)

if(METAFLUX_BUILD_DAEMON)
  add_executable(metaflux_nvml_provider_policy_test policy_setter_test.c)
  target_link_libraries(
    metaflux_nvml_provider_policy_test
    PRIVATE metaflux_nvml_provider MetaFlux::ClientFastpath
  )
  metaflux_configure_target(metaflux_nvml_provider_policy_test C)
  add_test(
    NAME metaflux.integration.provider.nvml-policy-setters
    COMMAND metaflux_nvml_provider_policy_test $<TARGET_FILE:metafluxd>
  )
  set_tests_properties(
    metaflux.integration.provider.nvml-policy-setters
    PROPERTIES LABELS "abi;integration;nvml;policy;provider;runtime" TIMEOUT 60
  )
endif()

add_test(
  NAME metaflux.integration.provider.cuda-nvml-mode
  COMMAND
    metaflux_cuda_nvml_provider_mode_test
    $<TARGET_FILE:metaflux_cuda_provider_mode_fixture>
    $<TARGET_FILE:metaflux_nvml_provider_mode_fixture>
    $<TARGET_FILE:metaflux_passthrough_cuda_good>
    $<TARGET_FILE:metaflux_passthrough_nvml_good>
)
set_tests_properties(
  metaflux.integration.provider.cuda-nvml-mode
  PROPERTIES LABELS "abi;fault;integration;passthrough;provider" TIMEOUT 60
)

add_test(
  NAME metaflux.unit.provider.nvml-install-layout
  COMMAND
    "${CMAKE_COMMAND}"
    "-DINSTALL_SCRIPT=${CMAKE_CURRENT_BINARY_DIR}/../cmake_install.cmake"
    "-DINSTALL_LIBDIR=${CMAKE_INSTALL_LIBDIR}"
    "-DLIBRARY_STEM=libnvidia-ml"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/check_install_layout.cmake"
)
set_tests_properties(
  metaflux.unit.provider.nvml-install-layout
  PROPERTIES LABELS "abi;packaging;provider"
)
