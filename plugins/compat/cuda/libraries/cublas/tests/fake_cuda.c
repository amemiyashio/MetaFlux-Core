#include "metaflux/client/protocol.h"

#include <stdint.h>
#include <string.h>

uint32_t mf_cuda_provider_bootstrap_abi_version(void) { return UINT32_C(1); }

int cuCtxGetCurrent(void** context) {
  if (context == NULL) {
    return 1;
  }
  *context = (void*)(uintptr_t)UINT64_C(0x4355000100000001);
  return 0;
}

int cuModuleLoadData(void** module, const void* image) {
  const mf_client_kernel_request_v1* request = (const mf_client_kernel_request_v1*)image;
  uint64_t size = 0U;
  if (module == NULL || image == NULL) {
    return 1;
  }
  size = mf_client_load_le64_v1(request->bytes + 8);
  if (mf_client_kernel_request_validate_v1((const uint8_t*)image, size) != MF_CLIENT_CONTROL_OK ||
      mf_client_load_le32_v1(request->bytes + 20) !=
          MF_CLIENT_KERNEL_REQUEST_OPERATION_MATMUL_F32_V1) {
    return 200;
  }
  *module = (void*)(uintptr_t)UINT64_C(0x4d4f4401);
  return 0;
}

int cuModuleGetFunction(void** function, void* module, const char* name) {
  if (function == NULL || module != (void*)(uintptr_t)UINT64_C(0x4d4f4401) || name == NULL ||
      strcmp(name, "metaflux_cublas_sgemm_f32") != 0) {
    return 1;
  }
  *function = (void*)(uintptr_t)UINT64_C(0x46554e01);
  return 0;
}

int cuModuleUnload(void* module) {
  return module == (void*)(uintptr_t)UINT64_C(0x4d4f4401) ? 0 : 1;
}

int cuLaunchKernel(void* function, unsigned int grid_x, unsigned int grid_y, unsigned int grid_z,
                   unsigned int block_x, unsigned int block_y, unsigned int block_z,
                   unsigned int shared_memory_bytes, void* stream, void** parameters,
                   void** extra) {
  uint32_t index = 0U;
  (void)stream;
  if (function != (void*)(uintptr_t)UINT64_C(0x46554e01) || grid_x != 1U || grid_y != 1U ||
      grid_z != 1U || block_x != 8U || block_y != 8U || block_z != 1U ||
      shared_memory_bytes != 0U || parameters == NULL || extra != NULL) {
    return 1;
  }
  for (index = 0U; index < 12U; ++index) {
    if (parameters[index] == NULL) {
      return 1;
    }
  }
  return *(const uint32_t*)parameters[3] == 4U && *(const uint32_t*)parameters[6] == 2U &&
                 *(const uint32_t*)parameters[7] == 2U && *(const uint32_t*)parameters[8] == 2U
             ? 0
             : 1;
}
