#include "metaflux/backend/api.h"

#include <stddef.h>
#include <stdint.h>

_Static_assert(offsetof(mf_backend_api_header_v1, abi_version) == 0, "version offset");
_Static_assert(offsetof(mf_backend_api_header_v1, capabilities) == 8, "caps offset");
_Static_assert(offsetof(mf_backend_api_v1, create_instance) == 24, "first function offset");
_Static_assert(sizeof(mf_backend_instance_v1) == 8, "handle width");

int main(void) {
  const mf_backend_api_v1 api = {
      .header =
          {
              .abi_version = MF_BACKEND_ABI_VERSION_1,
              .struct_size = (uint32_t)sizeof(mf_backend_api_v1),
              .capabilities = MF_BACKEND_CAP_COPY | MF_BACKEND_CAP_EVENTS,
              .extensions = 0,
          },
  };

  if (mf_backend_api_validate_v1(&api, (uint32_t)sizeof(mf_backend_api_header_v1),
                                 MF_BACKEND_CAP_COPY) != MF_BACKEND_SUCCESS) {
    return 1;
  }
  if (mf_backend_api_validate_v1(&api, (uint32_t)sizeof(mf_backend_api_v1) + UINT32_C(8),
                                 UINT64_C(0)) != MF_BACKEND_UNSUPPORTED) {
    return 2;
  }
  if (mf_backend_api_validate_v1(&api, (uint32_t)sizeof(mf_backend_api_header_v1),
                                 MF_BACKEND_CAP_POLICY) != MF_BACKEND_UNSUPPORTED) {
    return 3;
  }
  return 0;
}
