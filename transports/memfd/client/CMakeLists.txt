add_library(metaflux_transport_memfd_client INTERFACE)
add_library(MetaFlux::TransportMemfdClient ALIAS metaflux_transport_memfd_client)
set_target_properties(
  metaflux_transport_memfd_client
  PROPERTIES EXPORT_NAME TransportMemfdClient
)
target_link_libraries(
  metaflux_transport_memfd_client
  INTERFACE MetaFlux::ClientFastpath
)

install(
  TARGETS metaflux_transport_memfd_client
  EXPORT MetaFluxRuntimeTargets
  COMPONENT Runtime
)
