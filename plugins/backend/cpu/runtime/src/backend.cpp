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
  };
  return &api;
}
