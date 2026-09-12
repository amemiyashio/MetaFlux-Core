#ifndef METAFLUX_COMPILER_KERNEL_IR_HPP
#define METAFLUX_COMPILER_KERNEL_IR_HPP

#include <array>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

namespace metaflux::compiler {

inline constexpr std::uint32_t kKernelIrSchemaVersion = 2;
inline constexpr std::uint32_t kNoValue = std::numeric_limits<std::uint32_t>::max();

struct SourceLocation {
  std::uint32_t line = 1;
  std::uint32_t column = 1;

  friend constexpr bool operator==(const SourceLocation&, const SourceLocation&) = default;
};

enum class DiagnosticCode : std::uint32_t {
  PtxSyntax,
  PtxUnsupportedVersion,
  PtxUnsupportedTarget,
  PtxUnsupportedInstruction,
  PtxDuplicateSymbol,
  PtxUnknownSymbol,
  PtxTypeMismatch,
  KernelIrSchemaMismatch,
  KernelIrInvalidKernel,
  KernelIrInvalidParameter,
  KernelIrInvalidRegister,
  KernelIrInvalidOperation,
  KernelIrOperandCount,
  KernelIrIndexOutOfRange,
  KernelIrTypeMismatch,
  KernelIrUseBeforeDefinition,
  KernelIrDuplicateDefinition,
  KernelIrInvalidControlFlow,
  KernelIrMissingReturn,
};

[[nodiscard]] std::string_view diagnostic_code_name(DiagnosticCode code) noexcept;

struct Diagnostic {
  DiagnosticCode code = DiagnosticCode::PtxSyntax;
  SourceLocation location{};
  std::string message;
  std::string form;
};

enum class ParameterKind : std::uint32_t {
  BufferU32,
  ScalarU32,
  ScalarF32,
};

enum class ValueKind : std::uint32_t {
  Predicate,
  U32,
  U64,
  F32,
  GlobalAddress,
  SharedAddress,
};

enum class SpecialRegister : std::uint32_t {
  ThreadIdX,
  ThreadIdY,
  BlockIdX,
  BlockIdY,
  BlockDimX,
  BlockDimY,
  GridDimX,
  GridDimY,
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
  // Unary natural exponential, binary32 RNE/no-FTZ. Finite nonzero results
  // have at most 2 ULP error against a correctly rounded reference. +/-0 map
  // to 1, +infinity to +infinity, -infinity to +0, and NaN to NaN (payload free).
  // Overflow/underflow classifications follow binary32 RNE. Unlike *RnF32,
  // this does not promise correctly rounded finite results. CPU modes sharing
  // one math-helper identity additionally agree bit-for-bit on the corpus.
  ExpF32,
};

// Encoded consumers use the same operation vocabulary as the core verifier.
[[nodiscard]] bool is_valid_opcode(Opcode opcode) noexcept;
[[nodiscard]] std::string_view parameter_kind_name(ParameterKind kind) noexcept;
[[nodiscard]] std::string_view value_kind_name(ValueKind kind) noexcept;
[[nodiscard]] std::string_view opcode_name(Opcode opcode) noexcept;

struct Parameter {
  ParameterKind kind = ParameterKind::ScalarU32;
  SourceLocation location{};
};

struct Register {
  ValueKind kind = ValueKind::U32;
  SourceLocation location{};
};

struct SharedAllocation {
  std::uint32_t words = 0;
  SourceLocation location{};
};

struct Operation {
  Opcode opcode = Opcode::Return;
  std::uint32_t result = kNoValue;
  std::array<std::uint32_t, 3> inputs{kNoValue, kNoValue, kNoValue};
  std::uint32_t input_count = 0;
  std::uint32_t attribute = 0;
  bool flag = false;
  std::uint32_t predicate = kNoValue;
  bool predicate_negated = false;
  SourceLocation location{};
};

struct Kernel {
  std::uint32_t schema_version = kKernelIrSchemaVersion;
  std::uint32_t ptx_major = 9;
  std::uint32_t ptx_minor = 0;
  std::string name;
  std::vector<Parameter> parameters;
  std::vector<SharedAllocation> shared_allocations;
  std::vector<Register> registers;
  std::vector<Operation> operations;
};

[[nodiscard]] std::vector<Diagnostic> verify_kernel(const Kernel& kernel);

struct SerializationResult {
  std::string text;
  std::vector<Diagnostic> diagnostics;

  [[nodiscard]] bool ok() const noexcept { return diagnostics.empty(); }
};

// Locations are intentionally omitted so formatting-only PTX changes retain one
// semantic cache identity. Register names have already been resolved to indices.
[[nodiscard]] SerializationResult serialize_kernel(const Kernel& kernel);

} // namespace metaflux::compiler

#endif
