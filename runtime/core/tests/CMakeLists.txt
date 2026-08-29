add_executable(metaflux_runtime_core_smoke runtime_smoke.cpp)
target_link_libraries(metaflux_runtime_core_smoke PRIVATE MetaFlux::RuntimeCore)
metaflux_configure_target(metaflux_runtime_core_smoke CXX)
add_test(NAME metaflux.unit.runtime COMMAND metaflux_runtime_core_smoke)
set_tests_properties(metaflux.unit.runtime PROPERTIES LABELS "unit")

add_executable(metaflux_shared_device_layout_cpp_test shared_layout.cpp)
target_link_libraries(
  metaflux_shared_device_layout_cpp_test
  PRIVATE MetaFlux::SharedDeviceLayout
)
metaflux_configure_target(metaflux_shared_device_layout_cpp_test CXX)
add_test(
  NAME metaflux.contract.shared-device-layout.cpp
  COMMAND metaflux_shared_device_layout_cpp_test
)
set_tests_properties(
  metaflux.contract.shared-device-layout.cpp
  PROPERTIES LABELS "abi;unit"
)

find_package(Threads REQUIRED)

add_library(metaflux_runtime_host_fixture STATIC host_fixture.cpp)
add_library(MetaFlux::RuntimeHostFixture ALIAS metaflux_runtime_host_fixture)
target_include_directories(
  metaflux_runtime_host_fixture
  PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
target_link_libraries(
  metaflux_runtime_host_fixture
  PUBLIC MetaFlux::RuntimeCore MetaFlux::ClientFastpath
)
metaflux_configure_target(metaflux_runtime_host_fixture CXX)

add_executable(metaflux_runtime_registry_test registry.cpp)
target_link_libraries(
  metaflux_runtime_registry_test
  PRIVATE MetaFlux::RuntimeCore Threads::Threads
)
metaflux_configure_target(metaflux_runtime_registry_test CXX)
add_test(NAME metaflux.unit.runtime-registry COMMAND metaflux_runtime_registry_test)
set_tests_properties(
  metaflux.unit.runtime-registry
  PROPERTIES LABELS "abi;unit;stress" TIMEOUT 60
)

add_executable(metaflux_runtime_registry_recovery_test registry_recovery.cpp)
target_link_libraries(
  metaflux_runtime_registry_recovery_test
  PRIVATE MetaFlux::RuntimeCore
)
metaflux_configure_target(metaflux_runtime_registry_recovery_test CXX)
add_test(
  NAME metaflux.integration.runtime-registry-recovery
  COMMAND metaflux_runtime_registry_recovery_test
)
set_tests_properties(
  metaflux.integration.runtime-registry-recovery
  PROPERTIES LABELS "abi;integration;recovery" TIMEOUT 60
)

add_executable(metaflux_runtime_fastpath_registry_test fastpath_registry.cpp)
target_link_libraries(
  metaflux_runtime_fastpath_registry_test
  PRIVATE MetaFlux::RuntimeCore MetaFlux::ClientFastpath
)
metaflux_configure_target(metaflux_runtime_fastpath_registry_test CXX)
add_test(
  NAME metaflux.integration.runtime-fastpath-registry
  COMMAND metaflux_runtime_fastpath_registry_test
)
set_tests_properties(
  metaflux.integration.runtime-fastpath-registry
  PROPERTIES LABELS "abi;integration;recovery" TIMEOUT 60
)

add_executable(metaflux_runtime_recovery_model_test recovery_model.cpp)
target_link_libraries(
  metaflux_runtime_recovery_model_test
  PRIVATE MetaFlux::RuntimeCore
)
metaflux_configure_target(metaflux_runtime_recovery_model_test CXX)
add_test(NAME metaflux.unit.runtime-recovery-model COMMAND metaflux_runtime_recovery_model_test)
set_tests_properties(
  metaflux.unit.runtime-recovery-model
  PROPERTIES LABELS "abi;model;unit" TIMEOUT 60
)

add_executable(metaflux_runtime_host_fixture_test host_fixture_test.cpp)
target_link_libraries(
  metaflux_runtime_host_fixture_test
  PRIVATE MetaFlux::RuntimeHostFixture
)
metaflux_configure_target(metaflux_runtime_host_fixture_test CXX)
add_test(NAME metaflux.integration.runtime-host-fixture COMMAND metaflux_runtime_host_fixture_test)
set_tests_properties(
  metaflux.integration.runtime-host-fixture
  PROPERTIES LABELS "integration;unit" TIMEOUT 60
)
