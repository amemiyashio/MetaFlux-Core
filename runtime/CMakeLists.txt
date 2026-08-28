if(METAFLUX_BUILD_CLIENT_FASTPATH)
  add_subdirectory(client)
endif()

if(METAFLUX_BUILD_RUNTIME_CORE)
  add_subdirectory(core)
endif()

if(METAFLUX_BUILD_RUNTIME_CORE OR METAFLUX_BUILD_CLIENT_FASTPATH)
  include(CMakePackageConfigHelpers)
  configure_package_config_file(
    "${PROJECT_SOURCE_DIR}/cmake/MetaFluxConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/MetaFluxConfig.cmake"
    INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/MetaFlux"
    NO_CHECK_REQUIRED_COMPONENTS_MACRO
  )
  write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/MetaFluxConfigVersion.cmake"
    VERSION "${PROJECT_VERSION}"
    COMPATIBILITY SameMinorVersion
  )
  install(
    EXPORT MetaFluxRuntimeTargets
    FILE MetaFluxRuntimeTargets.cmake
    NAMESPACE MetaFlux::
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/MetaFlux"
    COMPONENT Runtime
  )
  install(
    FILES
      "${CMAKE_CURRENT_BINARY_DIR}/MetaFluxConfig.cmake"
      "${CMAKE_CURRENT_BINARY_DIR}/MetaFluxConfigVersion.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/MetaFlux"
    COMPONENT Runtime
  )
endif()
