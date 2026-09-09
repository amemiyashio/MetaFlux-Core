#include "metaflux/cublas/abi.h"
#include "metaflux/cublas/provider.h"

#include <stdint.h>

int main(void) {
  cublasHandle_t handle = NULL;
  cudaStream_t stream = (cudaStream_t)(uintptr_t)UINT64_C(0x53545201);
  cudaStream_t observed_stream = NULL;
  cublasPointerMode_t pointer_mode = CUBLAS_POINTER_MODE_DEVICE;
  cublasMath_t math_mode = CUBLAS_TENSOR_OP_MATH;
  int version = 0;
  int property = 0;
  float alpha = 1.0F;
  float beta = 0.0F;
  float beta_one = 1.0F;
  float unsupported_alpha = 2.0F;
  float unsupported_beta = 2.0F;
  const float* left = (const float*)(uintptr_t)UINT64_C(0x10000000);
  const float* right = (const float*)(uintptr_t)UINT64_C(0x10000200);
  float* output = (float*)(uintptr_t)UINT64_C(0x10000400);
  void* workspace = (void*)(uintptr_t)UINT64_C(0x10201000);

  if (mf_cublas_provider_bootstrap_abi_version() != UINT32_C(1) ||
      cublasCreate_v2(NULL) != CUBLAS_STATUS_INVALID_VALUE ||
      cublasCreate_v2(&handle) != CUBLAS_STATUS_SUCCESS || handle == NULL ||
      cublasGetVersion_v2(handle, &version) != CUBLAS_STATUS_SUCCESS ||
      version != MF_CUBLAS_VERSION ||
      cublasGetProperty(MF_CUBLAS_LIBRARY_PROPERTY_MAJOR, &property) != CUBLAS_STATUS_SUCCESS ||
      property != 12 || cublasGetCudartVersion() != (size_t)MF_CUBLAS_CUDART_VERSION ||
      cublasSetStream_v2(handle, stream) != CUBLAS_STATUS_SUCCESS ||
      cublasGetStream_v2(handle, &observed_stream) != CUBLAS_STATUS_SUCCESS ||
      observed_stream != stream ||
      cublasSetWorkspace_v2(handle, workspace, 8519680U) != CUBLAS_STATUS_SUCCESS ||
      cublasGetPointerMode_v2(handle, &pointer_mode) != CUBLAS_STATUS_SUCCESS ||
      pointer_mode != CUBLAS_POINTER_MODE_HOST ||
      cublasSetMathMode(handle, CUBLAS_DEFAULT_MATH) != CUBLAS_STATUS_SUCCESS ||
      cublasGetMathMode(handle, &math_mode) != CUBLAS_STATUS_SUCCESS ||
      math_mode != CUBLAS_DEFAULT_MATH ||
      cublasSgemm_v2(handle, CUBLAS_OP_N, CUBLAS_OP_N, 2, 2, 2, &alpha, left, 2, right, 2, &beta,
                     output, 2) != CUBLAS_STATUS_SUCCESS ||
      cublasSgemm_v2(handle, CUBLAS_OP_N, CUBLAS_OP_N, 2, 2, 2, &alpha, left, 2, right, 2,
                     &beta_one, output, 2) != CUBLAS_STATUS_SUCCESS ||
      cublasSgemm_v2(handle, CUBLAS_OP_N, CUBLAS_OP_N, 2, 2, 2, &unsupported_alpha, left, 2, right,
                     2, &beta, output, 2) != CUBLAS_STATUS_NOT_SUPPORTED ||
      cublasSgemm_v2(handle, CUBLAS_OP_N, CUBLAS_OP_N, 2, 2, 2, &alpha, left, 2, right, 2,
                     &unsupported_beta, output, 2) != CUBLAS_STATUS_NOT_SUPPORTED ||
      cublasSetPointerMode_v2(handle, CUBLAS_POINTER_MODE_DEVICE) != CUBLAS_STATUS_SUCCESS ||
      cublasSgemm_v2(handle, CUBLAS_OP_N, CUBLAS_OP_N, 2, 2, 2, &alpha, left, 2, right, 2, &beta,
                     output, 2) != CUBLAS_STATUS_NOT_SUPPORTED ||
      cublasDestroy_v2(handle) != CUBLAS_STATUS_SUCCESS ||
      cublasDestroy_v2(handle) != CUBLAS_STATUS_NOT_INITIALIZED ||
      cublasGetVersion_v2(handle, &version) != CUBLAS_STATUS_NOT_INITIALIZED) {
    return 1;
  }
  return 0;
}
