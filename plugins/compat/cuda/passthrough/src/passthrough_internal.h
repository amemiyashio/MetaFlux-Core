#ifndef METAFLUX_CUDA_PASSTHROUGH_INTERNAL_H
#define METAFLUX_CUDA_PASSTHROUGH_INTERNAL_H

#include "metaflux/cuda/passthrough.h"

#include <stddef.h>
#include <stdint.h>

#define MF_CUDA_PASSTHROUGH_MAX_DEFAULT_PAIRS UINT32_C(2)

typedef struct mf_cuda_passthrough_policy_v1 {
  uint32_t struct_size;
  uint32_t abi_version;
  uint32_t trusted_uid;
  uint32_t enforce_trusted_ancestors;
  const char* config_path;
  const char* proc_version_path;
  const char* pid_namespace_path;
  const char* mount_namespace_path;
  const char* install_root;
  const char* provider_self_path;
  uint32_t default_pair_count;
  uint32_t reserved;
  const char* default_cuda_paths[MF_CUDA_PASSTHROUGH_MAX_DEFAULT_PAIRS];
  const char* default_nvml_paths[MF_CUDA_PASSTHROUGH_MAX_DEFAULT_PAIRS];
} mf_cuda_passthrough_policy_v1;

void mf_cuda_passthrough_test_policy_init_v1(mf_cuda_passthrough_policy_v1* policy);

mf_cuda_passthrough_status_v1
mf_cuda_passthrough_pair_load_with_policy_v1(const mf_cuda_passthrough_policy_v1* policy,
                                             mf_cuda_passthrough_pair_v1** out_pair);

mf_cuda_passthrough_status_v1 mf_cuda_runtime_select_with_policy_v1(
    mf_cuda_runtime_mode_v1 requested_mode, const mf_cuda_managed_callbacks_v1* managed,
    const mf_cuda_passthrough_policy_v1* policy, mf_cuda_selected_runtime_v1* out_selected,
    mf_cuda_passthrough_pair_v1** out_pair);

#endif
