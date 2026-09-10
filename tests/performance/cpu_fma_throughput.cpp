// FP32 FMA kernel throughput benchmark for the CPU backend execution engine.
//
// Drives the real daemon-side execution engine (services/metafluxd/src/execution.cpp)
// through every execution mode (interpreter, cold-jit, warm-jit, administrator
// AOT) with one FP32 FMA kernel so that kernel execution dominates launch
// overhead. Measurement follows the frozen performance contract: per-launch
// CLOCK_MONOTONIC_RAW samples after warmup, emitted as METAFLUX_SAMPLE rows via
// benchmark_common.h. Baseline measurement only: the test asserts functional
// correctness and that each mode actually exercised its claimed path; it makes
// no performance-threshold assertion.

#include "benchmark_common.h"

#include "execution.hpp"

#include "metaflux/backend/cpu.h"
#include "metaflux/compiler/kernel_ir.hpp"
#include "metaflux/compiler/ptx_frontend.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr std::uint32_t kDefaultElementCount = 1024U * 1024U;
constexpr std::uint32_t kBlockThreads = 1024U;
constexpr std::uint32_t kDefaultFmaIterations = 64U;
constexpr std::uint32_t kAccumulators = 16U;
constexpr std::uint32_t kWarmupLaunches = 1U;
constexpr std::uint32_t kSampleLaunches = 3U;
constexpr std::uint32_t kPeerUid = 4242U;

// Every lane runs 16 independent 4-deep FMA chains x <- fma(x, scale, x0)
// starting from x0, then stores the pairwise-reduced sum over all 64 chain
// values. With x0 = 1.0f and scale = 0.5f every intermediate is an exactly
// representable dyadic rational, so the functional check is a deterministic
// bitwise comparison: 16 chains * (1.5 + 1.75 + 1.875 + 1.9375) = 113.0f.
constexpr float kSourceValue = 1.0F;
constexpr float kScaleValue = 0.5F;
constexpr float kExpectedValue = 113.0F;

[[nodiscard]] std::string build_fma_ptx(std::uint32_t fma_iterations) {
  // Kernel IR v2 requires single-assignment PTX registers: every FMA writes a
  // fresh register. The chain is 16 independent 4-deep FMA chains per lane so
  // the out-of-order engine has parallelism within a lane, followed by an SSA
  // pairwise reduction tree into one stored value.
  constexpr std::uint32_t kChains = 16U;
  const std::uint32_t chain_depth = fma_iterations / kChains;
  const std::uint32_t values_begin = 4U;
  const std::uint32_t values_end = values_begin + fma_iterations;
  const std::uint32_t reduction_begin = values_end;
  const std::uint32_t reduction_total = fma_iterations - 1U;
  std::string ptx;
  ptx += ".version 9.0\n";
  ptx += ".target sm_70\n";
  ptx += ".address_size 64\n";
  ptx += ".entry cpu_fma_throughput(\n";
  ptx += "  .param .u64 destination,\n";
  ptx += "  .param .u64 source,\n";
  ptx += "  .param .u32 count,\n";
  ptx += "  .param .f32 scale\n";
  ptx += ")\n";
  ptx += "{\n";
  ptx += "  .reg .pred %p;\n";
  ptx += "  .reg .b32 %r<8>;\n";
  ptx += "  .reg .b64 %rd<8>;\n";
  ptx += "  .reg .f32 %f<" + std::to_string(reduction_begin + reduction_total) + ">;\n";
  ptx += "  ld.param.u64 %rd0, [destination];\n";
  ptx += "  ld.param.u64 %rd1, [source];\n";
  ptx += "  ld.param.u32 %r0, [count];\n";
  ptx += "  ld.param.f32 %f1, [scale];\n";
  ptx += "  mov.u32 %r1, %tid.x;\n";
  ptx += "  mov.u32 %r2, %ctaid.x;\n";
  ptx += "  mov.u32 %r3, %ntid.x;\n";
  ptx += "  mad.lo.u32 %r4, %r2, %r3, %r1;\n";
  ptx += "  setp.ge.u32 %p, %r4, %r0;\n";
  ptx += "  @%p bra done;\n";
  ptx += "  mul.wide.u32 %rd3, %r4, 4;\n";
  ptx += "  add.u64 %rd4, %rd1, %rd3;\n";
  ptx += "  add.u64 %rd5, %rd0, %rd3;\n";
  ptx += "  ld.global.f32 %f0, [%rd4];\n";
  for (std::uint32_t chain = 0U; chain < kChains; ++chain) {
    for (std::uint32_t depth = 0U; depth < chain_depth; ++depth) {
      const auto destination_register = values_begin + chain * chain_depth + depth;
      const auto product_register = depth == 0U ? 0U : destination_register - 1U;
      ptx += "  fma.rn.f32 %f" + std::to_string(destination_register) + ", %f" +
             std::to_string(product_register) + ", %f1, %f0;\n";
    }
  }
  std::uint32_t level_base = values_begin;
  std::uint32_t level_count = fma_iterations;
  std::uint32_t next_register = reduction_begin;
  while (level_count > 1U) {
    for (std::uint32_t index = 0U; index < level_count / 2U; ++index) {
      ptx += "  add.rn.f32 %f" + std::to_string(next_register + index) + ", %f" +
             std::to_string(level_base + 2U * index) + ", %f" +
             std::to_string(level_base + 2U * index + 1U) + ";\n";
    }
    level_base = next_register;
    next_register += level_count / 2U;
    level_count /= 2U;
  }
  ptx += "  st.global.f32 [%rd5], %f" + std::to_string(level_base) + ";\n";
  ptx += "done:\n";
  ptx += "  ret;\n";
  ptx += "}\n";
  return ptx;
}

struct ModeOutcome final {
  bool ok = false;
  std::string diagnostic;
  std::uint64_t compile_ns = 0;
  std::vector<std::uint64_t> sample_ns;
};

[[nodiscard]] std::vector<metaflux::backend::cpu::Argument>
make_arguments(std::span<std::uint32_t> destination, std::span<const std::uint32_t> source,
               std::uint32_t element_count) {
  std::vector<metaflux::backend::cpu::Argument> arguments;
  arguments.emplace_back(metaflux::backend::cpu::BufferArgument{
      .words = destination, .writable = true});
  arguments.emplace_back(metaflux::backend::cpu::BufferArgument{
      .words = std::span<std::uint32_t>(const_cast<std::uint32_t*>(source.data()),
                                        source.size()),
      .writable = false,
  });
  arguments.emplace_back(element_count);
  arguments.emplace_back(metaflux::backend::cpu::Float32Argument{
      .bits = std::bit_cast<std::uint32_t>(kScaleValue)});
  return arguments;
}

[[nodiscard]] bool result_is_expected(std::span<const std::uint32_t> destination,
                                      std::string& diagnostic) {
  const auto expected_bits = std::bit_cast<std::uint32_t>(kExpectedValue);
  for (std::size_t index = 0; index < destination.size(); ++index) {
    if (destination[index] != expected_bits) {
      diagnostic = "element " + std::to_string(index) + " has bits 0x" +
                   [&] {
                       std::array<char, 9> text{};
                       (void)snprintf(text.data(), text.size(), "%08x", destination[index]);
                       return std::string(text.data());
                   }() +
                   " instead of the expected 0x" +
                   [&] {
                     std::array<char, 9> text{};
                     (void)snprintf(text.data(), text.size(), "%08x", expected_bits);
                     return std::string(text.data());
                   }();
      return false;
    }
  }
  return true;
}

[[nodiscard]] std::uint64_t median_sample(const std::vector<std::uint64_t>& samples) {
  auto sorted = samples;
  std::sort(sorted.begin(), sorted.end());
  return sorted.empty() ? 0U : sorted[sorted.size() / 2U];
}

[[nodiscard]] ModeOutcome run_mode(const std::string& mode_name,
                                   const std::filesystem::path& cache_root,
                                   const metaflux::compiler::Kernel& kernel,
                                   std::string_view canonical_kernel_ir,
                                   std::span<std::uint32_t> destination,
                                   std::span<const std::uint32_t> source,
                                   std::uint32_t element_count, std::uint32_t warmup_launches,
                                   std::uint32_t sample_launches) {
  ModeOutcome outcome;
  auto configuration = metaflux::service::parse_cpu_execution_configuration(
      mode_name, cache_root.string(), std::nullopt, std::nullopt);
  if (!configuration.ok()) {
    outcome.diagnostic = "execution configuration '" + mode_name + "' did not parse";
    return outcome;
  }
  if (mode_name != "interpreter") {
#ifdef METAFLUX_DAEMON_EXECUTABLE
    configuration.configuration->compiler_worker.executable = METAFLUX_DAEMON_EXECUTABLE;
#else
    outcome.diagnostic = "daemon compiler worker executable is not configured";
    return outcome;
#endif
  }

  metaflux::service::CpuExecutionEngine engine(std::move(*configuration.configuration));
  std::string diagnostic;
  if (!engine.initialize(diagnostic)) {
    outcome.diagnostic = "engine initialize failed: " + diagnostic;
    return outcome;
  }

  std::uint64_t prepare_started = 0;
  std::uint64_t prepare_finished = 0;
  if (mf_benchmark_now_ns(&prepare_started) != 0) {
    outcome.diagnostic = "clock readout failed before preparation";
    return outcome;
  }
  auto module = engine.prepare(kPeerUid, kernel, std::string(canonical_kernel_ir));
  if (mf_benchmark_now_ns(&prepare_finished) != 0) {
    outcome.diagnostic = "clock readout failed after preparation";
    return outcome;
  }
  if (!module.ok()) {
    outcome.diagnostic = "module prepare failed: " + module.diagnostic;
    return outcome;
  }
  outcome.compile_ns = prepare_finished - prepare_started;

  const auto arguments = make_arguments(destination, source, element_count);
  const metaflux::backend::cpu::LaunchDimensions dimensions{
      .grid_x = element_count / kBlockThreads,
      .block_x = kBlockThreads,
      .grid_y = 1U,
      .block_y = 1U,
  };

  const auto seeded = module.module->launch(arguments, dimensions);
  if (!seeded.ok()) {
    outcome.diagnostic = "first launch failed";
    return outcome;
  }
  if (!result_is_expected(destination, outcome.diagnostic)) {
    outcome.diagnostic = "functional check failed: " + outcome.diagnostic;
    return outcome;
  }

  for (std::uint32_t index = 0; index < warmup_launches; ++index) {
    const auto warmed = module.module->launch(arguments, dimensions);
    if (!warmed.ok()) {
      outcome.diagnostic = "warmup launch failed";
      return outcome;
    }
  }

  outcome.sample_ns.reserve(sample_launches);
  for (std::uint32_t index = 0; index < sample_launches; ++index) {
    std::uint64_t started = 0;
    std::uint64_t finished = 0;
    if (mf_benchmark_now_ns(&started) != 0) {
      outcome.diagnostic = "clock readout failed before launch";
      return outcome;
    }
    const auto launched = module.module->launch(arguments, dimensions);
    if (mf_benchmark_now_ns(&finished) != 0 || finished < started) {
      outcome.diagnostic = "clock readout failed after launch";
      return outcome;
    }
    if (!launched.ok()) {
      outcome.diagnostic = "sampled launch failed";
      return outcome;
    }
    outcome.sample_ns.push_back(finished - started);
  }

  // Prove the claimed execution path was actually exercised.
  const auto statistics = engine.statistics();
  if (mode_name == "interpreter") {
    if (statistics.compiler_requests != 0U || statistics.loaded_modules != 1U) {
      outcome.diagnostic = "interpreter mode invoked the compiler";
      return outcome;
    }
  } else if (mode_name == "cold-jit") {
    if (statistics.compiler_requests != 1U || statistics.compiler_worker_launches != 1U ||
        statistics.cache_misses != 1U || statistics.cache_hits != 0U) {
      outcome.diagnostic = "cold JIT statistics do not show exactly one compile";
      return outcome;
    }
  } else if (mode_name == "warm-jit") {
    if (statistics.compiler_requests != 0U || statistics.cache_hits != 1U ||
        statistics.cache_misses != 0U) {
      outcome.diagnostic = "warm JIT statistics do not show a lookup-only hit";
      return outcome;
    }
  } else if (mode_name == "aot") {
    if (statistics.compiler_requests != 0U || statistics.compiler_worker_launches != 0U ||
        statistics.cache_hits != 1U) {
      outcome.diagnostic = "AOT statistics do not show a prewarmed artifact hit";
      return outcome;
    }
  }
  outcome.ok = true;
  return outcome;
}

} // namespace

int main(int argc, char** argv) {
  std::string mode_selector = "all";
  std::uint32_t element_count = kDefaultElementCount;
  std::uint32_t fma_iterations = kDefaultFmaIterations;
  std::uint32_t warmup_launches = kWarmupLaunches;
  std::uint32_t sample_launches = kSampleLaunches;
  const auto decimal_argument = [](const char* text) {
    return text != nullptr && std::strspn(text, "0123456789") == std::strlen(text) &&
           text[0] != '\0';
  };
  int argument_index = 1;
  if (argument_index < argc && !decimal_argument(argv[argument_index])) {
    mode_selector = argv[argument_index];
    ++argument_index;
  }
  const bool arguments_valid = argc <= 6 && mode_selector != "--help" &&
                               (argument_index >= argc ||
                                mf_benchmark_parse_u32(argv[argument_index++], &element_count) ==
                                    0) &&
                               (argument_index >= argc ||
                                mf_benchmark_parse_u32(argv[argument_index++],
                                                       &fma_iterations) == 0) &&
                               (argument_index >= argc ||
                                mf_benchmark_parse_u32(argv[argument_index++],
                                                       &warmup_launches) == 0) &&
                               (argument_index >= argc ||
                                mf_benchmark_parse_u32(argv[argument_index++],
                                                       &sample_launches) == 0);
  if (!arguments_valid) {
    (void)fprintf(stderr,
                  "usage: %s [MODE=all|interpreter|cold-jit|warm-jit|aot] "
                  "[ELEMENTS=%u] [FMA_ITERATIONS=%u] [WARMUP=%u] [SAMPLES=%u]\n",
                  argv[0], kDefaultElementCount, kDefaultFmaIterations, kWarmupLaunches,
                  kSampleLaunches);
    return 2;
  }

  if (element_count == 0U || element_count % kBlockThreads != 0U ||
      element_count / kBlockThreads > (1U << 20U)) {
    (void)fprintf(stderr, "ELEMENTS must be a positive multiple of %u within the CTA grid bound\n",
                  kBlockThreads);
    return 2;
  }
  if (fma_iterations == 0U || fma_iterations % kAccumulators != 0U || sample_launches == 0U) {
    (void)fprintf(stderr, "FMA_ITERATIONS must be a positive multiple of %u and SAMPLES positive\n",
                  kAccumulators);
    return 2;
  }

  std::array<char, 64> pattern{};
  const std::string prefix = "/tmp/metaflux-cpu-fma-XXXXXX";
  std::copy(prefix.begin(), prefix.end(), pattern.begin());
  if (mkdtemp(pattern.data()) == nullptr) {
    (void)fprintf(stderr, "temporary benchmark directory creation failed\n");
    return 1;
  }
  const std::filesystem::path temporary(pattern.data());
  // Optional escape hatch for offline analysis: keep the artifact cache and
  // compiled kernel ELFs under an explicit root instead of a deleted temporary.
  const char* external_cache_root = std::getenv("METAFLUX_CPU_FMA_CACHE_ROOT");
  const std::filesystem::path cache_root =
      external_cache_root != nullptr && external_cache_root[0] != '\0'
          ? std::filesystem::path(external_cache_root)
          : temporary / "cache";
  const auto cleanup_temporary = [&] {
    if (external_cache_root != nullptr && external_cache_root[0] != '\0') {
      return;
    }
    std::error_code ignored;
    std::filesystem::remove_all(temporary, ignored);
  };

  const std::string ptx_text = build_fma_ptx(fma_iterations);
  const auto ptx_path = temporary / "cpu_fma_throughput.ptx";
  {
    std::ofstream ptx_file(ptx_path, std::ios::binary | std::ios::trunc);
    if (!ptx_file) {
      (void)fprintf(stderr, "benchmark PTX file creation failed\n");
      cleanup_temporary();
      return 1;
    }
    ptx_file.write(ptx_text.data(), static_cast<std::streamsize>(ptx_text.size()));
  }

  const auto parsed = metaflux::compiler::ptx::parse(ptx_text);
  if (!parsed.ok()) {
    (void)fprintf(stderr, "benchmark PTX did not parse: %s\n",
                  parsed.diagnostics.empty() ? "unknown"
                                             : parsed.diagnostics.front().message.c_str());
    cleanup_temporary();
    return 1;
  }
  const auto serialized = metaflux::compiler::serialize_kernel(*parsed.kernel);
  if (!serialized.ok()) {
    (void)fprintf(stderr, "benchmark kernel did not serialize\n");
    cleanup_temporary();
    return 1;
  }

  std::vector<std::uint32_t> destination(element_count, 0U);
  std::vector<std::uint32_t> source(element_count,
                                    std::bit_cast<std::uint32_t>(kSourceValue));
  const std::uint64_t flops_per_launch =
      static_cast<std::uint64_t>(element_count) * fma_iterations * 2U;

  mf_benchmark_emit_metadata_u64("elements", element_count);
  mf_benchmark_emit_metadata_u64("fma_iterations", fma_iterations);
  mf_benchmark_emit_metadata_u64("flops_per_launch", flops_per_launch);
  mf_benchmark_emit_metadata_u64("grid_ctas", element_count / kBlockThreads);
  mf_benchmark_emit_metadata_u64("block_threads", kBlockThreads);
  mf_benchmark_emit_metadata_text("clock", "CLOCK_MONOTONIC_RAW");

  const std::array<std::string, 4> modes{"interpreter", "cold-jit", "warm-jit", "aot"};
  bool aot_prewarmed = false;
  bool all_ok = true;
  for (const auto& mode_name : modes) {
    if (mode_selector != "all" && mode_selector != mode_name) {
      continue;
    }
    if (mode_name == "aot" && !aot_prewarmed) {
      // The administrator AOT tier is lookup-only; populate it exactly the way
      // the daemon's prewarm flow does before the AOT engine runs.
      auto prewarm_configuration = metaflux::service::parse_cpu_execution_configuration(
          "aot", cache_root.string(), std::nullopt, std::nullopt);
#ifdef METAFLUX_DAEMON_EXECUTABLE
      if (prewarm_configuration.ok()) {
        prewarm_configuration.configuration->compiler_worker.executable =
            METAFLUX_DAEMON_EXECUTABLE;
      }
#endif
      if (!prewarm_configuration.ok()) {
        (void)fprintf(stderr, "mode %s failed: prewarm configuration did not parse\n",
                      mode_name.c_str());
        all_ok = false;
        continue;
      }
      std::uint64_t prewarm_started = 0;
      std::uint64_t prewarm_finished = 0;
      (void)mf_benchmark_now_ns(&prewarm_started);
      const auto prewarmed = metaflux::service::prewarm_aot_file(
          *prewarm_configuration.configuration, ptx_path.string());
      (void)mf_benchmark_now_ns(&prewarm_finished);
      if (!prewarmed.success) {
        (void)fprintf(stderr, "mode %s failed: AOT prewarm failed: %s\n", mode_name.c_str(),
                      prewarmed.diagnostic.c_str());
        all_ok = false;
        continue;
      }
      mf_benchmark_emit_metadata_u64("aot_prewarm_compile_ns",
                                     prewarm_finished - prewarm_started);
      aot_prewarmed = true;
    }
    const auto outcome = run_mode(mode_name, cache_root, *parsed.kernel,
                                  serialized.text, destination, source, element_count,
                                  warmup_launches, sample_launches);
    mf_benchmark_emit_metadata_text("execution_mode", mode_name.c_str());
    if (!outcome.ok) {
      (void)fprintf(stderr, "mode %s failed: %s\n", mode_name.c_str(),
                    outcome.diagnostic.c_str());
      all_ok = false;
      continue;
    }
    mf_benchmark_emit_metadata_u64("compile_ns", outcome.compile_ns);
    for (std::uint32_t index = 0; index < outcome.sample_ns.size(); ++index) {
      const auto flops_per_second = static_cast<std::uint64_t>(
          static_cast<double>(flops_per_launch) * 1e9 /
          static_cast<double>(outcome.sample_ns[index]));
      mf_benchmark_emit_sample("cpu_fma_throughput", index, flops_per_second, "flop/s");
      mf_benchmark_emit_sample("cpu_fma_launch_ns", index, outcome.sample_ns[index], "ns");
    }
    const auto median_ns = median_sample(outcome.sample_ns);
    mf_benchmark_emit_metadata_u64("median_launch_ns", median_ns);
    const auto median_flops_per_second = static_cast<std::uint64_t>(
        static_cast<double>(flops_per_launch) * 1e9 / static_cast<double>(median_ns));
    mf_benchmark_emit_metadata_u64("median_flops_per_second", median_flops_per_second);
    char tflops[32];
    (void)snprintf(tflops, sizeof(tflops), "%.3f",
                   static_cast<double>(median_flops_per_second) / 1e12);
    mf_benchmark_emit_metadata_text(("median_tflops_" + mode_name).c_str(), tflops);
  }

  cleanup_temporary();
  return all_ok ? 0 : 1;
}
