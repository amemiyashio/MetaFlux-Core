#ifndef METAFLUX_SERVICE_EXECUTION_HPP
#define METAFLUX_SERVICE_EXECUTION_HPP

#include "compiler_worker.hpp"

#include "metaflux/backend/cpu/compiled_kernel.hpp"
#include "metaflux/backend/cpu/executor.hpp"
#include "metaflux/compiler/artifact_cache.hpp"
#include "metaflux/compiler/kernel_ir.hpp"

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <stop_token>
#include <string>
#include <string_view>

namespace metaflux::service {

enum class CpuExecutionMode : std::uint32_t {
  Interpreter,
  ColdJit,
  WarmJit,
  Aot,
};

[[nodiscard]] std::string_view cpu_execution_mode_name(CpuExecutionMode mode) noexcept;

struct CpuExecutionConfiguration final {
  CpuExecutionMode mode = CpuExecutionMode::Interpreter;
  compiler::PersistentCacheConfig cache{};
  backend::cpu::PlacementPaths placement_paths{};
  backend::cpu::PlacementPolicy placement{};
  CompilerWorkerPolicy compiler_worker{};
};

struct CpuExecutionConfigurationResult final {
  std::optional<CpuExecutionConfiguration> configuration;
  std::string diagnostic;

  [[nodiscard]] bool ok() const noexcept { return configuration.has_value(); }
};

[[nodiscard]] CpuExecutionConfigurationResult
parse_cpu_execution_configuration(std::optional<std::string_view> mode,
                                  std::optional<std::string_view> cache_root,
                                  std::optional<std::string_view> explicit_cpu = std::nullopt,
                                  std::optional<std::string_view> topology_root = std::nullopt);
[[nodiscard]] CpuExecutionConfigurationResult cpu_execution_configuration_from_environment();

enum class ModulePreparationError : std::uint32_t {
  None,
  CacheMiss,
  NotSupported,
  Malformed,
  ResourceExhausted,
  System,
};

class PreparedModule final {
public:
  PreparedModule(std::string canonical_kernel_ir,
                 std::shared_ptr<backend::cpu::CpuExecutor> executor, bool accesses_global_memory);
  PreparedModule(std::string canonical_kernel_ir, backend::cpu::compiler::PreparedArtifact artifact,
                 backend::cpu::LoadedCompiledKernel kernel,
                 std::shared_ptr<backend::cpu::CpuExecutor> executor, bool accesses_global_memory);
  ~PreparedModule();

  PreparedModule(PreparedModule&&) noexcept;
  PreparedModule& operator=(PreparedModule&&) noexcept;
  PreparedModule(const PreparedModule&) = delete;
  PreparedModule& operator=(const PreparedModule&) = delete;

  [[nodiscard]] backend::cpu::ExecutionResult
  launch(std::span<const backend::cpu::Argument> arguments,
         backend::cpu::LaunchDimensions dimensions) const;
  [[nodiscard]] backend::cpu::ExecutionResult
  launch(std::span<const backend::cpu::Argument> arguments,
         backend::cpu::LaunchDimensions dimensions, std::stop_token cancellation) const;
  [[nodiscard]] bool accesses_global_memory() const noexcept { return accesses_global_memory_; }
  [[nodiscard]] const char* executor_name() const noexcept {
    return artifact_.has_value() ? "cpu-compiled" : "cpu-interpreter";
  }
  [[nodiscard]] std::string_view canonical_kernel_ir() const noexcept {
    return canonical_kernel_ir_;
  }

private:
  std::string canonical_kernel_ir_;
  std::optional<backend::cpu::compiler::PreparedArtifact> artifact_;
  backend::cpu::LoadedCompiledKernel compiled_kernel_;
  std::shared_ptr<backend::cpu::CpuExecutor> executor_;
  bool accesses_global_memory_ = false;
};

struct PrepareModuleResult final {
  std::unique_ptr<PreparedModule> module;
  ModulePreparationError error = ModulePreparationError::System;
  std::string diagnostic;

  [[nodiscard]] bool ok() const noexcept { return module != nullptr; }
};

struct CpuExecutionStatistics final {
  std::uint64_t compiler_requests = 0;
  std::uint64_t compiler_worker_launches = 0;
  std::uint64_t compiler_worker_failures = 0;
  std::int64_t last_compiler_worker_pid = -1;
  std::uint64_t cache_hits = 0;
  std::uint64_t cache_misses = 0;
  std::uint64_t loaded_modules = 0;
  backend::cpu::CpuExecutorStatistics executor{};
};

class CpuExecutionEngine final {
public:
  explicit CpuExecutionEngine(CpuExecutionConfiguration configuration);
  ~CpuExecutionEngine();

  CpuExecutionEngine(const CpuExecutionEngine&) = delete;
  CpuExecutionEngine& operator=(const CpuExecutionEngine&) = delete;

  [[nodiscard]] bool initialize(std::string& diagnostic);
  [[nodiscard]] PrepareModuleResult prepare(std::uint32_t peer_uid, const compiler::Kernel& kernel,
                                            std::string canonical_kernel_ir);
  [[nodiscard]] PrepareModuleResult prepare(std::uint32_t peer_uid, const compiler::Kernel& kernel,
                                            std::string canonical_kernel_ir,
                                            std::stop_token cancellation);
  [[nodiscard]] CpuExecutionMode mode() const noexcept { return configuration_.mode; }
  [[nodiscard]] CpuExecutionStatistics statistics() const;

private:
  CpuExecutionConfiguration configuration_;
  std::shared_ptr<backend::cpu::CpuExecutor> executor_;
  std::unique_ptr<compiler::PersistentArtifactCache> cache_;
  std::atomic<std::uint64_t> compiler_requests_{0};
  std::atomic<std::uint64_t> compiler_worker_launches_{0};
  std::atomic<std::uint64_t> compiler_worker_failures_{0};
  std::atomic<std::int64_t> last_compiler_worker_pid_{-1};
  std::atomic<std::uint64_t> cache_hits_{0};
  std::atomic<std::uint64_t> cache_misses_{0};
  std::atomic<std::uint64_t> loaded_modules_{0};
};

struct AotPrewarmResult final {
  bool success = false;
  bool compiled = false;
  std::string cache_key;
  std::string diagnostic;
};

[[nodiscard]] AotPrewarmResult prewarm_aot_file(const CpuExecutionConfiguration& configuration,
                                                std::string_view ptx_path);

} // namespace metaflux::service

#endif
