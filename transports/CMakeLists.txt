if(METAFLUX_BUILD_MEMFD_TRANSPORT AND TARGET metaflux_runtime_core)
  add_subdirectory(memfd)
endif()

if(METAFLUX_BUILD_CDEV_TRANSPORT)
  add_subdirectory(cdev)
endif()

if(METAFLUX_BUILD_VFIO_USER_TRANSPORT)
  add_subdirectory(vfio-user)
endif()
