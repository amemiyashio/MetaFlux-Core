add_library(metaflux_nvml_provider SHARED src/provider.c)
add_library(MetaFlux::NvmlProvider ALIAS metaflux_nvml_provider)

set_target_properties(
  metaflux_nvml_provider
  PROPERTIES
    C_VISIBILITY_PRESET hidden
    OUTPUT_NAME nvidia-ml
    SOVERSION 1
    VERSION 1.0.0
    VISIBILITY_INLINES_HIDDEN YES
)

target_include_directories(
  metaflux_nvml_provider
  PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)
target_link_libraries(metaflux_nvml_provider PRIVATE MetaFlux::ClientFastpath)
target_link_options(
  metaflux_nvml_provider
  PRIVATE "LINKER:--version-script=${CMAKE_CURRENT_SOURCE_DIR}/exports.map"
)
set_property(
  TARGET metaflux_nvml_provider
  APPEND
  PROPERTY LINK_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/exports.map"
)
metaflux_configure_target(metaflux_nvml_provider C)

install(
  TARGETS metaflux_nvml_provider
  LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  COMPONENT Provider
)
