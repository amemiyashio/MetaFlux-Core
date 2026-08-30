#define _POSIX_C_SOURCE 200809L

#include "benchmark_common.h"

#include <nvml.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int mf_report_nvml_failure(nvmlReturn_t result, const char* expression, int line) {
  (void)fprintf(stderr, "m0100-nvml-benchmark: %s failed at line %d: %s\n", expression, line,
                nvmlErrorString(result));
  return 1;
}

#define MF_NVML_CALL(expression)                                                                   \
  do {                                                                                             \
    nvml_result = (expression);                                                                    \
    if (nvml_result != NVML_SUCCESS) {                                                             \
      exit_code = mf_report_nvml_failure(nvml_result, #expression, __LINE__);                      \
      goto cleanup;                                                                                \
    }                                                                                              \
  } while (0)

int main(int argc, char** argv) {
  uint64_t* getter_samples = (uint64_t*)0;
  uint64_t* init_samples = (uint64_t*)0;
  nvmlDevice_t device = (nvmlDevice_t)0;
  nvmlMemory_t memory;
  nvmlReturn_t nvml_result = NVML_SUCCESS;
  uint64_t start_ns = 0;
  uint64_t end_ns = 0;
  uint32_t warmup_count = 0;
  uint32_t sample_count = 0;
  uint32_t index = 0;
  unsigned int device_count = 0;
  uint32_t initialized = UINT32_C(0);
  int exit_code = 1;
  char device_name[NVML_DEVICE_NAME_BUFFER_SIZE];

  if (argc != 3 || mf_benchmark_parse_u32(argv[1], &warmup_count) != 0 ||
      mf_benchmark_parse_u32(argv[2], &sample_count) != 0) {
    (void)fprintf(stderr, "usage: %s WARMUP_COUNT SAMPLE_COUNT\n", argv[0]);
    return 2;
  }
  getter_samples = (uint64_t*)calloc((size_t)sample_count, sizeof(uint64_t));
  init_samples = (uint64_t*)calloc((size_t)sample_count, sizeof(uint64_t));
  if (getter_samples == (uint64_t*)0 || init_samples == (uint64_t*)0) {
    goto cleanup;
  }
  (void)memset(&memory, 0, sizeof(memory));
  (void)memset(device_name, 0, sizeof(device_name));

  MF_NVML_CALL(nvmlInit_v2());
  initialized = UINT32_C(1);
  MF_NVML_CALL(nvmlDeviceGetCount_v2(&device_count));
  if (device_count == 0) {
    (void)fprintf(stderr, "m0100-nvml-benchmark: no managed NVML device was published\n");
    goto cleanup;
  }
  MF_NVML_CALL(nvmlDeviceGetHandleByIndex_v2(0, &device));
  MF_NVML_CALL(nvmlDeviceGetName(device, device_name, (unsigned int)sizeof(device_name)));
  for (index = 0; index < warmup_count; ++index) {
    MF_NVML_CALL(nvmlDeviceGetMemoryInfo(device, &memory));
  }
  for (index = 0; index < sample_count; ++index) {
    if (mf_benchmark_now_ns(&start_ns) != 0) {
      goto cleanup;
    }
    MF_NVML_CALL(nvmlDeviceGetMemoryInfo(device, &memory));
    if (mf_benchmark_now_ns(&end_ns) != 0 || end_ns < start_ns) {
      goto cleanup;
    }
    getter_samples[index] = end_ns - start_ns;
  }
  MF_NVML_CALL(nvmlShutdown());
  initialized = UINT32_C(0);

  for (index = 0; index < warmup_count; ++index) {
    MF_NVML_CALL(nvmlInit_v2());
    initialized = UINT32_C(1);
    MF_NVML_CALL(nvmlShutdown());
    initialized = UINT32_C(0);
  }
  for (index = 0; index < sample_count; ++index) {
    if (mf_benchmark_now_ns(&start_ns) != 0) {
      goto cleanup;
    }
    MF_NVML_CALL(nvmlInit_v2());
    initialized = UINT32_C(1);
    if (mf_benchmark_now_ns(&end_ns) != 0 || end_ns < start_ns) {
      goto cleanup;
    }
    init_samples[index] = end_ns - start_ns;
    MF_NVML_CALL(nvmlShutdown());
    initialized = UINT32_C(0);
  }

  mf_benchmark_emit_metadata_text("workload", "managed_nvml_hot_getter_and_warm_init");
  mf_benchmark_emit_metadata_text("device_name", device_name);
  mf_benchmark_emit_metadata_u64("device_count", device_count);
  mf_benchmark_emit_metadata_u64("warmup_count", warmup_count);
  mf_benchmark_emit_metadata_u64("sample_count", sample_count);
  for (index = 0; index < sample_count; ++index) {
    mf_benchmark_emit_sample("nvml_hot_memory_getter_ns", index, getter_samples[index], "ns");
    mf_benchmark_emit_sample("nvml_warm_init_ns", index, init_samples[index], "ns");
  }
  exit_code = 0;

cleanup:
  if (initialized != UINT32_C(0) && nvmlShutdown() != NVML_SUCCESS) {
    exit_code = 1;
  }
  free(init_samples);
  free(getter_samples);
  return exit_code;
}
