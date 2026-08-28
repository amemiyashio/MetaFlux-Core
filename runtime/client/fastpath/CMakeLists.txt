add_library(metaflux_client_fastpath STATIC src/fastpath.c)
add_library(MetaFlux::ClientFastpath ALIAS metaflux_client_fastpath)

set_target_properties(
  metaflux_client_fastpath
  PROPERTIES
    C_VISIBILITY_PRESET hidden
    EXPORT_NAME ClientFastpath
    VISIBILITY_INLINES_HIDDEN YES
)

target_include_directories(
  metaflux_client_fastpath
  PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)
target_link_libraries(metaflux_client_fastpath PUBLIC MetaFlux::ClientProtocol)
metaflux_configure_target(metaflux_client_fastpath C)

install(
  TARGETS metaflux_client_fastpath
  EXPORT MetaFluxRuntimeTargets
  ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  COMPONENT Runtime
)
install(
  DIRECTORY include/
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
  COMPONENT Runtime
)
