# Included from tests/CMakeLists.txt; paths retain the tests/ directory scope.

if(BUILD_TESTING AND METAFLUX_BUILD_TESTS)
  add_test(
    NAME metaflux.architecture.pytorch-readiness
    COMMAND "${Python3_EXECUTABLE}" -B
      "${CMAKE_CURRENT_SOURCE_DIR}/../tools/check-pytorch-cuda-readiness.py"
      "${PROJECT_SOURCE_DIR}"
  )
  add_test(
    NAME metaflux.architecture.pytorch-readiness-selftest
    COMMAND "${Python3_EXECUTABLE}" -B
      "${CMAKE_CURRENT_SOURCE_DIR}/architecture/test-check-pytorch-cuda-readiness.py"
  )
  set_tests_properties(metaflux.architecture.pytorch-readiness
    metaflux.architecture.pytorch-readiness-selftest PROPERTIES LABELS "architecture;compatibility")
  add_test(
    NAME metaflux.architecture.main-workflow-selftest
    COMMAND "${Python3_EXECUTABLE}" -B
      "${CMAKE_CURRENT_SOURCE_DIR}/../agent/skills/main/scripts/test_workflow_state.py"
  )
  set_tests_properties(metaflux.architecture.main-workflow-selftest PROPERTIES LABELS "architecture")
  foreach(guard IN ITEMS rule_loading tool_gate commit_gate verification)
    string(REPLACE "_" "-" guard_name "${guard}")
    add_test(
      NAME "metaflux.architecture.${guard_name}-selftest"
      COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/../agent/skills/main/scripts/test_${guard}.py"
    )
    set_tests_properties("metaflux.architecture.${guard_name}-selftest" PROPERTIES LABELS "architecture")
  endforeach()
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
    NAME metaflux.architecture.agent-state-selftest
    COMMAND
      "${Python3_EXECUTABLE}" -B
      "${CMAKE_CURRENT_SOURCE_DIR}/architecture/test-check-agent-state.py"
  )
  set_tests_properties(
    metaflux.architecture.agent-state-selftest
    PROPERTIES LABELS "architecture"
  )
  add_test(
    NAME metaflux.architecture.reference-catalog
    COMMAND
      "${Python3_EXECUTABLE}" -B
      "${CMAKE_CURRENT_SOURCE_DIR}/../references/tools/reference.py"
      --root "${PROJECT_SOURCE_DIR}" verify
  )
  set_tests_properties(
    metaflux.architecture.reference-catalog
    PROPERTIES LABELS "architecture;reference"
  )
  add_test(
    NAME metaflux.architecture.reference-catalog-selftest
    COMMAND
      "${Python3_EXECUTABLE}" -B
      "${CMAKE_CURRENT_SOURCE_DIR}/../references/tools/test_reference.py"
  )
  set_tests_properties(
    metaflux.architecture.reference-catalog-selftest
    PROPERTIES LABELS "architecture;reference;unit"
  )
  add_test(
    NAME metaflux.architecture.agent-diagnostics-selftest
    COMMAND
      "${Python3_EXECUTABLE}" -B
      "${CMAKE_CURRENT_SOURCE_DIR}/architecture/test-agent-diagnostics.py"
  )
  set_tests_properties(
    metaflux.architecture.agent-diagnostics-selftest
    PROPERTIES LABELS "architecture"
  )
  add_test(
    NAME metaflux.architecture.agent-tool-detection-selftest
    COMMAND
      "${Python3_EXECUTABLE}" -B
      "${CMAKE_CURRENT_SOURCE_DIR}/../agent/skills/main/scripts/test_detect_agent_tool.py"
  )
  set_tests_properties(
    metaflux.architecture.agent-tool-detection-selftest
    PROPERTIES LABELS "architecture"
  )
  add_test(
    NAME metaflux.architecture.agent-commit-identity-selftest
    COMMAND
      "${Python3_EXECUTABLE}" -B
      "${CMAKE_CURRENT_SOURCE_DIR}/../agent/skills/main/scripts/test_commit_as_agent_tool.py"
  )
  set_tests_properties(
    metaflux.architecture.agent-commit-identity-selftest
    PROPERTIES LABELS "architecture"
  )
  add_test(
    NAME metaflux.architecture.repository-push-selftest
    COMMAND
      "${Python3_EXECUTABLE}" -B
      "${CMAKE_CURRENT_SOURCE_DIR}/../agent/skills/main/scripts/test_push_repository.py"
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
      "${CMAKE_CURRENT_SOURCE_DIR}/architecture/test-check-skill-routing.py"
  )
  set_tests_properties(
    metaflux.architecture.skill-routing-selftest
    PROPERTIES LABELS "architecture"
  )
  add_test(
    NAME metaflux.architecture.epoch-selftest
    COMMAND
      "${Python3_EXECUTABLE}" -B
      "${CMAKE_CURRENT_SOURCE_DIR}/../agent/skills/epoch/scripts/test_check_route_proposal.py"
  )
  set_tests_properties(
    metaflux.architecture.epoch-selftest
    PROPERTIES LABELS "architecture"
  )
  add_test(
    NAME metaflux.architecture.batch-selftest
    COMMAND
      "${Python3_EXECUTABLE}" -B
      "${CMAKE_CURRENT_SOURCE_DIR}/../agent/skills/batch/scripts/test_batch.py"
  )
  set_tests_properties(
    metaflux.architecture.batch-selftest
    PROPERTIES LABELS "architecture"
  )
endif()
