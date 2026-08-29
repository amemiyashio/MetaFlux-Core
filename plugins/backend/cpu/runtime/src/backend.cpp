#include "metaflux/backend/cpu.h"

#include <cstddef>

extern "C" const mf_backend_api_v1* mf_cpu_backend_get_api_v1(void) {
  static const mf_backend_api_v1 api = {
      .header =
          {
              .abi_version = MF_BACKEND_ABI_VERSION_1,
              .struct_size = static_cast<uint32_t>(sizeof(mf_backend_api_v1)),
              .capabilities = 0,
              .extensions = nullptr,
          },
      .create_instance = nullptr,
      .destroy_instance = nullptr,
      .enumerate_devices = nullptr,
      .compile = nullptr,
      .load_module = nullptr,
      .unload_module = nullptr,
      .create_context = nullptr,
      .destroy_context = nullptr,
      .create_queue = nullptr,
      .destroy_queue = nullptr,
      .allocate_memory = nullptr,
      .free_memory = nullptr,
      .submit = nullptr,
      .copy = nullptr,
      .create_event = nullptr,
      .destroy_event = nullptr,
      .query_event = nullptr,
      .wait_event = nullptr,
      .synchronize_queue = nullptr,
      .cancel_queue = nullptr,
      .read_metrics = nullptr,
      .set_policy = nullptr,
  };
  return &api;
}
