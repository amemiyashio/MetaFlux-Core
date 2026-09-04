# work-item-0.1.1.3 live bring-up qualification. The fixture server links
# only the external host-qualified libvfio-user prerequisite recorded in
# toolchains/vfio-user-1.json plus the generated contract projections; it is
# not a repository component and is never installed. The runner boots the
# pinned QEMU with shared file-backed guest RAM, the running host kernel, and
# the static guest metaflux_pci.ko, then asserts PCI identity, runtime BAR
# access, doorbell ioeventfd, MSI-X delivery, and guest-RAM DMA mapping.

metaflux_enforce_c_only_directory("${CMAKE_CURRENT_SOURCE_DIR}")

set(METAFLUX_VFIO_USER_ROOT "" CACHE PATH "libvfio-user host prerequisite root")

find_path(
  METAFLUX_VFIO_USER_INCLUDE_DIR
  NAMES libvfio-user.h
  HINTS "${METAFLUX_VFIO_USER_ROOT}" "$ENV{METAFLUX_VFIO_USER_ROOT}"
        "$ENV{HOME}/.local/libvfio-user"
  PATH_SUFFIXES include
)
find_library(
  METAFLUX_VFIO_USER_LIBRARY
  NAMES vfio-user
  HINTS "${METAFLUX_VFIO_USER_ROOT}" "$ENV{METAFLUX_VFIO_USER_ROOT}"
        "$ENV{HOME}/.local/libvfio-user"
  PATH_SUFFIXES lib
)

set(METAFLUX_VFIO_USER_LIVE_SERVER_ARG "")
if(METAFLUX_VFIO_USER_INCLUDE_DIR AND METAFLUX_VFIO_USER_LIBRARY)
  add_executable(metaflux_vfu_live_server src/metaflux_vfu_live_server.c)
  target_include_directories(
    metaflux_vfu_live_server
    SYSTEM PRIVATE "${METAFLUX_VFIO_USER_INCLUDE_DIR}"
  )
  target_include_directories(
    metaflux_vfu_live_server
    PRIVATE
      "${CMAKE_BINARY_DIR}/contracts/generated/include"
      "${CMAKE_SOURCE_DIR}/contracts/shared/device/v1/include"
      "${CMAKE_SOURCE_DIR}/contracts/uapi/linux/v1/include"
  )
  target_link_libraries(
    metaflux_vfu_live_server
    PRIVATE "${METAFLUX_VFIO_USER_LIBRARY}" pthread
  )
  target_compile_definitions(
    metaflux_vfu_live_server
    PRIVATE "METAFLUX_PROJECT_VERSION=\"${PROJECT_VERSION}\""
  )
  metaflux_configure_target(metaflux_vfu_live_server C)
  add_dependencies(metaflux_vfu_live_server metaflux_transport_schema_projection)
  get_filename_component(
    METAFLUX_VFIO_USER_LIBRARY_DIR "${METAFLUX_VFIO_USER_LIBRARY}" DIRECTORY
  )
  set_target_properties(
    metaflux_vfu_live_server
    PROPERTIES BUILD_RPATH "${METAFLUX_VFIO_USER_LIBRARY_DIR}"
  )
  set(METAFLUX_VFIO_USER_LIVE_SERVER_ARG "$<TARGET_FILE:metaflux_vfu_live_server>")
else()
  message(
    STATUS
    "libvfio-user prerequisite not found; the live bring-up server binary is not built"
  )
endif()

if(BUILD_TESTING AND METAFLUX_BUILD_TESTS)
  find_package(Python3 REQUIRED COMPONENTS Interpreter)
  if(METAFLUX_VFIO_USER_LIBRARY_DIR)
    set(METAFLUX_VFIO_USER_LIVE_TEST_ENV
      "ENVIRONMENT_MODIFICATION"
      "LD_LIBRARY_PATH=path_list_prepend:${METAFLUX_VFIO_USER_LIBRARY_DIR}"
    )
  else()
    set(METAFLUX_VFIO_USER_LIVE_TEST_ENV "")
  endif()
  add_test(
    NAME metaflux.transport.vfio-user-live-bringup
    COMMAND
      "${Python3_EXECUTABLE}" -B
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/run_vfio_user_live_bringup.py"
      --repository "${PROJECT_SOURCE_DIR}"
      --server "${METAFLUX_VFIO_USER_LIVE_SERVER_ARG}"
  )
  set_tests_properties(
    metaflux.transport.vfio-user-live-bringup
    PROPERTIES
    LABELS "transport;qualification;live"
    TIMEOUT 300
    SKIP_RETURN_CODE 77
    ${METAFLUX_VFIO_USER_LIVE_TEST_ENV}
  )
endif()
