#include "metaflux/backend/vulkan_lowering.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <initializer_list>
#include <iostream>
#include <string>
#include <sys/wait.h>
#include <unistd.h>

namespace {

using metaflux::backend::vulkan::LoweringStatus;
using metaflux::backend::vulkan::SpirvLoweredModule;
using metaflux::backend::vulkan::SpirvSemanticOpcode;
using metaflux::compiler::Kernel;
using metaflux::compiler::Opcode;
using metaflux::compiler::Operation;
using metaflux::compiler::Parameter;
using metaflux::compiler::ParameterKind;
using metaflux::compiler::Register;
using metaflux::compiler::SpecialRegister;
using metaflux::compiler::ValueKind;

Operation op(Opcode opcode, std::uint32_t result, std::initializer_list<std::uint32_t> inputs,
             std::uint32_t attribute = 0U) {
  Operation result_operation{
      .opcode = opcode,
      .result = result,
      .inputs = {},
      .input_count = static_cast<std::uint32_t>(inputs.size()),
      .attribute = attribute,
  };
  std::size_t index = 0U;
  for (const auto input : inputs) {
    result_operation.inputs[index++] = input;
  }
  return result_operation;
}

Kernel add_kernel() {
  Kernel kernel{
      .name = "add_u32",
      .parameters = {Parameter{.kind = ParameterKind::BufferU32},
                     Parameter{.kind = ParameterKind::BufferU32},
                     Parameter{.kind = ParameterKind::BufferU32},
                     Parameter{.kind = ParameterKind::ScalarU32}},
      .shared_allocations = {},
      .registers = {Register{.kind = ValueKind::GlobalAddress},
                    Register{.kind = ValueKind::GlobalAddress},
                    Register{.kind = ValueKind::GlobalAddress},
                    Register{.kind = ValueKind::U32},
                    Register{.kind = ValueKind::U32},
                    Register{.kind = ValueKind::U32},
                    Register{.kind = ValueKind::U32},
                    Register{.kind = ValueKind::U32},
                    Register{.kind = ValueKind::Predicate},
                    Register{.kind = ValueKind::U64},
                    Register{.kind = ValueKind::GlobalAddress},
                    Register{.kind = ValueKind::GlobalAddress},
                    Register{.kind = ValueKind::GlobalAddress},
                    Register{.kind = ValueKind::U32},
                    Register{.kind = ValueKind::U32},
                    Register{.kind = ValueKind::U32}},
      .operations = {op(Opcode::LoadParameterAddress, 0U, {}, 0U),
                     op(Opcode::LoadParameterAddress, 1U, {}, 1U),
                     op(Opcode::LoadParameterAddress, 2U, {}, 2U),
                     op(Opcode::LoadParameterU32, 3U, {}, 3U),
                     op(Opcode::MoveSpecialU32, 4U, {},
                        static_cast<std::uint32_t>(SpecialRegister::ThreadIdX)),
                     op(Opcode::MoveSpecialU32, 5U, {},
                        static_cast<std::uint32_t>(SpecialRegister::BlockIdX)),
                     op(Opcode::MoveSpecialU32, 6U, {},
                        static_cast<std::uint32_t>(SpecialRegister::BlockDimX)),
                     op(Opcode::MadLoU32, 7U, {5U, 6U, 4U}),
                     op(Opcode::SetPredicateGeU32, 8U, {7U, 3U}),
                     op(Opcode::BranchIf, metaflux::compiler::kNoValue, {8U}, 18U),
                     op(Opcode::MultiplyWideU32, 9U, {7U}, 4U),
                     op(Opcode::AddGlobalAddress, 10U, {0U, 9U}),
                     op(Opcode::AddGlobalAddress, 11U, {1U, 9U}),
                     op(Opcode::AddGlobalAddress, 12U, {2U, 9U}),
                     op(Opcode::LoadGlobalU32, 13U, {11U}),
                     op(Opcode::LoadGlobalU32, 14U, {12U}),
                     op(Opcode::AddU32, 15U, {13U, 14U}),
                     op(Opcode::StoreGlobalU32, metaflux::compiler::kNoValue, {10U, 15U}),
                     op(Opcode::Return, metaflux::compiler::kNoValue, {})},
  };
  return kernel;
}

Kernel copy_kernel() {
  Kernel kernel{
      .name = "copy_u32",
      .parameters = {Parameter{.kind = ParameterKind::BufferU32},
                     Parameter{.kind = ParameterKind::BufferU32},
                     Parameter{.kind = ParameterKind::ScalarU32}},
      .shared_allocations = {},
      .registers = {Register{.kind = ValueKind::GlobalAddress},
                    Register{.kind = ValueKind::GlobalAddress},
                    Register{.kind = ValueKind::U32},
                    Register{.kind = ValueKind::U32},
                    Register{.kind = ValueKind::U32},
                    Register{.kind = ValueKind::U32},
                    Register{.kind = ValueKind::U32},
                    Register{.kind = ValueKind::Predicate},
                    Register{.kind = ValueKind::U64},
                    Register{.kind = ValueKind::GlobalAddress},
                    Register{.kind = ValueKind::GlobalAddress},
                    Register{.kind = ValueKind::U32}},
      .operations = {op(Opcode::LoadParameterAddress, 0U, {}, 0U),
                     op(Opcode::LoadParameterAddress, 1U, {}, 1U),
                     op(Opcode::LoadParameterU32, 2U, {}, 2U),
                     op(Opcode::MoveSpecialU32, 3U, {},
                        static_cast<std::uint32_t>(SpecialRegister::ThreadIdX)),
                     op(Opcode::MoveSpecialU32, 4U, {},
                        static_cast<std::uint32_t>(SpecialRegister::BlockIdX)),
                     op(Opcode::MoveSpecialU32, 5U, {},
                        static_cast<std::uint32_t>(SpecialRegister::BlockDimX)),
                     op(Opcode::MadLoU32, 6U, {4U, 5U, 3U}),
                     op(Opcode::SetPredicateGeU32, 7U, {6U, 2U}),
                     op(Opcode::BranchIf, metaflux::compiler::kNoValue, {7U}, 14U),
                     op(Opcode::MultiplyWideU32, 8U, {6U}, 4U),
                     op(Opcode::AddGlobalAddress, 9U, {0U, 8U}),
                     op(Opcode::AddGlobalAddress, 10U, {1U, 8U}),
                     op(Opcode::LoadGlobalU32, 11U, {10U}),
                     op(Opcode::StoreGlobalU32, metaflux::compiler::kNoValue, {9U, 11U}),
                     op(Opcode::Return, metaflux::compiler::kNoValue, {})},
  };
  return kernel;
}

mf_vulkan_capability_profile_v1 target() {
  mf_vulkan_capability_profile_v1 result{};
  result.struct_size = sizeof(result);
  result.abi_version = MF_VULKAN_CAPABILITY_ABI_VERSION_1;
  result.status = MF_VULKAN_PROBE_SUCCESS;
  result.api_version = MF_VULKAN_API_VERSION_1_3;
  result.vendor_id = 1U;
  result.queue_family_index = 0U;
  result.queue_count = 1U;
  result.subgroup_size_min = 32U;
  result.subgroup_size_max = 64U;
  result.max_compute_workgroup_invocations = 1024U;
  result.max_compute_workgroup_size[0] = 1024U;
  result.max_compute_workgroup_size[1] = 1024U;
  result.max_compute_workgroup_size[2] = 64U;
  result.max_storage_buffer_range = 1U;
  result.max_uniform_buffer_range = 1U;
  result.feature_flags = MF_VULKAN_FEATURE_BUFFER_DEVICE_ADDRESS;
  result.memory_tier_flags = MF_VULKAN_MEMORY_TIER_STAGING;
  result.memory_heap_count = 1U;
  result.memory_type_count = 1U;
  result.device_local_heap_bytes = 1U;
  result.host_visible_heap_bytes = 1U;
  result.device_uuid[0] = 1U;
  result.driver_uuid[0] = 1U;
  result.pipeline_cache_uuid[0] = 1U;
  std::strcpy(result.target_environment, "schema=metaflux.vulkan.target.v1");
  std::fill(std::begin(result.target_digest), std::end(result.target_digest), 0x4aU);
  return result;
}

bool valid_add_lowering() {
  const auto profile = target();
  SpirvLoweredModule module{};
  const auto result = metaflux::backend::vulkan::lower_kernel(
      add_kernel(), profile, {8U, 1U, 1U}, &module);
  const bool valid = result.status == LoweringStatus::success && module.entry_point == "add_u32" &&
         module.instructions.size() == 19U && module.reflection.argument_count == 4U &&
         module.instructions[4].opcode == SpirvSemanticOpcode::builtin_local_invocation_id &&
         module.instructions[5].opcode == SpirvSemanticOpcode::builtin_workgroup_id &&
         module.instructions[6].opcode == SpirvSemanticOpcode::builtin_workgroup_size &&
         !module.reflection.has_workgroup_storage &&
         (module.reflection.builtin_flags &
          metaflux::backend::vulkan::kReflectionBuiltinLocalInvocationId) != 0U &&
         (module.reflection.builtin_flags &
          metaflux::backend::vulkan::kReflectionBuiltinWorkgroupId) != 0U &&
         (module.reflection.builtin_flags &
          metaflux::backend::vulkan::kReflectionBuiltinWorkgroupSize) != 0U &&
         module.canonical_text.find("spirv.branch_conditional") != std::string::npos &&
         module.canonical_text.find("spirv.builtin_workgroup_id result=5") !=
             std::string::npos &&
         module.canonical_text.find("spirv.builtin_workgroup_size result=6") !=
             std::string::npos &&
         module.canonical_text.find("spirv.store_global_u32") != std::string::npos &&
         module.canonical_text.find("schema=metaflux.vulkan.target.v1") != std::string::npos &&
         module.mlir_text.find("spirv.func @add_u32") != std::string::npos &&
         module.spirv_binary.size() > 5U &&
         module.spirv_binary[0] == 0x07230203U && module.spirv_binary[1] != 0U;
  if (!valid) {
    std::cerr << "Vulkan lowering failure: status="
              << metaflux::backend::vulkan::lowering_status_string(result.status)
              << " diagnostic=" << result.diagnostic << " mlir-bytes=" << module.mlir_text.size()
              << " spirv-words=" << module.spirv_binary.size() << '\n';
    if (!module.mlir_text.empty()) {
      std::cerr << module.mlir_text << '\n';
    }
  }
  return valid;
}

bool valid_copy_lowering() {
  const auto profile = target();
  SpirvLoweredModule module{};
  const auto result = metaflux::backend::vulkan::lower_kernel(
      copy_kernel(), profile, {8U, 1U, 1U}, &module);
  const bool valid = result.status == LoweringStatus::success && module.entry_point == "copy_u32" &&
         module.instructions.size() == 15U && module.reflection.argument_count == 3U &&
         module.instructions[12].opcode == SpirvSemanticOpcode::load_global_u32 &&
         module.instructions[13].opcode == SpirvSemanticOpcode::store_global_u32 &&
         module.canonical_text.find("spirv.load_global_u32") != std::string::npos &&
         module.canonical_text.find("spirv.store_global_u32") != std::string::npos &&
         module.canonical_text.find("spirv.add_u32") == std::string::npos &&
         module.mlir_text.find("spirv.Store") != std::string::npos &&
         module.spirv_binary.size() > 5U && module.spirv_binary[0] == 0x07230203U &&
         module.spirv_binary[1] != 0U;
  if (!valid) {
    std::cerr << "Vulkan Copy lowering failure: status="
              << metaflux::backend::vulkan::lowering_status_string(result.status)
              << " diagnostic=" << result.diagnostic << " mlir-bytes=" << module.mlir_text.size()
              << " spirv-words=" << module.spirv_binary.size() << '\n';
    if (!module.mlir_text.empty()) {
      std::cerr << module.mlir_text << '\n';
    }
  }
  return valid;
}

bool independently_validates_spirv() {
#ifdef METAFLUX_VULKAN_SPIRV_VAL
  const auto validate = [](const Kernel& kernel) {
    const auto profile = target();
    SpirvLoweredModule module{};
    const auto result = metaflux::backend::vulkan::lower_kernel(
        kernel, profile, {8U, 1U, 1U}, &module);
    if (result.status != LoweringStatus::success || module.spirv_binary.empty()) {
      return false;
    }

    char path[] = "/tmp/metaflux-vulkan-lowering-XXXXXX";
    const int fd = ::mkstemp(path);
    if (fd < 0) {
      (void)::unlink(path);
      return false;
    }
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(module.spirv_binary.data());
    std::size_t remaining = module.spirv_binary.size() * sizeof(std::uint32_t);
    while (remaining != 0U) {
      const ssize_t written = ::write(fd, bytes, remaining);
      if (written <= 0) {
        (void)::close(fd);
        (void)::unlink(path);
        return false;
      }
      bytes += written;
      remaining -= static_cast<std::size_t>(written);
    }
    (void)::close(fd);

    const pid_t child = ::fork();
    if (child == 0) {
      ::execl(METAFLUX_VULKAN_SPIRV_VAL, "spirv-val", "--target-env", "vulkan1.3", path,
              static_cast<char*>(nullptr));
      _exit(127);
    }
    if (child < 0) {
      (void)::unlink(path);
      return false;
    }
    int status = 0;
    const bool waited = ::waitpid(child, &status, 0) == child;
    (void)::unlink(path);
    const bool valid = waited && WIFEXITED(status) && WEXITSTATUS(status) == 0;
    if (!valid) {
      std::cerr << "SPIR-V validation failed for " << kernel.name << "; converted MLIR:\n"
                << module.mlir_text << '\n';
    }
    return valid;
  };
  return validate(add_kernel()) && validate(copy_kernel());
#else
  std::cout << "Vulkan lowering: spirv-val unavailable; independent validation skipped\n";
  return true;
#endif
}

bool unsupported_semantics_fail_before_emission() {
  auto kernel = add_kernel();
  kernel.operations[16] = op(Opcode::SubU32, 15U, {13U, 14U});
  SpirvLoweredModule module{};
  const auto result = metaflux::backend::vulkan::lower_kernel(
      kernel, target(), {8U, 1U, 1U}, &module);
  return result.status == LoweringStatus::unsupported_semantics &&
         result.diagnostic.find("verified u32 Add/Copy form") != std::string::npos &&
         module.spirv_binary.empty() && module.mlir_text.empty();
}

bool invalid_inputs() {
  auto profile = target();
  SpirvLoweredModule module{};
  auto invalid = add_kernel();
  invalid.operations.pop_back();
  const auto bad_kernel = metaflux::backend::vulkan::lower_kernel(
      invalid, profile, {8U, 1U, 1U}, &module);
  if (bad_kernel.status != LoweringStatus::invalid_kernel || bad_kernel.diagnostic.empty()) {
    return false;
  }
  profile.feature_flags = 0U;
  const auto bad_target = metaflux::backend::vulkan::lower_kernel(
      add_kernel(), profile, {8U, 1U, 1U}, &module);
  if (bad_target.status != LoweringStatus::invalid_target) {
    return false;
  }
  return metaflux::backend::vulkan::lower_kernel(add_kernel(), target(), {0U, 1U, 1U}, &module)
             .status == LoweringStatus::invalid_argument;
}

} // namespace

int main() {
  return valid_add_lowering() && valid_copy_lowering() && independently_validates_spirv() &&
                 unsupported_semantics_fail_before_emission() && invalid_inputs()
             ? 0
             : 1;
}
