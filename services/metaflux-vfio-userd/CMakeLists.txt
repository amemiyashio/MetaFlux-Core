add_executable(
  metaflux-vfio-userd
  main.cpp
)
target_compile_definitions(
  metaflux-vfio-userd
  PRIVATE "METAFLUX_PROJECT_VERSION=\"${PROJECT_VERSION}\""
)
target_link_libraries(
  metaflux-vfio-userd
  PRIVATE MetaFlux::TransportVfioUserServer
)
metaflux_configure_target(metaflux-vfio-userd CXX)
metaflux_register_component_graph_entry(
  metaflux-vfio-userd
  services.metaflux-vfio-userd
  daemon
  CXX
)

if(BUILD_TESTING AND METAFLUX_BUILD_TESTS)
  add_subdirectory(tests)
endif()

install(
  TARGETS metaflux-vfio-userd
  RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
  COMPONENT Daemon
)
