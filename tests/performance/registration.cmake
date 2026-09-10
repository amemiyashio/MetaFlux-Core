# Included from tests/CMakeLists.txt; paths retain the tests/ directory scope.

if(METAFLUX_BUILD_CLIENT_FASTPATH)
  add_executable(metaflux_fastpath_performance_smoke performance/fastpath_smoke.c)
  target_link_libraries(metaflux_fastpath_performance_smoke PRIVATE MetaFlux::ClientFastpath)
  metaflux_configure_target(metaflux_fastpath_performance_smoke C)
  add_test(NAME metaflux.performance.fastpath-smoke COMMAND metaflux_fastpath_performance_smoke)
  set_tests_properties(
    metaflux.performance.fastpath-smoke
    PROPERTIES LABELS "performance-smoke"
  )

  add_executable(
    metaflux_milestone_0_1_0_0_ring_benchmark
    performance/milestone_0_1_0_0_ring_audit.c
    performance/milestone_0_1_0_0_ring_benchmark.c
  )
  target_link_libraries(
    metaflux_milestone_0_1_0_0_ring_benchmark
    PRIVATE MetaFlux::ClientFastpath
  )
  target_link_options(
    metaflux_milestone_0_1_0_0_ring_benchmark
    PRIVATE
      "LINKER:--wrap=malloc"
      "LINKER:--wrap=calloc"
      "LINKER:--wrap=realloc"
      "LINKER:--wrap=aligned_alloc"
      "LINKER:--wrap=posix_memalign"
      "LINKER:--wrap=pthread_mutex_lock"
      "LINKER:--wrap=pthread_mutex_trylock"
      "LINKER:--wrap=pthread_rwlock_rdlock"
      "LINKER:--wrap=pthread_rwlock_tryrdlock"
      "LINKER:--wrap=pthread_rwlock_wrlock"
      "LINKER:--wrap=pthread_rwlock_trywrlock"
      "LINKER:--wrap=pthread_spin_lock"
      "LINKER:--wrap=pthread_spin_trylock"
  )
  metaflux_configure_target(metaflux_milestone_0_1_0_0_ring_benchmark C)
  add_test(
    NAME metaflux.performance.milestone-0.1.0.0-ring-smoke
    COMMAND metaflux_milestone_0_1_0_0_ring_benchmark 8 32
  )
  set_tests_properties(
    metaflux.performance.milestone-0.1.0.0-ring-smoke
    PROPERTIES LABELS "performance-smoke;runtime"
  )
endif()

if(TARGET metaflux_cpu_backend_runtime AND TARGET metaflux_cuda_ptx_frontend AND
   TARGET metaflux_compiler_core AND TARGET metafluxd)
  add_executable(
    metaflux_cpu_fma_throughput
    performance/cpu_fma_throughput.cpp
    ../services/metafluxd/compiler_worker_client.cpp
    ../services/metafluxd/compiler_worker_protocol.cpp
    ../services/metafluxd/execution.cpp
  )
  target_include_directories(
    metaflux_cpu_fma_throughput
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../services/metafluxd"
  )
  target_compile_definitions(
    metaflux_cpu_fma_throughput
    PRIVATE METAFLUX_DAEMON_EXECUTABLE="$<TARGET_FILE:metafluxd>"
  )
  target_link_libraries(
    metaflux_cpu_fma_throughput
    PRIVATE
      MetaFlux::CompilerCore
      MetaFlux::CudaPtxFrontend
      MetaFlux::CpuBackendCompiler
      MetaFlux::CpuBackendRuntime
  )
  metaflux_configure_target(metaflux_cpu_fma_throughput CXX)
  add_test(NAME metaflux.performance.cpu-fma-throughput COMMAND metaflux_cpu_fma_throughput)
  set_tests_properties(
    metaflux.performance.cpu-fma-throughput
    PROPERTIES LABELS "performance-smoke;runtime;daemon;compiler;cpu" TIMEOUT 600
  )
endif()

if(TARGET metaflux_cpu_backend_runtime)
  # Steady-state executor audit: the per-launch heap cost of the daemon-side
  # CTA launch path must be a constant (two equal windows allocate equally);
  # the remaining per-launch allocations are the kernel-boundary topology
  # revalidation required by the placement contract and stay visible in the
  # output for the next zero-overhead iteration.
  add_executable(
    metaflux_cpu_executor_audit
    performance/cpu_executor_audit.cpp
    performance/cpu_executor_audit_wrappers.c
  )
  target_include_directories(
    metaflux_cpu_executor_audit
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/performance"
  )
  target_link_libraries(metaflux_cpu_executor_audit PRIVATE MetaFlux::CpuBackendRuntime)
  target_link_options(
    metaflux_cpu_executor_audit
    PRIVATE
      "LINKER:--wrap=malloc"
      "LINKER:--wrap=calloc"
      "LINKER:--wrap=realloc"
      "LINKER:--wrap=aligned_alloc"
      "LINKER:--wrap=posix_memalign"
      "LINKER:--wrap=_Znwm"
      "LINKER:--wrap=_Znam"
      "LINKER:--wrap=_ZdlPv"
      "LINKER:--wrap=_ZdaPv"
      "LINKER:--wrap=_ZdlPvm"
      "LINKER:--wrap=_ZdaPvm"
      "LINKER:--wrap=_ZnwmSt11align_val_t"
      "LINKER:--wrap=_ZnamSt11align_val_t"
  )
  metaflux_configure_target(metaflux_cpu_executor_audit CXX)
  add_test(NAME metaflux.performance.executor-audit COMMAND metaflux_cpu_executor_audit)
  set_tests_properties(
    metaflux.performance.executor-audit
    PROPERTIES LABELS "performance-smoke;runtime;cpu" TIMEOUT 120
  )
endif()

if(BUILD_TESTING AND METAFLUX_BUILD_TESTS)
  add_test(
    NAME metaflux.performance.measurement-archive
    COMMAND
      "${Python3_EXECUTABLE}" -B
      "${CMAKE_CURRENT_SOURCE_DIR}/../tools/archive-transport-measurement.py"
      --lifecycle-core
      --output
      "${CMAKE_BINARY_DIR}/metaflux-transport-measurement-archive.json"
  )
  set_tests_properties(
    metaflux.performance.measurement-archive
    PROPERTIES LABELS "performance-smoke;release;unit"
  )
  add_test(
    NAME metaflux.performance.measurement-archive-selftest
    COMMAND
      "${Python3_EXECUTABLE}" -B
      "${CMAKE_CURRENT_SOURCE_DIR}/performance/test-archive-transport-measurement.py"
  )
  set_tests_properties(
    metaflux.performance.measurement-archive-selftest
    PROPERTIES LABELS "performance-smoke;release;unit"
  )
  if(TARGET metaflux_milestone_0_1_0_0_ring_benchmark)
    add_test(
      NAME metaflux.performance.milestone-0.1.1.5-transport-profile-selftest
      COMMAND
        "${CMAKE_COMMAND}" -E env
        "METAFLUX_RING_BENCHMARK=$<TARGET_FILE:metaflux_milestone_0_1_0_0_ring_benchmark>"
        "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/performance/test_milestone_0_1_1_0_transport_profile.py"
    )
    set_tests_properties(
      metaflux.performance.milestone-0.1.1.5-transport-profile-selftest
      PROPERTIES LABELS "performance-smoke;release;unit" TIMEOUT 180
    )
  endif()
  add_test(
    NAME metaflux.performance.live-cdev-archive-selftest
    COMMAND
      "${Python3_EXECUTABLE}" -B
      "${CMAKE_CURRENT_SOURCE_DIR}/performance/test_archive_live_cdev_profile.py"
  )
  set_tests_properties(
    metaflux.performance.live-cdev-archive-selftest
    PROPERTIES LABELS "performance-smoke;release;unit"
  )
  add_test(
    NAME metaflux.performance.optimization-runner-selftest
    COMMAND
      "${Python3_EXECUTABLE}" -B
      "${CMAKE_CURRENT_SOURCE_DIR}/performance/test_milestone_0_1_0_0_optimization.py"
  )
  set_tests_properties(
    metaflux.performance.optimization-runner-selftest
    PROPERTIES LABELS "performance-smoke;unit"
  )
  add_test(
    NAME metaflux.performance.measurement-runner-selftest
    COMMAND
      "${Python3_EXECUTABLE}" -B
      "${CMAKE_CURRENT_SOURCE_DIR}/performance/test_milestone_0_1_0_0_performance.py"
  )
  set_tests_properties(
    metaflux.performance.measurement-runner-selftest
    PROPERTIES LABELS "performance-smoke;unit"
  )
endif()

if(
  TARGET metaflux_milestone_0_1_0_0_ring_benchmark
  AND TARGET metaflux_cuda_provider
  AND TARGET metaflux_nvml_provider
  AND TARGET metafluxd
  AND METAFLUX_NVIDIA_HEADER_DIR
)
  if(NOT EXISTS "${METAFLUX_NVIDIA_HEADER_DIR}/nvml.h")
    message(FATAL_ERROR "METAFLUX_NVIDIA_HEADER_DIR does not contain nvml.h")
  endif()
  add_executable(
    metaflux_milestone_0_1_0_0_cuda_managed_benchmark
    performance/milestone_0_1_0_0_cuda_managed_benchmark.c
  )
  target_include_directories(
    metaflux_milestone_0_1_0_0_cuda_managed_benchmark
    SYSTEM PRIVATE "${METAFLUX_NVIDIA_HEADER_DIR}"
  )
  target_link_libraries(
    metaflux_milestone_0_1_0_0_cuda_managed_benchmark
    PRIVATE metaflux_cuda_provider
  )
  metaflux_configure_target(metaflux_milestone_0_1_0_0_cuda_managed_benchmark C)

  add_executable(
    metaflux_milestone_0_1_0_0_nvml_benchmark
    performance/milestone_0_1_0_0_nvml_benchmark.c
  )
  target_include_directories(
    metaflux_milestone_0_1_0_0_nvml_benchmark
    SYSTEM PRIVATE "${METAFLUX_NVIDIA_HEADER_DIR}"
  )
  target_link_libraries(
    metaflux_milestone_0_1_0_0_nvml_benchmark
    PRIVATE metaflux_nvml_provider
  )
  metaflux_configure_target(metaflux_milestone_0_1_0_0_nvml_benchmark C)

  add_test(
    NAME metaflux.performance.milestone-0.1.0.0-managed-smoke
    COMMAND
      "${Python3_EXECUTABLE}" -B
      "${CMAKE_CURRENT_SOURCE_DIR}/performance/run_milestone_0_1_0_0_performance.py"
      --mode smoke
      --budget-status provisional
      --output-dir "${CMAKE_CURRENT_BINARY_DIR}/milestone-0.1.0.0-performance-smoke-evidence"
      --repository "${PROJECT_SOURCE_DIR}"
      --milestone-plan
      "${PROJECT_SOURCE_DIR}/agent/plan/milestone-0.1.0.0-core-foundation/plan.md"
      --ring-benchmark $<TARGET_FILE:metaflux_milestone_0_1_0_0_ring_benchmark>
      --cuda-benchmark $<TARGET_FILE:metaflux_milestone_0_1_0_0_cuda_managed_benchmark>
      --nvml-benchmark $<TARGET_FILE:metaflux_milestone_0_1_0_0_nvml_benchmark>
      --daemon $<TARGET_FILE:metafluxd>
      --provider-dir $<TARGET_FILE_DIR:metaflux_cuda_provider>
      --cuda-provider $<TARGET_FILE:metaflux_cuda_provider>
      --nvml-provider $<TARGET_FILE:metaflux_nvml_provider>
      --compiler-epoch "${METAFLUX_COMPILER_EPOCH_FILE}"
      --build-manifest "${CMAKE_BINARY_DIR}/metaflux-build-manifest.json"
      --execution-mode interpreter
      --warmup 4
      --samples 16
      --copy-bytes 16777216
      --require-all-core-metrics
  )
  set_tests_properties(
    metaflux.performance.milestone-0.1.0.0-managed-smoke
    PROPERTIES LABELS "performance-smoke;integration;cuda;nvml;runtime" TIMEOUT 180
  )
endif()
