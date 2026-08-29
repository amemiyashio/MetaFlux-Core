add_library(metaflux_cuda_passthrough STATIC src/passthrough.c)
add_library(MetaFlux::CudaPassthrough ALIAS metaflux_cuda_passthrough)

set(
  METAFLUX_RUNTIME_INSTALL_ROOT
  ""
  CACHE STRING
  "Runtime install root rejected by vendor passthrough discovery; defaults to CMAKE_INSTALL_PREFIX"
)
if(METAFLUX_RUNTIME_INSTALL_ROOT STREQUAL "")
  set(metaflux_runtime_install_root "${CMAKE_INSTALL_PREFIX}")
else()
  set(metaflux_runtime_install_root "${METAFLUX_RUNTIME_INSTALL_ROOT}")
endif()
if(NOT IS_ABSOLUTE "${metaflux_runtime_install_root}")
  message(FATAL_ERROR "METAFLUX_RUNTIME_INSTALL_ROOT must be an absolute path")
endif()

set_target_properties(metaflux_cuda_passthrough PROPERTIES C_VISIBILITY_PRESET hidden)

target_include_directories(
  metaflux_cuda_passthrough
  PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)
target_compile_definitions(
  metaflux_cuda_passthrough
  PRIVATE MF_CUDA_PASSTHROUGH_INSTALL_ROOT="${metaflux_runtime_install_root}"
)
if(CMAKE_SYSROOT)
  target_link_libraries(metaflux_cuda_passthrough PRIVATE ${CMAKE_DL_LIBS})
endif()
metaflux_configure_target(metaflux_cuda_passthrough C)

if(BUILD_TESTING AND METAFLUX_BUILD_TESTS)
  add_subdirectory(tests)
endif()

install(
  TARGETS metaflux_cuda_passthrough
  ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  COMPONENT Provider
)
install(
  DIRECTORY include/
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
  COMPONENT Provider
)
