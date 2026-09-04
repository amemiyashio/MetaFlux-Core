# NO_CMAKE_FIND_ROOT_PATH: the pinned SDK lives outside the generic target
# sysroot, and the cross toolchain restricts root-path-only searches to it.
find_path(
  METAFLUX_VULKAN_INCLUDE_DIR
  vulkan/vulkan.h
  HINTS "${METAFLUX_VULKAN_SDK_DIR}/include"
  NO_CMAKE_FIND_ROOT_PATH
)
find_library(
  METAFLUX_VULKAN_LIBRARY
  NAMES vulkan
  HINTS "${METAFLUX_VULKAN_SDK_DIR}/lib"
  NO_CMAKE_FIND_ROOT_PATH
)
if(NOT METAFLUX_VULKAN_INCLUDE_DIR OR NOT METAFLUX_VULKAN_LIBRARY)
  message(
    FATAL_ERROR
    "METAFLUX_BUILD_VULKAN_BACKEND requires METAFLUX_VULKAN_SDK_DIR with Vulkan headers and loader"
  )
endif()

# Test executables need the pinned loader when CTest runs outside the Vulkan
# development shell. Keep only discovered store paths here; adding host
# /lib or /usr/lib ahead of the pinned runtime can mix incompatible libc
# private symbols with the Nix toolchain.
get_filename_component(METAFLUX_VULKAN_LIBRARY_DIR "${METAFLUX_VULKAN_LIBRARY}" DIRECTORY)
set(METAFLUX_VULKAN_TEST_LD_LIBRARY_PATH "${METAFLUX_VULKAN_LIBRARY_DIR}")
foreach(implicit_library_dir IN LISTS CMAKE_CXX_IMPLICIT_LINK_DIRECTORIES)
  if(implicit_library_dir MATCHES "^/nix/store/")
    string(APPEND METAFLUX_VULKAN_TEST_LD_LIBRARY_PATH ":${implicit_library_dir}")
  endif()
endforeach()

add_subdirectory(runtime)
if(METAFLUX_BUILD_COMPILER AND TARGET metaflux_compiler_core)
  add_subdirectory(compiler)
endif()
