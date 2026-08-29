add_executable(metaflux_cpu_interpreter_test cpu_interpreter_test.cpp)
target_link_libraries(
  metaflux_cpu_interpreter_test
  PRIVATE MetaFlux::CpuBackendRuntime
)
metaflux_configure_target(metaflux_cpu_interpreter_test CXX)
add_test(NAME metaflux.unit.cpu-interpreter COMMAND metaflux_cpu_interpreter_test)
set_tests_properties(
  metaflux.unit.cpu-interpreter
  PROPERTIES LABELS "unit;backend;cpu"
)

add_executable(metaflux_cpu_placement_test cpu_placement_test.cpp)
target_link_libraries(
  metaflux_cpu_placement_test
  PRIVATE MetaFlux::CpuBackendRuntime
)
metaflux_configure_target(metaflux_cpu_placement_test CXX)
add_test(NAME metaflux.unit.cpu-placement COMMAND metaflux_cpu_placement_test)
set_tests_properties(
  metaflux.unit.cpu-placement
  PROPERTIES LABELS "unit;backend;cpu;placement"
)

add_executable(metaflux_cpu_placement_amd_test cpu_placement_amd_test.cpp)
target_link_libraries(
  metaflux_cpu_placement_amd_test
  PRIVATE MetaFlux::CpuBackendRuntime
)
metaflux_configure_target(metaflux_cpu_placement_amd_test CXX)
add_test(NAME metaflux.integration.cpu-placement-amd COMMAND metaflux_cpu_placement_amd_test)
set_tests_properties(
  metaflux.integration.cpu-placement-amd
  PROPERTIES LABELS "integration;backend;cpu;placement" SKIP_RETURN_CODE 77
)

if(TARGET metaflux_cuda_ptx_frontend AND TARGET metaflux_compiler_core)
  add_executable(metaflux_cpu_add_copy_differential_test add_copy_differential_test.cpp)
  target_link_libraries(
    metaflux_cpu_add_copy_differential_test
    PRIVATE
      MetaFlux::CompilerCore
      MetaFlux::CpuBackendRuntime
      MetaFlux::CudaPtxFrontend
  )
  metaflux_configure_target(metaflux_cpu_add_copy_differential_test CXX)
  add_test(
    NAME metaflux.differential.cpu-add-copy
    COMMAND metaflux_cpu_add_copy_differential_test
  )
  set_tests_properties(
    metaflux.differential.cpu-add-copy
    PROPERTIES LABELS "unit;differential;compiler;backend;cpu"
  )

  add_executable(metaflux_cpu_ptx_corpus_differential_test ptx_corpus_differential_test.cpp)
  target_link_libraries(
    metaflux_cpu_ptx_corpus_differential_test
    PRIVATE
      MetaFlux::CompilerCore
      MetaFlux::CpuBackendRuntime
      MetaFlux::CudaPtxFrontend
  )
  target_compile_definitions(
    metaflux_cpu_ptx_corpus_differential_test
    PRIVATE
      METAFLUX_PTX_CORPUS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../../../../compat/cuda/compiler/ptx/corpus/fixtures"
  )
  metaflux_configure_target(metaflux_cpu_ptx_corpus_differential_test CXX)
  add_test(
    NAME metaflux.differential.cpu-ptx-corpus
    COMMAND metaflux_cpu_ptx_corpus_differential_test
  )
  set_tests_properties(
    metaflux.differential.cpu-ptx-corpus
    PROPERTIES LABELS "unit;differential;compiler;ptx;backend;cpu"
  )
endif()
