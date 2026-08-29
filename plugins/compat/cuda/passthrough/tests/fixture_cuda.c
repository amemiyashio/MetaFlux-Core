#define METAFLUX_CUDA_ABI_INTERNAL 1
#include "metaflux/cuda/abi.h"

#include <dlfcn.h>
#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

typedef struct mf_fixture_cuda_counter {
  const char* name;
  atomic_uint_least64_t count;
} mf_fixture_cuda_counter;

#define MF_CUDA_INTERNAL(name)
#define MF_CUDA_SYMBOL(name, version, status, route, parameters, arguments) {#name, 0},
static mf_fixture_cuda_counter mf_fixture_cuda_counters[] = {
#include "../../abi/driver/symbols.def"
};
#undef MF_CUDA_SYMBOL
#undef MF_CUDA_INTERNAL

void mf_fixture_cuda_record_call(const char* name) {
  size_t index = 0;
  for (index = 0; index < sizeof(mf_fixture_cuda_counters) / sizeof(mf_fixture_cuda_counters[0]);
       ++index) {
    if (strcmp(name, mf_fixture_cuda_counters[index].name) == 0) {
      (void)atomic_fetch_add_explicit(&mf_fixture_cuda_counters[index].count, UINT64_C(1),
                                      memory_order_relaxed);
      return;
    }
  }
}

uint64_t mf_fixture_cuda_call_count(const char* name) {
  size_t index = 0;
  if (name == (const char*)0) {
    return UINT64_MAX;
  }
  for (index = 0; index < sizeof(mf_fixture_cuda_counters) / sizeof(mf_fixture_cuda_counters[0]);
       ++index) {
    if (strcmp(name, mf_fixture_cuda_counters[index].name) == 0) {
      return atomic_load_explicit(&mf_fixture_cuda_counters[index].count, memory_order_relaxed);
    }
  }
  return UINT64_MAX;
}

CUresult cuInit(unsigned int flags) {
  mf_fixture_cuda_record_call("cuInit");
  return flags == UINT32_C(0) ? CUDA_SUCCESS : CUDA_ERROR_INVALID_VALUE;
}

CUresult cuDriverGetVersion(int* version) {
  mf_fixture_cuda_record_call("cuDriverGetVersion");
  if (version == (int*)0) {
    return CUDA_ERROR_INVALID_VALUE;
  }
  *version = 12070;
  return CUDA_SUCCESS;
}

CUresult cuDeviceGetCount(int* count) {
  mf_fixture_cuda_record_call("cuDeviceGetCount");
  if (count == (int*)0) {
    return CUDA_ERROR_INVALID_VALUE;
  }
  *count = 1;
  return CUDA_SUCCESS;
}

CUresult cuDeviceGet(CUdevice* device, int ordinal) {
  mf_fixture_cuda_record_call("cuDeviceGet");
  if (device == (CUdevice*)0 || ordinal != 0) {
    return ordinal == 0 ? CUDA_ERROR_INVALID_VALUE : CUDA_ERROR_INVALID_DEVICE;
  }
  *device = 0;
  return CUDA_SUCCESS;
}

CUresult cuDeviceGetName(char* name, int length, CUdevice device) {
  static const char value[] = "MetaFlux Vendor Fixture";
  mf_fixture_cuda_record_call("cuDeviceGetName");
  if (name == (char*)0 || length < (int)sizeof(value) || device != 0) {
    return CUDA_ERROR_INVALID_VALUE;
  }
  (void)memcpy(name, value, sizeof(value));
  return CUDA_SUCCESS;
}

static CUresult mf_fixture_cuda_uuid(CUuuid* uuid, CUdevice device, const char* symbol) {
  size_t index = 0;
  mf_fixture_cuda_record_call(symbol);
  if (uuid == (CUuuid*)0 || device != 0) {
    return CUDA_ERROR_INVALID_VALUE;
  }
  for (index = 0; index < sizeof(uuid->bytes); ++index) {
    uuid->bytes[index] = (char)index;
  }
  return CUDA_SUCCESS;
}

CUresult cuDeviceGetUuid(CUuuid* uuid, CUdevice device) {
  return mf_fixture_cuda_uuid(uuid, device, "cuDeviceGetUuid");
}

CUresult cuDeviceGetUuid_v2(CUuuid* uuid, CUdevice device) {
  return mf_fixture_cuda_uuid(uuid, device, "cuDeviceGetUuid_v2");
}

static const char* mf_fixture_cuda_route(const char* symbol, int version, cuuint64_t flags) {
  if ((flags & (cuuint64_t)CU_GET_PROC_ADDRESS_PER_THREAD_DEFAULT_STREAM) != UINT64_C(0)) {
    if (strcmp(symbol, "cuLaunchKernel") == 0) {
      return "cuLaunchKernel_ptsz";
    }
    if (strcmp(symbol, "cuMemcpyHtoD_v2") == 0) {
      return "cuMemcpyHtoD_v2_ptds";
    }
  }
  if (strcmp(symbol, "cuMemAlloc") == 0 && version >= 3020) {
    return "cuMemAlloc_v2";
  }
  return symbol;
}

static CUresult mf_fixture_cuda_get_proc_address(const char* symbol, void** function, int version,
                                                 cuuint64_t flags,
                                                 CUdriverProcAddressQueryResult* symbol_status,
                                                 const char* called_symbol) {
  const cuuint64_t known_flags = (cuuint64_t)(CU_GET_PROC_ADDRESS_LEGACY_STREAM |
                                              CU_GET_PROC_ADDRESS_PER_THREAD_DEFAULT_STREAM);
  const char* selected = (const char*)0;
  mf_fixture_cuda_record_call(called_symbol);
  if (symbol == (const char*)0 || function == (void**)0 || version < 0 ||
      (flags & ~known_flags) != UINT64_C(0) || flags == known_flags) {
    return CUDA_ERROR_INVALID_VALUE;
  }
  *function = (void*)0;
  selected = mf_fixture_cuda_route(symbol, version, flags);
  (void)dlerror();
  *function = dlsym(RTLD_DEFAULT, selected);
  if (dlerror() != (const char*)0) {
    *function = (void*)0;
  }
  if (symbol_status != (CUdriverProcAddressQueryResult*)0) {
    *symbol_status =
        *function == (void*)0 ? CU_GET_PROC_ADDRESS_SYMBOL_NOT_FOUND : CU_GET_PROC_ADDRESS_SUCCESS;
  }
  return CUDA_SUCCESS;
}

CUresult cuGetProcAddress(const char* symbol, void** function, int version, cuuint64_t flags) {
  return mf_fixture_cuda_get_proc_address(symbol, function, version, flags,
                                          (CUdriverProcAddressQueryResult*)0, "cuGetProcAddress");
}

CUresult cuGetProcAddress_v2(const char* symbol, void** function, int version, cuuint64_t flags,
                             CUdriverProcAddressQueryResult* symbol_status) {
  return mf_fixture_cuda_get_proc_address(symbol, function, version, flags, symbol_status,
                                          "cuGetProcAddress_v2");
}
