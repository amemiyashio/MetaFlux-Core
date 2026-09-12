#include "metaflux/compiler/kernel_ir.hpp"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <sstream>
#include <utility>

namespace metaflux::compiler {
namespace {

constexpr std::size_t kMaximumParameters = 64;
constexpr std::size_t kMaximumSharedAllocations = 64;
constexpr std::uint64_t kMaximumSharedWords = 49152U / sizeof(std::uint32_t);
constexpr std::size_t kMaximumRegisters = 4096;
constexpr std::size_t kMaximumOperations = 65536;

void append_diagnostic(std::vector<Diagnostic>& diagnostics, DiagnosticCode code,
                       SourceLocation location, std::string message, std::string form = {}) {
  diagnostics.push_back(Diagnostic{
      .code = code,
      .location = location,
      .message = std::move(message),
      .form = std::move(form),
  });
}

bool valid_kernel_name(std::string_view name) {
  if (name.empty()) {
    return false;
  }
  const auto first = static_cast<unsigned char>(name.front());
  if (std::isalpha(first) == 0 && name.front() != '_' && name.front() != '$') {
    return false;
  }
  return std::all_of(name.begin() + 1, name.end(), [](char character) {
    const auto value = static_cast<unsigned char>(character);
    return std::isalnum(value) != 0 || character == '_' || character == '$';
  });
}

struct OperationContract {
  bool has_result;
  ValueKind result_kind;
  std::array<ValueKind, 3> input_kinds;
  std::uint32_t input_count;
};

OperationContract operation_contract(Opcode opcode) {
  using enum Opcode;
  using enum ValueKind;
  switch (opcode) {
  case LoadParameterAddress:
    return {true, GlobalAddress, {}, 0};
  case LoadParameterU32:
    return {true, U32, {}, 0};
  case LoadParameterF32:
    return {true, F32, {}, 0};
  case LoadSharedAddress:
    return {true, SharedAddress, {}, 0};
  case MoveSpecialU32:
    return {true, U32, {}, 0};
  case MoveImmediateU32:
    return {true, U32, {}, 0};
  case AbsS32:
    return {true, U32, {U32, U32, U32}, 1};
  case AbsF32:
  case SqrtRnF32:
    return {true, F32, {F32, U32, U32}, 1};
  case AddU32:
  case SubU32:
  case MultiplyLoU32:
  case MultiplyHiU32:
    return {true, U32, {U32, U32, U32}, 2};
  case MadLoU32:
    return {true, U32, {U32, U32, U32}, 3};
  case MultiplyWideU32:
    return {true, U64, {U32, U32, U32}, 1};
  case AddGlobalAddress:
    return {true, GlobalAddress, {GlobalAddress, U64, U32}, 2};
  case AddSharedAddress:
    return {true, SharedAddress, {SharedAddress, U32, U32}, 2};
  case AddRnF32:
  case SubRnF32:
  case DivRnF32:
  case MultiplyRnF32:
    return {true, F32, {F32, F32, U32}, 2};
  case MadRnF32:
  case FmaRnF32:
    return {true, F32, {F32, F32, F32}, 3};
  case ConvertRnF32U32:
  case ConvertRnF32S32:
    return {true, F32, {U32, U32, U32}, 1};
  case ExpF32:
    return {true, F32, {F32, U32, U32}, 1};
  case ConvertRziU32F32:
    return {true, U32, {F32, U32, U32}, 1};
  case SetPredicateGeU32:
  case SetPredicateEqU32:
    return {true, Predicate, {U32, U32, U32}, 2};
  case SetPredicateLtF32:
    return {true, Predicate, {F32, F32, U32}, 2};
  case SetPredicateGtS32:
    return {true, Predicate, {U32, U32, U32}, 2};
  case SelectU32:
    return {true, U32, {U32, U32, Predicate}, 3};
  case SelectF32:
    return {true, F32, {F32, F32, Predicate}, 3};
  case BranchIf:
    return {false, U32, {Predicate, U32, U32}, 1};
  case LoadGlobalU32:
    return {true, U32, {GlobalAddress, U32, U32}, 1};
  case StoreGlobalU32:
    return {false, U32, {GlobalAddress, U32, U32}, 2};
  case LoadGlobalF32:
    return {true, F32, {GlobalAddress, U32, U32}, 1};
  case StoreGlobalF32:
    return {false, U32, {GlobalAddress, F32, U32}, 2};
  case StoreGlobalU8:
    return {false, U32, {GlobalAddress, U32, U32}, 2};
  case StoreGlobalU64:
    return {false, U32, {GlobalAddress, U64, U32}, 2};
  case LoadSharedU32:
    return {true, U32, {SharedAddress, U32, U32}, 1};
  case StoreSharedU32:
    return {false, U32, {SharedAddress, U32, U32}, 2};
  case BarrierSync:
  case Return:
    return {false, U32, {}, 0};
  }
  return {false, U32, {}, 0};
}

bool valid_parameter_kind(ParameterKind kind) {
  return kind == ParameterKind::BufferU32 || kind == ParameterKind::ScalarU32 ||
         kind == ParameterKind::ScalarF32;
}

bool valid_value_kind(ValueKind kind) {
  return kind == ValueKind::Predicate || kind == ValueKind::U32 || kind == ValueKind::U64 ||
         kind == ValueKind::F32 || kind == ValueKind::GlobalAddress ||
         kind == ValueKind::SharedAddress;
}

bool may_be_predicated(Opcode opcode) {
  return opcode == Opcode::StoreGlobalU32 || opcode == Opcode::StoreGlobalU64 ||
         opcode == Opcode::StoreSharedU32;
}

} // namespace

bool is_valid_opcode(Opcode opcode) noexcept {
  return static_cast<std::uint32_t>(opcode) <= static_cast<std::uint32_t>(Opcode::ExpF32);
}

std::string_view diagnostic_code_name(DiagnosticCode code) noexcept {
  using enum DiagnosticCode;
  switch (code) {
  case PtxSyntax:
    return "MF_PTX_SYNTAX";
  case PtxUnsupportedVersion:
    return "MF_PTX_UNSUPPORTED_VERSION";
  case PtxUnsupportedTarget:
    return "MF_PTX_UNSUPPORTED_TARGET";
  case PtxUnsupportedInstruction:
    return "MF_PTX_UNSUPPORTED_INSTRUCTION";
  case PtxDuplicateSymbol:
    return "MF_PTX_DUPLICATE_SYMBOL";
  case PtxUnknownSymbol:
    return "MF_PTX_UNKNOWN_SYMBOL";
  case PtxTypeMismatch:
    return "MF_PTX_TYPE_MISMATCH";
  case KernelIrSchemaMismatch:
    return "MF_KIR_SCHEMA_MISMATCH";
  case KernelIrInvalidKernel:
    return "MF_KIR_INVALID_KERNEL";
  case KernelIrInvalidParameter:
    return "MF_KIR_INVALID_PARAMETER";
  case KernelIrInvalidRegister:
    return "MF_KIR_INVALID_REGISTER";
  case KernelIrInvalidOperation:
    return "MF_KIR_INVALID_OPERATION";
  case KernelIrOperandCount:
    return "MF_KIR_OPERAND_COUNT";
  case KernelIrIndexOutOfRange:
    return "MF_KIR_INDEX_OUT_OF_RANGE";
  case KernelIrTypeMismatch:
    return "MF_KIR_TYPE_MISMATCH";
  case KernelIrUseBeforeDefinition:
    return "MF_KIR_USE_BEFORE_DEFINITION";
  case KernelIrDuplicateDefinition:
    return "MF_KIR_DUPLICATE_DEFINITION";
  case KernelIrInvalidControlFlow:
    return "MF_KIR_INVALID_CONTROL_FLOW";
  case KernelIrMissingReturn:
    return "MF_KIR_MISSING_RETURN";
  }
  return "MF_UNKNOWN_DIAGNOSTIC";
}

std::string_view parameter_kind_name(ParameterKind kind) noexcept {
  switch (kind) {
  case ParameterKind::BufferU32:
    return "buffer_u32";
  case ParameterKind::ScalarU32:
    return "scalar_u32";
  case ParameterKind::ScalarF32:
    return "scalar_f32";
  }
  return "invalid";
}

std::string_view value_kind_name(ValueKind kind) noexcept {
  switch (kind) {
  case ValueKind::Predicate:
    return "predicate";
  case ValueKind::U32:
    return "u32";
  case ValueKind::U64:
    return "u64";
  case ValueKind::F32:
    return "f32";
  case ValueKind::GlobalAddress:
    return "global_address";
  case ValueKind::SharedAddress:
    return "shared_address";
  }
  return "invalid";
}

std::string_view opcode_name(Opcode opcode) noexcept {
  using enum Opcode;
  switch (opcode) {
  case LoadParameterAddress:
    return "load_parameter_address";
  case LoadParameterU32:
    return "load_parameter_u32";
  case LoadParameterF32:
    return "load_parameter_f32";
  case LoadSharedAddress:
    return "load_shared_address";
  case MoveSpecialU32:
    return "move_special_u32";
  case MoveImmediateU32:
    return "move_immediate_u32";
  case AbsS32:
    return "abs_s32";
  case AbsF32:
    return "abs_f32";
  case SqrtRnF32:
    return "sqrt_rn_f32";
  case AddU32:
    return "add_u32";
  case SubU32:
    return "sub_u32";
  case MultiplyLoU32:
    return "multiply_lo_u32";
  case MultiplyHiU32:
    return "multiply_hi_u32";
  case MadLoU32:
    return "mad_lo_u32";
  case MultiplyWideU32:
    return "multiply_wide_u32";
  case AddGlobalAddress:
    return "add_global_address";
  case AddSharedAddress:
    return "add_shared_address";
  case AddRnF32:
    return "add_rn_f32";
  case SubRnF32:
    return "sub_rn_f32";
  case DivRnF32:
    return "div_rn_f32";
  case MultiplyRnF32:
    return "multiply_rn_f32";
  case MadRnF32:
    return "mad_rn_f32";
  case FmaRnF32:
    return "fma_rn_f32";
  case ConvertRnF32U32:
    return "convert_rn_f32_u32";
  case ConvertRnF32S32:
    return "convert_rn_f32_s32";
  case ExpF32:
    return "exp_f32";
  case ConvertRziU32F32:
    return "convert_rzi_u32_f32";
  case SetPredicateGeU32:
    return "set_predicate_ge_u32";
  case SetPredicateEqU32:
    return "set_predicate_eq_u32";
  case SetPredicateLtF32:
    return "set_predicate_lt_f32";
  case SetPredicateGtS32:
    return "set_predicate_gt_s32";
  case SelectU32:
    return "select_u32";
  case SelectF32:
    return "select_f32";
  case BranchIf:
    return "branch_if";
  case LoadGlobalU32:
    return "load_global_u32";
  case StoreGlobalU32:
    return "store_global_u32";
  case LoadGlobalF32:
    return "load_global_f32";
  case StoreGlobalF32:
    return "store_global_f32";
  case StoreGlobalU8:
    return "store_global_u8";
  case StoreGlobalU64:
    return "store_global_u64";
  case LoadSharedU32:
    return "load_shared_u32";
  case StoreSharedU32:
    return "store_shared_u32";
  case BarrierSync:
    return "barrier_sync";
  case Return:
    return "return";
  }
  return "invalid";
}

std::vector<Diagnostic> verify_kernel(const Kernel& kernel) {
  std::vector<Diagnostic> diagnostics;
  const SourceLocation kernel_location{};

  if (kernel.schema_version != kKernelIrSchemaVersion) {
    append_diagnostic(diagnostics, DiagnosticCode::KernelIrSchemaMismatch, kernel_location,
                      "Kernel IR schema version is not supported",
                      std::to_string(kernel.schema_version));
  }
  if (kernel.ptx_major != 9 || kernel.ptx_minor != 0) {
    append_diagnostic(diagnostics, DiagnosticCode::KernelIrInvalidKernel, kernel_location,
                      "Kernel IR v2 requires PTX 9.0 source semantics",
                      std::to_string(kernel.ptx_major) + "." + std::to_string(kernel.ptx_minor));
  }
  if (!valid_kernel_name(kernel.name)) {
    append_diagnostic(diagnostics, DiagnosticCode::KernelIrInvalidKernel, kernel_location,
                      "kernel name is empty or malformed", kernel.name);
  }
  if (kernel.parameters.size() > kMaximumParameters) {
    append_diagnostic(diagnostics, DiagnosticCode::KernelIrInvalidParameter, kernel_location,
                      "parameter count exceeds the Kernel IR v2 bound");
  }
  if (kernel.shared_allocations.size() > kMaximumSharedAllocations) {
    append_diagnostic(diagnostics, DiagnosticCode::KernelIrInvalidKernel, kernel_location,
                      "shared allocation count exceeds the Kernel IR v2 bound");
  }
  std::uint64_t shared_words = 0;
  for (const auto& allocation : kernel.shared_allocations) {
    shared_words += allocation.words;
    if (allocation.words == 0U || shared_words > kMaximumSharedWords) {
      append_diagnostic(diagnostics, DiagnosticCode::KernelIrInvalidKernel, allocation.location,
                        "static shared storage must be non-empty and at most 49152 bytes");
    }
  }
  if (kernel.registers.size() > kMaximumRegisters) {
    append_diagnostic(diagnostics, DiagnosticCode::KernelIrInvalidRegister, kernel_location,
                      "register count exceeds the Kernel IR v2 bound");
  }
  if (kernel.operations.empty() || kernel.operations.size() > kMaximumOperations) {
    append_diagnostic(diagnostics, DiagnosticCode::KernelIrInvalidOperation, kernel_location,
                      "operation count is empty or exceeds the Kernel IR v2 bound");
  }

  for (const auto& parameter : kernel.parameters) {
    if (!valid_parameter_kind(parameter.kind)) {
      append_diagnostic(diagnostics, DiagnosticCode::KernelIrInvalidParameter, parameter.location,
                        "parameter has an invalid kind");
    }
  }
  for (const auto& reg : kernel.registers) {
    if (!valid_value_kind(reg.kind)) {
      append_diagnostic(diagnostics, DiagnosticCode::KernelIrInvalidRegister, reg.location,
                        "register has an invalid value kind");
    }
  }

  const bool has_barrier = std::any_of(
      kernel.operations.begin(), kernel.operations.end(),
      [](const Operation& operation) { return operation.opcode == Opcode::BarrierSync; });
  const bool has_branch =
      std::any_of(kernel.operations.begin(), kernel.operations.end(),
                  [](const Operation& operation) { return operation.opcode == Opcode::BranchIf; });
  if (has_barrier && has_branch) {
    append_diagnostic(diagnostics, DiagnosticCode::KernelIrInvalidControlFlow, kernel_location,
                      "Kernel IR v2 requires unconditional full-CTA barrier participation");
  }

  std::vector<bool> defined(kernel.registers.size(), false);
  for (std::size_t operation_index = 0; operation_index < kernel.operations.size();
       ++operation_index) {
    const auto& operation = kernel.operations[operation_index];
    const auto contract = operation_contract(operation.opcode);

    if (!is_valid_opcode(operation.opcode)) {
      append_diagnostic(diagnostics, DiagnosticCode::KernelIrInvalidOperation, operation.location,
                        "operation opcode is invalid");
    }
    if (operation.input_count != contract.input_count || operation.input_count > 3U) {
      append_diagnostic(diagnostics, DiagnosticCode::KernelIrOperandCount, operation.location,
                        "operation has the wrong operand count",
                        std::string(opcode_name(operation.opcode)));
    }
    if (contract.has_result) {
      if (operation.result >= kernel.registers.size()) {
        append_diagnostic(diagnostics, DiagnosticCode::KernelIrIndexOutOfRange, operation.location,
                          "operation result register is out of range");
      } else {
        if (kernel.registers[operation.result].kind != contract.result_kind) {
          append_diagnostic(diagnostics, DiagnosticCode::KernelIrTypeMismatch, operation.location,
                            "operation result has the wrong value kind",
                            std::string(opcode_name(operation.opcode)));
        }
        if (defined[operation.result]) {
          append_diagnostic(diagnostics, DiagnosticCode::KernelIrDuplicateDefinition,
                            operation.location, "register is defined more than once",
                            std::to_string(operation.result));
        } else {
          defined[operation.result] = true;
        }
      }
    } else if (operation.result != kNoValue) {
      append_diagnostic(diagnostics, DiagnosticCode::KernelIrInvalidOperation, operation.location,
                        "operation must not define a result",
                        std::string(opcode_name(operation.opcode)));
    }

    const auto checked_inputs = std::min<std::uint32_t>(operation.input_count, 3U);
    for (std::uint32_t input_index = 0; input_index < checked_inputs; ++input_index) {
      const auto register_index = operation.inputs[input_index];
      if (register_index >= kernel.registers.size()) {
        append_diagnostic(diagnostics, DiagnosticCode::KernelIrIndexOutOfRange, operation.location,
                          "operation input register is out of range",
                          std::to_string(register_index));
        continue;
      }
      if (input_index < contract.input_count &&
          kernel.registers[register_index].kind != contract.input_kinds[input_index]) {
        append_diagnostic(diagnostics, DiagnosticCode::KernelIrTypeMismatch, operation.location,
                          "operation input has the wrong value kind",
                          std::string(opcode_name(operation.opcode)));
      }
      if (!defined[register_index]) {
        append_diagnostic(diagnostics, DiagnosticCode::KernelIrUseBeforeDefinition,
                          operation.location, "operation reads a register before its definition",
                          std::to_string(register_index));
      }
    }

    if (operation.predicate != kNoValue) {
      if (!may_be_predicated(operation.opcode)) {
        append_diagnostic(
            diagnostics, DiagnosticCode::KernelIrInvalidOperation, operation.location,
            "only advertised global/shared u32 store forms may carry an operation guard");
      } else if (operation.predicate >= kernel.registers.size()) {
        append_diagnostic(diagnostics, DiagnosticCode::KernelIrIndexOutOfRange, operation.location,
                          "operation predicate register is out of range");
      } else {
        if (kernel.registers[operation.predicate].kind != ValueKind::Predicate) {
          append_diagnostic(diagnostics, DiagnosticCode::KernelIrTypeMismatch, operation.location,
                            "operation guard must be a predicate register");
        }
        if (!defined[operation.predicate]) {
          append_diagnostic(diagnostics, DiagnosticCode::KernelIrUseBeforeDefinition,
                            operation.location,
                            "operation guard reads a predicate before its definition");
        }
      }
    } else if (operation.predicate_negated) {
      append_diagnostic(diagnostics, DiagnosticCode::KernelIrInvalidOperation, operation.location,
                        "predicate negation requires an operation guard");
    }

    using enum Opcode;
    switch (operation.opcode) {
    case LoadParameterAddress:
      if (operation.attribute >= kernel.parameters.size() ||
          (operation.attribute < kernel.parameters.size() &&
           kernel.parameters[operation.attribute].kind != ParameterKind::BufferU32)) {
        append_diagnostic(diagnostics, DiagnosticCode::KernelIrInvalidParameter, operation.location,
                          "address load must reference a buffer_u32 parameter");
      }
      break;
    case LoadParameterU32:
      if (operation.attribute >= kernel.parameters.size() ||
          (operation.attribute < kernel.parameters.size() &&
           kernel.parameters[operation.attribute].kind != ParameterKind::ScalarU32)) {
        append_diagnostic(diagnostics, DiagnosticCode::KernelIrInvalidParameter, operation.location,
                          "u32 load must reference a scalar_u32 parameter");
      }
      break;
    case LoadParameterF32:
      if (operation.attribute >= kernel.parameters.size() ||
          (operation.attribute < kernel.parameters.size() &&
           kernel.parameters[operation.attribute].kind != ParameterKind::ScalarF32)) {
        append_diagnostic(diagnostics, DiagnosticCode::KernelIrInvalidParameter, operation.location,
                          "f32 load must reference a scalar_f32 parameter");
      }
      break;
    case LoadSharedAddress:
      if (operation.attribute >= kernel.shared_allocations.size()) {
        append_diagnostic(diagnostics, DiagnosticCode::KernelIrIndexOutOfRange, operation.location,
                          "shared allocation selector is out of range");
      }
      break;
    case MoveSpecialU32:
      if (operation.attribute > static_cast<std::uint32_t>(SpecialRegister::GridDimY)) {
        append_diagnostic(diagnostics, DiagnosticCode::KernelIrInvalidOperation, operation.location,
                          "special-register selector is invalid");
      }
      break;
    case BranchIf:
      if (operation.attribute <= operation_index ||
          operation.attribute >= kernel.operations.size() ||
          (operation.attribute < kernel.operations.size() &&
           kernel.operations[operation.attribute].opcode != Return)) {
        append_diagnostic(diagnostics, DiagnosticCode::KernelIrInvalidControlFlow,
                          operation.location,
                          "Kernel IR v2 branches must target the final return guard");
      }
      break;
    case BarrierSync:
      if (operation.attribute != 0U) {
        append_diagnostic(diagnostics, DiagnosticCode::KernelIrInvalidOperation, operation.location,
                          "only bar.sync 0 is in the PTX manifest");
      }
      break;
    case Return:
      if (operation_index + 1U != kernel.operations.size()) {
        append_diagnostic(diagnostics, DiagnosticCode::KernelIrInvalidControlFlow,
                          operation.location, "Kernel IR v2 return must be the final operation");
      }
      break;
    default:
      break;
    }

    if (operation.opcode != BranchIf && operation.flag) {
      append_diagnostic(diagnostics, DiagnosticCode::KernelIrInvalidOperation, operation.location,
                        "only branch_if may carry the branch-negation flag");
    }
  }

  if (kernel.operations.empty() || kernel.operations.back().opcode != Opcode::Return) {
    append_diagnostic(diagnostics, DiagnosticCode::KernelIrMissingReturn, kernel_location,
                      "kernel must end with return");
  }
  return diagnostics;
}

SerializationResult serialize_kernel(const Kernel& kernel) {
  auto diagnostics = verify_kernel(kernel);
  if (!diagnostics.empty()) {
    return SerializationResult{.text = {}, .diagnostics = std::move(diagnostics)};
  }

  std::ostringstream output;
  output << "MFKIR " << kKernelIrSchemaVersion << '\n';
  output << "PTX " << kernel.ptx_major << ' ' << kernel.ptx_minor << '\n';
  output << "KERNEL " << kernel.name << '\n';
  output << "PARAMETERS " << kernel.parameters.size() << '\n';
  for (const auto& parameter : kernel.parameters) {
    output << "PARAMETER " << parameter_kind_name(parameter.kind) << '\n';
  }
  output << "SHARED_ALLOCATIONS " << kernel.shared_allocations.size() << '\n';
  for (const auto& allocation : kernel.shared_allocations) {
    output << "SHARED_U32 " << allocation.words << '\n';
  }
  output << "REGISTERS " << kernel.registers.size() << '\n';
  for (const auto& reg : kernel.registers) {
    output << "REGISTER " << value_kind_name(reg.kind) << '\n';
  }
  output << "OPERATIONS " << kernel.operations.size() << '\n';
  for (const auto& operation : kernel.operations) {
    output << "OP " << opcode_name(operation.opcode) << ' ';
    if (operation.result == kNoValue) {
      output << '-';
    } else {
      output << operation.result;
    }
    output << ' ' << operation.input_count;
    for (std::uint32_t index = 0; index < operation.input_count; ++index) {
      output << ' ' << operation.inputs[index];
    }
    output << ' ' << operation.attribute << ' ' << (operation.flag ? 1 : 0) << ' ';
    if (operation.predicate == kNoValue) {
      output << '-';
    } else {
      output << operation.predicate;
    }
    output << ' ' << (operation.predicate_negated ? 1 : 0) << '\n';
  }
  output << "END\n";
  return SerializationResult{.text = output.str(), .diagnostics = {}};
}

} // namespace metaflux::compiler
