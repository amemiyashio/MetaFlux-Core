#ifndef METAFLUX_BACKEND_VULKAN_LOWERING_HPP
#define METAFLUX_BACKEND_VULKAN_LOWERING_HPP

#include "metaflux/backend/vulkan_target.hpp"
#include "metaflux/compiler/kernel_ir.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace metaflux::backend::vulkan {

enum class LoweringStatus : std::uint32_t {
  success = 0,
  invalid_argument = 1,
  invalid_kernel = 2,
  invalid_target = 3,
  unsupported_semantics = 4,
  resource_exhausted = 5,
};

enum class SpirvSemanticOpcode : std::uint32_t {
  parameter_device_address = 0,
  parameter_scalar = 1,
  builtin_local_invocation_id = 2,
  builtin_workgroup_id = 3,
  builtin_workgroup_size = 4,
  builtin_num_workgroups = 5,
  add_u32 = 6,
  subtract_u32 = 7,
  multiply_low_u32 = 8,
  multiply_wide_u32 = 9,
  multiply_add_low_u32 = 10,
  add_address = 11,
  add_f32 = 12,
  subtract_f32 = 13,
  multiply_f32 = 14,
  multiply_add_f32 = 15,
  fused_multiply_add_f32 = 16,
  convert_f32_u32 = 17,
  convert_u32_f32 = 18,
  compare_u32 = 19,
  compare_f32 = 20,
  branch_conditional = 21,
  load_global_u32 = 22,
  store_global_u32 = 23,
  load_global_f32 = 24,
  store_global_f32 = 25,
  store_global_u64 = 26,
  load_workgroup_u32 = 27,
  store_workgroup_u32 = 28,
  control_barrier = 29,
  return_value = 30,
};

struct SpirvLoweredInstruction final {
  SpirvSemanticOpcode opcode = SpirvSemanticOpcode::return_value;
  std::uint32_t result = compiler::kNoValue;
  std::array<std::uint32_t, 3> inputs{compiler::kNoValue, compiler::kNoValue,
                                       compiler::kNoValue};
  std::uint32_t input_count = 0;
  std::uint32_t attribute = 0;
  std::uint32_t predicate = compiler::kNoValue;
  bool predicate_negated = false;
};

struct SpirvLoweredModule final {
  std::string entry_point;
  std::string target_environment;
  std::array<std::uint32_t, 3> workgroup_size{};
  SpirvModuleRequirements requirements{};
  SpirvReflection reflection{};
  std::vector<SpirvLoweredInstruction> instructions{};
  // canonical_text remains the stable semantic projection used by diagnostics
  // and cache identity tests. mlir_text and spirv_binary are the actual
  // target-constrained compiler products.
  std::string canonical_text;
  std::string mlir_text;
  std::vector<std::uint32_t> spirv_binary{};
};

struct LoweringResult final {
  LoweringStatus status = LoweringStatus::invalid_argument;
  std::string diagnostic;
};

[[nodiscard]] LoweringResult lower_kernel(const compiler::Kernel& kernel,
                                          const mf_vulkan_capability_profile_v1& profile,
                                          const std::array<std::uint32_t, 3>& workgroup_size,
                                          SpirvLoweredModule* out_module);

[[nodiscard]] const char* lowering_status_string(LoweringStatus status) noexcept;
[[nodiscard]] const char* spirv_semantic_opcode_string(SpirvSemanticOpcode opcode) noexcept;

} // namespace metaflux::backend::vulkan

#endif
