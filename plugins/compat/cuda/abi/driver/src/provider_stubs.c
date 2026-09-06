/* GENERATED typed surface stubs: symbols advertised by symbols.def whose
   managed implementations are not yet functional. Read-only queries report
   safe zero defaults; everything else returns CUDA_ERROR_NOT_SUPPORTED so
   callers observe a typed, deterministic gap. Entries graduate to
   functional implementations in provider.c through work-item-0.2.0.1.
   Generated against the R610 frozen signatures. */

#define METAFLUX_CUDA_ABI_INTERNAL 1
#include "metaflux/cuda/abi.h"

#include "managed-renames.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int mf_stub_trace_enabled = -1;

static int mf_stub_trace(void) {
  if (mf_stub_trace_enabled < 0) {
    mf_stub_trace_enabled = getenv("METAFLUX_TRACE_STUBS") != NULL ? 1 : 0;
  }
  return mf_stub_trace_enabled;
}

#define MF_STUB_TRACE \
  do { \
    if (mf_stub_trace()) { \
      fprintf(stderr, "MF_STUB_CALL %s\n", __func__); \
    } \
  } while (0)

CUresult cuCtxDetach(CUcontext ctx) {
  MF_STUB_TRACE;
  (void)ctx;

  return CUDA_ERROR_NOT_SUPPORTED;
}

CUresult cuCtxDisablePeerAccess(CUcontext peer_context) {
  MF_STUB_TRACE;
  (void)peer_context;

  return CUDA_ERROR_NOT_SUPPORTED;
}

CUresult cuCtxEnablePeerAccess(CUcontext peer_context, unsigned int flags) {
  MF_STUB_TRACE;
  (void)peer_context;
  (void)flags;

  return CUDA_ERROR_NOT_SUPPORTED;
}

CUresult cuCtxGetApiVersion(CUcontext ctx, unsigned int* version) {
  MF_STUB_TRACE;
  (void)ctx;
  if (version == (unsigned int*)0) {
    return CUDA_ERROR_INVALID_VALUE;
  }
  *version = 12000;
  return CUDA_SUCCESS;

}

CUresult cuCtxGetCacheConfig(CUfunc_cache* pconfig) {
  MF_STUB_TRACE;
  if (pconfig == (CUfunc_cache*)0) {
    return CUDA_ERROR_INVALID_VALUE;
  }
  *pconfig = CU_FUNC_CACHE_PREFER_NONE;
  return CUDA_SUCCESS;

}

CUresult cuCtxGetFlags(unsigned int* flags) {
  MF_STUB_TRACE;
  if (flags == (unsigned int*)0) {
    return CUDA_ERROR_INVALID_VALUE;
  }
  *flags = 0;
  return CUDA_SUCCESS;

}

CUresult cuCtxGetLimit(size_t* pvalue, CUlimit limit) {
  MF_STUB_TRACE;
  (void)limit;
  if (pvalue == (size_t*)0) {
    return CUDA_ERROR_INVALID_VALUE;
  }
  *pvalue = 0;
  return CUDA_SUCCESS;

}

CUresult cuCtxGetSharedMemConfig(CUsharedconfig* pconfig) {
  MF_STUB_TRACE;
  if (pconfig == (CUsharedconfig*)0) {
    return CUDA_ERROR_INVALID_VALUE;
  }
  *pconfig = CU_SHARED_MEM_CONFIG_DEFAULT_BANK_SIZE;
  return CUDA_SUCCESS;

}

CUresult cuCtxGetStreamPriorityRange(int* least_priority, int* greatest_priority) {
  MF_STUB_TRACE;
  if (least_priority == (int*)0 || greatest_priority == (int*)0) {
    return CUDA_ERROR_INVALID_VALUE;
  }
  *least_priority = 0;
  *greatest_priority = 0;
  return CUDA_SUCCESS;

}

CUresult cuCtxSetCacheConfig(CUfunc_cache config) {
  MF_STUB_TRACE;
  (void)config;

  return CUDA_ERROR_NOT_SUPPORTED;
}

CUresult cuCtxSetLimit(CUlimit limit, size_t value) {
  MF_STUB_TRACE;
  (void)limit;
  (void)value;

  return CUDA_ERROR_NOT_SUPPORTED;
}

CUresult cuCtxSetSharedMemConfig(CUsharedconfig config) {
  MF_STUB_TRACE;
  (void)config;

  return CUDA_ERROR_NOT_SUPPORTED;
}

CUresult cuDeviceCanAccessPeer(int* canAccessPeer, CUdevice dev, CUdevice peerDev) {
  MF_STUB_TRACE;
  (void)dev;
  (void)peerDev;
  if (canAccessPeer == (int*)0) {
    return CUDA_ERROR_INVALID_VALUE;
  }
  *canAccessPeer = 0;
  return CUDA_SUCCESS;

}

CUresult cuFuncGetAttribute(int* pi, CUfunction_attribute attrib, CUfunction hfunc) {
  MF_STUB_TRACE;
  (void)pi;
  (void)attrib;
  (void)hfunc;

  return CUDA_ERROR_NOT_SUPPORTED;
}

CUresult cuFuncSetAttribute(CUfunction hfunc, CUfunction_attribute attrib, int value) {
  MF_STUB_TRACE;
  (void)hfunc;
  (void)attrib;
  (void)value;

  return CUDA_ERROR_NOT_SUPPORTED;
}

CUresult cuFuncSetCacheConfig(CUfunction hfunc, CUfunc_cache config) {
  MF_STUB_TRACE;
  (void)hfunc;
  (void)config;

  return CUDA_ERROR_NOT_SUPPORTED;
}

CUresult cuFuncSetSharedMemConfig(CUfunction hfunc, CUsharedconfig config) {
  MF_STUB_TRACE;
  (void)hfunc;
  (void)config;

  return CUDA_ERROR_NOT_SUPPORTED;
}

static CUresult mf_export_init_0x10(void* sub_struct, unsigned int mode) {
  (void)sub_struct;
  (void)mode;
  return CUDA_SUCCESS;
}

static void* mf_export_ops[8];
static void* mf_export_vtable[64];

static const unsigned char mf_uuid_a094[16] = {
    0xa0, 0x94, 0x79, 0x8c, 0x2e, 0x74, 0x2e, 0x74,
    0x93, 0xf2, 0x08, 0x00, 0x20, 0x0c, 0x0a, 0x66};

CUresult cuGetExportTable(const void** ppExportTable, const CUuuid* pExportTableId) {
  MF_STUB_TRACE;
  if (pExportTableId == (const CUuuid*)0 || ppExportTable == (const void**)0) {
    return CUDA_ERROR_INVALID_VALUE;
  }
  /* UUID a094798c-...: the driver ops table whose +0x10 entry cudart invokes
     during one-time initialization. Its entries carry real semantics that are
     not implemented yet, so it stays NOT_FOUND (clean failure) instead of a
     partially populated table that segfaults libcudart. */
  if (memcmp(pExportTableId->bytes, mf_uuid_a094, 16) == 0) {
    return CUDA_ERROR_NOT_FOUND;
  }
  /* UUID 6bd5fb6c-...: the general driver vtable; slot[2] (offset 0x10) is
     the initialization entry cudart exercises. */
  mf_export_ops[2] = (void*)&mf_export_init_0x10;
  mf_export_vtable[14] = (void*)mf_export_ops;
  *ppExportTable = (const void*)mf_export_vtable;
  return CUDA_SUCCESS;
}

CUresult cuMemAllocManaged(CUdeviceptr* dptr, size_t bytesize, unsigned int flags) {
  MF_STUB_TRACE;
  (void)dptr;
  (void)bytesize;
  (void)flags;

  return CUDA_ERROR_NOT_SUPPORTED;
}

CUresult cuMemAllocPitch(CUdeviceptr* dptr, size_t* pPitch, size_t WidthInBytes, size_t Height, unsigned int ElementSizeBytes) {
  MF_STUB_TRACE;
  (void)dptr;
  (void)pPitch;
  (void)WidthInBytes;
  (void)Height;
  (void)ElementSizeBytes;

  return CUDA_ERROR_NOT_SUPPORTED;
}

CUresult cuMemFreeHost(void* p) {
  MF_STUB_TRACE;
  (void)p;

  return CUDA_ERROR_NOT_SUPPORTED;
}

CUresult cuMemGetAddressRange(CUdeviceptr* pbase, size_t* psize, CUdeviceptr dptr) {
  MF_STUB_TRACE;
  (void)pbase;
  (void)psize;
  (void)dptr;

  return CUDA_ERROR_NOT_SUPPORTED;
}

CUresult cuMemGetInfo(size_t* free_bytes, size_t* total_bytes) {
  MF_STUB_TRACE;
  (void)free_bytes;
  (void)total_bytes;

  return CUDA_ERROR_NOT_SUPPORTED;
}

CUresult cuMemHostAlloc(void** pp, size_t bytesize, unsigned int Flags) {
  MF_STUB_TRACE;
  (void)pp;
  (void)bytesize;
  (void)Flags;

  return CUDA_ERROR_NOT_SUPPORTED;
}

CUresult cuMemHostGetDevicePointer(CUdeviceptr* pdptr, void* p, unsigned int Flags) {
  MF_STUB_TRACE;
  (void)pdptr;
  (void)p;
  (void)Flags;

  return CUDA_ERROR_NOT_SUPPORTED;
}

CUresult cuMemHostGetFlags(unsigned int* pFlags, void* p) {
  MF_STUB_TRACE;
  (void)p;
  if (pFlags == (unsigned int*)0) {
    return CUDA_ERROR_INVALID_VALUE;
  }
  *pFlags = 0;
  return CUDA_SUCCESS;

}

CUresult cuMemHostRegister(void* p, size_t bytesize, unsigned int Flags) {
  MF_STUB_TRACE;
  (void)p;
  (void)bytesize;
  (void)Flags;

  return CUDA_ERROR_NOT_SUPPORTED;
}

CUresult cuMemHostUnregister(void* p) {
  MF_STUB_TRACE;
  (void)p;

  return CUDA_ERROR_NOT_SUPPORTED;
}

CUresult cuMemcpy2D(const CUDA_MEMCPY2D* pCopy) {
  MF_STUB_TRACE;
  (void)pCopy;

  return CUDA_ERROR_NOT_SUPPORTED;
}

CUresult cuMemcpy2DAsync(const CUDA_MEMCPY2D* pCopy, CUstream hStream) {
  MF_STUB_TRACE;
  (void)pCopy;
  (void)hStream;

  return CUDA_ERROR_NOT_SUPPORTED;
}

CUresult cuMemcpy3D(const CUDA_MEMCPY3D* pCopy) {
  MF_STUB_TRACE;
  (void)pCopy;

  return CUDA_ERROR_NOT_SUPPORTED;
}

CUresult cuMemcpy3DAsync(const CUDA_MEMCPY3D* pCopy, CUstream hStream) {
  MF_STUB_TRACE;
  (void)pCopy;
  (void)hStream;

  return CUDA_ERROR_NOT_SUPPORTED;
}

CUresult cuMemcpyPeer(CUdeviceptr dstDevice, CUcontext dstContext, CUdeviceptr srcDevice, CUcontext srcContext, size_t ByteCount) {
  MF_STUB_TRACE;
  (void)dstDevice;
  (void)dstContext;
  (void)srcDevice;
  (void)srcContext;
  (void)ByteCount;

  return CUDA_ERROR_NOT_SUPPORTED;
}

CUresult cuMemcpyPeerAsync(CUdeviceptr dstDevice, CUcontext dstContext, CUdeviceptr srcDevice, CUcontext srcContext, size_t ByteCount, CUstream hStream) {
  MF_STUB_TRACE;
  (void)dstDevice;
  (void)dstContext;
  (void)srcDevice;
  (void)srcContext;
  (void)ByteCount;
  (void)hStream;

  return CUDA_ERROR_NOT_SUPPORTED;
}

CUresult cuMemsetD2D8(CUdeviceptr dstDevice, size_t dstPitch, unsigned char uc, size_t Width, size_t Height) {
  MF_STUB_TRACE;
  (void)dstDevice;
  (void)dstPitch;
  (void)uc;
  (void)Width;
  (void)Height;

  return CUDA_ERROR_NOT_SUPPORTED;
}

CUresult cuMemsetD2D8Async(CUdeviceptr dstDevice, size_t dstPitch, unsigned char uc, size_t Width, size_t Height, CUstream hStream) {
  MF_STUB_TRACE;
  (void)dstDevice;
  (void)dstPitch;
  (void)uc;
  (void)Width;
  (void)Height;
  (void)hStream;

  return CUDA_ERROR_NOT_SUPPORTED;
}

CUresult cuMemsetD8(CUdeviceptr dstDevice, unsigned char uc, size_t N) {
  MF_STUB_TRACE;
  (void)dstDevice;
  (void)uc;
  (void)N;

  return CUDA_ERROR_NOT_SUPPORTED;
}

CUresult cuMemsetD8Async(CUdeviceptr dstDevice, unsigned char uc, size_t N, CUstream hStream) {
  MF_STUB_TRACE;
  (void)dstDevice;
  (void)uc;
  (void)N;
  (void)hStream;

  return CUDA_ERROR_NOT_SUPPORTED;
}

CUresult cuOccupancyMaxActiveBlocksPerMultiprocessorWithFlags(int* numBlocks, CUfunction func, int blockSize, size_t dynamicSMemSize, unsigned int flags) {
  MF_STUB_TRACE;
  (void)func;
  (void)blockSize;
  (void)dynamicSMemSize;
  (void)flags;
  if (numBlocks == (int*)0) {
    return CUDA_ERROR_INVALID_VALUE;
  }
  *numBlocks = 1;
  return CUDA_SUCCESS;

}

CUresult cuPointerGetAttribute(void* data, CUpointer_attribute attribute, CUdeviceptr ptr) {
  MF_STUB_TRACE;
  (void)data;
  (void)attribute;
  (void)ptr;

  return CUDA_ERROR_NOT_SUPPORTED;
}

CUresult cuPointerGetAttributes(unsigned int numAttributes, CUpointer_attribute* attributes, void** data, CUdeviceptr ptr) {
  MF_STUB_TRACE;
  (void)numAttributes;
  (void)attributes;
  (void)data;
  (void)ptr;

  return CUDA_ERROR_NOT_SUPPORTED;
}

CUresult cuProfilerInitialize(const char* configFile, const char* outputFile, unsigned int outputMode) {
  MF_STUB_TRACE;
  (void)configFile;
  (void)outputFile;
  (void)outputMode;

  return CUDA_ERROR_NOT_SUPPORTED;
}

CUresult cuProfilerStart(void) {
  MF_STUB_TRACE;

  return CUDA_ERROR_NOT_SUPPORTED;
}

CUresult cuProfilerStop(void) {
  MF_STUB_TRACE;

  return CUDA_ERROR_NOT_SUPPORTED;
}

CUresult cuStreamAddCallback(CUstream hStream, CUstreamCallback callback, void* userData, unsigned int flags) {
  MF_STUB_TRACE;
  (void)hStream;
  (void)callback;
  (void)userData;
  (void)flags;

  return CUDA_ERROR_NOT_SUPPORTED;
}

CUresult cuStreamAttachMemAsync(CUstream hStream, CUdeviceptr dptr, size_t length, unsigned int flags) {
  MF_STUB_TRACE;
  (void)hStream;
  (void)dptr;
  (void)length;
  (void)flags;

  return CUDA_ERROR_NOT_SUPPORTED;
}

CUresult cuStreamBatchMemOp(CUstream stream, unsigned int count, CUstreamBatchMemOpParams* param_array, unsigned int flags) {
  MF_STUB_TRACE;
  (void)stream;
  (void)count;
  (void)param_array;
  (void)flags;

  return CUDA_ERROR_NOT_SUPPORTED;
}

CUresult cuStreamCopyAttributes(CUstream dst, CUstream src) {
  MF_STUB_TRACE;
  (void)dst;
  (void)src;

  return CUDA_ERROR_NOT_SUPPORTED;
}

CUresult cuStreamGetAttribute(CUstream hStream, CUstreamAttrID attr, CUstreamAttrValue* value_out) {
  MF_STUB_TRACE;
  (void)hStream;
  (void)attr;
  (void)value_out;

  return CUDA_ERROR_NOT_SUPPORTED;
}

CUresult cuStreamGetCtx(CUstream hStream, CUcontext* pctx) {
  MF_STUB_TRACE;
  (void)hStream;
  (void)pctx;

  return CUDA_ERROR_NOT_SUPPORTED;
}

CUresult cuStreamGetFlags(CUstream hStream, unsigned int* flags) {
  MF_STUB_TRACE;
  (void)hStream;
  if (flags == (unsigned int*)0) {
    return CUDA_ERROR_INVALID_VALUE;
  }
  *flags = 0;
  return CUDA_SUCCESS;

}

CUresult cuStreamGetId(CUstream hStream, unsigned long long* streamId) {
  MF_STUB_TRACE;
  (void)hStream;
  (void)streamId;

  return CUDA_ERROR_NOT_SUPPORTED;
}

CUresult cuStreamSetAttribute(CUstream hStream, CUstreamAttrID attr, const CUstreamAttrValue* value) {
  MF_STUB_TRACE;
  (void)hStream;
  (void)attr;
  (void)value;

  return CUDA_ERROR_NOT_SUPPORTED;
}

CUresult cuStreamWaitValue32(CUstream stream, CUdeviceptr addr, unsigned int value, unsigned int flags) {
  MF_STUB_TRACE;
  (void)stream;
  (void)addr;
  (void)value;
  (void)flags;

  return CUDA_ERROR_NOT_SUPPORTED;
}

CUresult cuStreamWaitValue64(CUstream stream, CUdeviceptr addr, unsigned long long value, unsigned int flags) {
  MF_STUB_TRACE;
  (void)stream;
  (void)addr;
  (void)value;
  (void)flags;

  return CUDA_ERROR_NOT_SUPPORTED;
}

CUresult cuStreamWriteValue32(CUstream stream, CUdeviceptr addr, unsigned int value, unsigned int flags) {
  MF_STUB_TRACE;
  (void)stream;
  (void)addr;
  (void)value;
  (void)flags;

  return CUDA_ERROR_NOT_SUPPORTED;
}

CUresult cuStreamWriteValue64(CUstream stream, CUdeviceptr addr, unsigned long long value, unsigned int flags) {
  MF_STUB_TRACE;
  (void)stream;
  (void)addr;
  (void)value;
  (void)flags;

  return CUDA_ERROR_NOT_SUPPORTED;
}
