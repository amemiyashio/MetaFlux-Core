metaflux_enforce_c_only_directory("${CMAKE_CURRENT_SOURCE_DIR}")
add_library(metaflux_transport_cdev_client STATIC src/cdev.c)
add_library(MetaFlux::TransportCdevClient ALIAS metaflux_transport_cdev_client)
set_target_properties(
  metaflux_transport_cdev_client
  PROPERTIES
    C_VISIBILITY_PRESET hidden
    EXPORT_NAME TransportCdevClient
    VISIBILITY_INLINES_HIDDEN YES
)
target_include_directories(
  metaflux_transport_cdev_client
  PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)
target_link_libraries(
  metaflux_transport_cdev_client
  PUBLIC MetaFlux::ClientFastpath MetaFlux::TransportSchema
)
metaflux_configure_target(metaflux_transport_cdev_client C)
metaflux_register_component_graph_entry(
  metaflux_transport_cdev_client
  transport.cdev.client
  transport-client
  C
)
install(
  TARGETS metaflux_transport_cdev_client
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
  add_executable(metaflux_transport_cdev_client_test tests/cdev_test.c)
  target_link_libraries(metaflux_transport_cdev_client_test PRIVATE MetaFlux::TransportCdevClient)
  metaflux_configure_target(metaflux_transport_cdev_client_test C)
  add_test(NAME metaflux.transport.cdev-client COMMAND metaflux_transport_cdev_client_test)
  set_tests_properties(metaflux.transport.cdev-client PROPERTIES LABELS "transport;unit")
endif()
