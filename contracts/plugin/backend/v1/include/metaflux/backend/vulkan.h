#ifndef METAFLUX_BACKEND_VULKAN_H
#define METAFLUX_BACKEND_VULKAN_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MF_VULKAN_CAPABILITY_ABI_VERSION_1 UINT32_C(1)
#define MF_VULKAN_API_VERSION_1_3 UINT32_C(0x00403000)

/* Locked milestone-0.1.3.1 capability baseline: Vulkan 1.3 compute plus the
 * three features every advertised Kernel IR path requires. A probe that cannot
 * satisfy this set is unsupported, never silently downgraded. */
#define MF_VULKAN_BASELINE_MIN_API_VERSION MF_VULKAN_API_VERSION_1_3
#define MF_VULKAN_FEATURE_TIMELINE_SEMAPHORE (UINT32_C(1) << 0U)
#define MF_VULKAN_FEATURE_SYNCHRONIZATION2 (UINT32_C(1) << 1U)
#define MF_VULKAN_FEATURE_BUFFER_DEVICE_ADDRESS (UINT32_C(1) << 2U)
#define MF_VULKAN_KNOWN_FEATURE_FLAGS                                                  \
  (MF_VULKAN_FEATURE_TIMELINE_SEMAPHORE | MF_VULKAN_FEATURE_SYNCHRONIZATION2 |          \
   MF_VULKAN_FEATURE_BUFFER_DEVICE_ADDRESS)
#define MF_VULKAN_BASELINE_REQUIRED_FEATURE_FLAGS MF_VULKAN_KNOWN_FEATURE_FLAGS

#define MF_VULKAN_MEMORY_TIER_STAGING (UINT32_C(1) << 0U)
#define MF_VULKAN_KNOWN_MEMORY_TIER_FLAGS MF_VULKAN_MEMORY_TIER_STAGING

/* Two independent driver families for dual-matrix qualification. PCI vendor IDs
 * are the durable family keys; unknown vendors remain classifiable as OTHER so a
 * host probe can still succeed when the baseline features are present. */
#define MF_VULKAN_VENDOR_ID_AMD UINT32_C(0x1002)
#define MF_VULKAN_VENDOR_ID_NVIDIA UINT32_C(0x10DE)

typedef enum mf_vulkan_driver_family_v1 {
  MF_VULKAN_DRIVER_FAMILY_UNKNOWN = 0,
  MF_VULKAN_DRIVER_FAMILY_AMD = 1,
  MF_VULKAN_DRIVER_FAMILY_NVIDIA = 2,
  MF_VULKAN_DRIVER_FAMILY_OTHER = 3,
} mf_vulkan_driver_family_v1;

/* Host-side execution timeline anchors used by direct-Vulkan baselines. Values
 * are monotonic nanoseconds observed around enqueue, vkQueueSubmit2, kernel
 * start visibility, and completion observation. Ordering is required. */
typedef enum mf_vulkan_timestamp_phase_v1 {
  MF_VULKAN_TIMESTAMP_PHASE_ENQUEUE = 1,
  MF_VULKAN_TIMESTAMP_PHASE_SUBMIT = 2,
  MF_VULKAN_TIMESTAMP_PHASE_START = 3,
  MF_VULKAN_TIMESTAMP_PHASE_COMPLETION = 4,
} mf_vulkan_timestamp_phase_v1;

typedef struct mf_vulkan_execution_timestamps_v1 {
  uint32_t struct_size;
  uint32_t abi_version;
  uint64_t enqueue_ns;
  uint64_t submit_ns;
  uint64_t start_ns;
  uint64_t completion_ns;
} mf_vulkan_execution_timestamps_v1;

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

static inline mf_vulkan_driver_family_v1 mf_vulkan_driver_family_from_vendor_id_v1(
    uint32_t vendor_id) {
  if (vendor_id == MF_VULKAN_VENDOR_ID_AMD) {
    return MF_VULKAN_DRIVER_FAMILY_AMD;
  }
  if (vendor_id == MF_VULKAN_VENDOR_ID_NVIDIA) {
    return MF_VULKAN_DRIVER_FAMILY_NVIDIA;
  }
  if (vendor_id == 0U) {
    return MF_VULKAN_DRIVER_FAMILY_UNKNOWN;
  }
  return MF_VULKAN_DRIVER_FAMILY_OTHER;
}

static inline int mf_vulkan_baseline_features_satisfied_v1(uint32_t api_version,
                                                           uint32_t feature_flags) {
  return api_version >= MF_VULKAN_BASELINE_MIN_API_VERSION &&
                 (feature_flags & MF_VULKAN_BASELINE_REQUIRED_FEATURE_FLAGS) ==
                     MF_VULKAN_BASELINE_REQUIRED_FEATURE_FLAGS
             ? 1
             : 0;
}

static inline int mf_vulkan_execution_timestamps_ordered_v1(
    const mf_vulkan_execution_timestamps_v1* points) {
  return points != NULL && points->struct_size == (uint32_t)sizeof(*points) &&
                 points->abi_version == MF_VULKAN_CAPABILITY_ABI_VERSION_1 &&
                 points->enqueue_ns != 0U && points->submit_ns >= points->enqueue_ns &&
                 points->start_ns >= points->submit_ns &&
                 points->completion_ns >= points->start_ns
             ? 1
             : 0;
}

/* The probe owns no Vulkan handles after it returns. */
mf_vulkan_probe_status_v1 mf_vulkan_probe_capabilities_v1(
    mf_vulkan_capability_profile_v1* out_profile);

const char* mf_vulkan_probe_status_string_v1(mf_vulkan_probe_status_v1 status);

#ifdef __cplusplus
}
#endif

#endif
