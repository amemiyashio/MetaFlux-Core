#ifndef METAFLUX_CUBLAS_ABI_H
#define METAFLUX_CUBLAS_ABI_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__clang__) || defined(__GNUC__)
#define MF_CUBLAS_ABI_API __attribute__((visibility("default")))
#else
#define MF_CUBLAS_ABI_API
#endif

#define MF_CUBLAS_VERSION 120604
#define MF_CUBLAS_CUDART_VERSION 12060

typedef enum mf_cublas_status {
  CUBLAS_STATUS_SUCCESS = 0,
  CUBLAS_STATUS_NOT_INITIALIZED = 1,
  CUBLAS_STATUS_ALLOC_FAILED = 3,
  CUBLAS_STATUS_INVALID_VALUE = 7,
  CUBLAS_STATUS_ARCH_MISMATCH = 8,
  CUBLAS_STATUS_MAPPING_ERROR = 11,
  CUBLAS_STATUS_EXECUTION_FAILED = 13,
  CUBLAS_STATUS_INTERNAL_ERROR = 14,
  CUBLAS_STATUS_NOT_SUPPORTED = 15,
  CUBLAS_STATUS_LICENSE_ERROR = 16
} cublasStatus_t;

typedef enum mf_cublas_operation {
  CUBLAS_OP_N = 0,
  CUBLAS_OP_T = 1,
  CUBLAS_OP_C = 2,
  CUBLAS_OP_CONJG = 3
} cublasOperation_t;

typedef enum mf_cublas_pointer_mode {
  CUBLAS_POINTER_MODE_HOST = 0,
  CUBLAS_POINTER_MODE_DEVICE = 1
} cublasPointerMode_t;

typedef enum mf_cublas_math {
  CUBLAS_DEFAULT_MATH = 0,
  CUBLAS_TENSOR_OP_MATH = 1,
  CUBLAS_PEDANTIC_MATH = 2,
  CUBLAS_TF32_TENSOR_OP_MATH = 3,
  CUBLAS_MATH_DISALLOW_REDUCED_PRECISION_REDUCTION = 16
} cublasMath_t;

typedef enum mf_cuda_data_type {
  CUDA_R_32F = 0
} cudaDataType_t;

typedef enum mf_cublas_compute_type {
  CUBLAS_COMPUTE_32F = 68
} cublasComputeType_t;

typedef struct cublasLtContext* cublasLtHandle_t;
typedef struct mf_cublas_lt_matrix_layout* cublasLtMatrixLayout_t;
typedef struct mf_cublas_lt_matmul_desc* cublasLtMatmulDesc_t;
typedef struct mf_cublas_lt_matmul_preference* cublasLtMatmulPreference_t;

typedef struct mf_cublas_lt_matmul_algo {
  uint64_t data[8];
} cublasLtMatmulAlgo_t;

typedef struct mf_cublas_lt_matmul_heuristic_result {
  cublasLtMatmulAlgo_t algo;
  size_t workspaceSize;
  cublasStatus_t state;
  float wavesCount;
  int reserved[4];
} cublasLtMatmulHeuristicResult_t;

typedef enum mf_cublas_lt_order {
  CUBLASLT_ORDER_COL = 0,
  CUBLASLT_ORDER_ROW = 1
} cublasLtOrder_t;

typedef enum mf_cublas_lt_matmul_desc_attribute {
  CUBLASLT_MATMUL_DESC_COMPUTE_TYPE = 0,
  CUBLASLT_MATMUL_DESC_SCALE_TYPE = 1,
  CUBLASLT_MATMUL_DESC_POINTER_MODE = 2,
  CUBLASLT_MATMUL_DESC_TRANSA = 3,
  CUBLASLT_MATMUL_DESC_TRANSB = 4,
  CUBLASLT_MATMUL_DESC_TRANSC = 5,
  CUBLASLT_MATMUL_DESC_FILL_MODE = 6,
  CUBLASLT_MATMUL_DESC_EPILOGUE = 7,
  CUBLASLT_MATMUL_DESC_BIAS_POINTER = 8
} cublasLtMatmulDescAttributes_t;

typedef enum mf_cublas_lt_matrix_layout_attribute {
  CUBLASLT_MATRIX_LAYOUT_TYPE = 0,
  CUBLASLT_MATRIX_LAYOUT_ORDER = 1,
  CUBLASLT_MATRIX_LAYOUT_ROWS = 2,
  CUBLASLT_MATRIX_LAYOUT_COLS = 3,
  CUBLASLT_MATRIX_LAYOUT_LD = 4,
  CUBLASLT_MATRIX_LAYOUT_BATCH_COUNT = 5,
  CUBLASLT_MATRIX_LAYOUT_STRIDED_BATCH_OFFSET = 6
} cublasLtMatrixLayoutAttribute_t;

typedef enum mf_cublas_lt_matmul_preference_attribute {
  CUBLASLT_MATMUL_PREF_SEARCH_MODE = 0,
  CUBLASLT_MATMUL_PREF_MAX_WORKSPACE_BYTES = 1,
  CUBLASLT_MATMUL_PREF_REDUCTION_SCHEME_MASK = 3,
  CUBLASLT_MATMUL_PREF_MIN_ALIGNMENT_A_BYTES = 5,
  CUBLASLT_MATMUL_PREF_MIN_ALIGNMENT_B_BYTES = 6,
  CUBLASLT_MATMUL_PREF_MIN_ALIGNMENT_C_BYTES = 7,
  CUBLASLT_MATMUL_PREF_MIN_ALIGNMENT_D_BYTES = 8
} cublasLtMatmulPreferenceAttributes_t;

typedef enum mf_cublas_lt_epilogue {
  CUBLASLT_EPILOGUE_DEFAULT = 1,
  CUBLASLT_EPILOGUE_RELU = 2,
  CUBLASLT_EPILOGUE_BIAS = 4,
  CUBLASLT_EPILOGUE_RELU_BIAS = 6
} cublasLtEpilogue_t;

typedef enum mf_cublas_library_property {
  MF_CUBLAS_LIBRARY_PROPERTY_MAJOR = 0,
  MF_CUBLAS_LIBRARY_PROPERTY_MINOR = 1,
  MF_CUBLAS_LIBRARY_PROPERTY_PATCH = 2
} mf_cublas_library_property;

struct cublasContext;
typedef struct cublasContext* cublasHandle_t;
typedef struct CUstream_st* cudaStream_t;

MF_CUBLAS_ABI_API cublasStatus_t cublasCreate_v2(cublasHandle_t* handle);
MF_CUBLAS_ABI_API cublasStatus_t cublasDestroy_v2(cublasHandle_t handle);
MF_CUBLAS_ABI_API cublasStatus_t cublasGetVersion_v2(cublasHandle_t handle, int* version);
MF_CUBLAS_ABI_API cublasStatus_t cublasGetProperty(mf_cublas_library_property property, int* value);
MF_CUBLAS_ABI_API size_t cublasGetCudartVersion(void);
MF_CUBLAS_ABI_API cublasStatus_t cublasSetWorkspace_v2(cublasHandle_t handle, void* workspace,
                                                       size_t workspace_size);
MF_CUBLAS_ABI_API cublasStatus_t cublasSetStream_v2(cublasHandle_t handle, cudaStream_t stream);
MF_CUBLAS_ABI_API cublasStatus_t cublasGetStream_v2(cublasHandle_t handle, cudaStream_t* stream);
MF_CUBLAS_ABI_API cublasStatus_t cublasGetPointerMode_v2(cublasHandle_t handle,
                                                         cublasPointerMode_t* mode);
MF_CUBLAS_ABI_API cublasStatus_t cublasSetPointerMode_v2(cublasHandle_t handle,
                                                         cublasPointerMode_t mode);
MF_CUBLAS_ABI_API cublasStatus_t cublasGetMathMode(cublasHandle_t handle, cublasMath_t* mode);
MF_CUBLAS_ABI_API cublasStatus_t cublasSetMathMode(cublasHandle_t handle, cublasMath_t mode);
MF_CUBLAS_ABI_API const char* cublasGetStatusName(cublasStatus_t status);
MF_CUBLAS_ABI_API const char* cublasGetStatusString(cublasStatus_t status);
MF_CUBLAS_ABI_API cublasStatus_t
cublasLtMatmulDescCreate(cublasLtMatmulDesc_t* descriptor,
                         cublasComputeType_t compute_type, cudaDataType_t scale_type);
MF_CUBLAS_ABI_API cublasStatus_t
cublasLtMatmulDescDestroy(cublasLtMatmulDesc_t descriptor);
MF_CUBLAS_ABI_API cublasStatus_t cublasLtMatmulDescSetAttribute(
    cublasLtMatmulDesc_t descriptor, cublasLtMatmulDescAttributes_t attribute,
    const void* buffer, size_t size);
MF_CUBLAS_ABI_API cublasStatus_t cublasLtMatrixLayoutCreate(
    cublasLtMatrixLayout_t* layout, cudaDataType_t type, uint64_t rows,
    uint64_t columns, int64_t leading);
MF_CUBLAS_ABI_API cublasStatus_t
cublasLtMatrixLayoutDestroy(cublasLtMatrixLayout_t layout);
MF_CUBLAS_ABI_API cublasStatus_t cublasLtMatrixLayoutSetAttribute(
    cublasLtMatrixLayout_t layout, cublasLtMatrixLayoutAttribute_t attribute,
    const void* buffer, size_t size);
MF_CUBLAS_ABI_API cublasStatus_t
cublasLtMatmulPreferenceCreate(cublasLtMatmulPreference_t* preference);
MF_CUBLAS_ABI_API cublasStatus_t
cublasLtMatmulPreferenceDestroy(cublasLtMatmulPreference_t preference);
MF_CUBLAS_ABI_API cublasStatus_t cublasLtMatmulPreferenceSetAttribute(
    cublasLtMatmulPreference_t preference,
    cublasLtMatmulPreferenceAttributes_t attribute, const void* buffer,
    size_t size);
MF_CUBLAS_ABI_API cublasStatus_t cublasLtMatmulAlgoGetHeuristic(
    cublasLtHandle_t handle, cublasLtMatmulDesc_t descriptor,
    cublasLtMatrixLayout_t a_layout, cublasLtMatrixLayout_t b_layout,
    cublasLtMatrixLayout_t c_layout, cublasLtMatrixLayout_t d_layout,
    cublasLtMatmulPreference_t preference, int requested_count,
    cublasLtMatmulHeuristicResult_t results[], int* returned_count);
MF_CUBLAS_ABI_API cublasStatus_t cublasLtMatmul(
    cublasLtHandle_t handle, cublasLtMatmulDesc_t descriptor,
    const void* alpha, const void* a, cublasLtMatrixLayout_t a_layout,
    const void* b, cublasLtMatrixLayout_t b_layout, const void* beta,
    const void* c, cublasLtMatrixLayout_t c_layout, void* d,
    cublasLtMatrixLayout_t d_layout, const cublasLtMatmulAlgo_t* algorithm,
    void* workspace, size_t workspace_size, cudaStream_t stream);
MF_CUBLAS_ABI_API cublasStatus_t cublasSgemm_v2(cublasHandle_t handle, cublasOperation_t transa,
                                                cublasOperation_t transb, int m, int n, int k,
                                                const float* alpha, const float* a, int lda,
                                                const float* b, int ldb, const float* beta,
                                                float* c, int ldc);

#ifdef __cplusplus
}
#endif

#endif
