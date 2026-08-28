metaflux_add_component(
  ID backend.cpu.runtime
  ROLE backend-runtime
  OPTION METAFLUX_BUILD_CPU_BACKEND_RUNTIME
  DIRECTORY cpu
  TARGET metaflux_cpu_backend_runtime
  LANGUAGE CXX
)
