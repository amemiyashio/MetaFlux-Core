#include "compiler_worker.hpp"

#include "compiler_worker_protocol.hpp"

#include <array>
#include <cerrno>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <optional>
#include <signal.h>
#include <span>
#include <string>
#include <sys/prctl.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <unistd.h>
#include <utility>
#include <vector>

namespace metaflux::service {
namespace {

using backend::cpu::compiler::CompileDiagnostic;
using backend::cpu::compiler::CompileError;
using backend::cpu::compiler::CompileResult;

#if defined(__has_feature)
#if __has_feature(address_sanitizer)
#define METAFLUX_COMPILER_WORKER_ASAN 1
#endif
#endif
#if defined(__SANITIZE_ADDRESS__)
#define METAFLUX_COMPILER_WORKER_ASAN 1
#endif

[[nodiscard]] CompileResult failure(CompileError error, std::string message) {
  return CompileResult{
      .artifact = std::nullopt,
      .diagnostic =
          CompileDiagnostic{.error = error, .location = {}, .message = std::move(message)},
  };
}

[[nodiscard]] bool tighten_limit(int resource, rlim_t maximum) {
  rlimit inherited{};
  if (getrlimit(resource, &inherited) != 0) {
    return false;
  }
  const rlimit limit{
      .rlim_cur = inherited.rlim_cur < maximum ? inherited.rlim_cur : maximum,
      .rlim_max = inherited.rlim_max < maximum ? inherited.rlim_max : maximum,
  };
  return setrlimit(resource, &limit) == 0;
}

[[nodiscard]] std::optional<std::string>
apply_worker_limits(std::int64_t expected_parent_process_id) {
  constexpr rlim_t kFileBytes = static_cast<rlim_t>(512ULL * 1024ULL * 1024ULL);
  constexpr rlim_t kCpuSeconds = 60;
  constexpr rlim_t kOpenFiles = 64;

  if (expected_parent_process_id <= 1 ||
      static_cast<std::int64_t>(getppid()) != expected_parent_process_id ||
      prctl(PR_SET_PDEATHSIG, SIGKILL) != 0 ||
      static_cast<std::int64_t>(getppid()) != expected_parent_process_id) {
    return "compiler worker could not bind its lifetime to the parent";
  }
  static_cast<void>(umask(0077));
  if (!tighten_limit(RLIMIT_CORE, 0) || !tighten_limit(RLIMIT_FSIZE, kFileBytes) ||
      !tighten_limit(RLIMIT_CPU, kCpuSeconds) || !tighten_limit(RLIMIT_NOFILE, kOpenFiles)) {
    return "compiler worker could not install mandatory resource limits";
  }
#if !defined(METAFLUX_COMPILER_WORKER_ASAN)
  // ASan reserves a very large virtual shadow range before main; the other
  // mandatory limits and the parent deadline remain active in sanitizer runs.
  constexpr rlim_t kFourGiB = static_cast<rlim_t>(4ULL * 1024ULL * 1024ULL * 1024ULL);
  if (!tighten_limit(RLIMIT_AS, kFourGiB)) {
    return "compiler worker could not install the address-space limit";
  }
#endif
  return std::nullopt;
}

[[nodiscard]] std::optional<std::vector<std::byte>> read_request() {
  std::vector<std::byte> request;
  std::array<std::byte, 64U * 1024U> buffer{};
  while (true) {
    const auto count = read(STDIN_FILENO, buffer.data(), buffer.size());
    if (count > 0) {
      const auto size = static_cast<std::size_t>(count);
      if (size > compiler_worker_protocol::kMaximumRequestBytes - request.size()) {
        return std::nullopt;
      }
      request.insert(request.end(), buffer.begin(),
                     buffer.begin() + static_cast<std::ptrdiff_t>(size));
      continue;
    }
    if (count == 0) {
      return request;
    }
    if (errno != EINTR) {
      return std::nullopt;
    }
  }
}

[[nodiscard]] bool write_response(std::span<const std::byte> response) {
  std::size_t offset = 0;
  while (offset < response.size()) {
    const auto count = write(STDOUT_FILENO, response.data() + offset, response.size() - offset);
    if (count > 0) {
      offset += static_cast<std::size_t>(count);
      continue;
    }
    if (count < 0 && errno == EINTR) {
      continue;
    }
    return false;
  }
  return true;
}

[[nodiscard]] int emit_result(CompileResult result) {
  auto response = compiler_worker_protocol::encode_response(result, getpid());
  if (!response.has_value()) {
    response = compiler_worker_protocol::encode_response(
        failure(CompileError::ResourceLimit,
                "compiler worker result exceeds the bounded response protocol"),
        getpid());
  }
  return response.has_value() && write_response(*response) ? 0 : 74;
}

} // namespace

int run_compiler_worker_process(std::int64_t expected_parent_process_id) noexcept {
  try {
    if (const auto limit_error = apply_worker_limits(expected_parent_process_id);
        limit_error.has_value()) {
      return emit_result(failure(CompileError::ResourceLimit, *limit_error));
    }
    const auto request = read_request();
    if (!request.has_value()) {
      return emit_result(
          failure(CompileError::ResourceLimit, "compiler worker request exceeds its IPC bound"));
    }
    auto kernel = compiler_worker_protocol::decode_request(*request);
    if (!kernel.has_value()) {
      return emit_result(failure(CompileError::InvalidKernel,
                                 "compiler worker request is truncated or malformed"));
    }
    backend::cpu::compiler::CompileOptions options =
        backend::cpu::compiler::host_compile_options();
    options.deadline = std::chrono::steady_clock::now() + std::chrono::seconds(110);
    return emit_result(backend::cpu::compiler::compile_kernel(*kernel, options));
  } catch (const std::bad_alloc&) {
    return emit_result(
        failure(CompileError::ResourceLimit, "compiler worker allocation limit was reached"));
  } catch (const std::exception& error) {
    return emit_result(
        failure(CompileError::Io, "compiler worker exception: " + std::string(error.what())));
  } catch (...) {
    return emit_result(failure(CompileError::Io, "compiler worker raised an unknown exception"));
  }
}

} // namespace metaflux::service
