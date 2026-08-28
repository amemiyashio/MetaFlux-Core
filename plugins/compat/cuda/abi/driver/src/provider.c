#include "metaflux/cuda/provider.h"

#include "metaflux/client/fastpath.h"

uint32_t mf_cuda_provider_bootstrap_abi_version(void) {
  return mf_client_fastpath_bootstrap_abi_version();
}
