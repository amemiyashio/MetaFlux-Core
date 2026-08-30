add_library(metaflux_transport_vfio_user_server STATIC src/server.cpp)
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
endif()
