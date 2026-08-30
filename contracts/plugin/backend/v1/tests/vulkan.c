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

int main(void) {
  mf_vulkan_capability_profile_v1 profile = {0};
  profile.struct_size = (uint32_t)sizeof(profile);
  profile.abi_version = MF_VULKAN_CAPABILITY_ABI_VERSION_1;
  profile.feature_flags = MF_VULKAN_FEATURE_TIMELINE_SEMAPHORE |
                          MF_VULKAN_FEATURE_SYNCHRONIZATION2 |
                          MF_VULKAN_FEATURE_BUFFER_DEVICE_ADDRESS;
  return profile.struct_size == sizeof(profile) && profile.abi_version == 1U &&
                 profile.feature_flags != 0U && MF_VULKAN_API_VERSION_1_3 != 0U
             ? 0
             : 1;
}
