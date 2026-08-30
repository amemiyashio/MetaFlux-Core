#include "metaflux/backend/vulkan_target.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iterator>

namespace {

mf_vulkan_capability_profile_v1 profile() {
  mf_vulkan_capability_profile_v1 result{};
  result.struct_size = sizeof(result);
  result.abi_version = MF_VULKAN_CAPABILITY_ABI_VERSION_1;
  result.status = MF_VULKAN_PROBE_SUCCESS;
  result.api_version = MF_VULKAN_API_VERSION_1_3;
  result.queue_count = 1U;
  result.subgroup_size_min = 64U;
  result.subgroup_size_max = 64U;
  result.max_compute_workgroup_invocations = 1024U;
  result.max_compute_workgroup_size[0] = 1024U;
  result.max_compute_workgroup_size[1] = 1024U;
  result.max_compute_workgroup_size[2] = 64U;
  result.feature_flags = MF_VULKAN_FEATURE_TIMELINE_SEMAPHORE |
                         MF_VULKAN_FEATURE_SYNCHRONIZATION2 |
                         MF_VULKAN_FEATURE_BUFFER_DEVICE_ADDRESS;
  result.memory_tier_flags = MF_VULKAN_MEMORY_TIER_STAGING;
  std::strcpy(result.target_environment, "schema=metaflux.vulkan.target.v1;api=4030000");
  std::fill(std::begin(result.target_digest), std::end(result.target_digest), 0x2aU);
  return result;
}

metaflux::backend::vulkan::SpirvModuleRequirements module_for(
    const mf_vulkan_capability_profile_v1& target) {
  metaflux::backend::vulkan::SpirvModuleRequirements module{};
  std::memcpy(module.target_digest.data(), target.target_digest, module.target_digest.size());
  module.required_features = MF_VULKAN_FEATURE_BUFFER_DEVICE_ADDRESS;
  module.workgroup_size = {8U, 4U, 1U};
  module.address_space_flags = metaflux::backend::vulkan::kAddressStorageBuffer;
  module.requires_buffer_device_address = true;
  return module;
}

bool target_profile_guards() {
  auto target = profile();
  if (metaflux::backend::vulkan::validate_target_profile(
          target, MF_VULKAN_FEATURE_BUFFER_DEVICE_ADDRESS) !=
      metaflux::backend::vulkan::TargetStatus::success) {
    return false;
  }
  target.feature_flags &= ~MF_VULKAN_FEATURE_BUFFER_DEVICE_ADDRESS;
  if (metaflux::backend::vulkan::validate_target_profile(
          target, MF_VULKAN_FEATURE_BUFFER_DEVICE_ADDRESS) !=
      metaflux::backend::vulkan::TargetStatus::unsupported_features) {
    return false;
  }
  target = profile();
  target.target_environment[0] = '\0';
  return metaflux::backend::vulkan::validate_target_profile(target, 0U) ==
         metaflux::backend::vulkan::TargetStatus::invalid_profile;
}

bool module_guards() {
  const auto target = profile();
  auto module = module_for(target);
  if (metaflux::backend::vulkan::validate_spirv_module(target, module) !=
      metaflux::backend::vulkan::TargetStatus::success) {
    return false;
  }
  module.target_digest[0] ^= 0xffU;
  if (metaflux::backend::vulkan::validate_spirv_module(target, module) !=
      metaflux::backend::vulkan::TargetStatus::target_mismatch) {
    return false;
  }
  module = module_for(target);
  module.workgroup_size[0] = 2048U;
  if (metaflux::backend::vulkan::validate_spirv_module(target, module) !=
      metaflux::backend::vulkan::TargetStatus::limit_exceeded) {
    return false;
  }
  module = module_for(target);
  module.subgroup_width = 32U;
  if (metaflux::backend::vulkan::validate_spirv_module(target, module) !=
      metaflux::backend::vulkan::TargetStatus::unsupported_semantics) {
    return false;
  }
  module = module_for(target);
  module.address_space_flags = 0x80U;
  return metaflux::backend::vulkan::validate_spirv_module(target, module) ==
         metaflux::backend::vulkan::TargetStatus::invalid_module;
}

} // namespace

int main() {
  const bool ok = target_profile_guards() && module_guards();
  std::printf("vulkan target preflight: %s\n", ok ? "pass" : "fail");
  return ok ? 0 : 1;
}
