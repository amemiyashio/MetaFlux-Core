add_library(metaflux_vroot_model STATIC src/config_model.c)
add_library(MetaFlux::VrootModel ALIAS metaflux_vroot_model)

target_include_directories(
  metaflux_vroot_model
  PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
metaflux_configure_target(metaflux_vroot_model C)
metaflux_register_component_graph_entry(
  metaflux_vroot_model
  presentation.vroot-model
  pci-presentation
  C
)

if(BUILD_TESTING AND METAFLUX_BUILD_TESTS)
  add_executable(metaflux_vroot_config_model_test tests/config_model_test.c)
  target_link_libraries(metaflux_vroot_config_model_test PRIVATE MetaFlux::VrootModel)
  metaflux_configure_target(metaflux_vroot_config_model_test C)
  add_test(NAME metaflux.kernel.vroot-config-model COMMAND metaflux_vroot_config_model_test)
  set_tests_properties(
    metaflux.kernel.vroot-config-model
    PROPERTIES LABELS "kernel;vroot;pci;unit"
  )
endif()
