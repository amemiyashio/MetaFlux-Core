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
             std::uint32_t attribute = 0U, bool flag = false) {
  Operation result_operation{
      .opcode = opcode,
      .result = result,
      .inputs = {},
      .input_count = static_cast<std::uint32_t>(inputs.size()),
      .attribute = attribute,
      .flag = flag,
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

Kernel arithmetic_kernel(Opcode opcode, const char* name) {
  Kernel kernel = add_kernel();
  kernel.name = name;
  kernel.operations[16] = op(opcode, 15U, {13U, 14U});
  return kernel;
}

Kernel arithmetic_mad_kernel() {
  Kernel kernel = add_kernel();
  kernel.name = "mad_lo_u32";
  kernel.operations[16] = op(Opcode::MadLoU32, 15U, {13U, 14U, 3U});
  return kernel;
}

Kernel arithmetic_f32_kernel(Opcode opcode, const char* name) {
  Kernel kernel = arithmetic_kernel(opcode, name);
  kernel.registers[13].kind = ValueKind::F32;
  kernel.registers[14].kind = ValueKind::F32;
  kernel.registers[15].kind = ValueKind::F32;
  kernel.operations[14] = op(Opcode::LoadGlobalF32, 13U, {11U});
  kernel.operations[15] = op(Opcode::LoadGlobalF32, 14U, {12U});
  kernel.operations[16] = opcode == Opcode::MadRnF32 || opcode == Opcode::FmaRnF32
                              ? op(opcode, 15U, {13U, 14U, 13U})
                              : op(opcode, 15U, {13U, 14U});
  kernel.operations[17] = op(Opcode::StoreGlobalF32, metaflux::compiler::kNoValue, {10U, 15U});
  return kernel;
}

Kernel unsupported_f32_kernel() {
  Kernel kernel = arithmetic_f32_kernel(Opcode::MadRnF32, "unsupported_fma_f32");
  kernel.operations[16] = op(Opcode::FmaRnF32, 15U, {13U, 14U, 13U});
  return kernel;
}

Kernel conversion_kernel(Opcode opcode, const char* name) {
  Kernel kernel = add_kernel();
  kernel.name = name;
  const bool f32_from_u32 = opcode == Opcode::ConvertRnF32U32;
  kernel.registers[13].kind = f32_from_u32 ? ValueKind::U32 : ValueKind::F32;
  kernel.registers[14].kind = f32_from_u32 ? ValueKind::U32 : ValueKind::F32;
  kernel.registers[15].kind = f32_from_u32 ? ValueKind::F32 : ValueKind::U32;
  const auto load = f32_from_u32 ? Opcode::LoadGlobalU32 : Opcode::LoadGlobalF32;
  const auto store = f32_from_u32 ? Opcode::StoreGlobalF32 : Opcode::StoreGlobalU32;
  kernel.operations[14] = op(load, 13U, {11U});
  kernel.operations[15] = op(load, 14U, {12U});
  kernel.operations[16] = op(opcode, 15U, {13U});
  kernel.operations[17] = op(store, metaflux::compiler::kNoValue, {10U, 15U});
  return kernel;
}

Kernel predicate_f32_kernel() {
  return Kernel{
      .name = "predicate_lt_f32",
      .parameters = {Parameter{.kind = ParameterKind::BufferU32},
                     Parameter{.kind = ParameterKind::BufferU32},
                     Parameter{.kind = ParameterKind::ScalarF32}},
      .shared_allocations = {},
      .registers = {Register{.kind = ValueKind::GlobalAddress}, Register{.kind = ValueKind::GlobalAddress},
                    Register{.kind = ValueKind::F32}, Register{.kind = ValueKind::U32},
                    Register{.kind = ValueKind::U64}, Register{.kind = ValueKind::GlobalAddress},
                    Register{.kind = ValueKind::GlobalAddress}, Register{.kind = ValueKind::F32},
                    Register{.kind = ValueKind::Predicate}},
      .operations = {op(Opcode::LoadParameterAddress, 0U, {}, 0U),
                     op(Opcode::LoadParameterAddress, 1U, {}, 1U),
                     op(Opcode::LoadParameterF32, 2U, {}, 2U),
                     op(Opcode::MoveSpecialU32, 3U, {},
                        static_cast<std::uint32_t>(SpecialRegister::ThreadIdX)),
                     op(Opcode::MultiplyWideU32, 4U, {3U}, 4U),
                     op(Opcode::AddGlobalAddress, 5U, {1U, 4U}),
                     op(Opcode::AddGlobalAddress, 6U, {0U, 4U}),
                     op(Opcode::LoadGlobalF32, 7U, {5U}),
                     op(Opcode::SetPredicateLtF32, 8U, {7U, 2U}),
                     op(Opcode::BranchIf, metaflux::compiler::kNoValue, {8U}, 11U, true),
                     op(Opcode::StoreGlobalF32, metaflux::compiler::kNoValue, {6U, 7U}),
                     op(Opcode::Return, metaflux::compiler::kNoValue, {})},
  };
}

Kernel shared_barrier_kernel() {
  Kernel kernel{
      .name = "shared_reverse",
      .parameters = {Parameter{.kind = ParameterKind::BufferU32},
                     Parameter{.kind = ParameterKind::ScalarU32},
                     Parameter{.kind = ParameterKind::ScalarU32}},
      .shared_allocations = {metaflux::compiler::SharedAllocation{.words = 4U}},
      .registers = {Register{.kind = ValueKind::GlobalAddress}, Register{.kind = ValueKind::U32},
                    Register{.kind = ValueKind::U32}, Register{.kind = ValueKind::U32},
                    Register{.kind = ValueKind::SharedAddress}, Register{.kind = ValueKind::U32},
                    Register{.kind = ValueKind::U32}, Register{.kind = ValueKind::U32},
                    Register{.kind = ValueKind::SharedAddress}, Register{.kind = ValueKind::U32},
                    Register{.kind = ValueKind::Predicate}, Register{.kind = ValueKind::U32},
                    Register{.kind = ValueKind::U32}, Register{.kind = ValueKind::SharedAddress},
                    Register{.kind = ValueKind::U32}, Register{.kind = ValueKind::U64},
                    Register{.kind = ValueKind::GlobalAddress}},
      .operations = {op(Opcode::LoadParameterAddress, 0U, {}, 0U),
                     op(Opcode::LoadParameterU32, 1U, {}, 1U),
                     op(Opcode::LoadParameterU32, 2U, {}, 2U),
                     op(Opcode::MoveSpecialU32, 3U, {},
                        static_cast<std::uint32_t>(SpecialRegister::ThreadIdX)),
                     op(Opcode::LoadSharedAddress, 4U, {}, 0U),
                     op(Opcode::AddU32, 5U, {2U, 2U}),
                     op(Opcode::AddU32, 6U, {5U, 5U}),
                     op(Opcode::MultiplyLoU32, 7U, {3U, 6U}),
                     op(Opcode::AddSharedAddress, 8U, {4U, 7U}),
                     op(Opcode::AddU32, 9U, {3U, 2U}),
                     op(Opcode::SetPredicateEqU32, 10U, {2U, 2U}),
                     op(Opcode::BarrierSync, metaflux::compiler::kNoValue, {}),
                     op(Opcode::SubU32, 11U, {1U, 3U}),
                     op(Opcode::MultiplyLoU32, 12U, {11U, 6U}),
                     op(Opcode::AddSharedAddress, 13U, {4U, 12U}),
                     op(Opcode::LoadSharedU32, 14U, {13U}),
                     op(Opcode::MultiplyWideU32, 15U, {3U}, 4U),
                     op(Opcode::AddGlobalAddress, 16U, {0U, 15U}),
                     op(Opcode::StoreGlobalU32, metaflux::compiler::kNoValue, {16U, 14U}),
                     op(Opcode::Return, metaflux::compiler::kNoValue, {})},
  };
  kernel.operations.insert(kernel.operations.begin() + 11U,
                           op(Opcode::StoreSharedU32, metaflux::compiler::kNoValue, {8U, 9U}));
  kernel.operations[11].predicate = 10U;
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

bool valid_elementwise_u32_lowering() {
  const auto profile = target();
  const auto validate = [&profile](Opcode opcode, SpirvSemanticOpcode semantic,
                                   const char* mlir_opcode, const char* name) {
    SpirvLoweredModule module{};
    const auto result = metaflux::backend::vulkan::lower_kernel(
        arithmetic_kernel(opcode, name), profile, {8U, 1U, 1U}, &module);
    return result.status == LoweringStatus::success && module.instructions.size() == 19U &&
           module.instructions[16].opcode == semantic &&
           module.mlir_text.find(mlir_opcode) != std::string::npos &&
           module.spirv_binary.size() > 5U && module.spirv_binary[0] == 0x07230203U;
  };
  return validate(Opcode::SubU32, SpirvSemanticOpcode::subtract_u32, "arith.subi",
                  "subtract_u32") &&
         validate(Opcode::MultiplyLoU32, SpirvSemanticOpcode::multiply_low_u32, "arith.muli",
                  "multiply_u32");
}

bool valid_mad_lo_u32_lowering() {
  const auto profile = target();
  SpirvLoweredModule module{};
  const auto result = metaflux::backend::vulkan::lower_kernel(
      arithmetic_mad_kernel(), profile, {8U, 1U, 1U}, &module);
  const bool valid = result.status == LoweringStatus::success && module.instructions.size() == 19U &&
         module.instructions[16].opcode == SpirvSemanticOpcode::multiply_add_low_u32 &&
         module.mlir_text.find("spirv.IMul") != std::string::npos &&
         module.mlir_text.find("spirv.IAdd") != std::string::npos &&
         module.spirv_binary.size() > 5U && module.spirv_binary[0] == 0x07230203U;
  if (!valid) {
    std::cerr << "Vulkan MadLo lowering failure: status="
              << metaflux::backend::vulkan::lowering_status_string(result.status)
              << " diagnostic=" << result.diagnostic << " instructions="
              << module.instructions.size() << " mlir-bytes=" << module.mlir_text.size()
              << " spirv-words=" << module.spirv_binary.size() << '\n';
    if (!module.mlir_text.empty()) {
      std::cerr << module.mlir_text << '\n';
    }
  }
  return valid;
}

bool valid_elementwise_f32_lowering() {
  const auto profile = target();
  const auto validate = [&profile](Opcode opcode, SpirvSemanticOpcode semantic,
                                   const char* mlir_opcode, const char* name) {
    SpirvLoweredModule module{};
    const auto result = metaflux::backend::vulkan::lower_kernel(
        arithmetic_f32_kernel(opcode, name), profile, {8U, 1U, 1U}, &module);
    const bool valid = result.status == LoweringStatus::success && module.instructions.size() == 19U &&
           module.instructions[14].opcode == SpirvSemanticOpcode::load_global_f32 &&
           module.instructions[16].opcode == semantic &&
           module.instructions[17].opcode == SpirvSemanticOpcode::store_global_f32 &&
           module.mlir_text.find("memref<?xf32") != std::string::npos &&
           module.mlir_text.find(mlir_opcode) != std::string::npos &&
           module.spirv_binary.size() > 5U && module.spirv_binary[0] == 0x07230203U;
    if (!valid) {
      std::cerr << "Vulkan F32 lowering failure: " << name
                << " status=" << metaflux::backend::vulkan::lowering_status_string(result.status)
                << " diagnostic=" << result.diagnostic << " instructions="
                << module.instructions.size() << " mlir-bytes=" << module.mlir_text.size()
                << " spirv-words=" << module.spirv_binary.size() << '\n';
      if (!module.mlir_text.empty()) {
        std::cerr << module.mlir_text << '\n';
      }
    }
    return valid;
  };
  return validate(Opcode::AddRnF32, SpirvSemanticOpcode::add_f32, "arith.addf", "add_f32") &&
         validate(Opcode::SubRnF32, SpirvSemanticOpcode::subtract_f32, "arith.subf",
                  "subtract_f32") &&
         validate(Opcode::MultiplyRnF32, SpirvSemanticOpcode::multiply_f32, "arith.mulf",
                  "multiply_f32") &&
         validate(Opcode::MadRnF32, SpirvSemanticOpcode::multiply_add_f32, "spirv.FAdd",
                  "mad_f32");
}

bool valid_elementwise_conversion_lowering() {
  const auto profile = target();
  const auto validate = [&profile](Opcode opcode, SpirvSemanticOpcode semantic,
                                   const char* source_type, const char* result_type,
                                   const char* mlir_opcode, const char* name) {
    SpirvLoweredModule module{};
    const auto result = metaflux::backend::vulkan::lower_kernel(
        conversion_kernel(opcode, name), profile, {8U, 1U, 1U}, &module);
    const bool valid = result.status == LoweringStatus::success && module.instructions.size() == 19U &&
           module.instructions[16].opcode == semantic &&
           module.mlir_text.find(std::string("memref<?x") + source_type) != std::string::npos &&
           module.mlir_text.find(std::string("memref<?x") + result_type) != std::string::npos &&
           module.mlir_text.find(mlir_opcode) != std::string::npos &&
           module.spirv_binary.size() > 5U && module.spirv_binary[0] == 0x07230203U;
    if (!valid) {
      std::cerr << "Vulkan conversion lowering failure: " << name
                << " status=" << metaflux::backend::vulkan::lowering_status_string(result.status)
                << " diagnostic=" << result.diagnostic << " instructions="
                << module.instructions.size() << " mlir-bytes=" << module.mlir_text.size()
                << " spirv-words=" << module.spirv_binary.size() << '\n';
      if (!module.mlir_text.empty()) {
        std::cerr << module.mlir_text << '\n';
      }
    }
    return valid;
  };
  return validate(Opcode::ConvertRnF32U32, SpirvSemanticOpcode::convert_f32_u32, "i32", "f32",
                  "arith.uitofp", "convert_f32_u32") &&
         validate(Opcode::ConvertRziU32F32, SpirvSemanticOpcode::convert_u32_f32, "f32", "i32",
                  "arith.fptoui", "convert_u32_f32");
}

bool valid_f32_predicate_lowering() {
  SpirvLoweredModule module{};
  const auto result = metaflux::backend::vulkan::lower_kernel(
      predicate_f32_kernel(), target(), {8U, 1U, 1U}, &module);
  const bool valid = result.status == LoweringStatus::success && module.instructions.size() == 12U &&
         module.instructions[8].opcode == SpirvSemanticOpcode::compare_f32 &&
         module.instructions[9].opcode == SpirvSemanticOpcode::branch_conditional &&
         module.mlir_text.find("arith.cmpf") != std::string::npos &&
         module.mlir_text.find("scf.if") != std::string::npos &&
         module.mlir_text.find("memref.store") != std::string::npos &&
         module.spirv_binary.size() > 5U && module.spirv_binary[0] == 0x07230203U;
  if (!valid) {
    std::cerr << "Vulkan predicate lowering failure: status="
              << metaflux::backend::vulkan::lowering_status_string(result.status)
              << " diagnostic=" << result.diagnostic << " instructions="
              << module.instructions.size() << " mlir-bytes=" << module.mlir_text.size()
              << " spirv-words=" << module.spirv_binary.size() << '\n';
    if (!module.mlir_text.empty()) {
      std::cerr << module.mlir_text << '\n';
    }
  }
  return valid;
}

bool valid_shared_barrier_lowering() {
  const auto profile = target();
  SpirvLoweredModule module{};
  const auto result = metaflux::backend::vulkan::lower_kernel(
      shared_barrier_kernel(), profile, {4U, 1U, 1U}, &module);
  const bool valid = result.status == LoweringStatus::success &&
         module.entry_point == "shared_reverse" && module.instructions.size() == 21U &&
         module.reflection.argument_count == 3U && module.reflection.has_workgroup_storage &&
         module.canonical_text.find("spirv.control_barrier") != std::string::npos &&
         module.canonical_text.find("spirv.load_workgroup_u32") != std::string::npos &&
         module.canonical_text.find("spirv.store_workgroup_u32") != std::string::npos &&
         module.mlir_text.find("%arg1") != std::string::npos &&
         module.mlir_text.find("%arg2") != std::string::npos &&
         module.mlir_text.find("arith.subi") != std::string::npos &&
         module.mlir_text.find("arith.muli") != std::string::npos &&
         module.mlir_text.find("spirv.ControlBarrier") != std::string::npos &&
         module.mlir_text.find("Workgroup") != std::string::npos &&
         module.spirv_binary.size() > 5U && module.spirv_binary[0] == 0x07230203U &&
         module.spirv_binary[1] != 0U;
  if (!valid) {
    std::cerr << "Vulkan shared barrier lowering failure: status="
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
      std::cerr << "SPIR-V validation setup failed for " << kernel.name
                << ": status="
                << metaflux::backend::vulkan::lowering_status_string(result.status)
                << " diagnostic=" << result.diagnostic << " words="
                << module.spirv_binary.size() << '\n';
      return false;
    }

    char path[] = "/tmp/metaflux-vulkan-lowering-XXXXXX";
    const int fd = ::mkstemp(path);
    if (fd < 0) {
      std::cerr << "SPIR-V validation temporary file creation failed for " << kernel.name << '\n';
      return false;
    }
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(module.spirv_binary.data());
    std::size_t remaining = module.spirv_binary.size() * sizeof(std::uint32_t);
    while (remaining != 0U) {
      const ssize_t written = ::write(fd, bytes, remaining);
      if (written <= 0) {
        std::cerr << "SPIR-V validation temporary file write failed for " << kernel.name << '\n';
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
      std::cerr << "SPIR-V validation fork failed for " << kernel.name << '\n';
      (void)::unlink(path);
      return false;
    }
    int status = 0;
    const bool waited = ::waitpid(child, &status, 0) == child;
    (void)::unlink(path);
    const bool valid = waited && WIFEXITED(status) && WEXITSTATUS(status) == 0;
    if (!valid) {
      std::cerr << "SPIR-V validation failed for " << kernel.name << "; converted MLIR:\n"
                << module.mlir_text << "exit-status=" << status << '\n';
    }
    return valid;
  };
  return validate(add_kernel()) && validate(copy_kernel()) && validate(shared_barrier_kernel()) &&
         validate(arithmetic_kernel(Opcode::SubU32, "subtract_u32")) &&
         validate(arithmetic_kernel(Opcode::MultiplyLoU32, "multiply_u32")) &&
         validate(arithmetic_mad_kernel()) &&
         validate(arithmetic_f32_kernel(Opcode::AddRnF32, "add_f32")) &&
         validate(arithmetic_f32_kernel(Opcode::SubRnF32, "subtract_f32")) &&
         validate(arithmetic_f32_kernel(Opcode::MultiplyRnF32, "multiply_f32")) &&
         validate(arithmetic_f32_kernel(Opcode::MadRnF32, "mad_f32")) &&
         validate(conversion_kernel(Opcode::ConvertRnF32U32, "convert_f32_u32")) &&
         validate(conversion_kernel(Opcode::ConvertRziU32F32, "convert_u32_f32")) &&
         validate(predicate_f32_kernel());
#else
  std::cout << "Vulkan lowering: spirv-val unavailable; independent validation skipped\n";
  return true;
#endif
}

bool unsupported_semantics_fail_before_emission() {
  auto kernel = unsupported_f32_kernel();
  SpirvLoweredModule module{};
  const auto result = metaflux::backend::vulkan::lower_kernel(
      kernel, target(), {8U, 1U, 1U}, &module);
  const bool valid = result.status == LoweringStatus::unsupported_semantics &&
         result.diagnostic.find("verified u32 Add/Sub/Multiply/MadLo, f32 Add/Sub/Multiply/Mad, u32<->f32 conversions, f32 predicates, and Copy forms") != std::string::npos &&
         module.spirv_binary.empty() && module.mlir_text.empty();
  if (!valid) {
    std::cerr << "Vulkan unsupported semantics failure: status="
              << metaflux::backend::vulkan::lowering_status_string(result.status)
              << " diagnostic=" << result.diagnostic << " instructions="
              << module.instructions.size() << " mlir-bytes=" << module.mlir_text.size()
              << " spirv-words=" << module.spirv_binary.size() << '\n';
  }
  return valid;
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
  if (!valid_add_lowering()) {
    std::cerr << "Vulkan lowering stage failed: add\n";
    return 1;
  }
  if (!valid_copy_lowering()) {
    std::cerr << "Vulkan lowering stage failed: copy\n";
    return 1;
  }
  if (!valid_shared_barrier_lowering()) {
    std::cerr << "Vulkan lowering stage failed: shared-barrier\n";
    return 1;
  }
  if (!valid_elementwise_u32_lowering()) {
    std::cerr << "Vulkan lowering stage failed: u32-arithmetic\n";
    return 1;
  }
  if (!valid_mad_lo_u32_lowering()) {
    std::cerr << "Vulkan lowering stage failed: u32-mad\n";
    return 1;
  }
  if (!valid_elementwise_f32_lowering()) {
    std::cerr << "Vulkan lowering stage failed: f32-arithmetic\n";
    return 1;
  }
  if (!valid_elementwise_conversion_lowering()) {
    std::cerr << "Vulkan lowering stage failed: conversions\n";
    return 1;
  }
  if (!valid_f32_predicate_lowering()) {
    std::cerr << "Vulkan lowering stage failed: f32-predicate\n";
    return 1;
  }
  if (!independently_validates_spirv()) {
    std::cerr << "Vulkan lowering stage failed: independent-spirv-validation\n";
    return 1;
  }
  if (!unsupported_semantics_fail_before_emission()) {
    std::cerr << "Vulkan lowering stage failed: unsupported-semantics\n";
    return 1;
  }
  if (!invalid_inputs()) {
    std::cerr << "Vulkan lowering stage failed: invalid-inputs\n";
    return 1;
  }
  return 0;
}
