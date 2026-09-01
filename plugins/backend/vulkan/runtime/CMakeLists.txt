find_package(Threads REQUIRED)

add_library(
  metaflux_vulkan_backend STATIC
  src/capability.cpp
  src/memory.cpp
  src/target.cpp
  src/streams.cpp
  src/cache.cpp
  src/vulkan_device.cpp
  src/vulkan_admission.cpp
  src/vulkan_queue.cpp
  src/vulkan_pipeline.cpp
  src/vulkan_pipeline_cache.cpp
  src/vulkan_staging.cpp
  src/vulkan_device_copy.cpp
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

  add_executable(metaflux_vulkan_queue_test tests/vulkan_queue_test.cpp)
  target_link_libraries(metaflux_vulkan_queue_test PRIVATE MetaFlux::VulkanBackend)
  metaflux_configure_target(metaflux_vulkan_queue_test CXX)
  add_test(NAME metaflux.backend.vulkan-queue-executor COMMAND metaflux_vulkan_queue_test)
  set_tests_properties(
    metaflux.backend.vulkan-queue-executor
    PROPERTIES LABELS "backend;vulkan;execution"
  )

  find_program(
    METAFLUX_GLSLANG_VALIDATOR
    NAMES glslangValidator
    HINTS "${METAFLUX_VULKAN_SDK_DIR}/bin"
  )
  set(METAFLUX_VULKAN_PIPELINE_FIXTURE "${CMAKE_CURRENT_BINARY_DIR}/metaflux-pipeline-fixture.spv")
  if(METAFLUX_GLSLANG_VALIDATOR)
    add_custom_command(
      OUTPUT "${METAFLUX_VULKAN_PIPELINE_FIXTURE}"
      COMMAND
        "${METAFLUX_GLSLANG_VALIDATOR}" -V --target-env vulkan1.3
        -S comp -e main -o "${METAFLUX_VULKAN_PIPELINE_FIXTURE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/empty.comp"
      DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/empty.comp"
      VERBATIM
    )
    add_custom_target(metaflux_vulkan_pipeline_fixture
                      DEPENDS "${METAFLUX_VULKAN_PIPELINE_FIXTURE}")
  endif()
  add_executable(metaflux_vulkan_pipeline_test tests/vulkan_pipeline_test.cpp)
  target_link_libraries(metaflux_vulkan_pipeline_test PRIVATE MetaFlux::VulkanBackend)
  metaflux_configure_target(metaflux_vulkan_pipeline_test CXX)
  if(METAFLUX_GLSLANG_VALIDATOR)
    add_dependencies(metaflux_vulkan_pipeline_test metaflux_vulkan_pipeline_fixture)
    target_compile_definitions(
      metaflux_vulkan_pipeline_test PRIVATE
      METAFLUX_VULKAN_PIPELINE_FIXTURE="${METAFLUX_VULKAN_PIPELINE_FIXTURE}"
    )
  endif()
  add_test(NAME metaflux.backend.vulkan-pipeline COMMAND metaflux_vulkan_pipeline_test)
  set_tests_properties(
    metaflux.backend.vulkan-pipeline
    PROPERTIES LABELS "backend;vulkan;execution"
  )
endif()
