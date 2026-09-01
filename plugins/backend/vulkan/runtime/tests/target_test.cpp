#include "metaflux/backend/vulkan_target.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iterator>
#include <limits>

namespace {

mf_vulkan_capability_profile_v1 profile() {
  mf_vulkan_capability_profile_v1 result{};
  result.struct_size = sizeof(result);
  result.abi_version = MF_VULKAN_CAPABILITY_ABI_VERSION_1;
  result.status = MF_VULKAN_PROBE_SUCCESS;
  result.api_version = MF_VULKAN_API_VERSION_1_3;
  result.vendor_id = 1U;
  result.queue_family_index = 0U;
  result.queue_count = 1U;
  result.subgroup_size_min = 64U;
  result.subgroup_size_max = 64U;
  result.max_compute_workgroup_invocations = 1024U;
  result.max_compute_workgroup_size[0] = 1024U;
  result.max_compute_workgroup_size[1] = 1024U;
  result.max_compute_workgroup_size[2] = 64U;
  result.max_storage_buffer_range = 1U;
  result.max_uniform_buffer_range = 1U;
  result.feature_flags = MF_VULKAN_FEATURE_TIMELINE_SEMAPHORE | MF_VULKAN_FEATURE_SYNCHRONIZATION2 |
                         MF_VULKAN_FEATURE_BUFFER_DEVICE_ADDRESS;
  result.memory_tier_flags = MF_VULKAN_MEMORY_TIER_STAGING;
  result.memory_heap_count = 1U;
  result.memory_type_count = 1U;
  result.device_local_heap_bytes = 1U;
  result.host_visible_heap_bytes = 1U;
  result.device_uuid[0] = 1U;
  result.driver_uuid[0] = 1U;
  result.pipeline_cache_uuid[0] = 1U;
  std::strcpy(result.target_environment, "schema=metaflux.vulkan.target.v1;api=4030000");
  std::fill(std::begin(result.target_digest), std::end(result.target_digest), 0x2aU);
  return result;
}

metaflux::backend::vulkan::SpirvModuleRequirements
module_for(const mf_vulkan_capability_profile_v1& target) {
  metaflux::backend::vulkan::SpirvModuleRequirements module{};
  std::memcpy(module.target_digest.data(), target.target_digest, module.target_digest.size());
  module.required_features = MF_VULKAN_FEATURE_BUFFER_DEVICE_ADDRESS;
  module.workgroup_size = {8U, 4U, 1U};
  module.address_space_flags = metaflux::backend::vulkan::kAddressStorageBuffer;
  module.requires_buffer_device_address = true;
  return module;
}

metaflux::backend::vulkan::SpirvReflection
reflection_for(const mf_vulkan_capability_profile_v1& target,
               const metaflux::backend::vulkan::SpirvModuleRequirements& module) {
  metaflux::backend::vulkan::SpirvReflection reflection{};
  reflection.entry_point = "metaflux_main";
  reflection.execution_model = metaflux::backend::vulkan::kSpirvExecutionModelGlCompute;
  reflection.workgroup_size = module.workgroup_size;
  reflection.required_features = module.required_features;
  reflection.address_space_flags = module.address_space_flags;
  reflection.builtin_flags = metaflux::backend::vulkan::kReflectionBuiltinLocalInvocationId |
                             metaflux::backend::vulkan::kReflectionBuiltinWorkgroupId |
                             metaflux::backend::vulkan::kReflectionBuiltinNumWorkgroups;
  reflection.argument_count = 2U;
  std::uint64_t argument_block_size = 0U;
  if (mf_vulkan_argument_block_size_v1(reflection.argument_count, &argument_block_size) !=
          MF_VULKAN_ARGUMENT_VALID ||
      argument_block_size > std::numeric_limits<std::uint32_t>::max()) {
    return {};
  }
  reflection.argument_block_size = static_cast<std::uint32_t>(argument_block_size);
  std::memcpy(reflection.argument_target_digest.data(), target.target_digest,
              reflection.argument_target_digest.size());
  return reflection;
}

bool target_profile_guards() {
  auto target = profile();
  if (metaflux::backend::vulkan::validate_target_profile(target,
                                                         MF_VULKAN_FEATURE_BUFFER_DEVICE_ADDRESS) !=
      metaflux::backend::vulkan::TargetStatus::success) {
    return false;
  }
  target.feature_flags &= ~MF_VULKAN_FEATURE_BUFFER_DEVICE_ADDRESS;
  if (metaflux::backend::vulkan::validate_target_profile(target,
                                                         MF_VULKAN_FEATURE_BUFFER_DEVICE_ADDRESS) !=
      metaflux::backend::vulkan::TargetStatus::unsupported_features) {
    return false;
  }
  target = profile();
  target.target_environment[0] = '\0';
  if (metaflux::backend::vulkan::validate_target_profile(target, 0U) !=
      metaflux::backend::vulkan::TargetStatus::invalid_profile) {
    return false;
  }
  target = profile();
  target.host_visible_heap_bytes = 0U;
  if (metaflux::backend::vulkan::validate_target_profile(target, 0U) !=
      metaflux::backend::vulkan::TargetStatus::invalid_profile) {
    return false;
  }
  target = profile();
  target.reserved[0] = 1U;
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
  module.address_space_flags = metaflux::backend::vulkan::kAddressUniform;
  if (metaflux::backend::vulkan::validate_spirv_module(target, module) !=
      metaflux::backend::vulkan::TargetStatus::invalid_module) {
    return false;
  }
  module = module_for(target);
  module.address_space_flags = 0x80U;
  return metaflux::backend::vulkan::validate_spirv_module(target, module) ==
         metaflux::backend::vulkan::TargetStatus::invalid_module;
}

bool reflection_guards() {
  const auto target = profile();
  const auto module = module_for(target);
  auto reflection = reflection_for(target, module);
  if (metaflux::backend::vulkan::validate_spirv_reflection(target, module, reflection) !=
      metaflux::backend::vulkan::TargetStatus::success) {
    return false;
  }
  reflection.argument_target_digest[0] ^= 0xffU;
  if (metaflux::backend::vulkan::validate_spirv_reflection(target, module, reflection) !=
      metaflux::backend::vulkan::TargetStatus::target_mismatch) {
    return false;
  }
  reflection = reflection_for(target, module);
  reflection.entry_point.clear();
  if (metaflux::backend::vulkan::validate_spirv_reflection(target, module, reflection) !=
      metaflux::backend::vulkan::TargetStatus::invalid_module) {
    return false;
  }
  reflection = reflection_for(target, module);
  reflection.execution_model = 0U;
  if (metaflux::backend::vulkan::validate_spirv_reflection(target, module, reflection) !=
      metaflux::backend::vulkan::TargetStatus::invalid_module) {
    return false;
  }
  reflection = reflection_for(target, module);
  reflection.builtin_flags = metaflux::backend::vulkan::kReflectionBuiltinWorkgroupId;
  if (metaflux::backend::vulkan::validate_spirv_reflection(target, module, reflection) !=
      metaflux::backend::vulkan::TargetStatus::invalid_module) {
    return false;
  }
  reflection = reflection_for(target, module);
  reflection.argument_block_size -= 1U;
  if (metaflux::backend::vulkan::validate_spirv_reflection(target, module, reflection) !=
      metaflux::backend::vulkan::TargetStatus::invalid_module) {
    return false;
  }
  auto shared_module = module;
  shared_module.address_space_flags |= metaflux::backend::vulkan::kAddressWorkgroup;
  reflection = reflection_for(target, shared_module);
  if (metaflux::backend::vulkan::validate_spirv_reflection(target, shared_module, reflection) !=
      metaflux::backend::vulkan::TargetStatus::invalid_module) {
    return false;
  }
  reflection.has_workgroup_storage = true;
  return metaflux::backend::vulkan::validate_spirv_reflection(target, shared_module, reflection) ==
         metaflux::backend::vulkan::TargetStatus::success;
}

} // namespace

int main() {
  const bool ok = target_profile_guards() && module_guards() && reflection_guards();
  std::printf("vulkan target preflight: %s\n", ok ? "pass" : "fail");
  return ok ? 0 : 1;
}
