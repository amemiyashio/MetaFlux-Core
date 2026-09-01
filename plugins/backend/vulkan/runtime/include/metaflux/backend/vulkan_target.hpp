#ifndef METAFLUX_BACKEND_VULKAN_TARGET_HPP
#define METAFLUX_BACKEND_VULKAN_TARGET_HPP

#include "metaflux/backend/vulkan.h"
#include "metaflux/backend/vulkan_arguments.h"

#include <array>
#include <cstdint>
#include <string>

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

constexpr std::uint32_t kSpirvExecutionModelGlCompute = 5U;
constexpr std::uint32_t kReflectionBuiltinLocalInvocationId = 1U << 0U;
constexpr std::uint32_t kReflectionBuiltinWorkgroupId = 1U << 1U;
constexpr std::uint32_t kReflectionBuiltinNumWorkgroups = 1U << 2U;
constexpr std::uint32_t kReflectionBuiltinWorkgroupSize = 1U << 3U;
constexpr std::uint32_t kKnownReflectionBuiltinFlags = kReflectionBuiltinLocalInvocationId |
                                                       kReflectionBuiltinWorkgroupId |
                                                       kReflectionBuiltinNumWorkgroups |
                                                       kReflectionBuiltinWorkgroupSize;

// Host-independent observations extracted by a future SPIR-V reflection pass.
// The verifier below keeps the packed argument and target contract explicit
// without creating a Vulkan shader module.
struct SpirvReflection {
  std::string entry_point;
  std::uint32_t execution_model = 0;
  std::array<std::uint32_t, 3> workgroup_size{};
  std::uint32_t required_features = 0;
  std::uint32_t address_space_flags = 0;
  std::uint32_t builtin_flags = 0;
  std::uint32_t argument_count = 0;
  std::uint32_t argument_block_size = 0;
  std::array<std::uint8_t, 32> argument_target_digest{};
  bool has_workgroup_storage = false;
};

[[nodiscard]] TargetStatus validate_target_profile(const mf_vulkan_capability_profile_v1& profile,
                                                   std::uint32_t required_features) noexcept;

[[nodiscard]] TargetStatus validate_spirv_module(const mf_vulkan_capability_profile_v1& profile,
                                                 const SpirvModuleRequirements& module) noexcept;

[[nodiscard]] TargetStatus validate_spirv_reflection(const mf_vulkan_capability_profile_v1& profile,
                                                     const SpirvModuleRequirements& module,
                                                     const SpirvReflection& reflection) noexcept;

[[nodiscard]] const char* target_status_string(TargetStatus status) noexcept;

} // namespace metaflux::backend::vulkan

#endif
