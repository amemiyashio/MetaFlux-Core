add_library(metaflux_cpu_backend_runtime STATIC src/backend.cpp)
add_library(MetaFlux::CpuBackendRuntime ALIAS metaflux_cpu_backend_runtime)

target_include_directories(
  metaflux_cpu_backend_runtime
  PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)
target_link_libraries(metaflux_cpu_backend_runtime PUBLIC MetaFlux::BackendPluginApi)
metaflux_configure_target(metaflux_cpu_backend_runtime CXX)
