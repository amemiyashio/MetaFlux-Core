find_package(Python3 REQUIRED COMPONENTS Interpreter)

add_library(
  metaflux_transport_vfio_user_server STATIC
  src/server.cpp
  src/qmp_lifecycle.cpp
  src/qmp_socket.cpp
  src/msix.cpp
)
add_library(MetaFlux::TransportVfioUserServer ALIAS metaflux_transport_vfio_user_server)
set_target_properties(metaflux_transport_vfio_user_server PROPERTIES EXPORT_NAME TransportVfioUserServer)
target_include_directories(
  metaflux_transport_vfio_user_server
  PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)
target_link_libraries(
  metaflux_transport_vfio_user_server
  PUBLIC MetaFlux::TransportSchema MetaFlux::RuntimeCore
)
metaflux_configure_target(metaflux_transport_vfio_user_server CXX)
metaflux_register_component_graph_entry(
  metaflux_transport_vfio_user_server
  transport.vfio-user.server
  transport-worker
  CXX
)
install(
  TARGETS metaflux_transport_vfio_user_server
  EXPORT MetaFluxRuntimeTargets
  ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  COMPONENT Runtime
)
install(
  DIRECTORY include/
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
  COMPONENT Runtime
)

if(BUILD_TESTING AND METAFLUX_BUILD_TESTS)
  add_executable(metaflux_transport_vfio_user_server_test tests/server_test.cpp)
  target_link_libraries(
    metaflux_transport_vfio_user_server_test
    PRIVATE MetaFlux::TransportVfioUserServer MetaFlux::TransportVfioUserGuest
  )
  target_include_directories(
    metaflux_transport_vfio_user_server_test
    PRIVATE "${PROJECT_SOURCE_DIR}/transports/vfio-user/guest/include"
  )
  metaflux_configure_target(metaflux_transport_vfio_user_server_test CXX)
  add_test(NAME metaflux.transport.vfio-user-server COMMAND metaflux_transport_vfio_user_server_test)
  set_tests_properties(metaflux.transport.vfio-user-server PROPERTIES LABELS "transport;unit")

  add_executable(metaflux_transport_vfio_user_server_fuzz_test tests/server_fuzz_test.cpp)
  target_link_libraries(
    metaflux_transport_vfio_user_server_fuzz_test
    PRIVATE MetaFlux::TransportVfioUserServer
  )
  metaflux_configure_target(metaflux_transport_vfio_user_server_fuzz_test CXX)
  add_test(NAME metaflux.transport.vfio-user-server-fuzz COMMAND metaflux_transport_vfio_user_server_fuzz_test)
  set_tests_properties(metaflux.transport.vfio-user-server-fuzz PROPERTIES LABELS "transport;fuzz;unit")

  add_test(
    NAME metaflux.transport.vfio-user-profile
    COMMAND
      "${Python3_EXECUTABLE}" -B "${PROJECT_SOURCE_DIR}/tools/validate-vfio-user-profile.py"
      --root "${PROJECT_SOURCE_DIR}"
      --manifest "${PROJECT_SOURCE_DIR}/contracts/protocol/transport/v1/schema/manifest.json"
      --generate-header "${CMAKE_CURRENT_BINARY_DIR}/test-generated-vfio-user-profile.h"
  )
  set_tests_properties(
    metaflux.transport.vfio-user-profile
    PROPERTIES LABELS "transport;schema"
  )
  add_test(
    NAME metaflux.transport.vfio-user-profile-selftest
    COMMAND "${Python3_EXECUTABLE}" -B "${PROJECT_SOURCE_DIR}/tools/test-vfio-user-profile.py"
  )
  set_tests_properties(
    metaflux.transport.vfio-user-profile-selftest
    PROPERTIES LABELS "transport;schema;unit"
  )
  add_test(
    NAME metaflux.transport.pci-guest-profile
    COMMAND
      "${Python3_EXECUTABLE}" -B "${PROJECT_SOURCE_DIR}/tools/generate-pci-guest-profile.py"
      --root "${PROJECT_SOURCE_DIR}"
      --generate-header "${CMAKE_CURRENT_BINARY_DIR}/test-generated-pci-guest-profile.h"
  )
  set_tests_properties(
    metaflux.transport.pci-guest-profile
    PROPERTIES LABELS "transport;schema;pci"
  )
  add_test(
    NAME metaflux.transport.pci-guest-profile-selftest
    COMMAND "${Python3_EXECUTABLE}" -B "${PROJECT_SOURCE_DIR}/tools/test-pci-guest-profile.py"
  )
  set_tests_properties(
    metaflux.transport.pci-guest-profile-selftest
    PROPERTIES LABELS "transport;schema;unit;pci"
  )

  add_executable(metaflux_transport_vfio_user_msix_test tests/msix_test.cpp)
  target_link_libraries(
    metaflux_transport_vfio_user_msix_test
    PRIVATE MetaFlux::TransportVfioUserServer
  )
  metaflux_configure_target(metaflux_transport_vfio_user_msix_test CXX)
  add_test(NAME metaflux.transport.vfio-user-msix COMMAND metaflux_transport_vfio_user_msix_test)
  set_tests_properties(metaflux.transport.vfio-user-msix PROPERTIES LABELS "transport;unit")

  add_executable(metaflux_transport_vfio_user_qmp_test tests/qmp_lifecycle_test.cpp)
  target_link_libraries(
    metaflux_transport_vfio_user_qmp_test
    PRIVATE MetaFlux::TransportVfioUserServer
  )
  metaflux_configure_target(metaflux_transport_vfio_user_qmp_test CXX)
  add_test(NAME metaflux.transport.vfio-user-qmp COMMAND metaflux_transport_vfio_user_qmp_test)
  set_tests_properties(
    metaflux.transport.vfio-user-qmp
    PROPERTIES LABELS "transport;lifecycle;unit"
  )

  add_executable(metaflux_transport_vfio_user_qmp_socket_test tests/qmp_socket_test.cpp)
  target_link_libraries(
    metaflux_transport_vfio_user_qmp_socket_test
    PRIVATE MetaFlux::TransportVfioUserServer
  )
  metaflux_configure_target(metaflux_transport_vfio_user_qmp_socket_test CXX)
  add_test(
    NAME metaflux.transport.vfio-user-qmp-socket
    COMMAND metaflux_transport_vfio_user_qmp_socket_test
  )
  set_tests_properties(
    metaflux.transport.vfio-user-qmp-socket
    PROPERTIES LABELS "transport;lifecycle;unit"
  )

  add_executable(metaflux_transport_vfio_user_interleave_test tests/interleave_test.cpp)
  target_link_libraries(
    metaflux_transport_vfio_user_interleave_test
    PRIVATE MetaFlux::TransportVfioUserServer MetaFlux::TransportVfioUserGuest
  )
  target_include_directories(
    metaflux_transport_vfio_user_interleave_test
    PRIVATE "${PROJECT_SOURCE_DIR}/transports/vfio-user/guest/include"
  )
  metaflux_configure_target(metaflux_transport_vfio_user_interleave_test CXX)
  add_test(NAME metaflux.transport.vfio-user-interleave COMMAND metaflux_transport_vfio_user_interleave_test)
  set_tests_properties(metaflux.transport.vfio-user-interleave PROPERTIES LABELS "transport;unit")

  add_executable(metaflux_transport_vfio_user_lifecycle_failure_test tests/lifecycle_failure_test.cpp)
  target_link_libraries(
    metaflux_transport_vfio_user_lifecycle_failure_test
    PRIVATE MetaFlux::TransportVfioUserServer MetaFlux::TransportVfioUserGuest
  )
  target_include_directories(
    metaflux_transport_vfio_user_lifecycle_failure_test
    PRIVATE "${PROJECT_SOURCE_DIR}/transports/vfio-user/guest/include"
  )
  metaflux_configure_target(metaflux_transport_vfio_user_lifecycle_failure_test CXX)
  add_test(NAME metaflux.transport.vfio-user-lifecycle-failure COMMAND metaflux_transport_vfio_user_lifecycle_failure_test)
  set_tests_properties(
    metaflux.transport.vfio-user-lifecycle-failure
    PROPERTIES LABELS "transport;lifecycle;unit"
  )
endif()
