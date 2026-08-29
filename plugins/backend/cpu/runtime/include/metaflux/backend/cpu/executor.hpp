#ifndef METAFLUX_BACKEND_CPU_EXECUTOR_HPP
#define METAFLUX_BACKEND_CPU_EXECUTOR_HPP

#include "metaflux/backend/cpu/interpreter.hpp"
#include "metaflux/backend/cpu/placement.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <stop_token>
#include <string>

namespace metaflux::backend::cpu {

struct CpuExecutorOptions final {
  PlacementPaths paths;
  PlacementPolicy policy;
};

struct CpuExecutorStatistics final {
  std::uint64_t kernel_boundaries = 0;
  std::uint64_t topology_rebuilds = 0;
  std::uint64_t cta_jobs = 0;
  std::uint64_t whole_kernel_jobs = 0;
};

class CpuExecutor final {
public:
  using CtaTask = std::function<ExecutionResult(std::uint64_t cta_index, std::uint32_t numa_node)>;
  using KernelTask = std::function<ExecutionResult(std::uint32_t numa_node)>;

  CpuExecutor();
  explicit CpuExecutor(CpuExecutorOptions options);
  ~CpuExecutor();

  CpuExecutor(CpuExecutor&&) noexcept;
  CpuExecutor& operator=(CpuExecutor&&) noexcept;
  CpuExecutor(const CpuExecutor&) = delete;
  CpuExecutor& operator=(const CpuExecutor&) = delete;

  [[nodiscard]] ExecutionResult refresh();
  [[nodiscard]] ExecutionResult run_ctas(std::uint64_t count, CtaTask task);
  [[nodiscard]] ExecutionResult run_ctas(std::uint64_t count, CtaTask task,
                                         std::stop_token cancellation);
  [[nodiscard]] ExecutionResult run_kernel(KernelTask task);
  [[nodiscard]] ExecutionResult run_kernel(KernelTask task, std::stop_token cancellation);
  [[nodiscard]] std::optional<PlacementSnapshot> snapshot() const;
  [[nodiscard]] CpuExecutorStatistics statistics() const;
  [[nodiscard]] std::string last_diagnostic() const;

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

[[nodiscard]] CpuExecutor& default_cpu_executor();

} // namespace metaflux::backend::cpu

#endif
