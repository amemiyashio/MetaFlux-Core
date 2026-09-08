add_subdirectory(cpu)

if(METAFLUX_BUILD_VULKAN_BACKEND)
  add_subdirectory(vulkan)
endif()

if(METAFLUX_BUILD_GPU_BACKEND_PASSTHROUGH)
  add_subdirectory(gpu)
endif()
