if(TARGET metaflux_cuda_ptx_frontend AND TARGET metaflux_cpu_backend_runtime)
  find_program(METAFLUX_LLVM_READELF_EXECUTABLE NAMES llvm-readelf REQUIRED)
  find_program(METAFLUX_LLVM_OBJDUMP_EXECUTABLE NAMES llvm-objdump REQUIRED)

  add_executable(metaflux_cpu_compiler_pipeline_test compiler_pipeline_test.cpp)
  target_link_libraries(
    metaflux_cpu_compiler_pipeline_test
    PRIVATE
      MetaFlux::CpuBackendCompiler
      MetaFlux::CpuBackendRuntime
      MetaFlux::CudaPtxFrontend
  )
  target_compile_definitions(
    metaflux_cpu_compiler_pipeline_test
    PRIVATE METAFLUX_TEST_LLD_PATH="${METAFLUX_LLD_EXECUTABLE}"
  )
  metaflux_configure_target(metaflux_cpu_compiler_pipeline_test CXX)

  add_executable(metaflux_cpu_compiled_corpus_test compiled_corpus_test.cpp)
  target_link_libraries(
    metaflux_cpu_compiled_corpus_test
    PRIVATE
      MetaFlux::CpuBackendCompiler
      MetaFlux::CpuBackendRuntime
      MetaFlux::CudaPtxFrontend
  )
  target_compile_definitions(
    metaflux_cpu_compiled_corpus_test
    PRIVATE
      METAFLUX_PTX_CORPUS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../../../../compat/cuda/compiler/ptx/corpus/fixtures"
  )
  metaflux_configure_target(metaflux_cpu_compiled_corpus_test CXX)

  set(metaflux_cpu_qualified_elf "${CMAKE_CURRENT_BINARY_DIR}/qualified-add.so")
  add_test(
    NAME metaflux.integration.cpu-compiler-pipeline
    COMMAND metaflux_cpu_compiler_pipeline_test "${metaflux_cpu_qualified_elf}"
  )
  set_tests_properties(
    metaflux.integration.cpu-compiler-pipeline
    PROPERTIES LABELS "integration;compiler;backend;cpu"
  )

  add_test(
    NAME metaflux.differential.cpu-compiled-corpus
    COMMAND metaflux_cpu_compiled_corpus_test
  )
  set_tests_properties(
    metaflux.differential.cpu-compiled-corpus
    PROPERTIES LABELS "integration;differential;compiler;ptx;backend;cpu"
  )

  add_test(
    NAME metaflux.qualification.cpu-pic-elf-readelf
    COMMAND
      "${METAFLUX_LLVM_READELF_EXECUTABLE}"
      --file-header
      --program-headers
      --dyn-syms
      "${metaflux_cpu_qualified_elf}"
  )
  set_tests_properties(
    metaflux.qualification.cpu-pic-elf-readelf
    PROPERTIES
      DEPENDS metaflux.integration.cpu-compiler-pipeline
      LABELS "qualification;compiler;backend;cpu"
      PASS_REGULAR_EXPRESSION "metaflux_cpu_cta_v2"
  )

  add_test(
    NAME metaflux.qualification.cpu-simd-objdump
    COMMAND
      "${METAFLUX_LLVM_OBJDUMP_EXECUTABLE}"
      --disassemble
      --no-show-raw-insn
      "${metaflux_cpu_qualified_elf}"
  )
  set_tests_properties(
    metaflux.qualification.cpu-simd-objdump
    PROPERTIES
      DEPENDS metaflux.integration.cpu-compiler-pipeline
      LABELS "qualification;compiler;backend;cpu;simd"
      PASS_REGULAR_EXPRESSION "paddd"
  )
endif()
