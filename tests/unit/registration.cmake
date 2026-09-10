# Included from tests/CMakeLists.txt; paths retain the tests/ directory scope.

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

if(TARGET metaflux_cuda_ptx_frontend)
  add_executable(metaflux_ptx_frontend_smoke unit/ptx_frontend_smoke.cpp)
  target_link_libraries(metaflux_ptx_frontend_smoke PRIVATE MetaFlux::CudaPtxFrontend)
  metaflux_configure_target(metaflux_ptx_frontend_smoke CXX)
  add_test(NAME metaflux.unit.ptx-frontend COMMAND metaflux_ptx_frontend_smoke)
  set_tests_properties(metaflux.unit.ptx-frontend PROPERTIES LABELS "unit")
endif()
