#include "metaflux/client/fastpath.h"
#include "metaflux/client/protocol.h"

int main(void) {
  return mf_client_fastpath_bootstrap_abi_version() == MF_CLIENT_PROTOCOL_ABI_VERSION_1 ? 0 : 1;
}
