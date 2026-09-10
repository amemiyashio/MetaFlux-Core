# Included from tests/CMakeLists.txt; paths retain the tests/ directory scope.

if(BUILD_TESTING AND METAFLUX_BUILD_TESTS)
  add_test(
    NAME metaflux.architecture.build-directory
    COMMAND "${Python3_EXECUTABLE}" -B
      "${CMAKE_CURRENT_SOURCE_DIR}/cmake/test_build_directory.py"
      --cmake "${CMAKE_COMMAND}"
  )
  set_tests_properties(metaflux.architecture.build-directory PROPERTIES LABELS "architecture")
  add_test(
    NAME metaflux.architecture.build-entrypoints
    COMMAND "${Python3_EXECUTABLE}" -B
      "${CMAKE_CURRENT_SOURCE_DIR}/cmake/test_build_entrypoints.py"
  )
  set_tests_properties(metaflux.architecture.build-entrypoints PROPERTIES LABELS "architecture")
endif()
