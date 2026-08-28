#include "metaflux/client/protocol.h"
#include "metaflux/runtime/core.hpp"

int main() {
  return metaflux::runtime::bootstrap_client_protocol_abi_version() ==
                 MF_CLIENT_PROTOCOL_ABI_VERSION_1
             ? 0
             : 1;
}
