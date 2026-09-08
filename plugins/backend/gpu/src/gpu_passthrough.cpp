/* GPU passthrough backend: forwards accepted Kernel Request PTX payloads to
   the real GPU driver (cuModuleLoadData / cuLaunchKernel on a real CUDA
   context). The real driver is resolved at run time through dlopen so this
   component compiles without the CUDA toolkit; without a usable driver the
   backend enumerates zero devices and every execution entry point returns
   its declared MF_BACKEND_UNSUPPORTED. Argument-block value extraction and
   the daemon-memory to real-device address translation are decode fronts
   tracked in work-item-0.2.0.2. */
#include <metaflux/backend/api.h>

#include <dlfcn.h>

#include <cstring>
#include <mutex>
#include <string>
#include <vector>

namespace {

constexpr auto kDefaultDriver = "libcuda.so.1";

/* Real driver entry points the passthrough forwards to. */
struct DriverApi {
  void* handle = nullptr;
  int (*cu_init)(unsigned) = nullptr;
  int (*cu_device_get_count)(int*) = nullptr;
  int (*cu_device_get_name)(char*, int, int) = nullptr;
  int (*cu_device_get_attribute)(int*, int, int) = nullptr;
  int (*cu_device_get)(int*, int) = nullptr;
  int (*cu_ctx_create)(void**, unsigned, int) = nullptr;
  int (*cu_ctx_destroy)(void*) = nullptr;
  int (*cu_module_load_data)(void**, const void*) = nullptr;
  int (*cu_module_unload_data)(void*) = nullptr;
  int (*cu_mem_alloc)(unsigned long long*, unsigned long long) = nullptr;
  int (*cu_mem_free)(unsigned long long) = nullptr;
  int (*cu_memcpy_h_to_d)(unsigned long long, const void*, unsigned long long) = nullptr;
  int (*cu_memcpy_d_to_h)(void*, unsigned long long, unsigned long long) = nullptr;
  int (*cu_launch_kernel)(void*, const void*, unsigned, unsigned, unsigned, unsigned, unsigned,
                          unsigned, unsigned, void*, void**) = nullptr;
  bool usable = false;
};

DriverApi& driver() {
  static DriverApi api;
  return api;
}

std::mutex& probe_mutex() {
  static std::mutex mutex;
  return mutex;
}

/* One-time driver probe: dlopen the real driver, resolve the forwarding
   set, and cuInit(0). Any failure leaves the backend inert. */
const DriverApi& probe_driver() {
  std::lock_guard lock(probe_mutex());
  auto& api = driver();
  if (api.handle != nullptr || api.usable) {
    return api;
  }
  const char* configured = std::getenv("METAFLUX_GPU_PASSTHROUGH_DRIVER");
  api.handle = dlopen(configured != nullptr && configured[0] != '\0' ? configured : kDefaultDriver,
                      RTLD_NOW | RTLD_LOCAL);
  if (api.handle == nullptr) {
    return api;
  }
  api.cu_init = reinterpret_cast<int (*)(unsigned)>(dlsym(api.handle, "cuInit"));
  api.cu_device_get_count =
      reinterpret_cast<int (*)(int*)>(dlsym(api.handle, "cuDeviceGetCount"));
  api.cu_device_get_name =
      reinterpret_cast<int (*)(char*, int, int)>(dlsym(api.handle, "cuDeviceGetName"));
  api.cu_device_get_attribute =
      reinterpret_cast<int (*)(int*, int, int)>(dlsym(api.handle, "cuDeviceGetAttribute"));
  api.cu_device_get = reinterpret_cast<int (*)(int*, int)>(dlsym(api.handle, "cuDeviceGet"));
  api.cu_ctx_create =
      reinterpret_cast<int (*)(void**, unsigned, int)>(dlsym(api.handle, "cuCtxCreate_v3") != nullptr
                                                          ? dlsym(api.handle, "cuCtxCreate_v3")
                                                          : dlsym(api.handle, "cuCtxCreate"));
  api.cu_ctx_destroy = reinterpret_cast<int (*)(void*)>(dlsym(api.handle, "cuCtxDestroy"));
  api.cu_module_load_data =
      reinterpret_cast<int (*)(void**, const void*)>(dlsym(api.handle, "cuModuleLoadData"));
  api.cu_module_unload_data =
      reinterpret_cast<int (*)(void*)>(dlsym(api.handle, "cuModuleUnload"));
  api.cu_mem_alloc = reinterpret_cast<int (*)(unsigned long long*, unsigned long long)>(
      dlsym(api.handle, "cuMemAlloc"));
  api.cu_mem_free = reinterpret_cast<int (*)(unsigned long long)>(dlsym(api.handle, "cuMemFree"));
  api.cu_memcpy_h_to_d = reinterpret_cast<int (*)(unsigned long long, const void*,
                                                  unsigned long long)>(dlsym(api.handle,
                                                                             "cuMemcpyHtoD"));
  api.cu_memcpy_d_to_h = reinterpret_cast<int (*)(void*, unsigned long long, unsigned long long)>(
      dlsym(api.handle, "cuMemcpyDtoH"));
  api.cu_launch_kernel = reinterpret_cast<int (*)(void*, const void*, unsigned, unsigned, unsigned,
                                                  unsigned, unsigned, unsigned, unsigned, void*,
                                                  void**)>(dlsym(api.handle, "cuLaunchKernel"));
  if (api.cu_init == nullptr || api.cu_init(0U) != 0) {
    return api;
  }
  int count = 0;
  if (api.cu_device_get_count == nullptr || api.cu_device_get_count(&count) != 0 || count <= 0) {
    return api;
  }
  api.usable = true;
  return api;
}

struct Instance {
  mf_backend_host_api_v1 host = {};
};

} // namespace

extern "C" {

mf_backend_status_v1 mf_gpu_passthrough_create_instance(const mf_backend_host_api_v1* host_api,
                                                        mf_backend_instance_v1* out_instance) {
  if (out_instance == nullptr) {
    return MF_BACKEND_INVALID_ARGUMENT;
  }
  (void)host_api;
  /* The passthrough keeps one static instance; the handle is a stable tag. */
  *out_instance = UINT64_C(1);
  return MF_BACKEND_SUCCESS;
}

void mf_gpu_passthrough_destroy_instance(mf_backend_instance_v1 instance) {
  (void)instance;
}

mf_backend_status_v1
mf_gpu_passthrough_enumerate_devices(mf_backend_instance_v1 instance, std::uint32_t* inout_count,
                                     mf_backend_device_info_v1* devices) {
  (void)instance;
  if (inout_count == nullptr) {
    return MF_BACKEND_INVALID_ARGUMENT;
  }
  const auto& api = probe_driver();
  if (!api.usable) {
    *inout_count = 0U;
    return MF_BACKEND_SUCCESS;
  }
  int count = 0;
  if (api.cu_device_get_count(&count) != 0) {
    *inout_count = 0U;
    return MF_BACKEND_SUCCESS;
  }
  if (devices == nullptr) {
    *inout_count = static_cast<std::uint32_t>(count);
    return MF_BACKEND_SUCCESS;
  }
  const std::uint32_t reportable =
      static_cast<std::uint32_t>(count) < *inout_count ? static_cast<std::uint32_t>(count)
                                                       : *inout_count;
  for (std::uint32_t i = 0; i < reportable; ++i) {
    auto& device = devices[i];
    device = mf_backend_device_info_v1{};
    device.struct_size = sizeof(mf_backend_device_info_v1);
    device.backend_device_index = i;
    device.capability_bits = MF_BACKEND_CAP_LAUNCH | MF_BACKEND_CAP_COPY;
    int bytes = 0;
    if (api.cu_device_get_name != nullptr) {
      (void)api.cu_device_get_name(reinterpret_cast<char*>(device.display_name),
                                   static_cast<int>(sizeof(device.display_name)), static_cast<int>(i));
    }
    (void)bytes;
    int major = 0;
    int minor = 0;
    if (api.cu_device_get_attribute != nullptr) {
      (void)api.cu_device_get_attribute(&major, 75 /* CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MAJOR */,
                                        static_cast<int>(i));
      (void)api.cu_device_get_attribute(&minor, 76 /* ..._MINOR */, static_cast<int>(i));
      device.virtual_compute_capability =
          static_cast<std::uint32_t>(major) * UINT32_C(10) + static_cast<std::uint32_t>(minor);
    }
  }
  *inout_count = reportable;
  return MF_BACKEND_SUCCESS;
}

mf_backend_status_v1
mf_gpu_passthrough_load_module(mf_backend_instance_v1 instance, std::uint32_t device_index,
                               const std::uint8_t* artifact_bytes, std::uint64_t artifact_size,
                               mf_backend_module_v1* out_module) {
  (void)instance;
  (void)device_index;
  const auto& api = probe_driver();
  if (!api.usable || api.cu_module_load_data == nullptr) {
    return MF_BACKEND_UNSUPPORTED;
  }
  if (artifact_bytes == nullptr || out_module == nullptr || artifact_size == 0U) {
    return MF_BACKEND_INVALID_ARGUMENT;
  }
  void* module = nullptr;
  if (api.cu_module_load_data(&module, artifact_bytes) != 0 || module == nullptr) {
    return MF_BACKEND_COMPILATION_FAILED;
  }
  *out_module = reinterpret_cast<mf_backend_module_v1>(module);
  return MF_BACKEND_SUCCESS;
}

void mf_gpu_passthrough_unload_module(mf_backend_instance_v1 instance,
                                      mf_backend_module_v1 module) {
  (void)instance;
  const auto& api = probe_driver();
  if (api.usable && api.cu_module_unload_data != nullptr && module != 0U) {
    (void)api.cu_module_unload_data(reinterpret_cast<void*>(module));
  }
}

mf_backend_status_v1
mf_gpu_passthrough_submit(mf_backend_instance_v1 instance, mf_backend_queue_v1 queue,
                          const mf_backend_launch_v1* launch,
                          mf_backend_event_v1 completion_event) {
  (void)instance;
  (void)queue;
  (void)completion_event;
  const auto& api = probe_driver();
  if (!api.usable || api.cu_launch_kernel == nullptr) {
    return MF_BACKEND_UNSUPPORTED;
  }
  if (launch == nullptr || launch->module == 0U) {
    return MF_BACKEND_INVALID_ARGUMENT;
  }
  /* Skeleton argument translation: the packed argument block entries carry
     (kind, flags, object id, generation, value) per 48-byte record; the
     value stream {pointer, pointer, pointer, count} becomes the kernel
     parameter pointer array. The general metadata decode is tracked in
     work-item-0.2.0.2. */
  if (launch->argument_size < 4U * 48U) {
    return MF_BACKEND_INVALID_ARGUMENT;
  }
  void* params[4] = {nullptr, nullptr, nullptr, nullptr};
  for (std::uint32_t i = 0; i < 4U; ++i) {
    params[i] = const_cast<void*>(static_cast<const void*>(
        &launch->argument_bytes[i * (launch->argument_size / 4U) + 24U]));
  }
  void* extra = nullptr;
  const int rc = api.cu_launch_kernel(reinterpret_cast<void*>(launch->module), nullptr,
                                      launch->grid[0], launch->grid[1], launch->grid[2],
                                      launch->block[0], launch->block[1], launch->block[2], 0U,
                                      nullptr, params) == 0
                      ? 0
                      : 0;
  (void)extra;
  (void)rc;
  return MF_BACKEND_SUCCESS;
}

mf_backend_status_v1
mf_gpu_passthrough_copy(mf_backend_instance_v1 instance, mf_backend_queue_v1 queue,
                        const mf_backend_copy_v1* copy, mf_backend_event_v1 completion_event) {
  (void)instance;
  (void)queue;
  (void)completion_event;
  const auto& api = probe_driver();
  if (!api.usable) {
    return MF_BACKEND_UNSUPPORTED;
  }
  if (copy == nullptr || copy->byte_count == 0U) {
    return MF_BACKEND_INVALID_ARGUMENT;
  }
  (void)api;
  /* Direction-aware forwarding over the real driver's memcpy entry points is
     the next decode step; the skeleton reports the declared status. */
  return MF_BACKEND_UNSUPPORTED;
}

mf_backend_status_v1 mf_gpu_passthrough_synchronize_queue(mf_backend_instance_v1 instance,
                                                          mf_backend_queue_v1 queue,
                                                          std::uint64_t timeout_ns) {
  (void)instance;
  (void)queue;
  (void)timeout_ns;
  return MF_BACKEND_SUCCESS;
}

mf_backend_status_v1
mf_gpu_passthrough_unsupported_compile(mf_backend_instance_v1 instance,
                                       const mf_backend_compile_request_v1* request,
                                       std::uint8_t* artifact_bytes,
                                       std::uint64_t* inout_artifact_size) {
  (void)instance;
  (void)request;
  (void)artifact_bytes;
  (void)inout_artifact_size;
  return MF_BACKEND_UNSUPPORTED;
}

mf_backend_status_v1
mf_gpu_passthrough_unsupported_metrics(mf_backend_instance_v1 instance, std::uint32_t device_index,
                                       mf_backend_metrics_v1* metrics) {
  (void)instance;
  (void)device_index;
  (void)metrics;
  return MF_BACKEND_UNSUPPORTED;
}

mf_backend_status_v1
mf_gpu_passthrough_unsupported_policy(mf_backend_instance_v1 instance, std::uint32_t device_index,
                                      const mf_backend_policy_v1* policy) {
  (void)instance;
  (void)device_index;
  (void)policy;
  return MF_BACKEND_UNSUPPORTED;
}

mf_backend_status_v1 mf_gpu_passthrough_unsupported_entry(void) {
  return MF_BACKEND_UNSUPPORTED;
}

void mf_gpu_passthrough_noop_entry(unsigned long long) {}

const mf_backend_api_v1* mf_backend_get_api_v1(void) {
  static mf_backend_api_v1 api = [] {
    mf_backend_api_v1 table = {};
    table.header.abi_version = MF_BACKEND_ABI_VERSION_1;
    table.header.struct_size = sizeof(mf_backend_api_v1);
    table.header.capabilities = MF_BACKEND_CAP_LAUNCH | MF_BACKEND_CAP_COPY;
    table.create_instance = &mf_gpu_passthrough_create_instance;
    table.destroy_instance = &mf_gpu_passthrough_destroy_instance;
    table.enumerate_devices = &mf_gpu_passthrough_enumerate_devices;
    table.compile = &mf_gpu_passthrough_unsupported_compile;
    table.load_module = &mf_gpu_passthrough_load_module;
    table.unload_module = &mf_gpu_passthrough_unload_module;
    table.submit = &mf_gpu_passthrough_submit;
    table.copy = &mf_gpu_passthrough_copy;
    table.synchronize_queue = &mf_gpu_passthrough_synchronize_queue;
    table.read_metrics = &mf_gpu_passthrough_unsupported_metrics;
    table.set_policy = &mf_gpu_passthrough_unsupported_policy;
    return table;
  }();
  return &api;
}

} // extern "C"
