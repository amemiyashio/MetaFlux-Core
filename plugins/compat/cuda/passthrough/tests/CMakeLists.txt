function(metaflux_add_passthrough_fixture target source output_name version soversion directory map)
  if(IS_ABSOLUTE "${map}")
    set(map_path "${map}")
  else()
    set(map_path "${CMAKE_CURRENT_SOURCE_DIR}/${map}")
  endif()
  add_library(${target} SHARED ${source})
  target_compile_features(${target} PRIVATE c_std_17)
  target_compile_options(
    ${target}
    PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow
  )
  if(METAFLUX_ENABLE_WERROR)
    target_compile_options(${target} PRIVATE -Werror)
  endif()
  target_link_options(
    ${target}
    PRIVATE
      "LINKER:--build-id=sha1"
      "LINKER:--version-script=${map_path}"
  )
  set_property(TARGET ${target} APPEND PROPERTY LINK_DEPENDS "${map_path}")
  target_include_directories(
    ${target}
    PRIVATE
      "${CMAKE_CURRENT_SOURCE_DIR}/../../abi/driver/include"
      "${CMAKE_CURRENT_SOURCE_DIR}/../../management/nvml/include"
  )
  if(METAFLUX_USE_LLD)
    target_link_options(${target} PRIVATE -fuse-ld=lld)
  endif()
  set_target_properties(
    ${target}
    PROPERTIES
      C_VISIBILITY_PRESET default
      LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/${directory}"
      OUTPUT_NAME "${output_name}"
      SOVERSION "${soversion}"
      VERSION "${version}"
  )
endfunction()

set(cuda_fixture_map "${CMAKE_CURRENT_BINARY_DIR}/cuda-full.map")
set(nvml_fixture_map "${CMAKE_CURRENT_BINARY_DIR}/nvml-full.map")
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/../../abi/driver/symbols.def" cuda_fixture_rows)
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/../../management/nvml/symbols.def" nvml_fixture_rows)
set(cuda_fixture_exports "    mf_fixture_cuda_call_count;\n")
set(nvml_fixture_exports "    mf_fixture_nvml_call_count;\n")
foreach(symbol_row IN LISTS cuda_fixture_rows)
  if(symbol_row MATCHES "^MF_CUDA_SYMBOL\\(([A-Za-z0-9_]+),")
    string(APPEND cuda_fixture_exports "    ${CMAKE_MATCH_1};\n")
  endif()
endforeach()
foreach(symbol_row IN LISTS nvml_fixture_rows)
  if(symbol_row MATCHES "^MF_NVML_SYMBOL\\(([A-Za-z0-9_]+),")
    string(APPEND nvml_fixture_exports "    ${CMAKE_MATCH_1};\n")
  endif()
endforeach()
file(
  WRITE "${cuda_fixture_map}"
  "libcuda.so.1 {\n  global:\n${cuda_fixture_exports}  local:\n    *;\n};\n"
)
file(
  WRITE "${nvml_fixture_map}"
  "libnvidia-ml.so.1 {\n  global:\n${nvml_fixture_exports}  local:\n    *;\n};\n"
)

metaflux_add_passthrough_fixture(
  metaflux_passthrough_cuda_good
  fixture_cuda.c
  cuda
  777.42.01
  1
  good
  "${cuda_fixture_map}"
)
target_sources(metaflux_passthrough_cuda_good PRIVATE fixture_cuda_symbols.c)
target_compile_options(metaflux_passthrough_cuda_good PRIVATE -Wno-unused-parameter)
target_link_libraries(metaflux_passthrough_cuda_good PRIVATE ${CMAKE_DL_LIBS})
metaflux_add_passthrough_fixture(
  metaflux_passthrough_nvml_good
  fixture_nvml.c
  nvidia-ml
  777.42.01
  1
  good
  "${nvml_fixture_map}"
)
target_sources(metaflux_passthrough_nvml_good PRIVATE fixture_nvml_symbols.c)
target_compile_options(metaflux_passthrough_nvml_good PRIVATE -Wno-unused-parameter)
metaflux_add_passthrough_fixture(
  metaflux_passthrough_cuda_wrong_soname
  fixture_cuda.c
  cuda
  777.42.01
  9
  wrong-soname
  cuda.map
)
metaflux_add_passthrough_fixture(
  metaflux_passthrough_cuda_missing_bootstrap
  fixture_missing.c
  cuda
  777.42.01
  1
  missing-bootstrap
  missing.map
)
metaflux_add_passthrough_fixture(
  metaflux_passthrough_nvml_mismatched
  fixture_nvml.c
  nvidia-ml
  778.42.01
  1
  mismatched
  nvml.map
)
metaflux_add_passthrough_fixture(
  metaflux_passthrough_shadow_provider
  fixture_missing.c
  metaflux-shadow-provider
  1.0.0
  1
  shadow
  missing.map
)

add_executable(metaflux_cuda_passthrough_test passthrough_test.c)
add_dependencies(
  metaflux_cuda_passthrough_test
  metaflux_passthrough_cuda_good
  metaflux_passthrough_nvml_good
  metaflux_passthrough_shadow_provider
  metaflux_passthrough_cuda_wrong_soname
  metaflux_passthrough_cuda_missing_bootstrap
  metaflux_passthrough_nvml_mismatched
)
target_include_directories(
  metaflux_cuda_passthrough_test
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../src"
)
target_link_libraries(
  metaflux_cuda_passthrough_test
  PRIVATE MetaFlux::CudaPassthrough ${CMAKE_DL_LIBS}
)
metaflux_configure_target(metaflux_cuda_passthrough_test C)

add_test(
  NAME metaflux.unit.cuda-passthrough
  COMMAND
    metaflux_cuda_passthrough_test
    $<TARGET_FILE:metaflux_passthrough_cuda_good>
    $<TARGET_FILE:metaflux_passthrough_nvml_good>
    $<TARGET_FILE:metaflux_passthrough_shadow_provider>
    $<TARGET_FILE:metaflux_passthrough_cuda_wrong_soname>
    $<TARGET_FILE:metaflux_passthrough_cuda_missing_bootstrap>
    $<TARGET_FILE:metaflux_passthrough_nvml_mismatched>
)
set_tests_properties(
  metaflux.unit.cuda-passthrough
  PROPERTIES LABELS "abi;fault;passthrough;unit" TIMEOUT 60
)
