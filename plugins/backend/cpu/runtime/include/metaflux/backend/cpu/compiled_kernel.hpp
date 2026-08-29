#ifndef METAFLUX_BACKEND_CPU_COMPILED_KERNEL_HPP
#define METAFLUX_BACKEND_CPU_COMPILED_KERNEL_HPP

#include "metaflux/backend/cpu/interpreter.hpp"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>
#include <stop_token>
#include <string>
#include <string_view>
#include <vector>

namespace metaflux::backend::cpu {

inline constexpr std::uint32_t kCompiledKernelHelperAbiVersion = 2;
inline constexpr const char* kCompiledKernelEntrySymbol = "metaflux_cpu_cta_v2";

enum class CompiledStatus : std::uint32_t {
  Success = 0,
  InvalidLaunch = 1,
  ArgumentCount = 2,
  AddressOverflow = 3,
  MisalignedAddress = 4,
  OutOfBounds = 5,
  WriteToReadOnly = 6,
};

enum class CompiledParameterKind : std::uint32_t {
  BufferU32 = 0,
  ScalarU32 = 1,
  ScalarF32 = 2,
};

struct CompiledKernelSignature {
  std::vector<CompiledParameterKind> parameters;
  bool uses_floating_point = false;
};

struct ElfValidationResult {
  bool valid = false;
  std::string diagnostic;
};

[[nodiscard]] ElfValidationResult validate_x86_64_pic_elf(std::span<const std::byte> bytes);
[[nodiscard]] std::string sha256_file(const std::filesystem::path& path);

struct LoadCompiledKernelResult;

class LoadedCompiledKernel {
public:
  LoadedCompiledKernel() noexcept;
  ~LoadedCompiledKernel();
  LoadedCompiledKernel(LoadedCompiledKernel&&) noexcept;
  LoadedCompiledKernel& operator=(LoadedCompiledKernel&&) noexcept;
  LoadedCompiledKernel(const LoadedCompiledKernel&) = delete;
  LoadedCompiledKernel& operator=(const LoadedCompiledKernel&) = delete;

  [[nodiscard]] bool valid() const noexcept;
  [[nodiscard]] ExecutionResult launch(CpuExecutor& executor, std::span<const Argument> arguments,
                                       LaunchDimensions launch) const;
  [[nodiscard]] ExecutionResult launch(CpuExecutor& executor, std::span<const Argument> arguments,
                                       LaunchDimensions launch, std::stop_token cancellation) const;
  [[nodiscard]] ExecutionResult launch(std::span<const Argument> arguments,
                                       LaunchDimensions launch) const;
  [[nodiscard]] ExecutionResult launch(std::span<const Argument> arguments, LaunchDimensions launch,
                                       std::stop_token cancellation) const;

private:
  struct State;
  explicit LoadedCompiledKernel(std::unique_ptr<State> state) noexcept;
  std::unique_ptr<State> state_;

  friend LoadCompiledKernelResult load_compiled_kernel(const std::filesystem::path&,
                                                       std::string_view, CompiledKernelSignature);
};

struct LoadCompiledKernelResult {
  LoadedCompiledKernel kernel;
  std::string diagnostic;

  [[nodiscard]] bool ok() const noexcept { return kernel.valid(); }
};

[[nodiscard]] LoadCompiledKernelResult load_compiled_kernel(const std::filesystem::path& path,
                                                            std::string_view expected_sha256,
                                                            CompiledKernelSignature signature);

} // namespace metaflux::backend::cpu

#endif
