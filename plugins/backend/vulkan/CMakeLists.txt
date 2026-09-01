find_path(
  METAFLUX_VULKAN_INCLUDE_DIR
  vulkan/vulkan.h
  HINTS "${METAFLUX_VULKAN_SDK_DIR}/include"
)
find_library(
  METAFLUX_VULKAN_LIBRARY
  NAMES vulkan
  HINTS "${METAFLUX_VULKAN_SDK_DIR}/lib"
)
if(NOT METAFLUX_VULKAN_INCLUDE_DIR OR NOT METAFLUX_VULKAN_LIBRARY)
  message(
    FATAL_ERROR
    "METAFLUX_BUILD_VULKAN_BACKEND requires METAFLUX_VULKAN_SDK_DIR with Vulkan headers and loader"
  )
endif()

add_subdirectory(runtime)
if(METAFLUX_BUILD_COMPILER AND TARGET metaflux_compiler_core)
  add_subdirectory(compiler)
endif()
