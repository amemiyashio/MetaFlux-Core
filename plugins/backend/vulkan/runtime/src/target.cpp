#include "metaflux/backend/vulkan_target.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>

namespace metaflux::backend::vulkan {

namespace {

bool digest_present(const std::uint8_t* digest, std::size_t size) noexcept {
  for (std::size_t index = 0U; index < size; ++index) {
    if (digest[index] != 0U) {
      return true;
    }
  }
  return false;
}

} // namespace

TargetStatus validate_target_profile(const mf_vulkan_capability_profile_v1& profile,
                                     std::uint32_t required_features) noexcept {
  if (profile.struct_size != sizeof(profile) ||
      profile.abi_version != MF_VULKAN_CAPABILITY_ABI_VERSION_1 ||
      profile.status != MF_VULKAN_PROBE_SUCCESS || profile.api_version < MF_VULKAN_API_VERSION_1_3 ||
      profile.queue_count == 0U || profile.max_compute_workgroup_invocations == 0U ||
      profile.max_compute_workgroup_size[0] == 0U || profile.max_compute_workgroup_size[1] == 0U ||
      profile.max_compute_workgroup_size[2] == 0U || profile.subgroup_size_min == 0U ||
      profile.subgroup_size_min > profile.subgroup_size_max || profile.target_environment[0] == '\0' ||
      !digest_present(profile.target_digest, sizeof(profile.target_digest))) {
    return TargetStatus::invalid_profile;
  }
  if ((profile.feature_flags & required_features) != required_features ||
      (profile.memory_tier_flags & MF_VULKAN_MEMORY_TIER_STAGING) == 0U) {
    return TargetStatus::unsupported_features;
  }
  return TargetStatus::success;
}

TargetStatus validate_spirv_module(const mf_vulkan_capability_profile_v1& profile,
                                   const SpirvModuleRequirements& module) noexcept {
  const auto profile_status = validate_target_profile(profile, module.required_features);
  if (profile_status != TargetStatus::success) {
    return profile_status;
  }
  if (std::memcmp(module.target_digest.data(), profile.target_digest,
                  module.target_digest.size()) != 0) {
    return TargetStatus::target_mismatch;
  }
  if (module.address_space_flags == 0U ||
      (module.address_space_flags & ~kKnownAddressSpaces) != 0U) {
    return TargetStatus::invalid_module;
  }
  if (module.requires_buffer_device_address &&
      (module.required_features & MF_VULKAN_FEATURE_BUFFER_DEVICE_ADDRESS) == 0U) {
    return TargetStatus::invalid_module;
  }
  if (module.subgroup_width != 0U &&
      (profile.subgroup_size_min != profile.subgroup_size_max ||
       module.subgroup_width != profile.subgroup_size_min)) {
    return TargetStatus::unsupported_semantics;
  }
  std::uint64_t invocations = 1U;
  for (std::size_t index = 0U; index < module.workgroup_size.size(); ++index) {
    if (module.workgroup_size[index] == 0U ||
        module.workgroup_size[index] > profile.max_compute_workgroup_size[index] ||
        invocations > std::numeric_limits<std::uint64_t>::max() / module.workgroup_size[index]) {
      return TargetStatus::limit_exceeded;
    }
    invocations *= module.workgroup_size[index];
  }
  if (invocations > profile.max_compute_workgroup_invocations) {
    return TargetStatus::limit_exceeded;
  }
  return TargetStatus::success;
}

const char* target_status_string(TargetStatus status) noexcept {
  switch (status) {
  case TargetStatus::success:
    return "success";
  case TargetStatus::invalid_profile:
    return "invalid-profile";
  case TargetStatus::unsupported_features:
    return "unsupported-features";
  case TargetStatus::invalid_module:
    return "invalid-module";
  case TargetStatus::target_mismatch:
    return "target-mismatch";
  case TargetStatus::limit_exceeded:
    return "limit-exceeded";
  case TargetStatus::unsupported_semantics:
    return "unsupported-semantics";
  }
  return "unknown";
}

} // namespace metaflux::backend::vulkan
