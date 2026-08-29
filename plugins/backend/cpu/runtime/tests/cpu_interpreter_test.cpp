#include "metaflux/backend/cpu/interpreter.hpp"

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <span>
#include <stop_token>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr std::string_view kCopyKernelIr = R"kir(MFKIR 2
PTX 9 0
KERNEL copy_u32
PARAMETERS 3
PARAMETER buffer_u32
PARAMETER buffer_u32
PARAMETER scalar_u32
SHARED_ALLOCATIONS 0
REGISTERS 12
REGISTER global_address
REGISTER global_address
REGISTER u32
REGISTER u32
REGISTER u32
REGISTER u32
REGISTER u32
REGISTER predicate
REGISTER u64
REGISTER global_address
REGISTER global_address
REGISTER u32
OPERATIONS 15
OP load_parameter_address 0 0 0 0 - 0
OP load_parameter_address 1 0 1 0 - 0
OP load_parameter_u32 2 0 2 0 - 0
OP move_special_u32 3 0 0 0 - 0
OP move_special_u32 4 0 2 0 - 0
OP move_special_u32 5 0 4 0 - 0
OP mad_lo_u32 6 3 4 5 3 0 0 - 0
OP set_predicate_ge_u32 7 2 6 2 0 0 - 0
OP branch_if - 1 7 14 0 - 0
OP multiply_wide_u32 8 1 6 4 0 - 0
OP add_global_address 9 2 0 8 0 0 - 0
OP add_global_address 10 2 1 8 0 0 - 0
OP load_global_u32 11 1 10 0 0 - 0
OP store_global_u32 - 2 9 11 0 0 - 0
OP return - 0 0 0 - 0
END
)kir";

bool expect(bool condition, std::string_view message) {
  if (!condition) {
    std::cerr << "CPU interpreter test failure: " << message << '\n';
  }
  return condition;
}

std::string replace_once(std::string source, std::string_view before, std::string_view after) {
  const auto position = source.find(before);
  if (position != std::string::npos) {
    source.replace(position, before.size(), after);
  }
  return source;
}

std::vector<metaflux::backend::cpu::Argument>
copy_arguments(std::vector<std::uint32_t>& destination, std::vector<std::uint32_t>& source,
               std::uint32_t count, bool destination_writable = true) {
  using metaflux::backend::cpu::Argument;
  using metaflux::backend::cpu::BufferArgument;
  std::vector<Argument> arguments;
  arguments.emplace_back(BufferArgument{
      .words = std::span<std::uint32_t>(destination),
      .writable = destination_writable,
  });
  arguments.emplace_back(BufferArgument{
      .words = std::span<std::uint32_t>(source),
      .writable = false,
  });
  arguments.emplace_back(count);
  return arguments;
}

bool has_error(const metaflux::backend::cpu::ExecutionResult& result,
               metaflux::backend::cpu::ExecutionError error) {
  return !result.ok() && result.diagnostic.has_value() && result.diagnostic->error == error;
}

bool test_copy_execution() {
  constexpr std::uint32_t count = 257;
  std::vector<std::uint32_t> source(count);
  std::vector<std::uint32_t> destination(count, 0U);
  for (std::uint32_t index = 0; index < count; ++index) {
    source[index] = index * 2654435761U;
  }
  auto arguments = copy_arguments(destination, source, count);
  const auto result = metaflux::backend::cpu::execute_kernel_ir(kCopyKernelIr, arguments,
                                                                {.grid_x = 5, .block_x = 64});
  return expect(result.ok(), "canonical Copy Kernel IR must execute") &&
         expect(destination == source, "all logical lanes must copy exact u32 bits");
}

bool test_execution_failures() {
  std::vector<std::uint32_t> source{1U, 2U, 3U, 4U};
  std::vector<std::uint32_t> destination(4U, 0U);
  auto arguments = copy_arguments(destination, source, 4U);

  auto result = metaflux::backend::cpu::execute_kernel_ir(
      replace_once(std::string(kCopyKernelIr), "MFKIR 2", "MFKIR 1"), arguments,
      {.grid_x = 1, .block_x = 4});
  if (!expect(has_error(result, metaflux::backend::cpu::ExecutionError::SchemaMismatch),
              "schema mismatch must be rejected before execution")) {
    return false;
  }

  result = metaflux::backend::cpu::execute_kernel_ir(
      replace_once(std::string(kCopyKernelIr), "load_global_u32", "unsupported_global_u32"),
      arguments, {.grid_x = 1, .block_x = 4});
  if (!expect(has_error(result, metaflux::backend::cpu::ExecutionError::UnsupportedOperation),
              "unknown artifact operation must have a stable error")) {
    return false;
  }

  result = metaflux::backend::cpu::execute_kernel_ir(std::string(kCopyKernelIr) + "TRAILING",
                                                     arguments, {.grid_x = 1, .block_x = 4});
  if (!expect(has_error(result, metaflux::backend::cpu::ExecutionError::InvalidArtifact),
              "trailing artifact content must be rejected")) {
    return false;
  }

  result = metaflux::backend::cpu::execute_kernel_ir(kCopyKernelIr, arguments,
                                                     {.grid_x = 1, .block_x = 0});
  if (!expect(has_error(result, metaflux::backend::cpu::ExecutionError::InvalidLaunch),
              "zero-sized launch must be rejected")) {
    return false;
  }

  result = metaflux::backend::cpu::execute_kernel_ir(
      kCopyKernelIr, arguments,
      {.grid_x = 0xffffffffU, .block_x = 1024U, .grid_y = 0xffffffffU, .block_y = 1U});
  if (!expect(has_error(result, metaflux::backend::cpu::ExecutionError::InvalidLaunch),
              "overflowing 2D logical-thread products must be rejected before multiplication")) {
    return false;
  }

  std::vector<metaflux::backend::cpu::Argument> short_arguments(arguments.begin(),
                                                                arguments.end() - 1);
  result = metaflux::backend::cpu::execute_kernel_ir(kCopyKernelIr, short_arguments,
                                                     {.grid_x = 1, .block_x = 4});
  if (!expect(has_error(result, metaflux::backend::cpu::ExecutionError::ArgumentCount),
              "argument count mismatch must be rejected")) {
    return false;
  }

  auto wrong_type = arguments;
  wrong_type[2] = metaflux::backend::cpu::BufferArgument{
      .words = std::span<std::uint32_t>(source),
      .writable = false,
  };
  result = metaflux::backend::cpu::execute_kernel_ir(kCopyKernelIr, wrong_type,
                                                     {.grid_x = 1, .block_x = 4});
  if (!expect(has_error(result, metaflux::backend::cpu::ExecutionError::ArgumentType),
              "argument type mismatch must be rejected")) {
    return false;
  }

  auto read_only = copy_arguments(destination, source, 4U, false);
  result = metaflux::backend::cpu::execute_kernel_ir(kCopyKernelIr, read_only,
                                                     {.grid_x = 1, .block_x = 4});
  if (!expect(has_error(result, metaflux::backend::cpu::ExecutionError::WriteToReadOnly),
              "stores through read-only parameters must be rejected")) {
    return false;
  }

  std::vector<std::uint32_t> short_destination(3U, 0U);
  auto out_of_bounds = copy_arguments(short_destination, source, 4U);
  result = metaflux::backend::cpu::execute_kernel_ir(kCopyKernelIr, out_of_bounds,
                                                     {.grid_x = 1, .block_x = 4});
  if (!expect(has_error(result, metaflux::backend::cpu::ExecutionError::OutOfBounds),
              "global stores must check byte bounds")) {
    return false;
  }

  auto misaligned_text =
      replace_once(std::string(kCopyKernelIr), "OP multiply_wide_u32 8 1 6 4 0 - 0",
                   "OP multiply_wide_u32 8 1 6 2 0 - 0");
  result = metaflux::backend::cpu::execute_kernel_ir(misaligned_text, arguments,
                                                     {.grid_x = 1, .block_x = 4});
  return expect(has_error(result, metaflux::backend::cpu::ExecutionError::MisalignedAddress),
                "global accesses must reject misaligned addresses");
}

bool test_pre_cancelled_launch() {
  std::vector<std::uint32_t> source{1U, 2U, 3U, 4U};
  std::vector<std::uint32_t> destination(4U, 0U);
  auto arguments = copy_arguments(destination, source, 4U);
  std::stop_source cancellation;
  static_cast<void>(cancellation.request_stop());
  const auto result = metaflux::backend::cpu::execute_kernel_ir(
      kCopyKernelIr, arguments, {.grid_x = 1U, .block_x = 4U}, cancellation.get_token());
  return expect(has_error(result, metaflux::backend::cpu::ExecutionError::Cancelled),
                "a pre-cancelled interpreter launch must not enter scheduling") &&
         expect(std::all_of(destination.begin(), destination.end(),
                            [](std::uint32_t value) { return value == 0U; }),
                "a cancelled interpreter launch must not modify arguments");
}

bool test_error_names() {
  return expect(metaflux::backend::cpu::execution_error_name(
                    metaflux::backend::cpu::ExecutionError::OutOfBounds) == "MF_CPU_OUT_OF_BOUNDS",
                "execution errors need stable names") &&
         expect(metaflux::backend::cpu::execution_error_name(
                    metaflux::backend::cpu::ExecutionError::InvalidArtifact) !=
                    metaflux::backend::cpu::execution_error_name(
                        metaflux::backend::cpu::ExecutionError::SchemaMismatch),
                "execution error names must distinguish malformed and incompatible artifacts");
}

} // namespace

int main() {
  return test_copy_execution() && test_execution_failures() && test_pre_cancelled_launch() &&
                 test_error_names()
             ? 0
             : 1;
}
