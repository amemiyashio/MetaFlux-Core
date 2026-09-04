add_executable(metaflux_abi_c_smoke contract/abi_c_smoke.c)
target_link_libraries(metaflux_abi_c_smoke PRIVATE MetaFlux::BackendPluginApi)
metaflux_configure_target(metaflux_abi_c_smoke C)
add_test(NAME metaflux.abi.c COMMAND metaflux_abi_c_smoke)
set_tests_properties(metaflux.abi.c PROPERTIES LABELS "abi")

if(CMAKE_CXX_COMPILER_LOADED)
  add_executable(metaflux_abi_cpp_smoke contract/abi_cpp_smoke.cpp)
  target_link_libraries(metaflux_abi_cpp_smoke PRIVATE MetaFlux::BackendPluginApi)
  metaflux_configure_target(metaflux_abi_cpp_smoke CXX)
  add_test(NAME metaflux.abi.cpp COMMAND metaflux_abi_cpp_smoke)
  set_tests_properties(metaflux.abi.cpp PROPERTIES LABELS "abi")
endif()

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

if(TARGET metaflux_cpu_backend_runtime)
  add_executable(metaflux_backend_abi_smoke unit/backend_abi_smoke.cpp)
  target_link_libraries(metaflux_backend_abi_smoke PRIVATE MetaFlux::CpuBackendRuntime)
  metaflux_configure_target(metaflux_backend_abi_smoke CXX)
  add_test(NAME metaflux.abi.backend-cpu COMMAND metaflux_backend_abi_smoke)
  set_tests_properties(metaflux.abi.backend-cpu PROPERTIES LABELS "abi;unit")
endif()

if(TARGET metaflux_cpu_backend_runtime AND TARGET metaflux_cuda_ptx_frontend AND
   TARGET metaflux_compiler_core)
  add_executable(metaflux_backend_cpu_launch unit/backend_cpu_launch.cpp)
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

if(TARGET metaflux_transport_cdev_client AND TARGET metafluxd)
  find_package(ZLIB REQUIRED)
  add_executable(
    metaflux_transport_cdev_live_qualification
    ../kernel/tests/kselftest/cdev_qualification.c
  )
  target_link_libraries(
    metaflux_transport_cdev_live_qualification
    PRIVATE MetaFlux::TransportCdevClient MetaFlux::ClientFastpath
  )
  get_filename_component(_metaflux_live_zlib_dir "${ZLIB_LIBRARY}" DIRECTORY)
  target_compile_definitions(
    metaflux_transport_cdev_live_qualification
    PRIVATE
      METAFLUX_DAEMON_EXECUTABLE="$<TARGET_FILE:metafluxd>"
      METAFLUX_DAEMON_LIBRARY_PATH="${_metaflux_live_zlib_dir}"
  )
  metaflux_configure_target(metaflux_transport_cdev_live_qualification C)
  add_test(
    NAME metaflux.transport.cdev-live-qualification
    COMMAND metaflux_transport_cdev_live_qualification
  )
  set_tests_properties(
    metaflux.transport.cdev-live-qualification
    PROPERTIES
      LABELS "transport;qualification;live"
      SKIP_RETURN_CODE 77
  )
endif()

if(TARGET metaflux_cuda_ptx_frontend)
  add_executable(metaflux_ptx_frontend_smoke unit/ptx_frontend_smoke.cpp)
  target_link_libraries(metaflux_ptx_frontend_smoke PRIVATE MetaFlux::CudaPtxFrontend)
  metaflux_configure_target(metaflux_ptx_frontend_smoke CXX)
  add_test(NAME metaflux.unit.ptx-frontend COMMAND metaflux_ptx_frontend_smoke)
  set_tests_properties(metaflux.unit.ptx-frontend PROPERTIES LABELS "unit")
endif()

if(BUILD_TESTING AND METAFLUX_BUILD_TESTS)
  find_package(Python3 REQUIRED COMPONENTS Interpreter)
  if(NOT EXISTS "${CMAKE_BINARY_DIR}/metaflux-component-graph.json")
    message(FATAL_ERROR "MetaFlux component graph is missing from the build tree")
  endif()
  add_test(
    NAME metaflux.architecture.component-graph
    COMMAND
      "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/../tools/check-component-graph.py"
      "${CMAKE_BINARY_DIR}/metaflux-component-graph.json"
  )
  set_tests_properties(
    metaflux.architecture.component-graph
    PROPERTIES LABELS "architecture"
  )
  add_test(
    NAME metaflux.lifecycle.model-check
    COMMAND
      "${Python3_EXECUTABLE}"
      "${CMAKE_CURRENT_SOURCE_DIR}/../tools/check-lifecycle-model.py"
      --base-manifest
      "${CMAKE_CURRENT_SOURCE_DIR}/../contracts/protocol/transport/v1/schema/manifest.json"
      --manifest
      "${CMAKE_CURRENT_SOURCE_DIR}/../contracts/protocol/transport/v1/schema/extensions/lifecycle/v1/manifest.json"
      --model
      "${CMAKE_CURRENT_SOURCE_DIR}/../contracts/protocol/transport/v1/schema/extensions/lifecycle/v1/model.json"
      --bounds
      "${CMAKE_CURRENT_SOURCE_DIR}/lifecycle/model-bounds.json"
      --output
      "${CMAKE_BINARY_DIR}/metaflux-lifecycle-model-check.json"
  )
  set_tests_properties(
    metaflux.lifecycle.model-check
    PROPERTIES LABELS "lifecycle;model;architecture"
  )
  add_test(
    NAME metaflux.lifecycle.model-check-selftest
    COMMAND
      "${Python3_EXECUTABLE}" -B
      "${CMAKE_CURRENT_SOURCE_DIR}/lifecycle/test_model_checker.py"
  )
  set_tests_properties(
    metaflux.lifecycle.model-check-selftest
    PROPERTIES LABELS "lifecycle;model;unit"
  )
  add_test(
    NAME metaflux.lifecycle.admin-freeze
    COMMAND
      "${Python3_EXECUTABLE}" -B
      "${CMAKE_CURRENT_SOURCE_DIR}/../tools/validate-lifecycle-admin.py"
      --root "${PROJECT_SOURCE_DIR}"
      --generate-header
      "${CMAKE_BINARY_DIR}/contracts/generated/include/metaflux/lifecycle/admin_generated.h"
  )
  set_tests_properties(
    metaflux.lifecycle.admin-freeze
    PROPERTIES LABELS "lifecycle;schema;architecture"
  )
  add_test(
    NAME metaflux.lifecycle.admin-freeze-selftest
    COMMAND
      "${Python3_EXECUTABLE}" -B
      "${CMAKE_CURRENT_SOURCE_DIR}/../tools/test-lifecycle-admin.py"
  )
  set_tests_properties(
    metaflux.lifecycle.admin-freeze-selftest
    PROPERTIES LABELS "lifecycle;schema;unit"
  )
  add_test(
    NAME metaflux.lifecycle.admin-fixtures
    COMMAND
      "${Python3_EXECUTABLE}" -B
      "${CMAKE_CURRENT_SOURCE_DIR}/../tools/check-lifecycle-model.py"
      --base-manifest
      "${CMAKE_CURRENT_SOURCE_DIR}/../contracts/protocol/transport/v1/schema/manifest.json"
      --manifest
      "${CMAKE_CURRENT_SOURCE_DIR}/../contracts/protocol/transport/v1/schema/extensions/lifecycle/v1/manifest.json"
      --model
      "${CMAKE_CURRENT_SOURCE_DIR}/../contracts/protocol/transport/v1/schema/extensions/lifecycle/v1/model.json"
      --bounds
      "${CMAKE_CURRENT_SOURCE_DIR}/lifecycle/model-bounds.json"
      --output
      "${CMAKE_BINARY_DIR}/metaflux-lifecycle-admin-model-check.json"
      --generate-fixtures
      "${CMAKE_BINARY_DIR}/metaflux-lifecycle-admin-fixtures.json"
  )
  set_tests_properties(
    metaflux.lifecycle.admin-fixtures
    PROPERTIES LABELS "lifecycle;model;architecture"
  )
  add_test(
    NAME metaflux.packaging.vroot-dkms
    COMMAND
      "${Python3_EXECUTABLE}" -B
      "${CMAKE_CURRENT_SOURCE_DIR}/../tools/test-vroot-dkms-packaging.py"
  )
  set_tests_properties(
    metaflux.packaging.vroot-dkms
    PROPERTIES LABELS "packaging;vroot;unit"
  )
  add_test(
    NAME metaflux.packaging.vpci-dkms
    COMMAND
      "${Python3_EXECUTABLE}" -B
      "${CMAKE_CURRENT_SOURCE_DIR}/../tools/test-vpci-dkms-packaging.py"
  )
  set_tests_properties(
    metaflux.packaging.vpci-dkms
    PROPERTIES LABELS "packaging;pci;unit"
  )
  add_test(
    NAME metaflux.release.baremetal-vpci
    COMMAND
      "${Python3_EXECUTABLE}" -B
      "${CMAKE_CURRENT_SOURCE_DIR}/release/run_baremetal_vpci_gate.py"
      --output
      "${CMAKE_BINARY_DIR}/metaflux-baremetal-vpci-gate.json"
  )
  set_tests_properties(
    metaflux.release.baremetal-vpci
    PROPERTIES LABELS "packaging;vroot;release;qualification"
  )
  add_test(
    NAME metaflux.release.baremetal-vpci-selftest
    COMMAND
      "${Python3_EXECUTABLE}" -B
      "${CMAKE_CURRENT_SOURCE_DIR}/release/test_baremetal_vpci_gate.py"
  )
  set_tests_properties(
    metaflux.release.baremetal-vpci-selftest
    PROPERTIES LABELS "packaging;vroot;release;unit"
  )
  add_test(
    NAME metaflux.release.lifecycle-core-packaging
    COMMAND
      "${Python3_EXECUTABLE}" -B
      "${CMAKE_CURRENT_SOURCE_DIR}/release/run_lifecycle_core_packaging_gate.py"
      --output
      "${CMAKE_BINARY_DIR}/metaflux-lifecycle-core-packaging-gate.json"
  )
  set_tests_properties(
    metaflux.release.lifecycle-core-packaging
    PROPERTIES LABELS "packaging;lifecycle;release;qualification"
  )
  add_test(
    NAME metaflux.release.lifecycle-core-packaging-selftest
    COMMAND
      "${Python3_EXECUTABLE}" -B
      "${CMAKE_CURRENT_SOURCE_DIR}/release/test_lifecycle_core_packaging_gate.py"
  )
  set_tests_properties(
    metaflux.release.lifecycle-core-packaging-selftest
    PROPERTIES LABELS "packaging;lifecycle;release;unit"
  )
  add_test(
    NAME metaflux.packaging.backend-vulkan
    COMMAND
      "${Python3_EXECUTABLE}" -B
      "${CMAKE_CURRENT_SOURCE_DIR}/../tools/validate-backend-vulkan-packaging.py"
      --output
      "${CMAKE_BINARY_DIR}/metaflux-backend-vulkan-packaging.json"
  )
  set_tests_properties(
    metaflux.packaging.backend-vulkan
    PROPERTIES LABELS "packaging;vulkan;unit"
  )
  add_test(
    NAME metaflux.contract.vulkan-argument-freeze
    COMMAND
      "${Python3_EXECUTABLE}" -B
      "${CMAKE_CURRENT_SOURCE_DIR}/../tools/validate-vulkan-argument-freeze.py"
      --output
      "${CMAKE_BINARY_DIR}/metaflux-vulkan-argument-freeze.json"
  )
  set_tests_properties(
    metaflux.contract.vulkan-argument-freeze
    PROPERTIES LABELS "contract;vulkan;schema;architecture"
  )
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
      "${CMAKE_CURRENT_SOURCE_DIR}/../tools/test-archive-transport-measurement.py"
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
    NAME metaflux.architecture.agent-state-selftest
    COMMAND
      "${Python3_EXECUTABLE}" -B
      "${CMAKE_CURRENT_SOURCE_DIR}/../tools/test-check-agent-state.py"
  )
  set_tests_properties(
    metaflux.architecture.agent-state-selftest
    PROPERTIES LABELS "architecture"
  )
  add_test(
    NAME metaflux.architecture.agent-diagnostics-selftest
    COMMAND
      "${Python3_EXECUTABLE}" -B
      "${CMAKE_CURRENT_SOURCE_DIR}/../tools/test-agent-diagnostics.py"
  )
  set_tests_properties(
    metaflux.architecture.agent-diagnostics-selftest
    PROPERTIES LABELS "architecture"
  )
  add_test(
    NAME metaflux.architecture.agent-tool-detection-selftest
    COMMAND
      "${Python3_EXECUTABLE}" -B
      "${CMAKE_CURRENT_SOURCE_DIR}/../agent/skills/detect-agent-tool/scripts/test_detect_agent_tool.py"
  )
  set_tests_properties(
    metaflux.architecture.agent-tool-detection-selftest
    PROPERTIES LABELS "architecture"
  )
  add_test(
    NAME metaflux.architecture.agent-commit-identity-selftest
    COMMAND
      "${Python3_EXECUTABLE}" -B
      "${CMAKE_CURRENT_SOURCE_DIR}/../agent/skills/start-work/scripts/test_commit_as_agent_tool.py"
  )
  set_tests_properties(
    metaflux.architecture.agent-commit-identity-selftest
    PROPERTIES LABELS "architecture"
  )
  add_test(
    NAME metaflux.architecture.repository-push-selftest
    COMMAND
      "${Python3_EXECUTABLE}" -B
      "${CMAKE_CURRENT_SOURCE_DIR}/../agent/skills/push-repository/scripts/test_push_repository.py"
  )
  set_tests_properties(
    metaflux.architecture.repository-push-selftest
    PROPERTIES LABELS "architecture"
  )
  add_test(
    NAME metaflux.architecture.skill-routing
    COMMAND
      "${Python3_EXECUTABLE}" -B
      "${CMAKE_CURRENT_SOURCE_DIR}/../tools/check-skill-routing.py"
      "${CMAKE_CURRENT_SOURCE_DIR}/.."
  )
  set_tests_properties(
    metaflux.architecture.skill-routing
    PROPERTIES LABELS "architecture"
  )
  add_test(
    NAME metaflux.architecture.skill-routing-selftest
    COMMAND
      "${Python3_EXECUTABLE}" -B
      "${CMAKE_CURRENT_SOURCE_DIR}/../tools/test-check-skill-routing.py"
  )
  set_tests_properties(
    metaflux.architecture.skill-routing-selftest
    PROPERTIES LABELS "architecture"
  )
  add_test(
    NAME metaflux.release.matrix-assertions
    COMMAND
      "${Python3_EXECUTABLE}" -B
      "${CMAKE_CURRENT_SOURCE_DIR}/release/test_release_matrix_assertions.py"
  )
  set_tests_properties(
    metaflux.release.matrix-assertions
    PROPERTIES LABELS "release;unit"
  )
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

if(METAFLUX_BUILD_CUDA_DRIVER_PROVIDER OR METAFLUX_BUILD_NVML_PROVIDER)
  find_program(METAFLUX_READELF_EXECUTABLE NAMES llvm-readelf readelf REQUIRED)

  function(
    metaflux_add_provider_abi_gate
    provider_name
    provider_target
    provider_header
    expected_symbol
    expected_soname
    expected_version
  )
    set(smoke_target "metaflux_${provider_name}_provider_abi_smoke")
    add_executable(${smoke_target} contract/provider_abi_smoke.c)
    target_compile_definitions(
      ${smoke_target}
      PRIVATE
        "METAFLUX_PROVIDER_HEADER=\"${provider_header}\""
        "METAFLUX_PROVIDER_BOOTSTRAP=${expected_symbol}"
    )
    target_link_libraries(
      ${smoke_target}
      PRIVATE MetaFlux::ClientProtocol ${provider_target}
    )
    metaflux_configure_target(${smoke_target} C)
    add_test(
      NAME "metaflux.abi.provider.${provider_name}.bootstrap"
      COMMAND ${smoke_target}
    )
    set_tests_properties(
      "metaflux.abi.provider.${provider_name}.bootstrap"
      PROPERTIES LABELS "abi"
    )

    # Provider DT_NEEDED universe per decision-0009: libc.so.6 plus libdl.so.2 only where
    # the glibc 2.31 (Ubuntu 20.04) floor requires the historical DSO.
    # Passthrough discovery uses dlmopen/dlvsym. Those symbols live in libc on
    # glibc 2.34+ host builds, while the frozen 2.31 sysroot requires libdl.
    if(CMAKE_SYSROOT)
      set(provider_expected_needed "libc.so.6;libdl.so.2")
    else()
      set(provider_expected_needed "libc.so.6")
    endif()
    add_test(
      NAME "metaflux.abi.provider.${provider_name}.dynamic"
      COMMAND
        "${CMAKE_COMMAND}"
        "-DREADELF=${METAFLUX_READELF_EXECUTABLE}"
        "-DPROVIDER=$<TARGET_FILE:${provider_target}>"
        "-DEXPECTED_SONAME=${expected_soname}"
        "-DEXPECTED_NEEDED=${provider_expected_needed}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_provider_needed.cmake"
    )
    set_tests_properties(
      "metaflux.abi.provider.${provider_name}.dynamic"
      PROPERTIES LABELS "abi"
    )

    add_test(
      NAME "metaflux.abi.provider.${provider_name}.exports"
      COMMAND
        "${CMAKE_COMMAND}"
        "-DREADELF=${METAFLUX_READELF_EXECUTABLE}"
        "-DPROVIDER=$<TARGET_FILE:${provider_target}>"
        "-DEXPECTED_SYMBOL=${expected_symbol}"
        "-DEXPECTED_VERSION=${expected_version}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_provider_exports.cmake"
    )
    set_tests_properties(
      "metaflux.abi.provider.${provider_name}.exports"
      PROPERTIES LABELS "abi"
    )
  endfunction()

  if(METAFLUX_BUILD_CUDA_DRIVER_PROVIDER)
    metaflux_add_provider_abi_gate(
      cuda
      metaflux_cuda_provider
      metaflux/cuda/provider.h
      mf_cuda_provider_bootstrap_abi_version
      libcuda.so.1
      METAFLUX_CUDA_PROVIDER_1
    )
  endif()

  if(METAFLUX_BUILD_NVML_PROVIDER)
    metaflux_add_provider_abi_gate(
      nvml
      metaflux_nvml_provider
      metaflux/nvml/provider.h
      mf_nvml_provider_bootstrap_abi_version
      libnvidia-ml.so.1
      METAFLUX_NVML_PROVIDER_1
    )
  endif()
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

if(TARGET metaflux_cuda_provider AND TARGET metafluxd AND METAFLUX_NVIDIA_HEADER_DIR)
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

# Kernel debug-config probe: records CONFIG status and skips qualification when
# the host does not have a debug kernel. Live KUnit/sanitizer soak remains
# batch-0002 host gate; this probe only records CONFIG presence and skips
# qualification when unset.
find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_test(
  NAME metaflux.kernel.debug-config-probe
  COMMAND
    "${Python3_EXECUTABLE}" -B
    "${CMAKE_CURRENT_SOURCE_DIR}/../tools/probe-debug-kernel.py"
    --output
    "${CMAKE_BINARY_DIR}/metaflux-kernel-debug-config.json"
)
set_tests_properties(
  metaflux.kernel.debug-config-probe
  PROPERTIES LABELS "kernel;qualification"
)
add_test(
  NAME metaflux.kernel.debug-qualification
  COMMAND
    "${Python3_EXECUTABLE}" -B
    "${CMAKE_CURRENT_SOURCE_DIR}/../tools/probe-debug-kernel.py"
    --require-qualification
)
set_tests_properties(
  metaflux.kernel.debug-qualification
  PROPERTIES
    LABELS "kernel;qualification"
    SKIP_RETURN_CODE 77
)
# KUnit generation probe: skips when METAFLUX_LINUX_SRC is unset or invalid
# (no pinned linux-debug source). This gate is not gated on host CONFIG_KUNIT;
# that belongs to metaflux.kernel.debug-qualification.
add_test(
  NAME metaflux.kernel.kunit-generation
  COMMAND
    "${Python3_EXECUTABLE}" -B
    "${CMAKE_CURRENT_SOURCE_DIR}/../tests/kernel/probe-kunit-generation.py"
)
set_tests_properties(
  metaflux.kernel.kunit-generation
  PROPERTIES
    LABELS "kernel;qualification;kunit"
    SKIP_RETURN_CODE 77
)
# Userspace self-test of the generation/stale-handle helper logic.  This
# does NOT require a debug kernel and CAN pass on any host.
add_executable(
  metaflux_cdev_generation_helper_test
  "${CMAKE_CURRENT_SOURCE_DIR}/../kernel/tests/kunit/mf_cdev_generation_userspace_test.c"
)
target_include_directories(
  metaflux_cdev_generation_helper_test
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../kernel/core"
)
metaflux_configure_target(metaflux_cdev_generation_helper_test C)
add_test(
  NAME metaflux.kernel.cdev-generation-helper
  COMMAND metaflux_cdev_generation_helper_test
)
set_tests_properties(
  metaflux.kernel.cdev-generation-helper
  PROPERTIES
    LABELS "kernel;cdev;unit"
)
add_test(
  NAME metaflux.kernel.debug-config-probe-selftest
  COMMAND
    "${Python3_EXECUTABLE}" -B
    "${CMAKE_CURRENT_SOURCE_DIR}/../tools/test-probe-debug-kernel.py"
)
set_tests_properties(
  metaflux.kernel.debug-config-probe-selftest
  PROPERTIES LABELS "kernel;qualification;unit"
)
add_test(
  NAME metaflux.kernel.kunit-generation-selftest
  COMMAND
    "${Python3_EXECUTABLE}" -B
    "${CMAKE_CURRENT_SOURCE_DIR}/../tools/test-run-kunit-generation.py"
)
set_tests_properties(
  metaflux.kernel.kunit-generation-selftest
  PROPERTIES LABELS "kernel;qualification;unit"
)
# UML KUnit gate: re-enters the named linux-debug flake shell so CTest
# exercises the real in-kernel mf_cdev_generation qualification on the
# pinned linux_6_12 source. Skips 77 only when nix is unavailable; keeps
# its incremental UML build under kunit-uml-cache in this build tree.
add_test(
  NAME metaflux.kernel.kunit-uml
  COMMAND
    "${Python3_EXECUTABLE}" -B
    "${CMAKE_CURRENT_SOURCE_DIR}/../tools/run-kunit-uml-ctest.py"
)
set_tests_properties(
  metaflux.kernel.kunit-uml
  PROPERTIES
    LABELS "kernel;qualification;kunit"
    TIMEOUT 900
    SKIP_RETURN_CODE 77
    ENVIRONMENT "METAFLUX_KUNIT_CACHE_DIR=${CMAKE_BINARY_DIR}/kunit-uml-cache"
)
# Backend-Vulkan package gate: constructs the real metaflux-backend-vulkan
# package from a generic release build tree (skip 77 when the tree or the
# packaged shared backend is absent) and re-verifies deb/rpm/tar contents
# plus the glibc-2.31/NEEDED payload policy.
add_test(
  NAME metaflux.release.backend-vulkan-package
  COMMAND
    "${Python3_EXECUTABLE}" -B
    "${CMAKE_CURRENT_SOURCE_DIR}/../tools/test-backend-vulkan-package.py"
)
set_tests_properties(
  metaflux.release.backend-vulkan-package
  PROPERTIES
    LABELS "release;packaging"
    SKIP_RETURN_CODE 77
)
