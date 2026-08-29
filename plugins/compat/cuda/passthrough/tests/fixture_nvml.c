#define METAFLUX_NVML_ABI_INTERNAL 1
#include "metaflux/nvml/abi.h"

#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

typedef struct mf_fixture_nvml_counter {
  const char* name;
  atomic_uint_least64_t count;
} mf_fixture_nvml_counter;

#define MF_NVML_INTERNAL(name)
#define MF_NVML_SYMBOL(name, target, status, route, result, parameters, arguments) {#name, 0},
static mf_fixture_nvml_counter mf_fixture_nvml_counters[] = {
#include "../../management/nvml/symbols.def"
};
#undef MF_NVML_SYMBOL
#undef MF_NVML_INTERNAL

static atomic_uint mf_fixture_nvml_references;

void mf_fixture_nvml_record_call(const char* name) {
  size_t index = 0;
  for (index = 0; index < sizeof(mf_fixture_nvml_counters) / sizeof(mf_fixture_nvml_counters[0]);
       ++index) {
    if (strcmp(name, mf_fixture_nvml_counters[index].name) == 0) {
      (void)atomic_fetch_add_explicit(&mf_fixture_nvml_counters[index].count, UINT64_C(1),
                                      memory_order_relaxed);
      return;
    }
  }
}

uint64_t mf_fixture_nvml_call_count(const char* name) {
  size_t index = 0;
  if (name == (const char*)0) {
    return UINT64_MAX;
  }
  for (index = 0; index < sizeof(mf_fixture_nvml_counters) / sizeof(mf_fixture_nvml_counters[0]);
       ++index) {
    if (strcmp(name, mf_fixture_nvml_counters[index].name) == 0) {
      return atomic_load_explicit(&mf_fixture_nvml_counters[index].count, memory_order_relaxed);
    }
  }
  return UINT64_MAX;
}

static nvmlReturn_t mf_fixture_nvml_init(const char* symbol) {
  mf_fixture_nvml_record_call(symbol);
  (void)atomic_fetch_add_explicit(&mf_fixture_nvml_references, UINT32_C(1), memory_order_relaxed);
  return NVML_SUCCESS;
}

nvmlReturn_t nvmlInit(void) { return mf_fixture_nvml_init("nvmlInit"); }

nvmlReturn_t nvmlInit_v2(void) { return mf_fixture_nvml_init("nvmlInit_v2"); }

nvmlReturn_t nvmlInitWithFlags(unsigned int flags) {
  mf_fixture_nvml_record_call("nvmlInitWithFlags");
  if ((flags & ~UINT32_C(1)) != UINT32_C(0)) {
    return NVML_ERROR_INVALID_ARGUMENT;
  }
  (void)atomic_fetch_add_explicit(&mf_fixture_nvml_references, UINT32_C(1), memory_order_relaxed);
  return NVML_SUCCESS;
}

nvmlReturn_t nvmlShutdown(void) {
  unsigned int current = 0;
  mf_fixture_nvml_record_call("nvmlShutdown");
  current = atomic_load_explicit(&mf_fixture_nvml_references, memory_order_relaxed);
  if (current == UINT32_C(0)) {
    return NVML_ERROR_UNINITIALIZED;
  }
  (void)atomic_fetch_sub_explicit(&mf_fixture_nvml_references, UINT32_C(1), memory_order_relaxed);
  return NVML_SUCCESS;
}

static nvmlReturn_t mf_fixture_nvml_copy(char* output, unsigned int length, const char* value) {
  const size_t required = strlen(value) + (size_t)1;
  if (output == (char*)0 || (size_t)length < required) {
    return output == (char*)0 ? NVML_ERROR_INVALID_ARGUMENT : NVML_ERROR_INSUFFICIENT_SIZE;
  }
  (void)memcpy(output, value, required);
  return NVML_SUCCESS;
}

nvmlReturn_t nvmlSystemGetDriverVersion(char* version, unsigned int length) {
  mf_fixture_nvml_record_call("nvmlSystemGetDriverVersion");
  return mf_fixture_nvml_copy(version, length, "777.42.01");
}

nvmlReturn_t nvmlSystemGetNVMLVersion(char* version, unsigned int length) {
  mf_fixture_nvml_record_call("nvmlSystemGetNVMLVersion");
  return mf_fixture_nvml_copy(version, length, "12.777.42.01");
}

nvmlReturn_t nvmlSystemGetCudaDriverVersion(int* version) {
  mf_fixture_nvml_record_call("nvmlSystemGetCudaDriverVersion");
  if (version == (int*)0) {
    return NVML_ERROR_INVALID_ARGUMENT;
  }
  *version = 12070;
  return NVML_SUCCESS;
}

nvmlReturn_t nvmlSystemGetCudaDriverVersion_v2(int* version) {
  mf_fixture_nvml_record_call("nvmlSystemGetCudaDriverVersion_v2");
  if (version == (int*)0) {
    return NVML_ERROR_INVALID_ARGUMENT;
  }
  *version = 12070;
  return NVML_SUCCESS;
}

static nvmlReturn_t mf_fixture_nvml_count(unsigned int* count, const char* symbol) {
  mf_fixture_nvml_record_call(symbol);
  if (count == (unsigned int*)0) {
    return NVML_ERROR_INVALID_ARGUMENT;
  }
  *count = UINT32_C(1);
  return NVML_SUCCESS;
}

nvmlReturn_t nvmlDeviceGetCount(unsigned int* count) {
  return mf_fixture_nvml_count(count, "nvmlDeviceGetCount");
}

nvmlReturn_t nvmlDeviceGetCount_v2(unsigned int* count) {
  return mf_fixture_nvml_count(count, "nvmlDeviceGetCount_v2");
}

static nvmlReturn_t mf_fixture_nvml_handle(unsigned int index, nvmlDevice_t* device,
                                           const char* symbol) {
  mf_fixture_nvml_record_call(symbol);
  if (device == (nvmlDevice_t*)0) {
    return NVML_ERROR_INVALID_ARGUMENT;
  }
  if (index != UINT32_C(0)) {
    return NVML_ERROR_NOT_FOUND;
  }
  *device = (nvmlDevice_t)(uintptr_t)UINT64_C(0x777);
  return NVML_SUCCESS;
}

nvmlReturn_t nvmlDeviceGetHandleByIndex(unsigned int index, nvmlDevice_t* device) {
  return mf_fixture_nvml_handle(index, device, "nvmlDeviceGetHandleByIndex");
}

nvmlReturn_t nvmlDeviceGetHandleByIndex_v2(unsigned int index, nvmlDevice_t* device) {
  return mf_fixture_nvml_handle(index, device, "nvmlDeviceGetHandleByIndex_v2");
}

nvmlReturn_t nvmlDeviceGetName(nvmlDevice_t device, char* name, unsigned int length) {
  mf_fixture_nvml_record_call("nvmlDeviceGetName");
  return device == (nvmlDevice_t)(uintptr_t)UINT64_C(0x777)
             ? mf_fixture_nvml_copy(name, length, "MetaFlux Vendor Fixture")
             : NVML_ERROR_INVALID_ARGUMENT;
}

nvmlReturn_t nvmlDeviceGetUUID(nvmlDevice_t device, char* uuid, unsigned int length) {
  mf_fixture_nvml_record_call("nvmlDeviceGetUUID");
  return device == (nvmlDevice_t)(uintptr_t)UINT64_C(0x777)
             ? mf_fixture_nvml_copy(uuid, length, "GPU-00010203-0405-0607-0809-0a0b0c0d0e0f")
             : NVML_ERROR_INVALID_ARGUMENT;
}

const char* nvmlErrorString(nvmlReturn_t result) {
  mf_fixture_nvml_record_call("nvmlErrorString");
  return result == NVML_SUCCESS ? "Vendor Success" : "Vendor Error";
}
