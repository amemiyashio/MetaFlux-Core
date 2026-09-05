#include "metaflux/backend/cpu/compiler.hpp"

#include "mlir/Dialect/LLVMIR/LLVMDialect.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/DialectRegistry.h"
#include "mlir/IR/MLIRContext.h"
#include "mlir/IR/Verifier.h"
#include "mlir/Parser/Parser.h"
#include "mlir/Target/LLVMIR/Dialect/Builtin/BuiltinToLLVMIRTranslation.h"
#include "mlir/Target/LLVMIR/Dialect/LLVMIR/LLVMToLLVMIRTranslation.h"
#include "mlir/Target/LLVMIR/Export.h"

#include "llvm/ADT/StringMap.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Passes/OptimizationLevel.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Support/CodeGen.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/TargetParser/Host.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <cstdlib>
#include <csignal>
#include <cstddef>
#include <cstdint>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <limits>
#include <memory>
#include <numeric>
#include <optional>
#include <sstream>
#include <stop_token>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <string_view>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#include <utility>
#include <vector>

namespace metaflux::backend::cpu::compiler {
namespace {

using metaflux::compiler::Kernel;
using metaflux::compiler::Opcode;
using metaflux::compiler::Operation;
using metaflux::compiler::ParameterKind;
using metaflux::compiler::SourceLocation;
using metaflux::compiler::SpecialRegister;
using metaflux::compiler::ValueKind;

constexpr std::uint64_t kMaximumGridCtas = 1U << 20U;
constexpr std::uint64_t kMaximumThreadsPerCta = 1024U;

enum class GeneratedStatus : std::uint32_t {
  Success = 0,
  InvalidLaunch = 1,
  ArgumentCount = 2,
  AddressOverflow = 3,
  MisalignedAddress = 4,
  OutOfBounds = 5,
  WriteToReadOnly = 6,
};

std::uint16_t read_u16(std::span<const std::byte> bytes, std::size_t offset) {
  return static_cast<std::uint16_t>(std::to_integer<std::uint8_t>(bytes[offset])) |
         static_cast<std::uint16_t>(
             static_cast<std::uint16_t>(std::to_integer<std::uint8_t>(bytes[offset + 1U])) << 8U);
}

std::uint32_t read_u32(std::span<const std::byte> bytes, std::size_t offset) {
  std::uint32_t result = 0;
  for (std::uint32_t index = 0; index < 4U; ++index) {
    result |= static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[offset + index]))
              << (index * 8U);
  }
  return result;
}

std::uint64_t read_u64(std::span<const std::byte> bytes, std::size_t offset) {
  std::uint64_t result = 0;
  for (std::uint32_t index = 0; index < 8U; ++index) {
    result |= static_cast<std::uint64_t>(std::to_integer<std::uint8_t>(bytes[offset + index]))
              << (index * 8U);
  }
  return result;
}

struct TemporaryDirectory {
  std::filesystem::path path;

  TemporaryDirectory() = default;
  explicit TemporaryDirectory(std::filesystem::path directory) : path(std::move(directory)) {}
  TemporaryDirectory(TemporaryDirectory&& other) noexcept : path(std::exchange(other.path, {})) {}
  TemporaryDirectory& operator=(TemporaryDirectory&& other) noexcept {
    if (this != &other) {
      path = std::exchange(other.path, {});
    }
    return *this;
  }
  TemporaryDirectory(const TemporaryDirectory&) = delete;
  TemporaryDirectory& operator=(const TemporaryDirectory&) = delete;

  ~TemporaryDirectory() {
    if (!path.empty()) {
      std::error_code ignored;
      std::filesystem::remove_all(path, ignored);
    }
  }
};

CompileResult failure(CompileError error, std::string message, SourceLocation location = {}) {
  return CompileResult{
      .artifact = std::nullopt,
      .diagnostic =
          CompileDiagnostic{
              .error = error,
              .location = location,
              .message = std::move(message),
          },
  };
}

bool cancelled(const CompileOptions& options) {
  return options.cancellation.stop_requested() ||
         (options.deadline.has_value() && std::chrono::steady_clock::now() >= *options.deadline);
}

std::string mlir_type(ValueKind kind) {
  switch (kind) {
  case ValueKind::Predicate:
    return "i8";
  case ValueKind::U32:
    return "i32";
  case ValueKind::U64:
  case ValueKind::GlobalAddress:
  case ValueKind::SharedAddress:
    return "i64";
  case ValueKind::F32:
    return "f32";
  }
  return "i32";
}

std::uint64_t storage_size(ValueKind kind) {
  switch (kind) {
  case ValueKind::Predicate:
    return 1U;
  case ValueKind::U32:
  case ValueKind::F32:
    return 4U;
  case ValueKind::U64:
  case ValueKind::GlobalAddress:
  case ValueKind::SharedAddress:
    return 8U;
  }
  return 0U;
}

bool uses_floating_point(const Kernel& kernel) {
  return std::any_of(kernel.operations.begin(), kernel.operations.end(), [](const Operation& op) {
    switch (op.opcode) {
    case Opcode::LoadParameterF32:
    case Opcode::AddRnF32:
    case Opcode::SubRnF32:
    case Opcode::MultiplyRnF32:
    case Opcode::MadRnF32:
    case Opcode::FmaRnF32:
    case Opcode::ConvertRnF32U32:
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

enum class ElementwiseKind : std::uint8_t {
  CopyU32,
  AddU32,
};

struct ElementwisePlan {
  ElementwiseKind kind = ElementwiseKind::CopyU32;
  std::uint32_t destination_parameter = 0;
  std::uint32_t left_parameter = 0;
  std::uint32_t right_parameter = 0;
  std::uint32_t count_parameter = 0;
};

bool inputs_are(const Operation& operation, std::initializer_list<std::uint32_t> expected) {
  if (operation.input_count != expected.size()) {
    return false;
  }
  std::size_t index = 0;
  for (const auto input : expected) {
    if (operation.inputs[index++] != input) {
      return false;
    }
  }
  return true;
}

bool plain_operation(const Operation& operation, Opcode opcode) {
  return operation.opcode == opcode && operation.predicate == metaflux::compiler::kNoValue &&
         !operation.predicate_negated && (opcode == Opcode::BranchIf || !operation.flag);
}

std::optional<ElementwisePlan> elementwise_plan(const Kernel& kernel) {
  const auto& operations = kernel.operations;
  const bool add = operations.size() == 19U && kernel.parameters.size() == 4U;
  const bool copy = operations.size() == 15U && kernel.parameters.size() == 3U;
  if ((!add && !copy) || !kernel.shared_allocations.empty()) {
    return std::nullopt;
  }

  const std::size_t source_count = add ? 2U : 1U;
  const std::size_t count_load = source_count + 1U;
  const std::size_t thread_id = count_load + 1U;
  const std::size_t block_id = thread_id + 1U;
  const std::size_t block_dim = block_id + 1U;
  const std::size_t global_id = block_dim + 1U;
  const std::size_t predicate = global_id + 1U;
  const std::size_t branch = predicate + 1U;
  const std::size_t byte_offset = branch + 1U;
  const std::size_t destination_address = byte_offset + 1U;
  const std::size_t left_address = destination_address + 1U;
  const std::size_t right_address = add ? left_address + 1U : 0U;
  const std::size_t left_load = add ? right_address + 1U : left_address + 1U;
  const std::size_t right_load = add ? left_load + 1U : 0U;
  const std::size_t value = add ? right_load + 1U : left_load;
  const std::size_t store = value + 1U;
  const std::size_t return_index = store + 1U;

  for (std::size_t index = 0; index < source_count + 1U; ++index) {
    if (!plain_operation(operations[index], Opcode::LoadParameterAddress) ||
        operations[index].attribute >= kernel.parameters.size() ||
        kernel.parameters[operations[index].attribute].kind != ParameterKind::BufferU32) {
      return std::nullopt;
    }
  }
  if (!plain_operation(operations[count_load], Opcode::LoadParameterU32) ||
      operations[count_load].attribute >= kernel.parameters.size() ||
      kernel.parameters[operations[count_load].attribute].kind != ParameterKind::ScalarU32 ||
      !plain_operation(operations[thread_id], Opcode::MoveSpecialU32) ||
      operations[thread_id].attribute != static_cast<std::uint32_t>(SpecialRegister::ThreadIdX) ||
      !plain_operation(operations[block_id], Opcode::MoveSpecialU32) ||
      operations[block_id].attribute != static_cast<std::uint32_t>(SpecialRegister::BlockIdX) ||
      !plain_operation(operations[block_dim], Opcode::MoveSpecialU32) ||
      operations[block_dim].attribute != static_cast<std::uint32_t>(SpecialRegister::BlockDimX) ||
      !plain_operation(operations[global_id], Opcode::MadLoU32) ||
      !inputs_are(operations[global_id], {operations[block_id].result, operations[block_dim].result,
                                          operations[thread_id].result}) ||
      !plain_operation(operations[predicate], Opcode::SetPredicateGeU32) ||
      !inputs_are(operations[predicate],
                  {operations[global_id].result, operations[count_load].result}) ||
      !plain_operation(operations[branch], Opcode::BranchIf) || operations[branch].flag ||
      operations[branch].attribute != return_index ||
      !inputs_are(operations[branch], {operations[predicate].result}) ||
      !plain_operation(operations[byte_offset], Opcode::MultiplyWideU32) ||
      operations[byte_offset].attribute != 4U ||
      !inputs_are(operations[byte_offset], {operations[global_id].result}) ||
      !plain_operation(operations[destination_address], Opcode::AddGlobalAddress) ||
      !inputs_are(operations[destination_address],
                  {operations[0].result, operations[byte_offset].result}) ||
      !plain_operation(operations[left_address], Opcode::AddGlobalAddress) ||
      !inputs_are(operations[left_address],
                  {operations[1].result, operations[byte_offset].result}) ||
      !plain_operation(operations[left_load], Opcode::LoadGlobalU32) ||
      !inputs_are(operations[left_load], {operations[left_address].result})) {
    return std::nullopt;
  }

  if (add && (!plain_operation(operations[right_address], Opcode::AddGlobalAddress) ||
              !inputs_are(operations[right_address],
                          {operations[2].result, operations[byte_offset].result}) ||
              !plain_operation(operations[right_load], Opcode::LoadGlobalU32) ||
              !inputs_are(operations[right_load], {operations[right_address].result}) ||
              !plain_operation(operations[value], Opcode::AddU32) ||
              !inputs_are(operations[value],
                          {operations[left_load].result, operations[right_load].result}))) {
    return std::nullopt;
  }
  if (!plain_operation(operations[store], Opcode::StoreGlobalU32) ||
      !inputs_are(operations[store],
                  {operations[destination_address].result, operations[value].result}) ||
      !plain_operation(operations[return_index], Opcode::Return) ||
      operations[return_index].input_count != 0U) {
    return std::nullopt;
  }

  return ElementwisePlan{
      .kind = add ? ElementwiseKind::AddU32 : ElementwiseKind::CopyU32,
      .destination_parameter = operations[0].attribute,
      .left_parameter = operations[1].attribute,
      .right_parameter = add ? operations[2].attribute : 0U,
      .count_parameter = operations[count_load].attribute,
  };
}

class MlirEmitter {
public:
  explicit MlirEmitter(const Kernel& kernel)
      : kernel_(kernel), elementwise_(elementwise_plan(kernel)) {
    origins_.resize(kernel.registers.size());
    compute_address_origins();
    compute_shared_offsets();
  }

  [[nodiscard]] std::string emit() {
    output_ << "module attributes {llvm.target_triple = \"x86_64-unknown-linux-gnu\"} {\n";
    if (std::any_of(kernel_.operations.begin(), kernel_.operations.end(),
                    [](const Operation& op) { return op.opcode == Opcode::ConvertRziU32F32; })) {
      output_ << "  llvm.func @llvm.fptoui.sat.i32.f32(f32) -> i32\n";
    }
    output_ << "  llvm.func @" << kCpuCompiledEntrySymbol
            << "(%buffers: !llvm.ptr, %sizes: !llvm.ptr, %writable: !llvm.ptr, "
               "%scalars: !llvm.ptr, %argc: i32, %cta_linear: i32, %grid_x: i32, %block_x: i32, "
               "%grid_y: i32, %block_y: i32) -> i32 {\n";

    const auto entry = emit_entry_and_allocations();
    emit_cta_pipeline(entry);
    emit_error_blocks(entry);
    output_ << "  }\n}\n";
    return output_.str();
  }

private:
  enum class OriginKind : std::uint8_t {
    None,
    Global,
    Shared,
  };

  struct AddressOrigin {
    OriginKind kind = OriginKind::None;
    std::uint32_t index = 0;
  };

  struct EntryState {
    std::string zero_i32;
    std::string one_i32;
    std::string true_i1;
    std::string zero_i64;
    std::string three_i64;
    std::string block_threads;
    std::string grid_ctas;
    std::string active_storage;
    std::string shared_storage;
    std::vector<std::string> register_storage;
    std::string argument_error;
    std::string launch_error;
    std::string overflow_error;
    std::string alignment_error;
    std::string bounds_error;
    std::string readonly_error;
    std::string success;
  };

  const Kernel& kernel_;
  std::ostringstream output_;
  std::uint64_t next_value_ = 0;
  std::uint64_t next_block_ = 0;
  std::vector<AddressOrigin> origins_;
  std::vector<std::uint64_t> shared_offsets_;
  std::optional<ElementwisePlan> elementwise_;
  // Per-phase map of registers promoted from per-lane array storage to LLVM SSA
  // names. Only valid while the owning segment's lane loop is being emitted.
  std::unordered_map<std::uint32_t, std::string> promoted_;
  // Per-phase set of registers whose value advances one word per lane, making
  // global accesses over them stride-one contiguous.
  std::unordered_set<std::uint32_t> linear_registers_;

  [[nodiscard]] std::string value() { return "%v" + std::to_string(next_value_++); }
  [[nodiscard]] std::string block() { return "^b" + std::to_string(next_block_++); }

  void line(std::string_view text) { output_ << "    " << text << '\n'; }
  void declare_block(std::string_view name) { output_ << "  " << name << ":\n"; }

  [[nodiscard]] std::string source_location(const Operation& operation) const {
    return " loc(\"ptx:" + kernel_.name + "\":" + std::to_string(operation.location.line) + ":" +
           std::to_string(operation.location.column) + ")";
  }

  [[nodiscard]] std::string constant_i32(std::uint32_t number) {
    const auto result = value();
    line(result + " = llvm.mlir.constant(" + std::to_string(number) + " : i32) : i32");
    return result;
  }

  [[nodiscard]] std::string constant_i64(std::uint64_t number) {
    const auto result = value();
    line(result + " = llvm.mlir.constant(" + std::to_string(number) + " : i64) : i64");
    return result;
  }

  [[nodiscard]] std::string constant_i8(std::uint32_t number) {
    const auto result = value();
    line(result + " = llvm.mlir.constant(" + std::to_string(number) + " : i8) : i8");
    return result;
  }

  [[nodiscard]] std::string constant_f64_zero() {
    const auto result = value();
    line(result + " = llvm.mlir.constant(0.0 : f64) : f64");
    return result;
  }

  [[nodiscard]] std::string binary(std::string_view opcode, std::string_view left,
                                   std::string_view right, std::string_view type,
                                   const Operation* operation = nullptr) {
    const auto result = value();
    line(result + " = llvm." + std::string(opcode) + " " + std::string(left) + ", " +
         std::string(right) + " : " + std::string(type) +
         (operation == nullptr ? std::string{} : source_location(*operation)));
    return result;
  }

  [[nodiscard]] std::string compare(std::string_view predicate, std::string_view left,
                                    std::string_view right, std::string_view type,
                                    const Operation* operation = nullptr, bool floating = false) {
    const auto result = value();
    line(result + " = llvm." + (floating ? "fcmp" : "icmp") + " \"" + std::string(predicate) +
         "\" " + std::string(left) + ", " + std::string(right) + " : " + std::string(type) +
         (operation == nullptr ? std::string{} : source_location(*operation)));
    return result;
  }

  [[nodiscard]] std::string cast(std::string_view opcode, std::string_view input,
                                 std::string_view source_type, std::string_view target_type,
                                 const Operation* operation = nullptr) {
    const auto result = value();
    line(result + " = llvm." + std::string(opcode) + " " + std::string(input) + " : " +
         std::string(source_type) + " to " + std::string(target_type) +
         (operation == nullptr ? std::string{} : source_location(*operation)));
    return result;
  }

  [[nodiscard]] std::string select(std::string_view condition, std::string_view true_value,
                                   std::string_view false_value, std::string_view type,
                                   const Operation* operation = nullptr,
                                   std::string_view condition_type = "i1") {
    const auto result = value();
    line(result + " = llvm.select " + std::string(condition) + ", " + std::string(true_value) +
         ", " + std::string(false_value) + " : " + std::string(condition_type) + ", " +
         std::string(type) +
         (operation == nullptr ? std::string{} : source_location(*operation)));
    return result;
  }

  [[nodiscard]] std::string gep(std::string_view base, std::string_view index,
                                std::string_view element_type) {
    const auto result = value();
    line(result + " = llvm.getelementptr " + std::string(base) + "[" + std::string(index) +
         "] : (!llvm.ptr, i64) -> !llvm.ptr, " + std::string(element_type));
    return result;
  }

  [[nodiscard]] std::string gep_constant(std::string_view base, std::uint32_t index,
                                         std::string_view element_type) {
    const auto result = value();
    line(result + " = llvm.getelementptr " + std::string(base) + "[" + std::to_string(index) +
         "] : (!llvm.ptr) -> !llvm.ptr, " + std::string(element_type));
    return result;
  }

  [[nodiscard]] std::string load(std::string_view pointer, std::string_view type,
                                 const Operation* operation = nullptr) {
    const auto result = value();
    line(result + " = llvm.load " + std::string(pointer) + " : !llvm.ptr -> " + std::string(type) +
         (operation == nullptr ? std::string{} : source_location(*operation)));
    return result;
  }

  // The pure-operation SIMD region shape: every emitted value covers eight
  // consecutive lanes and lives only in SSA form; array transfers happen once
  // at the region boundary through masked loads and stores.
  static constexpr std::string_view kVectorRegionLanesText = "8";

  [[nodiscard]] static std::string vtype(std::string_view scalar_type) {
    return "vector<" + std::string(kVectorRegionLanesText) + "x" + std::string(scalar_type) +
           ">";
  }

  [[nodiscard]] std::string constant_splat_i32(std::uint32_t number) {
    const auto result = value();
    line(result + " = llvm.mlir.constant(dense<" + std::to_string(number) + "> : " +
         vtype("i32") + ") : " + vtype("i32"));
    return result;
  }

  [[nodiscard]] std::string constant_splat_i64(std::uint64_t number) {
    const auto result = value();
    line(result + " = llvm.mlir.constant(dense<" + std::to_string(number) + "> : " +
         vtype("i64") + ") : " + vtype("i64"));
    return result;
  }

  [[nodiscard]] std::string constant_splat_f64_zero() {
    const auto result = value();
    line(result + " = llvm.mlir.constant(dense<0.0> : " + vtype("f64") + ") : " + vtype("f64"));
    return result;
  }

  [[nodiscard]] std::string constant_true_mask() {
    const auto result = value();
    line(result + " = llvm.mlir.constant(dense<true> : " + vtype("i1") + ") : " + vtype("i1"));
    return result;
  }

  [[nodiscard]] std::string constant_lane_offsets() {
    const auto result = value();
    line(result + " = llvm.mlir.constant(dense<[0, 1, 2, 3, 4, 5, 6, 7]> : " + vtype("i32") +
         ") : " + vtype("i32"));
    return result;
  }

  [[nodiscard]] std::string undef_vector(std::string_view scalar_type) {
    const auto result = value();
    line(result + " = llvm.mlir.undef : " + vtype(scalar_type));
    return result;
  }

  [[nodiscard]] std::string splat_scalar(std::string_view scalar, std::string_view scalar_type) {
    const auto vector_type_text = vtype(scalar_type);
    auto accumulated = undef_vector(scalar_type);
    for (std::uint32_t lane = 0; lane < 8U; ++lane) {
      const auto index = constant_i32(lane);
      const auto index64 = cast("zext", index, "i32", "i64");
      const auto inserted = value();
      line(inserted + " = llvm.insertelement " + std::string(scalar) + ", " + accumulated +
           "[" + index64 + " : i64] : " + vector_type_text);
      accumulated = inserted;
    }
    return accumulated;
  }

  [[nodiscard]] std::string masked_load(std::string_view pointer, std::string_view scalar_type,
                                        std::string_view mask, std::uint32_t alignment) {
    const auto result = value();
    line(result + " = llvm.intr.masked.load " + std::string(pointer) + ", " + std::string(mask) +
         ", " + undef_vector(scalar_type) + " {alignment = " + std::to_string(alignment) +
         " : i32} : (!llvm.ptr, " + vtype("i1") + ", " + vtype(scalar_type) + ") -> " +
         vtype(scalar_type));
    return result;
  }

  void masked_store(std::string_view stored_value, std::string_view pointer,
                    std::string_view scalar_type, std::string_view mask,
                    std::uint32_t alignment) {
    line("llvm.intr.masked.store " + std::string(stored_value) + ", " + std::string(pointer) +
         ", " + std::string(mask) + " {alignment = " + std::to_string(alignment) +
         " : i32} : " + vtype(scalar_type) + ", " + vtype("i1") + " into !llvm.ptr");
  }

  [[nodiscard]] std::string vector_reduce_or(std::string_view mask) {
    const auto result = value();
    line(result + " = \"llvm.intr.vector.reduce.or\"(" + std::string(mask) + ") : (" +
         vtype("i1") + ") -> i1");
    return result;
  }

  [[nodiscard]] std::string extract_lane_zero(std::string_view vector_value,
                                              std::string_view scalar_type) {
    const auto index = constant_i32(0U);
    const auto index64 = cast("zext", index, "i32", "i64");
    const auto result = value();
    line(result + " = llvm.extractelement " + std::string(vector_value) + "[" + index64 +
         " : i64] : " + vtype(scalar_type));
    return result;
  }

  void store(std::string_view stored_value, std::string_view pointer, std::string_view type,
             const Operation* operation = nullptr) {
    line("llvm.store " + std::string(stored_value) + ", " + std::string(pointer) + " : " +
         std::string(type) + ", !llvm.ptr" +
         (operation == nullptr ? std::string{} : source_location(*operation)));
  }

  void compute_address_origins() {
    for (const auto& operation : kernel_.operations) {
      if (operation.result >= origins_.size()) {
        continue;
      }
      switch (operation.opcode) {
      case Opcode::LoadParameterAddress:
        origins_[operation.result] = {.kind = OriginKind::Global, .index = operation.attribute};
        break;
      case Opcode::LoadSharedAddress:
        origins_[operation.result] = {.kind = OriginKind::Shared, .index = operation.attribute};
        break;
      case Opcode::AddGlobalAddress:
      case Opcode::AddSharedAddress:
        origins_[operation.result] = origins_[operation.inputs[0]];
        break;
      default:
        break;
      }
    }
  }

  void compute_shared_offsets() {
    shared_offsets_.reserve(kernel_.shared_allocations.size());
    std::uint64_t offset = 0;
    for (const auto& allocation : kernel_.shared_allocations) {
      shared_offsets_.push_back(offset);
      offset += allocation.words;
    }
  }

  [[nodiscard]] EntryState emit_entry_and_allocations() {
    EntryState state;
    state.argument_error = block();
    state.launch_error = block();
    state.overflow_error = block();
    state.alignment_error = block();
    state.bounds_error = block();
    state.readonly_error = block();
    state.success = block();

    state.zero_i32 = constant_i32(0U);
    state.one_i32 = constant_i32(1U);
    const auto one_i8 = constant_i8(1U);
    state.true_i1 = cast("trunc", one_i8, "i8", "i1");
    state.zero_i64 = constant_i64(0U);
    state.three_i64 = constant_i64(3U);
    const auto expected_arguments =
        constant_i32(static_cast<std::uint32_t>(kernel_.parameters.size()));
    const auto argument_count_ok = compare("eq", "%argc", expected_arguments, "i32");
    const auto validate_launch = block();
    line("llvm.cond_br " + argument_count_ok + ", " + validate_launch + ", " +
         state.argument_error);
    declare_block(validate_launch);

    const auto grid_x_zero = compare("eq", "%grid_x", state.zero_i32, "i32");
    const auto grid_y_zero = compare("eq", "%grid_y", state.zero_i32, "i32");
    const auto block_x_zero = compare("eq", "%block_x", state.zero_i32, "i32");
    const auto block_y_zero = compare("eq", "%block_y", state.zero_i32, "i32");
    const auto grid_zero = binary("or", grid_x_zero, grid_y_zero, "i1");
    const auto block_zero = binary("or", block_x_zero, block_y_zero, "i1");
    const auto any_zero = binary("or", grid_zero, block_zero, "i1");
    const auto grid_x64 = cast("zext", "%grid_x", "i32", "i64");
    const auto grid_y64 = cast("zext", "%grid_y", "i32", "i64");
    const auto block_x64 = cast("zext", "%block_x", "i32", "i64");
    const auto block_y64 = cast("zext", "%block_y", "i32", "i64");
    const auto grid_product64 = binary("mul", grid_x64, grid_y64, "i64");
    const auto block_product64 = binary("mul", block_x64, block_y64, "i64");
    state.grid_ctas = cast("trunc", grid_product64, "i64", "i32");
    const auto maximum_grid = constant_i64(kMaximumGridCtas);
    const auto maximum_block = constant_i64(kMaximumThreadsPerCta);
    const auto grid_in_range = compare("ule", grid_product64, maximum_grid, "i64");
    const auto block_in_range = compare("ule", block_product64, maximum_block, "i64");
    const auto ranges_ok = binary("and", grid_in_range, block_in_range, "i1");
    const auto nonzero = binary("xor", any_zero, state.true_i1, "i1");
    const auto dimensions_ok = binary("and", nonzero, ranges_ok, "i1");
    const auto cta_in_range = compare("ult", "%cta_linear", state.grid_ctas, "i32");
    const auto launch_ok = binary("and", dimensions_ok, cta_in_range, "i1");
    const auto setup = block();
    line("llvm.cond_br " + launch_ok + ", " + setup + ", " + state.launch_error);
    declare_block(setup);

    state.block_threads = cast("trunc", block_product64, "i64", "i32");
    state.active_storage = value();
    line(state.active_storage + " = llvm.alloca " + state.block_threads +
         " x i8 : (i32) -> !llvm.ptr");
    state.register_storage.reserve(kernel_.registers.size());
    for (const auto& reg : kernel_.registers) {
      const auto storage = value();
      line(storage + " = llvm.alloca " + state.block_threads + " x " + mlir_type(reg.kind) +
           " : (i32) -> !llvm.ptr");
      state.register_storage.push_back(storage);
    }
    const auto shared_words = std::max<std::uint64_t>(
        1U, std::accumulate(kernel_.shared_allocations.begin(), kernel_.shared_allocations.end(),
                            std::uint64_t{0}, [](std::uint64_t total, const auto& allocation) {
                              return total + allocation.words;
                            }));
    const auto shared_count = constant_i32(static_cast<std::uint32_t>(shared_words));
    state.shared_storage = value();
    line(state.shared_storage + " = llvm.alloca " + shared_count + " x i32 : (i32) -> !llvm.ptr");
    return state;
  }

  void emit_cta_pipeline(const EntryState& state) {
    const auto block_coordinate_x = binary("urem", "%cta_linear", "%grid_x", "i32");
    const auto block_coordinate_y = binary("udiv", "%cta_linear", "%grid_x", "i32");

    if (elementwise_.has_value()) {
      const auto vector_path = block();
      const auto generic_path = block();
      const auto block_y_is_one = compare("eq", "%block_y", state.one_i32, "i32");
      line("llvm.cond_br " + block_y_is_one + ", " + vector_path + ", " + generic_path);
      declare_block(vector_path);
      emit_elementwise_lane_loop(state, *elementwise_, block_coordinate_x);
      declare_block(generic_path);
    }

    const auto active_initialized = emit_active_initialization(state);
    declare_block(active_initialized);
    const auto shared_initialized = emit_shared_initialization(state);
    declare_block(shared_initialized);

    auto phase_done = shared_initialized;
    const auto phases = operation_phases();
    for (const auto& phase : phases) {
      if (phase.empty()) {
        continue;
      }
      phase_done = emit_phase(state, phase, block_coordinate_x, block_coordinate_y);
      declare_block(phase_done);
    }
    line("llvm.br " + state.success);
  }

  void emit_elementwise_lane_loop(const EntryState& state, const ElementwisePlan& plan,
                                  std::string_view block_coordinate_x) {
    const auto count_pointer = gep_constant("%scalars", plan.count_parameter, "i32");
    const auto count = load(count_pointer, "i32");
    const auto start = binary("mul", block_coordinate_x, "%block_x", "i32");
    const auto starts_before_end = compare("ult", start, count, "i32");
    const auto unchecked_remaining = binary("sub", count, start, "i32");
    const auto remaining = select(starts_before_end, unchecked_remaining, state.zero_i32, "i32");
    const auto block_is_shorter = compare("ult", "%block_x", remaining, "i32");
    const auto active_lanes = select(block_is_shorter, "%block_x", remaining, "i32");
    const auto has_active_lanes = compare("ne", active_lanes, state.zero_i32, "i32");
    const auto validate_ranges = block();
    line("llvm.cond_br " + has_active_lanes + ", " + validate_ranges + ", " + state.success);
    declare_block(validate_ranges);

    const auto start64 = cast("zext", start, "i32", "i64");
    const auto active64 = cast("zext", active_lanes, "i32", "i64");
    const auto end64 = binary("add", start64, active64, "i64");
    std::vector<std::uint32_t> sources{plan.left_parameter};
    if (plan.kind == ElementwiseKind::AddU32) {
      sources.push_back(plan.right_parameter);
    }
    auto source_ranges_ok = state.true_i1;
    for (const auto parameter : sources) {
      const auto size_pointer = gep_constant("%sizes", parameter, "i64");
      const auto size = load(size_pointer, "i64");
      const auto fits = compare("ule", end64, size, "i64");
      source_ranges_ok = binary("and", source_ranges_ok, fits, "i1");
    }
    const auto validate_writable = block();
    line("llvm.cond_br " + source_ranges_ok + ", " + validate_writable + ", " + state.bounds_error);
    declare_block(validate_writable);
    const auto writable_pointer = gep_constant("%writable", plan.destination_parameter, "i32");
    const auto writable = load(writable_pointer, "i32");
    const auto is_writable = compare("ne", writable, state.zero_i32, "i32");
    const auto validate_destination_range = block();
    line("llvm.cond_br " + is_writable + ", " + validate_destination_range + ", " +
         state.readonly_error);
    declare_block(validate_destination_range);
    const auto destination_size_pointer = gep_constant("%sizes", plan.destination_parameter, "i64");
    const auto destination_size = load(destination_size_pointer, "i64");
    const auto destination_fits = compare("ule", end64, destination_size, "i64");
    const auto loop_entry = block();
    line("llvm.cond_br " + destination_fits + ", " + loop_entry + ", " + state.bounds_error);
    declare_block(loop_entry);

    const auto load_buffer_base = [&](std::uint32_t parameter) {
      const auto address_pointer = gep_constant("%buffers", parameter, "i64");
      const auto address = load(address_pointer, "i64");
      return cast("inttoptr", address, "i64", "!llvm.ptr");
    };
    const auto destination = load_buffer_base(plan.destination_parameter);
    const auto left = load_buffer_base(plan.left_parameter);
    const auto right = plan.kind == ElementwiseKind::AddU32 ? load_buffer_base(plan.right_parameter)
                                                            : std::string{};

    const auto loop_header = block();
    const auto loop_body = block();
    const auto lane = value();
    line("llvm.br " + loop_header + "(" + state.zero_i32 + " : i32)");
    output_ << "  " << loop_header << "(" << lane << ": i32):\n";
    const auto lane_in_range = compare("ult", lane, active_lanes, "i32");
    line("llvm.cond_br " + lane_in_range + ", " + loop_body + ", " + state.success);
    declare_block(loop_body);
    const auto lane64 = cast("zext", lane, "i32", "i64");
    const auto global_index = binary("add", start64, lane64, "i64");
    const auto left_value = load(gep(left, global_index, "i32"), "i32");
    auto result = left_value;
    if (plan.kind == ElementwiseKind::AddU32) {
      const auto right_value = load(gep(right, global_index, "i32"), "i32");
      result = binary("add", left_value, right_value, "i32");
    }
    store(result, gep(destination, global_index, "i32"), "i32");
    const auto next_lane = binary("add", lane, state.one_i32, "i32");
    line("llvm.br " + loop_header + "(" + next_lane + " : i32)");
  }

  [[nodiscard]] std::string emit_active_initialization(const EntryState& state) {
    const auto header = block();
    const auto body = block();
    const auto done = block();
    const auto lane = value();
    line("llvm.br " + header + "(" + state.zero_i32 + " : i32)");
    output_ << "  " << header << "(" << lane << ": i32):\n";
    const auto in_range = compare("ult", lane, state.block_threads, "i32");
    line("llvm.cond_br " + in_range + ", " + body + ", " + done);
    declare_block(body);
    const auto lane64 = cast("zext", lane, "i32", "i64");
    const auto active_pointer = gep(state.active_storage, lane64, "i8");
    const auto one_i8 = constant_i8(1U);
    store(one_i8, active_pointer, "i8");
    const auto next = binary("add", lane, state.one_i32, "i32");
    line("llvm.br " + header + "(" + next + " : i32)");
    return done;
  }

  [[nodiscard]] std::string emit_shared_initialization(const EntryState& state) {
    const auto header = block();
    const auto body = block();
    const auto done = block();
    const auto word = value();
    const auto total_words = std::accumulate(
        kernel_.shared_allocations.begin(), kernel_.shared_allocations.end(), std::uint64_t{0},
        [](std::uint64_t total, const auto& allocation) { return total + allocation.words; });
    if (total_words == 0U) {
      line("llvm.br " + done);
      return done;
    }
    const auto total = constant_i32(static_cast<std::uint32_t>(total_words));
    line("llvm.br " + header + "(" + state.zero_i32 + " : i32)");
    output_ << "  " << header << "(" << word << ": i32):\n";
    const auto in_range = compare("ult", word, total, "i32");
    line("llvm.cond_br " + in_range + ", " + body + ", " + done);
    declare_block(body);
    const auto word64 = cast("zext", word, "i32", "i64");
    const auto pointer = gep(state.shared_storage, word64, "i32");
    store(state.zero_i32, pointer, "i32");
    const auto next = binary("add", word, state.one_i32, "i32");
    line("llvm.br " + header + "(" + next + " : i32)");
    return done;
  }

  [[nodiscard]] std::vector<std::vector<std::size_t>> operation_phases() const {
    std::vector<std::vector<std::size_t>> phases(1);
    for (std::size_t index = 0; index < kernel_.operations.size(); ++index) {
      switch (kernel_.operations[index].opcode) {
      case Opcode::BarrierSync:
        phases.emplace_back();
        break;
      case Opcode::Return:
        break;
      default:
        phases.back().push_back(index);
        break;
      }
    }
    return phases;
  }

  // A SIMD region may contain unpredicated pure operations, which are
  // unobservable on inactive lanes, plus stride-one global loads and stores
  // whose checks and transfers vectorize with the group's effective mask.
  [[nodiscard]] bool vectorizable_region_opcode(const Operation& operation) const {
    if (operation.predicate != metaflux::compiler::kNoValue) {
      return false;
    }
    switch (operation.opcode) {
    case Opcode::LoadParameterAddress:
    case Opcode::LoadParameterU32:
    case Opcode::LoadParameterF32:
    case Opcode::LoadSharedAddress:
    case Opcode::MoveSpecialU32:
    case Opcode::AddU32:
    case Opcode::SubU32:
    case Opcode::MultiplyLoU32:
    case Opcode::MadLoU32:
    case Opcode::MultiplyWideU32:
    case Opcode::AddSharedAddress:
    case Opcode::AddGlobalAddress:
    case Opcode::AddRnF32:
    case Opcode::SubRnF32:
    case Opcode::MultiplyRnF32:
    case Opcode::MadRnF32:
    case Opcode::FmaRnF32:
    case Opcode::SetPredicateGeU32:
    case Opcode::SetPredicateEqU32:
    case Opcode::SetPredicateLtF32:
      return true;
    case Opcode::LoadGlobalU32:
    case Opcode::LoadGlobalF32:
    case Opcode::StoreGlobalU32:
    case Opcode::StoreGlobalF32:
      return linear_registers_.count(operation.inputs[0]) != 0U;
    default:
      return false;
    }
  }

  struct PhaseSegment {
    bool vectorized;
    std::vector<std::size_t> operations;
  };

  [[nodiscard]] std::vector<PhaseSegment> segment_phase(
      const std::vector<std::size_t>& operations) const {
    constexpr std::size_t kMinimumRegionOperations = 3;
    std::vector<PhaseSegment> segments;
    std::vector<std::size_t> scalar;
    std::size_t index = 0;
    while (index < operations.size()) {
      std::vector<std::size_t> region;
      while (index < operations.size() &&
             vectorizable_region_opcode(kernel_.operations[operations[index]])) {
        region.push_back(operations[index]);
        ++index;
      }
      if (region.size() >= kMinimumRegionOperations) {
        if (!scalar.empty()) {
          segments.push_back({false, scalar});
          scalar.clear();
        }
        segments.push_back({true, std::move(region)});
      } else {
        scalar.insert(scalar.end(), region.begin(), region.end());
      }
      if (index < operations.size()) {
        scalar.push_back(operations[index]);
        ++index;
      }
    }
    if (!scalar.empty()) {
      segments.push_back({false, std::move(scalar)});
    }
    return segments;
  }

  [[nodiscard]] std::string emit_scalar_segment(const EntryState& state,
                                                const std::vector<std::size_t>& operations,
                                                std::string_view block_coordinate_x,
                                                std::string_view block_coordinate_y) {
    const auto header = block();
    const auto body = block();
    const auto done = block();
    const auto lane = value();
    line("llvm.br " + header + "(" + state.zero_i32 + " : i32)");
    output_ << "  " << header << "(" << lane << ": i32):\n";
    const auto in_range = compare("ult", lane, state.block_threads, "i32");
    line("llvm.cond_br " + in_range + ", " + body + ", " + done);
    declare_block(body);
    const auto lane64 = cast("zext", lane, "i32", "i64");
    const auto thread_x = binary("urem", lane, "%block_x", "i32");
    const auto thread_y = binary("udiv", lane, "%block_x", "i32");
    for (const auto index : operations) {
      emit_operation(state, kernel_.operations[index], lane64, thread_x, thread_y,
                     block_coordinate_x, block_coordinate_y);
    }
    const auto next = binary("add", lane, state.one_i32, "i32");
    line("llvm.br " + header + "(" + next + " : i32)");
    return done;
  }

  [[nodiscard]] std::string fma_rn_vector(const std::vector<std::string>& inputs,
                                          const Operation& operation) {
    const auto v64 = vtype("f64");
    const auto v32 = vtype("f32");
    const auto vi64 = vtype("i64");
    const auto v1 = vtype("i1");
    const auto left = cast("fpext", inputs[0], v32, v64, &operation);
    const auto right = cast("fpext", inputs[1], v32, v64, &operation);
    const auto addend = cast("fpext", inputs[2], v32, v64, &operation);
    const auto product = binary("fmul", left, right, v64, &operation);
    const auto sum = binary("fadd", product, addend, v64, &operation);
    const auto rounded_addend = binary("fsub", sum, product, v64, &operation);
    const auto recovered_product = binary("fsub", sum, rounded_addend, v64, &operation);
    const auto product_error = binary("fsub", product, recovered_product, v64, &operation);
    const auto addend_error = binary("fsub", addend, rounded_addend, v64, &operation);
    const auto error = binary("fadd", product_error, addend_error, v64, &operation);

    const auto bits = cast("bitcast", sum, v64, vi64, &operation);
    const auto low_mask = constant_splat_i64(0x1fffffffU);
    const auto low = binary("and", bits, low_mask, vi64, &operation);
    const auto halfway_bit = constant_splat_i64(0x10000000U);
    const auto halfway = compare("eq", low, halfway_bit, vi64, &operation);
    const auto exponent_mask = constant_splat_i64(0x7ff0000000000000ULL);
    const auto exponent = binary("and", bits, exponent_mask, vi64, &operation);
    const auto finite = compare("ne", exponent, exponent_mask, vi64, &operation);
    const auto zero_f64 = constant_splat_f64_zero();
    const auto inexact = compare("one", error, zero_f64, v64, &operation, true);
    const auto adjust_halfway = binary("and", halfway, finite, v1, &operation);
    const auto adjust = binary("and", adjust_halfway, inexact, v1, &operation);

    const auto error_positive = compare("ogt", error, zero_f64, v64, &operation, true);
    const auto one_i64 = constant_splat_i64(1U);
    const auto sign_mask = constant_splat_i64(1ULL << 63U);
    const auto sign = binary("and", bits, sign_mask, vi64, &operation);
    const auto zero_i64 = constant_splat_i64(0U);
    const auto negative = compare("ne", sign, zero_i64, vi64, &operation);
    const auto increment = binary("xor", error_positive, negative, v1, &operation);
    const auto incremented = binary("add", bits, one_i64, vi64, &operation);
    const auto decremented = binary("sub", bits, one_i64, vi64, &operation);
    const auto nudged_bits = select(increment, incremented, decremented, vi64, &operation, v1);
    const auto adjusted_bits = select(adjust, nudged_bits, bits, vi64, &operation, v1);
    const auto adjusted = cast("bitcast", adjusted_bits, vi64, v64, &operation);
    return cast("fptrunc", adjusted, v64, v32, &operation);
  }

  [[nodiscard]] std::string emit_vector_region(const EntryState& state,
                                               const std::vector<std::size_t>& operations,
                                               std::string_view block_coordinate_x,
                                               std::string_view block_coordinate_y) {
    const auto done = block();
    const auto header = block();
    const auto body = block();
    const auto group_capacity = constant_i32(8U);
    const auto tail_allowance = constant_i32(7U);
    const auto threaded_plus_tail = binary("add", state.block_threads, tail_allowance, "i32");
    const auto group_count = binary("udiv", threaded_plus_tail, group_capacity, "i32");
    const auto group = value();
    line("llvm.br " + header + "(" + state.zero_i32 + " : i32)");
    output_ << "  " << header << "(" << group << ": i32):\n";
    const auto more_groups = compare("ult", group, group_count, "i32");
    line("llvm.cond_br " + more_groups + ", " + body + ", " + done);
    declare_block(body);

    const auto base32 = binary("mul", group, group_capacity, "i32");
    const auto base64 = cast("zext", base32, "i32", "i64");
    const auto lane_offsets = constant_lane_offsets();
    const auto base_lanes = splat_scalar(base32, "i32");
    const auto lanes = binary("add", lane_offsets, base_lanes, vtype("i32"));
    const auto thread_count = splat_scalar(state.block_threads, "i32");
    const auto lane_mask = compare("ult", lanes, thread_count, vtype("i32"));
    // Memory semantics need the per-lane active state: inactive lanes must not
    // access memory or raise checks, exactly like the scalar loop's gates.
    const auto active_pointer = gep(state.active_storage, base64, "i8");
    const auto active_raw = masked_load(active_pointer, "i8", lane_mask, 1U);
    const auto active_bits = cast("trunc", active_raw, vtype("i8"), vtype("i1"));
    const auto effective_mask = binary("and", lane_mask, active_bits, vtype("i1"));
    const auto true_mask = constant_true_mask();

    std::unordered_map<std::uint32_t, std::string> region_values;
    const auto scalar_align = [](ValueKind kind) {
      switch (kind) {
      case ValueKind::Predicate:
        return 1U;
      case ValueKind::U64:
      case ValueKind::GlobalAddress:
      case ValueKind::SharedAddress:
        return 8U;
      default:
        return 4U;
      }
    };
    const auto vreg_of = [&](std::uint32_t register_index) {
      if (const auto known = region_values.find(register_index); known != region_values.end()) {
        return known->second;
      }
      const auto kind = kernel_.registers[register_index].kind;
      const auto pointer = gep(state.register_storage[register_index], base64,
                               kind == ValueKind::Predicate ? "i8" : mlir_type(kind));
      const auto loaded =
          masked_load(pointer, kind == ValueKind::Predicate ? "i8" : mlir_type(kind),
                      lane_mask, scalar_align(kind));
      auto vector_value = loaded;
      if (kind == ValueKind::Predicate) {
        vector_value = cast("trunc", loaded, vtype("i8"), vtype("i1"));
      }
      region_values.emplace(register_index, vector_value);
      return vector_value;
    };

    for (const auto index : operations) {
      const auto& operation = kernel_.operations[index];
      const auto vinput = [&](std::uint32_t position) {
        return vreg_of(operation.inputs[position]);
      };
      std::string result;
      switch (operation.opcode) {
      case Opcode::LoadParameterAddress:
      case Opcode::LoadSharedAddress:
        result = splat_scalar(state.zero_i64, "i64");
        break;
      case Opcode::LoadParameterU32: {
        const auto pointer = gep_constant("%scalars", operation.attribute, "i32");
        result = splat_scalar(load(pointer, "i32", &operation), "i32");
        break;
      }
      case Opcode::LoadParameterF32: {
        const auto pointer = gep_constant("%scalars", operation.attribute, "i32");
        const auto bits = load(pointer, "i32", &operation);
        result = splat_scalar(cast("bitcast", bits, "i32", "f32", &operation), "f32");
        break;
      }
      case Opcode::MoveSpecialU32:
        switch (static_cast<SpecialRegister>(operation.attribute)) {
        case SpecialRegister::ThreadIdX:
          result = binary("urem", lanes, splat_scalar("%block_x", "i32"), vtype("i32"));
          break;
        case SpecialRegister::ThreadIdY:
          result = binary("udiv", lanes, splat_scalar("%block_x", "i32"), vtype("i32"));
          break;
        case SpecialRegister::BlockIdX:
          result = splat_scalar(block_coordinate_x, "i32");
          break;
        case SpecialRegister::BlockIdY:
          result = splat_scalar(block_coordinate_y, "i32");
          break;
        case SpecialRegister::BlockDimX:
          result = splat_scalar("%block_x", "i32");
          break;
        case SpecialRegister::BlockDimY:
          result = splat_scalar("%block_y", "i32");
          break;
        case SpecialRegister::GridDimX:
          result = splat_scalar("%grid_x", "i32");
          break;
        case SpecialRegister::GridDimY:
          result = splat_scalar("%grid_y", "i32");
          break;
        }
        break;
      case Opcode::AddU32:
        result = binary("add", vinput(0U), vinput(1U), vtype("i32"), &operation);
        break;
      case Opcode::SubU32:
        result = binary("sub", vinput(0U), vinput(1U), vtype("i32"), &operation);
        break;
      case Opcode::MultiplyLoU32:
        result = binary("mul", vinput(0U), vinput(1U), vtype("i32"), &operation);
        break;
      case Opcode::MadLoU32: {
        const auto product = binary("mul", vinput(0U), vinput(1U), vtype("i32"), &operation);
        result = binary("add", product, vinput(2U), vtype("i32"), &operation);
        break;
      }
      case Opcode::MultiplyWideU32: {
        const auto extended = cast("zext", vinput(0U), vtype("i32"), vtype("i64"), &operation);
        result = binary("mul", extended, constant_splat_i64(operation.attribute),
                        vtype("i64"), &operation);
        break;
      }
      case Opcode::AddSharedAddress: {
        const auto offset = cast("zext", vinput(1U), vtype("i32"), vtype("i64"), &operation);
        result = binary("add", vinput(0U), offset, vtype("i64"), &operation);
        break;
      }
      case Opcode::AddGlobalAddress: {
        const auto base = vinput(0U);
        const auto sum = binary("add", base, vinput(1U), vtype("i64"), &operation);
        const auto not_overflowed = compare("uge", sum, base, vtype("i64"), &operation);
        const auto overflowed = binary("xor", not_overflowed, true_mask, vtype("i1"), &operation);
        const auto dangerous = binary("and", overflowed, effective_mask, vtype("i1"), &operation);
        const auto any_overflow = vector_reduce_or(dangerous);
        const auto addressed = block();
        line("llvm.cond_br " + any_overflow + ", " + state.overflow_error + ", " + addressed);
        declare_block(addressed);
        result = sum;
        break;
      }
      case Opcode::LoadGlobalU32:
      case Opcode::LoadGlobalF32: {
        const auto byte_offsets = vreg_of(operation.inputs[0]);
        const auto shift = constant_splat_i64(2U);
        const auto word_index =
            binary("lshr", byte_offsets, shift, vtype("i64"), &operation);
        const auto origin = origins_[operation.inputs[0]];
        const auto size_pointer = gep_constant("%sizes", origin.index, "i64");
        const auto size = load(size_pointer, "i64", &operation);
        const auto size_vector = splat_scalar(size, "i64");
        const auto out_of_bounds =
            compare("uge", word_index, size_vector, vtype("i64"), &operation);
        const auto dangerous =
            binary("and", out_of_bounds, effective_mask, vtype("i1"), &operation);
        const auto any_violation = vector_reduce_or(dangerous);
        const auto bounds_ok = block();
        line("llvm.cond_br " + any_violation + ", " + state.bounds_error + ", " + bounds_ok);
        declare_block(bounds_ok);
        const auto first_word = extract_lane_zero(word_index, "i64");
        const auto address_pointer = gep_constant("%buffers", origin.index, "i64");
        const auto address = load(address_pointer, "i64", &operation);
        const auto base = cast("inttoptr", address, "i64", "!llvm.ptr", &operation);
        const auto pointer = gep(base, first_word, "i32");
        const auto bits = masked_load(pointer, "i32", effective_mask, 4U);
        result = operation.opcode == Opcode::LoadGlobalF32
                     ? cast("bitcast", bits, vtype("i32"), vtype("f32"), &operation)
                     : bits;
        break;
      }
      case Opcode::StoreGlobalU32:
      case Opcode::StoreGlobalF32: {
        const auto byte_offsets = vreg_of(operation.inputs[0]);
        const auto shift = constant_splat_i64(2U);
        const auto word_index =
            binary("lshr", byte_offsets, shift, vtype("i64"), &operation);
        const auto origin = origins_[operation.inputs[0]];
        const auto size_pointer = gep_constant("%sizes", origin.index, "i64");
        const auto size = load(size_pointer, "i64", &operation);
        const auto size_vector = splat_scalar(size, "i64");
        const auto out_of_bounds =
            compare("uge", word_index, size_vector, vtype("i64"), &operation);
        const auto dangerous =
            binary("and", out_of_bounds, effective_mask, vtype("i1"), &operation);
        const auto any_violation = vector_reduce_or(dangerous);
        const auto bounds_ok = block();
        line("llvm.cond_br " + any_violation + ", " + state.bounds_error + ", " + bounds_ok);
        declare_block(bounds_ok);
        const auto writable_pointer = gep_constant("%writable", origin.index, "i32");
        const auto writable_value = load(writable_pointer, "i32", &operation);
        const auto writable = compare("ne", writable_value, state.zero_i32, "i32", &operation);
        const auto any_effective = vector_reduce_or(effective_mask);
        const auto not_writable = binary("xor", writable, state.true_i1, "i1", &operation);
        const auto readonly_violation =
            binary("and", not_writable, any_effective, "i1", &operation);
        const auto store_ok = block();
        line("llvm.cond_br " + readonly_violation + ", " + state.readonly_error + ", " +
             store_ok);
        declare_block(store_ok);
        auto stored = vinput(1U);
        if (operation.opcode == Opcode::StoreGlobalF32) {
          stored = cast("bitcast", stored, vtype("f32"), vtype("i32"), &operation);
        }
        const auto first_word = extract_lane_zero(word_index, "i64");
        const auto address_pointer = gep_constant("%buffers", origin.index, "i64");
        const auto address = load(address_pointer, "i64", &operation);
        const auto base = cast("inttoptr", address, "i64", "!llvm.ptr", &operation);
        const auto pointer = gep(base, first_word, "i32");
        masked_store(stored, pointer, "i32", effective_mask, 4U);
        break;
      }
      case Opcode::AddRnF32:
        result = binary("fadd", vinput(0U), vinput(1U), vtype("f32"), &operation);
        break;
      case Opcode::SubRnF32:
        result = binary("fsub", vinput(0U), vinput(1U), vtype("f32"), &operation);
        break;
      case Opcode::MultiplyRnF32:
        result = binary("fmul", vinput(0U), vinput(1U), vtype("f32"), &operation);
        break;
      case Opcode::MadRnF32:
      case Opcode::FmaRnF32: {
        const std::vector<std::string> inputs{vinput(0U), vinput(1U), vinput(2U)};
        result = fma_rn_vector(inputs, operation);
        break;
      }
      case Opcode::SetPredicateGeU32:
        result = compare("uge", vinput(0U), vinput(1U), vtype("i32"), &operation);
        break;
      case Opcode::SetPredicateEqU32:
        result = compare("eq", vinput(0U), vinput(1U), vtype("i32"), &operation);
        break;
      case Opcode::SetPredicateLtF32:
        result = compare("olt", vinput(0U), vinput(1U), vtype("f32"), &operation, true);
        break;
      default:
        break;
      }
      if (operation.result != metaflux::compiler::kNoValue) {
        region_values.emplace(operation.result, std::move(result));
      }
    }

    std::unordered_set<const Operation*> region_pointers;
    for (const auto index : operations) {
      region_pointers.insert(&kernel_.operations[index]);
    }
    for (const auto index : operations) {
      const auto& operation = kernel_.operations[index];
      if (operation.result == metaflux::compiler::kNoValue) {
        continue;
      }
      const bool read_outside =
          std::any_of(kernel_.operations.begin(), kernel_.operations.end(),
                      [&](const Operation& reader) {
                        return reads_register(reader, operation.result) &&
                               !region_pointers.contains(&reader);
                      });
      if (!read_outside) {
        continue;
      }
      const auto kind = kernel_.registers[operation.result].kind;
      auto vector_value = region_values.at(operation.result);
      const auto scalar_type = kind == ValueKind::Predicate ? "i8" : mlir_type(kind);
      if (kind == ValueKind::Predicate) {
        vector_value = cast("zext", vector_value, vtype("i1"), vtype("i8"));
      }
      const auto pointer = gep(state.register_storage[operation.result], base64, scalar_type);
      masked_store(vector_value, pointer, scalar_type, lane_mask, scalar_align(kind));
    }

    const auto next_group = binary("add", group, state.one_i32, "i32");
    line("llvm.br " + header + "(" + next_group + " : i32)");
    return done;
  }

  [[nodiscard]] std::string emit_phase(const EntryState& state,
                                       const std::vector<std::size_t>& operations,
                                       std::string_view block_coordinate_x,
                                       std::string_view block_coordinate_y) {
    mark_linear_registers(operations);
    const auto segments = segment_phase(operations);
    auto done = std::string{};
    for (std::size_t position = 0; position < segments.size(); ++position) {
      const auto& segment = segments[position];
      if (segment.vectorized) {
        promoted_.clear();
        done = emit_vector_region(state, segment.operations, block_coordinate_x,
                                  block_coordinate_y);
      } else {
        begin_segment_promotions(segment.operations);
        done = emit_scalar_segment(state, segment.operations, block_coordinate_x,
                                   block_coordinate_y);
      }
      if (position + 1 < segments.size()) {
        // Intermediate segment joins fall through into the next segment's loop.
        declare_block(done);
      }
    }
    if (done.empty()) {
      done = block();
      line("llvm.br " + done);
    }
    return done;
  }

  [[nodiscard]] std::string register_pointer(const EntryState& state, std::uint32_t index,
                                             std::string_view lane64) {
    return gep(state.register_storage[index], lane64, mlir_type(kernel_.registers[index].kind));
  }

  [[nodiscard]] std::string load_register(const EntryState& state, std::uint32_t index,
                                          std::string_view lane64,
                                          const Operation* operation = nullptr) {
    if (const auto promoted = promoted_.find(index); promoted != promoted_.end()) {
      return promoted->second;
    }
    const auto kind = kernel_.registers[index].kind;
    const auto loaded = load(register_pointer(state, index, lane64), mlir_type(kind), operation);
    return kind == ValueKind::Predicate ? cast("trunc", loaded, "i8", "i1", operation) : loaded;
  }

  void store_register(const EntryState& state, std::uint32_t index, std::string_view lane64,
                      std::string stored_value, const Operation* operation = nullptr) {
    if (const auto promoted = promoted_.find(index); promoted != promoted_.end()) {
      promoted->second = std::move(stored_value);
      return;
    }
    const auto kind = kernel_.registers[index].kind;
    if (kind == ValueKind::Predicate) {
      stored_value = cast("zext", stored_value, "i1", "i8", operation);
    }
    store(stored_value, register_pointer(state, index, lane64), mlir_type(kind), operation);
  }

  // A promoted definition must be fault-free and side-effect-free so computing
  // it on inactive or predicated-off lanes is unobservable, and it must not
  // branch to an error block. Memory forms and AddGlobalAddress stay gated.
  [[nodiscard]] static bool pure_value_definition(const Operation& operation) {
    switch (operation.opcode) {
    case Opcode::LoadParameterAddress:
    case Opcode::LoadParameterU32:
    case Opcode::LoadParameterF32:
    case Opcode::LoadSharedAddress:
    case Opcode::MoveSpecialU32:
    case Opcode::AddU32:
    case Opcode::SubU32:
    case Opcode::MultiplyLoU32:
    case Opcode::MadLoU32:
    case Opcode::MultiplyWideU32:
    case Opcode::AddSharedAddress:
    case Opcode::AddRnF32:
    case Opcode::SubRnF32:
    case Opcode::MultiplyRnF32:
    case Opcode::MadRnF32:
    case Opcode::FmaRnF32:
    case Opcode::ConvertRnF32U32:
    case Opcode::ConvertRziU32F32:
    case Opcode::SetPredicateGeU32:
    case Opcode::SetPredicateEqU32:
    case Opcode::SetPredicateLtF32:
      return true;
    default:
      return false;
    }
  }

  [[nodiscard]] static bool writes_register(const Operation& operation) {
    switch (operation.opcode) {
    case Opcode::BranchIf:
    case Opcode::BarrierSync:
    case Opcode::Return:
    case Opcode::StoreGlobalU32:
    case Opcode::StoreGlobalF32:
    case Opcode::StoreGlobalU64:
    case Opcode::StoreSharedU32:
      return false;
    default:
      return true;
    }
  }

  [[nodiscard]] static bool reads_register(const Operation& operation,
                                           std::uint32_t register_index) {
    for (std::uint32_t input = 0; input < operation.input_count; ++input) {
      if (operation.inputs[input] == register_index) {
        return true;
      }
    }
    return operation.predicate == register_index;
  }

  [[nodiscard]] const Operation* unique_definition(std::uint32_t register_index) const {
    const Operation* definition = nullptr;
    std::size_t count = 0;
    for (const auto& operation : kernel_.operations) {
      if (operation.result == register_index && writes_register(operation)) {
        ++count;
        definition = &operation;
      }
    }
    return count == 1 ? definition : nullptr;
  }

  // A byte offset built as linear_id * 4 is provably four-byte aligned, so the
  // runtime alignment check can be elided along that construction chain.
  [[nodiscard]] bool aligned_by_construction(std::uint32_t address_register) const {
    const auto address = unique_definition(address_register);
    if (address == nullptr) {
      return false;
    }
    if (address->opcode == Opcode::LoadParameterAddress ||
        address->opcode == Opcode::LoadSharedAddress) {
      return true;
    }
    if (address->opcode != Opcode::AddGlobalAddress) {
      return false;
    }
    const auto offset = unique_definition(address->inputs[1]);
    return offset != nullptr && offset->opcode == Opcode::MultiplyWideU32 &&
           offset->attribute == 4U;
  }

  [[nodiscard]] std::string emit_aligned_word_offset(
      const EntryState& state, const Operation& operation,
      std::string_view byte_offset, std::uint32_t address_register) {
    if (aligned_by_construction(address_register)) {
      const auto shift = constant_i64(2U);
      return binary("lshr", byte_offset, shift, "i64", &operation);
    }
    const auto low_bits = binary("and", byte_offset, state.three_i64, "i64", &operation);
    const auto aligned = compare("eq", low_bits, state.zero_i64, "i64", &operation);
    const auto alignment_ok = block();
    line("llvm.cond_br " + aligned + ", " + alignment_ok + ", " + state.alignment_error);
    declare_block(alignment_ok);
    const auto shift = constant_i64(2U);
    return binary("lshr", byte_offset, shift, "i64", &operation);
  }

  // Registers whose value advances one word per lane: thread ids, the CTA-wide
  // mad over them, their *4 byte scaling, and address arithmetic over that.
  // A global memory access over such a register is stride-one contiguous and
  // can use one masked vector transfer per lane group.
  void mark_linear_registers(const std::vector<std::size_t>& operations) {
    linear_registers_.clear();
    for (const auto index : operations) {
      const auto& operation = kernel_.operations[index];
      switch (operation.opcode) {
      case Opcode::MoveSpecialU32:
        if (static_cast<SpecialRegister>(operation.attribute) == SpecialRegister::ThreadIdX) {
          linear_registers_.insert(operation.result);
        }
        break;
      case Opcode::MadLoU32:
        if (linear_registers_.count(operation.inputs[2]) != 0U) {
          linear_registers_.insert(operation.result);
        }
        break;
      case Opcode::MultiplyWideU32:
        if (operation.attribute == 4U && linear_registers_.count(operation.inputs[0]) != 0U) {
          linear_registers_.insert(operation.result);
        }
        break;
      case Opcode::AddGlobalAddress:
        if (linear_registers_.count(operation.inputs[1]) != 0U) {
          linear_registers_.insert(operation.result);
        }
        break;
      default:
        break;
      }
    }
  }

  // Kernel IR v2 is single-assignment, so a register defined in this scalar
  // segment can drop its per-lane array slot when its definition is pure and
  // every reader of the register sits in this same segment after the
  // definition. Values defined inside a lane loop do not dominate later loops,
  // and SIMD regions exchange values through the arrays, so any reader outside
  // the segment keeps the array form.
  void begin_segment_promotions(const std::vector<std::size_t>& segment) {
    promoted_.clear();
    std::unordered_set<std::size_t> segment_indices(segment.begin(), segment.end());
    std::unordered_map<std::size_t, std::size_t> segment_positions;
    for (std::size_t position = 0; position < segment.size(); ++position) {
      segment_positions.emplace(segment[position], position);
    }
    for (std::uint32_t register_index = 0; register_index < kernel_.registers.size();
         ++register_index) {
      std::size_t definition_position = segment.size();
      std::size_t definition_count = 0;
      for (std::size_t position = 0; position < segment.size(); ++position) {
        const auto& operation = kernel_.operations[segment[position]];
        if (operation.result == register_index && writes_register(operation)) {
          ++definition_count;
          definition_position = position;
        }
      }
      if (definition_count != 1) {
        continue;
      }
      if (!pure_value_definition(kernel_.operations[segment[definition_position]])) {
        continue;
      }
      bool promotable = true;
      for (std::size_t index = 0; index < kernel_.operations.size() && promotable; ++index) {
        if (!reads_register(kernel_.operations[index], register_index)) {
          continue;
        }
        const auto position = segment_positions.find(index);
        if (position == segment_positions.end() || position->second < definition_position) {
          promotable = false;
        }
      }
      if (promotable) {
        promoted_.emplace(register_index, std::string{});
      }
    }
  }

  void emit_operation(const EntryState& state, const Operation& operation, std::string_view lane64,
                      std::string_view thread_x, std::string_view thread_y,
                      std::string_view block_x, std::string_view block_y) {
    const auto continuation = block();
    if (operation.result != metaflux::compiler::kNoValue &&
        promoted_.contains(operation.result)) {
      // The promoted definition set excludes every faulting, storing, or
      // branching form, so skipping the active/predicate gates keeps semantics
      // while letting the SSA name dominate same-phase readers directly.
      emit_operation_body(state, operation, lane64, thread_x, thread_y, block_x, block_y,
                          continuation, std::string_view{});
      declare_block(continuation);
      return;
    }
    const auto active_pointer = gep(state.active_storage, lane64, "i8");
    const auto active_i8 = load(active_pointer, "i8", &operation);
    const auto active = cast("trunc", active_i8, "i8", "i1", &operation);
    const auto execute = block();
    line("llvm.cond_br " + active + ", " + execute + ", " + continuation);
    declare_block(execute);

    if (operation.predicate != metaflux::compiler::kNoValue) {
      auto predicate = load_register(state, operation.predicate, lane64, &operation);
      if (operation.predicate_negated) {
        predicate = binary("xor", predicate, state.true_i1, "i1", &operation);
      }
      const auto predicated_execute = block();
      line("llvm.cond_br " + predicate + ", " + predicated_execute + ", " + continuation);
      declare_block(predicated_execute);
    }

    emit_operation_body(state, operation, lane64, thread_x, thread_y, block_x, block_y,
                        continuation, active_pointer);
    declare_block(continuation);
  }

  void emit_operation_body(const EntryState& state, const Operation& operation,
                           std::string_view lane64, std::string_view thread_x,
                           std::string_view thread_y, std::string_view block_x,
                           std::string_view block_y, std::string_view continuation,
                           std::string_view active_pointer) {
    const auto input = [&](std::uint32_t position) {
      return load_register(state, operation.inputs[position], lane64, &operation);
    };
    std::string result;
    switch (operation.opcode) {
    case Opcode::LoadParameterAddress:
    case Opcode::LoadSharedAddress:
      result = state.zero_i64;
      break;
    case Opcode::LoadParameterU32: {
      const auto pointer = gep_constant("%scalars", operation.attribute, "i32");
      result = load(pointer, "i32", &operation);
      break;
    }
    case Opcode::LoadParameterF32: {
      const auto pointer = gep_constant("%scalars", operation.attribute, "i32");
      const auto bits = load(pointer, "i32", &operation);
      result = cast("bitcast", bits, "i32", "f32", &operation);
      break;
    }
    case Opcode::MoveSpecialU32:
      switch (static_cast<SpecialRegister>(operation.attribute)) {
      case SpecialRegister::ThreadIdX:
        result = thread_x;
        break;
      case SpecialRegister::ThreadIdY:
        result = thread_y;
        break;
      case SpecialRegister::BlockIdX:
        result = block_x;
        break;
      case SpecialRegister::BlockIdY:
        result = block_y;
        break;
      case SpecialRegister::BlockDimX:
        result = "%block_x";
        break;
      case SpecialRegister::BlockDimY:
        result = "%block_y";
        break;
      case SpecialRegister::GridDimX:
        result = "%grid_x";
        break;
      case SpecialRegister::GridDimY:
        result = "%grid_y";
        break;
      }
      break;
    case Opcode::AddU32:
      result = binary("add", input(0U), input(1U), "i32", &operation);
      break;
    case Opcode::SubU32:
      result = binary("sub", input(0U), input(1U), "i32", &operation);
      break;
    case Opcode::MultiplyLoU32:
      result = binary("mul", input(0U), input(1U), "i32", &operation);
      break;
    case Opcode::MadLoU32: {
      const auto product = binary("mul", input(0U), input(1U), "i32", &operation);
      result = binary("add", product, input(2U), "i32", &operation);
      break;
    }
    case Opcode::MultiplyWideU32: {
      const auto extended = cast("zext", input(0U), "i32", "i64", &operation);
      const auto multiplier = constant_i64(operation.attribute);
      result = binary("mul", extended, multiplier, "i64", &operation);
      break;
    }
    case Opcode::AddGlobalAddress: {
      const auto base = input(0U);
      result = binary("add", base, input(1U), "i64", &operation);
      const auto no_overflow = compare("uge", result, base, "i64", &operation);
      const auto valid = block();
      line("llvm.cond_br " + no_overflow + ", " + valid + ", " + state.overflow_error);
      declare_block(valid);
      break;
    }
    case Opcode::AddSharedAddress: {
      const auto offset = cast("zext", input(1U), "i32", "i64", &operation);
      result = binary("add", input(0U), offset, "i64", &operation);
      break;
    }
    case Opcode::AddRnF32:
      result = binary("fadd", input(0U), input(1U), "f32", &operation);
      break;
    case Opcode::SubRnF32:
      result = binary("fsub", input(0U), input(1U), "f32", &operation);
      break;
    case Opcode::MultiplyRnF32:
      result = binary("fmul", input(0U), input(1U), "f32", &operation);
      break;
    case Opcode::MadRnF32:
    case Opcode::FmaRnF32: {
      // Binary64 represents the binary32 product exactly. TwoSum recovers the addition
      // residual so the rare double-rounding midpoint can be nudged in the exact direction.
      const auto left = cast("fpext", input(0U), "f32", "f64", &operation);
      const auto right = cast("fpext", input(1U), "f32", "f64", &operation);
      const auto addend = cast("fpext", input(2U), "f32", "f64", &operation);
      const auto product = binary("fmul", left, right, "f64", &operation);
      const auto sum = binary("fadd", product, addend, "f64", &operation);
      const auto rounded_addend = binary("fsub", sum, product, "f64", &operation);
      const auto recovered_product = binary("fsub", sum, rounded_addend, "f64", &operation);
      const auto product_error = binary("fsub", product, recovered_product, "f64", &operation);
      const auto addend_error = binary("fsub", addend, rounded_addend, "f64", &operation);
      const auto error = binary("fadd", product_error, addend_error, "f64", &operation);

      const auto bits = cast("bitcast", sum, "f64", "i64", &operation);
      const auto low_mask = constant_i64(0x1fffffffU);
      const auto low = binary("and", bits, low_mask, "i64", &operation);
      const auto halfway_bit = constant_i64(0x10000000U);
      const auto halfway = compare("eq", low, halfway_bit, "i64", &operation);
      const auto exponent_mask = constant_i64(0x7ff0000000000000ULL);
      const auto exponent = binary("and", bits, exponent_mask, "i64", &operation);
      const auto finite = compare("ne", exponent, exponent_mask, "i64", &operation);
      const auto zero_f64 = constant_f64_zero();
      const auto inexact = compare("one", error, zero_f64, "f64", &operation, true);
      const auto adjust_halfway = binary("and", halfway, finite, "i1", &operation);
      const auto adjust = binary("and", adjust_halfway, inexact, "i1", &operation);

      const auto error_positive = compare("ogt", error, zero_f64, "f64", &operation, true);
      const auto one_i64 = constant_i64(1U);
      const auto sign_shift = constant_i64(63U);
      const auto sign_mask = binary("shl", one_i64, sign_shift, "i64", &operation);
      const auto sign = binary("and", bits, sign_mask, "i64", &operation);
      const auto zero_i64 = constant_i64(0U);
      const auto negative = compare("ne", sign, zero_i64, "i64", &operation);
      const auto increment = binary("xor", error_positive, negative, "i1", &operation);
      const auto incremented = binary("add", bits, one_i64, "i64", &operation);
      const auto decremented = binary("sub", bits, one_i64, "i64", &operation);
      const auto nudged_bits = select(increment, incremented, decremented, "i64", &operation);
      const auto adjusted_bits = select(adjust, nudged_bits, bits, "i64", &operation);
      const auto adjusted = cast("bitcast", adjusted_bits, "i64", "f64", &operation);
      result = cast("fptrunc", adjusted, "f64", "f32", &operation);
      break;
    }
    case Opcode::ConvertRnF32U32:
      result = cast("uitofp", input(0U), "i32", "f32", &operation);
      break;
    case Opcode::ConvertRziU32F32: {
      const auto source = input(0U);
      result = value();
      line(result + " = llvm.call @llvm.fptoui.sat.i32.f32(" + source + ") : (f32) -> i32" +
           source_location(operation));
      break;
    }
    case Opcode::SetPredicateGeU32:
      result = compare("uge", input(0U), input(1U), "i32", &operation);
      break;
    case Opcode::SetPredicateEqU32:
      result = compare("eq", input(0U), input(1U), "i32", &operation);
      break;
    case Opcode::SetPredicateLtF32:
      result = compare("olt", input(0U), input(1U), "f32", &operation, true);
      break;
    case Opcode::BranchIf: {
      auto taken = input(0U);
      if (operation.flag) {
        taken = binary("xor", taken, state.true_i1, "i1", &operation);
      }
      const auto remains_active = binary("xor", taken, state.true_i1, "i1", &operation);
      const auto remains_active_i8 = cast("zext", remains_active, "i1", "i8", &operation);
      store(remains_active_i8, active_pointer, "i8", &operation);
      line("llvm.br " + std::string(continuation));
      return;
    }
    case Opcode::LoadGlobalU32:
    case Opcode::LoadGlobalF32:
      result = emit_global_load(state, operation, lane64);
      break;
    case Opcode::StoreGlobalU32:
    case Opcode::StoreGlobalF32:
      emit_global_store(state, operation, lane64, input(1U));
      line("llvm.br " + std::string(continuation));
      return;
    case Opcode::StoreGlobalU64:
      emit_global_store_u64(state, operation, lane64, input(1U));
      line("llvm.br " + std::string(continuation));
      return;
    case Opcode::LoadSharedU32:
      result = emit_shared_load(state, operation, lane64);
      break;
    case Opcode::StoreSharedU32:
      emit_shared_store(state, operation, lane64, input(1U));
      line("llvm.br " + std::string(continuation));
      return;
    case Opcode::BarrierSync:
    case Opcode::Return:
      line("llvm.br " + std::string(continuation));
      return;
    }
    store_register(state, operation.result, lane64, result, &operation);
    line("llvm.br " + std::string(continuation));
  }

  [[nodiscard]] std::string emit_global_pointer(const EntryState& state, const Operation& operation,
                                                std::string_view lane64, bool write) {
    const auto address_register = operation.inputs[0];
    const auto origin = origins_[address_register];
    const auto byte_offset = load_register(state, address_register, lane64, &operation);
    const auto word_index =
        emit_aligned_word_offset(state, operation, byte_offset, address_register);
    if (write) {
      const auto writable_pointer = gep_constant("%writable", origin.index, "i32");
      const auto writable_value = load(writable_pointer, "i32", &operation);
      const auto writable = compare("ne", writable_value, state.zero_i32, "i32", &operation);
      const auto writable_ok = block();
      line("llvm.cond_br " + writable + ", " + writable_ok + ", " + state.readonly_error);
      declare_block(writable_ok);
    }
    const auto size_pointer = gep_constant("%sizes", origin.index, "i64");
    const auto size = load(size_pointer, "i64", &operation);
    const auto in_bounds = compare("ult", word_index, size, "i64", &operation);
    const auto bounds_ok = block();
    line("llvm.cond_br " + in_bounds + ", " + bounds_ok + ", " + state.bounds_error);
    declare_block(bounds_ok);
    const auto address_pointer = gep_constant("%buffers", origin.index, "i64");
    const auto address = load(address_pointer, "i64", &operation);
    const auto base = cast("inttoptr", address, "i64", "!llvm.ptr", &operation);
    return gep(base, word_index, "i32");
  }

  [[nodiscard]] std::string emit_global_load(const EntryState& state, const Operation& operation,
                                             std::string_view lane64) {
    const auto pointer = emit_global_pointer(state, operation, lane64, false);
    const auto bits = load(pointer, "i32", &operation);
    return operation.opcode == Opcode::LoadGlobalF32
               ? cast("bitcast", bits, "i32", "f32", &operation)
               : bits;
  }

  void emit_global_store(const EntryState& state, const Operation& operation,
                         std::string_view lane64, std::string stored_value) {
    const auto pointer = emit_global_pointer(state, operation, lane64, true);
    if (operation.opcode == Opcode::StoreGlobalF32) {
      stored_value = cast("bitcast", stored_value, "f32", "i32", &operation);
    }
    store(stored_value, pointer, "i32", &operation);
  }

  void emit_global_store_u64(const EntryState& state, const Operation& operation,
                             std::string_view lane64, std::string stored_value) {
    const auto address_register = operation.inputs[0];
    const auto origin = origins_[address_register];
    const auto byte_offset = load_register(state, address_register, lane64, &operation);
    const auto low_bits = binary("and", byte_offset, constant_i64(7U), "i64", &operation);
    const auto aligned = compare("eq", low_bits, state.zero_i64, "i64", &operation);
    const auto alignment_ok = block();
    line("llvm.cond_br " + aligned + ", " + alignment_ok + ", " + state.alignment_error);
    declare_block(alignment_ok);
    const auto word_index = binary("lshr", byte_offset, constant_i64(2U), "i64", &operation);
    const auto next_index = binary("add", word_index, constant_i64(1U), "i64", &operation);
    const auto writable_pointer = gep_constant("%writable", origin.index, "i32");
    const auto writable_value = load(writable_pointer, "i32", &operation);
    const auto writable = compare("ne", writable_value, state.zero_i32, "i32", &operation);
    const auto writable_ok = block();
    line("llvm.cond_br " + writable + ", " + writable_ok + ", " + state.readonly_error);
    declare_block(writable_ok);
    const auto size_pointer = gep_constant("%sizes", origin.index, "i64");
    const auto size = load(size_pointer, "i64", &operation);
    const auto in_bounds = compare("ult", next_index, size, "i64", &operation);
    const auto bounds_ok = block();
    line("llvm.cond_br " + in_bounds + ", " + bounds_ok + ", " + state.bounds_error);
    declare_block(bounds_ok);
    const auto address_pointer = gep_constant("%buffers", origin.index, "i64");
    const auto address = load(address_pointer, "i64", &operation);
    const auto base = cast("inttoptr", address, "i64", "!llvm.ptr", &operation);
    const auto low_pointer = gep(base, word_index, "i32");
    const auto high_pointer = gep(base, next_index, "i32");
    const auto low = cast("trunc", stored_value, "i64", "i32", &operation);
    const auto high_shift = binary("lshr", stored_value, constant_i64(32U), "i64", &operation);
    const auto high = cast("trunc", high_shift, "i64", "i32", &operation);
    store(low, low_pointer, "i32", &operation);
    store(high, high_pointer, "i32", &operation);
  }

  [[nodiscard]] std::string emit_shared_pointer(const EntryState& state, const Operation& operation,
                                                std::string_view lane64) {
    const auto address_register = operation.inputs[0];
    const auto origin = origins_[address_register];
    const auto byte_offset = load_register(state, address_register, lane64, &operation);
    const auto word_index =
        emit_aligned_word_offset(state, operation, byte_offset, address_register);
    const auto allocation_words = constant_i64(kernel_.shared_allocations[origin.index].words);
    const auto in_bounds = compare("ult", word_index, allocation_words, "i64", &operation);
    const auto bounds_ok = block();
    line("llvm.cond_br " + in_bounds + ", " + bounds_ok + ", " + state.bounds_error);
    declare_block(bounds_ok);
    const auto allocation_offset = constant_i64(shared_offsets_[origin.index]);
    const auto absolute_index = binary("add", word_index, allocation_offset, "i64", &operation);
    return gep(state.shared_storage, absolute_index, "i32");
  }

  [[nodiscard]] std::string emit_shared_load(const EntryState& state, const Operation& operation,
                                             std::string_view lane64) {
    return load(emit_shared_pointer(state, operation, lane64), "i32", &operation);
  }

  void emit_shared_store(const EntryState& state, const Operation& operation,
                         std::string_view lane64, std::string_view stored_value) {
    store(stored_value, emit_shared_pointer(state, operation, lane64), "i32", &operation);
  }

  void emit_error_blocks(const EntryState& state) {
    const auto emit_return = [&](std::string_view block_name, GeneratedStatus status) {
      declare_block(block_name);
      const auto code = constant_i32(static_cast<std::uint32_t>(status));
      line("llvm.return " + code + " : i32");
    };
    emit_return(state.argument_error, GeneratedStatus::ArgumentCount);
    emit_return(state.launch_error, GeneratedStatus::InvalidLaunch);
    emit_return(state.overflow_error, GeneratedStatus::AddressOverflow);
    emit_return(state.alignment_error, GeneratedStatus::MisalignedAddress);
    emit_return(state.bounds_error, GeneratedStatus::OutOfBounds);
    emit_return(state.readonly_error, GeneratedStatus::WriteToReadOnly);
    emit_return(state.success, GeneratedStatus::Success);
  }
};

std::optional<TemporaryDirectory> create_temporary_directory(const CompileOptions& options) {
  std::filesystem::path root = options.temporary_root;
  if (root.empty()) {
    std::error_code error;
    root = std::filesystem::temp_directory_path(error);
    if (error) {
      return std::nullopt;
    }
  }
  std::error_code error;
  std::filesystem::create_directories(root, error);
  if (error) {
    return std::nullopt;
  }
  std::string pattern = (root / "metaflux-cpu-compile-XXXXXX").string();
  if (mkdtemp(pattern.data()) == nullptr) {
    return std::nullopt;
  }
  return TemporaryDirectory(std::filesystem::path(pattern));
}

std::optional<std::vector<std::byte>> read_artifact(const std::filesystem::path& path,
                                                    std::uint64_t maximum_bytes) {
  std::ifstream input(path, std::ios::binary | std::ios::ate);
  if (!input) {
    return std::nullopt;
  }
  const auto end = input.tellg();
  if (end < 0 || static_cast<std::uint64_t>(end) > maximum_bytes) {
    return std::nullopt;
  }
  std::vector<std::byte> bytes(static_cast<std::size_t>(end));
  input.seekg(0, std::ios::beg);
  if (!bytes.empty()) {
    input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
  }
  return input ? std::optional<std::vector<std::byte>>(std::move(bytes)) : std::nullopt;
}

enum class LinkResult : std::uint8_t {
  Success,
  Cancelled,
  Failed,
};

LinkResult link_shared_object(const CompileOptions& options, const std::filesystem::path& object,
                              const std::filesystem::path& output,
                              const std::filesystem::path& diagnostics) {
  const auto linker = options.linker_path.empty() ? std::filesystem::path(METAFLUX_DEFAULT_LLD_PATH)
                                                  : options.linker_path;
  const int diagnostic_fd =
      open(diagnostics.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0600);
  if (diagnostic_fd < 0) {
    return LinkResult::Failed;
  }

  const pid_t child = fork();
  if (child < 0) {
    static_cast<void>(close(diagnostic_fd));
    return LinkResult::Failed;
  }
  if (child == 0) {
    if (dup2(diagnostic_fd, STDERR_FILENO) < 0) {
      _exit(126);
    }
    static_cast<void>(close(diagnostic_fd));
    const rlimit address_space{.rlim_cur = options.limits.linker_address_space_bytes,
                               .rlim_max = options.limits.linker_address_space_bytes};
    const rlimit cpu_time{.rlim_cur = options.limits.linker_cpu_seconds,
                          .rlim_max = options.limits.linker_cpu_seconds};
    const rlimit file_size{.rlim_cur = options.limits.maximum_artifact_bytes,
                           .rlim_max = options.limits.maximum_artifact_bytes};
    if (setrlimit(RLIMIT_AS, &address_space) != 0 || setrlimit(RLIMIT_CPU, &cpu_time) != 0 ||
        setrlimit(RLIMIT_FSIZE, &file_size) != 0) {
      _exit(126);
    }

    const std::string linker_text = linker.string();
    const std::string object_text = object.string();
    const std::string output_text = output.string();
    std::array<char*, 13> arguments{
        const_cast<char*>(linker_text.c_str()),
        const_cast<char*>("--shared"),
        const_cast<char*>("--build-id=none"),
        const_cast<char*>("--no-undefined"),
        const_cast<char*>("--fatal-warnings"),
        const_cast<char*>("--threads=1"),
        const_cast<char*>("--hash-style=sysv"),
        const_cast<char*>("-z"),
        const_cast<char*>("noexecstack"),
        const_cast<char*>("-o"),
        const_cast<char*>(output_text.c_str()),
        const_cast<char*>(object_text.c_str()),
        nullptr,
    };
    execv(linker_text.c_str(), arguments.data());
    _exit(127);
  }
  static_cast<void>(close(diagnostic_fd));

  int status = 0;
  while (true) {
    const auto waited = waitpid(child, &status, WNOHANG);
    if (waited == child) {
      break;
    }
    if (waited < 0) {
      return LinkResult::Failed;
    }
    if (cancelled(options)) {
      static_cast<void>(kill(child, SIGKILL));
      while (waitpid(child, &status, 0) < 0 && errno == EINTR) {
      }
      return LinkResult::Cancelled;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }
  return WIFEXITED(status) && WEXITSTATUS(status) == 0 ? LinkResult::Success : LinkResult::Failed;
}

std::string read_diagnostic_text(const std::filesystem::path& path) {
  std::ifstream input(path);
  std::ostringstream output;
  output << input.rdbuf();
  auto text = output.str();
  constexpr std::size_t kMaximumDiagnostic = 4096U;
  if (text.size() > kMaximumDiagnostic) {
    text.resize(kMaximumDiagnostic);
  }
  return text;
}

std::uint64_t register_storage_bound(const Kernel& kernel) {
  std::uint64_t per_lane = 1U;
  for (const auto& reg : kernel.registers) {
    per_lane += storage_size(reg.kind);
  }
  return per_lane * kMaximumThreadsPerCta;
}

std::string canonical_feature_string(std::vector<std::string> features) {
  std::sort(features.begin(), features.end());
  features.erase(std::unique(features.begin(), features.end()), features.end());
  std::string result;
  for (const auto& feature : features) {
    if (!result.empty()) {
      result.push_back(',');
    }
    result.append(feature);
  }
  return result;
}

} // namespace

bool validate_compiled_elf(std::span<const std::byte> bytes) noexcept {
  constexpr std::size_t kHeaderSize = 64U;
  constexpr std::size_t kProgramHeaderSize = 56U;
  if (bytes.size() < kHeaderSize || std::to_integer<std::uint8_t>(bytes[0]) != 0x7fU ||
      std::to_integer<char>(bytes[1]) != 'E' || std::to_integer<char>(bytes[2]) != 'L' ||
      std::to_integer<char>(bytes[3]) != 'F' || std::to_integer<std::uint8_t>(bytes[4]) != 2U ||
      std::to_integer<std::uint8_t>(bytes[5]) != 1U || read_u16(bytes, 16U) != 3U ||
      read_u16(bytes, 18U) != 62U || read_u32(bytes, 20U) != 1U) {
    return false;
  }
  const auto program_offset = read_u64(bytes, 32U);
  const auto entry_size = read_u16(bytes, 54U);
  const auto entry_count = read_u16(bytes, 56U);
  if (entry_size != kProgramHeaderSize || entry_count == 0U || program_offset > bytes.size() ||
      static_cast<std::uint64_t>(entry_count) >
          (static_cast<std::uint64_t>(bytes.size()) - program_offset) / kProgramHeaderSize) {
    return false;
  }
  bool dynamic = false;
  bool executable = false;
  for (std::uint16_t index = 0; index < entry_count; ++index) {
    const auto offset = static_cast<std::size_t>(program_offset) +
                        static_cast<std::size_t>(index) * kProgramHeaderSize;
    const auto type = read_u32(bytes, offset);
    const auto flags = read_u32(bytes, offset + 4U);
    const auto file_offset = read_u64(bytes, offset + 8U);
    const auto file_size = read_u64(bytes, offset + 32U);
    const auto memory_size = read_u64(bytes, offset + 40U);
    if (file_size > memory_size || file_offset > bytes.size() ||
        file_size > static_cast<std::uint64_t>(bytes.size()) - file_offset) {
      return false;
    }
    dynamic = dynamic || type == 2U;
    if (type == 1U && (flags & 1U) != 0U) {
      executable = true;
      if ((flags & 2U) != 0U) {
        return false;
      }
    }
  }
  return dynamic && executable;
}

std::string_view compile_error_name(CompileError error) noexcept {
  switch (error) {
  case CompileError::None:
    return "MF_CPU_COMPILE_NONE";
  case CompileError::InvalidKernel:
    return "MF_CPU_COMPILE_INVALID_KERNEL";
  case CompileError::UnsupportedTarget:
    return "MF_CPU_COMPILE_UNSUPPORTED_TARGET";
  case CompileError::ResourceLimit:
    return "MF_CPU_COMPILE_RESOURCE_LIMIT";
  case CompileError::Cancelled:
    return "MF_CPU_COMPILE_CANCELLED";
  case CompileError::MlirGeneration:
    return "MF_CPU_COMPILE_MLIR_GENERATION";
  case CompileError::MlirVerification:
    return "MF_CPU_COMPILE_MLIR_VERIFICATION";
  case CompileError::LlvmTranslation:
    return "MF_CPU_COMPILE_LLVM_TRANSLATION";
  case CompileError::LlvmVerification:
    return "MF_CPU_COMPILE_LLVM_VERIFICATION";
  case CompileError::ObjectEmission:
    return "MF_CPU_COMPILE_OBJECT_EMISSION";
  case CompileError::LinkerFailed:
    return "MF_CPU_COMPILE_LINKER_FAILED";
  case CompileError::InvalidElf:
    return "MF_CPU_COMPILE_INVALID_ELF";
  case CompileError::Io:
    return "MF_CPU_COMPILE_IO";
  }
  return "MF_CPU_COMPILE_UNKNOWN";
}

CompileResult compile_kernel(const Kernel& kernel, const CompileOptions& options) {
  if (cancelled(options)) {
    return failure(CompileError::Cancelled, "compilation was cancelled before verification");
  }
  const auto kernel_diagnostics = metaflux::compiler::verify_kernel(kernel);
  if (!kernel_diagnostics.empty()) {
    return failure(CompileError::InvalidKernel, kernel_diagnostics.front().message,
                   kernel_diagnostics.front().location);
  }
  if (options.target_triple != "x86_64-unknown-linux-gnu" &&
      options.target_triple != "x86_64-pc-linux-gnu") {
    return failure(CompileError::UnsupportedTarget,
                   "compiler epoch 1 CPU artifacts require x86_64 Linux");
  }
  if (register_storage_bound(kernel) > options.limits.maximum_register_storage_bytes) {
    return failure(CompileError::ResourceLimit,
                   "per-CTA register storage exceeds the configured compiler limit");
  }

  MlirEmitter emitter(kernel);
  auto mlir_text = emitter.emit();
  if (mlir_text.empty()) {
    return failure(CompileError::MlirGeneration, "Kernel IR lowering produced empty MLIR");
  }
  if (cancelled(options)) {
    return failure(CompileError::Cancelled, "compilation was cancelled after MLIR generation");
  }

  mlir::DialectRegistry registry;
  registry.insert<mlir::LLVM::LLVMDialect>();
  mlir::registerBuiltinDialectTranslation(registry);
  mlir::registerLLVMDialectTranslation(registry);
  mlir::MLIRContext context(registry);
  auto module = mlir::parseSourceString<mlir::ModuleOp>(mlir_text, &context);
  if (!module) {
    return failure(CompileError::MlirGeneration, "generated LLVM-dialect MLIR did not parse");
  }
  if (mlir::failed(mlir::verify(*module))) {
    return failure(CompileError::MlirVerification, "generated LLVM-dialect MLIR did not verify");
  }

  llvm::LLVMContext llvm_context;
  auto llvm_module = mlir::translateModuleToLLVMIR(*module, llvm_context, "metaflux-kernel-v2");
  if (llvm_module == nullptr) {
    return failure(CompileError::LlvmTranslation, "MLIR to LLVM IR translation failed");
  }
  const llvm::Triple target_triple(options.target_triple);
  llvm_module->setTargetTriple(target_triple);
  llvm_module->setSourceFileName("metaflux-kernel-v2");
  if (auto* entry = llvm_module->getFunction(kCpuCompiledEntrySymbol); entry != nullptr) {
    entry->addFnAttr("no-builtins");
  }

  static const bool target_initialized = [] {
    return !llvm::InitializeNativeTarget() && !llvm::InitializeNativeTargetAsmPrinter();
  }();
  if (!target_initialized) {
    return failure(CompileError::UnsupportedTarget, "LLVM native x86 target initialization failed");
  }
  std::string target_error;
  const auto* target = llvm::TargetRegistry::lookupTarget(target_triple, target_error);
  if (target == nullptr) {
    return failure(CompileError::UnsupportedTarget, target_error);
  }
  llvm::TargetOptions target_options;
  const auto codegen_level = options.optimization == OptimizationLevel::O0
                                 ? llvm::CodeGenOptLevel::None
                                 : llvm::CodeGenOptLevel::Default;
  std::unique_ptr<llvm::TargetMachine> target_machine(target->createTargetMachine(
      target_triple, options.cpu_name, canonical_feature_string(options.canonical_features),
      target_options, llvm::Reloc::PIC_, llvm::CodeModel::Small, codegen_level));
  if (target_machine == nullptr) {
    return failure(CompileError::UnsupportedTarget, "LLVM rejected the x86 target environment");
  }
  llvm_module->setDataLayout(target_machine->createDataLayout());
  if (llvm::verifyModule(*llvm_module, &llvm::errs())) {
    return failure(CompileError::LlvmVerification, "translated LLVM IR did not verify");
  }

  if (options.optimization == OptimizationLevel::O2) {
    llvm::LoopAnalysisManager loop_analyses;
    llvm::FunctionAnalysisManager function_analyses;
    llvm::CGSCCAnalysisManager cgscc_analyses;
    llvm::ModuleAnalysisManager module_analyses;
    llvm::PassBuilder pass_builder(target_machine.get());
    pass_builder.registerModuleAnalyses(module_analyses);
    pass_builder.registerCGSCCAnalyses(cgscc_analyses);
    pass_builder.registerFunctionAnalyses(function_analyses);
    pass_builder.registerLoopAnalyses(loop_analyses);
    pass_builder.crossRegisterProxies(loop_analyses, function_analyses, cgscc_analyses,
                                      module_analyses);
    auto pipeline = pass_builder.buildPerModuleDefaultPipeline(llvm::OptimizationLevel::O2);
    pipeline.run(*llvm_module, module_analyses);
    if (llvm::verifyModule(*llvm_module, &llvm::errs())) {
      return failure(CompileError::LlvmVerification, "optimized LLVM IR did not verify");
    }
  }
  if (cancelled(options)) {
    return failure(CompileError::Cancelled, "compilation was cancelled before object emission");
  }

  std::string llvm_ir_text;
  llvm::raw_string_ostream llvm_ir_stream(llvm_ir_text);
  llvm_module->print(llvm_ir_stream, nullptr);
  llvm_ir_stream.flush();

  auto temporary = create_temporary_directory(options);
  if (!temporary.has_value()) {
    return failure(CompileError::Io, "compiler temporary directory creation failed");
  }
  const auto object_path = temporary->path / "kernel.o";
  const auto elf_path = temporary->path / "kernel.so";
  const auto linker_diagnostics = temporary->path / "lld.stderr";
  std::error_code object_error;
  llvm::raw_fd_ostream object_stream(object_path.string(), object_error, llvm::sys::fs::OF_None);
  if (object_error) {
    return failure(CompileError::Io, object_error.message());
  }
  llvm::legacy::PassManager object_pipeline;
  if (target_machine->addPassesToEmitFile(object_pipeline, object_stream, nullptr,
                                          llvm::CodeGenFileType::ObjectFile, false)) {
    return failure(CompileError::ObjectEmission, "LLVM target cannot emit an x86 object file");
  }
  object_pipeline.run(*llvm_module);
  object_stream.flush();
  if (object_stream.has_error()) {
    return failure(CompileError::ObjectEmission, "LLVM object stream reported an I/O error");
  }
  object_stream.close();

  const auto link_result = link_shared_object(options, object_path, elf_path, linker_diagnostics);
  if (link_result == LinkResult::Cancelled) {
    return failure(CompileError::Cancelled, "compilation was cancelled while linking PIC ELF");
  }
  if (link_result != LinkResult::Success) {
    return failure(CompileError::LinkerFailed,
                   "ld.lld failed: " + read_diagnostic_text(linker_diagnostics));
  }
  const auto elf = read_artifact(elf_path, options.limits.maximum_artifact_bytes);
  if (!elf.has_value()) {
    return failure(CompileError::Io, "linked PIC ELF cannot be read within the artifact limit");
  }
  if (!validate_compiled_elf(*elf)) {
    return failure(CompileError::InvalidElf, "linked artifact is not a valid x86_64 PIC ELF");
  }

  std::vector<ParameterKind> parameters;
  parameters.reserve(kernel.parameters.size());
  for (const auto& parameter : kernel.parameters) {
    parameters.push_back(parameter.kind);
  }
  return CompileResult{
      .artifact =
          CompiledArtifact{
              .elf = *elf,
              .elf_sha256 = metaflux::compiler::sha256_hex(*elf),
              .mlir_text = std::move(mlir_text),
              .llvm_ir_text = std::move(llvm_ir_text),
              .parameters = std::move(parameters),
              .uses_floating_point = uses_floating_point(kernel),
          },
      .diagnostic = std::nullopt,
  };
}

CompileOptions host_compile_options() {
  CompileOptions options;
  options.cpu_name = std::string(llvm::sys::getHostCPUName());
  const auto host_features = llvm::sys::getHostCPUFeatures();
  options.canonical_features.reserve(host_features.size());
  for (const auto& feature : host_features) {
    // "64bit" is the execution-mode bit implied by the x86_64 target triple;
    // it never belongs in the ISA feature string.
    if (feature.first() == "64bit") {
      continue;
    }
    // Subtarget parsing only honors sign-prefixed entries; unprefixed names
    // silently discard the CPU's implied ISA (down to x87-only codegen).
    options.canonical_features.emplace_back(feature.second ? "+" : "-") += feature.first();
  }
  return options;
}

metaflux::compiler::CacheIdentity make_cpu_cache_identity(const CompileOptions& options) {
  auto toolchain_fingerprint = std::string(kCpuToolchainFingerprint);
  if (!options.linker_path.empty()) {
    toolchain_fingerprint += ";explicit-lld=" + options.linker_path.string();
  }
  return metaflux::compiler::CacheIdentity{
      .toolchain_fingerprint = std::move(toolchain_fingerprint),
      .compiler_epoch = 1,
      .kernel_ir_schema = metaflux::compiler::kKernelIrSchemaVersion,
      .pass_pipeline = std::string(kCpuPipelineIdentity),
      .target_triple = options.target_triple,
      .cpu_name = options.cpu_name,
      .canonical_features = options.canonical_features,
      .optimization_level = options.optimization == OptimizationLevel::O0 ? "O0" : "O2",
      .fp_semantics = "ptx-9.0-rn-no-ftz",
      .backend_abi = kCpuBackendAbiVersion,
      .helper_abi = kCpuHelperAbiVersion,
      .pgo_id = std::string(kCpuPgoIdentity),
  };
}

} // namespace metaflux::backend::cpu::compiler
