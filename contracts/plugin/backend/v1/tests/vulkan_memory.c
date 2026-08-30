#include "metaflux/backend/vulkan_memory.h"

#include <stdint.h>

static mf_vulkan_external_memory_profile_v0 staging_profile(void) {
  mf_vulkan_external_memory_profile_v0 profile = {0};
  profile.struct_size = sizeof(profile);
  profile.abi_version = MF_VULKAN_EXTERNAL_MEMORY_ABI_VERSION_0;
  profile.tier = MF_VULKAN_MEMORY_TIER_STAGING_V0;
  profile.handle_type = MF_VULKAN_MEMORY_HANDLE_NONE_V0;
  profile.sync_type = MF_VULKAN_MEMORY_SYNC_NONE_V0;
  profile.flags = MF_VULKAN_MEMORY_FLAG_HOST_VISIBLE_V0 |
                  MF_VULKAN_MEMORY_FLAG_DEVICE_LOCAL_V0;
  profile.memory_type_bits = UINT32_C(0x3);
  profile.size = UINT64_C(4096);
  profile.alignment = UINT64_C(256);
  profile.generation = UINT64_C(2);
  profile.permissions = UINT64_C(3);
  return profile;
}

int main(void) {
  mf_vulkan_external_memory_profile_v0 staging = staging_profile();
  if (!mf_vulkan_external_memory_profile_valid_v0(&staging)) {
    return 1;
  }
  staging.flags |= MF_VULKAN_MEMORY_FLAG_DIRECT_IMPORT_V0;
  if (mf_vulkan_external_memory_profile_valid_v0(&staging)) {
    return 2;
  }
  staging = staging_profile();
  staging.tier = MF_VULKAN_MEMORY_TIER_DMA_BUF_V0;
  staging.handle_type = MF_VULKAN_MEMORY_HANDLE_DMA_BUF_V0;
  staging.sync_type = MF_VULKAN_MEMORY_SYNC_SEMAPHORE_FD_V0;
  staging.flags |= MF_VULKAN_MEMORY_FLAG_DIRECT_IMPORT_V0;
  return mf_vulkan_external_memory_profile_valid_v0(&staging) ? 0 : 3;
}
