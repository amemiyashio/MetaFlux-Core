find_package(Threads REQUIRED)

add_library(
  metaflux_cpu_backend_runtime STATIC
  src/backend.cpp
  src/compiled_kernel.cpp
  src/executor.cpp
  src/interpreter.cpp
  src/placement.cpp
)
add_library(MetaFlux::CpuBackendRuntime ALIAS metaflux_cpu_backend_runtime)

target_include_directories(
  metaflux_cpu_backend_runtime
  PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)
target_link_libraries(
  metaflux_cpu_backend_runtime
  PUBLIC MetaFlux::BackendPluginApi
  PRIVATE ${CMAKE_DL_LIBS} Threads::Threads
)
metaflux_configure_target(metaflux_cpu_backend_runtime CXX)

if(BUILD_TESTING AND METAFLUX_BUILD_TESTS)
  add_subdirectory(tests)
endif()
