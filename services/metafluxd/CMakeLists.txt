find_package(Threads REQUIRED)
find_package(ZLIB REQUIRED)

if(
  NOT TARGET metaflux_cuda_ptx_frontend
  OR NOT TARGET metaflux_cpu_backend_compiler
  OR NOT TARGET metaflux_cpu_backend_runtime
)
  message(
    FATAL_ERROR
      "metafluxd requires the PTX frontend, CPU backend compiler, and CPU backend runtime"
  )
endif()

add_executable(
  metafluxd
  compiler_worker_client.cpp
  compiler_worker_process.cpp
  compiler_worker_protocol.cpp
  execution.cpp
  main.cpp
  server.cpp
)
target_compile_definitions(
  metafluxd
  PRIVATE "METAFLUX_PROJECT_VERSION=\"${PROJECT_VERSION}\""
)

target_link_libraries(
  metafluxd
  PRIVATE
    MetaFlux::RuntimeCore
    MetaFlux::CompilerCore
    MetaFlux::ClientFastpath
    MetaFlux::CudaPtxFrontend
    MetaFlux::CpuBackendCompiler
    MetaFlux::CpuBackendRuntime
    Threads::Threads
    ZLIB::ZLIB
)
target_include_directories(
  metafluxd
  PRIVATE "${PROJECT_SOURCE_DIR}/transports/cdev/worker/include"
)
if(TARGET MetaFlux::TransportCdevWorker)
  target_link_libraries(metafluxd PRIVATE MetaFlux::TransportCdevWorker)
  target_compile_definitions(metafluxd PRIVATE METAFLUX_DAEMON_CDEV_BACKEND=1)
endif()
# Keep zlib/libstdcxx discoverable under env-cleared live/root helpers that
# invoke the build-tree daemon without the ambient Nix shell PATH/LD path.
get_filename_component(_metafluxd_zlib_dir "${ZLIB_LIBRARY}" DIRECTORY)
set_target_properties(
  metafluxd
  PROPERTIES
    BUILD_RPATH "${_metafluxd_zlib_dir};$ORIGIN;$ORIGIN/../lib"
    INSTALL_RPATH "$ORIGIN;$ORIGIN/../lib"
)
metaflux_configure_target(metafluxd CXX)
metaflux_register_component_graph_entry(
  metafluxd
  services.metafluxd
  daemon
  CXX
)

if(BUILD_TESTING AND METAFLUX_BUILD_TESTS)
  add_subdirectory(tests)
endif()

install(
  TARGETS metafluxd
  RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
  COMPONENT Daemon
)
install(
  FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/../../packaging/common/metafluxd.service"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../packaging/common/metafluxd.socket"
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/systemd/system"
  COMPONENT Daemon
)
install(
  FILES "${CMAKE_CURRENT_SOURCE_DIR}/../../packaging/common/metaflux.sysusers"
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/sysusers.d"
  RENAME metaflux.conf
  COMPONENT Daemon
)
install(
  FILES "${CMAKE_CURRENT_SOURCE_DIR}/../../packaging/common/metaflux.tmpfiles"
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/tmpfiles.d"
  RENAME metaflux.conf
  COMPONENT Daemon
)
install(
  FILES "${CMAKE_CURRENT_SOURCE_DIR}/../../packaging/common/70-metaflux.rules"
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/udev/rules.d"
  COMPONENT Daemon
)
install(
  FILES "${CMAKE_CURRENT_SOURCE_DIR}/../../packaging/common/vendor-libraries.conf.example"
  DESTINATION "${CMAKE_INSTALL_DATADIR}/doc/metaflux"
  COMPONENT Daemon
)
