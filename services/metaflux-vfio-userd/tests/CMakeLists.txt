add_executable(
  metaflux-vfio-userd-test
  service_test.cpp
)
target_link_libraries(
  metaflux-vfio-userd-test
  PRIVATE MetaFlux::TransportVfioUserGuest MetaFlux::ClientFastpath
)
metaflux_configure_target(metaflux-vfio-userd-test CXX)
add_dependencies(metaflux-vfio-userd-test metaflux_vfio_user_profile)
add_test(
  NAME metaflux.services.vfio-userd
  COMMAND metaflux-vfio-userd-test "$<TARGET_FILE:metaflux-vfio-userd>"
)
set_tests_properties(
  metaflux.services.vfio-userd
  PROPERTIES LABELS "service;transport;integration"
)
