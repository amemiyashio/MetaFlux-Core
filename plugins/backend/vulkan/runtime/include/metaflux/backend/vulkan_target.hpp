#ifndef METAFLUX_BACKEND_VULKAN_TARGET_HPP
#define METAFLUX_BACKEND_VULKAN_TARGET_HPP

#include "metaflux/backend/vulkan.h"

#include <array>
#include <cstdint>

namespace metaflux::backend::vulkan {

enum class TargetStatus : std::uint32_t {
  success = 0,
  invalid_profile = 1,
  unsupported_features = 2,
  invalid_module = 3,
  target_mismatch = 4,
  limit_exceeded = 5,
  unsupported_semantics = 6,
};

constexpr std::uint32_t kAddressStorageBuffer = 1U << 0U;
constexpr std::uint32_t kAddressUniform = 1U << 1U;
constexpr std::uint32_t kAddressWorkgroup = 1U << 2U;
constexpr std::uint32_t kKnownAddressSpaces =
    kAddressStorageBuffer | kAddressUniform | kAddressWorkgroup;

struct SpirvModuleRequirements {
  std::array<std::uint8_t, 32> target_digest{};
  std::uint32_t required_features = 0;
  std::array<std::uint32_t, 3> workgroup_size{};
  std::uint32_t subgroup_width = 0;
  std::uint32_t address_space_flags = 0;
  bool requires_buffer_device_address = false;
};

[[nodiscard]] TargetStatus validate_target_profile(
    const mf_vulkan_capability_profile_v1& profile,
    std::uint32_t required_features) noexcept;

[[nodiscard]] TargetStatus validate_spirv_module(
    const mf_vulkan_capability_profile_v1& profile,
    const SpirvModuleRequirements& module) noexcept;

[[nodiscard]] const char* target_status_string(TargetStatus status) noexcept;

} // namespace metaflux::backend::vulkan

#endif
