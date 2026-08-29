add_executable(metaflux_shared_device_layout_c_test layout.c)
target_link_libraries(
  metaflux_shared_device_layout_c_test
  PRIVATE MetaFlux::SharedDeviceLayout
)
metaflux_configure_target(metaflux_shared_device_layout_c_test C)
add_test(
  NAME metaflux.contract.shared-device-layout.c
  COMMAND metaflux_shared_device_layout_c_test
)
set_tests_properties(
  metaflux.contract.shared-device-layout.c
  PROPERTIES LABELS "abi;unit"
)
