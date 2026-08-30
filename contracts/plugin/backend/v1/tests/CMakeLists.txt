add_executable(metaflux_backend_plugin_api_v1_test api.c)
target_link_libraries(
  metaflux_backend_plugin_api_v1_test
  PRIVATE MetaFlux::BackendPluginApi
)
metaflux_configure_target(metaflux_backend_plugin_api_v1_test C)
add_test(
  NAME metaflux.contract.backend-plugin-api.v1
  COMMAND metaflux_backend_plugin_api_v1_test
)
set_tests_properties(
  metaflux.contract.backend-plugin-api.v1
  PROPERTIES LABELS "abi;unit"
)

add_executable(metaflux_backend_vulkan_abi_test vulkan.c)
target_link_libraries(
  metaflux_backend_vulkan_abi_test
  PRIVATE MetaFlux::BackendPluginApi
)
metaflux_configure_target(metaflux_backend_vulkan_abi_test C)
add_test(
  NAME metaflux.contract.backend-vulkan-abi.v1
  COMMAND metaflux_backend_vulkan_abi_test
)
set_tests_properties(
  metaflux.contract.backend-vulkan-abi.v1
  PROPERTIES LABELS "abi;unit;vulkan"
)

add_executable(metaflux_backend_vulkan_arguments_test vulkan_arguments.c)
target_link_libraries(
  metaflux_backend_vulkan_arguments_test
  PRIVATE MetaFlux::BackendPluginApi
)
metaflux_configure_target(metaflux_backend_vulkan_arguments_test C)
add_test(
  NAME metaflux.contract.backend-vulkan-arguments.v1
  COMMAND metaflux_backend_vulkan_arguments_test
)
set_tests_properties(
  metaflux.contract.backend-vulkan-arguments.v1
  PROPERTIES LABELS "abi;unit;vulkan"
)

add_executable(metaflux_backend_vulkan_memory_test vulkan_memory.c)
target_link_libraries(
  metaflux_backend_vulkan_memory_test
  PRIVATE MetaFlux::BackendPluginApi
)
metaflux_configure_target(metaflux_backend_vulkan_memory_test C)
add_test(
  NAME metaflux.contract.backend-vulkan-memory.v0
  COMMAND metaflux_backend_vulkan_memory_test
)
set_tests_properties(
  metaflux.contract.backend-vulkan-memory.v0
  PROPERTIES LABELS "abi;unit;vulkan"
)
