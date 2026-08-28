#include "metaflux/nvml/provider.h"

#include "metaflux/client/fastpath.h"

uint32_t mf_nvml_provider_bootstrap_abi_version(void) {
  return mf_client_fastpath_bootstrap_abi_version();
}
