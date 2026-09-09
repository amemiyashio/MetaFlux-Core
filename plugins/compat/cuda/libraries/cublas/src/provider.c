#define _GNU_SOURCE

#include "metaflux/cublas/provider.h"
#include "metaflux/cublas/abi.h"

#include "metaflux/client/protocol.h"

#include <dlfcn.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MF_CUBLAS_HANDLE_MAGIC UINT32_C(0x4d46424c)
#define MF_CUBLAS_LT_DESC_MAGIC UINT32_C(0x4d464c44)
#define MF_CUBLAS_LT_LAYOUT_MAGIC UINT32_C(0x4d464c4c)
#define MF_CUBLAS_LT_PREFERENCE_MAGIC UINT32_C(0x4d464c50)
#define MF_CUBLAS_LT_ALGORITHM_MAGIC UINT64_C(0x4d464c414c474f31)
#define MF_CUBLAS_DRIVER_BOOTSTRAP_ABI UINT32_C(1)
#define MF_CUBLAS_KERNEL_ENTRY "metaflux_cublas_sgemm_f32"
#define MF_CUBLAS_LT_BIAS_KERNEL_ENTRY "metaflux_cublas_lt_matmul_bias_f32"

typedef int mf_cu_result;
typedef void* mf_cu_context;
typedef void* mf_cu_module;
typedef void* mf_cu_function;
typedef unsigned long long mf_cu_device_pointer;

typedef uint32_t (*mf_cuda_bootstrap_fn)(void);
typedef mf_cu_result (*mf_cu_ctx_get_current_fn)(mf_cu_context* context);
typedef mf_cu_result (*mf_cu_module_load_data_fn)(mf_cu_module* module, const void* image);
typedef mf_cu_result (*mf_cu_module_get_function_fn)(mf_cu_function* function, mf_cu_module module,
                                                     const char* name);
typedef mf_cu_result (*mf_cu_module_unload_fn)(mf_cu_module module);
typedef mf_cu_result (*mf_cu_launch_kernel_fn)(
    mf_cu_function function, unsigned int grid_x, unsigned int grid_y, unsigned int grid_z,
    unsigned int block_x, unsigned int block_y, unsigned int block_z,
    unsigned int shared_memory_bytes, cudaStream_t stream, void** kernel_parameters, void** extra);

typedef struct mf_cublas_driver {
  void* library;
  mf_cu_ctx_get_current_fn ctx_get_current;
  mf_cu_module_load_data_fn module_load_data;
  mf_cu_module_get_function_fn module_get_function;
  mf_cu_module_unload_fn module_unload;
  mf_cu_launch_kernel_fn launch_kernel;
} mf_cublas_driver;

struct cublasContext {
  uint32_t magic;
  cudaStream_t stream;
  void* workspace;
  size_t workspace_size;
  cublasPointerMode_t pointer_mode;
  cublasMath_t math_mode;
  mf_cublas_driver driver;
  mf_cu_module module;
  mf_cu_function function;
  mf_cu_module lt_bias_module;
  mf_cu_function lt_bias_function;
  struct cublasContext* next;
};

struct mf_cublas_lt_matmul_desc {
  uint32_t magic;
  cublasComputeType_t compute_type;
  cudaDataType_t scale_type;
  cublasOperation_t transa;
  cublasOperation_t transb;
  cublasLtEpilogue_t epilogue;
  const void* bias;
  struct mf_cublas_lt_matmul_desc* next;
};

struct mf_cublas_lt_matrix_layout {
  uint32_t magic;
  cudaDataType_t type;
  uint64_t rows;
  uint64_t columns;
  int64_t leading;
  cublasLtOrder_t order;
  int32_t batch_count;
  int64_t batch_stride;
  struct mf_cublas_lt_matrix_layout* next;
};

struct mf_cublas_lt_matmul_preference {
  uint32_t magic;
  uint64_t workspace_size;
  uint32_t minimum_alignment[4];
  struct mf_cublas_lt_matmul_preference* next;
};

static pthread_mutex_t mf_cublas_mutex = PTHREAD_MUTEX_INITIALIZER;
static struct cublasContext* mf_cublas_handles;
static struct mf_cublas_lt_matmul_desc* mf_cublas_lt_descriptors;
static struct mf_cublas_lt_matrix_layout* mf_cublas_lt_layouts;
static struct mf_cublas_lt_matmul_preference* mf_cublas_lt_preferences;

static const char mf_cublas_sgemm_ptx[] = ".version 9.0\n"
                                          ".target sm_70\n"
                                          ".address_size 64\n"
                                          ".visible .entry " MF_CUBLAS_KERNEL_ENTRY "(\n"
                                          "  .param .u64 destination,\n"
                                          "  .param .u64 left,\n"
                                          "  .param .u64 right,\n"
                                          "  .param .u32 element_count,\n"
                                          "  .param .u32 transpose_left,\n"
                                          "  .param .u32 transpose_right,\n"
                                          "  .param .u32 rows,\n"
                                          "  .param .u32 columns,\n"
                                          "  .param .u32 inner,\n"
                                          "  .param .u32 leading_left,\n"
                                          "  .param .u32 leading_right,\n"
                                          "  .param .u32 leading_destination,\n"
                                          "  .param .u32 alpha_bits,\n"
                                          "  .param .u32 beta_bits\n"
                                          ")\n"
                                          "{\n"
                                          "  ret;\n"
                                          "}\n";

static const char mf_cublas_lt_bias_ptx[] =
    ".version 9.0\n"
    ".target sm_70\n"
    ".address_size 64\n"
    ".visible .entry " MF_CUBLAS_LT_BIAS_KERNEL_ENTRY "(\n"
    "  .param .u64 destination,\n"
    "  .param .u64 left,\n"
    "  .param .u64 right,\n"
    "  .param .u64 bias,\n"
    "  .param .u32 element_count,\n"
    "  .param .u32 transpose_left,\n"
    "  .param .u32 transpose_right,\n"
    "  .param .u32 rows,\n"
    "  .param .u32 columns,\n"
    "  .param .u32 inner,\n"
    "  .param .u32 leading_left,\n"
    "  .param .u32 leading_right,\n"
    "  .param .u32 leading_destination,\n"
    "  .param .u32 alpha_bits,\n"
    "  .param .u32 beta_bits,\n"
    "  .param .u32 epilogue\n"
    ")\n"
    "{\n"
    "  ret;\n"
    "}\n";

static int mf_cublas_trace_enabled(void) {
  static int enabled = -1;
  if (enabled < 0) {
    enabled = getenv("METAFLUX_TRACE_STUBS") != NULL ? 1 : 0;
  }
  return enabled;
}

static int mf_cublas_resolve(void* library, const char* name, void* destination,
                             size_t destination_size) {
  void* symbol = NULL;
  if (library == NULL || name == NULL || destination == NULL ||
      destination_size != sizeof(symbol)) {
    return 0;
  }
  symbol = dlsym(library, name);
  if (symbol == NULL) {
    return 0;
  }
  (void)memcpy(destination, &symbol, sizeof(symbol));
  return 1;
}

static void mf_cublas_driver_close(mf_cublas_driver* driver) {
  if (driver != NULL && driver->library != NULL) {
    (void)dlclose(driver->library);
    (void)memset(driver, 0, sizeof(*driver));
  }
}

static cublasStatus_t mf_cublas_driver_open(mf_cublas_driver* driver) {
  mf_cuda_bootstrap_fn bootstrap = NULL;
  mf_cu_context current = NULL;
  if (driver == NULL) {
    return CUBLAS_STATUS_INVALID_VALUE;
  }
  (void)memset(driver, 0, sizeof(*driver));
  driver->library = dlopen("libcuda.so.1", RTLD_NOW | RTLD_LOCAL);
  if (driver->library == NULL ||
      !mf_cublas_resolve(driver->library, "mf_cuda_provider_bootstrap_abi_version", &bootstrap,
                         sizeof(bootstrap)) ||
      bootstrap() != MF_CUBLAS_DRIVER_BOOTSTRAP_ABI ||
      !mf_cublas_resolve(driver->library, "cuCtxGetCurrent", &driver->ctx_get_current,
                         sizeof(driver->ctx_get_current)) ||
      !mf_cublas_resolve(driver->library, "cuModuleLoadData", &driver->module_load_data,
                         sizeof(driver->module_load_data)) ||
      !mf_cublas_resolve(driver->library, "cuModuleGetFunction", &driver->module_get_function,
                         sizeof(driver->module_get_function)) ||
      !mf_cublas_resolve(driver->library, "cuModuleUnload", &driver->module_unload,
                         sizeof(driver->module_unload)) ||
      !mf_cublas_resolve(driver->library, "cuLaunchKernel", &driver->launch_kernel,
                         sizeof(driver->launch_kernel)) ||
      driver->ctx_get_current(&current) != 0 || current == NULL) {
    mf_cublas_driver_close(driver);
    return CUBLAS_STATUS_NOT_INITIALIZED;
  }
  return CUBLAS_STATUS_SUCCESS;
}

static struct cublasContext* mf_cublas_find_locked(cublasHandle_t handle) {
  struct cublasContext* current = mf_cublas_handles;
  while (current != NULL) {
    if (current == handle && current->magic == MF_CUBLAS_HANDLE_MAGIC) {
      return current;
    }
    current = current->next;
  }
  return NULL;
}

static struct mf_cublas_lt_matmul_desc*
mf_cublas_lt_find_descriptor_locked(cublasLtMatmulDesc_t descriptor) {
  struct mf_cublas_lt_matmul_desc* current = mf_cublas_lt_descriptors;
  while (current != NULL) {
    if (current == descriptor && current->magic == MF_CUBLAS_LT_DESC_MAGIC) {
      return current;
    }
    current = current->next;
  }
  return NULL;
}

static struct mf_cublas_lt_matrix_layout*
mf_cublas_lt_find_layout_locked(cublasLtMatrixLayout_t layout) {
  struct mf_cublas_lt_matrix_layout* current = mf_cublas_lt_layouts;
  while (current != NULL) {
    if (current == layout && current->magic == MF_CUBLAS_LT_LAYOUT_MAGIC) {
      return current;
    }
    current = current->next;
  }
  return NULL;
}

static struct mf_cublas_lt_matmul_preference*
mf_cublas_lt_find_preference_locked(cublasLtMatmulPreference_t preference) {
  struct mf_cublas_lt_matmul_preference* current = mf_cublas_lt_preferences;
  while (current != NULL) {
    if (current == preference && current->magic == MF_CUBLAS_LT_PREFERENCE_MAGIC) {
      return current;
    }
    current = current->next;
  }
  return NULL;
}

static cublasStatus_t mf_cublas_map_driver_status(mf_cu_result result) {
  switch (result) {
  case 0:
    return CUBLAS_STATUS_SUCCESS;
  case 1:
  case 201:
  case 400:
    return CUBLAS_STATUS_INVALID_VALUE;
  case 2:
    return CUBLAS_STATUS_ALLOC_FAILED;
  case 3:
  case 4:
  case 100:
    return CUBLAS_STATUS_NOT_INITIALIZED;
  case 801:
    return CUBLAS_STATUS_NOT_SUPPORTED;
  default:
    return CUBLAS_STATUS_EXECUTION_FAILED;
  }
}

static int mf_cublas_operation_supported(cublasOperation_t operation) {
  return operation == CUBLAS_OP_N || operation == CUBLAS_OP_T || operation == CUBLAS_OP_C;
}

static int mf_cublas_matrix_span(uint32_t rows, uint32_t columns, uint32_t leading,
                                 uint32_t* span) {
  const uint64_t required =
      columns == UINT32_C(0) ? UINT64_C(0) : (uint64_t)(columns - UINT32_C(1)) * leading + rows;
  if (span == NULL || rows == UINT32_C(0) || columns == UINT32_C(0) || leading < rows ||
      required > UINT32_MAX) {
    return 0;
  }
  *span = (uint32_t)required;
  return 1;
}

static cublasStatus_t mf_cublas_prepare_sgemm_locked(struct cublasContext* handle) {
  uint8_t payload[MF_CLIENT_KERNEL_REQUEST_HEADER_SIZE_V1 + sizeof(mf_cublas_sgemm_ptx) - 1U];
  mf_client_kernel_request_v1* request = (mf_client_kernel_request_v1*)payload;
  mf_cu_result result = 0;
  if (handle->module != NULL && handle->function != NULL) {
    return CUBLAS_STATUS_SUCCESS;
  }
  mf_client_kernel_request_init_v1(request, MF_CLIENT_KERNEL_REQUEST_PROFILE_BASELINE_V1,
                                   MF_CLIENT_KERNEL_REQUEST_OPERATION_MATMUL_F32_V1,
                                   MF_CLIENT_KERNEL_REQUEST_KERNEL_IR_SCHEMA_VERSION_V1,
                                   sizeof(payload));
  (void)memcpy(payload + MF_CLIENT_KERNEL_REQUEST_HEADER_SIZE_V1, mf_cublas_sgemm_ptx,
               sizeof(mf_cublas_sgemm_ptx) - 1U);
  result = handle->driver.module_load_data(&handle->module, payload);
  if (result == 0) {
    result = handle->driver.module_get_function(&handle->function, handle->module,
                                                MF_CUBLAS_KERNEL_ENTRY);
  }
  if (result != 0) {
    if (handle->module != NULL) {
      (void)handle->driver.module_unload(handle->module);
    }
    handle->module = NULL;
    handle->function = NULL;
  }
  return mf_cublas_map_driver_status(result);
}

static cublasStatus_t mf_cublas_prepare_lt_bias_locked(struct cublasContext* handle) {
  uint8_t payload[MF_CLIENT_KERNEL_REQUEST_HEADER_SIZE_V1 + sizeof(mf_cublas_lt_bias_ptx) - 1U];
  mf_client_kernel_request_v1* request = (mf_client_kernel_request_v1*)payload;
  mf_cu_result result = 0;
  if (handle->lt_bias_module != NULL && handle->lt_bias_function != NULL) {
    return CUBLAS_STATUS_SUCCESS;
  }
  mf_client_kernel_request_init_v1(request, MF_CLIENT_KERNEL_REQUEST_PROFILE_BASELINE_V1,
                                   MF_CLIENT_KERNEL_REQUEST_OPERATION_MATMUL_F32_V1,
                                   MF_CLIENT_KERNEL_REQUEST_KERNEL_IR_SCHEMA_VERSION_V1,
                                   sizeof(payload));
  (void)memcpy(payload + MF_CLIENT_KERNEL_REQUEST_HEADER_SIZE_V1, mf_cublas_lt_bias_ptx,
               sizeof(mf_cublas_lt_bias_ptx) - 1U);
  result = handle->driver.module_load_data(&handle->lt_bias_module, payload);
  if (result == 0) {
    result = handle->driver.module_get_function(&handle->lt_bias_function, handle->lt_bias_module,
                                                MF_CUBLAS_LT_BIAS_KERNEL_ENTRY);
  }
  if (result != 0) {
    if (handle->lt_bias_module != NULL) {
      (void)handle->driver.module_unload(handle->lt_bias_module);
    }
    handle->lt_bias_module = NULL;
    handle->lt_bias_function = NULL;
  }
  return mf_cublas_map_driver_status(result);
}

uint32_t mf_cublas_provider_bootstrap_abi_version(void) { return UINT32_C(1); }

cublasStatus_t cublasCreate_v2(cublasHandle_t* handle) {
  struct cublasContext* created = NULL;
  cublasStatus_t status = CUBLAS_STATUS_SUCCESS;
  if (handle == NULL) {
    return CUBLAS_STATUS_INVALID_VALUE;
  }
  *handle = NULL;
  created = (struct cublasContext*)calloc(1U, sizeof(*created));
  if (created == NULL) {
    return CUBLAS_STATUS_ALLOC_FAILED;
  }
  status = mf_cublas_driver_open(&created->driver);
  if (status != CUBLAS_STATUS_SUCCESS) {
    free(created);
    return status;
  }
  created->magic = MF_CUBLAS_HANDLE_MAGIC;
  created->pointer_mode = CUBLAS_POINTER_MODE_HOST;
  created->math_mode = CUBLAS_DEFAULT_MATH;
  (void)pthread_mutex_lock(&mf_cublas_mutex);
  created->next = mf_cublas_handles;
  mf_cublas_handles = created;
  (void)pthread_mutex_unlock(&mf_cublas_mutex);
  *handle = created;
  return CUBLAS_STATUS_SUCCESS;
}

cublasStatus_t cublasDestroy_v2(cublasHandle_t handle) {
  struct cublasContext** link = NULL;
  cublasStatus_t status = CUBLAS_STATUS_NOT_INITIALIZED;
  (void)pthread_mutex_lock(&mf_cublas_mutex);
  link = &mf_cublas_handles;
  while (*link != NULL && *link != handle) {
    link = &(*link)->next;
  }
  if (*link != NULL && (*link)->magic == MF_CUBLAS_HANDLE_MAGIC) {
    struct cublasContext* current = *link;
    if (current->module != NULL) {
      status = mf_cublas_map_driver_status(current->driver.module_unload(current->module));
    } else {
      status = CUBLAS_STATUS_SUCCESS;
    }
    if (current->lt_bias_module != NULL) {
      const cublasStatus_t lt_status =
          mf_cublas_map_driver_status(current->driver.module_unload(current->lt_bias_module));
      if (status == CUBLAS_STATUS_SUCCESS) {
        status = lt_status;
      }
    }
    *link = current->next;
    current->magic = UINT32_C(0);
    mf_cublas_driver_close(&current->driver);
    free(current);
  }
  (void)pthread_mutex_unlock(&mf_cublas_mutex);
  return status;
}

cublasStatus_t cublasGetVersion_v2(cublasHandle_t handle, int* version) {
  cublasStatus_t status = CUBLAS_STATUS_NOT_INITIALIZED;
  (void)pthread_mutex_lock(&mf_cublas_mutex);
  if (mf_cublas_find_locked(handle) != NULL) {
    status = version == NULL ? CUBLAS_STATUS_INVALID_VALUE : CUBLAS_STATUS_SUCCESS;
    if (version != NULL) {
      *version = MF_CUBLAS_VERSION;
    }
  }
  (void)pthread_mutex_unlock(&mf_cublas_mutex);
  return status;
}

cublasStatus_t cublasGetProperty(mf_cublas_library_property property, int* value) {
  if (value == NULL) {
    return CUBLAS_STATUS_INVALID_VALUE;
  }
  switch (property) {
  case MF_CUBLAS_LIBRARY_PROPERTY_MAJOR:
    *value = 12;
    break;
  case MF_CUBLAS_LIBRARY_PROPERTY_MINOR:
    *value = 6;
    break;
  case MF_CUBLAS_LIBRARY_PROPERTY_PATCH:
    *value = 4;
    break;
  default:
    return CUBLAS_STATUS_INVALID_VALUE;
  }
  return CUBLAS_STATUS_SUCCESS;
}

size_t cublasGetCudartVersion(void) { return (size_t)MF_CUBLAS_CUDART_VERSION; }

cublasStatus_t cublasSetWorkspace_v2(cublasHandle_t handle, void* workspace,
                                     size_t workspace_size) {
  struct cublasContext* current = NULL;
  cublasStatus_t status = CUBLAS_STATUS_NOT_INITIALIZED;
  (void)pthread_mutex_lock(&mf_cublas_mutex);
  current = mf_cublas_find_locked(handle);
  if (current != NULL) {
    if ((workspace == NULL) != (workspace_size == 0U)) {
      status = CUBLAS_STATUS_INVALID_VALUE;
    } else {
      current->workspace = workspace;
      current->workspace_size = workspace_size;
      status = CUBLAS_STATUS_SUCCESS;
    }
  }
  (void)pthread_mutex_unlock(&mf_cublas_mutex);
  return status;
}

cublasStatus_t cublasSetStream_v2(cublasHandle_t handle, cudaStream_t stream) {
  struct cublasContext* current = NULL;
  cublasStatus_t status = CUBLAS_STATUS_NOT_INITIALIZED;
  (void)pthread_mutex_lock(&mf_cublas_mutex);
  current = mf_cublas_find_locked(handle);
  if (current != NULL) {
    current->stream = stream;
    status = CUBLAS_STATUS_SUCCESS;
  }
  (void)pthread_mutex_unlock(&mf_cublas_mutex);
  return status;
}

cublasStatus_t cublasGetStream_v2(cublasHandle_t handle, cudaStream_t* stream) {
  struct cublasContext* current = NULL;
  cublasStatus_t status = CUBLAS_STATUS_NOT_INITIALIZED;
  (void)pthread_mutex_lock(&mf_cublas_mutex);
  current = mf_cublas_find_locked(handle);
  if (current != NULL) {
    status = stream == NULL ? CUBLAS_STATUS_INVALID_VALUE : CUBLAS_STATUS_SUCCESS;
    if (stream != NULL) {
      *stream = current->stream;
    }
  }
  (void)pthread_mutex_unlock(&mf_cublas_mutex);
  return status;
}

cublasStatus_t cublasGetPointerMode_v2(cublasHandle_t handle, cublasPointerMode_t* mode) {
  struct cublasContext* current = NULL;
  cublasStatus_t status = CUBLAS_STATUS_NOT_INITIALIZED;
  (void)pthread_mutex_lock(&mf_cublas_mutex);
  current = mf_cublas_find_locked(handle);
  if (current != NULL) {
    status = mode == NULL ? CUBLAS_STATUS_INVALID_VALUE : CUBLAS_STATUS_SUCCESS;
    if (mode != NULL) {
      *mode = current->pointer_mode;
    }
  }
  (void)pthread_mutex_unlock(&mf_cublas_mutex);
  return status;
}

cublasStatus_t cublasSetPointerMode_v2(cublasHandle_t handle, cublasPointerMode_t mode) {
  struct cublasContext* current = NULL;
  cublasStatus_t status = CUBLAS_STATUS_NOT_INITIALIZED;
  (void)pthread_mutex_lock(&mf_cublas_mutex);
  current = mf_cublas_find_locked(handle);
  if (current != NULL) {
    if (mode != CUBLAS_POINTER_MODE_HOST && mode != CUBLAS_POINTER_MODE_DEVICE) {
      status = CUBLAS_STATUS_INVALID_VALUE;
    } else {
      current->pointer_mode = mode;
      status = CUBLAS_STATUS_SUCCESS;
    }
  }
  (void)pthread_mutex_unlock(&mf_cublas_mutex);
  return status;
}

cublasStatus_t cublasGetMathMode(cublasHandle_t handle, cublasMath_t* mode) {
  struct cublasContext* current = NULL;
  cublasStatus_t status = CUBLAS_STATUS_NOT_INITIALIZED;
  (void)pthread_mutex_lock(&mf_cublas_mutex);
  current = mf_cublas_find_locked(handle);
  if (current != NULL) {
    status = mode == NULL ? CUBLAS_STATUS_INVALID_VALUE : CUBLAS_STATUS_SUCCESS;
    if (mode != NULL) {
      *mode = current->math_mode;
    }
  }
  (void)pthread_mutex_unlock(&mf_cublas_mutex);
  return status;
}

cublasStatus_t cublasSetMathMode(cublasHandle_t handle, cublasMath_t mode) {
  struct cublasContext* current = NULL;
  cublasStatus_t status = CUBLAS_STATUS_NOT_INITIALIZED;
  const int base = (int)mode & 0xf;
  const int flags = (int)mode & ~0xf;
  (void)pthread_mutex_lock(&mf_cublas_mutex);
  current = mf_cublas_find_locked(handle);
  if (current != NULL) {
    if (base > (int)CUBLAS_TF32_TENSOR_OP_MATH ||
        (flags & ~(int)CUBLAS_MATH_DISALLOW_REDUCED_PRECISION_REDUCTION) != 0) {
      status = CUBLAS_STATUS_INVALID_VALUE;
    } else {
      current->math_mode = mode;
      status = CUBLAS_STATUS_SUCCESS;
    }
  }
  (void)pthread_mutex_unlock(&mf_cublas_mutex);
  return status;
}

const char* cublasGetStatusName(cublasStatus_t status) {
  switch (status) {
  case CUBLAS_STATUS_SUCCESS:
    return "CUBLAS_STATUS_SUCCESS";
  case CUBLAS_STATUS_NOT_INITIALIZED:
    return "CUBLAS_STATUS_NOT_INITIALIZED";
  case CUBLAS_STATUS_ALLOC_FAILED:
    return "CUBLAS_STATUS_ALLOC_FAILED";
  case CUBLAS_STATUS_INVALID_VALUE:
    return "CUBLAS_STATUS_INVALID_VALUE";
  case CUBLAS_STATUS_ARCH_MISMATCH:
    return "CUBLAS_STATUS_ARCH_MISMATCH";
  case CUBLAS_STATUS_MAPPING_ERROR:
    return "CUBLAS_STATUS_MAPPING_ERROR";
  case CUBLAS_STATUS_EXECUTION_FAILED:
    return "CUBLAS_STATUS_EXECUTION_FAILED";
  case CUBLAS_STATUS_INTERNAL_ERROR:
    return "CUBLAS_STATUS_INTERNAL_ERROR";
  case CUBLAS_STATUS_NOT_SUPPORTED:
    return "CUBLAS_STATUS_NOT_SUPPORTED";
  case CUBLAS_STATUS_LICENSE_ERROR:
    return "CUBLAS_STATUS_LICENSE_ERROR";
  default:
    return "CUBLAS_STATUS_UNKNOWN";
  }
}

const char* cublasGetStatusString(cublasStatus_t status) {
  switch (status) {
  case CUBLAS_STATUS_SUCCESS:
    return "success";
  case CUBLAS_STATUS_NOT_INITIALIZED:
    return "library or MetaFlux CUDA context is not initialized";
  case CUBLAS_STATUS_ALLOC_FAILED:
    return "resource allocation failed";
  case CUBLAS_STATUS_INVALID_VALUE:
    return "invalid value";
  case CUBLAS_STATUS_NOT_SUPPORTED:
    return "operation is outside the qualified MetaFlux cuBLAS profile";
  case CUBLAS_STATUS_EXECUTION_FAILED:
    return "daemon submission failed";
  default:
    return "cuBLAS operation failed";
  }
}

cublasStatus_t cublasSgemm_v2(cublasHandle_t handle, cublasOperation_t transa,
                              cublasOperation_t transb, int m, int n, int k, const float* alpha,
                              const float* a, int lda, const float* b, int ldb, const float* beta,
                              float* c, int ldc) {
  struct cublasContext* current = NULL;
  cublasStatus_t status = CUBLAS_STATUS_NOT_INITIALIZED;
  uint32_t output_span = 0U;
  uint32_t left_span = 0U;
  uint32_t right_span = 0U;
  uint32_t values[11];
  mf_cu_device_pointer pointers[3];
  void* parameters[14];
  mf_cu_result result = 0;
  uint32_t transpose_left = 0U;
  uint32_t transpose_right = 0U;
  uint32_t element_count = 0U;
  unsigned int grid_x = 0U;
  unsigned int grid_y = 0U;
  const unsigned int block_x = 8U;
  const unsigned int block_y = 8U;

  (void)pthread_mutex_lock(&mf_cublas_mutex);
  current = mf_cublas_find_locked(handle);
  if (current == NULL) {
    goto done;
  }
  if (current->pointer_mode != CUBLAS_POINTER_MODE_HOST) {
    status = CUBLAS_STATUS_NOT_SUPPORTED;
    goto done;
  }
  if (!mf_cublas_operation_supported(transa) || !mf_cublas_operation_supported(transb) || m <= 0 ||
      n <= 0 || k <= 0 || alpha == NULL || beta == NULL || a == NULL || b == NULL || c == NULL ||
      *alpha != 1.0F || (*beta != 0.0F && *beta != 1.0F)) {
    status = CUBLAS_STATUS_NOT_SUPPORTED;
    goto done;
  }
  transpose_left = transa == CUBLAS_OP_N ? UINT32_C(0) : UINT32_C(1);
  transpose_right = transb == CUBLAS_OP_N ? UINT32_C(0) : UINT32_C(1);
  if (!mf_cublas_matrix_span(transpose_left == 0U ? (uint32_t)m : (uint32_t)k,
                             transpose_left == 0U ? (uint32_t)k : (uint32_t)m, (uint32_t)lda,
                             &left_span) ||
      !mf_cublas_matrix_span(transpose_right == 0U ? (uint32_t)k : (uint32_t)n,
                             transpose_right == 0U ? (uint32_t)n : (uint32_t)k, (uint32_t)ldb,
                             &right_span) ||
      !mf_cublas_matrix_span((uint32_t)m, (uint32_t)n, (uint32_t)ldc, &output_span) ||
      (uint64_t)(uint32_t)m * (uint64_t)(uint32_t)n > UINT32_MAX) {
    status = CUBLAS_STATUS_INVALID_VALUE;
    goto done;
  }
  status = mf_cublas_prepare_sgemm_locked(current);
  if (status != CUBLAS_STATUS_SUCCESS) {
    goto done;
  }

  element_count = (uint32_t)m * (uint32_t)n;
  pointers[0] = (mf_cu_device_pointer)(uintptr_t)c;
  pointers[1] = (mf_cu_device_pointer)(uintptr_t)a;
  pointers[2] = (mf_cu_device_pointer)(uintptr_t)b;
  values[0] = element_count;
  values[1] = transpose_left;
  values[2] = transpose_right;
  values[3] = (uint32_t)m;
  values[4] = (uint32_t)n;
  values[5] = (uint32_t)k;
  values[6] = (uint32_t)lda;
  values[7] = (uint32_t)ldb;
  values[8] = (uint32_t)ldc;
  (void)memcpy(&values[9], alpha, sizeof(values[9]));
  if (*beta == 0.0F) {
    values[10] = UINT32_C(0);
  } else {
    (void)memcpy(&values[10], beta, sizeof(values[10]));
  }
  parameters[0] = &pointers[0];
  parameters[1] = &pointers[1];
  parameters[2] = &pointers[2];
  parameters[3] = &values[0];
  parameters[4] = &values[1];
  parameters[5] = &values[2];
  parameters[6] = &values[3];
  parameters[7] = &values[4];
  parameters[8] = &values[5];
  parameters[9] = &values[6];
  parameters[10] = &values[7];
  parameters[11] = &values[8];
  parameters[12] = &values[9];
  parameters[13] = &values[10];
  grid_x = ((unsigned int)m + block_x - 1U) / block_x;
  grid_y = ((unsigned int)n + block_y - 1U) / block_y;
  result = current->driver.launch_kernel(current->function, grid_x, grid_y, 1U, block_x, block_y,
                                         1U, 0U, current->stream, parameters, NULL);
  status = mf_cublas_map_driver_status(result);
  if (status == CUBLAS_STATUS_SUCCESS && mf_cublas_trace_enabled() != 0) {
    fprintf(stderr,
            "MF_CUBLAS_REQUEST operation=sgemm-f32 transa=%u transb=%u m=%u n=%u k=%u "
            "lda=%u ldb=%u ldc=%u alpha-bits=%08x beta-bits=%08x "
            "left-span=%u right-span=%u output-span=%u\n",
            transpose_left, transpose_right, (uint32_t)m, (uint32_t)n, (uint32_t)k, (uint32_t)lda,
            (uint32_t)ldb, (uint32_t)ldc, values[9], values[10], left_span, right_span,
            output_span);
  }

done:
  (void)pthread_mutex_unlock(&mf_cublas_mutex);
  return status;
}

cublasStatus_t cublasLtMatmulDescCreate(cublasLtMatmulDesc_t* descriptor,
                                        cublasComputeType_t compute_type,
                                        cudaDataType_t scale_type) {
  struct mf_cublas_lt_matmul_desc* created = NULL;
  if (descriptor == NULL) {
    return CUBLAS_STATUS_INVALID_VALUE;
  }
  *descriptor = NULL;
  if (compute_type != CUBLAS_COMPUTE_32F || scale_type != CUDA_R_32F) {
    return CUBLAS_STATUS_NOT_SUPPORTED;
  }
  created = (struct mf_cublas_lt_matmul_desc*)calloc(1U, sizeof(*created));
  if (created == NULL) {
    return CUBLAS_STATUS_ALLOC_FAILED;
  }
  created->magic = MF_CUBLAS_LT_DESC_MAGIC;
  created->compute_type = compute_type;
  created->scale_type = scale_type;
  created->transa = CUBLAS_OP_N;
  created->transb = CUBLAS_OP_N;
  created->epilogue = CUBLASLT_EPILOGUE_DEFAULT;
  (void)pthread_mutex_lock(&mf_cublas_mutex);
  created->next = mf_cublas_lt_descriptors;
  mf_cublas_lt_descriptors = created;
  (void)pthread_mutex_unlock(&mf_cublas_mutex);
  *descriptor = created;
  return CUBLAS_STATUS_SUCCESS;
}

cublasStatus_t cublasLtMatmulDescDestroy(cublasLtMatmulDesc_t descriptor) {
  struct mf_cublas_lt_matmul_desc** link = NULL;
  cublasStatus_t status = CUBLAS_STATUS_INVALID_VALUE;
  (void)pthread_mutex_lock(&mf_cublas_mutex);
  link = &mf_cublas_lt_descriptors;
  while (*link != NULL && *link != descriptor) {
    link = &(*link)->next;
  }
  if (*link != NULL && (*link)->magic == MF_CUBLAS_LT_DESC_MAGIC) {
    struct mf_cublas_lt_matmul_desc* current = *link;
    *link = current->next;
    current->magic = UINT32_C(0);
    free(current);
    status = CUBLAS_STATUS_SUCCESS;
  }
  (void)pthread_mutex_unlock(&mf_cublas_mutex);
  return status;
}

cublasStatus_t cublasLtMatmulDescSetAttribute(
    cublasLtMatmulDesc_t descriptor, cublasLtMatmulDescAttributes_t attribute,
    const void* buffer, size_t size) {
  struct mf_cublas_lt_matmul_desc* current = NULL;
  cublasStatus_t status = CUBLAS_STATUS_INVALID_VALUE;
  (void)pthread_mutex_lock(&mf_cublas_mutex);
  current = mf_cublas_lt_find_descriptor_locked(descriptor);
  if (current == NULL || buffer == NULL) {
    goto done;
  }
  switch (attribute) {
  case CUBLASLT_MATMUL_DESC_TRANSA:
    if (size == sizeof(current->transa)) {
      (void)memcpy(&current->transa, buffer, size);
      status = mf_cublas_operation_supported(current->transa) ? CUBLAS_STATUS_SUCCESS
                                                              : CUBLAS_STATUS_NOT_SUPPORTED;
    }
    break;
  case CUBLASLT_MATMUL_DESC_TRANSB:
    if (size == sizeof(current->transb)) {
      (void)memcpy(&current->transb, buffer, size);
      status = mf_cublas_operation_supported(current->transb) ? CUBLAS_STATUS_SUCCESS
                                                              : CUBLAS_STATUS_NOT_SUPPORTED;
    }
    break;
  case CUBLASLT_MATMUL_DESC_EPILOGUE:
    if (size == sizeof(current->epilogue)) {
      (void)memcpy(&current->epilogue, buffer, size);
      status = current->epilogue == CUBLASLT_EPILOGUE_BIAS
                   ? CUBLAS_STATUS_SUCCESS
                   : CUBLAS_STATUS_NOT_SUPPORTED;
    }
    break;
  case CUBLASLT_MATMUL_DESC_BIAS_POINTER:
    if (size == sizeof(current->bias)) {
      (void)memcpy(&current->bias, buffer, size);
      status = current->bias != NULL ? CUBLAS_STATUS_SUCCESS : CUBLAS_STATUS_INVALID_VALUE;
    }
    break;
  default:
    status = CUBLAS_STATUS_NOT_SUPPORTED;
    break;
  }

done:
  (void)pthread_mutex_unlock(&mf_cublas_mutex);
  return status;
}

cublasStatus_t cublasLtMatrixLayoutCreate(cublasLtMatrixLayout_t* layout,
                                           cudaDataType_t type, uint64_t rows,
                                           uint64_t columns, int64_t leading) {
  struct mf_cublas_lt_matrix_layout* created = NULL;
  uint32_t span = UINT32_C(0);
  if (layout == NULL) {
    return CUBLAS_STATUS_INVALID_VALUE;
  }
  *layout = NULL;
  if (type != CUDA_R_32F || rows > UINT32_MAX || columns > UINT32_MAX ||
      leading < 0 || (uint64_t)leading > UINT32_MAX ||
      !mf_cublas_matrix_span((uint32_t)rows, (uint32_t)columns,
                             (uint32_t)leading, &span)) {
    return type == CUDA_R_32F ? CUBLAS_STATUS_INVALID_VALUE : CUBLAS_STATUS_NOT_SUPPORTED;
  }
  created = (struct mf_cublas_lt_matrix_layout*)calloc(1U, sizeof(*created));
  if (created == NULL) {
    return CUBLAS_STATUS_ALLOC_FAILED;
  }
  created->magic = MF_CUBLAS_LT_LAYOUT_MAGIC;
  created->type = type;
  created->rows = rows;
  created->columns = columns;
  created->leading = leading;
  created->order = CUBLASLT_ORDER_COL;
  created->batch_count = 1;
  (void)pthread_mutex_lock(&mf_cublas_mutex);
  created->next = mf_cublas_lt_layouts;
  mf_cublas_lt_layouts = created;
  (void)pthread_mutex_unlock(&mf_cublas_mutex);
  *layout = created;
  return CUBLAS_STATUS_SUCCESS;
}

cublasStatus_t cublasLtMatrixLayoutDestroy(cublasLtMatrixLayout_t layout) {
  struct mf_cublas_lt_matrix_layout** link = NULL;
  cublasStatus_t status = CUBLAS_STATUS_INVALID_VALUE;
  (void)pthread_mutex_lock(&mf_cublas_mutex);
  link = &mf_cublas_lt_layouts;
  while (*link != NULL && *link != layout) {
    link = &(*link)->next;
  }
  if (*link != NULL && (*link)->magic == MF_CUBLAS_LT_LAYOUT_MAGIC) {
    struct mf_cublas_lt_matrix_layout* current = *link;
    *link = current->next;
    current->magic = UINT32_C(0);
    free(current);
    status = CUBLAS_STATUS_SUCCESS;
  }
  (void)pthread_mutex_unlock(&mf_cublas_mutex);
  return status;
}

cublasStatus_t cublasLtMatrixLayoutSetAttribute(
    cublasLtMatrixLayout_t layout, cublasLtMatrixLayoutAttribute_t attribute,
    const void* buffer, size_t size) {
  struct mf_cublas_lt_matrix_layout* current = NULL;
  cublasStatus_t status = CUBLAS_STATUS_INVALID_VALUE;
  (void)pthread_mutex_lock(&mf_cublas_mutex);
  current = mf_cublas_lt_find_layout_locked(layout);
  if (current == NULL || buffer == NULL) {
    goto done;
  }
  if (attribute == CUBLASLT_MATRIX_LAYOUT_ORDER &&
      size == sizeof(current->order)) {
    cublasLtOrder_t order = CUBLASLT_ORDER_COL;
    (void)memcpy(&order, buffer, size);
    if (order == CUBLASLT_ORDER_COL) {
      current->order = order;
      status = CUBLAS_STATUS_SUCCESS;
    } else {
      status = CUBLAS_STATUS_NOT_SUPPORTED;
    }
  } else if (attribute == CUBLASLT_MATRIX_LAYOUT_BATCH_COUNT &&
             size == sizeof(current->batch_count)) {
    int32_t batch_count = 0;
    (void)memcpy(&batch_count, buffer, size);
    if (batch_count == 1) {
      current->batch_count = batch_count;
      status = CUBLAS_STATUS_SUCCESS;
    } else {
      status = CUBLAS_STATUS_NOT_SUPPORTED;
    }
  } else if (attribute == CUBLASLT_MATRIX_LAYOUT_STRIDED_BATCH_OFFSET &&
             size == sizeof(current->batch_stride)) {
    int64_t batch_stride = 0;
    (void)memcpy(&batch_stride, buffer, size);
    if (batch_stride == 0) {
      current->batch_stride = batch_stride;
      status = CUBLAS_STATUS_SUCCESS;
    } else {
      status = CUBLAS_STATUS_NOT_SUPPORTED;
    }
  } else {
    status = CUBLAS_STATUS_NOT_SUPPORTED;
  }

done:
  (void)pthread_mutex_unlock(&mf_cublas_mutex);
  return status;
}

cublasStatus_t
cublasLtMatmulPreferenceCreate(cublasLtMatmulPreference_t* preference) {
  struct mf_cublas_lt_matmul_preference* created = NULL;
  if (preference == NULL) {
    return CUBLAS_STATUS_INVALID_VALUE;
  }
  *preference = NULL;
  created = (struct mf_cublas_lt_matmul_preference*)calloc(1U, sizeof(*created));
  if (created == NULL) {
    return CUBLAS_STATUS_ALLOC_FAILED;
  }
  created->magic = MF_CUBLAS_LT_PREFERENCE_MAGIC;
  created->minimum_alignment[0] = UINT32_C(256);
  created->minimum_alignment[1] = UINT32_C(256);
  created->minimum_alignment[2] = UINT32_C(256);
  created->minimum_alignment[3] = UINT32_C(256);
  (void)pthread_mutex_lock(&mf_cublas_mutex);
  created->next = mf_cublas_lt_preferences;
  mf_cublas_lt_preferences = created;
  (void)pthread_mutex_unlock(&mf_cublas_mutex);
  *preference = created;
  return CUBLAS_STATUS_SUCCESS;
}

cublasStatus_t
cublasLtMatmulPreferenceDestroy(cublasLtMatmulPreference_t preference) {
  struct mf_cublas_lt_matmul_preference** link = NULL;
  cublasStatus_t status = CUBLAS_STATUS_INVALID_VALUE;
  (void)pthread_mutex_lock(&mf_cublas_mutex);
  link = &mf_cublas_lt_preferences;
  while (*link != NULL && *link != preference) {
    link = &(*link)->next;
  }
  if (*link != NULL && (*link)->magic == MF_CUBLAS_LT_PREFERENCE_MAGIC) {
    struct mf_cublas_lt_matmul_preference* current = *link;
    *link = current->next;
    current->magic = UINT32_C(0);
    free(current);
    status = CUBLAS_STATUS_SUCCESS;
  }
  (void)pthread_mutex_unlock(&mf_cublas_mutex);
  return status;
}

cublasStatus_t cublasLtMatmulPreferenceSetAttribute(
    cublasLtMatmulPreference_t preference,
    cublasLtMatmulPreferenceAttributes_t attribute, const void* buffer,
    size_t size) {
  struct mf_cublas_lt_matmul_preference* current = NULL;
  cublasStatus_t status = CUBLAS_STATUS_INVALID_VALUE;
  (void)pthread_mutex_lock(&mf_cublas_mutex);
  current = mf_cublas_lt_find_preference_locked(preference);
  if (current == NULL || buffer == NULL) {
    goto done;
  }
  if (attribute == CUBLASLT_MATMUL_PREF_MAX_WORKSPACE_BYTES) {
    if (size == sizeof(current->workspace_size)) {
      (void)memcpy(&current->workspace_size, buffer, size);
      status = CUBLAS_STATUS_SUCCESS;
    }
  } else if (attribute >= CUBLASLT_MATMUL_PREF_MIN_ALIGNMENT_A_BYTES &&
             attribute <= CUBLASLT_MATMUL_PREF_MIN_ALIGNMENT_D_BYTES) {
    const size_t index =
        (size_t)(attribute - CUBLASLT_MATMUL_PREF_MIN_ALIGNMENT_A_BYTES);
    uint32_t alignment = UINT32_C(0);
    if (size == sizeof(alignment)) {
      (void)memcpy(&alignment, buffer, size);
      if (alignment != UINT32_C(0) &&
          (alignment & (alignment - UINT32_C(1))) == UINT32_C(0)) {
        current->minimum_alignment[index] = alignment;
        status = CUBLAS_STATUS_SUCCESS;
      }
    }
  } else {
    status = CUBLAS_STATUS_NOT_SUPPORTED;
  }

done:
  (void)pthread_mutex_unlock(&mf_cublas_mutex);
  return status;
}

static int mf_cublas_lt_configuration_locked(
    struct mf_cublas_lt_matmul_desc* operation,
    struct mf_cublas_lt_matrix_layout* a,
    struct mf_cublas_lt_matrix_layout* b,
    struct mf_cublas_lt_matrix_layout* c,
    struct mf_cublas_lt_matrix_layout* d, uint32_t* rows,
    uint32_t* columns, uint32_t* inner) {
  uint64_t output_rows = UINT64_C(0);
  uint64_t output_columns = UINT64_C(0);
  uint64_t left_inner = UINT64_C(0);
  uint64_t right_inner = UINT64_C(0);
  if (operation == NULL || a == NULL || b == NULL || c == NULL || d == NULL ||
      rows == NULL || columns == NULL || inner == NULL ||
      operation->compute_type != CUBLAS_COMPUTE_32F ||
      operation->scale_type != CUDA_R_32F ||
      operation->transa != CUBLAS_OP_T || operation->transb != CUBLAS_OP_N ||
      operation->epilogue != CUBLASLT_EPILOGUE_BIAS ||
      operation->bias == NULL || a->type != CUDA_R_32F ||
      b->type != CUDA_R_32F || c->type != CUDA_R_32F ||
      d->type != CUDA_R_32F || a->order != CUBLASLT_ORDER_COL ||
      b->order != CUBLASLT_ORDER_COL || c->order != CUBLASLT_ORDER_COL ||
      d->order != CUBLASLT_ORDER_COL || a->batch_count != 1 ||
      b->batch_count != 1 || c->batch_count != 1 || d->batch_count != 1 ||
      a->batch_stride != INT64_C(0) || b->batch_stride != INT64_C(0) ||
      c->batch_stride != INT64_C(0) || d->batch_stride != INT64_C(0)) {
    return 0;
  }
  output_rows = a->columns;
  left_inner = a->rows;
  right_inner = b->rows;
  output_columns = b->columns;
  if (left_inner != right_inner || c->rows != output_rows ||
      c->columns != output_columns || d->rows != output_rows ||
      d->columns != output_columns || output_rows > UINT32_MAX ||
      output_columns > UINT32_MAX || left_inner > UINT32_MAX) {
    return 0;
  }
  *rows = (uint32_t)output_rows;
  *columns = (uint32_t)output_columns;
  *inner = (uint32_t)left_inner;
  return 1;
}

cublasStatus_t cublasLtMatmulAlgoGetHeuristic(
    cublasLtHandle_t handle, cublasLtMatmulDesc_t descriptor,
    cublasLtMatrixLayout_t a_layout, cublasLtMatrixLayout_t b_layout,
    cublasLtMatrixLayout_t c_layout, cublasLtMatrixLayout_t d_layout,
    cublasLtMatmulPreference_t preference, int requested_count,
    cublasLtMatmulHeuristicResult_t results[], int* returned_count) {
  struct cublasContext* current = NULL;
  struct mf_cublas_lt_matmul_desc* operation = NULL;
  struct mf_cublas_lt_matrix_layout* a = NULL;
  struct mf_cublas_lt_matrix_layout* b = NULL;
  struct mf_cublas_lt_matrix_layout* c = NULL;
  struct mf_cublas_lt_matrix_layout* d = NULL;
  struct mf_cublas_lt_matmul_preference* pref = NULL;
  uint32_t rows = UINT32_C(0);
  uint32_t columns = UINT32_C(0);
  uint32_t inner = UINT32_C(0);
  cublasStatus_t status = CUBLAS_STATUS_NOT_SUPPORTED;
  if (requested_count <= 0 || results == NULL || returned_count == NULL) {
    return CUBLAS_STATUS_INVALID_VALUE;
  }
  *returned_count = 0;
  (void)pthread_mutex_lock(&mf_cublas_mutex);
  current = mf_cublas_find_locked((cublasHandle_t)(void*)handle);
  operation = mf_cublas_lt_find_descriptor_locked(descriptor);
  a = mf_cublas_lt_find_layout_locked(a_layout);
  b = mf_cublas_lt_find_layout_locked(b_layout);
  c = mf_cublas_lt_find_layout_locked(c_layout);
  d = mf_cublas_lt_find_layout_locked(d_layout);
  pref = mf_cublas_lt_find_preference_locked(preference);
  if (current == NULL) {
    status = CUBLAS_STATUS_NOT_INITIALIZED;
  } else if (operation == NULL || a == NULL || b == NULL || c == NULL ||
             d == NULL || pref == NULL) {
    status = CUBLAS_STATUS_INVALID_VALUE;
  } else if (mf_cublas_lt_configuration_locked(operation, a, b, c, d, &rows,
                                                &columns, &inner)) {
    (void)memset(&results[0], 0, sizeof(results[0]));
    results[0].algo.data[0] = MF_CUBLAS_LT_ALGORITHM_MAGIC;
    results[0].workspaceSize = UINT64_C(0);
    results[0].state = CUBLAS_STATUS_SUCCESS;
    results[0].wavesCount = 1.0F;
    *returned_count = 1;
    status = CUBLAS_STATUS_SUCCESS;
  }
  (void)rows;
  (void)columns;
  (void)inner;
  (void)pthread_mutex_unlock(&mf_cublas_mutex);
  return status;
}

cublasStatus_t cublasLtMatmul(
    cublasLtHandle_t handle, cublasLtMatmulDesc_t descriptor,
    const void* alpha, const void* a_pointer, cublasLtMatrixLayout_t a_layout,
    const void* b_pointer, cublasLtMatrixLayout_t b_layout, const void* beta,
    const void* c_pointer, cublasLtMatrixLayout_t c_layout, void* d_pointer,
    cublasLtMatrixLayout_t d_layout, const cublasLtMatmulAlgo_t* algorithm,
    void* workspace, size_t workspace_size, cudaStream_t stream) {
  struct cublasContext* current = NULL;
  struct mf_cublas_lt_matmul_desc* operation = NULL;
  struct mf_cublas_lt_matrix_layout* a = NULL;
  struct mf_cublas_lt_matrix_layout* b = NULL;
  struct mf_cublas_lt_matrix_layout* c = NULL;
  struct mf_cublas_lt_matrix_layout* d = NULL;
  mf_cu_device_pointer pointers[4];
  uint32_t values[12] = {0};
  void* parameters[16];
  uint32_t rows = UINT32_C(0);
  uint32_t columns = UINT32_C(0);
  uint32_t inner = UINT32_C(0);
  uint32_t left_span = UINT32_C(0);
  uint32_t right_span = UINT32_C(0);
  uint32_t output_span = UINT32_C(0);
  uint32_t alpha_bits = UINT32_C(0);
  uint32_t beta_bits = UINT32_C(0);
  unsigned int grid_x = UINT32_C(0);
  unsigned int grid_y = UINT32_C(0);
  const unsigned int block_x = UINT32_C(8);
  const unsigned int block_y = UINT32_C(8);
  cublasStatus_t status = CUBLAS_STATUS_NOT_SUPPORTED;
  mf_cu_result result = 0;
  if (alpha == NULL || beta == NULL || a_pointer == NULL ||
      b_pointer == NULL || c_pointer == NULL || d_pointer == NULL ||
      algorithm == NULL || (workspace_size != 0U && workspace == NULL)) {
    if (mf_cublas_trace_enabled() != 0) {
      fprintf(stderr, "MF_CUBLAS_REJECT operation=lt-matmul-bias-f32 reason=arguments\n");
    }
    return CUBLAS_STATUS_INVALID_VALUE;
  }
  (void)memcpy(&alpha_bits, alpha, sizeof(alpha_bits));
  (void)memcpy(&beta_bits, beta, sizeof(beta_bits));
  (void)pthread_mutex_lock(&mf_cublas_mutex);
  current = mf_cublas_find_locked((cublasHandle_t)(void*)handle);
  operation = mf_cublas_lt_find_descriptor_locked(descriptor);
  a = mf_cublas_lt_find_layout_locked(a_layout);
  b = mf_cublas_lt_find_layout_locked(b_layout);
  c = mf_cublas_lt_find_layout_locked(c_layout);
  d = mf_cublas_lt_find_layout_locked(d_layout);
  if (current == NULL) {
    if (mf_cublas_trace_enabled() != 0) {
      fprintf(stderr, "MF_CUBLAS_REJECT operation=lt-matmul-bias-f32 reason=handle\n");
    }
    status = CUBLAS_STATUS_NOT_INITIALIZED;
    goto done;
  }
  if (operation == NULL || a == NULL || b == NULL || c == NULL || d == NULL ||
      current->pointer_mode != CUBLAS_POINTER_MODE_HOST) {
    if (mf_cublas_trace_enabled() != 0) {
      fprintf(stderr,
              "MF_CUBLAS_REJECT operation=lt-matmul-bias-f32 reason=objects-or-pointer-mode "
              "descriptor=%u a=%u b=%u c=%u d=%u pointer-mode=%d\n",
              operation != NULL, a != NULL, b != NULL, c != NULL, d != NULL,
              (int)current->pointer_mode);
    }
    status = CUBLAS_STATUS_INVALID_VALUE;
    goto done;
  }
  if (!mf_cublas_lt_configuration_locked(operation, a, b, c, d, &rows,
                                          &columns, &inner) ||
      alpha_bits != UINT32_C(0x3f800000) || beta_bits != UINT32_C(0) ||
      c_pointer != d_pointer || c_layout != d_layout) {
    if (mf_cublas_trace_enabled() != 0) {
      fprintf(stderr,
              "MF_CUBLAS_REJECT operation=lt-matmul-bias-f32 reason=configuration "
              "alpha-bits=%08x beta-bits=%08x same-output=%u same-layout=%u\n",
              alpha_bits, beta_bits, c_pointer == d_pointer, c_layout == d_layout);
    }
    status = CUBLAS_STATUS_NOT_SUPPORTED;
    goto done;
  }
  if (algorithm->data[0] != MF_CUBLAS_LT_ALGORITHM_MAGIC) {
    if (mf_cublas_trace_enabled() != 0) {
      fprintf(stderr, "MF_CUBLAS_REJECT operation=lt-matmul-bias-f32 reason=algorithm\n");
    }
    status = CUBLAS_STATUS_INVALID_VALUE;
    goto done;
  }
  if (!mf_cublas_matrix_span((uint32_t)a->rows, (uint32_t)a->columns,
                             (uint32_t)a->leading, &left_span) ||
      !mf_cublas_matrix_span((uint32_t)b->rows, (uint32_t)b->columns,
                             (uint32_t)b->leading, &right_span) ||
      !mf_cublas_matrix_span(rows, columns, (uint32_t)d->leading,
                             &output_span)) {
    if (mf_cublas_trace_enabled() != 0) {
      fprintf(stderr, "MF_CUBLAS_REJECT operation=lt-matmul-bias-f32 reason=layout-span\n");
    }
    status = CUBLAS_STATUS_INVALID_VALUE;
    goto done;
  }
  status = mf_cublas_prepare_lt_bias_locked(current);
  if (status != CUBLAS_STATUS_SUCCESS) {
    goto done;
  }
  pointers[0] = (mf_cu_device_pointer)(uintptr_t)d_pointer;
  pointers[1] = (mf_cu_device_pointer)(uintptr_t)a_pointer;
  pointers[2] = (mf_cu_device_pointer)(uintptr_t)b_pointer;
  pointers[3] = (mf_cu_device_pointer)(uintptr_t)operation->bias;
  values[0] = rows * columns;
  values[1] = UINT32_C(1);
  values[2] = UINT32_C(0);
  values[3] = rows;
  values[4] = columns;
  values[5] = inner;
  values[6] = (uint32_t)a->leading;
  values[7] = (uint32_t)b->leading;
  values[8] = (uint32_t)d->leading;
  values[9] = alpha_bits;
  values[10] = beta_bits;
  values[11] = (uint32_t)CUBLASLT_EPILOGUE_BIAS;
  for (uint32_t index = UINT32_C(0); index < UINT32_C(4); ++index) {
    parameters[index] = &pointers[index];
  }
  for (uint32_t index = UINT32_C(0); index < UINT32_C(12); ++index) {
    parameters[index + UINT32_C(4)] = &values[index];
  }
  grid_x = (rows + block_x - UINT32_C(1)) / block_x;
  grid_y = (columns + block_y - UINT32_C(1)) / block_y;
  result = current->driver.launch_kernel(
      current->lt_bias_function, grid_x, grid_y, UINT32_C(1), block_x,
      block_y, UINT32_C(1), UINT32_C(0), stream, parameters, NULL);
  status = mf_cublas_map_driver_status(result);
  if (status == CUBLAS_STATUS_SUCCESS && mf_cublas_trace_enabled() != 0) {
    fprintf(stderr,
            "MF_CUBLAS_REQUEST operation=lt-matmul-bias-f32 "
            "transa=1 transb=0 m=%u n=%u k=%u lda=%u ldb=%u ldc=%u "
            "alpha-bits=%08x beta-bits=%08x epilogue=%u "
            "left-span=%u right-span=%u bias-span=%u output-span=%u\n",
            rows, columns, inner, (uint32_t)a->leading,
            (uint32_t)b->leading, (uint32_t)d->leading, alpha_bits,
            beta_bits, (uint32_t)CUBLASLT_EPILOGUE_BIAS, left_span,
            right_span, rows, output_span);
  }

done:
  (void)pthread_mutex_unlock(&mf_cublas_mutex);
  return status;
}
