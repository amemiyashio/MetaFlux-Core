add_library(metaflux_shared_device_layout INTERFACE)
add_library(MetaFlux::SharedDeviceLayout ALIAS metaflux_shared_device_layout)
set_target_properties(
  metaflux_shared_device_layout
  PROPERTIES EXPORT_NAME SharedDeviceLayout
)

target_include_directories(
  metaflux_shared_device_layout
  INTERFACE
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

install(
  DIRECTORY include/
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
  COMPONENT Runtime
)
