metaflux_enforce_c_only_directory("${CMAKE_CURRENT_SOURCE_DIR}")
add_library(metaflux_transport_vfio_user_guest STATIC src/guest.c)
add_library(MetaFlux::TransportVfioUserGuest ALIAS metaflux_transport_vfio_user_guest)
set_target_properties(
  metaflux_transport_vfio_user_guest
  PROPERTIES
    C_VISIBILITY_PRESET hidden
    EXPORT_NAME TransportVfioUserGuest
    VISIBILITY_INLINES_HIDDEN YES
)
target_include_directories(
  metaflux_transport_vfio_user_guest
  PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)
target_link_libraries(
  metaflux_transport_vfio_user_guest
  PUBLIC MetaFlux::TransportSchema MetaFlux::ClientFastpath
)
metaflux_configure_target(metaflux_transport_vfio_user_guest C)
metaflux_register_component_graph_entry(
  metaflux_transport_vfio_user_guest
  transport.vfio-user.guest
  transport-client
  C
)
install(
  TARGETS metaflux_transport_vfio_user_guest
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
  add_executable(metaflux_transport_vfio_user_guest_test tests/guest_test.c)
  target_link_libraries(
    metaflux_transport_vfio_user_guest_test
    PRIVATE MetaFlux::TransportVfioUserGuest
  )
  metaflux_configure_target(metaflux_transport_vfio_user_guest_test C)
  add_test(NAME metaflux.transport.vfio-user-guest COMMAND metaflux_transport_vfio_user_guest_test)
  set_tests_properties(metaflux.transport.vfio-user-guest PROPERTIES LABELS "transport;unit")

  add_executable(metaflux_transport_vfio_user_guest_protocol_fuzz_test
                 tests/protocol_fuzz_test.c)
  target_link_libraries(
    metaflux_transport_vfio_user_guest_protocol_fuzz_test
    PRIVATE MetaFlux::TransportVfioUserGuest
  )
  metaflux_configure_target(metaflux_transport_vfio_user_guest_protocol_fuzz_test C)
  add_test(
    NAME metaflux.transport.vfio-user-guest-protocol-fuzz
    COMMAND metaflux_transport_vfio_user_guest_protocol_fuzz_test
  )
  set_tests_properties(
    metaflux.transport.vfio-user-guest-protocol-fuzz
    PROPERTIES LABELS "transport;qualification;fuzz"
  )

  add_executable(metaflux_transport_vfio_user_ring_backend_test tests/ring_backend_test.c)
  target_link_libraries(
    metaflux_transport_vfio_user_ring_backend_test
    PRIVATE MetaFlux::TransportVfioUserGuest
  )
  metaflux_configure_target(metaflux_transport_vfio_user_ring_backend_test C)
  add_test(NAME metaflux.transport.vfio-user-ring-backend COMMAND metaflux_transport_vfio_user_ring_backend_test)
  set_tests_properties(metaflux.transport.vfio-user-ring-backend PROPERTIES LABELS "transport;unit")
endif()
