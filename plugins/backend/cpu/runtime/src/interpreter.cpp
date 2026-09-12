#include "metaflux/backend/cpu/interpreter.hpp"
#include "metaflux/backend/cpu/compiled_kernel.hpp"

#include "metaflux/backend/cpu/executor.hpp"

#include <algorithm>
#include <array>
#include <cfenv>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#if defined(__i386__) || defined(__x86_64__)
#include <xmmintrin.h>
#endif

namespace metaflux::backend::cpu {
namespace {

template <typename Target, typename Source>
[[nodiscard]] Target bit_cast_compatible(const Source& source) noexcept {
  static_assert(sizeof(Target) == sizeof(Source));
  static_assert(std::is_trivially_copyable_v<Target>);
  static_assert(std::is_trivially_copyable_v<Source>);
  Target target;
  std::memcpy(&target, &source, sizeof(target));
  return target;
}

constexpr std::uint32_t kNoValue = std::numeric_limits<std::uint32_t>::max();
constexpr std::size_t kMaximumArtifactBytes = 4U * 1024U * 1024U;
constexpr std::size_t kMaximumParameters = 64;
constexpr std::size_t kMaximumSharedAllocations = 64;
constexpr std::uint64_t kMaximumSharedWords = 49152U / sizeof(std::uint32_t);
constexpr std::size_t kMaximumRegisters = 4096;
constexpr std::size_t kMaximumOperations = 65536;
constexpr std::uint64_t kMaximumLogicalThreads = 16U * 1024U * 1024U;
constexpr std::uint32_t kMaximumThreadsPerCta = 1024;
constexpr std::uint64_t kMaximumThreadSteps = 131072;

enum class ParameterKind : std::uint32_t { BufferU32, ScalarU32, ScalarF32 };

enum class ValueKind : std::uint32_t {
  Predicate,
  U32,
  U64,
  F32,
  GlobalAddress,
  SharedAddress,
};

enum class Opcode : std::uint32_t {
  LoadParameterAddress,
  LoadParameterU32,
  LoadParameterF32,
  LoadSharedAddress,
  MoveSpecialU32,
  MoveImmediateU32,
  AbsS32,
  AbsF32,
  SqrtRnF32,
  ExpF32,
  AddU32,
  SubU32,
  MultiplyLoU32,
  MadLoU32,
  MultiplyWideU32,
  AddGlobalAddress,
  AddSharedAddress,
  AddRnF32,
  SubRnF32,
  DivRnF32,
  MultiplyRnF32,
  MadRnF32,
  FmaRnF32,
  ConvertRnF32U32,
  ConvertRziU32F32,
  SetPredicateGeU32,
  SetPredicateEqU32,
  SetPredicateLtF32,
  SetPredicateGtS32,
  SelectU32,
  SelectF32,
  BranchIf,
  LoadGlobalU32,
  StoreGlobalU32,
  LoadGlobalF32,
  StoreGlobalF32,
  StoreGlobalU8,
  StoreGlobalU64,
  LoadSharedU32,
  StoreSharedU32,
  BarrierSync,
  Return,
  ConvertRnF32S32,
};

struct Operation {
  Opcode opcode;
  std::uint32_t result;
  std::array<std::uint32_t, 3> inputs{kNoValue, kNoValue, kNoValue};
  std::uint32_t input_count;
  std::uint32_t attribute;
  bool flag;
  std::uint32_t predicate;
  bool predicate_negated;
};

struct Kernel {
  std::vector<ParameterKind> parameters;
  std::vector<std::uint32_t> shared_words;
  std::vector<ValueKind> registers;
  std::vector<Operation> operations;
  ExpF32Helper exp_f32 = nullptr;
};

struct ParseResult {
  std::optional<Kernel> kernel;
  ExecutionDiagnostic diagnostic;
};

class TokenReader {
public:
  explicit TokenReader(std::string_view text) : stream_(std::string(text)) {}

  bool take(std::string& token) { return static_cast<bool>(stream_ >> token); }

  bool expect(std::string_view expected) {
    std::string token;
    return take(token) && token == expected;
  }

  bool exhausted() {
    std::string token;
    return !take(token);
  }

private:
  std::istringstream stream_;
};

std::optional<std::uint32_t> parse_u32(std::string_view text) {
  std::uint32_t value = 0;
  const auto result = std::from_chars(text.data(), text.data() + text.size(), value, 10);
  if (result.ec != std::errc{} || result.ptr != text.data() + text.size()) {
    return std::nullopt;
  }
  return value;
}

std::optional<bool> parse_bool(std::string_view text) {
  if (text == "0") {
    return false;
  }
  if (text == "1") {
    return true;
  }
  return std::nullopt;
}

std::optional<ParameterKind> parse_parameter_kind(std::string_view text) {
  if (text == "buffer_u32") {
    return ParameterKind::BufferU32;
  }
  if (text == "scalar_u32") {
    return ParameterKind::ScalarU32;
  }
  if (text == "scalar_f32") {
    return ParameterKind::ScalarF32;
  }
  return std::nullopt;
}

std::optional<ValueKind> parse_value_kind(std::string_view text) {
  if (text == "predicate") {
    return ValueKind::Predicate;
  }
  if (text == "u32") {
    return ValueKind::U32;
  }
  if (text == "u64") {
    return ValueKind::U64;
  }
  if (text == "f32") {
    return ValueKind::F32;
  }
  if (text == "global_address") {
    return ValueKind::GlobalAddress;
  }
  if (text == "shared_address") {
    return ValueKind::SharedAddress;
  }
  return std::nullopt;
}

std::optional<Opcode> parse_opcode(std::string_view text) {
  using Pair = std::pair<std::string_view, Opcode>;
  constexpr std::array<Pair, 43> entries{{
      {"load_parameter_address", Opcode::LoadParameterAddress},
      {"load_parameter_u32", Opcode::LoadParameterU32},
      {"load_parameter_f32", Opcode::LoadParameterF32},
      {"load_shared_address", Opcode::LoadSharedAddress},
      {"move_special_u32", Opcode::MoveSpecialU32},
      {"move_immediate_u32", Opcode::MoveImmediateU32},
      {"abs_s32", Opcode::AbsS32},
      {"abs_f32", Opcode::AbsF32},
      {"sqrt_rn_f32", Opcode::SqrtRnF32},
      {"exp_f32", Opcode::ExpF32},
      {"add_u32", Opcode::AddU32},
      {"sub_u32", Opcode::SubU32},
      {"multiply_lo_u32", Opcode::MultiplyLoU32},
      {"mad_lo_u32", Opcode::MadLoU32},
      {"multiply_wide_u32", Opcode::MultiplyWideU32},
      {"add_global_address", Opcode::AddGlobalAddress},
      {"add_shared_address", Opcode::AddSharedAddress},
      {"add_rn_f32", Opcode::AddRnF32},
      {"sub_rn_f32", Opcode::SubRnF32},
      {"div_rn_f32", Opcode::DivRnF32},
      {"multiply_rn_f32", Opcode::MultiplyRnF32},
      {"mad_rn_f32", Opcode::MadRnF32},
      {"fma_rn_f32", Opcode::FmaRnF32},
      {"convert_rn_f32_u32", Opcode::ConvertRnF32U32},
      {"convert_rn_f32_s32", Opcode::ConvertRnF32S32},
      {"convert_rzi_u32_f32", Opcode::ConvertRziU32F32},
      {"set_predicate_ge_u32", Opcode::SetPredicateGeU32},
      {"set_predicate_eq_u32", Opcode::SetPredicateEqU32},
      {"set_predicate_lt_f32", Opcode::SetPredicateLtF32},
      {"set_predicate_gt_s32", Opcode::SetPredicateGtS32},
      {"select_u32", Opcode::SelectU32},
      {"select_f32", Opcode::SelectF32},
      {"branch_if", Opcode::BranchIf},
      {"load_global_u32", Opcode::LoadGlobalU32},
      {"store_global_u32", Opcode::StoreGlobalU32},
      {"load_global_f32", Opcode::LoadGlobalF32},
      {"store_global_f32", Opcode::StoreGlobalF32},
      {"store_global_u8", Opcode::StoreGlobalU8},
      {"store_global_u64", Opcode::StoreGlobalU64},
      {"load_shared_u32", Opcode::LoadSharedU32},
      {"store_shared_u32", Opcode::StoreSharedU32},
      {"barrier_sync", Opcode::BarrierSync},
      {"return", Opcode::Return},
  }};
  const auto found = std::find_if(entries.begin(), entries.end(),
                                  [&](const Pair& entry) { return entry.first == text; });
  return found == entries.end() ? std::nullopt : std::optional<Opcode>(found->second);
}

ExecutionDiagnostic base_diagnostic(ExecutionError error, std::uint32_t operation = kNoValue) {
  return ExecutionDiagnostic{.error = error,
                             .operation = operation,
                             .block_x = 0,
                             .block_y = 0,
                             .thread_x = 0,
                             .thread_y = 0};
}

ParseResult parse_failure(ExecutionError error, std::uint32_t operation = kNoValue) {
  return ParseResult{.kernel = std::nullopt, .diagnostic = base_diagnostic(error, operation)};
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
  case ExpF32:
    return {true, F32, {F32, U32, U32}, 1};
  case AddU32:
  case SubU32:
  case MultiplyLoU32:
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

bool may_be_predicated(Opcode opcode) {
  return opcode == Opcode::StoreGlobalU32 || opcode == Opcode::StoreGlobalU64 ||
         opcode == Opcode::StoreSharedU32;
}

std::optional<std::uint32_t> take_u32(TokenReader& reader) {
  std::string token;
  if (!reader.take(token)) {
    return std::nullopt;
  }
  return parse_u32(token);
}

std::optional<ExecutionError> validate_kernel(const Kernel& kernel) {
  if (kernel.shared_words.size() > kMaximumSharedAllocations) {
    return ExecutionError::InvalidArtifact;
  }
  std::uint64_t shared_total = 0;
  for (const auto words : kernel.shared_words) {
    shared_total += words;
    if (words == 0U || shared_total > kMaximumSharedWords) {
      return ExecutionError::InvalidArtifact;
    }
  }
  const bool has_barrier = std::any_of(
      kernel.operations.begin(), kernel.operations.end(),
      [](const Operation& operation) { return operation.opcode == Opcode::BarrierSync; });
  const bool has_branch =
      std::any_of(kernel.operations.begin(), kernel.operations.end(),
                  [](const Operation& operation) { return operation.opcode == Opcode::BranchIf; });
  if (has_barrier && has_branch) {
    return ExecutionError::InvalidArtifact;
  }

  std::vector<bool> defined(kernel.registers.size(), false);
  for (std::size_t index = 0; index < kernel.operations.size(); ++index) {
    const auto& operation = kernel.operations[index];
    const auto contract = operation_contract(operation.opcode);
    if (operation.input_count != contract.input_count || operation.input_count > 3U) {
      return ExecutionError::InvalidArtifact;
    }
    if (contract.has_result) {
      if (operation.result >= kernel.registers.size() || defined[operation.result] ||
          kernel.registers[operation.result] != contract.result_kind) {
        return ExecutionError::InvalidArtifact;
      }
      defined[operation.result] = true;
    } else if (operation.result != kNoValue) {
      return ExecutionError::InvalidArtifact;
    }
    for (std::uint32_t input = 0; input < operation.input_count; ++input) {
      const auto register_index = operation.inputs[input];
      if (register_index >= kernel.registers.size() || !defined[register_index] ||
          kernel.registers[register_index] != contract.input_kinds[input]) {
        return ExecutionError::InvalidArtifact;
      }
    }
    if (operation.predicate != kNoValue) {
      if (!may_be_predicated(operation.opcode) || operation.predicate >= kernel.registers.size() ||
          !defined[operation.predicate] ||
          kernel.registers[operation.predicate] != ValueKind::Predicate) {
        return ExecutionError::InvalidArtifact;
      }
    } else if (operation.predicate_negated) {
      return ExecutionError::InvalidArtifact;
    }
    switch (operation.opcode) {
    case Opcode::LoadParameterAddress:
      if (operation.attribute >= kernel.parameters.size() ||
          kernel.parameters[operation.attribute] != ParameterKind::BufferU32) {
        return ExecutionError::InvalidArtifact;
      }
      break;
    case Opcode::LoadParameterU32:
      if (operation.attribute >= kernel.parameters.size() ||
          kernel.parameters[operation.attribute] != ParameterKind::ScalarU32) {
        return ExecutionError::InvalidArtifact;
      }
      break;
    case Opcode::LoadParameterF32:
      if (operation.attribute >= kernel.parameters.size() ||
          kernel.parameters[operation.attribute] != ParameterKind::ScalarF32) {
        return ExecutionError::InvalidArtifact;
      }
      break;
    case Opcode::LoadSharedAddress:
      if (operation.attribute >= kernel.shared_words.size()) {
        return ExecutionError::InvalidArtifact;
      }
      break;
    case Opcode::MoveSpecialU32:
      if (operation.attribute > 7U) {
        return ExecutionError::InvalidArtifact;
      }
      break;
    case Opcode::BranchIf:
      if (operation.attribute <= index || operation.attribute >= kernel.operations.size() ||
          kernel.operations[operation.attribute].opcode != Opcode::Return) {
        return ExecutionError::InvalidArtifact;
      }
      break;
    case Opcode::BarrierSync:
      if (operation.attribute != 0U) {
        return ExecutionError::InvalidArtifact;
      }
      break;
    case Opcode::Return:
      if (index + 1U != kernel.operations.size()) {
        return ExecutionError::InvalidArtifact;
      }
      break;
    default:
      break;
    }
    if (operation.opcode != Opcode::BranchIf && operation.flag) {
      return ExecutionError::InvalidArtifact;
    }
  }
  if (kernel.operations.empty() || kernel.operations.back().opcode != Opcode::Return) {
    return ExecutionError::InvalidArtifact;
  }
  return std::nullopt;
}

ParseResult parse_kernel(std::string_view text) {
  if (text.size() > kMaximumArtifactBytes) {
    return parse_failure(ExecutionError::InvalidArtifact);
  }
  TokenReader reader(text);
  if (!reader.expect("MFKIR")) {
    return parse_failure(ExecutionError::InvalidArtifact);
  }
  const auto schema = take_u32(reader);
  if (!schema.has_value() || *schema != 2U) {
    return parse_failure(ExecutionError::SchemaMismatch);
  }
  if (!reader.expect("PTX")) {
    return parse_failure(ExecutionError::InvalidArtifact);
  }
  const auto ptx_major = take_u32(reader);
  const auto ptx_minor = take_u32(reader);
  if (!ptx_major.has_value() || !ptx_minor.has_value() || *ptx_major != 9U || *ptx_minor != 0U) {
    return parse_failure(ExecutionError::SchemaMismatch);
  }
  if (!reader.expect("KERNEL")) {
    return parse_failure(ExecutionError::InvalidArtifact);
  }
  std::string kernel_name;
  if (!reader.take(kernel_name) || kernel_name.empty()) {
    return parse_failure(ExecutionError::InvalidArtifact);
  }

  Kernel kernel;
  if (!reader.expect("PARAMETERS")) {
    return parse_failure(ExecutionError::InvalidArtifact);
  }
  const auto parameter_count = take_u32(reader);
  if (!parameter_count.has_value() || *parameter_count > kMaximumParameters) {
    return parse_failure(ExecutionError::InvalidArtifact);
  }
  kernel.parameters.reserve(*parameter_count);
  for (std::uint32_t index = 0; index < *parameter_count; ++index) {
    static_cast<void>(index);
    if (!reader.expect("PARAMETER")) {
      return parse_failure(ExecutionError::InvalidArtifact);
    }
    std::string text_kind;
    if (!reader.take(text_kind)) {
      return parse_failure(ExecutionError::InvalidArtifact);
    }
    const auto kind = parse_parameter_kind(text_kind);
    if (!kind.has_value()) {
      return parse_failure(ExecutionError::InvalidArtifact);
    }
    kernel.parameters.push_back(*kind);
  }

  if (!reader.expect("SHARED_ALLOCATIONS")) {
    return parse_failure(ExecutionError::InvalidArtifact);
  }
  const auto shared_count = take_u32(reader);
  if (!shared_count.has_value() || *shared_count > kMaximumSharedAllocations) {
    return parse_failure(ExecutionError::InvalidArtifact);
  }
  kernel.shared_words.reserve(*shared_count);
  for (std::uint32_t index = 0; index < *shared_count; ++index) {
    static_cast<void>(index);
    if (!reader.expect("SHARED_U32")) {
      return parse_failure(ExecutionError::InvalidArtifact);
    }
    const auto words = take_u32(reader);
    if (!words.has_value()) {
      return parse_failure(ExecutionError::InvalidArtifact);
    }
    kernel.shared_words.push_back(*words);
  }

  if (!reader.expect("REGISTERS")) {
    return parse_failure(ExecutionError::InvalidArtifact);
  }
  const auto register_count = take_u32(reader);
  if (!register_count.has_value() || *register_count > kMaximumRegisters) {
    return parse_failure(ExecutionError::InvalidArtifact);
  }
  kernel.registers.reserve(*register_count);
  for (std::uint32_t index = 0; index < *register_count; ++index) {
    static_cast<void>(index);
    if (!reader.expect("REGISTER")) {
      return parse_failure(ExecutionError::InvalidArtifact);
    }
    std::string text_kind;
    if (!reader.take(text_kind)) {
      return parse_failure(ExecutionError::InvalidArtifact);
    }
    const auto kind = parse_value_kind(text_kind);
    if (!kind.has_value()) {
      return parse_failure(ExecutionError::InvalidArtifact);
    }
    kernel.registers.push_back(*kind);
  }

  if (!reader.expect("OPERATIONS")) {
    return parse_failure(ExecutionError::InvalidArtifact);
  }
  const auto operation_count = take_u32(reader);
  if (!operation_count.has_value() || *operation_count == 0U ||
      *operation_count > kMaximumOperations) {
    return parse_failure(ExecutionError::InvalidArtifact);
  }
  kernel.operations.reserve(*operation_count);
  for (std::uint32_t index = 0; index < *operation_count; ++index) {
    if (!reader.expect("OP")) {
      return parse_failure(ExecutionError::InvalidArtifact, index);
    }
    std::string opcode_text;
    if (!reader.take(opcode_text)) {
      return parse_failure(ExecutionError::InvalidArtifact, index);
    }
    const auto opcode = parse_opcode(opcode_text);
    if (!opcode.has_value()) {
      return parse_failure(ExecutionError::UnsupportedOperation, index);
    }
    std::string result_text;
    if (!reader.take(result_text)) {
      return parse_failure(ExecutionError::InvalidArtifact, index);
    }
    std::uint32_t result = kNoValue;
    if (result_text != "-") {
      const auto parsed = parse_u32(result_text);
      if (!parsed.has_value()) {
        return parse_failure(ExecutionError::InvalidArtifact, index);
      }
      result = *parsed;
    }
    const auto input_count = take_u32(reader);
    if (!input_count.has_value() || *input_count > 3U) {
      return parse_failure(ExecutionError::InvalidArtifact, index);
    }
    Operation operation{.opcode = *opcode,
                        .result = result,
                        .inputs = {},
                        .input_count = *input_count,
                        .attribute = 0,
                        .flag = false,
                        .predicate = kNoValue,
                        .predicate_negated = false};
    for (std::uint32_t input = 0; input < *input_count; ++input) {
      const auto value = take_u32(reader);
      if (!value.has_value()) {
        return parse_failure(ExecutionError::InvalidArtifact, index);
      }
      operation.inputs[input] = *value;
    }
    const auto attribute = take_u32(reader);
    std::string flag_text;
    std::string predicate_text;
    std::string predicate_negated_text;
    if (!attribute.has_value() || !reader.take(flag_text) || !reader.take(predicate_text) ||
        !reader.take(predicate_negated_text)) {
      return parse_failure(ExecutionError::InvalidArtifact, index);
    }
    const auto flag = parse_bool(flag_text);
    const auto predicate_negated = parse_bool(predicate_negated_text);
    if (!flag.has_value() || !predicate_negated.has_value()) {
      return parse_failure(ExecutionError::InvalidArtifact, index);
    }
    if (predicate_text != "-") {
      const auto predicate = parse_u32(predicate_text);
      if (!predicate.has_value()) {
        return parse_failure(ExecutionError::InvalidArtifact, index);
      }
      operation.predicate = *predicate;
    }
    operation.attribute = *attribute;
    operation.flag = *flag;
    operation.predicate_negated = *predicate_negated;
    kernel.operations.push_back(operation);
  }
  if (!reader.expect("END") || !reader.exhausted()) {
    return parse_failure(ExecutionError::InvalidArtifact);
  }
  if (const auto error = validate_kernel(kernel); error.has_value()) {
    return parse_failure(*error);
  }
  return ParseResult{.kernel = std::move(kernel), .diagnostic = {}};
}

struct GlobalAddress {
  std::uint32_t parameter;
  std::uint64_t byte_offset;
};

struct SharedAddress {
  std::uint32_t allocation;
  std::uint64_t byte_offset;
};

using RuntimeValue = std::variant<std::monostate, bool, std::uint32_t, std::uint64_t, float,
                                  GlobalAddress, SharedAddress>;

struct ThreadState {
  std::vector<RuntimeValue> registers;
  std::uint32_t thread_x;
  std::uint32_t thread_y;
  std::uint32_t pc = 0;
  std::uint64_t steps = 0;
  bool waiting = false;
  bool returned = false;
};

struct Coordinates {
  std::uint32_t block_x;
  std::uint32_t block_y;
  std::uint32_t thread_x;
  std::uint32_t thread_y;
};

ExecutionResult execution_failure(ExecutionError error, std::uint32_t operation,
                                  Coordinates coordinates) {
  return ExecutionResult{.diagnostic = ExecutionDiagnostic{.error = error,
                                                           .operation = operation,
                                                           .block_x = coordinates.block_x,
                                                           .block_y = coordinates.block_y,
                                                           .thread_x = coordinates.thread_x,
                                                           .thread_y = coordinates.thread_y}};
}

std::optional<ExecutionError> validate_arguments(const Kernel& kernel,
                                                 std::span<const Argument> arguments) {
  if (arguments.size() != kernel.parameters.size()) {
    return ExecutionError::ArgumentCount;
  }
  for (std::size_t index = 0; index < arguments.size(); ++index) {
    switch (kernel.parameters[index]) {
    case ParameterKind::BufferU32:
      if (!std::holds_alternative<BufferArgument>(arguments[index])) {
        return ExecutionError::ArgumentType;
      }
      break;
    case ParameterKind::ScalarU32:
      if (!std::holds_alternative<std::uint32_t>(arguments[index])) {
        return ExecutionError::ArgumentType;
      }
      break;
    case ParameterKind::ScalarF32:
      if (!std::holds_alternative<Float32Argument>(arguments[index])) {
        return ExecutionError::ArgumentType;
      }
      break;
    }
  }
  return std::nullopt;
}

bool has_fp_operations(const Kernel& kernel) {
  return std::any_of(kernel.operations.begin(), kernel.operations.end(), [](const Operation& op) {
    switch (op.opcode) {
    case Opcode::LoadParameterF32:
    case Opcode::AddRnF32:
    case Opcode::SubRnF32:
    case Opcode::DivRnF32:
    case Opcode::MultiplyRnF32:
    case Opcode::MadRnF32:
    case Opcode::FmaRnF32:
    case Opcode::ConvertRnF32U32:
    case Opcode::ConvertRnF32S32:
    case Opcode::ConvertRziU32F32:
    case Opcode::SetPredicateLtF32:
    case Opcode::LoadGlobalF32:
    case Opcode::StoreGlobalF32:
      return true;
    default:
      return false;
    }
  });
}

bool fp_environment_supported() {
  if (!std::numeric_limits<float>::is_iec559 || std::fegetround() != FE_TONEAREST) {
    return false;
  }
#if defined(__i386__) || defined(__x86_64__)
  constexpr unsigned int kDaz = 1U << 6U;
  constexpr unsigned int kFtz = 1U << 15U;
  if ((_mm_getcsr() & (kDaz | kFtz)) != 0U) {
    return false;
  }
#endif
  return true;
}

float add_rn(float left, float right) {
  volatile float result = left + right;
  return result;
}

float sub_rn(float left, float right) {
  volatile float result = left - right;
  return result;
}

float div_rn(float left, float right) {
  volatile float result = left / right;
  return result;
}

std::uint32_t abs_s32(std::uint32_t bits) {
  const std::uint32_t mask = std::uint32_t{0} - (bits >> 31U);
  return (bits ^ mask) - mask;
}

float abs_f32(float value) {
  std::uint32_t bits = 0;
  static_assert(sizeof(bits) == sizeof(value));
  (void)std::memcpy(&bits, &value, sizeof(bits));
  bits &= 0x7fffffffU;
  float result = 0.0F;
  (void)std::memcpy(&result, &bits, sizeof(result));
  return result;
}

float sqrt_rn(float value) {
  const volatile float result = std::sqrt(value);
  return result;
}

float multiply_rn(float left, float right) {
  volatile float result = left * right;
  return result;
}

std::uint32_t convert_rzi_u32(float value) {
  if (std::isnan(value) || value <= 0.0F) {
    return 0U;
  }
  constexpr float kU32UpperBoundary = 4294967296.0F;
  if (value >= kU32UpperBoundary) {
    return std::numeric_limits<std::uint32_t>::max();
  }
  return static_cast<std::uint32_t>(std::trunc(value));
}

std::optional<ExecutionError> global_word(const GlobalAddress& address,
                                          std::span<const Argument> arguments, bool write,
                                          std::uint32_t*& word) {
  if (address.parameter >= arguments.size() ||
      !std::holds_alternative<BufferArgument>(arguments[address.parameter])) {
    return ExecutionError::ArgumentType;
  }
  if ((address.byte_offset % sizeof(std::uint32_t)) != 0U) {
    return ExecutionError::MisalignedAddress;
  }
  auto buffer = std::get<BufferArgument>(arguments[address.parameter]);
  if (write && !buffer.writable) {
    return ExecutionError::WriteToReadOnly;
  }
  const auto index = address.byte_offset / sizeof(std::uint32_t);
  if (index >= buffer.words.size()) {
    return ExecutionError::OutOfBounds;
  }
  word = &buffer.words[static_cast<std::size_t>(index)];
  return std::nullopt;
}

std::optional<ExecutionError> global_byte(const GlobalAddress& address,
                                          std::span<const Argument> arguments, bool write,
                                          std::uint8_t*& byte) {
  if (address.parameter >= arguments.size() ||
      !std::holds_alternative<BufferArgument>(arguments[address.parameter])) {
    return ExecutionError::ArgumentType;
  }
  auto buffer = std::get<BufferArgument>(arguments[address.parameter]);
  if (write && !buffer.writable) {
    return ExecutionError::WriteToReadOnly;
  }
  const auto byte_offset = address.byte_offset;
  if (byte_offset >= buffer.words.size() * sizeof(std::uint32_t)) {
    return ExecutionError::OutOfBounds;
  }
  byte = reinterpret_cast<std::uint8_t*>(buffer.words.data()) + byte_offset;
  return std::nullopt;
}

std::optional<ExecutionError> store_global_u64(const GlobalAddress& address,
                                               std::span<const Argument> arguments,
                                               std::uint64_t value) {
  if (address.parameter >= arguments.size() ||
      !std::holds_alternative<BufferArgument>(arguments[address.parameter])) {
    return ExecutionError::ArgumentType;
  }
  if ((address.byte_offset % sizeof(std::uint64_t)) != 0U) {
    return ExecutionError::MisalignedAddress;
  }
  auto buffer = std::get<BufferArgument>(arguments[address.parameter]);
  if (!buffer.writable) {
    return ExecutionError::WriteToReadOnly;
  }
  const auto index = address.byte_offset / sizeof(std::uint32_t);
  if (index + 1U >= buffer.words.size()) {
    return ExecutionError::OutOfBounds;
  }
  buffer.words[static_cast<std::size_t>(index)] = static_cast<std::uint32_t>(value);
  buffer.words[static_cast<std::size_t>(index) + 1U] =
      static_cast<std::uint32_t>(value >> 32U);
  return std::nullopt;
}

std::optional<ExecutionError> shared_word(const SharedAddress& address,
                                          std::vector<std::vector<std::uint32_t>>& shared,
                                          std::uint32_t*& word) {
  if (address.allocation >= shared.size()) {
    return ExecutionError::InvalidArtifact;
  }
  if ((address.byte_offset % sizeof(std::uint32_t)) != 0U) {
    return ExecutionError::MisalignedAddress;
  }
  const auto index = address.byte_offset / sizeof(std::uint32_t);
  if (index >= shared[address.allocation].size()) {
    return ExecutionError::OutOfBounds;
  }
  word = &shared[address.allocation][static_cast<std::size_t>(index)];
  return std::nullopt;
}

std::uint32_t special_value(std::uint32_t selector, Coordinates coordinates,
                            LaunchDimensions launch) {
  switch (selector) {
  case 0:
    return coordinates.thread_x;
  case 1:
    return coordinates.thread_y;
  case 2:
    return coordinates.block_x;
  case 3:
    return coordinates.block_y;
  case 4:
    return launch.block_x;
  case 5:
    return launch.block_y;
  case 6:
    return launch.grid_x;
  case 7:
    return launch.grid_y;
  default:
    return 0U;
  }
}

std::optional<ExecutionResult> execute_one(const Kernel& kernel,
                                           std::span<const Argument> arguments,
                                           LaunchDimensions launch, Coordinates coordinates,
                                           ThreadState& thread,
                                           std::vector<std::vector<std::uint32_t>>& shared) {
  if (thread.pc >= kernel.operations.size()) {
    return execution_failure(ExecutionError::InvalidArtifact, thread.pc, coordinates);
  }
  if (++thread.steps > kMaximumThreadSteps) {
    return execution_failure(ExecutionError::StepLimit, thread.pc, coordinates);
  }
  const auto& operation = kernel.operations[thread.pc];
  if (operation.predicate != kNoValue) {
    const bool predicate = std::get<bool>(thread.registers[operation.predicate]);
    const bool active = operation.predicate_negated ? !predicate : predicate;
    if (!active) {
      ++thread.pc;
      return std::nullopt;
    }
  }

  auto& values = thread.registers;
  switch (operation.opcode) {
  case Opcode::LoadParameterAddress:
    values[operation.result] = GlobalAddress{.parameter = operation.attribute, .byte_offset = 0};
    ++thread.pc;
    break;
  case Opcode::LoadParameterU32:
    values[operation.result] = std::get<std::uint32_t>(arguments[operation.attribute]);
    ++thread.pc;
    break;
  case Opcode::LoadParameterF32:
    values[operation.result] =
        bit_cast_compatible<float>(std::get<Float32Argument>(arguments[operation.attribute]).bits);
    ++thread.pc;
    break;
  case Opcode::LoadSharedAddress:
    values[operation.result] = SharedAddress{.allocation = operation.attribute, .byte_offset = 0};
    ++thread.pc;
    break;
  case Opcode::MoveSpecialU32:
    values[operation.result] = special_value(operation.attribute, coordinates, launch);
    ++thread.pc;
    break;
  case Opcode::MoveImmediateU32:
    values[operation.result] = operation.attribute;
    ++thread.pc;
    break;
  case Opcode::AbsS32:
    values[operation.result] = abs_s32(std::get<std::uint32_t>(values[operation.inputs[0]]));
    ++thread.pc;
    break;
  case Opcode::AbsF32:
    values[operation.result] = abs_f32(std::get<float>(values[operation.inputs[0]]));
    ++thread.pc;
    break;
  case Opcode::SqrtRnF32:
    values[operation.result] = sqrt_rn(std::get<float>(values[operation.inputs[0]]));
    ++thread.pc;
    break;
  case Opcode::ExpF32:
    values[operation.result] = kernel.exp_f32(std::get<float>(values[operation.inputs[0]]));
    ++thread.pc;
    break;
  case Opcode::AddU32:
    values[operation.result] = std::get<std::uint32_t>(values[operation.inputs[0]]) +
                               std::get<std::uint32_t>(values[operation.inputs[1]]);
    ++thread.pc;
    break;
  case Opcode::SubU32:
    values[operation.result] = std::get<std::uint32_t>(values[operation.inputs[0]]) -
                               std::get<std::uint32_t>(values[operation.inputs[1]]);
    ++thread.pc;
    break;
  case Opcode::MultiplyLoU32:
    values[operation.result] = std::get<std::uint32_t>(values[operation.inputs[0]]) *
                               std::get<std::uint32_t>(values[operation.inputs[1]]);
    ++thread.pc;
    break;
  case Opcode::MadLoU32:
    values[operation.result] = std::get<std::uint32_t>(values[operation.inputs[0]]) *
                                   std::get<std::uint32_t>(values[operation.inputs[1]]) +
                               std::get<std::uint32_t>(values[operation.inputs[2]]);
    ++thread.pc;
    break;
  case Opcode::MultiplyWideU32:
    values[operation.result] =
        static_cast<std::uint64_t>(std::get<std::uint32_t>(values[operation.inputs[0]])) *
        operation.attribute;
    ++thread.pc;
    break;
  case Opcode::AddGlobalAddress: {
    const auto base = std::get<GlobalAddress>(values[operation.inputs[0]]);
    const auto offset = std::get<std::uint64_t>(values[operation.inputs[1]]);
    if (offset > std::numeric_limits<std::uint64_t>::max() - base.byte_offset) {
      return execution_failure(ExecutionError::AddressOverflow, thread.pc, coordinates);
    }
    values[operation.result] =
        GlobalAddress{.parameter = base.parameter, .byte_offset = base.byte_offset + offset};
    ++thread.pc;
    break;
  }
  case Opcode::AddSharedAddress: {
    const auto base = std::get<SharedAddress>(values[operation.inputs[0]]);
    const auto offset = std::get<std::uint32_t>(values[operation.inputs[1]]);
    values[operation.result] =
        SharedAddress{.allocation = base.allocation, .byte_offset = base.byte_offset + offset};
    ++thread.pc;
    break;
  }
  case Opcode::AddRnF32:
    values[operation.result] = add_rn(std::get<float>(values[operation.inputs[0]]),
                                      std::get<float>(values[operation.inputs[1]]));
    ++thread.pc;
    break;
  case Opcode::SubRnF32:
    values[operation.result] = sub_rn(std::get<float>(values[operation.inputs[0]]),
                                      std::get<float>(values[operation.inputs[1]]));
    ++thread.pc;
    break;
  case Opcode::DivRnF32:
    values[operation.result] = div_rn(std::get<float>(values[operation.inputs[0]]),
                                      std::get<float>(values[operation.inputs[1]]));
    ++thread.pc;
    break;
  case Opcode::MultiplyRnF32:
    values[operation.result] = multiply_rn(std::get<float>(values[operation.inputs[0]]),
                                           std::get<float>(values[operation.inputs[1]]));
    ++thread.pc;
    break;
  case Opcode::MadRnF32:
  case Opcode::FmaRnF32:
    values[operation.result] = std::fma(std::get<float>(values[operation.inputs[0]]),
                                        std::get<float>(values[operation.inputs[1]]),
                                        std::get<float>(values[operation.inputs[2]]));
    ++thread.pc;
    break;
  case Opcode::ConvertRnF32U32: {
    volatile float converted =
        static_cast<float>(std::get<std::uint32_t>(values[operation.inputs[0]]));
    values[operation.result] = converted;
    ++thread.pc;
    break;
  }
  case Opcode::ConvertRnF32S32: {
    volatile float converted = static_cast<float>(
        static_cast<std::int32_t>(std::get<std::uint32_t>(values[operation.inputs[0]])));
    values[operation.result] = converted;
    ++thread.pc;
    break;
  }
  case Opcode::ConvertRziU32F32:
    values[operation.result] = convert_rzi_u32(std::get<float>(values[operation.inputs[0]]));
    ++thread.pc;
    break;
  case Opcode::SetPredicateGeU32:
    values[operation.result] = std::get<std::uint32_t>(values[operation.inputs[0]]) >=
                               std::get<std::uint32_t>(values[operation.inputs[1]]);
    ++thread.pc;
    break;
  case Opcode::SetPredicateGtS32:
    values[operation.result] = static_cast<std::int32_t>(std::get<std::uint32_t>(values[operation.inputs[0]])) >
                               static_cast<std::int32_t>(std::get<std::uint32_t>(values[operation.inputs[1]]));
    ++thread.pc;
    break;
  case Opcode::SelectU32:
    values[operation.result] = std::get<bool>(values[operation.inputs[2]])
                                   ? std::get<std::uint32_t>(values[operation.inputs[0]])
                                   : std::get<std::uint32_t>(values[operation.inputs[1]]);
    ++thread.pc;
    break;
  case Opcode::SelectF32:
    values[operation.result] = std::get<bool>(values[operation.inputs[2]])
                                   ? std::get<float>(values[operation.inputs[0]])
                                   : std::get<float>(values[operation.inputs[1]]);
    ++thread.pc;
    break;
  case Opcode::SetPredicateEqU32:
    values[operation.result] = std::get<std::uint32_t>(values[operation.inputs[0]]) ==
                               std::get<std::uint32_t>(values[operation.inputs[1]]);
    ++thread.pc;
    break;
  case Opcode::SetPredicateLtF32:
    values[operation.result] =
        std::get<float>(values[operation.inputs[0]]) < std::get<float>(values[operation.inputs[1]]);
    ++thread.pc;
    break;
  case Opcode::BranchIf: {
    const bool predicate = std::get<bool>(values[operation.inputs[0]]);
    const bool taken = operation.flag ? !predicate : predicate;
    thread.pc = taken ? operation.attribute : thread.pc + 1U;
    break;
  }
  case Opcode::LoadGlobalU32:
  case Opcode::LoadGlobalF32: {
    const auto address = std::get<GlobalAddress>(values[operation.inputs[0]]);
    std::uint32_t* word = nullptr;
    if (const auto error = global_word(address, arguments, false, word); error.has_value()) {
      return execution_failure(*error, thread.pc, coordinates);
    }
    if (operation.opcode == Opcode::LoadGlobalU32) {
      values[operation.result] = *word;
    } else {
      values[operation.result] = bit_cast_compatible<float>(*word);
    }
    ++thread.pc;
    break;
  }
  case Opcode::StoreGlobalU32:
  case Opcode::StoreGlobalF32: {
    const auto address = std::get<GlobalAddress>(values[operation.inputs[0]]);
    std::uint32_t* word = nullptr;
    if (const auto error = global_word(address, arguments, true, word); error.has_value()) {
      return execution_failure(*error, thread.pc, coordinates);
    }
    *word = operation.opcode == Opcode::StoreGlobalU32
                ? std::get<std::uint32_t>(values[operation.inputs[1]])
                : bit_cast_compatible<std::uint32_t>(std::get<float>(values[operation.inputs[1]]));
    ++thread.pc;
    break;
  }
  case Opcode::StoreGlobalU8: {
    const auto address = std::get<GlobalAddress>(values[operation.inputs[0]]);
    std::uint8_t* byte = nullptr;
    if (const auto error = global_byte(address, arguments, true, byte); error.has_value()) {
      return execution_failure(*error, thread.pc, coordinates);
    }
    *byte = static_cast<std::uint8_t>(std::get<std::uint32_t>(values[operation.inputs[1]]));
    ++thread.pc;
    break;
  }
  case Opcode::StoreGlobalU64: {
    const auto address = std::get<GlobalAddress>(values[operation.inputs[0]]);
    if (const auto error =
            store_global_u64(address, arguments, std::get<std::uint64_t>(values[operation.inputs[1]]));
        error.has_value()) {
      return execution_failure(*error, thread.pc, coordinates);
    }
    ++thread.pc;
    break;
  }
  case Opcode::LoadSharedU32: {
    const auto address = std::get<SharedAddress>(values[operation.inputs[0]]);
    std::uint32_t* word = nullptr;
    if (const auto error = shared_word(address, shared, word); error.has_value()) {
      return execution_failure(*error, thread.pc, coordinates);
    }
    values[operation.result] = *word;
    ++thread.pc;
    break;
  }
  case Opcode::StoreSharedU32: {
    const auto address = std::get<SharedAddress>(values[operation.inputs[0]]);
    std::uint32_t* word = nullptr;
    if (const auto error = shared_word(address, shared, word); error.has_value()) {
      return execution_failure(*error, thread.pc, coordinates);
    }
    *word = std::get<std::uint32_t>(values[operation.inputs[1]]);
    ++thread.pc;
    break;
  }
  case Opcode::BarrierSync:
    thread.waiting = true;
    break;
  case Opcode::Return:
    thread.returned = true;
    ++thread.pc;
    break;
  }
  return std::nullopt;
}

ExecutionResult execute_cta(const Kernel& kernel, std::span<const Argument> arguments,
                            LaunchDimensions launch, std::uint32_t block_x, std::uint32_t block_y,
                            std::stop_token cancellation) {
  std::vector<std::vector<std::uint32_t>> shared;
  shared.reserve(kernel.shared_words.size());
  for (const auto words : kernel.shared_words) {
    shared.emplace_back(words, 0U);
  }

  std::vector<ThreadState> threads;
  threads.reserve(static_cast<std::size_t>(launch.block_x) * launch.block_y);
  for (std::uint32_t thread_y = 0; thread_y < launch.block_y; ++thread_y) {
    for (std::uint32_t thread_x = 0; thread_x < launch.block_x; ++thread_x) {
      threads.push_back(ThreadState{.registers = std::vector<RuntimeValue>(kernel.registers.size()),
                                    .thread_x = thread_x,
                                    .thread_y = thread_y});
    }
  }

  while (true) {
    if (cancellation.stop_requested()) {
      return execution_failure(
          ExecutionError::Cancelled, kNoValue,
          Coordinates{.block_x = block_x, .block_y = block_y, .thread_x = 0, .thread_y = 0});
    }
    std::size_t returned = 0;
    std::size_t waiting = 0;
    bool progressed = false;
    for (auto& thread : threads) {
      if (thread.returned) {
        ++returned;
        continue;
      }
      if (thread.waiting) {
        ++waiting;
        continue;
      }
      const Coordinates coordinates{.block_x = block_x,
                                    .block_y = block_y,
                                    .thread_x = thread.thread_x,
                                    .thread_y = thread.thread_y};
      if (const auto failure = execute_one(kernel, arguments, launch, coordinates, thread, shared);
          failure.has_value()) {
        return *failure;
      }
      progressed = true;
      if (thread.returned) {
        ++returned;
      } else if (thread.waiting) {
        ++waiting;
      }
    }
    if (returned == threads.size()) {
      return ExecutionResult{};
    }
    if (waiting == threads.size()) {
      const auto barrier_pc = threads.front().pc;
      const bool same_barrier =
          std::all_of(threads.begin(), threads.end(), [&](const auto& thread) {
            return thread.waiting && thread.pc == barrier_pc;
          });
      if (!same_barrier) {
        return execution_failure(
            ExecutionError::BarrierDivergence, barrier_pc,
            Coordinates{.block_x = block_x, .block_y = block_y, .thread_x = 0, .thread_y = 0});
      }
      for (auto& thread : threads) {
        thread.waiting = false;
        ++thread.pc;
      }
      continue;
    }
    if (!progressed || (waiting != 0U && returned != 0U && waiting + returned == threads.size())) {
      const auto operation =
          waiting == 0U ? kNoValue
                        : std::find_if(threads.begin(), threads.end(), [](const auto& thread) {
                            return thread.waiting;
                          })->pc;
      return execution_failure(
          ExecutionError::BarrierDivergence, operation,
          Coordinates{.block_x = block_x, .block_y = block_y, .thread_x = 0, .thread_y = 0});
    }
  }
}

} // namespace

std::string_view execution_error_name(ExecutionError error) noexcept {
  switch (error) {
  case ExecutionError::InvalidArtifact:
    return "MF_CPU_INVALID_ARTIFACT";
  case ExecutionError::SchemaMismatch:
    return "MF_CPU_SCHEMA_MISMATCH";
  case ExecutionError::UnsupportedOperation:
    return "MF_CPU_UNSUPPORTED_OPERATION";
  case ExecutionError::InvalidLaunch:
    return "MF_CPU_INVALID_LAUNCH";
  case ExecutionError::ArgumentCount:
    return "MF_CPU_ARGUMENT_COUNT";
  case ExecutionError::ArgumentType:
    return "MF_CPU_ARGUMENT_TYPE";
  case ExecutionError::AddressOverflow:
    return "MF_CPU_ADDRESS_OVERFLOW";
  case ExecutionError::MisalignedAddress:
    return "MF_CPU_MISALIGNED_ADDRESS";
  case ExecutionError::OutOfBounds:
    return "MF_CPU_OUT_OF_BOUNDS";
  case ExecutionError::WriteToReadOnly:
    return "MF_CPU_WRITE_TO_READ_ONLY";
  case ExecutionError::BarrierDivergence:
    return "MF_CPU_BARRIER_DIVERGENCE";
  case ExecutionError::UnsupportedFpEnvironment:
    return "MF_CPU_UNSUPPORTED_FP_ENVIRONMENT";
  case ExecutionError::StepLimit:
    return "MF_CPU_STEP_LIMIT";
  case ExecutionError::PlacementUnavailable:
    return "MF_CPU_PLACEMENT_UNAVAILABLE";
  case ExecutionError::PlacementPinLost:
    return "MF_CPU_PLACEMENT_PIN_LOST";
  case ExecutionError::Cancelled:
    return "MF_CPU_CANCELLED";
  case ExecutionError::System:
    return "MF_CPU_SYSTEM_ERROR";
  }
  return "MF_CPU_UNKNOWN_ERROR";
}

ExecutionResult execute_kernel_ir(CpuExecutor& executor, std::string_view canonical_kernel_ir,
                                  std::span<const Argument> arguments, LaunchDimensions launch) {
  return execute_kernel_ir(executor, canonical_kernel_ir, arguments, launch, std::stop_token{});
}

ExecutionResult execute_kernel_ir(CpuExecutor& executor, std::string_view canonical_kernel_ir,
                                  std::span<const Argument> arguments, LaunchDimensions launch,
                                  std::stop_token cancellation) {
  if (cancellation.stop_requested()) {
    return ExecutionResult{.diagnostic = base_diagnostic(ExecutionError::Cancelled)};
  }
  auto parsed = parse_kernel(canonical_kernel_ir);
  if (!parsed.kernel.has_value()) {
    return ExecutionResult{.diagnostic = parsed.diagnostic};
  }
  if (std::any_of(parsed.kernel->operations.begin(), parsed.kernel->operations.end(),
                  [](const auto& operation) { return operation.opcode == Opcode::ExpF32; })) {
    const auto& math = host_math_helper();
    if (math.exp_f32 == nullptr) {
      return ExecutionResult{.diagnostic = base_diagnostic(ExecutionError::UnsupportedOperation)};
    }
    parsed.kernel->exp_f32 = math.exp_f32;
  }
  const auto& kernel = *parsed.kernel;
  if (launch.grid_x == 0U || launch.grid_y == 0U || launch.block_x == 0U || launch.block_y == 0U) {
    return ExecutionResult{.diagnostic = base_diagnostic(ExecutionError::InvalidLaunch)};
  }
  const auto threads_per_cta = static_cast<std::uint64_t>(launch.block_x) * launch.block_y;
  if (threads_per_cta > kMaximumThreadsPerCta ||
      launch.grid_x > kMaximumLogicalThreads / threads_per_cta) {
    return ExecutionResult{.diagnostic = base_diagnostic(ExecutionError::InvalidLaunch)};
  }
  const auto x_threads = threads_per_cta * launch.grid_x;
  if (launch.grid_y > kMaximumLogicalThreads / x_threads) {
    return ExecutionResult{.diagnostic = base_diagnostic(ExecutionError::InvalidLaunch)};
  }
  if (const auto error = validate_arguments(kernel, arguments); error.has_value()) {
    return ExecutionResult{.diagnostic = base_diagnostic(*error)};
  }
  if (has_fp_operations(kernel) && !fp_environment_supported()) {
    return ExecutionResult{.diagnostic = base_diagnostic(ExecutionError::UnsupportedFpEnvironment)};
  }
  const auto cta_count = static_cast<std::uint64_t>(launch.grid_x) * launch.grid_y;
  return executor.run_ctas(
      cta_count,
      [&, cancellation](std::uint64_t cta_index, std::uint32_t) {
        const auto block_x = static_cast<std::uint32_t>(cta_index % launch.grid_x);
        const auto block_y = static_cast<std::uint32_t>(cta_index / launch.grid_x);
        return execute_cta(kernel, arguments, launch, block_x, block_y, cancellation);
      },
      cancellation);
}

ExecutionResult execute_kernel_ir(std::string_view canonical_kernel_ir,
                                  std::span<const Argument> arguments, LaunchDimensions launch) {
  return execute_kernel_ir(default_cpu_executor(), canonical_kernel_ir, arguments, launch,
                           std::stop_token{});
}

ExecutionResult execute_kernel_ir(std::string_view canonical_kernel_ir,
                                  std::span<const Argument> arguments, LaunchDimensions launch,
                                  std::stop_token cancellation) {
  return execute_kernel_ir(default_cpu_executor(), canonical_kernel_ir, arguments, launch,
                           cancellation);
}

} // namespace metaflux::backend::cpu
