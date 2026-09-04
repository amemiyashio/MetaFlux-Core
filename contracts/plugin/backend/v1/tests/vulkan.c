#include "metaflux/backend/vulkan.h"

#include <stddef.h>
#include <stdint.h>

_Static_assert(offsetof(mf_vulkan_capability_profile_v1, struct_size) == 0,
               "Vulkan profile size offset");
_Static_assert(offsetof(mf_vulkan_capability_profile_v1, target_digest) >
                   offsetof(mf_vulkan_capability_profile_v1, target_environment),
               "Vulkan target digest ordering");
_Static_assert(sizeof(((mf_vulkan_capability_profile_v1*)0)->target_digest) == 32,
               "Vulkan target digest width");
_Static_assert(offsetof(mf_vulkan_execution_timestamps_v1, enqueue_ns) == 8,
               "Vulkan timestamp enqueue offset");
_Static_assert(sizeof(mf_vulkan_execution_timestamps_v1) == 40, "Vulkan timestamp record size");

static int prove_driver_families(void) {
  return mf_vulkan_driver_family_from_vendor_id_v1(MF_VULKAN_VENDOR_ID_AMD) ==
             MF_VULKAN_DRIVER_FAMILY_AMD &&
         mf_vulkan_driver_family_from_vendor_id_v1(MF_VULKAN_VENDOR_ID_NVIDIA) ==
             MF_VULKAN_DRIVER_FAMILY_NVIDIA &&
         mf_vulkan_driver_family_from_vendor_id_v1(0U) == MF_VULKAN_DRIVER_FAMILY_UNKNOWN &&
         mf_vulkan_driver_family_from_vendor_id_v1(UINT32_C(0x8086)) ==
             MF_VULKAN_DRIVER_FAMILY_OTHER &&
         MF_VULKAN_VENDOR_ID_AMD != MF_VULKAN_VENDOR_ID_NVIDIA;
}

static int prove_baseline_lock(void) {
  const uint32_t required = MF_VULKAN_BASELINE_REQUIRED_FEATURE_FLAGS;
  return mf_vulkan_baseline_features_satisfied_v1(MF_VULKAN_BASELINE_MIN_API_VERSION, required) ==
             1 &&
         mf_vulkan_baseline_features_satisfied_v1(MF_VULKAN_API_VERSION_1_3 - 1U, required) == 0 &&
         mf_vulkan_baseline_features_satisfied_v1(
             MF_VULKAN_API_VERSION_1_3, required & ~MF_VULKAN_FEATURE_BUFFER_DEVICE_ADDRESS) == 0 &&
         required == (MF_VULKAN_FEATURE_TIMELINE_SEMAPHORE | MF_VULKAN_FEATURE_SYNCHRONIZATION2 |
                      MF_VULKAN_FEATURE_BUFFER_DEVICE_ADDRESS);
}

static int prove_timestamp_ordering(void) {
  mf_vulkan_execution_timestamps_v1 points = {0};
  points.struct_size = (uint32_t)sizeof(points);
  points.abi_version = MF_VULKAN_CAPABILITY_ABI_VERSION_1;
  points.enqueue_ns = UINT64_C(100);
  points.submit_ns = UINT64_C(200);
  points.start_ns = UINT64_C(300);
  points.completion_ns = UINT64_C(400);
  if (mf_vulkan_execution_timestamps_ordered_v1(&points) != 1) {
    return 0;
  }
  points.start_ns = UINT64_C(150);
  if (mf_vulkan_execution_timestamps_ordered_v1(&points) != 0) {
    return 0;
  }
  points.start_ns = UINT64_C(300);
  points.enqueue_ns = 0U;
  return mf_vulkan_execution_timestamps_ordered_v1(&points) == 0;
}

int main(void) {
  mf_vulkan_capability_profile_v1 profile = {0};
  profile.struct_size = (uint32_t)sizeof(profile);
  profile.abi_version = MF_VULKAN_CAPABILITY_ABI_VERSION_1;
  profile.feature_flags = MF_VULKAN_BASELINE_REQUIRED_FEATURE_FLAGS;
  profile.api_version = MF_VULKAN_BASELINE_MIN_API_VERSION;
  profile.vendor_id = MF_VULKAN_VENDOR_ID_AMD;
  return profile.struct_size == sizeof(profile) && profile.abi_version == 1U &&
                 mf_vulkan_baseline_features_satisfied_v1(profile.api_version,
                                                          profile.feature_flags) == 1 &&
                 mf_vulkan_driver_family_from_vendor_id_v1(profile.vendor_id) ==
                     MF_VULKAN_DRIVER_FAMILY_AMD &&
                 prove_driver_families() && prove_baseline_lock() && prove_timestamp_ordering()
             ? 0
             : 1;
}
