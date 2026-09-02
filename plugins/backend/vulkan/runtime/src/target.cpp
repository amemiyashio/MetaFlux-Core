#include "metaflux/backend/vulkan_target.hpp"

#include "metaflux/backend/vulkan_arguments.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string_view>

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

bool bytes_present(const std::uint8_t* bytes, std::size_t size) noexcept {
  return digest_present(bytes, size);
}

bool bytes_clear(const std::uint8_t* bytes, std::size_t size) noexcept {
  return std::all_of(bytes, bytes + size, [](std::uint8_t byte) { return byte == 0U; });
}

bool terminated_string(const char* bytes, std::size_t size) noexcept {
  return bytes != nullptr && bytes[0] != '\0' && std::memchr(bytes, '\0', size) != nullptr;
}

bool valid_entry_point(std::string_view name) noexcept {
  if (name.empty() || name.size() > 255U) {
    return false;
  }
  return std::all_of(name.begin(), name.end(), [](char character) {
    const auto byte = static_cast<unsigned char>(character);
    return byte >= 0x21U && byte <= 0x7eU;
  });
}

} // namespace

TargetStatus validate_target_profile(const mf_vulkan_capability_profile_v1& profile,
                                     std::uint32_t required_features) noexcept {
  if (profile.struct_size != sizeof(profile) ||
      profile.abi_version != MF_VULKAN_CAPABILITY_ABI_VERSION_1 ||
      profile.status != MF_VULKAN_PROBE_SUCCESS ||
      profile.api_version < MF_VULKAN_API_VERSION_1_3 || profile.queue_count == 0U ||
      profile.queue_family_index == UINT32_MAX || profile.vendor_id == 0U ||
      profile.max_compute_workgroup_invocations == 0U ||
      profile.max_compute_workgroup_size[0] == 0U || profile.max_compute_workgroup_size[1] == 0U ||
      profile.max_compute_workgroup_size[2] == 0U || profile.subgroup_size_min == 0U ||
      profile.subgroup_size_min > profile.subgroup_size_max ||
      profile.max_storage_buffer_range == 0U || profile.max_uniform_buffer_range == 0U ||
      profile.memory_heap_count == 0U || profile.memory_type_count == 0U ||
      profile.device_local_heap_bytes == 0U || profile.host_visible_heap_bytes == 0U ||
      (profile.feature_flags & ~MF_VULKAN_KNOWN_FEATURE_FLAGS) != 0U ||
      (profile.memory_tier_flags & ~MF_VULKAN_KNOWN_MEMORY_TIER_FLAGS) != 0U ||
      !terminated_string(profile.target_environment, sizeof(profile.target_environment)) ||
      !bytes_present(profile.device_uuid, sizeof(profile.device_uuid)) ||
      !bytes_present(profile.driver_uuid, sizeof(profile.driver_uuid)) ||
      !bytes_present(profile.pipeline_cache_uuid, sizeof(profile.pipeline_cache_uuid)) ||
      !digest_present(profile.target_digest, sizeof(profile.target_digest)) ||
      !bytes_clear(profile.reserved, sizeof(profile.reserved))) {
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
      ((module.required_features & MF_VULKAN_FEATURE_BUFFER_DEVICE_ADDRESS) == 0U ||
       (module.address_space_flags & kAddressStorageBuffer) == 0U)) {
    return TargetStatus::invalid_module;
  }
  if (module.subgroup_width != 0U && (profile.subgroup_size_min != profile.subgroup_size_max ||
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

TargetStatus validate_spirv_reflection(const mf_vulkan_capability_profile_v1& profile,
                                       const SpirvModuleRequirements& module,
                                       const SpirvReflection& reflection) noexcept {
  const auto module_status = validate_spirv_module(profile, module);
  if (module_status != TargetStatus::success) {
    return module_status;
  }
  if (!valid_entry_point(reflection.entry_point) ||
      reflection.execution_model != kSpirvExecutionModelGlCompute ||
      reflection.required_features != module.required_features ||
      reflection.address_space_flags != module.address_space_flags ||
      reflection.workgroup_size != module.workgroup_size ||
      (reflection.builtin_flags & ~kKnownReflectionBuiltinFlags) != 0U ||
      (reflection.builtin_flags & kReflectionBuiltinLocalInvocationId) == 0U) {
    return TargetStatus::invalid_module;
  }
  if (std::memcmp(reflection.argument_target_digest.data(), profile.target_digest,
                  reflection.argument_target_digest.size()) != 0) {
    return TargetStatus::target_mismatch;
  }
  std::uint64_t expected_argument_size = 0U;
  if (mf_vulkan_argument_block_size_v1(reflection.argument_count, &expected_argument_size) !=
          MF_VULKAN_ARGUMENT_VALID ||
      expected_argument_size > std::numeric_limits<std::uint32_t>::max() ||
      reflection.argument_block_size != static_cast<std::uint32_t>(expected_argument_size)) {
    return TargetStatus::invalid_module;
  }
  const bool requires_workgroup_storage = (module.address_space_flags & kAddressWorkgroup) != 0U;
  if (reflection.has_workgroup_storage != requires_workgroup_storage) {
    return TargetStatus::invalid_module;
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
