#include "metaflux/runtime/core.hpp"

#include "metaflux/client/protocol.h"

namespace metaflux::runtime {

std::uint32_t bootstrap_client_protocol_abi_version() noexcept {
  return MF_CLIENT_PROTOCOL_ABI_VERSION_1;
}

} // namespace metaflux::runtime
