#ifndef METAFLUX_CUDA_ABI_H
#define METAFLUX_CUDA_ABI_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__clang__) || defined(__GNUC__)
#define MF_CUDA_ABI_API __attribute__((visibility("default")))
#else
#define MF_CUDA_ABI_API
#endif

#define MF_CUDA_DRIVER_API_VERSION 12060

typedef int CUdevice;
typedef unsigned int CUdeviceptr_v1;
typedef unsigned long long CUdeviceptr;
typedef unsigned long long cuuint64_t;
typedef struct CUctx_st* CUcontext;
typedef struct CUmod_st* CUmodule;
typedef struct CUfunc_st* CUfunction;
typedef struct CUstream_st* CUstream;
typedef struct CUevent_st* CUevent;
typedef struct CUexecAffinityParam_st CUexecAffinityParam;
typedef struct CUctxCigParam_st CUctxCigParam;

/* CUDA 12 library-management and device surface. Values mirror the frozen
   R610 vendor headers; enums below are the provider-local minimal faithful
   definitions and are never inspected by the typed stubs that receive them. */
typedef struct CUlib_st* CUlibrary;
typedef struct CUkern_st* CUkernel;
typedef struct CUmemPoolHandle_st* CUmemoryPool;
typedef struct CUjit_option_st CUjit_option;
typedef struct CUasyncNotificationInfo_st CUasyncNotificationInfo;
typedef struct CUasyncCallbackEntry_st* CUasyncCallbackHandle;
typedef void (*CUasyncCallback)(CUasyncNotificationInfo* info, void* user_data,
                                CUasyncCallbackHandle callback);
typedef struct CUgraph_st* CUgraph;
typedef struct CUgraphConditionalHandle_st* CUgraphConditionalHandle;
typedef struct CUgraphExec_st* CUgraphExec;
typedef struct CUgraphNode_st* CUgraphNode;
typedef struct CUDA_GRAPH_NODE_PARAMS_st CUDA_GRAPH_NODE_PARAMS;
typedef enum CUstreamCaptureMode_enum {
    CU_STREAM_CAPTURE_MODE_GLOBAL = 0,
    CU_STREAM_CAPTURE_MODE_THREAD_LOCAL = 1,
    CU_STREAM_CAPTURE_MODE_RELAXED = 2
} CUstreamCaptureMode;
typedef enum CUstreamCaptureStatus_enum {
    CU_STREAM_CAPTURE_STATUS_NONE = 0,
    CU_STREAM_CAPTURE_STATUS_ACTIVE = 1,
    CU_STREAM_CAPTURE_STATUS_INVALIDATED = 2
} CUstreamCaptureStatus;

typedef enum CUlimit_enum {
    CU_LIMIT_STACK_SIZE = 0x00,
    CU_LIMIT_PRINTF_FIFO_SIZE = 0x01,
    CU_LIMIT_MALLOC_HEAP_SIZE = 0x02,
    CU_LIMIT_DEV_RUNTIME_SYNC_DEPTH = 0x03,
    CU_LIMIT_DEV_RUNTIME_PENDING_LAUNCH_COUNT = 0x04,
    CU_LIMIT_MAX_L2_FETCH_GRANULARITY = 0x05,
    CU_LIMIT_PERSISTING_L2_CACHE_SIZE = 0x06
} CUlimit;
typedef enum CUsharedconfig_enum {
    CU_SHARED_MEM_CONFIG_DEFAULT_BANK_SIZE = 0x00,
    CU_SHARED_MEM_CONFIG_FOUR_BYTE_BANK_SIZE = 0x01,
    CU_SHARED_MEM_CONFIG_EIGHT_BYTE_BANK_SIZE = 0x02
} CUsharedconfig;
typedef enum CUpointer_attribute_enum {
    CU_POINTER_ATTRIBUTE_CONTEXT = 1,
    CU_POINTER_ATTRIBUTE_MEMORY_TYPE = 2,
    CU_POINTER_ATTRIBUTE_DEVICE_POINTER = 3,
    CU_POINTER_ATTRIBUTE_HOST_POINTER = 4,
    CU_POINTER_ATTRIBUTE_DEVICE_ORDINAL = 9,
    CU_POINTER_ATTRIBUTE_IS_MANAGED = 8
} CUpointer_attribute;
typedef enum CUstreamAttrID_enum {
    CU_STREAM_ATTRIBUTE_ACCESS_POLICY_WINDOW = 1,
    CU_STREAM_ATTRIBUTE_SYNCHRONIZATION_POLICY = 3
} CUstreamAttrID;
typedef struct CUstreamAttrValue_st CUstreamAttrValue;
typedef struct CUstreamBatchMemOpParams_st CUstreamBatchMemOpParams;
typedef struct CUDA_MEMCPY2D_st CUDA_MEMCPY2D;

typedef struct CUgraphEdgeData_st CUgraphEdgeData;
typedef struct CUgraphNodeParams_st CUgraphNodeParams;
typedef struct CUDA_EXT_SEM_SIGNAL_NODE_PARAMS_st CUDA_EXT_SEM_SIGNAL_NODE_PARAMS;
typedef struct CUDA_EXT_SEM_WAIT_NODE_PARAMS_st CUDA_EXT_SEM_WAIT_NODE_PARAMS;
typedef struct CUDA_HOST_NODE_PARAMS_st CUDA_HOST_NODE_PARAMS;
typedef struct CUDA_MEMCPY3D_st CUDA_MEMCPY3D;
typedef struct CUDA_MEMSET_NODE_PARAMS_st CUDA_MEMSET_NODE_PARAMS;
typedef struct CUDA_MEM_ALLOC_NODE_PARAMS_st CUDA_MEM_ALLOC_NODE_PARAMS;
typedef struct CUDA_KERNEL_NODE_PARAMS_st CUDA_KERNEL_NODE_PARAMS;
typedef struct CUuserObject_st* CUuserObject;
typedef void (*CUhostFn)(void* userData);
typedef enum CUgraphInstantiateResult_enum {
    CUDA_GRAPH_INSTANTIATE_SUCCESS = 0,
    CUDA_GRAPH_INSTANTIATE_ERROR = 1,
    CUDA_GRAPH_INSTANTIATE_INVALID_STRUCTURE = 2,
    CUDA_GRAPH_INSTANTIATE_NODE_OPERATION_NOT_SUPPORTED = 3,
    CUDA_GRAPH_INSTANTIATE_MULTIPLE_CTXS_NOT_SUPPORTED = 4,
    CUDA_GRAPH_INSTANTIATE_CONDITIONAL_HANDLE_UNUSED = 5
} CUgraphInstantiateResult;
typedef struct CUDA_GRAPH_INSTANTIATE_PARAMS_st {
    cuuint64_t flags;
    CUstream hUploadStream;
    CUgraphNode hErrNode_out;
    CUgraphInstantiateResult result_out;
} CUDA_GRAPH_INSTANTIATE_PARAMS;
typedef enum CUmoduleLoadingMode_enum {
    CU_MODULE_EAGER_LOADING = 0x1,
    CU_MODULE_LAZY_LOADING = 0x2
} CUmoduleLoadingMode;
typedef enum CUdevice_P2PAttribute_enum {
    CU_DEVICE_P2P_ATTRIBUTE_PERFORMANCE_RANK = 0x01,
    CU_DEVICE_P2P_ATTRIBUTE_ACCESS_SUPPORTED = 0x02,
    CU_DEVICE_P2P_ATTRIBUTE_NATIVE_ATOMIC_SUPPORTED = 0x03,
    CU_DEVICE_P2P_ATTRIBUTE_ACCESS_ACCESS_SUPPORTED = 0x04,
    CU_DEVICE_P2P_ATTRIBUTE_CUDA_ARRAY_ACCESS_SUPPORTED = 0x04
} CUdevice_P2PAttribute;
typedef enum CUarray_format_enum {
    CU_AD_FORMAT_UNSIGNED_INT8 = 0x01,
    CU_AD_FORMAT_FLOAT = 0x20
} CUarray_format;
typedef enum CUfunction_attribute_enum {
    CU_FUNC_ATTRIBUTE_MAX_THREADS_PER_BLOCK = 0x00,
    CU_FUNC_ATTRIBUTE_SHARED_SIZE_BYTES = 0x01,
    CU_FUNC_ATTRIBUTE_NUM_REGS = 0x04
} CUfunction_attribute;
typedef enum CUfunc_config_enum {
    CU_FUNC_CACHE_PREFER_NONE = 0x00,
    CU_FUNC_CACHE_PREFER_SHARED = 0x01,
    CU_FUNC_CACHE_PREFER_L1 = 0x02,
    CU_FUNC_CACHE_PREFER_EQUAL = 0x03
} CUfunc_config;
typedef CUfunc_config CUfunc_cache;
typedef enum CUgraphExecUpdateResult_enum {
    CU_GRAPH_EXEC_UPDATE_SUCCESS = 0x0,
    CU_GRAPH_EXEC_UPDATE_ERROR = 0x1
} CUgraphExecUpdateResult;
typedef struct CUgraphExecUpdateResultInfo_st {
    CUgraphExecUpdateResult result;
    CUgraphNode errorNode;
    CUgraphNode errorFromNode;
} CUgraphExecUpdateResultInfo;
typedef enum CUflushGPUDirectRDMAWritesTarget_enum {
    CU_FLUSH_GPU_DIRECT_RDMA_WRITES_TARGET_CURRENT_CTX = 0
} CUflushGPUDirectRDMAWritesTarget;
typedef enum CUflushGPUDirectRDMAWritesMode_enum {
    CU_FLUSH_GPU_DIRECT_RDMA_WRITES_MODE_TIMER = 0
} CUflushGPUDirectRDMAWritesMode;
typedef enum CUdevice_attribute_enum {
    CU_DEVICE_ATTRIBUTE_MAX_THREADS_PER_BLOCK = 1,
    CU_DEVICE_ATTRIBUTE_MAX_BLOCK_DIM_X = 2,
    CU_DEVICE_ATTRIBUTE_MAX_BLOCK_DIM_Y = 3,
    CU_DEVICE_ATTRIBUTE_MAX_BLOCK_DIM_Z = 4,
    CU_DEVICE_ATTRIBUTE_MAX_GRID_DIM_X = 5,
    CU_DEVICE_ATTRIBUTE_MAX_GRID_DIM_Y = 6,
    CU_DEVICE_ATTRIBUTE_MAX_GRID_DIM_Z = 7,
    CU_DEVICE_ATTRIBUTE_MAX_SHARED_MEMORY_PER_BLOCK = 8,
    CU_DEVICE_ATTRIBUTE_TOTAL_CONSTANT_MEMORY = 9,
    CU_DEVICE_ATTRIBUTE_WARP_SIZE = 10,
    CU_DEVICE_ATTRIBUTE_MAX_PITCH = 11,
    CU_DEVICE_ATTRIBUTE_MAX_REGISTERS_PER_BLOCK = 12,
    CU_DEVICE_ATTRIBUTE_CLOCK_RATE = 13,
    CU_DEVICE_ATTRIBUTE_TEXTURE_ALIGNMENT = 14,
    CU_DEVICE_ATTRIBUTE_GPU_OVERLAP = 15,
    CU_DEVICE_ATTRIBUTE_MULTIPROCESSOR_COUNT = 16,
    CU_DEVICE_ATTRIBUTE_KERNEL_EXEC_TIMEOUT = 17,
    CU_DEVICE_ATTRIBUTE_INTEGRATED = 18,
    CU_DEVICE_ATTRIBUTE_CAN_MAP_HOST_MEMORY = 19,
    CU_DEVICE_ATTRIBUTE_COMPUTE_MODE = 20,
    CU_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURECUBEMAP_WIDTH = 52,
    CU_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURECUBEMAP_LAYERED_WIDTH = 53,
    CU_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURECUBEMAP_LAYERED_LAYERS = 54,
    CU_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE1D_WIDTH = 21,
    CU_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE2D_WIDTH = 22,
    CU_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE2D_HEIGHT = 23,
    CU_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE3D_WIDTH = 24,
    CU_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE3D_HEIGHT = 25,
    CU_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE3D_DEPTH = 26,
    CU_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE2D_LAYERED_WIDTH = 27,
    CU_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE2D_LAYERED_HEIGHT = 28,
    CU_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE2D_LAYERED_LAYERS = 29,
    CU_DEVICE_ATTRIBUTE_SURFACE_ALIGNMENT = 30,
    CU_DEVICE_ATTRIBUTE_CONCURRENT_KERNELS = 31,
    CU_DEVICE_ATTRIBUTE_ECC_ENABLED = 32,
    CU_DEVICE_ATTRIBUTE_PCI_BUS_ID = 33,
    CU_DEVICE_ATTRIBUTE_PCI_DEVICE_ID = 34,
    CU_DEVICE_ATTRIBUTE_PCI_DOMAIN_ID = 50,
    CU_DEVICE_ATTRIBUTE_TCC_DRIVER = 35,
    CU_DEVICE_ATTRIBUTE_MEMORY_CLOCK_RATE = 36,
    CU_DEVICE_ATTRIBUTE_GLOBAL_MEMORY_BUS_WIDTH = 37,
    CU_DEVICE_ATTRIBUTE_L2_CACHE_SIZE = 38,
    CU_DEVICE_ATTRIBUTE_MAX_THREADS_PER_MULTIPROCESSOR = 39,
    CU_DEVICE_ATTRIBUTE_ASYNC_ENGINE_COUNT = 40,
    CU_DEVICE_ATTRIBUTE_UNIFIED_ADDRESSING = 41,
    CU_DEVICE_ATTRIBUTE_CAN_TEX2D_GATHER = 44,
    CU_DEVICE_ATTRIBUTE_CAN_USE_STREAM_MEM_OPS_V1 = 92,
    CU_DEVICE_ATTRIBUTE_CAN_USE_64_BIT_STREAM_MEM_OPS_V1 = 93,
    CU_DEVICE_ATTRIBUTE_CAN_USE_STREAM_WAIT_VALUE_NOR_V1 = 94,
    CU_DEVICE_ATTRIBUTE_GENERIC_COMPRESSION_SUPPORTED = 107,
    CU_DEVICE_ATTRIBUTE_MAX_PERSISTING_L2_CACHE_SIZE = 108,
    CU_DEVICE_ATTRIBUTE_MAX_ACCESS_POLICY_WINDOW_SIZE = 109,
    CU_DEVICE_ATTRIBUTE_GPU_DIRECT_RDMA_WITH_CUDA_VMM_SUPPORTED = 110,
    CU_DEVICE_ATTRIBUTE_RESERVED_SHARED_MEMORY_PER_BLOCK = 111,
    CU_DEVICE_ATTRIBUTE_SPARSE_CUDA_ARRAY_SUPPORTED = 112,
    CU_DEVICE_ATTRIBUTE_READ_ONLY_HOST_REGISTER_SUPPORTED = 113,
    CU_DEVICE_ATTRIBUTE_TIMELINE_SEMAPHORE_INTEROP_SUPPORTED = 114,
    CU_DEVICE_ATTRIBUTE_GPU_DIRECT_RDMA_SUPPORTED = 116,
    CU_DEVICE_ATTRIBUTE_GPU_DIRECT_RDMA_FLUSH_WRITES_OPTIONS = 117,
    CU_DEVICE_ATTRIBUTE_GPU_DIRECT_RDMA_WRITES_ORDERING = 118,
    CU_DEVICE_ATTRIBUTE_MEMPOOL_SUPPORTED_HANDLE_TYPES = 119,
    CU_DEVICE_ATTRIBUTE_CLUSTER_LAUNCH = 120,
    CU_DEVICE_ATTRIBUTE_DEFERRED_MAPPING_CUDA_ARRAY_SUPPORTED = 121,
    CU_DEVICE_ATTRIBUTE_CAN_USE_64_BIT_STREAM_MEM_OPS = 122,
    CU_DEVICE_ATTRIBUTE_CAN_USE_STREAM_WAIT_VALUE_NOR = 123,
    CU_DEVICE_ATTRIBUTE_DMA_BUF_SUPPORTED = 124,
    CU_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE1D_LAYERED_WIDTH = 42,
    CU_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE1D_LAYERED_LAYERS = 43,
    CU_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE2D_GATHER_WIDTH = 45,
    CU_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE2D_GATHER_HEIGHT = 46,
    CU_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE3D_WIDTH_ALTERNATE = 47,
    CU_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE3D_HEIGHT_ALTERNATE = 48,
    CU_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE3D_DEPTH_ALTERNATE = 49,
    CU_DEVICE_ATTRIBUTE_MAXIMUM_SURFACE1D_WIDTH = 55,
    CU_DEVICE_ATTRIBUTE_MAXIMUM_SURFACE2D_WIDTH = 56,
    CU_DEVICE_ATTRIBUTE_MAXIMUM_SURFACE2D_HEIGHT = 57,
    CU_DEVICE_ATTRIBUTE_MAXIMUM_SURFACE3D_WIDTH = 58,
    CU_DEVICE_ATTRIBUTE_MAXIMUM_SURFACE3D_HEIGHT = 59,
    CU_DEVICE_ATTRIBUTE_MAXIMUM_SURFACE3D_DEPTH = 60,
    CU_DEVICE_ATTRIBUTE_MAXIMUM_SURFACE1D_LAYERED_WIDTH = 61,
    CU_DEVICE_ATTRIBUTE_MAXIMUM_SURFACE1D_LAYERED_LAYERS = 62,
    CU_DEVICE_ATTRIBUTE_MAXIMUM_SURFACE2D_LAYERED_WIDTH = 63,
    CU_DEVICE_ATTRIBUTE_MAXIMUM_SURFACE2D_LAYERED_HEIGHT = 64,
    CU_DEVICE_ATTRIBUTE_MAXIMUM_SURFACE2D_LAYERED_LAYERS = 65,
    CU_DEVICE_ATTRIBUTE_MAXIMUM_SURFACECUBEMAP_WIDTH = 66,
    CU_DEVICE_ATTRIBUTE_MAXIMUM_SURFACECUBEMAP_LAYERED_WIDTH = 67,
    CU_DEVICE_ATTRIBUTE_MAXIMUM_SURFACECUBEMAP_LAYERED_LAYERS = 68,
    CU_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE1D_LINEAR_WIDTH = 69,
    CU_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE2D_LINEAR_WIDTH = 70,
    CU_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE2D_LINEAR_HEIGHT = 71,
    CU_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE2D_LINEAR_PITCH = 72,
    CU_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE2D_MIPMAPPED_WIDTH = 73,
    CU_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE2D_MIPMAPPED_HEIGHT = 74,
    CU_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE1D_MIPMAPPED_WIDTH = 77,
    CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MAJOR = 75,
    CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MINOR = 76,
    CU_DEVICE_ATTRIBUTE_STREAM_PRIORITIES_SUPPORTED = 78,
    CU_DEVICE_ATTRIBUTE_MAX_SHARED_MEMORY_PER_MULTIPROCESSOR = 81,
    CU_DEVICE_ATTRIBUTE_MAX_REGISTERS_PER_MULTIPROCESSOR = 82,
    CU_DEVICE_ATTRIBUTE_MANAGED_MEMORY = 83,
    CU_DEVICE_ATTRIBUTE_HOST_NATIVE_ATOMIC_SUPPORTED = 86,
    CU_DEVICE_ATTRIBUTE_PAGEABLE_MEMORY_ACCESS = 88,
    CU_DEVICE_ATTRIBUTE_CONCURRENT_MANAGED_ACCESS = 89,
    CU_DEVICE_ATTRIBUTE_COMPUTE_PREEMPTION_SUPPORTED = 90,
    CU_DEVICE_ATTRIBUTE_CAN_USE_HOST_POINTER_FOR_REGISTERED_MEM = 91,
    CU_DEVICE_ATTRIBUTE_MAX_BLOCKS_PER_MULTIPROCESSOR = 106,
    CU_DEVICE_ATTRIBUTE_HOST_REGISTER_SUPPORTED = 99,
    CU_DEVICE_ATTRIBUTE_MEMORY_POOLS_SUPPORTED = 115
} CUdevice_attribute;

typedef struct CUctxCreateParams_st {
  CUexecAffinityParam* execAffinityParams;
  int numExecAffinityParams;
  CUctxCigParam* cigParams;
} CUctxCreateParams;

typedef struct CUuuid_st {
  char bytes[16];
} CUuuid;

#if defined(__cplusplus)
#define MF_CUDA_ABI_STATIC_ASSERT(condition, message) static_assert(condition, message)
#else
#define MF_CUDA_ABI_STATIC_ASSERT(condition, message) _Static_assert(condition, message)
#endif
MF_CUDA_ABI_STATIC_ASSERT(sizeof(CUdevice) == 4, "CUdevice ABI size");
MF_CUDA_ABI_STATIC_ASSERT(sizeof(CUdeviceptr_v1) == 4, "legacy CUdeviceptr ABI size");
MF_CUDA_ABI_STATIC_ASSERT(sizeof(CUdeviceptr) == 8, "CUdeviceptr ABI size");
MF_CUDA_ABI_STATIC_ASSERT(sizeof(CUuuid) == 16, "CUuuid ABI size");
MF_CUDA_ABI_STATIC_ASSERT(sizeof(CUctxCreateParams) == 24, "CUctxCreateParams ABI size");
MF_CUDA_ABI_STATIC_ASSERT(offsetof(CUctxCreateParams, execAffinityParams) == 0,
                          "CUctxCreateParams exec-affinity offset");
MF_CUDA_ABI_STATIC_ASSERT(offsetof(CUctxCreateParams, numExecAffinityParams) == 8,
                          "CUctxCreateParams count offset");
MF_CUDA_ABI_STATIC_ASSERT(offsetof(CUctxCreateParams, cigParams) == 16,
                          "CUctxCreateParams CIG offset");
#undef MF_CUDA_ABI_STATIC_ASSERT

typedef enum cudaError_enum {
  CUDA_SUCCESS = 0,
  CUDA_ERROR_INVALID_VALUE = 1,
  CUDA_ERROR_OUT_OF_MEMORY = 2,
  CUDA_ERROR_NOT_INITIALIZED = 3,
  CUDA_ERROR_DEINITIALIZED = 4,
  CUDA_ERROR_NO_DEVICE = 100,
  CUDA_ERROR_INVALID_DEVICE = 101,
  CUDA_ERROR_INVALID_IMAGE = 200,
  CUDA_ERROR_INVALID_CONTEXT = 201,
  CUDA_ERROR_INVALID_HANDLE = 400,
  CUDA_ERROR_NOT_FOUND = 500,
  CUDA_ERROR_NOT_READY = 600,
  CUDA_ERROR_ILLEGAL_ADDRESS = 700,
  CUDA_ERROR_CONTEXT_IS_DESTROYED = 709,
  CUDA_ERROR_LAUNCH_FAILED = 719,
  CUDA_ERROR_NOT_SUPPORTED = 801,
  CUDA_ERROR_SYSTEM_NOT_READY = 802,
  CUDA_ERROR_UNKNOWN = 999
} CUresult;
typedef void (*CUstreamCallback)(CUstream hStream, CUresult status, void* userData);

typedef enum CUdriverProcAddress_flags_enum {
  CU_GET_PROC_ADDRESS_DEFAULT = 0,
  CU_GET_PROC_ADDRESS_LEGACY_STREAM = 1 << 0,
  CU_GET_PROC_ADDRESS_PER_THREAD_DEFAULT_STREAM = 1 << 1
} CUdriverProcAddress_flags;

typedef enum CUdriverProcAddressQueryResult_enum {
  CU_GET_PROC_ADDRESS_SUCCESS = 0,
  CU_GET_PROC_ADDRESS_SYMBOL_NOT_FOUND = 1,
  CU_GET_PROC_ADDRESS_VERSION_NOT_SUFFICIENT = 2
} CUdriverProcAddressQueryResult;

enum {
  CU_STREAM_DEFAULT = 0,
  CU_STREAM_NON_BLOCKING = 1,
  CU_EVENT_DEFAULT = 0,
  CU_EVENT_BLOCKING_SYNC = 1,
  CU_EVENT_DISABLE_TIMING = 2,
  CU_EVENT_INTERPROCESS = 4
};

MF_CUDA_ABI_API CUresult cuInit(unsigned int flags);
MF_CUDA_ABI_API CUresult cuDriverGetVersion(int* driver_version);
MF_CUDA_ABI_API CUresult cuDeviceGet(CUdevice* device, int ordinal);
MF_CUDA_ABI_API CUresult cuDeviceGetCount(int* count);
MF_CUDA_ABI_API CUresult cuDeviceGetName(char* name, int length, CUdevice device);
MF_CUDA_ABI_API CUresult cuDeviceGetUuid(CUuuid* uuid, CUdevice device);
MF_CUDA_ABI_API CUresult cuDeviceGetUuid_v2(CUuuid* uuid, CUdevice device);
MF_CUDA_ABI_API CUresult cuDeviceComputeCapability(int* major, int* minor, CUdevice device);
MF_CUDA_ABI_API CUresult cuDeviceTotalMem(unsigned int* bytes, CUdevice device);
MF_CUDA_ABI_API CUresult cuDeviceTotalMem_v2(size_t* bytes, CUdevice device);
MF_CUDA_ABI_API CUresult cuDeviceGetPCIBusId(char* pci_bus_id, int length, CUdevice device);
MF_CUDA_ABI_API CUresult cuDevicePrimaryCtxRetain(CUcontext* context, CUdevice device);
MF_CUDA_ABI_API CUresult cuDevicePrimaryCtxRelease(CUdevice device);
MF_CUDA_ABI_API CUresult cuDevicePrimaryCtxRelease_v2(CUdevice device);
MF_CUDA_ABI_API CUresult cuDevicePrimaryCtxReset(CUdevice device);
MF_CUDA_ABI_API CUresult cuDevicePrimaryCtxReset_v2(CUdevice device);

MF_CUDA_ABI_API CUresult cuCtxCreate(CUcontext* context, unsigned int flags, CUdevice device);
MF_CUDA_ABI_API CUresult cuCtxCreate_v2(CUcontext* context, unsigned int flags, CUdevice device);
MF_CUDA_ABI_API CUresult cuCtxCreate_v4(CUcontext* context, CUctxCreateParams* parameters,
                                        unsigned int flags, CUdevice device);
MF_CUDA_ABI_API CUresult cuCtxDestroy(CUcontext context);
MF_CUDA_ABI_API CUresult cuCtxDestroy_v2(CUcontext context);
MF_CUDA_ABI_API CUresult cuCtxSetCurrent(CUcontext context);
MF_CUDA_ABI_API CUresult cuCtxGetCurrent(CUcontext* context);
MF_CUDA_ABI_API CUresult cuCtxGetDevice(CUdevice* device);
MF_CUDA_ABI_API CUresult cuCtxPushCurrent(CUcontext context);
MF_CUDA_ABI_API CUresult cuCtxPushCurrent_v2(CUcontext context);
MF_CUDA_ABI_API CUresult cuCtxPopCurrent(CUcontext* context);
MF_CUDA_ABI_API CUresult cuCtxPopCurrent_v2(CUcontext* context);
MF_CUDA_ABI_API CUresult cuCtxSynchronize(void);

MF_CUDA_ABI_API CUresult cuModuleLoadData(CUmodule* module, const void* image);
MF_CUDA_ABI_API CUresult cuModuleUnload(CUmodule module);
MF_CUDA_ABI_API CUresult cuModuleGetFunction(CUfunction* function, CUmodule module,
                                             const char* name);
MF_CUDA_ABI_API CUresult cuModuleGetLoadingMode(CUmoduleLoadingMode* mode);

MF_CUDA_ABI_API CUresult cuLibraryLoadData(CUlibrary* library, const void* code,
                                           CUjit_option* jit_options,
                                           void** jit_option_values, unsigned int num_jit_options,
                                           CUjit_option* library_options,
                                           void** library_option_values,
                                           unsigned int num_library_options);
MF_CUDA_ABI_API CUresult cuLibraryLoadFromFile(CUlibrary* library, const char* file_name,
                                               CUjit_option* jit_options,
                                               void** jit_option_values,
                                               unsigned int num_jit_options,
                                               CUjit_option* library_options,
                                               void** library_option_values,
                                               unsigned int num_library_options);
MF_CUDA_ABI_API CUresult cuLibraryUnload(CUlibrary library);
MF_CUDA_ABI_API CUresult cuLibraryGetKernel(CUkernel* kernel, CUlibrary library,
                                            const char* name);
MF_CUDA_ABI_API CUresult cuLibraryGetModule(CUmodule* module, CUlibrary library);
MF_CUDA_ABI_API CUresult cuLibraryGetGlobal(CUdeviceptr* dptr, size_t* bytes, CUlibrary library,
                                            const char* name);
MF_CUDA_ABI_API CUresult cuLibraryGetManaged(CUdeviceptr* dptr, size_t* bytes, CUlibrary library,
                                             const char* name);
MF_CUDA_ABI_API CUresult cuLibraryGetUnifiedFunction(void** fptr, CUlibrary library,
                                                     const char* name);
MF_CUDA_ABI_API CUresult cuLibraryGetKernelCount(unsigned int* count, CUlibrary library);
MF_CUDA_ABI_API CUresult cuLibraryEnumerateKernels(CUkernel* kernels, unsigned int num_kernels,
                                                   CUlibrary library);
MF_CUDA_ABI_API CUresult cuKernelGetFunction(CUfunction* function, CUkernel kernel);
MF_CUDA_ABI_API CUresult cuKernelGetAttribute(int* pi, CUfunction_attribute attrib,
                                              CUkernel kernel, CUdevice device);
MF_CUDA_ABI_API CUresult cuKernelSetAttribute(CUfunction_attribute attrib, int value,
                                              CUkernel kernel, CUdevice device);
MF_CUDA_ABI_API CUresult cuKernelSetCacheConfig(CUfunc_config config, CUkernel kernel,
                                                CUdevice device);
MF_CUDA_ABI_API CUresult cuKernelGetName(const char** name, CUkernel kernel);
MF_CUDA_ABI_API CUresult cuKernelGetParamInfo(CUkernel kernel, size_t index, size_t* param_offset,
                                              size_t* param_size);

MF_CUDA_ABI_API CUresult cuDeviceGetP2PAttribute(int* value, CUdevice_P2PAttribute attrib,
                                                 CUdevice source_device,
                                                 CUdevice destination_device);
MF_CUDA_ABI_API CUresult cuDeviceGetAttribute(int* value, CUdevice_attribute attrib,
                                              CUdevice device);
MF_CUDA_ABI_API CUresult cuDeviceRegisterAsyncNotification(CUdevice device,
                                                           CUasyncCallback callback_func,
                                                           void* user_data,
                                                           CUasyncCallbackHandle* callback);
MF_CUDA_ABI_API CUresult cuDeviceUnregisterAsyncNotification(CUdevice device,
                                                             CUasyncCallbackHandle callback);
MF_CUDA_ABI_API CUresult cuGraphConditionalHandleCreate(CUgraphConditionalHandle* handle_out,
                                                        CUgraph graph, CUcontext context,
                                                        unsigned int flags);
MF_CUDA_ABI_API CUresult cuGraphExecNodeSetParams(CUgraphExec graph_exec, CUgraphNode node,
                                                  const CUDA_GRAPH_NODE_PARAMS* node_params);
MF_CUDA_ABI_API CUresult cuGraphNodeSetParams(CUgraphNode node,
                                              const CUDA_GRAPH_NODE_PARAMS* node_params);
MF_CUDA_ABI_API CUresult cuGraphAddNode(CUgraphNode* graph_node, CUgraph graph,
                                        const CUgraphNode* dependencies,
                                        const CUgraphEdgeData* dependency_data,
                                        size_t num_dependencies,
                                        CUgraphNodeParams* node_params);
MF_CUDA_ABI_API CUresult cuGraphExecGetFlags(CUgraphExec graph_exec, unsigned long long* flags);
MF_CUDA_ABI_API CUresult cuGraphInstantiateWithParams(CUgraphExec* graph_exec, CUgraph graph,
                                                      CUDA_GRAPH_INSTANTIATE_PARAMS*
                                                          instantiate_params);
MF_CUDA_ABI_API CUresult cuGraphUpload(CUgraphExec graph_exec, CUstream stream);
MF_CUDA_ABI_API CUresult cuGraphNodeSetEnabled(CUgraphNode node, int enabled);
MF_CUDA_ABI_API CUresult cuGraphNodeGetEnabled(CUgraphNode node, int* enabled);
MF_CUDA_ABI_API CUresult cuUserObjectCreate(CUuserObject* object_out, void* ptr, CUhostFn destroy,
                                            unsigned int initial_refcount, unsigned int flags);
MF_CUDA_ABI_API CUresult cuUserObjectRetain(CUuserObject object, unsigned int count);
MF_CUDA_ABI_API CUresult cuUserObjectRelease(CUuserObject object, unsigned int count);
MF_CUDA_ABI_API CUresult cuGraphRetainUserObject(CUgraph graph, CUuserObject object,
                                                 unsigned int count, unsigned int flags);
MF_CUDA_ABI_API CUresult cuGraphReleaseUserObject(CUgraph graph, CUuserObject object,
                                                  unsigned int count);
MF_CUDA_ABI_API CUresult cuGraphDebugDotPrint(CUgraph graph, const char* path, unsigned int flags);
MF_CUDA_ABI_API CUresult cuGraphKernelNodeSetAttribute(CUgraphNode node, CUfunction_attribute attrib,
                                                       const void* value);
MF_CUDA_ABI_API CUresult cuGraphKernelNodeGetAttribute(CUgraphNode node, CUfunction_attribute attrib,
                                                       void* value);
MF_CUDA_ABI_API CUresult cuGraphKernelNodeCopyAttributes(CUgraphNode destination,
                                                         CUgraphNode source);
MF_CUDA_ABI_API CUresult cuGraphExecUpdate(CUgraphExec graph_exec, CUgraph graph,
                                           CUgraphExecUpdateResultInfo* result_info);
MF_CUDA_ABI_API CUresult cuStreamCreateWithPriority(CUstream* phStream, unsigned int flags, int priority);
MF_CUDA_ABI_API CUresult cuStreamGetPriority(CUstream hStream, int* priority);
MF_CUDA_ABI_API CUresult cuStreamGetFlags(CUstream hStream, unsigned int* flags);
MF_CUDA_ABI_API CUresult cuStreamGetCtx(CUstream hStream, CUcontext* pctx);
MF_CUDA_ABI_API CUresult cuStreamGetId(CUstream hStream, unsigned long long* streamId);
MF_CUDA_ABI_API CUresult cuStreamAddCallback(CUstream hStream, CUstreamCallback callback, void* userData, unsigned int flags);
MF_CUDA_ABI_API CUresult cuStreamAttachMemAsync(CUstream hStream, CUdeviceptr dptr, size_t length, unsigned int flags);
MF_CUDA_ABI_API CUresult cuStreamCopyAttributes(CUstream dst, CUstream src);
MF_CUDA_ABI_API CUresult cuStreamGetAttribute(CUstream hStream, CUstreamAttrID attr, CUstreamAttrValue* value_out);
MF_CUDA_ABI_API CUresult cuStreamSetAttribute(CUstream hStream, CUstreamAttrID attr, const CUstreamAttrValue* value);
MF_CUDA_ABI_API CUresult cuStreamWaitValue32(CUstream stream, CUdeviceptr addr, unsigned int value, unsigned int flags);
MF_CUDA_ABI_API CUresult cuStreamWaitValue64(CUstream stream, CUdeviceptr addr, unsigned long long value, unsigned int flags);
MF_CUDA_ABI_API CUresult cuStreamWriteValue32(CUstream stream, CUdeviceptr addr, unsigned int value, unsigned int flags);
MF_CUDA_ABI_API CUresult cuStreamWriteValue64(CUstream stream, CUdeviceptr addr, unsigned long long value, unsigned int flags);
MF_CUDA_ABI_API CUresult cuStreamBatchMemOp(CUstream stream, unsigned int count, CUstreamBatchMemOpParams* param_array, unsigned int flags);
MF_CUDA_ABI_API CUresult cuCtxGetFlags(unsigned int* flags);
MF_CUDA_ABI_API CUresult cuCtxDetach(CUcontext ctx);
MF_CUDA_ABI_API CUresult cuCtxGetApiVersion(CUcontext ctx, unsigned int* version);
MF_CUDA_ABI_API CUresult cuCtxSetLimit(CUlimit limit, size_t value);
MF_CUDA_ABI_API CUresult cuCtxGetLimit(size_t* pvalue, CUlimit limit);
MF_CUDA_ABI_API CUresult cuCtxGetCacheConfig(CUfunc_cache* pconfig);
MF_CUDA_ABI_API CUresult cuCtxSetCacheConfig(CUfunc_cache config);
MF_CUDA_ABI_API CUresult cuCtxGetSharedMemConfig(CUsharedconfig* pconfig);
MF_CUDA_ABI_API CUresult cuCtxSetSharedMemConfig(CUsharedconfig config);
MF_CUDA_ABI_API CUresult cuCtxGetStreamPriorityRange(int* least_priority, int* greatest_priority);
MF_CUDA_ABI_API CUresult cuCtxEnablePeerAccess(CUcontext peer_context, unsigned int flags);
MF_CUDA_ABI_API CUresult cuCtxDisablePeerAccess(CUcontext peer_context);
MF_CUDA_ABI_API CUresult cuMemGetInfo(size_t* free_bytes, size_t* total_bytes);
MF_CUDA_ABI_API CUresult cuMemAllocManaged(CUdeviceptr* dptr, size_t bytesize, unsigned int flags);
MF_CUDA_ABI_API CUresult cuMemAllocPitch(CUdeviceptr* dptr, size_t* pPitch, size_t WidthInBytes, size_t Height, unsigned int ElementSizeBytes);
MF_CUDA_ABI_API CUresult cuMemGetAddressRange(CUdeviceptr* pbase, size_t* psize, CUdeviceptr dptr);
MF_CUDA_ABI_API CUresult cuMemFreeHost(void* p);
MF_CUDA_ABI_API CUresult cuMemHostAlloc(void** pp, size_t bytesize, unsigned int Flags);
MF_CUDA_ABI_API CUresult cuMemHostGetDevicePointer(CUdeviceptr* pdptr, void* p, unsigned int Flags);
MF_CUDA_ABI_API CUresult cuMemHostGetFlags(unsigned int* pFlags, void* p);
MF_CUDA_ABI_API CUresult cuMemHostRegister(void* p, size_t bytesize, unsigned int Flags);
MF_CUDA_ABI_API CUresult cuMemHostUnregister(void* p);
MF_CUDA_ABI_API CUresult cuMemcpy2D(const CUDA_MEMCPY2D* pCopy);
MF_CUDA_ABI_API CUresult cuMemcpy2DAsync(const CUDA_MEMCPY2D* pCopy, CUstream hStream);
MF_CUDA_ABI_API CUresult cuMemcpy3D(const CUDA_MEMCPY3D* pCopy);
MF_CUDA_ABI_API CUresult cuMemcpy3DAsync(const CUDA_MEMCPY3D* pCopy, CUstream hStream);
MF_CUDA_ABI_API CUresult cuMemcpyPeer(CUdeviceptr dstDevice, CUcontext dstContext, CUdeviceptr srcDevice, CUcontext srcContext, size_t ByteCount);
MF_CUDA_ABI_API CUresult cuMemcpyPeerAsync(CUdeviceptr dstDevice, CUcontext dstContext, CUdeviceptr srcDevice, CUcontext srcContext, size_t ByteCount, CUstream hStream);
MF_CUDA_ABI_API CUresult cuMemsetD8(CUdeviceptr dstDevice, unsigned char uc, size_t N);
MF_CUDA_ABI_API CUresult cuMemsetD8Async(CUdeviceptr dstDevice, unsigned char uc, size_t N, CUstream hStream);
MF_CUDA_ABI_API CUresult cuMemsetD2D8(CUdeviceptr dstDevice, size_t dstPitch, unsigned char uc, size_t Width, size_t Height);
MF_CUDA_ABI_API CUresult cuMemsetD2D8Async(CUdeviceptr dstDevice, size_t dstPitch, unsigned char uc, size_t Width, size_t Height, CUstream hStream);
MF_CUDA_ABI_API CUresult cuPointerGetAttribute(void* data, CUpointer_attribute attribute, CUdeviceptr ptr);
MF_CUDA_ABI_API CUresult cuPointerGetAttributes(unsigned int numAttributes, CUpointer_attribute* attributes, void** data, CUdeviceptr ptr);
MF_CUDA_ABI_API CUresult cuFuncGetAttribute(int* pi, CUfunction_attribute attrib, CUfunction hfunc);
MF_CUDA_ABI_API CUresult cuFuncSetAttribute(CUfunction hfunc, CUfunction_attribute attrib, int value);
MF_CUDA_ABI_API CUresult cuFuncSetCacheConfig(CUfunction hfunc, CUfunc_cache config);
MF_CUDA_ABI_API CUresult cuFuncSetSharedMemConfig(CUfunction hfunc, CUsharedconfig config);
MF_CUDA_ABI_API CUresult cuDeviceCanAccessPeer(int* canAccessPeer, CUdevice dev, CUdevice peerDev);
MF_CUDA_ABI_API CUresult cuOccupancyMaxActiveBlocksPerMultiprocessorWithFlags(int* numBlocks, CUfunction func, int blockSize, size_t dynamicSMemSize, unsigned int flags);
MF_CUDA_ABI_API CUresult cuProfilerInitialize(const char* configFile, const char* outputFile, unsigned int outputMode);
MF_CUDA_ABI_API CUresult cuProfilerStart(void);
MF_CUDA_ABI_API CUresult cuProfilerStop(void);
MF_CUDA_ABI_API CUresult cuGetExportTable(const void** ppExportTable, const CUuuid* pExportTableId);

MF_CUDA_ABI_API CUresult cuThreadExchangeStreamCaptureMode(CUstreamCaptureMode* mode);
MF_CUDA_ABI_API CUresult cuGraphExecEventWaitNodeSetEvent(CUgraphExec graph_exec, CUgraphNode node,
                                                          CUevent event);
MF_CUDA_ABI_API CUresult cuGraphExecEventRecordNodeSetEvent(CUgraphExec graph_exec,
                                                            CUgraphNode node, CUevent event);
MF_CUDA_ABI_API CUresult cuGraphEventWaitNodeGetEvent(CUgraphNode node, CUevent* event_out);
MF_CUDA_ABI_API CUresult cuGraphEventRecordNodeGetEvent(CUgraphNode node, CUevent* event_out);
MF_CUDA_ABI_API CUresult cuGraphExecChildGraphNodeSetParams(CUgraphExec graph_exec,
                                                            CUgraphNode node,
                                                            CUgraph child_graph);
MF_CUDA_ABI_API CUresult cuGraphExecExternalSemaphoresSignalNodeSetParams(
    CUgraphExec graph_exec, CUgraphNode node, const CUDA_EXT_SEM_SIGNAL_NODE_PARAMS* params);
MF_CUDA_ABI_API CUresult cuGraphExecExternalSemaphoresWaitNodeSetParams(
    CUgraphExec graph_exec, CUgraphNode node, const CUDA_EXT_SEM_WAIT_NODE_PARAMS* params);
MF_CUDA_ABI_API CUresult cuGraphExecHostNodeSetParams(CUgraphExec graph_exec, CUgraphNode node,
                                                      const CUDA_HOST_NODE_PARAMS* params);
MF_CUDA_ABI_API CUresult cuGraphExecMemcpyNodeSetParams(CUgraphExec graph_exec, CUgraphNode node,
                                                        const CUDA_MEMCPY3D* params);
MF_CUDA_ABI_API CUresult cuGraphExecMemsetNodeSetParams(CUgraphExec graph_exec, CUgraphNode node,
                                                        const CUDA_MEMSET_NODE_PARAMS* params);
MF_CUDA_ABI_API CUresult cuGraphExecMemAllocNodeSetParams(CUgraphExec graph_exec, CUgraphNode node,
                                                          const CUDA_MEM_ALLOC_NODE_PARAMS*
                                                              params);
MF_CUDA_ABI_API CUresult cuGraphExecMemFreeNodeSetParams(CUgraphExec graph_exec, CUgraphNode node,
                                                         CUdeviceptr device_pointer);
MF_CUDA_ABI_API CUresult cuGraphExecKernelNodeSetParams(CUgraphExec graph_exec, CUgraphNode node,
                                                        const CUDA_KERNEL_NODE_PARAMS*
                                                            node_params);
MF_CUDA_ABI_API CUresult cuStreamUpdateCaptureDependencies(CUstream stream,
                                                           CUuserObject* dependencies,
                                                           unsigned int num_dependencies,
                                                           unsigned int flags);
MF_CUDA_ABI_API CUresult cuStreamGetCaptureInfo(CUstream stream,
                                                CUstreamCaptureStatus* capture_status,
                                                cuuint64_t* pid);
MF_CUDA_ABI_API CUresult cuStreamIsCapturing(CUstream stream, CUstreamCaptureStatus* capture_status);
MF_CUDA_ABI_API CUresult cuDeviceGetTexture1DLinearMaxWidth(size_t* max_width_in_elements,
                                                            CUarray_format format,
                                                            unsigned int num_channels,
                                                            CUdevice device);
MF_CUDA_ABI_API CUresult cuDeviceGetByPCIBusId(CUdevice* device, const char* pci_bus_id);
MF_CUDA_ABI_API CUresult cuDeviceGetDefaultMemPool(CUmemoryPool* pool, CUdevice device);
MF_CUDA_ABI_API CUresult cuDeviceSetMemPool(CUdevice device, CUmemoryPool pool);
MF_CUDA_ABI_API CUresult cuDeviceGetMemPool(CUmemoryPool* pool, CUdevice device);
MF_CUDA_ABI_API CUresult cuFlushGPUDirectRDMAWrites(CUflushGPUDirectRDMAWritesTarget target,
                                                    CUflushGPUDirectRDMAWritesMode mode,
                                                    unsigned int flags);
MF_CUDA_ABI_API CUresult cuCtxResetPersistingL2Cache(void);

MF_CUDA_ABI_API CUresult cuMemAlloc(CUdeviceptr_v1* device_pointer, unsigned int bytes);
MF_CUDA_ABI_API CUresult cuMemAlloc_v2(CUdeviceptr* device_pointer, size_t bytes);
MF_CUDA_ABI_API CUresult cuMemFree(CUdeviceptr_v1 device_pointer);
MF_CUDA_ABI_API CUresult cuMemFree_v2(CUdeviceptr device_pointer);
MF_CUDA_ABI_API CUresult cuMemcpyHtoD(CUdeviceptr_v1 destination, const void* source,
                                      unsigned int bytes);
MF_CUDA_ABI_API CUresult cuMemcpyHtoD_v2(CUdeviceptr destination, const void* source, size_t bytes);
MF_CUDA_ABI_API CUresult cuMemcpyHtoD_v2_ptds(CUdeviceptr destination, const void* source,
                                              size_t bytes);
MF_CUDA_ABI_API CUresult cuMemcpyDtoH(void* destination, CUdeviceptr_v1 source, unsigned int bytes);
MF_CUDA_ABI_API CUresult cuMemcpyDtoH_v2(void* destination, CUdeviceptr source, size_t bytes);
MF_CUDA_ABI_API CUresult cuMemcpyDtoH_v2_ptds(void* destination, CUdeviceptr source, size_t bytes);
MF_CUDA_ABI_API CUresult cuMemcpyDtoD(CUdeviceptr_v1 destination, CUdeviceptr_v1 source,
                                      unsigned int bytes);
MF_CUDA_ABI_API CUresult cuMemcpyDtoD_v2(CUdeviceptr destination, CUdeviceptr source, size_t bytes);
MF_CUDA_ABI_API CUresult cuMemcpyDtoD_v2_ptds(CUdeviceptr destination, CUdeviceptr source,
                                              size_t bytes);

MF_CUDA_ABI_API CUresult cuMemcpyHtoDAsync(CUdeviceptr_v1 destination, const void* source,
                                           unsigned int bytes, CUstream stream);
MF_CUDA_ABI_API CUresult cuMemcpyHtoDAsync_v2(CUdeviceptr destination, const void* source,
                                              size_t bytes, CUstream stream);
MF_CUDA_ABI_API CUresult cuMemcpyHtoDAsync_v2_ptsz(CUdeviceptr destination, const void* source,
                                                   size_t bytes, CUstream stream);
MF_CUDA_ABI_API CUresult cuMemcpyDtoHAsync(void* destination, CUdeviceptr_v1 source,
                                           unsigned int bytes, CUstream stream);
MF_CUDA_ABI_API CUresult cuMemcpyDtoHAsync_v2(void* destination, CUdeviceptr source, size_t bytes,
                                              CUstream stream);
MF_CUDA_ABI_API CUresult cuMemcpyDtoHAsync_v2_ptsz(void* destination, CUdeviceptr source,
                                                   size_t bytes, CUstream stream);
MF_CUDA_ABI_API CUresult cuMemcpyDtoDAsync(CUdeviceptr_v1 destination, CUdeviceptr_v1 source,
                                           unsigned int bytes, CUstream stream);
MF_CUDA_ABI_API CUresult cuMemcpyDtoDAsync_v2(CUdeviceptr destination, CUdeviceptr source,
                                              size_t bytes, CUstream stream);
MF_CUDA_ABI_API CUresult cuMemcpyDtoDAsync_v2_ptsz(CUdeviceptr destination, CUdeviceptr source,
                                                   size_t bytes, CUstream stream);

MF_CUDA_ABI_API CUresult cuStreamCreate(CUstream* stream, unsigned int flags);
MF_CUDA_ABI_API CUresult cuStreamQuery(CUstream stream);
MF_CUDA_ABI_API CUresult cuStreamQuery_ptsz(CUstream stream);
MF_CUDA_ABI_API CUresult cuStreamSynchronize(CUstream stream);
MF_CUDA_ABI_API CUresult cuStreamSynchronize_ptsz(CUstream stream);
MF_CUDA_ABI_API CUresult cuStreamWaitEvent(CUstream stream, CUevent event, unsigned int flags);
MF_CUDA_ABI_API CUresult cuStreamWaitEvent_ptsz(CUstream stream, CUevent event, unsigned int flags);
MF_CUDA_ABI_API CUresult cuStreamDestroy(CUstream stream);
MF_CUDA_ABI_API CUresult cuStreamDestroy_v2(CUstream stream);
MF_CUDA_ABI_API CUresult cuEventCreate(CUevent* event, unsigned int flags);
MF_CUDA_ABI_API CUresult cuEventRecord(CUevent event, CUstream stream);
MF_CUDA_ABI_API CUresult cuEventRecord_ptsz(CUevent event, CUstream stream);
MF_CUDA_ABI_API CUresult cuEventQuery(CUevent event);
MF_CUDA_ABI_API CUresult cuEventSynchronize(CUevent event);
MF_CUDA_ABI_API CUresult cuEventDestroy(CUevent event);
MF_CUDA_ABI_API CUresult cuEventDestroy_v2(CUevent event);
MF_CUDA_ABI_API CUresult cuEventElapsedTime(float* milliseconds, CUevent start, CUevent end);

MF_CUDA_ABI_API CUresult cuLaunchKernel(CUfunction function, unsigned int grid_x,
                                        unsigned int grid_y, unsigned int grid_z,
                                        unsigned int block_x, unsigned int block_y,
                                        unsigned int block_z, unsigned int shared_memory_bytes,
                                        CUstream stream, void** kernel_parameters, void** extra);
MF_CUDA_ABI_API CUresult cuLaunchKernel_ptsz(CUfunction function, unsigned int grid_x,
                                             unsigned int grid_y, unsigned int grid_z,
                                             unsigned int block_x, unsigned int block_y,
                                             unsigned int block_z, unsigned int shared_memory_bytes,
                                             CUstream stream, void** kernel_parameters,
                                             void** extra);

MF_CUDA_ABI_API CUresult cuGetErrorName(CUresult error, const char** name);
MF_CUDA_ABI_API CUresult cuGetErrorString(CUresult error, const char** description);
MF_CUDA_ABI_API CUresult cuGetProcAddress(const char* symbol, void** function, int cuda_version,
                                          cuuint64_t flags);
MF_CUDA_ABI_API CUresult cuGetProcAddress_v2(const char* symbol, void** function, int cuda_version,
                                             cuuint64_t flags,
                                             CUdriverProcAddressQueryResult* symbol_status);

#if !defined(METAFLUX_CUDA_ABI_INTERNAL)
#define cuDeviceTotalMem cuDeviceTotalMem_v2
#define cuCtxCreate cuCtxCreate_v2
#define cuCtxDestroy cuCtxDestroy_v2
#define cuCtxPushCurrent cuCtxPushCurrent_v2
#define cuCtxPopCurrent cuCtxPopCurrent_v2
#define cuDevicePrimaryCtxRelease cuDevicePrimaryCtxRelease_v2
#define cuDevicePrimaryCtxReset cuDevicePrimaryCtxReset_v2
#define cuMemAlloc cuMemAlloc_v2
#define cuMemFree cuMemFree_v2
#define cuMemcpyHtoD cuMemcpyHtoD_v2
#define cuMemcpyDtoH cuMemcpyDtoH_v2
#define cuMemcpyDtoD cuMemcpyDtoD_v2
#define cuMemcpyHtoDAsync cuMemcpyHtoDAsync_v2
#define cuMemcpyDtoHAsync cuMemcpyDtoHAsync_v2
#define cuMemcpyDtoDAsync cuMemcpyDtoDAsync_v2
#define cuStreamDestroy cuStreamDestroy_v2
#define cuEventDestroy cuEventDestroy_v2
#define cuGetProcAddress cuGetProcAddress_v2
#endif

#ifdef __cplusplus
}
#endif

#endif
