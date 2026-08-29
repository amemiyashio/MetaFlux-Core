#include "execution.hpp"

#include "metaflux/compiler/kernel_ir.hpp"
#include "metaflux/compiler/ptx_frontend.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <optional>
#include <span>
#include <stop_token>
#include <string>
#include <string_view>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

namespace {

class TemporaryDirectory final {
public:
  TemporaryDirectory() {
    std::array<char, 64> pattern{};
    const std::string prefix = "/tmp/metafluxd-execution-XXXXXX";
    std::copy(prefix.begin(), prefix.end(), pattern.begin());
    if (mkdtemp(pattern.data()) != nullptr) {
      path_ = pattern.data();
    }
  }

  ~TemporaryDirectory() {
    if (!path_.empty()) {
      std::error_code error;
      std::filesystem::remove_all(path_, error);
    }
  }

  [[nodiscard]] const std::filesystem::path& path() const noexcept { return path_; }

private:
  std::filesystem::path path_;
};

[[nodiscard]] bool expect(bool condition, std::string_view message) {
  if (!condition) {
    std::cerr << "metafluxd execution test: " << message << '\n';
  }
  return condition;
}

[[nodiscard]] std::optional<std::string> read_source(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    return std::nullopt;
  }
  return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

[[nodiscard]] bool launch_add(metaflux::service::PreparedModule& module) {
  std::array<std::uint32_t, 8> destination{};
  std::array<std::uint32_t, 8> left{1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U};
  std::array<std::uint32_t, 8> right{8U, 7U, 6U, 5U, 4U, 3U, 2U, 1U};
  std::vector<metaflux::backend::cpu::Argument> arguments;
  arguments.emplace_back(metaflux::backend::cpu::BufferArgument{
      .words = destination,
      .writable = true,
  });
  arguments.emplace_back(metaflux::backend::cpu::BufferArgument{
      .words = left,
      .writable = false,
  });
  arguments.emplace_back(metaflux::backend::cpu::BufferArgument{
      .words = right,
      .writable = false,
  });
  arguments.emplace_back(static_cast<std::uint32_t>(destination.size()));
  const auto result = module.launch(
      arguments, metaflux::backend::cpu::LaunchDimensions{.grid_x = 1U, .block_x = 8U});
  if (!result.ok()) {
    return false;
  }
  for (const auto value : destination) {
    if (value != 9U) {
      return false;
    }
  }
  return true;
}

[[nodiscard]] bool launch_cancelled(metaflux::service::PreparedModule& module) {
  std::array<std::uint32_t, 8> destination{};
  std::array<std::uint32_t, 8> source{};
  source.fill(1U);
  std::vector<metaflux::backend::cpu::Argument> arguments;
  arguments.emplace_back(
      metaflux::backend::cpu::BufferArgument{.words = destination, .writable = true});
  arguments.emplace_back(
      metaflux::backend::cpu::BufferArgument{.words = source, .writable = false});
  arguments.emplace_back(
      metaflux::backend::cpu::BufferArgument{.words = source, .writable = false});
  arguments.emplace_back(static_cast<std::uint32_t>(destination.size()));
  std::stop_source cancellation;
  static_cast<void>(cancellation.request_stop());
  const auto result = module.launch(
      arguments, metaflux::backend::cpu::LaunchDimensions{.grid_x = 1U, .block_x = 8U},
      cancellation.get_token());
  return result.diagnostic.has_value() &&
         result.diagnostic->error == metaflux::backend::cpu::ExecutionError::Cancelled &&
         std::all_of(destination.begin(), destination.end(),
                     [](std::uint32_t value) { return value == 0U; });
}

[[nodiscard]] metaflux::service::CpuExecutionConfigurationResult
execution_configuration(std::string_view mode, const std::filesystem::path& cache_root) {
  const char* topology_root = std::getenv("METAFLUX_CPU_TOPOLOGY_ROOT");
  return metaflux::service::parse_cpu_execution_configuration(
      mode, cache_root.string(), std::nullopt,
      topology_root == nullptr ? std::nullopt : std::optional<std::string_view>(topology_root));
}

[[nodiscard]] bool test_configuration_parser() {
  using metaflux::service::CpuExecutionMode;
  const auto defaults =
      metaflux::service::parse_cpu_execution_configuration(std::nullopt, std::nullopt);
  const auto interpreter = metaflux::service::parse_cpu_execution_configuration(
      "interpreter", "/tmp/metaflux-cache-policy");
  const auto cold = metaflux::service::parse_cpu_execution_configuration("cold-jit", std::nullopt);
  const auto warm = metaflux::service::parse_cpu_execution_configuration("warm-jit", std::nullopt);
  const auto aot = metaflux::service::parse_cpu_execution_configuration("aot", std::nullopt);
  const auto pinned =
      metaflux::service::parse_cpu_execution_configuration("interpreter", std::nullopt, "7");
  const auto topology = metaflux::service::parse_cpu_execution_configuration(
      "interpreter", std::nullopt, std::nullopt, "/tmp/metaflux-cpu-topology");
  return expect(defaults.ok() && defaults.configuration->mode == CpuExecutionMode::Interpreter,
                "unset mode must default to interpreter") &&
         expect(interpreter.ok() &&
                    interpreter.configuration->cache.mutable_root ==
                        "/tmp/metaflux-cache-policy/mutable" &&
                    interpreter.configuration->cache.aot_root == "/tmp/metaflux-cache-policy/aot",
                "cache override must split mutable and AOT roots") &&
         expect(cold.ok() && cold.configuration->mode == CpuExecutionMode::ColdJit,
                "cold-jit must parse exactly") &&
         expect(warm.ok() && warm.configuration->mode == CpuExecutionMode::WarmJit,
                "warm-jit must parse exactly") &&
         expect(aot.ok() && aot.configuration->mode == CpuExecutionMode::Aot,
                "aot must parse exactly") &&
         expect(pinned.ok() && pinned.configuration->placement.explicit_cpu == 7U,
                "one explicit CPU pin must parse exactly") &&
         expect(topology.ok() &&
                    topology.configuration->placement_paths.sys_cpu_root ==
                        "/tmp/metaflux-cpu-topology/cpu" &&
                    topology.configuration->placement_paths.sys_node_root ==
                        "/tmp/metaflux-cpu-topology/node" &&
                    topology.configuration->placement_paths.cgroup_directory ==
                        std::filesystem::path("/tmp/metaflux-cpu-topology/cgroup"),
                "topology root must map to the CPU, node, and cgroup placement paths") &&
         expect(!metaflux::service::parse_cpu_execution_configuration("interpreter", std::nullopt,
                                                                      "7-8")
                     .ok(),
                "CPU pin ranges must be rejected") &&
         expect(
             !metaflux::service::parse_cpu_execution_configuration("COLD-JIT", std::nullopt).ok(),
             "mode aliases must be rejected") &&
         expect(!metaflux::service::parse_cpu_execution_configuration("", std::nullopt).ok(),
                "empty mode must be rejected") &&
         expect(
             !metaflux::service::parse_cpu_execution_configuration("interpreter", "relative").ok(),
             "relative cache root must be rejected") &&
         expect(!metaflux::service::parse_cpu_execution_configuration("interpreter", "/").ok(),
                "filesystem root must be rejected") &&
         expect(!metaflux::service::parse_cpu_execution_configuration("interpreter",
                                                                      "/tmp/metaflux/../other")
                     .ok(),
                "parent components in cache root must be rejected") &&
         expect(!metaflux::service::parse_cpu_execution_configuration("interpreter", std::nullopt,
                                                                      std::nullopt, "relative")
                     .ok(),
                "relative topology root must be rejected") &&
         expect(!metaflux::service::parse_cpu_execution_configuration("interpreter", std::nullopt,
                                                                      std::nullopt, "/")
                     .ok(),
                "filesystem root must be rejected as a topology root") &&
         expect(!metaflux::service::parse_cpu_execution_configuration(
                     "interpreter", std::nullopt, std::nullopt, "/tmp/metaflux-topology/../other")
                     .ok(),
                "parent components in topology root must be rejected") &&
         expect(!metaflux::service::parse_cpu_execution_configuration("interpreter", std::nullopt,
                                                                      std::nullopt, "")
                     .ok(),
                "empty topology root must be rejected");
}

[[nodiscard]] bool test_execution_modes() {
  constexpr std::uint32_t kFirstUid = 1001U;
  constexpr std::uint32_t kSecondUid = 2002U;
  TemporaryDirectory temporary;
  if (!expect(!temporary.path().empty(), "temporary cache directory must be available")) {
    return false;
  }
  const auto source = read_source(METAFLUX_DAEMON_ADD_PTX);
  if (!expect(source.has_value(), "canonical Add PTX fixture must be readable")) {
    return false;
  }
  const auto parsed = metaflux::compiler::ptx::parse(*source);
  if (!expect(parsed.ok(), "canonical Add PTX fixture must parse")) {
    return false;
  }
  const auto serialized = metaflux::compiler::serialize_kernel(*parsed.kernel);
  if (!expect(serialized.ok(), "canonical Add kernel must serialize")) {
    return false;
  }
  const auto cache_root = temporary.path() / "cache";
  metaflux::compiler::Kernel compute_only;
  compute_only.name = "compute_only";
  compute_only.operations.push_back(
      metaflux::compiler::Operation{.opcode = metaflux::compiler::Opcode::Return});
  const auto compute_only_serialized = metaflux::compiler::serialize_kernel(compute_only);
  auto interpreter_configuration = execution_configuration("interpreter", cache_root);
  if (!expect(compute_only_serialized.ok() && interpreter_configuration.ok(),
              "compute-only Kernel IR and interpreter configuration must be valid")) {
    return false;
  }
  {
    metaflux::service::CpuExecutionEngine interpreter(
        std::move(*interpreter_configuration.configuration));
    std::string diagnostic;
    if (!expect(interpreter.initialize(diagnostic), "interpreter must initialize")) {
      return false;
    }
    auto memory_module = interpreter.prepare(kFirstUid, *parsed.kernel, serialized.text);
    auto compute_module =
        interpreter.prepare(kFirstUid, compute_only, compute_only_serialized.text);
    if (!expect(memory_module.ok() && memory_module.module->accesses_global_memory(),
                "global load/store Kernel IR must classify as memory-active") ||
        !expect(compute_module.ok() && !compute_module.module->accesses_global_memory(),
                "Kernel IR without global load/store must stay compute-only")) {
      return false;
    }
  }
  auto configuration = execution_configuration("cold-jit", cache_root);
  if (!expect(configuration.ok(), "cold JIT configuration must parse")) {
    return false;
  }
  configuration.configuration->compiler_worker.executable = METAFLUX_DAEMON_EXECUTABLE;

  {
    metaflux::service::CpuExecutionEngine cold(std::move(*configuration.configuration));
    std::string diagnostic;
    if (!expect(cold.initialize(diagnostic), "cold JIT cache must initialize")) {
      std::cerr << diagnostic << '\n';
      return false;
    }
    auto module = cold.prepare(kFirstUid, *parsed.kernel, serialized.text);
    const auto first_statistics = cold.statistics();
    if (!expect(module.ok() && module.module->accesses_global_memory(),
                "cold JIT miss must compile, load, and retain memory-access metadata") ||
        !expect(first_statistics.compiler_requests == 1U &&
                    first_statistics.compiler_worker_launches == 1U &&
                    first_statistics.compiler_worker_failures == 0U &&
                    first_statistics.last_compiler_worker_pid !=
                        static_cast<std::int64_t>(getpid()) &&
                    first_statistics.cache_misses == 1U && first_statistics.cache_hits == 0U,
                "cold JIT miss must invoke exactly one compiler request") ||
        !expect(launch_cancelled(*module.module),
                "a pre-cancelled compiled module must not enter native CTA code") ||
        !expect(launch_add(*module.module), "cold JIT Add must execute") ||
        !expect(std::filesystem::is_directory(cache_root / "mutable" / "users" /
                                              std::to_string(kFirstUid) / "epoch-1"),
                "mutable cache must be rooted under the authenticated UID")) {
      if (!module.diagnostic.empty()) {
        std::cerr << module.diagnostic << '\n';
      }
      return false;
    }
    auto repeated = cold.prepare(kFirstUid, *parsed.kernel, serialized.text);
    const auto repeated_statistics = cold.statistics();
    if (!expect(repeated.ok(), "cold JIT must reuse its generated cache artifact") ||
        !expect(repeated_statistics.compiler_requests == 1U &&
                    repeated_statistics.cache_misses == 1U && repeated_statistics.cache_hits == 1U,
                "cold JIT cache hit must not invoke a second compiler request") ||
        !expect(launch_add(*repeated.module), "cold JIT cached Add must execute")) {
      if (!repeated.diagnostic.empty()) {
        std::cerr << repeated.diagnostic << '\n';
      }
      return false;
    }
    if (!expect(cold.statistics().executor.cta_jobs == 2U &&
                    cold.statistics().executor.whole_kernel_jobs == 0U,
                "compiled launch must execute as CTA work through the CPU worker pool")) {
      return false;
    }
  }

  configuration = execution_configuration("warm-jit", cache_root);
  configuration.configuration->compiler_worker.executable = METAFLUX_DAEMON_EXECUTABLE;
  {
    metaflux::service::CpuExecutionEngine warm(std::move(*configuration.configuration));
    std::string diagnostic;
    if (!expect(warm.initialize(diagnostic), "warm JIT cache must initialize")) {
      return false;
    }
    auto isolated_miss = warm.prepare(kSecondUid, *parsed.kernel, serialized.text);
    auto hit = warm.prepare(kFirstUid, *parsed.kernel, serialized.text);
    const auto statistics = warm.statistics();
    if (!expect(!isolated_miss.ok() &&
                    isolated_miss.error == metaflux::service::ModulePreparationError::CacheMiss,
                "a different peer UID must not consume mutable cache content") ||
        !expect(hit.ok(), "warm JIT must load the authenticated UID's artifact") ||
        !expect(statistics.compiler_requests == 0U && statistics.cache_hits == 1U &&
                    statistics.cache_misses == 1U,
                "warm JIT lookup must never invoke the compiler") ||
        !expect(launch_add(*hit.module), "warm JIT Add must execute") ||
        !expect(
            !std::filesystem::exists(cache_root / "mutable" / "users" / std::to_string(kSecondUid)),
            "warm miss must not materialize another UID directory")) {
      return false;
    }
  }

  configuration = execution_configuration("aot", cache_root);
  configuration.configuration->compiler_worker.executable = METAFLUX_DAEMON_EXECUTABLE;
  {
    metaflux::service::CpuExecutionEngine aot_miss(*configuration.configuration);
    std::string diagnostic;
    if (!expect(aot_miss.initialize(diagnostic), "AOT lookup-only mode must initialize") ||
        !expect(!aot_miss.prepare(kFirstUid, *parsed.kernel, serialized.text).ok(),
                "unpopulated AOT cache must miss") ||
        !expect(aot_miss.statistics().compiler_requests == 0U,
                "AOT miss must not invoke the compiler") ||
        !expect(!std::filesystem::exists(cache_root / "aot" / ".mutable-tier-disabled"),
                "AOT miss must not create a mutable cache path")) {
      return false;
    }
  }

  const auto prewarm =
      metaflux::service::prewarm_aot_file(*configuration.configuration, METAFLUX_DAEMON_ADD_PTX);
  if (!expect(prewarm.success && prewarm.compiled, "first AOT prewarm must compile") ||
      !expect(prewarm.cache_key.size() >= 64U, "AOT prewarm must return its cache key")) {
    std::cerr << prewarm.diagnostic << '\n';
    return false;
  }
  const auto digest = prewarm.cache_key.substr(prewarm.cache_key.size() - 64U);
  const auto aot_elf =
      cache_root / "aot" / "epoch-1" / digest.substr(0U, 2U) / digest / "kernel.so";
  struct stat attributes{};
  if (!expect(stat(aot_elf.c_str(), &attributes) == 0 && (attributes.st_mode & 0222U) == 0U,
              "prewarmed AOT ELF must be read-only")) {
    return false;
  }

  configuration.configuration->compiler_worker.executable = temporary.path() / "missing-worker";
  {
    metaflux::service::CpuExecutionEngine aot(std::move(*configuration.configuration));
    std::string diagnostic;
    if (!expect(aot.initialize(diagnostic), "prewarmed AOT mode must initialize")) {
      return false;
    }
    auto module = aot.prepare(kSecondUid, *parsed.kernel, serialized.text);
    const auto statistics = aot.statistics();
    if (!expect(module.ok(), "administrator AOT must be readable across peer UIDs") ||
        !expect(statistics.compiler_requests == 0U && statistics.compiler_worker_launches == 0U &&
                    statistics.cache_hits == 1U,
                "AOT hit must load without a compiler worker") ||
        !expect(launch_add(*module.module), "AOT Add must execute")) {
      return false;
    }
  }

  configuration = execution_configuration("cold-jit", cache_root);
  configuration.configuration->compiler_worker.executable = METAFLUX_DAEMON_EXECUTABLE;
  {
    metaflux::service::CpuExecutionEngine cold_with_aot(std::move(*configuration.configuration));
    std::string diagnostic;
    if (!expect(cold_with_aot.initialize(diagnostic),
                "cold JIT must initialize with a read-only AOT tier")) {
      return false;
    }
    auto module = cold_with_aot.prepare(kSecondUid, *parsed.kernel, serialized.text);
    const auto statistics = cold_with_aot.statistics();
    if (statistics.compiler_requests != 0U || statistics.cache_hits != 1U ||
        statistics.cache_misses != 0U) {
      std::cerr << "AOT precedence statistics: compiler_requests=" << statistics.compiler_requests
                << " cache_hits=" << statistics.cache_hits
                << " cache_misses=" << statistics.cache_misses << '\n';
    }
    if (!expect(module.ok(), "cold JIT must honor administrator AOT precedence") ||
        !expect(statistics.compiler_requests == 0U && statistics.cache_hits == 1U &&
                    statistics.cache_misses == 0U,
                "an AOT-precedence hit must bypass the cold compiler request") ||
        !expect(launch_add(*module.module), "AOT-precedence Add must execute")) {
      return false;
    }
  }
  std::error_code cleanup_error;
  std::filesystem::permissions(aot_elf.parent_path(), std::filesystem::perms::owner_write,
                               std::filesystem::perm_options::add, cleanup_error);
  return true;
}

} // namespace

int main() { return test_configuration_parser() && test_execution_modes() ? 0 : 1; }
