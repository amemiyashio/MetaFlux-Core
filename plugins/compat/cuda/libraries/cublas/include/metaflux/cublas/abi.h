#ifndef METAFLUX_CUBLAS_ABI_H
#define METAFLUX_CUBLAS_ABI_H

#include <stddef.h>

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
MF_CUBLAS_ABI_API cublasStatus_t cublasSgemm_v2(cublasHandle_t handle, cublasOperation_t transa,
                                                cublasOperation_t transb, int m, int n, int k,
                                                const float* alpha, const float* a, int lda,
                                                const float* b, int ldb, const float* beta,
                                                float* c, int ldc);

#ifdef __cplusplus
}
#endif

#endif
