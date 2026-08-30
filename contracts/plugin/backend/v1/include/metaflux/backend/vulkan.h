#ifndef METAFLUX_BACKEND_VULKAN_H
#define METAFLUX_BACKEND_VULKAN_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MF_VULKAN_CAPABILITY_ABI_VERSION_1 UINT32_C(1)
#define MF_VULKAN_API_VERSION_1_3 UINT32_C(0x00403000)

#define MF_VULKAN_FEATURE_TIMELINE_SEMAPHORE (UINT32_C(1) << 0U)
#define MF_VULKAN_FEATURE_SYNCHRONIZATION2 (UINT32_C(1) << 1U)
#define MF_VULKAN_FEATURE_BUFFER_DEVICE_ADDRESS (UINT32_C(1) << 2U)

#define MF_VULKAN_MEMORY_TIER_STAGING (UINT32_C(1) << 0U)

typedef enum mf_vulkan_probe_status_v1 {
  MF_VULKAN_PROBE_SUCCESS = 0,
  MF_VULKAN_PROBE_INVALID_ARGUMENT = 1,
  MF_VULKAN_PROBE_LOADER_UNAVAILABLE = 2,
  MF_VULKAN_PROBE_INITIALIZATION_FAILED = 3,
  MF_VULKAN_PROBE_NO_DEVICE = 4,
  MF_VULKAN_PROBE_UNSUPPORTED_DEVICE = 5,
  MF_VULKAN_PROBE_INTERNAL_ERROR = 6,
} mf_vulkan_probe_status_v1;

typedef struct mf_vulkan_capability_profile_v1 {
  uint32_t struct_size;
  uint32_t abi_version;
  uint32_t status;
  uint32_t api_version;
  uint32_t driver_version;
  uint32_t vendor_id;
  uint32_t device_id;
  uint32_t device_type;
  uint32_t queue_family_index;
  uint32_t queue_count;
  uint32_t subgroup_size_min;
  uint32_t subgroup_size_max;
  uint32_t max_compute_workgroup_invocations;
  uint32_t max_compute_workgroup_size[3];
  uint64_t max_storage_buffer_range;
  uint64_t max_uniform_buffer_range;
  uint32_t feature_flags;
  uint32_t memory_tier_flags;
  uint32_t memory_heap_count;
  uint32_t memory_type_count;
  uint64_t device_local_heap_bytes;
  uint64_t host_visible_heap_bytes;
  uint8_t device_uuid[16];
  uint8_t driver_uuid[16];
  uint8_t pipeline_cache_uuid[16];
  char device_name[256];
  char target_environment[512];
  uint8_t target_digest[32];
  uint8_t reserved[32];
} mf_vulkan_capability_profile_v1;

/* The probe owns no Vulkan handles after it returns. */
mf_vulkan_probe_status_v1 mf_vulkan_probe_capabilities_v1(
    mf_vulkan_capability_profile_v1* out_profile);

const char* mf_vulkan_probe_status_string_v1(mf_vulkan_probe_status_v1 status);

#ifdef __cplusplus
}
#endif

#endif
