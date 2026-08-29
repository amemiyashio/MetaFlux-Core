find_package(Threads REQUIRED)

add_executable(metaflux_client_fastpath_ring_test ring.c)
target_link_libraries(
  metaflux_client_fastpath_ring_test
  PRIVATE MetaFlux::ClientFastpath Threads::Threads
)
metaflux_configure_target(metaflux_client_fastpath_ring_test C)
add_test(
  NAME metaflux.unit.client-fastpath-ring
  COMMAND metaflux_client_fastpath_ring_test
)
set_tests_properties(
  metaflux.unit.client-fastpath-ring
  PROPERTIES LABELS "unit;stress" TIMEOUT 60
)
