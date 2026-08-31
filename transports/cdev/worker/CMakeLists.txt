add_library(metaflux_transport_cdev_worker STATIC src/worker.cpp)
add_library(MetaFlux::TransportCdevWorker ALIAS metaflux_transport_cdev_worker)
set_target_properties(metaflux_transport_cdev_worker PROPERTIES EXPORT_NAME TransportCdevWorker)
target_include_directories(
  metaflux_transport_cdev_worker
  PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
    "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/contracts/shared/device/v1/include>"
)
target_link_libraries(
  metaflux_transport_cdev_worker
  PUBLIC MetaFlux::TransportSchema MetaFlux::RuntimeCore MetaFlux::BackendPluginApi
)
metaflux_configure_target(metaflux_transport_cdev_worker CXX)
metaflux_register_component_graph_entry(
  metaflux_transport_cdev_worker
  transport.cdev.worker
  transport-worker
  CXX
)
install(
  TARGETS metaflux_transport_cdev_worker
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
  add_executable(metaflux_transport_cdev_worker_test tests/worker_test.cpp)
  target_link_libraries(
    metaflux_transport_cdev_worker_test
    PRIVATE MetaFlux::TransportCdevWorker MetaFlux::ClientFastpath
  )
  if(TARGET metaflux_cpu_backend_runtime)
    target_link_libraries(metaflux_transport_cdev_worker_test PRIVATE MetaFlux::CpuBackendRuntime)
    target_compile_definitions(metaflux_transport_cdev_worker_test PRIVATE METAFLUX_CPU_BACKEND=1)
  endif()
  if(TARGET metaflux_cpu_backend_runtime AND TARGET metaflux_cuda_ptx_frontend AND
     TARGET metaflux_compiler_core)
    target_link_libraries(
      metaflux_transport_cdev_worker_test
      PRIVATE MetaFlux::CompilerCore MetaFlux::CudaPtxFrontend
    )
    target_compile_definitions(
      metaflux_transport_cdev_worker_test
      PRIVATE
        METAFLUX_CPU_CDEV_LAUNCH=1
        METAFLUX_CPU_ADD_PTX="${PROJECT_SOURCE_DIR}/plugins/compat/cuda/compiler/ptx/corpus/fixtures/positive-add-copy.ptx"
    )
  endif()
  target_include_directories(
    metaflux_transport_cdev_worker_test
    PRIVATE "${PROJECT_SOURCE_DIR}/contracts/shared/device/v1/include"
  )
  metaflux_configure_target(metaflux_transport_cdev_worker_test CXX)
  add_test(NAME metaflux.transport.cdev-worker COMMAND metaflux_transport_cdev_worker_test)
  set_tests_properties(metaflux.transport.cdev-worker PROPERTIES LABELS "transport;unit")
endif()
