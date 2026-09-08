metaflux_add_component(
  ID backend.gpu.passthrough
  ROLE backend-runtime
  OPTION METAFLUX_BUILD_GPU_BACKEND_PASSTHROUGH
  DIRECTORY src
  TARGET metaflux_gpu_backend_passthrough
  LANGUAGE CXX
)
