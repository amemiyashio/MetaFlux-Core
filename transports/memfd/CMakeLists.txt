metaflux_add_component(
  ID transport.memfd.client
  ROLE transport-client
  OPTION METAFLUX_BUILD_MEMFD_TRANSPORT
  DIRECTORY client
  TARGET metaflux_transport_memfd_client
  LANGUAGE C
)

metaflux_add_component(
  ID transport.memfd.worker
  ROLE transport-worker
  OPTION METAFLUX_BUILD_MEMFD_TRANSPORT
  DIRECTORY worker
  TARGET metaflux_transport_memfd_worker
  LANGUAGE CXX
)
