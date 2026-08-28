#include "metaflux/backend/cpu.h"

#include <cstddef>

int main() {
  const auto* api = mf_cpu_backend_get_api_v1();
  return api != nullptr && api->header.abi_version == MF_BACKEND_ABI_VERSION_1 &&
                 api->header.struct_size >= sizeof(mf_backend_api_v1)
             ? 0
             : 1;
}
