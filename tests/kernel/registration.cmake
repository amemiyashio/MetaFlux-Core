# Included from tests/CMakeLists.txt; paths retain the tests/ directory scope.

if(TARGET metaflux_transport_cdev_client)
  find_package(ZLIB REQUIRED)
  add_executable(
    metaflux_transport_cdev_live_qualification
    ../linux-kernel-drivers/tests/kselftest/cdev_qualification.c
  )
  target_link_libraries(
    metaflux_transport_cdev_live_qualification
    PRIVATE MetaFlux::TransportCdevClient MetaFlux::ClientFastpath
  )
  get_filename_component(_metaflux_live_zlib_dir "${ZLIB_LIBRARY}" DIRECTORY)
  # Without the daemon target (guest/standalone qualification builds) the
  # live qualification skips its daemon CDEV_BIND section.
  if(TARGET metafluxd)
    set(_metaflux_live_daemon_define "METAFLUX_DAEMON_EXECUTABLE=\"$<TARGET_FILE:metafluxd>\"")
  else()
    set(_metaflux_live_daemon_define "METAFLUX_DAEMON_EXECUTABLE=\"\"")
  endif()
  target_compile_definitions(
    metaflux_transport_cdev_live_qualification
    PRIVATE
      "${_metaflux_live_daemon_define}"
      METAFLUX_DAEMON_LIBRARY_PATH="${_metaflux_live_zlib_dir}"
  )
  metaflux_configure_target(metaflux_transport_cdev_live_qualification C)
  add_test(
    NAME metaflux.transport.cdev-live-qualification
    COMMAND metaflux_transport_cdev_live_qualification
  )
  set_tests_properties(
    metaflux.transport.cdev-live-qualification
    PROPERTIES
      LABELS "transport;qualification;live"
      SKIP_RETURN_CODE 77
  )
endif()

# Kernel debug-config probe: records CONFIG status and skips qualification when
# the host does not have a debug kernel. Live KUnit/sanitizer soak remains
# batch-0002 host gate; this probe only records CONFIG presence and skips
# qualification when unset.
add_test(
  NAME metaflux.kernel.debug-config-probe
  COMMAND
    "${Python3_EXECUTABLE}" -B
    "${CMAKE_CURRENT_SOURCE_DIR}/../tools/probe-debug-kernel.py"
    --output
    "${CMAKE_BINARY_DIR}/metaflux-kernel-debug-config.json"
)
set_tests_properties(
  metaflux.kernel.debug-config-probe
  PROPERTIES LABELS "kernel;qualification"
)
add_test(
  NAME metaflux.kernel.debug-qualification
  COMMAND
    "${Python3_EXECUTABLE}" -B
    "${CMAKE_CURRENT_SOURCE_DIR}/../tools/probe-debug-kernel.py"
    --require-qualification
)
set_tests_properties(
  metaflux.kernel.debug-qualification
  PROPERTIES
    LABELS "kernel;qualification"
    SKIP_RETURN_CODE 77
)
# KUnit generation probe: skips when METAFLUX_LINUX_SRC is unset or invalid
# (no pinned linux-debug source). This gate is not gated on host CONFIG_KUNIT;
# that belongs to metaflux.kernel.debug-qualification.
add_test(
  NAME metaflux.kernel.kunit-generation
  COMMAND
    "${Python3_EXECUTABLE}" -B
    "${CMAKE_CURRENT_SOURCE_DIR}/kernel/probe-kunit-generation.py"
)
set_tests_properties(
  metaflux.kernel.kunit-generation
  PROPERTIES
    LABELS "kernel;qualification;kunit"
    SKIP_RETURN_CODE 77
)
# Userspace self-test of the generation/stale-handle helper logic.  This
# does NOT require a debug kernel and CAN pass on any host.
add_executable(
  metaflux_cdev_generation_helper_test
  "${CMAKE_CURRENT_SOURCE_DIR}/../linux-kernel-drivers/tests/kunit/mf_cdev_generation_userspace_test.c"
)
target_include_directories(
  metaflux_cdev_generation_helper_test
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../linux-kernel-drivers/core"
)
metaflux_configure_target(metaflux_cdev_generation_helper_test C)
add_test(
  NAME metaflux.kernel.cdev-generation-helper
  COMMAND metaflux_cdev_generation_helper_test
)
set_tests_properties(
  metaflux.kernel.cdev-generation-helper
  PROPERTIES
    LABELS "kernel;cdev;unit"
)
add_test(
  NAME metaflux.kernel.debug-config-probe-selftest
  COMMAND
    "${Python3_EXECUTABLE}" -B
    "${CMAKE_CURRENT_SOURCE_DIR}/kernel/test-probe-debug-kernel.py"
)
set_tests_properties(
  metaflux.kernel.debug-config-probe-selftest
  PROPERTIES LABELS "kernel;qualification;unit"
)
add_test(
  NAME metaflux.kernel.kunit-generation-selftest
  COMMAND
    "${Python3_EXECUTABLE}" -B
    "${CMAKE_CURRENT_SOURCE_DIR}/kernel/test-run-kunit-generation.py"
)
set_tests_properties(
  metaflux.kernel.kunit-generation-selftest
  PROPERTIES LABELS "kernel;qualification;unit"
)
# UML KUnit gate: re-enters the named linux-debug flake shell so CTest
# exercises the real in-kernel mf_cdev_generation qualification on the
# pinned linux_6_12 source. Skips 77 only when nix is unavailable; keeps
# its incremental UML build under kunit-uml-cache in this build tree.
add_test(
  NAME metaflux.kernel.kunit-uml
  COMMAND
    "${Python3_EXECUTABLE}" -B
    "${CMAKE_CURRENT_SOURCE_DIR}/../tools/run-kunit-uml-ctest.py"
)
set_tests_properties(
  metaflux.kernel.kunit-uml
  PROPERTIES
    LABELS "kernel;qualification;kunit"
    TIMEOUT 900
    SKIP_RETURN_CODE 77
    ENVIRONMENT "METAFLUX_KUNIT_CACHE_DIR=${CMAKE_BINARY_DIR}/kunit-uml-cache"
)
# Debug-kernel supply-path self-test: exercises the runner's .config reader,
# guest-console parsers, initramfs packer, and vermagic helpers plus the
# missing-bzImage skip path. It never boots a guest; the real guest run stays
# a manual batch gate (see linux-kernel-drivers/tests/README.md "Batch-0002 debug kernel
# supply path").
add_test(
  NAME metaflux.kernel.debug-kernel-selftest
  COMMAND
    "${Python3_EXECUTABLE}" -B
    "${CMAKE_CURRENT_SOURCE_DIR}/kernel/test-run-debug-kernel-qualification.py"
)
set_tests_properties(
  metaflux.kernel.debug-kernel-selftest
  PROPERTIES
    LABELS "kernel;qualification;unit"
    TIMEOUT 900
    SKIP_RETURN_CODE 77
)
