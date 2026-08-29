set(METAFLUX_BUILD_CUDA_PASSTHROUGH_HELPER OFF)
if(METAFLUX_BUILD_CUDA_DRIVER_PROVIDER OR METAFLUX_BUILD_NVML_PROVIDER)
  set(METAFLUX_BUILD_CUDA_PASSTHROUGH_HELPER ON)
endif()

metaflux_add_component(
  ID compat.cuda.passthrough
  ROLE passthrough-helper
  OPTION METAFLUX_BUILD_CUDA_PASSTHROUGH_HELPER
  DIRECTORY passthrough
  TARGET metaflux_cuda_passthrough
  LANGUAGE C
)

metaflux_add_component(
  ID compat.cuda.driver
  ROLE compat-provider
  OPTION METAFLUX_BUILD_CUDA_DRIVER_PROVIDER
  DIRECTORY abi/driver
  TARGET metaflux_cuda_provider
  LANGUAGE C
)

metaflux_add_component(
  ID compat.cuda.nvml
  ROLE management-provider
  OPTION METAFLUX_BUILD_NVML_PROVIDER
  DIRECTORY management/nvml
  TARGET metaflux_nvml_provider
  LANGUAGE C
)

metaflux_add_component(
  ID compat.cuda.ptx
  ROLE compiler-frontend
  OPTION METAFLUX_BUILD_CUDA_PTX_FRONTEND
  DIRECTORY compiler/ptx
  TARGET metaflux_cuda_ptx_frontend
  LANGUAGE CXX
)
