#ifndef METAFLUX_BACKEND_CPU_INTERPRETER_HPP
#define METAFLUX_BACKEND_CPU_INTERPRETER_HPP

#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <stop_token>
#include <string_view>
#include <variant>

namespace metaflux::backend::cpu {

struct BufferArgument {
  std::span<std::uint32_t> words;
  bool writable = false;
};

struct Float32Argument {
  std::uint32_t bits = 0;
};

using Argument = std::variant<std::uint32_t, Float32Argument, BufferArgument>;

struct LaunchDimensions {
  std::uint32_t grid_x = 1;
  std::uint32_t block_x = 1;
  std::uint32_t grid_y = 1;
  std::uint32_t block_y = 1;
};

enum class ExecutionError : std::uint32_t {
  InvalidArtifact,
  SchemaMismatch,
  UnsupportedOperation,
  InvalidLaunch,
  ArgumentCount,
  ArgumentType,
  AddressOverflow,
  MisalignedAddress,
  OutOfBounds,
  WriteToReadOnly,
  BarrierDivergence,
  UnsupportedFpEnvironment,
  StepLimit,
  PlacementUnavailable,
  PlacementPinLost,
  System,
  Cancelled,
};

[[nodiscard]] std::string_view execution_error_name(ExecutionError error) noexcept;

struct ExecutionDiagnostic {
  ExecutionError error = ExecutionError::InvalidArtifact;
  std::uint32_t operation = std::numeric_limits<std::uint32_t>::max();
  std::uint32_t block_x = 0;
  std::uint32_t block_y = 0;
  std::uint32_t thread_x = 0;
  std::uint32_t thread_y = 0;
};

struct ExecutionResult {
  std::optional<ExecutionDiagnostic> diagnostic;

  [[nodiscard]] bool ok() const noexcept { return !diagnostic.has_value(); }
};

class CpuExecutor;

[[nodiscard]] ExecutionResult execute_kernel_ir(CpuExecutor& executor,
                                                std::string_view canonical_kernel_ir,
                                                std::span<const Argument> arguments,
                                                LaunchDimensions launch);
[[nodiscard]] ExecutionResult execute_kernel_ir(CpuExecutor& executor,
                                                std::string_view canonical_kernel_ir,
                                                std::span<const Argument> arguments,
                                                LaunchDimensions launch,
                                                std::stop_token cancellation);

[[nodiscard]] ExecutionResult execute_kernel_ir(std::string_view canonical_kernel_ir,
                                                std::span<const Argument> arguments,
                                                LaunchDimensions launch);
[[nodiscard]] ExecutionResult execute_kernel_ir(std::string_view canonical_kernel_ir,
                                                std::span<const Argument> arguments,
                                                LaunchDimensions launch,
                                                std::stop_token cancellation);

} // namespace metaflux::backend::cpu

#endif
