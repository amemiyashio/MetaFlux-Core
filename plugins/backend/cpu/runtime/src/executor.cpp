#include "metaflux/backend/cpu/executor.hpp"

#include <algorithm>
#include <condition_variable>
#include <cstddef>
#include <cstring>
#include <exception>
#include <limits>
#include <memory>
#include <mutex>
#include <pthread.h>
#include <sched.h>
#include <thread>
#include <utility>
#include <vector>

namespace metaflux::backend::cpu {
namespace {

[[nodiscard]] ExecutionResult execution_failure(ExecutionError error) {
  return {.diagnostic = ExecutionDiagnostic{.error = error}};
}

[[nodiscard]] ExecutionError placement_execution_error(PlacementError error) {
  if (error == PlacementError::ExplicitPinLost) {
    return ExecutionError::PlacementPinLost;
  }
  return error == PlacementError::System ? ExecutionError::System
                                         : ExecutionError::PlacementUnavailable;
}

[[nodiscard]] bool equivalent(const PlacementSnapshot& left, const PlacementSnapshot& right) {
  return left.sched_affinity == right.sched_affinity && left.online_cpus == right.online_cpus &&
         left.cpuset_cpus == right.cpuset_cpus && left.effective_cpus == right.effective_cpus &&
         left.online_mems == right.online_mems && left.cpuset_mems == right.cpuset_mems &&
         left.effective_mems == right.effective_mems &&
         left.physical_cores == right.physical_cores &&
         left.reserved_control_core == right.reserved_control_core && left.pools == right.pools &&
         left.explicit_cpu == right.explicit_cpu &&
         left.cross_node_stealing == right.cross_node_stealing;
}

[[nodiscard]] std::string pin_current_thread(std::uint32_t cpu) {
  const std::size_t cpu_count = std::max<std::size_t>(CPU_SETSIZE, cpu + 1U);
  const auto byte_count = CPU_ALLOC_SIZE(cpu_count);
  cpu_set_t* mask = CPU_ALLOC(cpu_count);
  if (mask == nullptr) {
    return "worker affinity mask allocation failed";
  }
  CPU_ZERO_S(byte_count, mask);
  CPU_SET_S(cpu, byte_count, mask);
  const int set_error = pthread_setaffinity_np(pthread_self(), byte_count, mask);
  if (set_error != 0) {
    CPU_FREE(mask);
    return "pthread_setaffinity_np failed for CPU " + std::to_string(cpu) + ": " +
           std::strerror(set_error);
  }
  CPU_ZERO_S(byte_count, mask);
  const int get_error = pthread_getaffinity_np(pthread_self(), byte_count, mask);
  const bool exact = get_error == 0 && CPU_COUNT_S(byte_count, mask) == 1 &&
                     CPU_ISSET_S(cpu, byte_count, mask) != 0;
  CPU_FREE(mask);
  if (get_error != 0) {
    return "pthread_getaffinity_np failed for CPU " + std::to_string(cpu) + ": " +
           std::strerror(get_error);
  }
  return exact ? std::string{}
               : "kernel did not preserve the exact worker pin for CPU " + std::to_string(cpu);
}

struct Batch final {
  CpuExecutor::CtaTask task;
  std::stop_token cancellation;
  std::uint64_t remaining = 0;
  std::mutex mutex;
  std::condition_variable complete;
  std::optional<std::pair<std::uint64_t, ExecutionDiagnostic>> first_failure;
  bool cancellation_observed = false;

  void finish(std::uint64_t index, ExecutionResult result) {
    std::lock_guard lock(mutex);
    if (!result.ok() && result.diagnostic->error == ExecutionError::Cancelled) {
      cancellation_observed = true;
    }
    if (!result.ok() && (!first_failure.has_value() || index < first_failure->first)) {
      first_failure = std::pair{index, *result.diagnostic};
    }
    --remaining;
    if (remaining == 0U) {
      complete.notify_one();
    }
  }

  void discard(std::uint64_t first_index, std::uint64_t count) {
    if (count == 0U) {
      return;
    }
    std::lock_guard lock(mutex);
    cancellation_observed = true;
    const ExecutionDiagnostic cancelled{.error = ExecutionError::Cancelled};
    if (!first_failure.has_value() || first_index < first_failure->first) {
      first_failure = std::pair{first_index, cancelled};
    }
    remaining -= count;
    if (remaining == 0U) {
      complete.notify_one();
    }
  }

  [[nodiscard]] ExecutionResult wait() {
    std::unique_lock lock(mutex);
    complete.wait(lock, [&] { return remaining == 0U; });
    if (cancellation_observed) {
      return execution_failure(ExecutionError::Cancelled);
    }
    return first_failure.has_value() ? ExecutionResult{.diagnostic = first_failure->second}
                                     : ExecutionResult{};
  }
};

struct Job final {
  std::uint64_t index = 0;
  Batch* batch = nullptr;
};

class NodePool final {
public:
  NodePool(std::uint32_t node, std::vector<std::uint32_t> cpus)
      : node_(node), cpus_(std::move(cpus)) {}

  ~NodePool() { stop(); }
  NodePool(const NodePool&) = delete;
  NodePool& operator=(const NodePool&) = delete;

  [[nodiscard]] bool start(std::string& diagnostic) {
    for (const auto cpu : cpus_) {
      threads_.emplace_back([this, cpu] { worker(cpu); });
    }
    std::unique_lock lock(mutex_);
    started_condition_.wait(lock, [&] { return started_ == cpus_.size(); });
    if (!startup_error_.empty()) {
      diagnostic = startup_error_;
      lock.unlock();
      stop();
      return false;
    }
    return true;
  }

  void enqueue(Job job) {
    {
      std::lock_guard lock(mutex_);
      jobs_.push_back(std::move(job));
    }
    work_condition_.notify_one();
  }

  void reserve_jobs(std::size_t count) {
    std::lock_guard lock(mutex_);
    jobs_.clear();
    next_job_ = 0U;
    jobs_.reserve(count);
  }

  void discard_pending(Batch* batch) {
    std::uint64_t first_index = 0;
    std::uint64_t count = 0;
    {
      std::lock_guard lock(mutex_);
      if (next_job_ == jobs_.size()) {
        return;
      }
      first_index = jobs_[next_job_].index;
      count = static_cast<std::uint64_t>(jobs_.size() - next_job_);
      jobs_.resize(next_job_);
    }
    batch->discard(first_index, count);
  }

  [[nodiscard]] std::uint32_t node() const noexcept { return node_; }

private:
  void stop() {
    {
      std::lock_guard lock(mutex_);
      stopping_ = true;
    }
    work_condition_.notify_all();
    for (auto& thread : threads_) {
      if (thread.joinable()) {
        thread.join();
      }
    }
    threads_.clear();
  }

  void worker(std::uint32_t cpu) {
    const std::string pin_error = pin_current_thread(cpu);
    {
      std::lock_guard lock(mutex_);
      if (!pin_error.empty() && startup_error_.empty()) {
        startup_error_ = pin_error;
      }
      ++started_;
    }
    started_condition_.notify_one();
    if (!pin_error.empty()) {
      return;
    }

    while (true) {
      Job job;
      {
        std::unique_lock lock(mutex_);
        work_condition_.wait(lock, [&] { return stopping_ || next_job_ < jobs_.size(); });
        if (stopping_ && next_job_ == jobs_.size()) {
          return;
        }
        job = std::move(jobs_[next_job_]);
        ++next_job_;
      }

      ExecutionResult result;
      try {
        result = job.batch->cancellation.stop_requested()
                     ? execution_failure(ExecutionError::Cancelled)
                     : job.batch->task(job.index, node_);
      } catch (...) {
        result = execution_failure(ExecutionError::System);
      }
      job.batch->finish(job.index, std::move(result));
    }
  }

  std::uint32_t node_ = 0;
  std::vector<std::uint32_t> cpus_;
  std::vector<std::thread> threads_;
  std::vector<Job> jobs_;
  std::size_t next_job_ = 0;
  std::mutex mutex_;
  std::condition_variable work_condition_;
  std::condition_variable started_condition_;
  std::size_t started_ = 0;
  bool stopping_ = false;
  std::string startup_error_;
};

} // namespace

struct CpuExecutor::Impl final {
  explicit Impl(CpuExecutorOptions configured) : options(std::move(configured)) {}

  explicit Impl(PlacementPathsResult configured) {
    if (configured.ok()) {
      options.paths = std::move(*configured.paths);
      return;
    }
    configuration_error = configured.error;
    configuration_diagnostic = std::move(configured.diagnostic);
  }
  [[nodiscard]] bool acquire_boundary(std::stop_token cancellation) {
    std::unique_lock lock(boundary_mutex);
    if (cancellation.stop_requested()) {
      return false;
    }
    const bool acquired =
        boundary_condition.wait(lock, cancellation, [&] { return !boundary_active; });
    if (!acquired || cancellation.stop_requested()) {
      return false;
    }
    boundary_active = true;
    return true;
  }

  void release_boundary() {
    {
      std::lock_guard lock(boundary_mutex);
      boundary_active = false;
    }
    boundary_condition.notify_all();
  }

  struct BoundaryLease final {
    Impl& owner;
    ~BoundaryLease() { owner.release_boundary(); }
  };

  [[nodiscard]] ExecutionResult ensure_placement() {
    if (configuration_error != PlacementError::None) {
      diagnostic =
          std::string(placement_error_name(configuration_error)) + ": " + configuration_diagnostic;
      return execution_failure(placement_execution_error(configuration_error));
    }
    // Steady-state revalidation: cheap identity inputs (sched_getaffinity +
    // stat metadata of the consumed topology files) prove the cached
    // snapshot still holds without constructing snapshot containers or
    // parsing file content. Any change falls through to the full discovery,
    // preserving the automatic quiesce-and-rebuild contract.
    if (placement.has_value() &&
        placement_snapshot_current(options.paths, options.policy, *placement)) {
      diagnostic.clear();
      return {};
    }
    auto discovered = discover_cpu_placement(options.paths, options.policy);
    if (!discovered.ok()) {
      pools.clear();
      placement.reset();
      diagnostic =
          std::string(placement_error_name(discovered.error)) + ": " + discovered.diagnostic;
      return execution_failure(placement_execution_error(discovered.error));
    }
    if (placement.has_value() && equivalent(*placement, *discovered.snapshot)) {
      diagnostic.clear();
      return {};
    }

    const std::uint64_t generation = placement.has_value() ? placement->generation + 1U : 1U;
    pools.clear();
    std::vector<std::unique_ptr<NodePool>> replacement;
    replacement.reserve(discovered.snapshot->pools.size());
    for (const auto& pool_placement : discovered.snapshot->pools) {
      auto pool = std::make_unique<NodePool>(pool_placement.numa_node, pool_placement.worker_cpus);
      std::string startup_diagnostic;
      if (!pool->start(startup_diagnostic)) {
        replacement.clear();
        placement.reset();
        diagnostic = std::move(startup_diagnostic);
        return execution_failure(ExecutionError::PlacementUnavailable);
      }
      replacement.push_back(std::move(pool));
    }
    discovered.snapshot->generation = generation;
    placement = std::move(*discovered.snapshot);
    pools = std::move(replacement);
    ++statistics.topology_rebuilds;
    diagnostic.clear();
    return {};
  }

  [[nodiscard]] ExecutionResult run(std::uint64_t count, CtaTask task, bool whole_kernel,
                                    std::stop_token cancellation) {
    if (count == 0U || count > std::numeric_limits<std::size_t>::max() || !task) {
      return execution_failure(ExecutionError::InvalidLaunch);
    }
    if (!acquire_boundary(cancellation)) {
      return execution_failure(ExecutionError::Cancelled);
    }
    BoundaryLease boundary{*this};
    if (cancellation.stop_requested()) {
      return execution_failure(ExecutionError::Cancelled);
    }
    {
      std::lock_guard lock(state_mutex);
      ++statistics.kernel_boundaries;
      if (auto refreshed = ensure_placement(); !refreshed.ok()) {
        return refreshed;
      }
    }
    if (cancellation.stop_requested()) {
      return execution_failure(ExecutionError::Cancelled);
    }

    // Steady-state launch is allocation-free: the boundary serializes
    // launches, so one reusable Batch and one jobs-per-pool vector serve
    // every launch after the first. Batch outlives the worker threads
    // because the pools (which join their workers on destruction) are
    // declared after batch_storage and are rebuilt only between launches.
    if (!batch_storage) {
      batch_storage = std::make_unique<Batch>();
    }
    Batch* batch = batch_storage.get();
    try {
      batch->task = std::move(task);
      batch->cancellation = cancellation;
      batch->remaining = count;
      batch->first_failure.reset();
      batch->cancellation_observed = false;
      jobs_per_pool.assign(pools.size(), 0U);
      const auto pool_count = static_cast<std::uint64_t>(pools.size());
      const auto jobs_per_pool_floor = count / pool_count;
      const auto pools_with_extra_job = count % pool_count;
      for (std::size_t index = 0; index < pools.size(); ++index) {
        jobs_per_pool[index] = static_cast<std::size_t>(jobs_per_pool_floor +
                                                        (index < pools_with_extra_job ? 1U : 0U));
      }
      for (std::size_t index = 0; index < pools.size(); ++index) {
        if (cancellation.stop_requested()) {
          return execution_failure(ExecutionError::Cancelled);
        }
        pools[index]->reserve_jobs(jobs_per_pool[index]);
      }
    } catch (...) {
      return execution_failure(ExecutionError::System);
    }

    std::stop_callback cancel_pending(cancellation, [this, batch] {
      for (auto& pool : pools) {
        pool->discard_pending(batch);
      }
    });
    std::uint64_t dispatched = 0;
    for (std::uint64_t index = 0; index < count; ++index) {
      if (cancellation.stop_requested()) {
        break;
      }
      pools[static_cast<std::size_t>(index % pools.size())]->enqueue(
          Job{.index = index, .batch = batch});
      ++dispatched;
    }
    if (dispatched != count) {
      batch->discard(dispatched, count - dispatched);
    }
    auto result = batch->wait();
    {
      std::lock_guard lock(state_mutex);
      if (whole_kernel) {
        ++statistics.whole_kernel_jobs;
      } else {
        statistics.cta_jobs += count;
      }
    }
    return result;
  }

  CpuExecutorOptions options;
  PlacementError configuration_error = PlacementError::None;
  std::string configuration_diagnostic;
  // Declared before the pools so the reusable Batch is destroyed after the
  // worker threads have been joined by pool destruction.
  std::unique_ptr<Batch> batch_storage;
  std::vector<std::size_t> jobs_per_pool;
  std::mutex boundary_mutex;
  std::condition_variable_any boundary_condition;
  bool boundary_active = false;
  mutable std::mutex state_mutex;
  std::optional<PlacementSnapshot> placement;
  std::vector<std::unique_ptr<NodePool>> pools;
  CpuExecutorStatistics statistics;
  std::string diagnostic;
};

CpuExecutor::CpuExecutor() : impl_(std::make_unique<Impl>(placement_paths_from_environment())) {}

CpuExecutor::CpuExecutor(CpuExecutorOptions options)
    : impl_(std::make_unique<Impl>(std::move(options))) {}

CpuExecutor::~CpuExecutor() = default;
CpuExecutor::CpuExecutor(CpuExecutor&&) noexcept = default;
CpuExecutor& CpuExecutor::operator=(CpuExecutor&&) noexcept = default;

ExecutionResult CpuExecutor::refresh() {
  static const std::stop_token no_cancellation;
  if (!impl_->acquire_boundary(no_cancellation)) {
    return execution_failure(ExecutionError::Cancelled);
  }
  Impl::BoundaryLease boundary{*impl_};
  std::lock_guard lock(impl_->state_mutex);
  return impl_->ensure_placement();
}

ExecutionResult CpuExecutor::run_ctas(std::uint64_t count, CtaTask task) {
  return impl_->run(count, std::move(task), false, std::stop_token{});
}

ExecutionResult CpuExecutor::run_ctas(std::uint64_t count, CtaTask task,
                                      std::stop_token cancellation) {
  return impl_->run(count, std::move(task), false, cancellation);
}

ExecutionResult CpuExecutor::run_kernel(KernelTask task) {
  return run_kernel(std::move(task), std::stop_token{});
}

ExecutionResult CpuExecutor::run_kernel(KernelTask task, std::stop_token cancellation) {
  if (!task) {
    return execution_failure(ExecutionError::InvalidLaunch);
  }
  return impl_->run(
      1U, [task = std::move(task)](std::uint64_t, std::uint32_t node) { return task(node); }, true,
      cancellation);
}

std::optional<PlacementSnapshot> CpuExecutor::snapshot() const {
  std::lock_guard lock(impl_->state_mutex);
  return impl_->placement;
}

CpuExecutorStatistics CpuExecutor::statistics() const {
  std::lock_guard lock(impl_->state_mutex);
  return impl_->statistics;
}

std::string CpuExecutor::last_diagnostic() const {
  std::lock_guard lock(impl_->state_mutex);
  return impl_->diagnostic;
}

CpuExecutor& default_cpu_executor() {
  static CpuExecutor executor;
  return executor;
}

} // namespace metaflux::backend::cpu
