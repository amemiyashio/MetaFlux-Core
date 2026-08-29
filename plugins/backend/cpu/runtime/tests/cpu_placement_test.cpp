#include "metaflux/backend/cpu/executor.hpp"
#include "metaflux/backend/cpu/placement.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sched.h>
#include <span>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace {

using metaflux::backend::cpu::CpuExecutor;
using metaflux::backend::cpu::CpuExecutorOptions;
using metaflux::backend::cpu::ExecutionError;
using metaflux::backend::cpu::ExecutionResult;
using metaflux::backend::cpu::PlacementError;
using metaflux::backend::cpu::PlacementPaths;
using metaflux::backend::cpu::PlacementPolicy;

bool expect(bool condition, std::string_view message) {
  if (!condition) {
    std::cerr << "CPU placement test failure: " << message << '\n';
  }
  return condition;
}

class TemporaryTree final {
public:
  TemporaryTree() {
    std::string pattern =
        (std::filesystem::temp_directory_path() / "metaflux-cpu-placement-XXXXXX").string();
    std::vector<char> writable(pattern.begin(), pattern.end());
    writable.push_back('\0');
    const char* created = mkdtemp(writable.data());
    if (created != nullptr) {
      root_ = created;
    }
  }

  ~TemporaryTree() {
    std::error_code ignored;
    std::filesystem::remove_all(root_, ignored);
  }

  TemporaryTree(const TemporaryTree&) = delete;
  TemporaryTree& operator=(const TemporaryTree&) = delete;

  [[nodiscard]] bool valid() const noexcept { return !root_.empty(); }
  [[nodiscard]] const std::filesystem::path& root() const noexcept { return root_; }

private:
  std::filesystem::path root_;
};

class SyntheticPlacement final {
public:
  SyntheticPlacement() {
    if (!tree_.valid()) {
      return;
    }
    paths_.sys_cpu_root = tree_.root() / "cpu";
    paths_.sys_node_root = tree_.root() / "node";
    paths_.cgroup_directory = tree_.root() / "cgroup";
  }

  [[nodiscard]] bool valid() const noexcept { return tree_.valid(); }
  [[nodiscard]] PlacementPaths paths(std::vector<std::uint32_t> affinity) const {
    auto result = paths_;
    result.affinity_override = std::move(affinity);
    return result;
  }

  bool write(std::filesystem::path relative, std::string_view contents) const {
    const auto path = tree_.root() / std::move(relative);
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    if (error) {
      return false;
    }
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output << contents << '\n';
    return output.good();
  }

  bool configure(std::string_view online_cpus, std::string_view cpuset_cpus,
                 std::string_view online_nodes, std::string_view cpuset_mems) const {
    return write("cpu/online", online_cpus) && write("node/online", online_nodes) &&
           write("cgroup/cpuset.cpus.effective", cpuset_cpus) &&
           write("cgroup/cpuset.mems.effective", cpuset_mems);
  }

  bool cpu(std::uint32_t id, std::uint32_t package, std::uint32_t core,
           std::string_view siblings) const {
    const auto base = std::filesystem::path("cpu") / ("cpu" + std::to_string(id)) / "topology";
    return write(base / "physical_package_id", std::to_string(package)) &&
           write(base / "core_id", std::to_string(core)) &&
           write(base / "thread_siblings_list", siblings);
  }

  bool node(std::uint32_t id, std::string_view cpus) const {
    return write(std::filesystem::path("node") / ("node" + std::to_string(id)) / "cpulist", cpus);
  }

private:
  TemporaryTree tree_;
  PlacementPaths paths_;
};

bool configure_four_core_smt(SyntheticPlacement& fixture, bool two_nodes = false) {
  if (!fixture.valid() ||
      !fixture.configure("0-7", "0-7", two_nodes ? "0-1" : "0", two_nodes ? "0-1" : "0")) {
    return false;
  }
  for (std::uint32_t cpu = 0; cpu < 8U; ++cpu) {
    const auto sibling_leader = cpu % 4U;
    const auto core_id = two_nodes ? sibling_leader % 2U : sibling_leader;
    if (!fixture.cpu(cpu, 0U, core_id,
                     std::to_string(sibling_leader) + "," + std::to_string(sibling_leader + 4U))) {
      return false;
    }
  }
  return two_nodes ? fixture.node(0U, "0-1,4-5") && fixture.node(1U, "2-3,6-7")
                   : fixture.node(0U, "0-7");
}

bool test_effective_smt_and_reservation() {
  SyntheticPlacement fixture;
  if (!expect(configure_four_core_smt(fixture), "synthetic SMT topology must be created")) {
    return false;
  }
  const auto result = metaflux::backend::cpu::discover_cpu_placement(
      fixture.paths({0U, 1U, 2U, 3U, 4U, 5U, 6U, 7U}));
  return expect(result.ok(), "four-core SMT placement must resolve") &&
         expect(result.snapshot->effective_cpus.size() == 8U,
                "all three CPU masks must intersect exactly") &&
         expect(result.snapshot->physical_cores.size() == 4U,
                "SMT siblings must collapse into physical cores") &&
         expect(result.snapshot->reserved_control_core.has_value() &&
                    result.snapshot->reserved_control_core->effective_siblings ==
                        std::vector<std::uint32_t>({0U, 4U}),
                "four physical cores must reserve one whole control core") &&
         expect(result.snapshot->pools.size() == 1U && result.snapshot->pools.front().worker_cpus ==
                                                           std::vector<std::uint32_t>({1U, 2U, 3U}),
                "auto mode must choose the first effective sibling without oversubscription");
}

bool test_small_set_and_no_cpu() {
  SyntheticPlacement fixture;
  if (!expect(configure_four_core_smt(fixture), "small-set topology must be created") ||
      !expect(fixture.write("cgroup/cpuset.cpus.effective", "0-1,4-5"),
              "small cpuset must be written")) {
    return false;
  }
  const auto small = metaflux::backend::cpu::discover_cpu_placement(
      fixture.paths({0U, 1U, 2U, 3U, 4U, 5U, 6U, 7U}));
  if (!expect(small.ok(), "two-core effective placement must resolve") ||
      !expect(!small.snapshot->reserved_control_core.has_value(),
              "fewer than four cores must reserve none") ||
      !expect(small.snapshot->worker_count() == 2U,
              "small effective sets must use one worker per core without oversubscription")) {
    return false;
  }

  const auto none = metaflux::backend::cpu::discover_cpu_placement(fixture.paths({2U, 3U}));
  return expect(!none.ok() && none.error == PlacementError::NoEffectiveCpu,
                "empty affinity/online/cpuset intersection needs a stable placement error");
}

bool test_numa_mems_and_pin_loss() {
  SyntheticPlacement fixture;
  if (!expect(configure_four_core_smt(fixture, true), "synthetic NUMA topology must be created") ||
      !expect(fixture.write("cgroup/cpuset.mems.effective", "1"),
              "restricted effective mems must be written")) {
    return false;
  }
  const std::vector<std::uint32_t> all{0U, 1U, 2U, 3U, 4U, 5U, 6U, 7U};
  const auto numa = metaflux::backend::cpu::discover_cpu_placement(fixture.paths(all));
  if (!expect(numa.ok(), "NUMA placement must resolve") ||
      !expect(numa.snapshot->effective_mems == std::vector<std::uint32_t>({1U}),
              "effective mems must intersect online and cgroup masks") ||
      !expect(numa.snapshot->pools.size() == 1U && numa.snapshot->pools.front().numa_node == 1U &&
                  numa.snapshot->pools.front().worker_cpus == std::vector<std::uint32_t>({2U, 3U}),
              "workers must be grouped only on an effective local-memory node")) {
    return false;
  }

  const auto explicit_pin = metaflux::backend::cpu::discover_cpu_placement(
      fixture.paths(all), PlacementPolicy{.explicit_cpu = 6U});
  if (!expect(explicit_pin.ok() && explicit_pin.snapshot->worker_count() == 1U &&
                  explicit_pin.snapshot->pools.front().worker_cpus.front() == 6U,
              "an effective explicit pin must select exactly that processing unit") ||
      !expect(fixture.write("cgroup/cpuset.cpus.effective", "0-5,7"),
              "pin-loss cpuset must be written")) {
    return false;
  }
  const auto lost = metaflux::backend::cpu::discover_cpu_placement(
      fixture.paths(all), PlacementPolicy{.explicit_cpu = 6U});
  return expect(!lost.ok() && lost.error == PlacementError::ExplicitPinLost,
                "loss of an explicit pin must have a stable placement error");
}

std::string cpu_list(std::span<const std::uint32_t> cpus) {
  std::string result;
  for (const auto cpu : cpus) {
    if (!result.empty()) {
      result.push_back(',');
    }
    result += std::to_string(cpu);
  }
  return result;
}

bool test_kernel_boundary_refresh() {
  const auto host_paths = metaflux::backend::cpu::placement_paths_from_environment();
  if (!expect(host_paths.ok(), "default placement environment must be valid")) {
    return false;
  }
  const auto host = metaflux::backend::cpu::discover_cpu_placement(*host_paths.paths);
  if (!expect(host.ok(), "host placement must be discoverable for worker integration")) {
    return false;
  }
  if (host.snapshot->effective_cpus.size() < 2U) {
    std::cout << "CPU placement refresh subtest skipped: fewer than two effective CPUs\n";
    return true;
  }
  const std::array selected{host.snapshot->effective_cpus[0], host.snapshot->effective_cpus[1]};
  SyntheticPlacement fixture;
  const auto selected_list = cpu_list(selected);
  if (!expect(fixture.configure(selected_list, selected_list, "0", "0"),
              "refresh fixture masks must be created") ||
      !expect(fixture.node(0U, selected_list), "refresh fixture NUMA node must be created") ||
      !expect(fixture.cpu(selected[0], 0U, 0U, std::to_string(selected[0])) &&
                  fixture.cpu(selected[1], 0U, 1U, std::to_string(selected[1])),
              "refresh fixture physical cores must be created")) {
    return false;
  }

  CpuExecutor executor(CpuExecutorOptions{
      .paths = fixture.paths({selected[0], selected[1]}),
      .policy = PlacementPolicy{.explicit_cpu = selected[0]},
  });
  const auto first = executor.run_ctas(1U, [&](std::uint64_t, std::uint32_t) {
    return sched_getcpu() == static_cast<int>(selected[0])
               ? ExecutionResult{}
               : ExecutionResult{.diagnostic = metaflux::backend::cpu::ExecutionDiagnostic{
                                     .error = ExecutionError::System}};
  });
  if (!expect(first.ok(), "first explicit-pin kernel must execute on the pinned worker") ||
      !expect(executor.snapshot()->generation == 1U,
              "first kernel boundary must publish placement generation one") ||
      !expect(fixture.write("cgroup/cpuset.cpus.effective", std::to_string(selected[1])),
              "refreshed cpuset must be written")) {
    return false;
  }
  const auto lost =
      executor.run_ctas(1U, [](std::uint64_t, std::uint32_t) { return ExecutionResult{}; });
  if (!expect(!lost.ok() && lost.diagnostic->error == ExecutionError::PlacementPinLost,
              "pin loss must take effect only at the next kernel boundary") ||
      !expect(fixture.write("cgroup/cpuset.cpus.effective", selected_list),
              "auto-refresh cpuset must be restored")) {
    return false;
  }

  CpuExecutor automatic(CpuExecutorOptions{
      .paths = fixture.paths({selected[0], selected[1]}),
      .policy = {},
  });
  if (!expect(automatic.run_ctas(2U, [](std::uint64_t, std::uint32_t) { return ExecutionResult{}; })
                  .ok(),
              "initial automatic pool must execute") ||
      !expect(automatic.snapshot()->generation == 1U && automatic.snapshot()->worker_count() == 2U,
              "initial automatic pool must contain both small-set cores") ||
      !expect(fixture.write("cgroup/cpuset.cpus.effective", std::to_string(selected[1])),
              "automatic topology refresh must restrict the cpuset")) {
    return false;
  }
  const auto rebuilt = automatic.run_ctas(1U, [&](std::uint64_t, std::uint32_t) {
    return sched_getcpu() == static_cast<int>(selected[1])
               ? ExecutionResult{}
               : ExecutionResult{.diagnostic = metaflux::backend::cpu::ExecutionDiagnostic{
                                     .error = ExecutionError::System}};
  });
  return expect(rebuilt.ok(), "refreshed automatic pool must execute on the remaining CPU") &&
         expect(automatic.snapshot()->generation == 2U &&
                    automatic.snapshot()->worker_count() == 1U &&
                    automatic.statistics().topology_rebuilds == 2U,
                "effective-set changes must quiesce and rebuild at the next kernel boundary");
}

bool test_cooperative_cancellation() {
  using namespace std::chrono_literals;
  const auto host_paths = metaflux::backend::cpu::placement_paths_from_environment();
  if (!expect(host_paths.ok(), "cancellation test placement environment must be valid")) {
    return false;
  }
  const auto host = metaflux::backend::cpu::discover_cpu_placement(*host_paths.paths);
  if (!expect(host.ok() && !host.snapshot->effective_cpus.empty(),
              "cancellation test needs one effective CPU")) {
    return false;
  }
  const auto selected = host.snapshot->effective_cpus.front();
  const auto selected_text = std::to_string(selected);
  SyntheticPlacement fixture;
  if (!expect(fixture.configure(selected_text, selected_text, "0", "0") &&
                  fixture.node(0U, selected_text) && fixture.cpu(selected, 0U, 0U, selected_text),
              "single-worker cancellation fixture must be created")) {
    return false;
  }
  CpuExecutor executor(CpuExecutorOptions{
      .paths = fixture.paths({selected}),
      .policy = {},
  });

  std::mutex coordination_mutex;
  std::condition_variable coordination;
  bool first_entered = false;
  bool release_first = false;
  ExecutionResult first_result;
  std::thread first([&] {
    first_result = executor.run_ctas(1U, [&](std::uint64_t, std::uint32_t) {
      std::unique_lock lock(coordination_mutex);
      first_entered = true;
      coordination.notify_all();
      coordination.wait(lock, [&] { return release_first; });
      return ExecutionResult{};
    });
  });
  {
    std::unique_lock lock(coordination_mutex);
    if (!coordination.wait_for(lock, 5s, [&] { return first_entered; })) {
      release_first = true;
      lock.unlock();
      coordination.notify_all();
      first.join();
      return expect(false, "first kernel must enter its CTA before boundary cancellation");
    }
  }

  std::stop_source boundary_stop;
  bool boundary_waiter_started = false;
  bool boundary_waiter_finished = false;
  std::atomic<bool> second_task_started{false};
  ExecutionResult boundary_result;
  std::thread boundary_waiter([&] {
    {
      std::lock_guard lock(coordination_mutex);
      boundary_waiter_started = true;
    }
    coordination.notify_all();
    boundary_result = executor.run_ctas(
        1U,
        [&](std::uint64_t, std::uint32_t) {
          second_task_started.store(true, std::memory_order_relaxed);
          return ExecutionResult{};
        },
        boundary_stop.get_token());
    {
      std::lock_guard lock(coordination_mutex);
      boundary_waiter_finished = true;
    }
    coordination.notify_all();
  });
  {
    std::unique_lock lock(coordination_mutex);
    coordination.wait(lock, [&] { return boundary_waiter_started; });
  }
  static_cast<void>(boundary_stop.request_stop());
  bool boundary_cancelled_while_blocked = false;
  {
    std::unique_lock lock(coordination_mutex);
    boundary_cancelled_while_blocked =
        coordination.wait_for(lock, 2s, [&] { return boundary_waiter_finished; });
    release_first = true;
  }
  coordination.notify_all();
  boundary_waiter.join();
  first.join();
  if (!expect(boundary_cancelled_while_blocked,
              "a cancelled boundary waiter must finish before the active kernel is released") ||
      !expect(boundary_result.diagnostic.has_value() &&
                  boundary_result.diagnostic->error == ExecutionError::Cancelled,
              "boundary cancellation must return the stable cancellation error") ||
      !expect(!second_task_started.load(std::memory_order_relaxed),
              "a cancelled boundary waiter must not dispatch its CTA") ||
      !expect(first_result.ok(), "the active kernel must remain unaffected by a waiter cancel")) {
    return false;
  }

  std::stop_source dispatch_stop;
  std::atomic<std::uint32_t> executed{0U};
  bool bulk_entered = false;
  bool release_bulk = false;
  ExecutionResult bulk_result;
  std::thread bulk([&] {
    bulk_result = executor.run_ctas(
        1024U,
        [&](std::uint64_t, std::uint32_t) {
          const auto ordinal = executed.fetch_add(1U, std::memory_order_relaxed);
          if (ordinal == 0U) {
            std::unique_lock lock(coordination_mutex);
            bulk_entered = true;
            coordination.notify_all();
            coordination.wait(lock, [&] { return release_bulk; });
          }
          return ExecutionResult{};
        },
        dispatch_stop.get_token());
  });
  {
    std::unique_lock lock(coordination_mutex);
    if (!coordination.wait_for(lock, 5s, [&] { return bulk_entered; })) {
      release_bulk = true;
      lock.unlock();
      coordination.notify_all();
      bulk.join();
      return expect(false, "bulk cancellation kernel must enter its first CTA");
    }
  }
  static_cast<void>(dispatch_stop.request_stop());
  {
    std::lock_guard lock(coordination_mutex);
    release_bulk = true;
  }
  coordination.notify_all();
  bulk.join();
  return expect(bulk_result.diagnostic.has_value() &&
                    bulk_result.diagnostic->error == ExecutionError::Cancelled,
                "dispatch cancellation must return the stable cancellation error") &&
         expect(executed.load(std::memory_order_relaxed) == 1U,
                "all not-yet-started CTAs must be discarded in bulk");
}

bool test_default_environment_strictness() {
  CpuExecutor configured_from_environment;
  if (!expect(configured_from_environment.refresh().ok(),
              "default executor must honor a valid topology environment")) {
    return false;
  }

  const char* previous_value = std::getenv("METAFLUX_CPU_TOPOLOGY_ROOT");
  const std::optional<std::string> previous =
      previous_value == nullptr ? std::nullopt : std::optional<std::string>(previous_value);
  if (!expect(setenv("METAFLUX_CPU_TOPOLOGY_ROOT", "relative-topology", 1) == 0,
              "strictness test must set an invalid topology environment")) {
    return false;
  }
  CpuExecutor invalid_environment;
  const auto invalid = invalid_environment.refresh();
  const auto invalid_diagnostic = invalid_environment.last_diagnostic();
  const bool restored = previous.has_value()
                            ? setenv("METAFLUX_CPU_TOPOLOGY_ROOT", previous->c_str(), 1) == 0
                            : unsetenv("METAFLUX_CPU_TOPOLOGY_ROOT") == 0;
  return expect(restored, "strictness test must restore the topology environment") &&
         expect(invalid.diagnostic.has_value() &&
                    invalid.diagnostic->error == ExecutionError::PlacementUnavailable,
                "invalid default topology environment must reject placement") &&
         expect(!invalid_environment.snapshot().has_value(),
                "invalid default topology environment must not fall back to host placement") &&
         expect(invalid_diagnostic.find("MF_CPU_INVALID_TOPOLOGY") != std::string::npos,
                "invalid default topology environment must retain its stable diagnostic");
}

bool test_stable_names() {
  return expect(metaflux::backend::cpu::placement_error_name(PlacementError::ExplicitPinLost) ==
                    "MF_CPU_EXPLICIT_PIN_LOST",
                "placement errors need stable names") &&
         expect(metaflux::backend::cpu::execution_error_name(ExecutionError::PlacementPinLost) ==
                    "MF_CPU_PLACEMENT_PIN_LOST",
                "execution placement errors need stable names") &&
         expect(metaflux::backend::cpu::execution_error_name(ExecutionError::Cancelled) ==
                    "MF_CPU_CANCELLED",
                "execution cancellation needs a stable name");
}

} // namespace

int main() {
  return test_effective_smt_and_reservation() && test_small_set_and_no_cpu() &&
                 test_numa_mems_and_pin_loss() && test_kernel_boundary_refresh() &&
                 test_cooperative_cancellation() && test_default_environment_strictness() &&
                 test_stable_names()
             ? 0
             : 1;
}
