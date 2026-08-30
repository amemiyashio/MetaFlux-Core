add_library(
  metaflux_vulkan_backend STATIC
  src/capability.cpp
  src/memory.cpp
  src/target.cpp
)
add_library(MetaFlux::VulkanBackend ALIAS metaflux_vulkan_backend)

target_include_directories(
  metaflux_vulkan_backend
  PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/contracts/plugin/backend/v1/include>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)
target_include_directories(metaflux_vulkan_backend SYSTEM PRIVATE "${METAFLUX_VULKAN_INCLUDE_DIR}")
target_link_libraries(metaflux_vulkan_backend PRIVATE "${METAFLUX_VULKAN_LIBRARY}")
metaflux_configure_target(metaflux_vulkan_backend CXX)

if(BUILD_TESTING AND METAFLUX_BUILD_TESTS)
  add_executable(metaflux_vulkan_capability_test tests/capability_test.cpp)
  target_link_libraries(metaflux_vulkan_capability_test PRIVATE MetaFlux::VulkanBackend)
  metaflux_configure_target(metaflux_vulkan_capability_test CXX)
  add_test(NAME metaflux.backend.vulkan-capability COMMAND metaflux_vulkan_capability_test)
  set_tests_properties(
    metaflux.backend.vulkan-capability
    PROPERTIES LABELS "backend;vulkan;capability"
  )

  add_executable(metaflux_vulkan_memory_test tests/memory_test.cpp)
  target_link_libraries(metaflux_vulkan_memory_test PRIVATE MetaFlux::VulkanBackend)
  metaflux_configure_target(metaflux_vulkan_memory_test CXX)
  add_test(NAME metaflux.backend.vulkan-memory COMMAND metaflux_vulkan_memory_test)
  set_tests_properties(
    metaflux.backend.vulkan-memory
    PROPERTIES LABELS "backend;vulkan;memory"
  )

  add_executable(metaflux_vulkan_target_test tests/target_test.cpp)
  target_link_libraries(metaflux_vulkan_target_test PRIVATE MetaFlux::VulkanBackend)
  metaflux_configure_target(metaflux_vulkan_target_test CXX)
  add_test(NAME metaflux.backend.vulkan-target-preflight COMMAND metaflux_vulkan_target_test)
  set_tests_properties(
    metaflux.backend.vulkan-target-preflight
    PROPERTIES LABELS "backend;vulkan;compiler"
  )
endif()
