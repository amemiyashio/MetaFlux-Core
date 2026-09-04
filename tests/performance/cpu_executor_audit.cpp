// Steady-state executor audit: proves the daemon-side CTA launch path has a
// constant, non-accumulating per-launch heap cost and records what that cost
// is. The kernel-boundary topology revalidation required by the placement
// contract still allocates per launch (snapshot vectors/strings); this row
// pins that cost as an exact per-launch constant so any accidental growth
// or leak in the launch path fails the build, and keeps the remaining
// discovery work visible for the next zero-overhead iteration.
//
// Heap counting interposes BOTH the malloc family and C++ operator new/delete
// (cpu_executor_audit_wrappers.c); calls that resolve entirely inside the
// shared C++ runtime are outside interposition, as for the client ring audit.

#include "metaflux/backend/cpu/executor.hpp"

#include <cstdint>
#include <cstdio>
#include <ctime>

extern "C" {
// Provided by cpu_executor_audit_wrappers.c.
int mf_exec_audit_begin(void);
int mf_exec_audit_end(void);
std::uint64_t mf_exec_audit_heap_allocation_attempts(void);
}

namespace {

constexpr std::uint64_t kWindowLaunches = 512U;
constexpr unsigned kWarmupLaunches = 16U;

[[nodiscard]] std::uint64_t monotonic_raw_ns() {
  timespec now{};
  clock_gettime(CLOCK_MONOTONIC_RAW, &now);
  return static_cast<std::uint64_t>(now.tv_sec) * 1000000000ULL +
         static_cast<std::uint64_t>(now.tv_nsec);
}

} // namespace

int main() {
  using metaflux::backend::cpu::CpuExecutor;
  CpuExecutor executor;
  const auto noop = [](std::uint64_t, std::uint32_t) {
    return metaflux::backend::cpu::ExecutionResult{};
  };

  for (unsigned warmup = 0; warmup < kWarmupLaunches; ++warmup) {
    const auto warmed = executor.run_ctas(1U, noop);
    if (!warmed.ok()) {
      std::printf("executor-audit: FAIL warmup launch\n");
      return 1;
    }
  }
  std::fflush(nullptr);

  // Two equal measurement windows: the steady state must show an identical
  // heap-allocation count in both windows (constant per-launch cost, no
  // accumulation); the constant itself is printed so the remaining
  // per-launch topology-revalidation work stays visible.
  mf_exec_audit_begin();
  const std::uint64_t window_a_start = monotonic_raw_ns();
  for (std::uint64_t launch = 0; launch < kWindowLaunches; ++launch) {
    const auto result = executor.run_ctas(1U, noop);
    if (!result.ok()) {
      mf_exec_audit_end();
      std::printf("executor-audit: FAIL launch in window A\n");
      return 1;
    }
  }
  const std::uint64_t window_a_ns = monotonic_raw_ns() - window_a_start;
  const std::uint64_t heap_a = mf_exec_audit_heap_allocation_attempts();
  mf_exec_audit_end();

  mf_exec_audit_begin();
  const std::uint64_t window_b_start = monotonic_raw_ns();
  for (std::uint64_t launch = 0; launch < kWindowLaunches; ++launch) {
    const auto result = executor.run_ctas(1U, noop);
    if (!result.ok()) {
      mf_exec_audit_end();
      std::printf("executor-audit: FAIL launch in window B\n");
      return 1;
    }
  }
  const std::uint64_t window_b_ns = monotonic_raw_ns() - window_b_start;
  const std::uint64_t heap_b = mf_exec_audit_heap_allocation_attempts();
  mf_exec_audit_end();

  if (heap_a != heap_b) {
    std::printf(
        "executor-audit: FAIL steady state is not constant: window A=%llu, "
        "window B=%llu allocations\n",
        static_cast<unsigned long long>(heap_a),
        static_cast<unsigned long long>(heap_b));
    return 1;
  }

  const std::uint64_t per_launch = heap_a / kWindowLaunches;
  const double mean_a = static_cast<double>(window_a_ns) / static_cast<double>(kWindowLaunches);
  const double mean_b = static_cast<double>(window_b_ns) / static_cast<double>(kWindowLaunches);
  std::printf("METAFLUX_METADATA\tworkload\tcpu-executor-steady-launch\n");
  std::printf("METAFLUX_METADATA\tclock\tCLOCK_MONOTONIC_RAW\n");
  std::printf("METAFLUX_METADATA\twindow_launches\t%llu\n",
              static_cast<unsigned long long>(kWindowLaunches));
  std::printf("METAFLUX_SAMPLE\texecutor_steady_heap_allocs_per_launch\t%llu\n",
              static_cast<unsigned long long>(per_launch));
  std::printf("METAFLUX_SAMPLE\texecutor_steady_launch_ns_window_a\t%.0f\n", mean_a);
  std::printf("METAFLUX_SAMPLE\texecutor_steady_launch_ns_window_b\t%.0f\n", mean_b);
  std::printf("executor-audit: PASS steady per-launch heap allocations=%llu "
              "(kernel-boundary topology revalidation), launch=%.0f ns\n",
              static_cast<unsigned long long>(per_launch), mean_b);
  return 0;
}
