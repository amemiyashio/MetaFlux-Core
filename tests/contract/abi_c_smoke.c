#include "metaflux/backend/api.h"

#include <stddef.h>

_Static_assert(offsetof(mf_backend_api_header_v1, abi_version) == 0, "ABI version offset");
_Static_assert(offsetof(mf_backend_api_header_v1, struct_size) == 4, "ABI size offset");
_Static_assert(sizeof(((mf_backend_api_header_v1*)0)->capabilities) == 8, "capability width");
_Static_assert(offsetof(mf_backend_api_v1, header) == 0, "function table header offset");

int main(void) {
  const mf_backend_api_header_v1 header = {
      .abi_version = MF_BACKEND_ABI_VERSION_1,
      .struct_size = (uint32_t)sizeof(mf_backend_api_header_v1),
      .capabilities = 0,
      .extensions = NULL,
  };
  const mf_backend_api_v1 api = {.header = header};
  return api.header.abi_version == MF_BACKEND_ABI_VERSION_1 ? 0 : 1;
}
