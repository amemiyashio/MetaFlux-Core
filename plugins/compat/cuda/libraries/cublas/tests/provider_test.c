#include "metaflux/cublas/abi.h"
#include "metaflux/cublas/provider.h"

#include <stdint.h>

int main(void) {
  cublasHandle_t handle = NULL;
  cublasLtMatmulDesc_t lt_descriptor = NULL;
  cublasLtMatrixLayout_t lt_left_layout = NULL;
  cublasLtMatrixLayout_t lt_right_layout = NULL;
  cublasLtMatrixLayout_t lt_output_layout = NULL;
  cublasLtMatmulPreference_t lt_preference = NULL;
  cublasLtMatmulHeuristicResult_t lt_result = {0};
  cublasLtMatmulAlgo_t invalid_algorithm = {{0}};
  int lt_result_count = 0;
  cublasOperation_t lt_transpose = CUBLAS_OP_T;
  cublasOperation_t lt_no_transpose = CUBLAS_OP_N;
  cublasLtEpilogue_t lt_epilogue = CUBLASLT_EPILOGUE_BIAS;
  uint64_t lt_workspace_limit = UINT64_C(1048576);
  uint32_t lt_alignment = UINT32_C(256);
  uint32_t lt_invalid_alignment = UINT32_C(3);
  cublasLtOrder_t lt_row_order = CUBLASLT_ORDER_ROW;
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
  const float* bias = (const float*)(uintptr_t)UINT64_C(0x10000600);
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
  if (cublasCreate_v2(&handle) != CUBLAS_STATUS_SUCCESS) {
    return 2;
  }
  if (sizeof(cublasLtMatmulAlgo_t) != 64U ||
      sizeof(cublasLtMatmulHeuristicResult_t) != 96U) {
    return 3;
  }
  if (cublasLtMatmulDescCreate(NULL, CUBLAS_COMPUTE_32F, CUDA_R_32F) !=
          CUBLAS_STATUS_INVALID_VALUE ||
      cublasLtMatmulDescCreate(&lt_descriptor, CUBLAS_COMPUTE_32F, CUDA_R_32F) !=
          CUBLAS_STATUS_SUCCESS ||
      cublasLtMatrixLayoutCreate(&lt_left_layout, CUDA_R_32F, 2U, 2U, 2) !=
          CUBLAS_STATUS_SUCCESS ||
      cublasLtMatrixLayoutCreate(&lt_right_layout, CUDA_R_32F, 2U, 2U, 2) !=
          CUBLAS_STATUS_SUCCESS ||
      cublasLtMatrixLayoutCreate(&lt_output_layout, CUDA_R_32F, 2U, 2U, 2) !=
          CUBLAS_STATUS_SUCCESS ||
      cublasLtMatmulPreferenceCreate(&lt_preference) != CUBLAS_STATUS_SUCCESS) {
    return 4;
  }
  if (cublasLtMatmulDescSetAttribute(lt_descriptor, CUBLASLT_MATMUL_DESC_TRANSA,
                                     &lt_transpose, sizeof(lt_transpose)) !=
          CUBLAS_STATUS_SUCCESS ||
      cublasLtMatmulDescSetAttribute(lt_descriptor, CUBLASLT_MATMUL_DESC_TRANSB,
                                     &lt_no_transpose, sizeof(lt_no_transpose)) !=
          CUBLAS_STATUS_SUCCESS ||
      cublasLtMatmulDescSetAttribute(lt_descriptor, CUBLASLT_MATMUL_DESC_EPILOGUE,
                                     &lt_epilogue, sizeof(lt_epilogue)) !=
          CUBLAS_STATUS_SUCCESS ||
      cublasLtMatmulDescSetAttribute(lt_descriptor, CUBLASLT_MATMUL_DESC_BIAS_POINTER,
                                     &bias, sizeof(bias)) != CUBLAS_STATUS_SUCCESS ||
      cublasLtMatmulPreferenceSetAttribute(
          lt_preference, CUBLASLT_MATMUL_PREF_MAX_WORKSPACE_BYTES,
          &lt_workspace_limit, sizeof(lt_workspace_limit)) != CUBLAS_STATUS_SUCCESS ||
      cublasLtMatmulPreferenceSetAttribute(
          lt_preference, CUBLASLT_MATMUL_PREF_MIN_ALIGNMENT_A_BYTES,
          &lt_alignment, sizeof(lt_alignment)) != CUBLAS_STATUS_SUCCESS ||
      cublasLtMatmulPreferenceSetAttribute(
          lt_preference, CUBLASLT_MATMUL_PREF_MIN_ALIGNMENT_B_BYTES,
          &lt_alignment, sizeof(lt_alignment)) != CUBLAS_STATUS_SUCCESS ||
      cublasLtMatmulPreferenceSetAttribute(
          lt_preference, CUBLASLT_MATMUL_PREF_MIN_ALIGNMENT_C_BYTES,
          &lt_alignment, sizeof(lt_alignment)) != CUBLAS_STATUS_SUCCESS ||
      cublasLtMatmulPreferenceSetAttribute(
          lt_preference, CUBLASLT_MATMUL_PREF_MIN_ALIGNMENT_D_BYTES,
          &lt_alignment, sizeof(lt_alignment)) != CUBLAS_STATUS_SUCCESS) {
    return 5;
  }
  if (cublasLtMatmulAlgoGetHeuristic(
          (cublasLtHandle_t)(void*)handle, lt_descriptor, lt_left_layout,
          lt_right_layout, lt_output_layout, lt_output_layout, lt_preference,
          1, &lt_result, &lt_result_count) != CUBLAS_STATUS_SUCCESS ||
      lt_result_count != 1 || lt_result.state != CUBLAS_STATUS_SUCCESS ||
      lt_result.workspaceSize != 0U) {
    return 6;
  }
  if (cublasLtMatmul((cublasLtHandle_t)(void*)handle, lt_descriptor, &alpha,
                     left, lt_left_layout, right, lt_right_layout, &beta,
                     output, lt_output_layout, output, lt_output_layout,
                     &lt_result.algo, workspace, (size_t)lt_workspace_limit,
                     stream) != CUBLAS_STATUS_SUCCESS) {
    return 7;
  }
  if (cublasLtMatmul((cublasLtHandle_t)(void*)handle, lt_descriptor, &alpha,
                     left, lt_left_layout, right, lt_right_layout, &beta_one,
                     output, lt_output_layout, output, lt_output_layout,
                     &lt_result.algo, workspace, (size_t)lt_workspace_limit,
                     stream) != CUBLAS_STATUS_NOT_SUPPORTED ||
      cublasLtMatmul((cublasLtHandle_t)(void*)handle, lt_descriptor, &alpha,
                     left, lt_left_layout, right, lt_right_layout, &beta,
                     output, lt_output_layout, output, lt_output_layout,
                     &invalid_algorithm, workspace, (size_t)lt_workspace_limit,
                     stream) != CUBLAS_STATUS_INVALID_VALUE ||
      cublasLtMatmulDescSetAttribute(lt_descriptor, CUBLASLT_MATMUL_DESC_TRANSA,
                                     &lt_transpose, 1U) != CUBLAS_STATUS_INVALID_VALUE ||
      cublasLtMatrixLayoutSetAttribute(lt_left_layout,
                                       CUBLASLT_MATRIX_LAYOUT_ORDER,
                                       &lt_row_order, sizeof(lt_row_order)) !=
          CUBLAS_STATUS_NOT_SUPPORTED ||
      cublasLtMatmulPreferenceSetAttribute(
          lt_preference, CUBLASLT_MATMUL_PREF_MIN_ALIGNMENT_A_BYTES,
          &lt_invalid_alignment, sizeof(lt_invalid_alignment)) !=
          CUBLAS_STATUS_INVALID_VALUE ||
      cublasLtMatmulAlgoGetHeuristic(
          (cublasLtHandle_t)(void*)handle, lt_descriptor, lt_left_layout,
          lt_right_layout, lt_output_layout, lt_output_layout, lt_preference,
          0, &lt_result, &lt_result_count) != CUBLAS_STATUS_INVALID_VALUE) {
    return 8;
  }
  if (cublasLtMatmulPreferenceDestroy(lt_preference) != CUBLAS_STATUS_SUCCESS ||
      cublasLtMatmulPreferenceDestroy(lt_preference) != CUBLAS_STATUS_INVALID_VALUE ||
      cublasLtMatrixLayoutDestroy(lt_left_layout) != CUBLAS_STATUS_SUCCESS ||
      cublasLtMatrixLayoutDestroy(lt_left_layout) != CUBLAS_STATUS_INVALID_VALUE ||
      cublasLtMatrixLayoutDestroy(lt_right_layout) != CUBLAS_STATUS_SUCCESS ||
      cublasLtMatrixLayoutDestroy(lt_output_layout) != CUBLAS_STATUS_SUCCESS ||
      cublasLtMatmulDescDestroy(lt_descriptor) != CUBLAS_STATUS_SUCCESS ||
      cublasLtMatmulDescDestroy(lt_descriptor) != CUBLAS_STATUS_INVALID_VALUE ||
      cublasDestroy_v2(handle) != CUBLAS_STATUS_SUCCESS) {
    return 9;
  }
  return 0;
}
