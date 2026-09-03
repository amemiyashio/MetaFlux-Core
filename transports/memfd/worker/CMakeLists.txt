add_library(metaflux_transport_memfd_worker STATIC src/worker.cpp)
add_library(MetaFlux::TransportMemfdWorker ALIAS metaflux_transport_memfd_worker)
set_target_properties(
  metaflux_transport_memfd_worker
  PROPERTIES EXPORT_NAME TransportMemfdWorker
)
target_include_directories(
  metaflux_transport_memfd_worker
  PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)
target_link_libraries(
  metaflux_transport_memfd_worker
  PUBLIC MetaFlux::RuntimeCore
)
metaflux_configure_target(metaflux_transport_memfd_worker CXX)

install(
  TARGETS metaflux_transport_memfd_worker
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
  add_executable(metaflux_transport_memfd_worker_test tests/worker_test.cpp)
  target_link_libraries(
    metaflux_transport_memfd_worker_test
    PRIVATE MetaFlux::TransportMemfdWorker
  )
  metaflux_configure_target(metaflux_transport_memfd_worker_test CXX)
  add_test(NAME metaflux.transport.memfd-worker COMMAND metaflux_transport_memfd_worker_test)
  set_tests_properties(metaflux.transport.memfd-worker PROPERTIES LABELS "transport;lifecycle;unit")

  add_executable(metaflux_transport_memfd_worker_lifecycle_failure_test tests/lifecycle_failure_test.cpp)
  target_link_libraries(
    metaflux_transport_memfd_worker_lifecycle_failure_test
    PRIVATE MetaFlux::TransportMemfdWorker
  )
  metaflux_configure_target(metaflux_transport_memfd_worker_lifecycle_failure_test CXX)
  add_test(NAME metaflux.transport.memfd-worker-lifecycle-failure
           COMMAND metaflux_transport_memfd_worker_lifecycle_failure_test)
  set_tests_properties(metaflux.transport.memfd-worker-lifecycle-failure
    PROPERTIES LABELS "transport;lifecycle;unit")
endif()
