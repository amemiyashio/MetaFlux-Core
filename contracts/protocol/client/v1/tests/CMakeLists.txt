add_executable(metaflux_client_protocol_v1_test protocol.c)
target_link_libraries(
  metaflux_client_protocol_v1_test
  PRIVATE MetaFlux::ClientProtocol
)
metaflux_configure_target(metaflux_client_protocol_v1_test C)
add_test(
  NAME metaflux.contract.client-protocol.v1
  COMMAND metaflux_client_protocol_v1_test
)
set_tests_properties(
  metaflux.contract.client-protocol.v1
  PROPERTIES LABELS "abi;unit"
)
