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

#define MF_CUDA_DRIVER_API_VERSION 13030

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
