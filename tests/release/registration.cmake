# Included from tests/CMakeLists.txt; paths retain the tests/ directory scope.

if(BUILD_TESTING AND METAFLUX_BUILD_TESTS)
  add_test(
    NAME metaflux.packaging.vroot-dkms
    COMMAND
      "${Python3_EXECUTABLE}" -B
      "${CMAKE_CURRENT_SOURCE_DIR}/release/test-vroot-dkms-packaging.py"
  )
  set_tests_properties(
    metaflux.packaging.vroot-dkms
    PROPERTIES LABELS "packaging;vroot;unit"
  )
  add_test(
    NAME metaflux.packaging.vpci-dkms
    COMMAND
      "${Python3_EXECUTABLE}" -B
      "${CMAKE_CURRENT_SOURCE_DIR}/release/test-vpci-dkms-packaging.py"
  )
  set_tests_properties(
    metaflux.packaging.vpci-dkms
    PROPERTIES LABELS "packaging;pci;unit"
  )
  add_test(
    NAME metaflux.release.baremetal-vpci
    COMMAND
      "${Python3_EXECUTABLE}" -B
      "${CMAKE_CURRENT_SOURCE_DIR}/release/run_baremetal_vpci_gate.py"
      --output
      "${CMAKE_BINARY_DIR}/metaflux-baremetal-vpci-gate.json"
  )
  set_tests_properties(
    metaflux.release.baremetal-vpci
    PROPERTIES LABELS "packaging;vroot;release;qualification"
  )
  add_test(
    NAME metaflux.release.baremetal-vpci-selftest
    COMMAND
      "${Python3_EXECUTABLE}" -B
      "${CMAKE_CURRENT_SOURCE_DIR}/release/test_baremetal_vpci_gate.py"
  )
  set_tests_properties(
    metaflux.release.baremetal-vpci-selftest
    PROPERTIES LABELS "packaging;vroot;release;unit"
  )
  add_test(
    NAME metaflux.release.lifecycle-core-packaging
    COMMAND
      "${Python3_EXECUTABLE}" -B
      "${CMAKE_CURRENT_SOURCE_DIR}/release/run_lifecycle_core_packaging_gate.py"
      --output
      "${CMAKE_BINARY_DIR}/metaflux-lifecycle-core-packaging-gate.json"
  )
  set_tests_properties(
    metaflux.release.lifecycle-core-packaging
    PROPERTIES LABELS "packaging;lifecycle;release;qualification"
  )
  add_test(
    NAME metaflux.release.lifecycle-core-packaging-selftest
    COMMAND
      "${Python3_EXECUTABLE}" -B
      "${CMAKE_CURRENT_SOURCE_DIR}/release/test_lifecycle_core_packaging_gate.py"
  )
  set_tests_properties(
    metaflux.release.lifecycle-core-packaging-selftest
    PROPERTIES LABELS "packaging;lifecycle;release;unit"
  )
  add_test(
    NAME metaflux.packaging.backend-vulkan
    COMMAND
      "${Python3_EXECUTABLE}" -B
      "${CMAKE_CURRENT_SOURCE_DIR}/../tools/validate-backend-vulkan-packaging.py"
      --output
      "${CMAKE_BINARY_DIR}/metaflux-backend-vulkan-packaging.json"
  )
  set_tests_properties(
    metaflux.packaging.backend-vulkan
    PROPERTIES LABELS "packaging;vulkan;unit"
  )
  add_test(
    NAME metaflux.release.matrix-assertions
    COMMAND
      "${Python3_EXECUTABLE}" -B
      "${CMAKE_CURRENT_SOURCE_DIR}/release/test_release_matrix_assertions.py"
  )
  set_tests_properties(
    metaflux.release.matrix-assertions
    PROPERTIES LABELS "release;unit"
  )
endif()

# Backend-Vulkan package gate: constructs the real metaflux-backend-vulkan
# package from a generic release build tree (skip 77 when the tree or the
# packaged shared backend is absent) and re-verifies deb/rpm/tar contents
# plus the glibc-2.31/NEEDED payload policy.
add_test(
  NAME metaflux.release.backend-vulkan-package
  COMMAND
    "${Python3_EXECUTABLE}" -B
    "${CMAKE_CURRENT_SOURCE_DIR}/release/test-backend-vulkan-package.py"
)
set_tests_properties(
  metaflux.release.backend-vulkan-package
  PROPERTIES
    LABELS "release;packaging"
    SKIP_RETURN_CODE 77
)
