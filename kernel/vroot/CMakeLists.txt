find_package(Python3 REQUIRED COMPONENTS Interpreter)

set(MF_VROOT_PROFILE_HEADER_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated/include/metaflux/vroot")
set(MF_VROOT_PROFILE_HEADER "${MF_VROOT_PROFILE_HEADER_DIR}/generated_profile.h")
add_custom_command(
  OUTPUT "${MF_VROOT_PROFILE_HEADER}"
  COMMAND
    Python3::Interpreter "${PROJECT_SOURCE_DIR}/tools/validate-vroot-profile.py"
    --root "${PROJECT_SOURCE_DIR}"
    --manifest "${PROJECT_SOURCE_DIR}/contracts/protocol/transport/v1/schema/extensions/vroot/v1/manifest.json"
    --generate-header "${MF_VROOT_PROFILE_HEADER}"
  DEPENDS
    "${PROJECT_SOURCE_DIR}/tools/validate-vroot-profile.py"
    "${PROJECT_SOURCE_DIR}/contracts/protocol/transport/v1/schema/extensions/vroot/v1/manifest.json"
    "${PROJECT_SOURCE_DIR}/contracts/protocol/transport/v1/schema/extensions/vroot/v1/profile.json"
    "${PROJECT_SOURCE_DIR}/contracts/protocol/transport/v1/schema/manifest.json"
    "${PROJECT_SOURCE_DIR}/contracts/protocol/transport/v1/schema/extensions/lifecycle/v1/manifest.json"
    "${PROJECT_SOURCE_DIR}/contracts/protocol/transport/v1/schema/extensions/lifecycle/v1/model.json"
  VERBATIM
)
add_custom_target(metaflux_vroot_profile DEPENDS "${MF_VROOT_PROFILE_HEADER}")

add_library(metaflux_vroot_model STATIC src/config_model.c)
add_library(MetaFlux::VrootModel ALIAS metaflux_vroot_model)
add_dependencies(metaflux_vroot_model metaflux_vroot_profile)

target_include_directories(
  metaflux_vroot_model
  PUBLIC
    "${CMAKE_CURRENT_BINARY_DIR}/generated/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
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
  add_test(
    NAME metaflux.kernel.vroot-profile
    COMMAND
      "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tools/validate-vroot-profile.py"
      --root "${PROJECT_SOURCE_DIR}"
      --manifest "${PROJECT_SOURCE_DIR}/contracts/protocol/transport/v1/schema/extensions/vroot/v1/manifest.json"
      --generate-header "${CMAKE_CURRENT_BINARY_DIR}/test-generated-profile.h"
  )
  set_tests_properties(
    metaflux.kernel.vroot-profile
    PROPERTIES LABELS "kernel;vroot;pci;schema"
  )
  add_test(
    NAME metaflux.kernel.vroot-profile-selftest
    COMMAND "${Python3_EXECUTABLE}" -B "${PROJECT_SOURCE_DIR}/tests/lifecycle/test-vroot-profile.py"
  )
  set_tests_properties(
    metaflux.kernel.vroot-profile-selftest
    PROPERTIES LABELS "kernel;vroot;pci;unit"
  )
endif()
