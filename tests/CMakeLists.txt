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

if(METAFLUX_BUILD_RUNTIME_CORE)
  add_executable(metaflux_runtime_unit_smoke unit/runtime_smoke.cpp)
  target_link_libraries(metaflux_runtime_unit_smoke PRIVATE MetaFlux::RuntimeCore)
  metaflux_configure_target(metaflux_runtime_unit_smoke CXX)
  add_test(NAME metaflux.unit.runtime COMMAND metaflux_runtime_unit_smoke)
  set_tests_properties(metaflux.unit.runtime PROPERTIES LABELS "unit")

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
endif()

if(TARGET metaflux_cpu_backend_runtime)
  add_executable(metaflux_backend_abi_smoke unit/backend_abi_smoke.cpp)
  target_link_libraries(metaflux_backend_abi_smoke PRIVATE MetaFlux::CpuBackendRuntime)
  metaflux_configure_target(metaflux_backend_abi_smoke CXX)
  add_test(NAME metaflux.abi.backend-cpu COMMAND metaflux_backend_abi_smoke)
  set_tests_properties(metaflux.abi.backend-cpu PROPERTIES LABELS "abi;unit")
endif()

if(TARGET metaflux_cuda_ptx_frontend)
  add_executable(metaflux_ptx_frontend_smoke unit/ptx_frontend_smoke.cpp)
  target_link_libraries(metaflux_ptx_frontend_smoke PRIVATE MetaFlux::CudaPtxFrontend)
  metaflux_configure_target(metaflux_ptx_frontend_smoke CXX)
  add_test(NAME metaflux.unit.ptx-frontend COMMAND metaflux_ptx_frontend_smoke)
  set_tests_properties(metaflux.unit.ptx-frontend PROPERTIES LABELS "unit")
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

    # Provider DT_NEEDED universe per D0009: libc.so.6 plus libpthread.so.0 and
    # libdl.so.2 only where the glibc 2.31 (Ubuntu 20.04) floor requires them.
    # Current fixtures only need libc; the expected set stays exact per target.
    add_test(
      NAME "metaflux.abi.provider.${provider_name}.dynamic"
      COMMAND
        "${CMAKE_COMMAND}"
        "-DREADELF=${METAFLUX_READELF_EXECUTABLE}"
        "-DPROVIDER=$<TARGET_FILE:${provider_target}>"
        "-DEXPECTED_SONAME=${expected_soname}"
        "-DEXPECTED_NEEDED=libc.so.6"
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

if(METAFLUX_BUILD_DAEMON)
  add_test(NAME metaflux.integration.daemon-version COMMAND metafluxd --version)
  set_tests_properties(metaflux.integration.daemon-version PROPERTIES LABELS "integration")
endif()
