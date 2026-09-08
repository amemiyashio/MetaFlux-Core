add_library(metaflux_gpu_backend_passthrough STATIC gpu_passthrough.cpp)
add_library(MetaFlux::GpuBackendPassthrough ALIAS metaflux_gpu_backend_passthrough)

target_include_directories(
  metaflux_gpu_backend_passthrough
  PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)
target_link_libraries(
  metaflux_gpu_backend_passthrough
  PUBLIC MetaFlux::BackendPluginApi
  PRIVATE ${CMAKE_DL_LIBS}
)
metaflux_configure_target(metaflux_gpu_backend_passthrough CXX)
