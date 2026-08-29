#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L

#include "benchmark_common.h"

#include <cuda.h>
#include <dlfcn.h>
#include <sched.h>

#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

typedef CUresult(CUDAAPI* mf_cu_init_fn)(unsigned int);
typedef CUresult(CUDAAPI* mf_cu_driver_get_version_fn)(int*);
typedef CUresult(CUDAAPI* mf_cu_device_get_count_fn)(int*);
typedef CUresult(CUDAAPI* mf_cu_device_get_fn)(CUdevice*, int);
typedef CUresult(CUDAAPI* mf_cu_device_get_name_fn)(char*, int, CUdevice);
typedef CUresult(CUDAAPI* mf_cu_device_get_uuid_fn)(CUuuid*, CUdevice);
typedef CUresult(CUDAAPI* mf_cu_device_get_pci_bus_id_fn)(char*, int, CUdevice);
typedef CUresult(CUDAAPI* mf_cu_ctx_create_fn)(CUcontext*, unsigned int, CUdevice);
typedef CUresult(CUDAAPI* mf_cu_ctx_destroy_fn)(CUcontext);
typedef CUresult(CUDAAPI* mf_cu_stream_create_fn)(CUstream*, unsigned int);
typedef CUresult(CUDAAPI* mf_cu_stream_destroy_fn)(CUstream);
typedef CUresult(CUDAAPI* mf_cu_stream_synchronize_fn)(CUstream);
typedef CUresult(CUDAAPI* mf_cu_mem_alloc_fn)(CUdeviceptr*, size_t);
typedef CUresult(CUDAAPI* mf_cu_mem_free_fn)(CUdeviceptr);
typedef CUresult(CUDAAPI* mf_cu_memcpy_h2d_async_fn)(CUdeviceptr, const void*, size_t, CUstream);
typedef CUresult(CUDAAPI* mf_cu_memcpy_d2h_async_fn)(void*, CUdeviceptr, size_t, CUstream);
typedef CUresult(CUDAAPI* mf_cu_memcpy_d2d_async_fn)(CUdeviceptr, CUdeviceptr, size_t, CUstream);

typedef struct mf_cuda_api {
  mf_cu_init_fn init;
  mf_cu_driver_get_version_fn driver_get_version;
  mf_cu_device_get_count_fn device_get_count;
  mf_cu_device_get_fn device_get;
  mf_cu_device_get_name_fn device_get_name;
  mf_cu_device_get_uuid_fn device_get_uuid;
  mf_cu_device_get_pci_bus_id_fn device_get_pci_bus_id;
  mf_cu_ctx_create_fn ctx_create;
  mf_cu_ctx_destroy_fn ctx_destroy;
  mf_cu_stream_create_fn stream_create;
  mf_cu_stream_destroy_fn stream_destroy;
  mf_cu_stream_synchronize_fn stream_synchronize;
  mf_cu_mem_alloc_fn mem_alloc;
  mf_cu_mem_free_fn mem_free;
  mf_cu_memcpy_h2d_async_fn memcpy_h2d_async;
  mf_cu_memcpy_d2h_async_fn memcpy_d2h_async;
  mf_cu_memcpy_d2d_async_fn memcpy_d2d_async;
} mf_cuda_api;

typedef enum mf_copy_direction {
  MF_COPY_DIRECTION_H2D = 0,
  MF_COPY_DIRECTION_D2H = 1,
  MF_COPY_DIRECTION_D2D = 2,
} mf_copy_direction;

typedef struct mf_arguments {
  const char* direction;
  const char* device_bdf;
  const char* cuda_library;
  const char* host_allocation;
  uint64_t copy_bytes;
  uint32_t warmup_count;
  uint32_t sample_count;
  int cpu;
  int numa_node;
} mf_arguments;

static int mf_parse_nonnegative_int(const char* text, int* out_value) {
  char* end = (char*)0;
  long value = 0;
  if (text == (const char*)0 || out_value == (int*)0 || text[0] == '\0') {
    return -1;
  }
  errno = 0;
  value = strtol(text, &end, 10);
  if (errno != 0 || end == text || *end != '\0' || value < 0 || value > INT_MAX) {
    return -1;
  }
  *out_value = (int)value;
  return 0;
}

static int mf_parse_arguments(int argc, char** argv, mf_arguments* arguments) {
  int index = 0;
  if (arguments == (mf_arguments*)0) {
    return -1;
  }
  (void)memset(arguments, 0, sizeof(*arguments));
  arguments->cpu = -1;
  arguments->numa_node = -1;
  if (argc != 19) {
    return -1;
  }
  for (index = 1; index < argc; index += 2) {
    const char* option = argv[index];
    const char* value = argv[index + 1];
    if (strcmp(option, "--direction") == 0 && arguments->direction == (const char*)0) {
      arguments->direction = value;
    } else if (strcmp(option, "--warmup") == 0 && arguments->warmup_count == 0) {
      if (mf_benchmark_parse_u32(value, &arguments->warmup_count) != 0) {
        return -1;
      }
    } else if (strcmp(option, "--samples") == 0 && arguments->sample_count == 0) {
      if (mf_benchmark_parse_u32(value, &arguments->sample_count) != 0) {
        return -1;
      }
    } else if (strcmp(option, "--copy-bytes") == 0 && arguments->copy_bytes == 0) {
      if (mf_benchmark_parse_u64(value, &arguments->copy_bytes) != 0) {
        return -1;
      }
    } else if (strcmp(option, "--device-bdf") == 0 &&
               arguments->device_bdf == (const char*)0) {
      arguments->device_bdf = value;
    } else if (strcmp(option, "--cuda-library") == 0 &&
               arguments->cuda_library == (const char*)0) {
      arguments->cuda_library = value;
    } else if (strcmp(option, "--host-allocation") == 0 &&
               arguments->host_allocation == (const char*)0) {
      arguments->host_allocation = value;
    } else if (strcmp(option, "--cpu") == 0 && arguments->cpu < 0) {
      if (mf_parse_nonnegative_int(value, &arguments->cpu) != 0) {
        return -1;
      }
    } else if (strcmp(option, "--numa-node") == 0 && arguments->numa_node < 0) {
      if (mf_parse_nonnegative_int(value, &arguments->numa_node) != 0) {
        return -1;
      }
    } else {
      return -1;
    }
  }
  if (arguments->direction == (const char*)0 || arguments->device_bdf == (const char*)0 ||
      arguments->cuda_library == (const char*)0 ||
      arguments->host_allocation == (const char*)0 || arguments->warmup_count == 0 ||
      arguments->sample_count == 0 || arguments->copy_bytes == 0 || arguments->cpu < 0 ||
      arguments->numa_node < 0 || arguments->cuda_library[0] != '/' ||
      strcmp(arguments->host_allocation, "malloc_pageable") != 0 ||
      (strcmp(arguments->direction, "h2d") != 0 && strcmp(arguments->direction, "d2h") != 0 &&
       strcmp(arguments->direction, "d2d") != 0)) {
    return -1;
  }
  return 0;
}

static int mf_load_symbol(void* library, const char* name, void* destination,
                          size_t destination_size) {
  void* symbol = (void*)0;
  const char* error = (const char*)0;
  (void)dlerror();
  symbol = dlsym(library, name);
  error = dlerror();
  if (error != (const char*)0 || symbol == (void*)0 || destination_size != sizeof(symbol)) {
    (void)fprintf(stderr, "m0001-native-copy: dlsym(%s) failed: %s\n", name,
                  error == (const char*)0 ? "invalid function pointer representation" : error);
    return -1;
  }
  (void)memcpy(destination, &symbol, destination_size);
  return 0;
}

#define MF_LOAD(api, library, member, symbol)                                                    \
  do {                                                                                            \
    if (mf_load_symbol((library), (symbol), &(api)->member, sizeof((api)->member)) != 0) {        \
      return -1;                                                                                  \
    }                                                                                             \
  } while (0)

static int mf_load_cuda_api(void* library, mf_cuda_api* api) {
  (void)memset(api, 0, sizeof(*api));
  MF_LOAD(api, library, init, "cuInit");
  MF_LOAD(api, library, driver_get_version, "cuDriverGetVersion");
  MF_LOAD(api, library, device_get_count, "cuDeviceGetCount");
  MF_LOAD(api, library, device_get, "cuDeviceGet");
  MF_LOAD(api, library, device_get_name, "cuDeviceGetName");
  MF_LOAD(api, library, device_get_uuid, "cuDeviceGetUuid_v2");
  MF_LOAD(api, library, device_get_pci_bus_id, "cuDeviceGetPCIBusId");
  MF_LOAD(api, library, ctx_create, "cuCtxCreate_v2");
  MF_LOAD(api, library, ctx_destroy, "cuCtxDestroy_v2");
  MF_LOAD(api, library, stream_create, "cuStreamCreate");
  MF_LOAD(api, library, stream_destroy, "cuStreamDestroy_v2");
  MF_LOAD(api, library, stream_synchronize, "cuStreamSynchronize");
  MF_LOAD(api, library, mem_alloc, "cuMemAlloc_v2");
  MF_LOAD(api, library, mem_free, "cuMemFree_v2");
  MF_LOAD(api, library, memcpy_h2d_async, "cuMemcpyHtoDAsync_v2");
  MF_LOAD(api, library, memcpy_d2h_async, "cuMemcpyDtoHAsync_v2");
  MF_LOAD(api, library, memcpy_d2d_async, "cuMemcpyDtoDAsync_v2");
  return 0;
}

static int mf_report_cuda_failure(CUresult result, const char* expression, int line) {
  (void)fprintf(stderr, "m0001-native-copy: %s failed at line %d with CUresult %d\n", expression,
                line, (int)result);
  return 1;
}

#define MF_CUDA_CALL(expression)                                                                  \
  do {                                                                                            \
    cuda_result = (expression);                                                                   \
    if (cuda_result != CUDA_SUCCESS) {                                                            \
      exit_code = mf_report_cuda_failure(cuda_result, #expression, __LINE__);                     \
      goto cleanup;                                                                               \
    }                                                                                             \
  } while (0)

static int mf_format_uuid(const CUuuid* uuid, char* destination, size_t capacity) {
  const unsigned char* bytes = (const unsigned char*)uuid->bytes;
  int length = snprintf(
      destination, capacity,
      "GPU-%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x", bytes[0],
      bytes[1], bytes[2], bytes[3], bytes[4], bytes[5], bytes[6], bytes[7], bytes[8], bytes[9],
      bytes[10], bytes[11], bytes[12], bytes[13], bytes[14], bytes[15]);
  return length > 0 && (size_t)length < capacity ? 0 : -1;
}

static int mf_cpu_placement_matches(const mf_arguments* arguments, int* out_actual_cpu,
                                    int* out_affinity_count) {
  cpu_set_t affinity;
  char node_path[PATH_MAX];
  int actual_cpu = sched_getcpu();
  int path_length = 0;
  CPU_ZERO(&affinity);
  if (actual_cpu < 0 || sched_getaffinity(0, sizeof(affinity), &affinity) != 0 ||
      arguments->cpu >= CPU_SETSIZE) {
    return -1;
  }
  *out_affinity_count = CPU_COUNT(&affinity);
  *out_actual_cpu = actual_cpu;
  path_length = snprintf(node_path, sizeof(node_path), "/sys/devices/system/cpu/cpu%d/node%d",
                         arguments->cpu, arguments->numa_node);
  if (actual_cpu != arguments->cpu || *out_affinity_count != 1 ||
      !CPU_ISSET(arguments->cpu, &affinity) || path_length <= 0 ||
      (size_t)path_length >= sizeof(node_path) || access(node_path, F_OK) != 0) {
    return -1;
  }
  return 0;
}

static mf_copy_direction mf_direction(const char* direction) {
  if (strcmp(direction, "h2d") == 0) {
    return MF_COPY_DIRECTION_H2D;
  }
  if (strcmp(direction, "d2h") == 0) {
    return MF_COPY_DIRECTION_D2H;
  }
  return MF_COPY_DIRECTION_D2D;
}

static CUresult mf_enqueue_copy(const mf_cuda_api* api, mf_copy_direction direction,
                                CUdeviceptr source_device, CUdeviceptr destination_device,
                                const void* source_host, void* destination_host, size_t copy_bytes,
                                CUstream stream) {
  if (direction == MF_COPY_DIRECTION_H2D) {
    return api->memcpy_h2d_async(destination_device, source_host, copy_bytes, stream);
  }
  if (direction == MF_COPY_DIRECTION_D2H) {
    return api->memcpy_d2h_async(destination_host, source_device, copy_bytes, stream);
  }
  return api->memcpy_d2d_async(destination_device, source_device, copy_bytes, stream);
}

int main(int argc, char** argv) {
  mf_arguments arguments;
  mf_cuda_api api;
  mf_copy_direction direction = MF_COPY_DIRECTION_H2D;
  void* cuda_library = (void*)0;
  uint8_t* source_host = (uint8_t*)0;
  uint8_t* destination_host = (uint8_t*)0;
  uint64_t* samples = (uint64_t*)0;
  CUdeviceptr source_device = (CUdeviceptr)0;
  CUdeviceptr destination_device = (CUdeviceptr)0;
  CUcontext context = (CUcontext)0;
  CUstream stream = (CUstream)0;
  CUdevice selected_device = 0;
  CUuuid selected_uuid;
  CUresult cuda_result = CUDA_SUCCESS;
  size_t copy_bytes = 0;
  uint64_t byte_index = 0;
  uint64_t start_ns = 0;
  uint64_t end_ns = 0;
  uint32_t index = 0;
  int device_count = 0;
  int ordinal = 0;
  int found_device = 0;
  int driver_version = 0;
  int actual_cpu = -1;
  int affinity_count = 0;
  int exit_code = 1;
  char selected_bdf[32];
  char selected_name[256];
  char selected_uuid_text[64];
  char metric[64];

  if (mf_parse_arguments(argc, argv, &arguments) != 0 || arguments.copy_bytes > SIZE_MAX) {
    (void)fprintf(stderr,
                  "usage: %s --direction h2d|d2h|d2d --warmup COUNT --samples COUNT "
                  "--copy-bytes BYTES --device-bdf BDF --cuda-library ABSOLUTE_PATH "
                  "--host-allocation malloc_pageable --cpu CPU --numa-node NODE\n",
                  argv[0]);
    return 2;
  }
  if (mf_cpu_placement_matches(&arguments, &actual_cpu, &affinity_count) != 0) {
    (void)fprintf(stderr, "m0001-native-copy: CPU affinity/NUMA placement mismatch\n");
    return 1;
  }
  copy_bytes = (size_t)arguments.copy_bytes;
  direction = mf_direction(arguments.direction);
  source_host = (uint8_t*)malloc(copy_bytes);
  destination_host = (uint8_t*)malloc(copy_bytes);
  samples = (uint64_t*)calloc((size_t)arguments.sample_count, sizeof(*samples));
  if (source_host == (uint8_t*)0 || destination_host == (uint8_t*)0 ||
      samples == (uint64_t*)0) {
    goto cleanup;
  }
  for (byte_index = 0; byte_index < arguments.copy_bytes; ++byte_index) {
    source_host[(size_t)byte_index] = (uint8_t)(byte_index % UINT64_C(251));
  }
  (void)memset(destination_host, 0, copy_bytes);
  (void)memset(&api, 0, sizeof(api));
  (void)memset(&selected_uuid, 0, sizeof(selected_uuid));
  (void)memset(selected_bdf, 0, sizeof(selected_bdf));
  (void)memset(selected_name, 0, sizeof(selected_name));
  (void)memset(selected_uuid_text, 0, sizeof(selected_uuid_text));

  cuda_library = dlopen(arguments.cuda_library, RTLD_NOW | RTLD_LOCAL);
  if (cuda_library == (void*)0) {
    (void)fprintf(stderr, "m0001-native-copy: dlopen(%s) failed: %s\n", arguments.cuda_library,
                  dlerror());
    goto cleanup;
  }
  if (mf_load_cuda_api(cuda_library, &api) != 0) {
    goto cleanup;
  }
  MF_CUDA_CALL(api.init(0));
  MF_CUDA_CALL(api.driver_get_version(&driver_version));
  MF_CUDA_CALL(api.device_get_count(&device_count));
  for (ordinal = 0; ordinal < device_count; ++ordinal) {
    CUdevice candidate = 0;
    char candidate_bdf[32];
    (void)memset(candidate_bdf, 0, sizeof(candidate_bdf));
    MF_CUDA_CALL(api.device_get(&candidate, ordinal));
    MF_CUDA_CALL(api.device_get_pci_bus_id(candidate_bdf, (int)sizeof(candidate_bdf), candidate));
    if (strcasecmp(candidate_bdf, arguments.device_bdf) == 0) {
      selected_device = candidate;
      (void)memcpy(selected_bdf, candidate_bdf, sizeof(selected_bdf));
      found_device = 1;
      break;
    }
  }
  if (!found_device) {
    (void)fprintf(stderr, "m0001-native-copy: requested BDF %s is not visible\n",
                  arguments.device_bdf);
    goto cleanup;
  }
  MF_CUDA_CALL(api.device_get_name(selected_name, (int)sizeof(selected_name), selected_device));
  MF_CUDA_CALL(api.device_get_uuid(&selected_uuid, selected_device));
  if (mf_format_uuid(&selected_uuid, selected_uuid_text, sizeof(selected_uuid_text)) != 0) {
    goto cleanup;
  }
  MF_CUDA_CALL(api.ctx_create(&context, 0, selected_device));
  MF_CUDA_CALL(api.stream_create(&stream, CU_STREAM_NON_BLOCKING));
  MF_CUDA_CALL(api.mem_alloc(&source_device, copy_bytes));
  MF_CUDA_CALL(api.mem_alloc(&destination_device, copy_bytes));
  MF_CUDA_CALL(api.memcpy_h2d_async(source_device, source_host, copy_bytes, stream));
  MF_CUDA_CALL(api.stream_synchronize(stream));

  for (index = 0; index < arguments.warmup_count; ++index) {
    MF_CUDA_CALL(mf_enqueue_copy(&api, direction, source_device, destination_device, source_host,
                                 destination_host, copy_bytes, stream));
    MF_CUDA_CALL(api.stream_synchronize(stream));
  }
  for (index = 0; index < arguments.sample_count; ++index) {
    if (mf_benchmark_now_ns(&start_ns) != 0) {
      goto cleanup;
    }
    MF_CUDA_CALL(mf_enqueue_copy(&api, direction, source_device, destination_device, source_host,
                                 destination_host, copy_bytes, stream));
    MF_CUDA_CALL(api.stream_synchronize(stream));
    if (mf_benchmark_now_ns(&end_ns) != 0 || end_ns <= start_ns) {
      goto cleanup;
    }
    samples[index] = end_ns - start_ns;
  }
  if (direction != MF_COPY_DIRECTION_D2H) {
    CUdeviceptr correctness_device = direction == MF_COPY_DIRECTION_H2D ? destination_device
                                                                        : destination_device;
    MF_CUDA_CALL(
        api.memcpy_d2h_async(destination_host, correctness_device, copy_bytes, stream));
    MF_CUDA_CALL(api.stream_synchronize(stream));
  }
  if (memcmp(source_host, destination_host, copy_bytes) != 0 ||
      mf_cpu_placement_matches(&arguments, &actual_cpu, &affinity_count) != 0) {
    (void)fprintf(stderr, "m0001-native-copy: copy correctness or placement mismatch\n");
    goto cleanup;
  }

  (void)snprintf(metric, sizeof(metric), "native_cuda_%s_complete_ns", arguments.direction);
  mf_benchmark_emit_metadata_text("workload", "same_path_native_copy");
  mf_benchmark_emit_metadata_text("direction", arguments.direction);
  mf_benchmark_emit_metadata_text("api", direction == MF_COPY_DIRECTION_H2D
                                             ? "cuMemcpyHtoDAsync"
                                             : direction == MF_COPY_DIRECTION_D2H
                                                   ? "cuMemcpyDtoHAsync"
                                                   : "cuMemcpyDtoDAsync");
  mf_benchmark_emit_metadata_text("completion_boundary", "cuStreamSynchronize_return");
  mf_benchmark_emit_metadata_text("provider_mode", "native");
  mf_benchmark_emit_metadata_text("host_allocation", arguments.host_allocation);
  mf_benchmark_emit_metadata_text("clock", "CLOCK_MONOTONIC_RAW");
  mf_benchmark_emit_metadata_text("stopping_rule", "exact_fixed_sample_count_no_deletion");
  mf_benchmark_emit_metadata_text("device_bdf", selected_bdf);
  mf_benchmark_emit_metadata_text("device_uuid", selected_uuid_text);
  mf_benchmark_emit_metadata_text("device_name", selected_name);
  mf_benchmark_emit_metadata_text("cuda_library_path", arguments.cuda_library);
  mf_benchmark_emit_metadata_text("cuda_visible_devices", getenv("CUDA_VISIBLE_DEVICES") == NULL
                                                              ? ""
                                                              : getenv("CUDA_VISIBLE_DEVICES"));
  mf_benchmark_emit_metadata_text("cuda_device_order", getenv("CUDA_DEVICE_ORDER") == NULL
                                                           ? ""
                                                           : getenv("CUDA_DEVICE_ORDER"));
  mf_benchmark_emit_metadata_text("ld_library_path", getenv("LD_LIBRARY_PATH") == NULL
                                                        ? ""
                                                        : getenv("LD_LIBRARY_PATH"));
  mf_benchmark_emit_metadata_u64("copy_bytes", arguments.copy_bytes);
  mf_benchmark_emit_metadata_u64("warmup_count", arguments.warmup_count);
  mf_benchmark_emit_metadata_u64("sample_count", arguments.sample_count);
  mf_benchmark_emit_metadata_u64("requested_cpu", (uint64_t)arguments.cpu);
  mf_benchmark_emit_metadata_u64("actual_cpu", (uint64_t)actual_cpu);
  mf_benchmark_emit_metadata_u64("effective_affinity_count", (uint64_t)affinity_count);
  mf_benchmark_emit_metadata_u64("numa_node", (uint64_t)arguments.numa_node);
  mf_benchmark_emit_metadata_u64("selected_device_ordinal", (uint64_t)ordinal);
  mf_benchmark_emit_metadata_u64("driver_version", (uint64_t)driver_version);
  mf_benchmark_emit_metadata_u64("copy_correctness", UINT64_C(1));
  for (index = 0; index < arguments.sample_count; ++index) {
    mf_benchmark_emit_sample(metric, index, samples[index], "ns");
  }
  exit_code = 0;

cleanup:
  if (destination_device != (CUdeviceptr)0 && api.mem_free != (mf_cu_mem_free_fn)0) {
    (void)api.mem_free(destination_device);
  }
  if (source_device != (CUdeviceptr)0 && api.mem_free != (mf_cu_mem_free_fn)0) {
    (void)api.mem_free(source_device);
  }
  if (stream != (CUstream)0 && api.stream_destroy != (mf_cu_stream_destroy_fn)0) {
    (void)api.stream_destroy(stream);
  }
  if (context != (CUcontext)0 && api.ctx_destroy != (mf_cu_ctx_destroy_fn)0) {
    (void)api.ctx_destroy(context);
  }
  if (cuda_library != (void*)0) {
    (void)dlclose(cuda_library);
  }
  free(samples);
  free(destination_host);
  free(source_host);
  return exit_code;
}
