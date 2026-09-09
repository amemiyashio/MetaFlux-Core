#include "execution.hpp"

#include "metaflux/compiler/kernel_ir.hpp"
#include "metaflux/compiler/ptx_frontend.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

namespace {

class TemporaryDirectory final {
public:
  TemporaryDirectory() {
    std::array<char, 64> pattern{};
    const std::string prefix = "/tmp/metafluxd-compiler-worker-XXXXXX";
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
    std::cerr << "metafluxd compiler worker test: " << message << '\n';
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

[[nodiscard]] bool write_script(const std::filesystem::path& path, std::string_view body) {
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  output << "#!/bin/sh\n" << body << '\n';
  output.close();
  return output.good() && chmod(path.c_str(), 0700) == 0;
}

[[nodiscard]] bool child_is_reaped(std::int64_t process_id) {
  if (process_id <= 0) {
    return false;
  }
  int status = 0;
  errno = 0;
  const auto result = waitpid(static_cast<pid_t>(process_id), &status, WNOHANG);
  return result == -1 && errno == ECHILD;
}

struct KernelFixture final {
  metaflux::compiler::Kernel kernel;
  std::string canonical;
};

[[nodiscard]] std::optional<KernelFixture> load_kernel(const std::filesystem::path& path) {
  const auto source = read_source(path);
  if (!source.has_value()) {
    return std::nullopt;
  }
  auto parsed = metaflux::compiler::ptx::parse(*source);
  if (!parsed.ok()) {
    return std::nullopt;
  }
  auto serialized = metaflux::compiler::serialize_kernel(*parsed.kernel);
  if (!serialized.ok()) {
    return std::nullopt;
  }
  return KernelFixture{.kernel = std::move(*parsed.kernel),
                       .canonical = std::move(serialized.text)};
}

[[nodiscard]] metaflux::service::CpuExecutionConfigurationResult
configuration(std::string_view mode, const std::filesystem::path& cache_root,
              const std::filesystem::path& worker,
              std::chrono::milliseconds deadline = std::chrono::seconds(120)) {
  const char* topology_root = std::getenv("METAFLUX_CPU_TOPOLOGY_ROOT");
  auto result = metaflux::service::parse_cpu_execution_configuration(
      mode, cache_root.string(), std::nullopt,
      topology_root == nullptr ? std::nullopt : std::optional<std::string_view>(topology_root));
  if (result.ok()) {
    // Worker/cache behavior must not depend on the capacity of the test host's /tmp.
    result.configuration->cache.filesystem_space = [] {
      return std::optional<metaflux::compiler::CacheFilesystemSpace>{
          metaflux::compiler::CacheFilesystemSpace{
              .total_bytes = 64ULL * 1024ULL * 1024ULL * 1024ULL,
              .available_bytes = 64ULL * 1024ULL * 1024ULL * 1024ULL,
          }};
    };
    result.configuration->compiler_worker.executable = worker;
    result.configuration->compiler_worker.deadline = deadline;
  }
  return result;
}

[[nodiscard]] bool test_real_worker_and_warm_bypass(const TemporaryDirectory& temporary,
                                                    const KernelFixture& fixture) {
  constexpr std::uint32_t kUid = 4101U;
  const auto cache_root = temporary.path() / "real-cache";
  auto cold_configuration =
      configuration("cold-jit", cache_root, std::filesystem::path(METAFLUX_DAEMON_EXECUTABLE));
  if (!expect(cold_configuration.ok(), "cold worker configuration must parse")) {
    return false;
  }
  {
    metaflux::service::CpuExecutionEngine engine(std::move(*cold_configuration.configuration));
    std::string diagnostic;
    if (!expect(engine.initialize(diagnostic), "cold worker engine must initialize")) {
      std::cerr << diagnostic << '\n';
      return false;
    }
    auto prepared = engine.prepare(kUid, fixture.kernel, fixture.canonical);
    const auto statistics = engine.statistics();
    if (!expect(prepared.ok(), "cold miss must compile through the worker") ||
        !expect(statistics.compiler_requests == 1U && statistics.compiler_worker_launches == 1U &&
                    statistics.compiler_worker_failures == 0U,
                "cold miss must launch exactly one successful worker") ||
        !expect(statistics.last_compiler_worker_pid > 0 &&
                    statistics.last_compiler_worker_pid != static_cast<std::int64_t>(getpid()),
                "compiler work must execute in a distinct process") ||
        !expect(child_is_reaped(statistics.last_compiler_worker_pid),
                "successful compiler worker must be reaped")) {
      std::cerr << prepared.diagnostic << '\n';
      return false;
    }
  }

  auto warm_configuration =
      configuration("warm-jit", cache_root, temporary.path() / "missing-worker");
  metaflux::service::CpuExecutionEngine warm(std::move(*warm_configuration.configuration));
  std::string diagnostic;
  if (!expect(warm.initialize(diagnostic), "warm worker-bypass engine must initialize")) {
    return false;
  }
  auto prepared = warm.prepare(kUid, fixture.kernel, fixture.canonical);
  const auto statistics = warm.statistics();
  return expect(prepared.ok(), "warm hit must load with no worker executable present") &&
         expect(statistics.compiler_requests == 0U && statistics.compiler_worker_launches == 0U &&
                    statistics.cache_hits == 1U,
                "warm hit must not contact or launch a compiler worker");
}

[[nodiscard]] bool test_worker_fault(const TemporaryDirectory& temporary,
                                     const KernelFixture& fixture, std::string_view name,
                                     std::string_view script_body, std::string_view expected,
                                     std::chrono::milliseconds deadline) {
  const auto worker = temporary.path() / (std::string(name) + "-worker");
  if (!expect(write_script(worker, script_body), "fault worker script must be executable")) {
    return false;
  }
  const auto cache_root = temporary.path() / (std::string(name) + "-cache");
  auto worker_configuration = configuration("cold-jit", cache_root, worker, deadline);
  metaflux::service::CpuExecutionEngine engine(std::move(*worker_configuration.configuration));
  std::string diagnostic;
  if (!expect(engine.initialize(diagnostic), "fault worker engine must initialize")) {
    return false;
  }
  const auto prepared = engine.prepare(4202U, fixture.kernel, fixture.canonical);
  const auto statistics = engine.statistics();
  return expect(!prepared.ok(), "worker fault must fail module preparation") &&
         expect(prepared.diagnostic.find(expected) != std::string::npos,
                "worker fault must return its stable diagnostic") &&
         expect(statistics.compiler_requests == 1U && statistics.compiler_worker_launches == 1U &&
                    statistics.compiler_worker_failures == 1U,
                "worker fault statistics must count one bounded attempt") &&
         expect(child_is_reaped(statistics.last_compiler_worker_pid),
                "failed compiler worker must be reaped");
}

[[nodiscard]] bool test_worker_cancellation(const TemporaryDirectory& temporary,
                                            const KernelFixture& fixture) {
  using namespace std::chrono_literals;
  const auto worker = temporary.path() / "cancel-worker";
  const auto marker = temporary.path() / "cancel-worker-started";
  const std::string body = "printf ready > " + marker.string() + "\nsleep 30";
  if (!expect(write_script(worker, body), "cancellation worker script must be executable")) {
    return false;
  }
  auto worker_configuration =
      configuration("cold-jit", temporary.path() / "cancel-cache", worker, 30s);
  if (!expect(worker_configuration.ok(), "cancellation worker configuration must parse")) {
    return false;
  }
  metaflux::service::CpuExecutionEngine engine(std::move(*worker_configuration.configuration));
  std::string diagnostic;
  if (!expect(engine.initialize(diagnostic), "cancellation worker engine must initialize")) {
    return false;
  }

  std::stop_source pre_cancelled;
  static_cast<void>(pre_cancelled.request_stop());
  const auto pre_cancelled_result =
      engine.prepare(4303U, fixture.kernel, fixture.canonical, pre_cancelled.get_token());
  if (!expect(!pre_cancelled_result.ok() &&
                  pre_cancelled_result.diagnostic == "module preparation cancelled",
              "pre-cancelled preparation must fail before worker launch") ||
      !expect(engine.statistics().compiler_worker_launches == 0U,
              "pre-cancelled preparation must not spawn a worker")) {
    return false;
  }

  std::stop_source cancellation;
  metaflux::service::PrepareModuleResult prepared;
  std::thread preparing([&] {
    prepared = engine.prepare(4303U, fixture.kernel, fixture.canonical, cancellation.get_token());
  });
  const auto marker_deadline = std::chrono::steady_clock::now() + 5s;
  while (!std::filesystem::exists(marker) && std::chrono::steady_clock::now() < marker_deadline) {
    std::this_thread::sleep_for(1ms);
  }
  if (!std::filesystem::exists(marker)) {
    static_cast<void>(cancellation.request_stop());
    preparing.join();
    return expect(false, "cancellation worker must publish its started marker");
  }

  const auto requested_at = std::chrono::steady_clock::now();
  static_cast<void>(cancellation.request_stop());
  preparing.join();
  const auto cancellation_latency = std::chrono::steady_clock::now() - requested_at;
  const auto statistics = engine.statistics();
  return expect(!prepared.ok() &&
                    prepared.diagnostic.find("compiler worker cancelled") != std::string::npos,
                "worker cancellation must propagate a stable preparation diagnostic") &&
         expect(cancellation_latency < 5s,
                "worker cancellation must not wait for the worker's 30-second sleep") &&
         expect(statistics.compiler_requests == 1U && statistics.compiler_worker_launches == 1U &&
                    statistics.compiler_worker_failures == 1U,
                "worker cancellation statistics must count one launched failed request") &&
         expect(child_is_reaped(statistics.last_compiler_worker_pid),
                "cancelled compiler worker must be reaped before prepare returns");
}

} // namespace

int main() {
  TemporaryDirectory temporary;
  const auto fixture = load_kernel(METAFLUX_DAEMON_ADD_PTX);
  const auto cast_fixture = load_kernel(METAFLUX_DAEMON_CAST_PTX);
  if (!expect(!temporary.path().empty(), "temporary directory must be available") ||
      !expect(fixture.has_value(), "canonical Add Kernel IR must load") ||
      !expect(cast_fixture.has_value(), "canonical signed-conversion Kernel IR must load")) {
    return 1;
  }
  const bool real = test_real_worker_and_warm_bypass(temporary, *fixture) &&
                    test_real_worker_and_warm_bypass(temporary, *cast_fixture);
  const bool crash = test_worker_fault(temporary, *fixture, "crash", "kill -SEGV $$",
                                       "terminated by signal", std::chrono::seconds(5));
  const bool timeout = test_worker_fault(temporary, *fixture, "timeout", "sleep 30",
                                         "deadline exceeded", std::chrono::milliseconds(100));
  const bool truncated = test_worker_fault(temporary, *fixture, "truncated", "printf x",
                                           "truncated or malformed", std::chrono::seconds(5));
  const bool cancelled = test_worker_cancellation(temporary, *fixture);
  return real && crash && timeout && truncated && cancelled ? 0 : 1;
}
