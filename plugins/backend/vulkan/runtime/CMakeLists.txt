find_package(Threads REQUIRED)

# The packaged backend is a shared library (packaging/build.py kind
# backend-vulkan); in-tree builds keep the static archive so tests and the
# daemon link it without loader setup.
if(METAFLUX_VULKAN_BACKEND_SHARED)
  set(METAFLUX_VULKAN_BACKEND_LIBRARY_TYPE SHARED)
else()
  set(METAFLUX_VULKAN_BACKEND_LIBRARY_TYPE STATIC)
endif()

add_library(
  metaflux_vulkan_backend ${METAFLUX_VULKAN_BACKEND_LIBRARY_TYPE}
  src/capability.cpp
  src/execution_timestamps.cpp
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
# The Nix clang wrapper can report the Vulkan SDK as an implicit include during
# configure even though the runtime shell does not expose it to later commands.
# Keep the discovered SDK explicit and public for source-local Vulkan headers in
# both the backend and its tests.
target_compile_options(metaflux_vulkan_backend PUBLIC
                       "SHELL:-isystem ${METAFLUX_VULKAN_INCLUDE_DIR}")
target_link_libraries(metaflux_vulkan_backend PRIVATE "${METAFLUX_VULKAN_LIBRARY}" Threads::Threads)
if(METAFLUX_VULKAN_BACKEND_SHARED)
  # The packaged shared backend stays within the glibc-2.31 generic artifact
  # policy: no private libstdc++/libgcc runtime dependency.
  target_link_options(metaflux_vulkan_backend PRIVATE -static-libstdc++ -static-libgcc)
endif()
metaflux_configure_target(metaflux_vulkan_backend CXX)

if(BUILD_TESTING AND METAFLUX_BUILD_TESTS)
  add_executable(metaflux_vulkan_capability_test tests/capability_test.cpp)
  target_link_libraries(metaflux_vulkan_capability_test PRIVATE MetaFlux::VulkanBackend)
  metaflux_configure_target(metaflux_vulkan_capability_test CXX)
  add_test(NAME metaflux.backend.vulkan-capability COMMAND metaflux_vulkan_capability_test)
  set_tests_properties(
    metaflux.backend.vulkan-capability
    PROPERTIES LABELS "backend;vulkan;capability"
               ENVIRONMENT "LD_LIBRARY_PATH=${METAFLUX_VULKAN_TEST_LD_LIBRARY_PATH}"
  )

  add_executable(metaflux_vulkan_memory_test tests/memory_test.cpp)
  target_link_libraries(metaflux_vulkan_memory_test PRIVATE MetaFlux::VulkanBackend)
  metaflux_configure_target(metaflux_vulkan_memory_test CXX)
  add_test(NAME metaflux.backend.vulkan-memory COMMAND metaflux_vulkan_memory_test)
  set_tests_properties(
    metaflux.backend.vulkan-memory
    PROPERTIES LABELS "backend;vulkan;memory"
               ENVIRONMENT "LD_LIBRARY_PATH=${METAFLUX_VULKAN_TEST_LD_LIBRARY_PATH}"
  )

  add_executable(metaflux_vulkan_target_test tests/target_test.cpp)
  target_link_libraries(metaflux_vulkan_target_test PRIVATE MetaFlux::VulkanBackend)
  metaflux_configure_target(metaflux_vulkan_target_test CXX)
  add_test(NAME metaflux.backend.vulkan-target-preflight COMMAND metaflux_vulkan_target_test)
  set_tests_properties(
    metaflux.backend.vulkan-target-preflight
    PROPERTIES LABELS "backend;vulkan;compiler"
               ENVIRONMENT "LD_LIBRARY_PATH=${METAFLUX_VULKAN_TEST_LD_LIBRARY_PATH}"
  )

  add_executable(metaflux_vulkan_streams_test tests/streams_test.cpp)
  target_link_libraries(metaflux_vulkan_streams_test PRIVATE MetaFlux::VulkanBackend)
  metaflux_configure_target(metaflux_vulkan_streams_test CXX)
  add_test(NAME metaflux.backend.vulkan-stream-graph COMMAND metaflux_vulkan_streams_test)
  set_tests_properties(
    metaflux.backend.vulkan-stream-graph
    PROPERTIES LABELS "backend;vulkan;execution"
               ENVIRONMENT "LD_LIBRARY_PATH=${METAFLUX_VULKAN_TEST_LD_LIBRARY_PATH}"
  )

  add_executable(
    metaflux_milestone_0_1_3_6_vulkan_stage_profile
    "${PROJECT_SOURCE_DIR}/tests/performance/milestone_0_1_3_6_vulkan_stage_profile.cpp"
  )
  target_include_directories(
    metaflux_milestone_0_1_3_6_vulkan_stage_profile
    PRIVATE "${PROJECT_SOURCE_DIR}/tests/performance"
  )
  target_link_libraries(
    metaflux_milestone_0_1_3_6_vulkan_stage_profile
    PRIVATE MetaFlux::VulkanBackend
  )
  metaflux_configure_target(metaflux_milestone_0_1_3_6_vulkan_stage_profile CXX)
  add_test(
    NAME metaflux.performance.milestone-0.1.3.6-vulkan-stage-smoke
    COMMAND metaflux_milestone_0_1_3_6_vulkan_stage_profile 8 32
  )
  set_tests_properties(
    metaflux.performance.milestone-0.1.3.6-vulkan-stage-smoke
    PROPERTIES LABELS "performance-smoke;backend;vulkan"
               ENVIRONMENT "LD_LIBRARY_PATH=${METAFLUX_VULKAN_TEST_LD_LIBRARY_PATH}"
  )
  find_package(Python3 REQUIRED COMPONENTS Interpreter)
  add_test(
    NAME metaflux.performance.milestone-0.1.3.6-vulkan-profile-selftest
    COMMAND
      "${Python3_EXECUTABLE}" -B
      "${PROJECT_SOURCE_DIR}/tests/performance/test_milestone_0_1_3_6_vulkan_profile.py"
  )
  set_tests_properties(
    metaflux.performance.milestone-0.1.3.6-vulkan-profile-selftest
    PROPERTIES
      LABELS "performance-smoke;backend;vulkan;release"
      TIMEOUT 180
      ENVIRONMENT
        "LD_LIBRARY_PATH=${METAFLUX_VULKAN_TEST_LD_LIBRARY_PATH};METAFLUX_VULKAN_STAGE_BENCHMARK=$<TARGET_FILE:metaflux_milestone_0_1_3_6_vulkan_stage_profile>"
  )

  add_executable(metaflux_vulkan_cache_test tests/cache_test.cpp)
  target_link_libraries(metaflux_vulkan_cache_test PRIVATE MetaFlux::VulkanBackend)
  metaflux_configure_target(metaflux_vulkan_cache_test CXX)
  add_test(NAME metaflux.backend.vulkan-cache-model COMMAND metaflux_vulkan_cache_test)
  set_tests_properties(
    metaflux.backend.vulkan-cache-model
    PROPERTIES LABELS "backend;vulkan;cache"
               ENVIRONMENT "LD_LIBRARY_PATH=${METAFLUX_VULKAN_TEST_LD_LIBRARY_PATH}"
  )

  add_executable(metaflux_vulkan_device_test tests/vulkan_device_test.cpp)
  target_link_libraries(metaflux_vulkan_device_test PRIVATE MetaFlux::VulkanBackend)
  metaflux_configure_target(metaflux_vulkan_device_test CXX)
  add_test(NAME metaflux.backend.vulkan-device COMMAND metaflux_vulkan_device_test)
  set_tests_properties(
    metaflux.backend.vulkan-device
    PROPERTIES LABELS "backend;vulkan;device"
               ENVIRONMENT "LD_LIBRARY_PATH=${METAFLUX_VULKAN_TEST_LD_LIBRARY_PATH}"
  )

  add_executable(metaflux_vulkan_queue_test tests/vulkan_queue_test.cpp)
  target_link_libraries(metaflux_vulkan_queue_test PRIVATE MetaFlux::VulkanBackend)
  metaflux_configure_target(metaflux_vulkan_queue_test CXX)
  add_test(NAME metaflux.backend.vulkan-queue-executor COMMAND metaflux_vulkan_queue_test)
  set_tests_properties(
    metaflux.backend.vulkan-queue-executor
    PROPERTIES LABELS "backend;vulkan;execution"
               ENVIRONMENT "LD_LIBRARY_PATH=${METAFLUX_VULKAN_TEST_LD_LIBRARY_PATH}"
  )

  find_program(
    METAFLUX_GLSLANG_VALIDATOR
    NAMES glslangValidator
    HINTS "${METAFLUX_VULKAN_SDK_DIR}/bin"
  )
  find_program(
    METAFLUX_SPIRV_VAL
    NAMES spirv-val
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
    if(METAFLUX_SPIRV_VAL)
      add_dependencies(metaflux_vulkan_backend metaflux_vulkan_pipeline_fixture)
      add_test(
        NAME metaflux.backend.vulkan-spirv-validation
        COMMAND "${METAFLUX_SPIRV_VAL}" --target-env vulkan1.3
                "${METAFLUX_VULKAN_PIPELINE_FIXTURE}"
      )
      set_tests_properties(
        metaflux.backend.vulkan-spirv-validation
        PROPERTIES LABELS "backend;vulkan;compiler"
                   ENVIRONMENT "LD_LIBRARY_PATH=${METAFLUX_VULKAN_TEST_LD_LIBRARY_PATH}"
      )
    endif()
  endif()
  if(METAFLUX_GLSLANG_VALIDATOR)
    set(METAFLUX_VULKAN_FMA_FIXTURE "${CMAKE_CURRENT_BINARY_DIR}/metaflux-fma.spv")
    add_custom_command(
      OUTPUT "${METAFLUX_VULKAN_FMA_FIXTURE}"
      COMMAND
        "${METAFLUX_GLSLANG_VALIDATOR}" -V --target-env vulkan1.3
        -S comp -e main -o "${METAFLUX_VULKAN_FMA_FIXTURE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/fma.comp"
      DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/fma.comp"
      VERBATIM
    )
    add_custom_target(metaflux_vulkan_fma_fixture
                      DEPENDS "${METAFLUX_VULKAN_FMA_FIXTURE}")
    add_executable(metaflux_vulkan_fma_profile tests/fma_profile.cpp)
    target_include_directories(
      metaflux_vulkan_fma_profile
      PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src"
              "${PROJECT_SOURCE_DIR}/tests/performance"
    )
    target_link_libraries(metaflux_vulkan_fma_profile PRIVATE MetaFlux::VulkanBackend)
    metaflux_configure_target(metaflux_vulkan_fma_profile CXX)
    add_dependencies(metaflux_vulkan_fma_profile metaflux_vulkan_fma_fixture)
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
               ENVIRONMENT "LD_LIBRARY_PATH=${METAFLUX_VULKAN_TEST_LD_LIBRARY_PATH}"
  )
endif()
