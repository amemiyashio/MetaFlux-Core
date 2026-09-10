# Included from tests/CMakeLists.txt; paths retain the tests/ directory scope.

if(TARGET metaflux_cpu_backend_runtime AND TARGET metaflux_cuda_ptx_frontend AND
   TARGET metaflux_compiler_core)
  add_executable(metaflux_backend_cpu_launch compatibility/backend_cpu_launch.cpp)
  target_link_libraries(
    metaflux_backend_cpu_launch
    PRIVATE MetaFlux::CpuBackendRuntime MetaFlux::CompilerCore MetaFlux::CudaPtxFrontend
  )
  target_compile_definitions(
    metaflux_backend_cpu_launch
    PRIVATE
      METAFLUX_CPU_ADD_PTX="${PROJECT_SOURCE_DIR}/plugins/compat/cuda/compiler/ptx/corpus/fixtures/positive-add-copy.ptx"
  )
  metaflux_configure_target(metaflux_backend_cpu_launch CXX)
  add_test(NAME metaflux.backend.cpu-launch COMMAND metaflux_backend_cpu_launch)
  set_tests_properties(metaflux.backend.cpu-launch PROPERTIES LABELS "backend;cpu;cuda;unit")
endif()

if(BUILD_TESTING AND METAFLUX_BUILD_TESTS)
  add_test(
    NAME metaflux.compatibility.pytorch-cuda-probe-selftest
    COMMAND
      "${Python3_EXECUTABLE}" -B
      "${CMAKE_CURRENT_SOURCE_DIR}/compatibility/test_pytorch_cuda_probe.py"
  )
  set_tests_properties(
    metaflux.compatibility.pytorch-cuda-probe-selftest
    PROPERTIES LABELS "compatibility;cuda;unit"
  )
  add_test(
    NAME metaflux.compatibility.pytorch-cuda-stock-baseline-selftest
    COMMAND
      "${Python3_EXECUTABLE}" -B
      "${CMAKE_CURRENT_SOURCE_DIR}/compatibility/test_pytorch_cuda_stock_baseline.py"
  )
  set_tests_properties(
    metaflux.compatibility.pytorch-cuda-stock-baseline-selftest
    PROPERTIES LABELS "compatibility;cuda;unit"
  )
  add_test(
    NAME metaflux.compatibility.pytorch-cuda-concat-selftest
    COMMAND
      "${Python3_EXECUTABLE}" -B
      "${CMAKE_CURRENT_SOURCE_DIR}/compatibility/test_pytorch_cuda_cat.py"
  )
  set_tests_properties(
    metaflux.compatibility.pytorch-cuda-concat-selftest
    PROPERTIES LABELS "compatibility;cuda;pytorch;unit"
  )
  add_test(
    NAME metaflux.compatibility.pytorch-cuda-cpu-frontier-selftest
    COMMAND
      "${Python3_EXECUTABLE}" -B
      "${CMAKE_CURRENT_SOURCE_DIR}/compatibility/test_pytorch_cuda_cpu_frontier.py"
  )
  set_tests_properties(
    metaflux.compatibility.pytorch-cuda-cpu-frontier-selftest
    PROPERTIES LABELS "compatibility;cuda;pytorch;cpu;unit"
  )
  find_program(METAFLUX_NIX_EXECUTABLE NAMES nix)
  if(METAFLUX_NIX_EXECUTABLE)
    add_test(
      NAME metaflux.compatibility.pytorch-cuda-stock-baseline
      COMMAND
        "${METAFLUX_NIX_EXECUTABLE}" develop ".#pytorch-baseline" --command
        python3 -B
        "${CMAKE_CURRENT_SOURCE_DIR}/compatibility/run_pytorch_cuda_stock_baseline.py"
        --daemon $<TARGET_FILE:metafluxd>
        --provider-dir $<TARGET_FILE_DIR:metaflux_cuda_provider>
    )
    set_tests_properties(
      metaflux.compatibility.pytorch-cuda-stock-baseline
      PROPERTIES
        LABELS "compatibility;integration;cuda;pytorch"
        TIMEOUT 120
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    )
    add_test(
      NAME metaflux.compatibility.pytorch-cuda-concat
      COMMAND
        "${METAFLUX_NIX_EXECUTABLE}" develop ".#pytorch-baseline" --command
        python3 -B
        "${CMAKE_CURRENT_SOURCE_DIR}/compatibility/run_pytorch_cuda_cat.py"
        --daemon $<TARGET_FILE:metafluxd>
        --provider-dir $<TARGET_FILE_DIR:metaflux_cuda_provider>
    )
    set_tests_properties(
      metaflux.compatibility.pytorch-cuda-concat
      PROPERTIES
        LABELS "compatibility;integration;cuda;pytorch;cpu"
        TIMEOUT 180
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    )
    if(TARGET metaflux_cublas_provider)
      add_test(
        NAME metaflux.compatibility.pytorch-cuda-cpu-frontier
        COMMAND
          "${METAFLUX_NIX_EXECUTABLE}" develop ".#pytorch-baseline" --command
          python3 -B
          "${CMAKE_CURRENT_SOURCE_DIR}/compatibility/run_pytorch_cuda_cpu_frontier.py"
          --daemon $<TARGET_FILE:metafluxd>
          --provider-dir $<TARGET_FILE_DIR:metaflux_cuda_provider>
          --cublas-provider $<TARGET_FILE:metaflux_cublas_provider>
      )
      set_tests_properties(
        metaflux.compatibility.pytorch-cuda-cpu-frontier
        PROPERTIES
          LABELS "compatibility;integration;cuda;pytorch;cpu"
          TIMEOUT 600
          WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
      )
      foreach(metaflux_pytorch_frontier_mode IN ITEMS cold-jit warm-jit aot)
        add_test(
          NAME
            "metaflux.compatibility.pytorch-cuda-cpu-frontier.compiled-subset.${metaflux_pytorch_frontier_mode}"
          COMMAND
            "${METAFLUX_NIX_EXECUTABLE}" develop ".#pytorch-baseline" --command
            python3 -B
            "${CMAKE_CURRENT_SOURCE_DIR}/compatibility/run_pytorch_cuda_cpu_frontier.py"
            --daemon $<TARGET_FILE:metafluxd>
            --provider-dir $<TARGET_FILE_DIR:metaflux_cuda_provider>
            --cublas-provider $<TARGET_FILE:metaflux_cublas_provider>
            --compiled-subset
            --execution-mode "${metaflux_pytorch_frontier_mode}"
        )
        set_tests_properties(
          "metaflux.compatibility.pytorch-cuda-cpu-frontier.compiled-subset.${metaflux_pytorch_frontier_mode}"
          PROPERTIES
            LABELS "compatibility;integration;cuda;pytorch;compiler;cpu"
            TIMEOUT 900
            WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
        )
      endforeach()
    endif()
    foreach(metaflux_pytorch_execution_mode IN ITEMS cold-jit warm-jit aot)
      add_test(
        NAME "metaflux.compatibility.pytorch-cuda-stock-baseline.${metaflux_pytorch_execution_mode}"
        COMMAND
          "${METAFLUX_NIX_EXECUTABLE}" develop ".#pytorch-baseline" --command
          python3 -B
          "${CMAKE_CURRENT_SOURCE_DIR}/compatibility/run_pytorch_cuda_stock_baseline.py"
          --daemon $<TARGET_FILE:metafluxd>
          --provider-dir $<TARGET_FILE_DIR:metaflux_cuda_provider>
          --execution-mode "${metaflux_pytorch_execution_mode}"
          --ptx
          "${CMAKE_CURRENT_SOURCE_DIR}/../plugins/compat/cuda/compiler/ptx/corpus/fixtures/positive-add-copy.ptx"
      )
      set_tests_properties(
        "metaflux.compatibility.pytorch-cuda-stock-baseline.${metaflux_pytorch_execution_mode}"
        PROPERTIES
          LABELS "compatibility;integration;cuda;pytorch;compiler;cpu"
          TIMEOUT 240
          WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
      )
    endforeach()
  endif()
  add_test(
    NAME metaflux.toolchain.pytorch-cuda-client-manifests
    COMMAND
      "${Python3_EXECUTABLE}" -B
      "${CMAKE_CURRENT_SOURCE_DIR}/../toolchains/tests/verify_pytorch_cuda_clients.py"
  )
  set_tests_properties(
    metaflux.toolchain.pytorch-cuda-client-manifests
    PROPERTIES LABELS "toolchain;cuda;unit"
  )
endif()

if(METAFLUX_BUILD_CUDA_DRIVER_PROVIDER AND METAFLUX_BUILD_NVML_PROVIDER)
  add_executable(metaflux_provider_load_smoke compatibility/provider_load_smoke.c)
  target_link_libraries(
    metaflux_provider_load_smoke
    PRIVATE "${CMAKE_DL_LIBS}"
  )
  metaflux_configure_target(metaflux_provider_load_smoke C)
  add_test(
    NAME metaflux.integration.provider.co-load
    COMMAND
      metaflux_provider_load_smoke
      "$<TARGET_FILE:metaflux_cuda_provider>"
      "$<TARGET_FILE:metaflux_nvml_provider>"
  )
  set_tests_properties(
    metaflux.integration.provider.co-load
    PROPERTIES LABELS "integration"
  )
endif()

if(NOT METAFLUX_NVIDIA_HEADER_DIR)
  message(
    WARNING
    "METAFLUX_NVIDIA_HEADER_DIR is unset; CUDA/NVML compatibility acceptance "
    "and managed benchmark rows are EXCLUDED from this build. The default "
    "'nix develop .#dev' shell declares it from the frozen provider-headers "
    "tool (families/R610); qualification rows must not silently disappear."
  )
endif()

if(
  TARGET metaflux_cuda_provider
  AND TARGET metafluxd
  AND METAFLUX_NVIDIA_HEADER_DIR
)
  if(NOT EXISTS "${METAFLUX_NVIDIA_HEADER_DIR}/cuda.h")
    message(FATAL_ERROR "METAFLUX_NVIDIA_HEADER_DIR does not contain cuda.h")
  endif()
  add_executable(metaflux_cuda_add_copy_acceptance compatibility/cuda_add_copy.c)
  target_include_directories(
    metaflux_cuda_add_copy_acceptance
    SYSTEM PRIVATE "${METAFLUX_NVIDIA_HEADER_DIR}"
  )
  target_link_libraries(metaflux_cuda_add_copy_acceptance PRIVATE metaflux_cuda_provider)
  metaflux_configure_target(metaflux_cuda_add_copy_acceptance C)
  add_executable(metaflux_cuda_process_holder compatibility/cuda_process_holder.c)
  target_include_directories(
    metaflux_cuda_process_holder
    SYSTEM PRIVATE "${METAFLUX_NVIDIA_HEADER_DIR}"
  )
  target_link_libraries(metaflux_cuda_process_holder PRIVATE metaflux_cuda_provider)
  set_target_properties(
    metaflux_cuda_process_holder
    PROPERTIES OUTPUT_NAME metaflux-cuda-process-holder
  )
  metaflux_configure_target(metaflux_cuda_process_holder C)
  add_test(
    NAME metaflux.integration.cuda-add-copy-managed
    COMMAND
      "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/compatibility/run_cuda_acceptance.py"
      --daemon $<TARGET_FILE:metafluxd>
      --application $<TARGET_FILE:metaflux_cuda_add_copy_acceptance>
      --provider-dir $<TARGET_FILE_DIR:metaflux_cuda_provider>
      --execution-mode interpreter
      --ptx
      "${CMAKE_CURRENT_SOURCE_DIR}/../plugins/compat/cuda/compiler/ptx/corpus/fixtures/positive-add-copy.ptx"
  )
  set_tests_properties(
    metaflux.integration.cuda-add-copy-managed
    PROPERTIES LABELS "compatibility;integration;cuda" TIMEOUT 60
  )

  foreach(metaflux_execution_mode IN ITEMS cold-jit warm-jit aot)
    add_test(
      NAME "metaflux.integration.cuda-add-copy-managed.${metaflux_execution_mode}"
      COMMAND
        "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/compatibility/run_cuda_acceptance.py"
        --daemon $<TARGET_FILE:metafluxd>
        --application $<TARGET_FILE:metaflux_cuda_add_copy_acceptance>
        --provider-dir $<TARGET_FILE_DIR:metaflux_cuda_provider>
        --execution-mode "${metaflux_execution_mode}"
        --ptx
        "${CMAKE_CURRENT_SOURCE_DIR}/../plugins/compat/cuda/compiler/ptx/corpus/fixtures/positive-add-copy.ptx"
    )
    set_tests_properties(
      "metaflux.integration.cuda-add-copy-managed.${metaflux_execution_mode}"
      PROPERTIES LABELS "compatibility;integration;cuda;compiler;cpu" TIMEOUT 240
    )
  endforeach()

  add_executable(metaflux_cuda_reduction_acceptance compatibility/cuda_reduction.c)
  target_include_directories(
    metaflux_cuda_reduction_acceptance
    SYSTEM PRIVATE "${METAFLUX_NVIDIA_HEADER_DIR}"
  )
  target_link_libraries(metaflux_cuda_reduction_acceptance PRIVATE metaflux_cuda_provider)
  metaflux_configure_target(metaflux_cuda_reduction_acceptance C)
  add_test(
    NAME metaflux.integration.cuda-reduction-managed
    COMMAND
      "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/compatibility/run_cuda_acceptance.py"
      --daemon $<TARGET_FILE:metafluxd>
      --application $<TARGET_FILE:metaflux_cuda_reduction_acceptance>
      --provider-dir $<TARGET_FILE_DIR:metaflux_cuda_provider>
      --execution-mode interpreter
      --ptx
      "${CMAKE_CURRENT_SOURCE_DIR}/compatibility/shared-reduction.ptx"
  )
  set_tests_properties(
    metaflux.integration.cuda-reduction-managed
    PROPERTIES LABELS "compatibility;integration;cuda" TIMEOUT 60
  )

  foreach(metaflux_execution_mode IN ITEMS cold-jit warm-jit aot)
    add_test(
      NAME "metaflux.integration.cuda-reduction-managed.${metaflux_execution_mode}"
      COMMAND
        "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/compatibility/run_cuda_acceptance.py"
        --daemon $<TARGET_FILE:metafluxd>
        --application $<TARGET_FILE:metaflux_cuda_reduction_acceptance>
        --provider-dir $<TARGET_FILE_DIR:metaflux_cuda_provider>
        --execution-mode "${metaflux_execution_mode}"
        --ptx
        "${CMAKE_CURRENT_SOURCE_DIR}/compatibility/shared-reduction.ptx"
    )
    set_tests_properties(
      "metaflux.integration.cuda-reduction-managed.${metaflux_execution_mode}"
      PROPERTIES LABELS "compatibility;integration;cuda;compiler;cpu" TIMEOUT 240
    )
  endforeach()
endif()

if(METAFLUX_BUILD_DAEMON)
  set(
    metaflux_daemon_version_check
    "${CMAKE_CURRENT_BINARY_DIR}/check-daemon-version.cmake"
  )
  file(
    GENERATE
    OUTPUT "${metaflux_daemon_version_check}"
    CONTENT [=[
if(NOT DEFINED DAEMON OR NOT DEFINED EXPECTED_VERSION)
  message(FATAL_ERROR "DAEMON and EXPECTED_VERSION are required")
endif()
execute_process(
  COMMAND "${DAEMON}" --version
  RESULT_VARIABLE daemon_result
  OUTPUT_VARIABLE daemon_stdout
  ERROR_VARIABLE daemon_stderr
)
if(NOT daemon_result EQUAL 0)
  message(FATAL_ERROR "metafluxd --version exited with ${daemon_result}: ${daemon_stderr}")
endif()
set(expected_stdout "metafluxd ${EXPECTED_VERSION}\n")
if(NOT daemon_stdout STREQUAL expected_stdout)
  string(REPLACE "\n" "\\n" rendered_stdout "${daemon_stdout}")
  message(
    FATAL_ERROR
      "metafluxd --version produced '${rendered_stdout}', expected 'metafluxd ${EXPECTED_VERSION}\\n'"
  )
endif()
if(NOT daemon_stderr STREQUAL "")
  message(FATAL_ERROR "metafluxd --version wrote to stderr: ${daemon_stderr}")
endif()
]=]
  )
  add_test(
    NAME metaflux.integration.daemon-version
    COMMAND
      "${CMAKE_COMMAND}"
      "-DDAEMON=$<TARGET_FILE:metafluxd>"
      "-DEXPECTED_VERSION=${PROJECT_VERSION}"
      -P "${metaflux_daemon_version_check}"
  )
  set_tests_properties(metaflux.integration.daemon-version PROPERTIES LABELS "integration")
endif()
