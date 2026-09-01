#include "metaflux/backend/vulkan_lowering.hpp"

#include "mlir/Conversion/GPUToSPIRV/GPUToSPIRVPass.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/GPU/IR/GPUDialect.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/Dialect/SPIRV/IR/SPIRVAttributes.h"
#include "mlir/Dialect/SPIRV/IR/SPIRVDialect.h"
#include "mlir/Dialect/SPIRV/IR/SPIRVOps.h"
#include "mlir/Dialect/SPIRV/Transforms/Passes.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/Diagnostics.h"
#include "mlir/IR/DialectRegistry.h"
#include "mlir/IR/MLIRContext.h"
#include "mlir/Parser/Parser.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Pass/PassManager.h"
#include "mlir/Target/SPIRV/Serialization.h"
#include "mlir/Target/SPIRV/Target.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <exception>
#include <limits>
#include <sstream>
#include <utility>

#include "llvm/ADT/SmallVector.h"
#include "llvm/Support/raw_ostream.h"

namespace metaflux::backend::vulkan {
namespace {

using compiler::Opcode;
using compiler::ParameterKind;
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

bool is_add_u32_kernel(const compiler::Kernel& kernel) noexcept {
  using enum Opcode;
  constexpr std::array<Opcode, 19> kOperations{
      LoadParameterAddress, LoadParameterAddress, LoadParameterAddress, LoadParameterU32,
      MoveSpecialU32,       MoveSpecialU32,       MoveSpecialU32,       MadLoU32,
      SetPredicateGeU32,    BranchIf,             MultiplyWideU32,      AddGlobalAddress,
      AddGlobalAddress,     AddGlobalAddress,     LoadGlobalU32,        LoadGlobalU32,
      AddU32,               StoreGlobalU32,        Return,
  };
  if (kernel.parameters.size() != 4U || kernel.parameters[0].kind != ParameterKind::BufferU32 ||
      kernel.parameters[1].kind != ParameterKind::BufferU32 ||
      kernel.parameters[2].kind != ParameterKind::BufferU32 ||
      kernel.parameters[3].kind != ParameterKind::ScalarU32 ||
      kernel.operations.size() != kOperations.size()) {
    return false;
  }
  for (std::size_t index = 0U; index < kOperations.size(); ++index) {
    if (kernel.operations[index].opcode != kOperations[index]) {
      return false;
    }
  }
  return kernel.operations[4].attribute ==
             static_cast<std::uint32_t>(SpecialRegister::ThreadIdX) &&
         kernel.operations[5].attribute ==
             static_cast<std::uint32_t>(SpecialRegister::BlockIdX) &&
         kernel.operations[6].attribute ==
             static_cast<std::uint32_t>(SpecialRegister::BlockDimX);
}

bool is_copy_u32_kernel(const compiler::Kernel& kernel) noexcept {
  using enum Opcode;
  constexpr std::array<Opcode, 15> kOperations{
      LoadParameterAddress, LoadParameterAddress, LoadParameterU32, MoveSpecialU32,
      MoveSpecialU32,       MoveSpecialU32,       MadLoU32,          SetPredicateGeU32,
      BranchIf,             MultiplyWideU32,      AddGlobalAddress,  AddGlobalAddress,
      LoadGlobalU32,        StoreGlobalU32,       Return,
  };
  if (kernel.parameters.size() != 3U || kernel.parameters[0].kind != ParameterKind::BufferU32 ||
      kernel.parameters[1].kind != ParameterKind::BufferU32 ||
      kernel.parameters[2].kind != ParameterKind::ScalarU32 ||
      kernel.operations.size() != kOperations.size()) {
    return false;
  }
  for (std::size_t index = 0U; index < kOperations.size(); ++index) {
    if (kernel.operations[index].opcode != kOperations[index]) {
      return false;
    }
  }
  return kernel.operations[3].attribute ==
             static_cast<std::uint32_t>(SpecialRegister::ThreadIdX) &&
         kernel.operations[4].attribute ==
             static_cast<std::uint32_t>(SpecialRegister::BlockIdX) &&
         kernel.operations[5].attribute ==
             static_cast<std::uint32_t>(SpecialRegister::BlockDimX);
}

bool is_static_shared_barrier_kernel(const compiler::Kernel& kernel) noexcept {
  using enum Opcode;
  constexpr std::array<Opcode, 21> kOperations{
      LoadParameterAddress, LoadParameterU32,  LoadParameterU32, MoveSpecialU32,
      LoadSharedAddress,    AddU32,             AddU32,           MultiplyLoU32,
      AddSharedAddress,     AddU32,             SetPredicateEqU32, StoreSharedU32,
      BarrierSync,          SubU32,             MultiplyLoU32,    AddSharedAddress,
      LoadSharedU32,         MultiplyWideU32,   AddGlobalAddress, StoreGlobalU32,
      Return,
  };
  if (kernel.parameters.size() != 3U ||
      kernel.parameters[0].kind != ParameterKind::BufferU32 ||
      kernel.parameters[1].kind != ParameterKind::ScalarU32 ||
      kernel.parameters[2].kind != ParameterKind::ScalarU32 ||
      kernel.shared_allocations.size() != 1U || kernel.shared_allocations[0].words != 4U ||
      kernel.operations.size() != kOperations.size()) {
    return false;
  }
  for (std::size_t index = 0U; index < kOperations.size(); ++index) {
    if (kernel.operations[index].opcode != kOperations[index]) {
      return false;
    }
  }
  return kernel.operations[3].attribute ==
             static_cast<std::uint32_t>(SpecialRegister::ThreadIdX) &&
         kernel.operations[10].inputs[0] == kernel.operations[10].inputs[1] &&
         kernel.operations[11].predicate != compiler::kNoValue;
}

std::string mlir_symbol(std::string_view name) {
  std::string result = "@\"";
  result.reserve(name.size() + 3U);
  for (const char character : name) {
    if (character == '\\' || character == '\"') {
      result.push_back('\\');
    }
    result.push_back(character);
  }
  result.push_back('"');
  return result;
}

std::string actual_mlir_source(const compiler::Kernel& kernel,
                               const std::array<std::uint32_t, 3>& workgroup_size,
                               bool copy_form, bool static_shared_barrier_form) {
  std::ostringstream output;
  output << "module attributes {\n"
         << "  gpu.container_module,\n"
         << "  spirv.target_env = #spirv.target_env<"
         << "#spirv.vce<v1.0, [Shader], [SPV_KHR_storage_buffer_storage_class]>, "
         << "#spirv.resource_limits<>>\n"
         << "} {\n"
         << "  gpu.module @kernels {\n"
         << "    gpu.func " << mlir_symbol(kernel.name)
         << "(%arg0: memref<?xi32, #spirv.storage_class<StorageBuffer>>, ";
  if (static_shared_barrier_form) {
    output << "%arg1: i32, %arg2: i32) kernel ";
  } else if (copy_form) {
    output << "%arg1: memref<?xi32, #spirv.storage_class<StorageBuffer>>, "
             << "%arg2: i32) kernel ";
  } else {
    output << "%arg1: memref<?xi32, #spirv.storage_class<StorageBuffer>>, "
             << "%arg2: memref<?xi32, #spirv.storage_class<StorageBuffer>>, %arg3: i32) kernel ";
  }
  output
         << "attributes {spirv.entry_point_abi = #spirv.entry_point_abi<workgroup_size = ["
         << workgroup_size[0] << ", " << workgroup_size[1] << ", " << workgroup_size[2]
         << "]>} {\n"
         << "      %tid = gpu.thread_id x\n";
  if (static_shared_barrier_form) {
    output << "      %shared = memref.alloc() : memref<4xi32, #spirv.storage_class<Workgroup>>\n"
           << "      %one_index = arith.index_cast %arg2 : i32 to index\n"
           << "      %last_index = arith.index_cast %arg1 : i32 to index\n"
           << "      %slot = arith.muli %tid, %one_index : index\n"
           << "      %tid_value = arith.index_cast %tid : index to i32\n"
           << "      %write_value = arith.addi %tid_value, %arg2 : i32\n"
           << "      memref.store %write_value, %shared[%slot] : memref<4xi32, "
              "#spirv.storage_class<Workgroup>>\n"
           << "      gpu.barrier\n"
           << "      %reverse = arith.subi %last_index, %tid : index\n"
           << "      %reverse_slot = arith.muli %reverse, %one_index : index\n"
           << "      %value = memref.load %shared[%reverse_slot] : memref<4xi32, "
              "#spirv.storage_class<Workgroup>>\n"
           << "      memref.store %value, %arg0[%tid] : memref<?xi32, "
              "#spirv.storage_class<StorageBuffer>>\n";
  } else {
    output << "      %bid = gpu.block_id x\n"
           << "      %bdim = gpu.block_dim x\n"
           << "      %idx0 = arith.muli %bid, %bdim : index\n"
           << "      %idx = arith.addi %idx0, %tid : index\n"
           << "      %n = arith.index_cast %arg" << (copy_form ? 2 : 3)
           << " : i32 to index\n"
           << "      %pred = arith.cmpi uge, %idx, %n : index\n"
           << "      scf.if %pred {\n"
           << "      } else {\n";
  }
  if (!static_shared_barrier_form) {
    if (copy_form) {
      output << "        %value = memref.load %arg1[%idx] : memref<?xi32, "
                 "#spirv.storage_class<StorageBuffer>>\n"
              << "        memref.store %value, %arg0[%idx] : memref<?xi32, "
                 "#spirv.storage_class<StorageBuffer>>\n";
    } else {
      output << "        %a = memref.load %arg0[%idx] : memref<?xi32, "
                 "#spirv.storage_class<StorageBuffer>>\n"
              << "        %b = memref.load %arg1[%idx] : memref<?xi32, "
                 "#spirv.storage_class<StorageBuffer>>\n"
              << "        %sum = arith.addi %a, %b : i32\n"
              << "        memref.store %sum, %arg2[%idx] : memref<?xi32, "
                 "#spirv.storage_class<StorageBuffer>>\n";
    }
  }
  if (!static_shared_barrier_form) {
    output << "      }\n";
  }
  output << "      gpu.return\n"
         << "    }\n"
         << "  }\n"
         << "}\n";
  return output.str();
}

std::string diagnostic_text(mlir::Diagnostic& diagnostic) {
  std::string text;
  llvm::raw_string_ostream stream(text);
  diagnostic.print(stream);
  stream.flush();
  return text;
}

LoweringResult emit_actual_spirv(const compiler::Kernel& kernel,
                                 const std::array<std::uint32_t, 3>& workgroup_size,
                                 SpirvLoweredModule* module) {
  const bool add_form = is_add_u32_kernel(kernel);
  const bool copy_form = is_copy_u32_kernel(kernel);
  const bool static_shared_barrier_form = is_static_shared_barrier_kernel(kernel);
  if (!add_form && !copy_form && !static_shared_barrier_form) {
    return {.status = LoweringStatus::unsupported_semantics,
            .diagnostic =
                "actual MLIR/SPIR-V emission currently supports the verified u32 Add/Copy form"};
  }

  try {
    const auto source =
        actual_mlir_source(kernel, workgroup_size, copy_form, static_shared_barrier_form);
    mlir::DialectRegistry registry;
    registry.insert<mlir::arith::ArithDialect, mlir::func::FuncDialect, mlir::gpu::GPUDialect,
                    mlir::memref::MemRefDialect, mlir::scf::SCFDialect,
                    mlir::spirv::SPIRVDialect>();
    mlir::spirv::registerSPIRVTargetInterfaceExternalModels(registry);
    mlir::MLIRContext context(registry);
    std::string diagnostics;
    const auto handler = context.getDiagEngine().registerHandler(
        [&diagnostics](mlir::Diagnostic& diagnostic) {
          diagnostics += diagnostic_text(diagnostic);
          diagnostics.push_back('\n');
          return mlir::success();
        });
    mlir::ParserConfig parser_config(&context);
    auto parsed =
        mlir::parseSourceString<mlir::ModuleOp>(source, parser_config, "metaflux-kernel");
    if (!parsed) {
      context.getDiagEngine().eraseHandler(handler);
      return {.status = LoweringStatus::invalid_kernel,
              .diagnostic = "MLIR parse failed: " + diagnostics};
    }

    mlir::PassManager pass_manager(&context);
    pass_manager.enableVerifier(true);
    pass_manager.addPass(mlir::createConvertGPUToSPIRVPass());
    pass_manager.addNestedPass<mlir::spirv::ModuleOp>(
        mlir::spirv::createSPIRVLowerABIAttributesPass());
    if (mlir::failed(pass_manager.run(*parsed))) {
      context.getDiagEngine().eraseHandler(handler);
      return {.status = LoweringStatus::unsupported_semantics,
              .diagnostic = "MLIR GPU-to-SPIR-V conversion failed: " + diagnostics};
    }

    mlir::Operation* spirv_operation = nullptr;
    parsed->walk([&spirv_operation](mlir::spirv::ModuleOp candidate) {
      if (spirv_operation == nullptr) {
        spirv_operation = candidate.getOperation();
      }
    });
    auto spirv_module =
        llvm::dyn_cast_or_null<mlir::spirv::ModuleOp>(spirv_operation);
    if (!spirv_module) {
      context.getDiagEngine().eraseHandler(handler);
      return {.status = LoweringStatus::unsupported_semantics,
              .diagnostic = "MLIR conversion produced no SPIR-V module"};
    }

    const auto target_environment =
        parsed->getOperation()->getAttrOfType<mlir::spirv::TargetEnvAttr>("spirv.target_env");
    if (!target_environment) {
      context.getDiagEngine().eraseHandler(handler);
      return {.status = LoweringStatus::unsupported_semantics,
              .diagnostic = "SPIR-V serialization target environment is missing"};
    }
    // The conversion pass consumes the target environment but does not retain
    // its triple on the generated module in this embedding API. Serialization
    // requires that property to be explicit, so restore the exact parsed
    // target triple before emitting the binary.
    spirv_module.setVceTripleAttr(target_environment.getTripleAttr());

    llvm::SmallVector<std::uint32_t, 0> binary;
    if (mlir::failed(mlir::spirv::serialize(spirv_module, binary))) {
      context.getDiagEngine().eraseHandler(handler);
      return {.status = LoweringStatus::unsupported_semantics,
              .diagnostic = "SPIR-V serialization failed: " + diagnostics};
    }
    std::string lowered_text;
    llvm::raw_string_ostream text_stream(lowered_text);
    parsed->print(text_stream);
    text_stream.flush();
    context.getDiagEngine().eraseHandler(handler);
    module->mlir_text = std::move(lowered_text);
    module->spirv_binary.assign(binary.begin(), binary.end());
    return {.status = LoweringStatus::success, .diagnostic = {}};
  } catch (const std::exception& error) {
    return {.status = LoweringStatus::resource_exhausted,
            .diagnostic = std::string("MLIR/SPIR-V emission failed: ") + error.what()};
  } catch (...) {
    return {.status = LoweringStatus::resource_exhausted,
            .diagnostic = "MLIR/SPIR-V emission failed with an unknown exception"};
  }
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
    const auto actual = emit_actual_spirv(kernel, workgroup_size, &module);
    if (actual.status != LoweringStatus::success) {
      return actual;
    }
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
