add_executable(metafluxd main.cpp)

target_link_libraries(
  metafluxd
  PRIVATE
    MetaFlux::RuntimeCore
    MetaFlux::CompilerCore
)

if(TARGET metaflux_cpu_backend_runtime)
  target_link_libraries(metafluxd PRIVATE MetaFlux::CpuBackendRuntime)
  target_compile_definitions(metafluxd PRIVATE METAFLUX_HAVE_BUILTIN_CPU_BACKEND=1)
endif()
metaflux_configure_target(metafluxd CXX)

install(
  TARGETS metafluxd
  RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
  COMPONENT Daemon
)
