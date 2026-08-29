#include "metaflux/backend/api.h"

#include <cstddef>
#include <cstdint>
#include <type_traits>

static_assert(std::is_standard_layout_v<mf_backend_api_header_v1>);
static_assert(std::is_standard_layout_v<mf_backend_api_v1>);
static_assert(offsetof(mf_backend_api_header_v1, abi_version) == 0);
static_assert(offsetof(mf_backend_api_header_v1, struct_size) == 4);
static_assert(sizeof(mf_backend_api_header_v1::capabilities) == 8);
static_assert(offsetof(mf_backend_api_v1, header) == 0);

int main() {
  const mf_backend_api_header_v1 header{
      MF_BACKEND_ABI_VERSION_1,
      static_cast<std::uint32_t>(sizeof(mf_backend_api_header_v1)),
      0,
      nullptr,
  };
  mf_backend_api_v1 api{};
  api.header = header;
  return api.header.abi_version == MF_BACKEND_ABI_VERSION_1 ? 0 : 1;
}
