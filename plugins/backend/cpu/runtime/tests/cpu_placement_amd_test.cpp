#include "metaflux/backend/cpu/executor.hpp"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sched.h>
#include <span>
#include <sstream>
#include <string>
#include <string_view>

namespace {

constexpr std::string_view kReturnKernel = R"kir(MFKIR 2
PTX 9 0
KERNEL placement_return
PARAMETERS 0
SHARED_ALLOCATIONS 0
REGISTERS 0
OPERATIONS 1
OP return - 0 0 0 - 0
END
)kir";

std::string cpu_vendor() {
  std::ifstream input("/proc/cpuinfo");
  for (std::string line; std::getline(input, line);) {
    if (line.starts_with("vendor_id")) {
      const auto separator = line.find(':');
      if (separator != std::string::npos) {
        std::string value = line.substr(separator + 1U);
        value.erase(0U, value.find_first_not_of(" \t"));
        return value;
      }
    }
  }
  return {};
}

bool exactly_pinned_to(std::uint32_t cpu) {
  const std::size_t cpu_count = std::max<std::size_t>(CPU_SETSIZE, cpu + 1U);
  const auto bytes = CPU_ALLOC_SIZE(cpu_count);
  cpu_set_t* mask = CPU_ALLOC(cpu_count);
  if (mask == nullptr) {
    return false;
  }
  CPU_ZERO_S(bytes, mask);
  const bool result = sched_getaffinity(0, bytes, mask) == 0 && CPU_COUNT_S(bytes, mask) == 1 &&
                      CPU_ISSET_S(cpu, bytes, mask) != 0;
  CPU_FREE(mask);
  return result;
}

} // namespace

int main() {
  using namespace metaflux::backend::cpu;
  if (std::getenv("METAFLUX_CPU_TOPOLOGY_ROOT") != nullptr) {
    std::cout << "{\"status\":\"skipped\",\"reason\":\"synthetic_topology_fixture_active\"}\n";
    return 77;
  }
  const std::string vendor = cpu_vendor();
  if (vendor != "AuthenticAMD") {
    std::cout << "{\"status\":\"skipped\",\"reason\":\"host_cpu_vendor_is_not_amd\","
                 "\"observed_vendor\":\""
              << vendor << "\"}\n";
    return 77;
  }

  CpuExecutor executor;
  const auto refreshed = executor.refresh();
  const auto placement = executor.snapshot();
  if (!refreshed.ok() || !placement.has_value()) {
    std::cerr << executor.last_diagnostic() << '\n';
    return 1;
  }
  if (placement->pools.size() != 1U || placement->effective_mems.size() != 1U) {
    std::cout << "{\"status\":\"skipped\","
                 "\"reason\":\"amd_host_is_not_single_numa\",\"pool_count\":"
              << placement->pools.size() << "}\n";
    return 77;
  }

  std::atomic<bool> exact_pinning{true};
  const auto probe_count = std::max<std::uint64_t>(4U, placement->worker_count() * 4U);
  const auto probes = executor.run_ctas(probe_count, [&](std::uint64_t, std::uint32_t node) {
    const int observed = sched_getcpu();
    const auto& pool = placement->pools.front();
    const bool valid = observed >= 0 && node == pool.numa_node &&
                       std::binary_search(pool.worker_cpus.begin(), pool.worker_cpus.end(),
                                          static_cast<std::uint32_t>(observed)) &&
                       exactly_pinned_to(static_cast<std::uint32_t>(observed));
    if (!valid) {
      exact_pinning.store(false, std::memory_order_relaxed);
      return ExecutionResult{.diagnostic = ExecutionDiagnostic{.error = ExecutionError::System}};
    }
    return ExecutionResult{};
  });
  if (!probes.ok() || !exact_pinning.load(std::memory_order_relaxed)) {
    std::cerr << "AMD placement worker did not retain its exact same-node CPU pin\n";
    return 1;
  }

  const auto interpreted =
      execute_kernel_ir(executor, kReturnKernel, std::span<const Argument>{},
                        {.grid_x = 8U, .block_x = 1U, .grid_y = 2U, .block_y = 1U});
  if (!interpreted.ok()) {
    std::cerr << "AMD placement interpreter integration failed: "
              << execution_error_name(interpreted.diagnostic->error) << '\n';
    return 1;
  }
  const auto statistics = executor.statistics();
  if (statistics.cta_jobs != probe_count + 16U || statistics.topology_rebuilds != 1U) {
    std::cerr << "AMD placement executor statistics do not prove CTA-pool execution\n";
    return 1;
  }

  std::cout << "{\"status\":\"measured\",\"vendor\":\"AuthenticAMD\","
               "\"numa_node\":"
            << placement->pools.front().numa_node
            << ",\"effective_cpu_count\":" << placement->effective_cpus.size()
            << ",\"physical_core_count\":" << placement->physical_cores.size()
            << ",\"worker_count\":" << placement->worker_count() << ",\"reserved_control_core\":"
            << (placement->reserved_control_core.has_value() ? "true" : "false")
            << ",\"exact_worker_pinning\":true,\"cross_node_stealing\":false,"
               "\"interpreter_cta_jobs\":16,\"placement_generation\":"
            << placement->generation << "}\n";
  return 0;
}
