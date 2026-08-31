find_package(Threads REQUIRED)

add_library(
  metaflux_vulkan_backend STATIC
  src/capability.cpp
  src/memory.cpp
  src/target.cpp
  src/streams.cpp
  src/cache.cpp
  src/vulkan_device.cpp
  src/vulkan_staging.cpp
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
target_link_libraries(metaflux_vulkan_backend PRIVATE "${METAFLUX_VULKAN_LIBRARY}" Threads::Threads)
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

  add_executable(metaflux_vulkan_streams_test tests/streams_test.cpp)
  target_link_libraries(metaflux_vulkan_streams_test PRIVATE MetaFlux::VulkanBackend)
  metaflux_configure_target(metaflux_vulkan_streams_test CXX)
  add_test(NAME metaflux.backend.vulkan-stream-graph COMMAND metaflux_vulkan_streams_test)
  set_tests_properties(
    metaflux.backend.vulkan-stream-graph
    PROPERTIES LABELS "backend;vulkan;execution"
  )

  add_executable(metaflux_vulkan_cache_test tests/cache_test.cpp)
  target_link_libraries(metaflux_vulkan_cache_test PRIVATE MetaFlux::VulkanBackend)
  metaflux_configure_target(metaflux_vulkan_cache_test CXX)
  add_test(NAME metaflux.backend.vulkan-cache-model COMMAND metaflux_vulkan_cache_test)
  set_tests_properties(
    metaflux.backend.vulkan-cache-model
    PROPERTIES LABELS "backend;vulkan;cache"
  )

  add_executable(metaflux_vulkan_device_test tests/vulkan_device_test.cpp)
  target_link_libraries(metaflux_vulkan_device_test PRIVATE MetaFlux::VulkanBackend)
  metaflux_configure_target(metaflux_vulkan_device_test CXX)
  add_test(NAME metaflux.backend.vulkan-device COMMAND metaflux_vulkan_device_test)
  set_tests_properties(
    metaflux.backend.vulkan-device
    PROPERTIES LABELS "backend;vulkan;device"
  )
endif()
