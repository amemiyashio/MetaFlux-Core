add_executable(metaflux_cuda_ptx_frontend_test ptx_frontend_test.cpp)
target_link_libraries(
  metaflux_cuda_ptx_frontend_test
  PRIVATE MetaFlux::CudaPtxFrontend
)
target_compile_definitions(
  metaflux_cuda_ptx_frontend_test
  PRIVATE
    METAFLUX_PTX_CORPUS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../corpus/fixtures"
    METAFLUX_PTX_FORMS_PATH="${CMAKE_CURRENT_SOURCE_DIR}/../manifest/forms.jsonl"
)
metaflux_configure_target(metaflux_cuda_ptx_frontend_test CXX)
add_test(NAME metaflux.unit.ptx-parser COMMAND metaflux_cuda_ptx_frontend_test)
set_tests_properties(metaflux.unit.ptx-parser PROPERTIES LABELS "unit;compiler;ptx")

add_executable(metaflux_cuda_ptx_fuzz_driver ptx_fuzz_driver.cpp)
target_link_libraries(
  metaflux_cuda_ptx_fuzz_driver
  PRIVATE MetaFlux::CudaPtxFrontend
)
metaflux_configure_target(metaflux_cuda_ptx_fuzz_driver CXX)

find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_test(
  NAME metaflux.unit.ptx-manifest
  COMMAND
    Python3::Interpreter
    "${CMAKE_CURRENT_SOURCE_DIR}/validate_manifest.py"
    "${CMAKE_CURRENT_SOURCE_DIR}/../manifest/capabilities.json"
    "${CMAKE_CURRENT_SOURCE_DIR}/../manifest/forms.jsonl"
    "${CMAKE_CURRENT_SOURCE_DIR}/../corpus/index.json"
)
set_tests_properties(
  metaflux.unit.ptx-manifest
  PROPERTIES LABELS "unit;compiler;ptx;manifest"
)
