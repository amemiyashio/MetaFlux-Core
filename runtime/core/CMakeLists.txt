add_library(
  metaflux_runtime_core STATIC
  src/lifecycle.cpp
  src/recovery_model.cpp
  src/registry_recovery.cpp
  src/runtime.cpp
)
add_library(MetaFlux::RuntimeCore ALIAS metaflux_runtime_core)
set_target_properties(metaflux_runtime_core PROPERTIES EXPORT_NAME RuntimeCore)

target_include_directories(
  metaflux_runtime_core
  PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)
target_link_libraries(
  metaflux_runtime_core
  PUBLIC MetaFlux::ClientProtocol MetaFlux::SharedDeviceLayout
)
target_compile_features(metaflux_runtime_core PUBLIC cxx_std_20)
metaflux_configure_target(metaflux_runtime_core CXX)
metaflux_register_component_graph_entry(
  metaflux_runtime_core
  runtime.core
  runtime-core
  CXX
)

if(BUILD_TESTING AND METAFLUX_BUILD_TESTS)
  add_subdirectory(tests)
endif()

install(
  TARGETS metaflux_runtime_core
  EXPORT MetaFluxRuntimeTargets
  ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  COMPONENT Runtime
)
install(
  DIRECTORY include/
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
  COMPONENT Runtime
)
