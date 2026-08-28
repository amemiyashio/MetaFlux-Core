add_library(metaflux_cuda_ptx_frontend STATIC src/frontend.cpp)
add_library(MetaFlux::CudaPtxFrontend ALIAS metaflux_cuda_ptx_frontend)

target_include_directories(
  metaflux_cuda_ptx_frontend
  PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
)
target_link_libraries(metaflux_cuda_ptx_frontend PUBLIC MetaFlux::CompilerCore)
metaflux_configure_target(metaflux_cuda_ptx_frontend CXX)
