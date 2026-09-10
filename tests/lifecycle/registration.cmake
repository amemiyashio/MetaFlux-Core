# Included from tests/CMakeLists.txt; paths retain the tests/ directory scope.

if(BUILD_TESTING AND METAFLUX_BUILD_TESTS)
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
      "${CMAKE_CURRENT_SOURCE_DIR}/lifecycle/test-lifecycle-admin.py"
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
endif()
