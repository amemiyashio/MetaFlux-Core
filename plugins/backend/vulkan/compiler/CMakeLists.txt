add_library(metaflux_vulkan_compiler STATIC src/lowering.cpp)
add_library(MetaFlux::VulkanCompiler ALIAS metaflux_vulkan_compiler)

target_include_directories(
  metaflux_vulkan_compiler
  PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${PROJECT_SOURCE_DIR}/plugins/backend/vulkan/runtime/include"
    "${PROJECT_SOURCE_DIR}/compiler/core/include"
    "${PROJECT_SOURCE_DIR}/contracts/plugin/backend/v1/include"
)
target_link_libraries(
  metaflux_vulkan_compiler
  PRIVATE MetaFlux::CompilerCore MetaFlux::VulkanBackend
)
metaflux_configure_target(metaflux_vulkan_compiler CXX)
metaflux_register_component_graph_entry(
  metaflux_vulkan_compiler
  compiler.vulkan-lowering
  backend-compiler
  CXX
)

if(BUILD_TESTING AND METAFLUX_BUILD_TESTS)
  add_executable(metaflux_vulkan_lowering_test tests/lowering_test.cpp)
  target_link_libraries(metaflux_vulkan_lowering_test PRIVATE MetaFlux::VulkanCompiler)
  metaflux_configure_target(metaflux_vulkan_lowering_test CXX)
  add_test(NAME metaflux.backend.vulkan-lowering COMMAND metaflux_vulkan_lowering_test)
  set_tests_properties(
    metaflux.backend.vulkan-lowering
    PROPERTIES LABELS "backend;vulkan;compiler;unit"
               ENVIRONMENT "LD_LIBRARY_PATH=${METAFLUX_VULKAN_TEST_LD_LIBRARY_PATH}"
  )
endif()
