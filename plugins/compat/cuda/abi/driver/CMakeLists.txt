add_library(metaflux_cuda_provider SHARED src/provider.c)
add_library(MetaFlux::CudaProvider ALIAS metaflux_cuda_provider)

set_target_properties(
  metaflux_cuda_provider
  PROPERTIES
    C_VISIBILITY_PRESET hidden
    OUTPUT_NAME cuda
    SOVERSION 1
    VERSION 1.0.0
    VISIBILITY_INLINES_HIDDEN YES
)

target_include_directories(
  metaflux_cuda_provider
  PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)
target_link_libraries(metaflux_cuda_provider PRIVATE MetaFlux::ClientFastpath)
target_link_options(
  metaflux_cuda_provider
  PRIVATE "LINKER:--version-script=${CMAKE_CURRENT_SOURCE_DIR}/exports.map"
)
set_property(
  TARGET metaflux_cuda_provider
  APPEND
  PROPERTY LINK_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/exports.map"
)
metaflux_configure_target(metaflux_cuda_provider C)

install(
  TARGETS metaflux_cuda_provider
  LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  COMPONENT Provider
)
