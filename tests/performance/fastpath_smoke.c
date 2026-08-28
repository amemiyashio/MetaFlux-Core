#include "metaflux/client/fastpath.h"

#include "metaflux/client/protocol.h"

int main(void) {
  uint32_t observed = 0;
  for (uint32_t index = 0; index < UINT32_C(10000); ++index) {
    observed |= mf_client_fastpath_bootstrap_abi_version();
  }
  return observed == MF_CLIENT_PROTOCOL_ABI_VERSION_1 ? 0 : 1;
}
