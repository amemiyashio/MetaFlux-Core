# Included from tests/CMakeLists.txt; paths retain the tests/ directory scope.

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

if(
  METAFLUX_BUILD_CUDA_DRIVER_PROVIDER
  OR METAFLUX_BUILD_CUBLAS_PROVIDER
  OR METAFLUX_BUILD_NVML_PROVIDER
)
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
    if(provider_name STREQUAL "cublas" OR CMAKE_SYSROOT)
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

  if(METAFLUX_BUILD_CUBLAS_PROVIDER)
    metaflux_add_provider_abi_gate(
      cublas
      metaflux_cublas_provider
      metaflux/cublas/provider.h
      mf_cublas_provider_bootstrap_abi_version
      libmetaflux-cublas-provider.so.1
      METAFLUX_CUBLAS_PROVIDER_1
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

if(BUILD_TESTING AND METAFLUX_BUILD_TESTS)
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
endif()
