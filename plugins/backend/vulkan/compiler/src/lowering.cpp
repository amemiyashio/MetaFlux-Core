#include "metaflux/backend/vulkan_lowering.hpp"

#include <algorithm>
#include <cstring>
#include <limits>
#include <sstream>
#include <utility>

namespace metaflux::backend::vulkan {
namespace {

using compiler::Opcode;
using compiler::SpecialRegister;
using compiler::ValueKind;

LoweringStatus map_target_status(TargetStatus status) noexcept {
  switch (status) {
  case TargetStatus::success:
    return LoweringStatus::success;
  case TargetStatus::unsupported_features:
  case TargetStatus::limit_exceeded:
  case TargetStatus::target_mismatch:
  case TargetStatus::invalid_profile:
  case TargetStatus::invalid_module:
    return LoweringStatus::invalid_target;
  case TargetStatus::unsupported_semantics:
    return LoweringStatus::unsupported_semantics;
  }
  return LoweringStatus::invalid_target;
}

std::string first_kernel_diagnostic(const std::vector<compiler::Diagnostic>& diagnostics) {
  if (diagnostics.empty()) {
    return {};
  }
  const auto& diagnostic = diagnostics.front();
  std::ostringstream output;
  output << compiler::diagnostic_code_name(diagnostic.code) << " at "
         << diagnostic.location.line << ':' << diagnostic.location.column << ": "
         << diagnostic.message;
  if (!diagnostic.form.empty()) {
    output << " [" << diagnostic.form << ']';
  }
  return output.str();
}

SpirvSemanticOpcode map_opcode(Opcode opcode) noexcept {
  using enum Opcode;
  switch (opcode) {
  case LoadParameterAddress:
    return SpirvSemanticOpcode::parameter_device_address;
  case LoadParameterU32:
  case LoadParameterF32:
    return SpirvSemanticOpcode::parameter_scalar;
  case LoadSharedAddress:
    return SpirvSemanticOpcode::parameter_device_address;
  case MoveSpecialU32:
    return SpirvSemanticOpcode::builtin_local_invocation_id;
  case AddU32:
    return SpirvSemanticOpcode::add_u32;
  case SubU32:
    return SpirvSemanticOpcode::subtract_u32;
  case MultiplyLoU32:
    return SpirvSemanticOpcode::multiply_low_u32;
  case MadLoU32:
    return SpirvSemanticOpcode::multiply_add_low_u32;
  case MultiplyWideU32:
    return SpirvSemanticOpcode::multiply_wide_u32;
  case AddGlobalAddress:
  case AddSharedAddress:
    return SpirvSemanticOpcode::add_address;
  case AddRnF32:
    return SpirvSemanticOpcode::add_f32;
  case SubRnF32:
    return SpirvSemanticOpcode::subtract_f32;
  case MultiplyRnF32:
    return SpirvSemanticOpcode::multiply_f32;
  case MadRnF32:
    return SpirvSemanticOpcode::multiply_add_f32;
  case FmaRnF32:
    return SpirvSemanticOpcode::fused_multiply_add_f32;
  case ConvertRnF32U32:
    return SpirvSemanticOpcode::convert_f32_u32;
  case ConvertRziU32F32:
    return SpirvSemanticOpcode::convert_u32_f32;
  case SetPredicateGeU32:
  case SetPredicateEqU32:
    return SpirvSemanticOpcode::compare_u32;
  case SetPredicateLtF32:
    return SpirvSemanticOpcode::compare_f32;
  case BranchIf:
    return SpirvSemanticOpcode::branch_conditional;
  case LoadGlobalU32:
    return SpirvSemanticOpcode::load_global_u32;
  case StoreGlobalU32:
    return SpirvSemanticOpcode::store_global_u32;
  case LoadGlobalF32:
    return SpirvSemanticOpcode::load_global_f32;
  case StoreGlobalF32:
    return SpirvSemanticOpcode::store_global_f32;
  case LoadSharedU32:
    return SpirvSemanticOpcode::load_workgroup_u32;
  case StoreSharedU32:
    return SpirvSemanticOpcode::store_workgroup_u32;
  case BarrierSync:
    return SpirvSemanticOpcode::control_barrier;
  case Return:
    return SpirvSemanticOpcode::return_value;
  }
  return SpirvSemanticOpcode::return_value;
}

SpirvSemanticOpcode map_operation(const compiler::Operation& operation) noexcept {
  if (operation.opcode != Opcode::MoveSpecialU32) {
    return map_opcode(operation.opcode);
  }
  switch (static_cast<SpecialRegister>(operation.attribute)) {
  case SpecialRegister::ThreadIdX:
  case SpecialRegister::ThreadIdY:
    return SpirvSemanticOpcode::builtin_local_invocation_id;
  case SpecialRegister::BlockIdX:
  case SpecialRegister::BlockIdY:
    return SpirvSemanticOpcode::builtin_workgroup_id;
  case SpecialRegister::BlockDimX:
  case SpecialRegister::BlockDimY:
    return SpirvSemanticOpcode::builtin_workgroup_size;
  case SpecialRegister::GridDimX:
  case SpecialRegister::GridDimY:
    return SpirvSemanticOpcode::builtin_num_workgroups;
  }
  return SpirvSemanticOpcode::return_value;
}

bool has_global_address(const compiler::Kernel& kernel) noexcept {
  return std::any_of(kernel.parameters.begin(), kernel.parameters.end(), [](const auto& parameter) {
           return parameter.kind == compiler::ParameterKind::BufferU32;
         }) ||
         std::any_of(kernel.registers.begin(), kernel.registers.end(), [](const auto& reg) {
           return reg.kind == ValueKind::GlobalAddress;
         });
}

void add_builtin_for_special(SpecialRegister special, std::uint32_t* flags) noexcept {
  if (flags == nullptr) {
    return;
  }
  switch (special) {
  case SpecialRegister::ThreadIdX:
  case SpecialRegister::ThreadIdY:
    *flags |= kReflectionBuiltinLocalInvocationId;
    break;
  case SpecialRegister::BlockIdX:
  case SpecialRegister::BlockIdY:
    *flags |= kReflectionBuiltinWorkgroupId;
    break;
  case SpecialRegister::BlockDimX:
  case SpecialRegister::BlockDimY:
    *flags |= kReflectionBuiltinWorkgroupSize;
    break;
  case SpecialRegister::GridDimX:
  case SpecialRegister::GridDimY:
    *flags |= kReflectionBuiltinNumWorkgroups;
    break;
  }
}

std::string text_for(const SpirvLoweredModule& module) {
  std::ostringstream output;
  output << "spirv.module @" << module.entry_point << " {\n";
  output << "  spirv.target_env = \"" << module.target_environment << "\"\n";
  output << "  spirv.execution_model = GLCompute\n";
  output << "  spirv.workgroup_size = " << module.workgroup_size[0] << ','
         << module.workgroup_size[1] << ',' << module.workgroup_size[2] << "\n";
  output << "  spirv.argument_abi = 1; size = " << module.reflection.argument_block_size << "\n";
  for (const auto& instruction : module.instructions) {
    output << "  spirv." << spirv_semantic_opcode_string(instruction.opcode) << " result=";
    if (instruction.result == compiler::kNoValue) {
      output << '-';
    } else {
      output << instruction.result;
    }
    output << " inputs=" << instruction.input_count;
    for (std::uint32_t index = 0U; index < instruction.input_count; ++index) {
      output << ',' << instruction.inputs[index];
    }
    if (instruction.opcode == SpirvSemanticOpcode::builtin_local_invocation_id ||
        instruction.opcode == SpirvSemanticOpcode::builtin_workgroup_id ||
        instruction.opcode == SpirvSemanticOpcode::builtin_workgroup_size ||
        instruction.opcode == SpirvSemanticOpcode::builtin_num_workgroups) {
      output << " builtin=" << instruction.attribute;
    }
    if (instruction.predicate != compiler::kNoValue) {
      output << " predicate=" << instruction.predicate
             << (instruction.predicate_negated ? ":negated" : ":positive");
    }
    output << '\n';
  }
  output << "}\n";
  return output.str();
}

} // namespace

LoweringResult lower_kernel(const compiler::Kernel& kernel,
                            const mf_vulkan_capability_profile_v1& profile,
                            const std::array<std::uint32_t, 3>& workgroup_size,
                            SpirvLoweredModule* out_module) {
  if (out_module == nullptr || workgroup_size[0] == 0U || workgroup_size[1] == 0U ||
      workgroup_size[2] == 0U) {
    return {.status = LoweringStatus::invalid_argument,
            .diagnostic = "lowering output and workgroup dimensions are required"};
  }
  const auto diagnostics = compiler::verify_kernel(kernel);
  if (!diagnostics.empty()) {
    return {.status = LoweringStatus::invalid_kernel,
            .diagnostic = first_kernel_diagnostic(diagnostics)};
  }

  SpirvModuleRequirements requirements{};
  std::memcpy(requirements.target_digest.data(), profile.target_digest,
              requirements.target_digest.size());
  requirements.workgroup_size = workgroup_size;
  if (has_global_address(kernel)) {
    requirements.address_space_flags |= kAddressStorageBuffer;
    requirements.required_features |= MF_VULKAN_FEATURE_BUFFER_DEVICE_ADDRESS;
    requirements.requires_buffer_device_address = true;
  }
  if (std::any_of(kernel.parameters.begin(), kernel.parameters.end(), [](const auto& parameter) {
        return parameter.kind != compiler::ParameterKind::BufferU32;
      })) {
    requirements.address_space_flags |= kAddressUniform;
  }
  if (!kernel.shared_allocations.empty()) {
    requirements.address_space_flags |= kAddressWorkgroup;
  }
  if (requirements.address_space_flags == 0U) {
    return {.status = LoweringStatus::unsupported_semantics,
            .diagnostic = "Kernel IR has no representable SPIR-V address space"};
  }
  const auto target_status = validate_spirv_module(profile, requirements);
  if (target_status != TargetStatus::success) {
    return {.status = map_target_status(target_status),
            .diagnostic = std::string("target preflight: ") + target_status_string(target_status)};
  }

  SpirvReflection reflection{};
  reflection.entry_point = kernel.name;
  reflection.execution_model = kSpirvExecutionModelGlCompute;
  reflection.workgroup_size = workgroup_size;
  reflection.required_features = requirements.required_features;
  reflection.address_space_flags = requirements.address_space_flags;
  reflection.builtin_flags = kReflectionBuiltinLocalInvocationId;
  reflection.argument_count = static_cast<std::uint32_t>(kernel.parameters.size());
  std::uint64_t argument_size = 0U;
  if (mf_vulkan_argument_block_size_v1(reflection.argument_count, &argument_size) !=
          MF_VULKAN_ARGUMENT_VALID ||
      argument_size > std::numeric_limits<std::uint32_t>::max()) {
    return {.status = LoweringStatus::resource_exhausted,
            .diagnostic = "packed argument block exceeds the ABI bound"};
  }
  reflection.argument_block_size = static_cast<std::uint32_t>(argument_size);
  std::memcpy(reflection.argument_target_digest.data(), profile.target_digest,
              reflection.argument_target_digest.size());
  reflection.has_workgroup_storage = !kernel.shared_allocations.empty();
  for (const auto& operation : kernel.operations) {
    if (operation.opcode == Opcode::MoveSpecialU32) {
      add_builtin_for_special(static_cast<SpecialRegister>(operation.attribute),
                              &reflection.builtin_flags);
    }
  }
  const auto reflection_status = validate_spirv_reflection(profile, requirements, reflection);
  if (reflection_status != TargetStatus::success) {
    return {.status = map_target_status(reflection_status),
            .diagnostic = std::string("reflection validation: ") +
                          target_status_string(reflection_status)};
  }

  SpirvLoweredModule module{};
  module.entry_point = kernel.name;
  module.target_environment = profile.target_environment;
  module.workgroup_size = workgroup_size;
  module.requirements = requirements;
  module.reflection = reflection;
  try {
    module.instructions.reserve(kernel.operations.size());
    for (const auto& operation : kernel.operations) {
      module.instructions.push_back(SpirvLoweredInstruction{
          .opcode = map_operation(operation),
          .result = operation.result,
          .inputs = operation.inputs,
          .input_count = operation.input_count,
          .attribute = operation.attribute,
          .predicate = operation.predicate,
          .predicate_negated = operation.predicate_negated,
      });
    }
    module.canonical_text = text_for(module);
  } catch (...) {
    return {.status = LoweringStatus::resource_exhausted,
            .diagnostic = "SPIR-V dialect projection allocation failed"};
  }
  *out_module = std::move(module);
  return {.status = LoweringStatus::success, .diagnostic = {}};
}

const char* lowering_status_string(LoweringStatus status) noexcept {
  switch (status) {
  case LoweringStatus::success:
    return "success";
  case LoweringStatus::invalid_argument:
    return "invalid-argument";
  case LoweringStatus::invalid_kernel:
    return "invalid-kernel";
  case LoweringStatus::invalid_target:
    return "invalid-target";
  case LoweringStatus::unsupported_semantics:
    return "unsupported-semantics";
  case LoweringStatus::resource_exhausted:
    return "resource-exhausted";
  }
  return "unknown";
}

const char* spirv_semantic_opcode_string(SpirvSemanticOpcode opcode) noexcept {
  switch (opcode) {
  case SpirvSemanticOpcode::parameter_device_address:
    return "load_parameter_address";
  case SpirvSemanticOpcode::parameter_scalar:
    return "load_parameter_scalar";
  case SpirvSemanticOpcode::builtin_local_invocation_id:
    return "builtin_local_invocation_id";
  case SpirvSemanticOpcode::builtin_workgroup_id:
    return "builtin_workgroup_id";
  case SpirvSemanticOpcode::builtin_workgroup_size:
    return "builtin_workgroup_size";
  case SpirvSemanticOpcode::builtin_num_workgroups:
    return "builtin_num_workgroups";
  case SpirvSemanticOpcode::add_u32:
    return "iadd";
  case SpirvSemanticOpcode::subtract_u32:
    return "isub";
  case SpirvSemanticOpcode::multiply_low_u32:
    return "imul";
  case SpirvSemanticOpcode::multiply_wide_u32:
    return "umul_extended";
  case SpirvSemanticOpcode::multiply_add_low_u32:
    return "mad_u32";
  case SpirvSemanticOpcode::add_address:
    return "access_chain";
  case SpirvSemanticOpcode::add_f32:
    return "fadd";
  case SpirvSemanticOpcode::subtract_f32:
    return "fsub";
  case SpirvSemanticOpcode::multiply_f32:
    return "fmul";
  case SpirvSemanticOpcode::multiply_add_f32:
    return "fma";
  case SpirvSemanticOpcode::fused_multiply_add_f32:
    return "fma_fused";
  case SpirvSemanticOpcode::convert_f32_u32:
    return "convert_f32_u32";
  case SpirvSemanticOpcode::convert_u32_f32:
    return "convert_u32_f32";
  case SpirvSemanticOpcode::compare_u32:
    return "compare_u32";
  case SpirvSemanticOpcode::compare_f32:
    return "compare_f32";
  case SpirvSemanticOpcode::branch_conditional:
    return "branch_conditional";
  case SpirvSemanticOpcode::load_global_u32:
    return "load_global_u32";
  case SpirvSemanticOpcode::store_global_u32:
    return "store_global_u32";
  case SpirvSemanticOpcode::load_global_f32:
    return "load_global_f32";
  case SpirvSemanticOpcode::store_global_f32:
    return "store_global_f32";
  case SpirvSemanticOpcode::load_workgroup_u32:
    return "load_workgroup_u32";
  case SpirvSemanticOpcode::store_workgroup_u32:
    return "store_workgroup_u32";
  case SpirvSemanticOpcode::control_barrier:
    return "control_barrier";
  case SpirvSemanticOpcode::return_value:
    return "return";
  }
  return "unknown";
}

} // namespace metaflux::backend::vulkan
