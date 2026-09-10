# Included from tests/CMakeLists.txt; paths retain the tests/ directory scope.

if(BUILD_TESTING AND METAFLUX_BUILD_TESTS)
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
      "${CMAKE_CURRENT_SOURCE_DIR}/../agent/skills/detect-agent-tool/scripts/test_detect_agent_tool.py"
  )
  set_tests_properties(
    metaflux.architecture.agent-tool-detection-selftest
    PROPERTIES LABELS "architecture"
  )
  add_test(
    NAME metaflux.architecture.agent-commit-identity-selftest
    COMMAND
      "${Python3_EXECUTABLE}" -B
      "${CMAKE_CURRENT_SOURCE_DIR}/../agent/skills/start-work/scripts/test_commit_as_agent_tool.py"
  )
  set_tests_properties(
    metaflux.architecture.agent-commit-identity-selftest
    PROPERTIES LABELS "architecture"
  )
  add_test(
    NAME metaflux.architecture.repository-push-selftest
    COMMAND
      "${Python3_EXECUTABLE}" -B
      "${CMAKE_CURRENT_SOURCE_DIR}/../agent/skills/push-repository/scripts/test_push_repository.py"
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
    NAME metaflux.architecture.replan-roadmap-selftest
    COMMAND
      "${Python3_EXECUTABLE}" -B
      "${CMAKE_CURRENT_SOURCE_DIR}/../agent/skills/replan-roadmap/scripts/test_check_route_proposal.py"
  )
  set_tests_properties(
    metaflux.architecture.replan-roadmap-selftest
    PROPERTIES LABELS "architecture"
  )
  add_test(
    NAME metaflux.architecture.accept-and-advance-selftest
    COMMAND
      "${Python3_EXECUTABLE}" -B
      "${CMAKE_CURRENT_SOURCE_DIR}/../agent/skills/accept-and-advance/scripts/test_accept_and_advance.py"
  )
  set_tests_properties(
    metaflux.architecture.accept-and-advance-selftest
    PROPERTIES LABELS "architecture"
  )
endif()
