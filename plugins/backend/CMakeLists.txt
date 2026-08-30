add_subdirectory(cpu)

if(METAFLUX_BUILD_VULKAN_BACKEND)
  add_subdirectory(vulkan)
endif()
