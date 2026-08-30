#include "metaflux/backend/vulkan_capability.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <iterator>

int main() {
  mf_vulkan_capability_profile_v1 profile{};
  if (metaflux::backend::vulkan::probe(&profile) == MF_VULKAN_PROBE_INVALID_ARGUMENT ||
      profile.struct_size != sizeof(profile) ||
      profile.abi_version != MF_VULKAN_CAPABILITY_ABI_VERSION_1) {
    return 1;
  }
  const auto status = static_cast<mf_vulkan_probe_status_v1>(profile.status);
  if (status == MF_VULKAN_PROBE_LOADER_UNAVAILABLE || status == MF_VULKAN_PROBE_NO_DEVICE ||
      status == MF_VULKAN_PROBE_UNSUPPORTED_DEVICE) {
    std::printf("vulkan capability probe: %s (not qualified on this host)\n",
                mf_vulkan_probe_status_string_v1(status));
    return 0;
  }
  if (status != MF_VULKAN_PROBE_SUCCESS || profile.api_version < MF_VULKAN_API_VERSION_1_3 ||
      (profile.feature_flags & (MF_VULKAN_FEATURE_TIMELINE_SEMAPHORE |
                                MF_VULKAN_FEATURE_SYNCHRONIZATION2 |
                                MF_VULKAN_FEATURE_BUFFER_DEVICE_ADDRESS)) !=
          (MF_VULKAN_FEATURE_TIMELINE_SEMAPHORE | MF_VULKAN_FEATURE_SYNCHRONIZATION2 |
           MF_VULKAN_FEATURE_BUFFER_DEVICE_ADDRESS) ||
      profile.queue_count == 0U || profile.subgroup_size_min == 0U ||
      profile.subgroup_size_min != profile.subgroup_size_max ||
      profile.target_environment[0] == '\0') {
    return 2;
  }
  const auto digest_empty = std::all_of(
      std::begin(profile.target_digest), std::end(profile.target_digest),
      [](const std::uint8_t value) { return value == 0U; });
  if (digest_empty || profile.memory_tier_flags == 0U) {
    return 3;
  }
  mf_vulkan_capability_profile_v1 repeated{};
  if (metaflux::backend::vulkan::probe(&repeated) != MF_VULKAN_PROBE_SUCCESS ||
      std::strcmp(profile.target_environment, repeated.target_environment) != 0 ||
      std::memcmp(profile.target_digest, repeated.target_digest, sizeof(profile.target_digest)) !=
          0) {
    return 4;
  }
  std::printf("vulkan capability probe: %s api=%u vendor=0x%08x device=0x%08x queue=%u/%u "
              "subgroup=%u staging=%s digest=%02x%02x%02x%02x...\n",
              mf_vulkan_probe_status_string_v1(status), profile.api_version, profile.vendor_id,
              profile.device_id, profile.queue_family_index, profile.queue_count,
              profile.subgroup_size_min,
              (profile.memory_tier_flags & MF_VULKAN_MEMORY_TIER_STAGING) != 0U ? "yes" : "no",
              profile.target_digest[0], profile.target_digest[1], profile.target_digest[2],
              profile.target_digest[3]);
  return 0;
}
