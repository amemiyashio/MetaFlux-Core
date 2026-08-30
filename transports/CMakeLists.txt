if(METAFLUX_BUILD_CDEV_TRANSPORT)
  add_subdirectory(cdev)
endif()

if(METAFLUX_BUILD_VFIO_USER_TRANSPORT)
  add_subdirectory(vfio-user)
endif()
