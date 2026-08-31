#ifndef METAFLUX_NVML_PROVIDER_H
#define METAFLUX_NVML_PROVIDER_H

#include <stdint.h>

#include "metaflux/nvml/abi.h"

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__clang__) || defined(__GNUC__)
#define MF_NVML_PROVIDER_API __attribute__((visibility("default")))
#else
#define MF_NVML_PROVIDER_API
#endif

MF_NVML_PROVIDER_API uint32_t mf_nvml_provider_bootstrap_abi_version(void);

#if defined(METAFLUX_PROVIDER_TESTING)
#include "metaflux/client/fastpath.h"

typedef struct mf_cuda_passthrough_policy_v1 mf_cuda_passthrough_policy_v1;

#define MF_NVML_PROVIDER_PROCESS_COMPUTE UINT32_C(1)
#define MF_NVML_PROVIDER_PROCESS_GRAPHICS UINT32_C(2)
#define MF_NVML_PROVIDER_PROCESS_MPS UINT32_C(4)

typedef struct mf_nvml_provider_test_process_v1 {
  uint32_t device_index;
  uint32_t pid;
  uint64_t used_gpu_memory;
  uint32_t gpu_instance_id;
  uint32_t compute_instance_id;
  uint32_t kinds;
  uint32_t reserved;
} mf_nvml_provider_test_process_v1;

typedef struct mf_nvml_provider_test_transport_v1 {
  int32_t registry_fd;
  mf_registry_view_id_v1 registry_view_id;
  const mf_nvml_provider_test_process_v1* processes;
  uint32_t process_count;
  uint32_t process_data_available;
} mf_nvml_provider_test_transport_v1;

typedef struct mf_nvml_provider_test_mode_snapshot_v1 {
  uint32_t mode_frozen;
  uint32_t requested_mode;
  uint32_t selected_runtime;
  uint32_t selector_status;
  int64_t namespace_id;
  char driver_build[64];
  char cuda_path[4096];
  char nvml_path[4096];
} mf_nvml_provider_test_mode_snapshot_v1;

MF_NVML_PROVIDER_API int
mf_nvml_provider_test_install_transport_v1(const mf_nvml_provider_test_transport_v1* transport);
MF_NVML_PROVIDER_API uint64_t mf_nvml_provider_test_process_view_revision_v1(void);
MF_NVML_PROVIDER_API int
mf_nvml_provider_test_install_passthrough_policy_v1(const mf_cuda_passthrough_policy_v1* policy);
MF_NVML_PROVIDER_API void mf_nvml_provider_test_force_dirty_rollback_v1(uint32_t enabled);
MF_NVML_PROVIDER_API int
mf_nvml_provider_test_get_mode_snapshot_v1(mf_nvml_provider_test_mode_snapshot_v1* snapshot);
MF_NVML_PROVIDER_API uint64_t mf_nvml_provider_test_vendor_call_count_v1(const char* symbol);
MF_NVML_PROVIDER_API uint32_t mf_nvml_provider_test_validate_vendor_surface_v1(void);
MF_NVML_PROVIDER_API void mf_nvml_provider_test_reset_v1(void);
#endif

#ifdef __cplusplus
}
#endif

#endif
