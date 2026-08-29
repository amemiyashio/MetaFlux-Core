metaflux_add_component(
  ID backend.cpu.runtime
  ROLE backend-runtime
  OPTION METAFLUX_BUILD_CPU_BACKEND_RUNTIME
  DIRECTORY runtime
  TARGET metaflux_cpu_backend_runtime
  LANGUAGE CXX
)

metaflux_add_component(
  ID backend.cpu.compiler
  ROLE backend-compiler
  OPTION METAFLUX_BUILD_CPU_BACKEND_COMPILER
  DIRECTORY compiler
  TARGET metaflux_cpu_backend_compiler
  LANGUAGE CXX
)
