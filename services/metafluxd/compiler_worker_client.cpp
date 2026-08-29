#include "compiler_worker.hpp"

#include "compiler_worker_protocol.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <limits>
#include <optional>
#include <poll.h>
#include <signal.h>
#include <spawn.h>
#include <string>
#include <string_view>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <system_error>
#include <unistd.h>
#include <utility>
#include <vector>

extern char** environ;

namespace metaflux::service {
namespace {

using Clock = std::chrono::steady_clock;
using backend::cpu::compiler::CompileDiagnostic;
using backend::cpu::compiler::CompileError;
using backend::cpu::compiler::CompileResult;

class UniqueDescriptor final {
public:
  UniqueDescriptor() = default;
  explicit UniqueDescriptor(int descriptor) : descriptor_(descriptor) {}
  ~UniqueDescriptor() { reset(); }
  UniqueDescriptor(const UniqueDescriptor&) = delete;
  UniqueDescriptor& operator=(const UniqueDescriptor&) = delete;
  UniqueDescriptor(UniqueDescriptor&& other) noexcept
      : descriptor_(std::exchange(other.descriptor_, -1)) {}
  UniqueDescriptor& operator=(UniqueDescriptor&& other) noexcept {
    if (this != &other) {
      reset();
      descriptor_ = std::exchange(other.descriptor_, -1);
    }
    return *this;
  }

  [[nodiscard]] int get() const noexcept { return descriptor_; }
  [[nodiscard]] explicit operator bool() const noexcept { return descriptor_ >= 0; }
  [[nodiscard]] bool move_above_standard_streams() {
    if (descriptor_ >= STDERR_FILENO + 1) {
      return true;
    }
    const int replacement = fcntl(descriptor_, F_DUPFD_CLOEXEC, STDERR_FILENO + 1);
    if (replacement < 0) {
      return false;
    }
    reset();
    descriptor_ = replacement;
    return true;
  }
  void reset() noexcept {
    if (descriptor_ >= 0) {
      static_cast<void>(close(descriptor_));
      descriptor_ = -1;
    }
  }

private:
  int descriptor_ = -1;
};

class ChildProcess final {
public:
  explicit ChildProcess(pid_t process_id) : process_id_(process_id) {}
  ~ChildProcess() { terminate_and_reap(); }
  ChildProcess(const ChildProcess&) = delete;
  ChildProcess& operator=(const ChildProcess&) = delete;
  ChildProcess(ChildProcess&& other) noexcept
      : process_id_(std::exchange(other.process_id_, -1)), status_(other.status_),
        reaped_(std::exchange(other.reaped_, true)) {}
  ChildProcess& operator=(ChildProcess&&) = delete;

  [[nodiscard]] pid_t process_id() const noexcept { return process_id_; }
  [[nodiscard]] bool reaped() const noexcept { return reaped_; }
  [[nodiscard]] int status() const noexcept { return status_; }

  [[nodiscard]] bool try_reap() noexcept {
    if (reaped_) {
      return true;
    }
    while (true) {
      const pid_t result = waitpid(process_id_, &status_, WNOHANG);
      if (result == process_id_) {
        reaped_ = true;
        return true;
      }
      if (result == 0) {
        return false;
      }
      if (errno == EINTR) {
        continue;
      }
      if (errno == ECHILD) {
        reaped_ = true;
      }
      return reaped_;
    }
  }

  void terminate_and_reap() noexcept {
    if (reaped_ || process_id_ <= 0) {
      return;
    }
    static_cast<void>(kill(-process_id_, SIGKILL));
    static_cast<void>(kill(process_id_, SIGKILL));
    while (waitpid(process_id_, &status_, 0) < 0) {
      if (errno == EINTR) {
        continue;
      }
      break;
    }
    reaped_ = true;
  }

private:
  pid_t process_id_;
  int status_ = 0;
  bool reaped_ = false;
};

[[nodiscard]] CompileResult failure(CompileError error, std::string message) {
  return CompileResult{
      .artifact = std::nullopt,
      .diagnostic =
          CompileDiagnostic{.error = error, .location = {}, .message = std::move(message)},
  };
}

[[nodiscard]] CompilerWorkerInvocation invocation_failure(CompileError error, std::string message,
                                                          std::int64_t process_id = -1,
                                                          bool launched = false) {
  return {.compilation = failure(error, std::move(message)),
          .process_id = process_id,
          .launched = launched};
}

[[nodiscard]] bool set_nonblocking(int descriptor) {
  const int flags = fcntl(descriptor, F_GETFL, 0);
  return flags >= 0 && fcntl(descriptor, F_SETFL, flags | O_NONBLOCK) == 0;
}

enum class WaitResult : std::uint8_t {
  Ready,
  Timeout,
  Cancelled,
  Error,
};

constexpr int kCancellationPollSliceMilliseconds = 10;

[[nodiscard]] int remaining_milliseconds(Clock::time_point deadline) {
  const auto now = Clock::now();
  if (now >= deadline) {
    return 0;
  }
  const auto remaining = deadline - now;
  const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
      remaining + std::chrono::microseconds(999));
  return static_cast<int>(std::min<std::int64_t>(milliseconds.count(), INT_MAX));
}

[[nodiscard]] WaitResult wait_for_descriptor(int descriptor, short events,
                                             Clock::time_point deadline,
                                             std::stop_token cancellation) {
  pollfd poll_descriptor{.fd = descriptor, .events = events, .revents = 0};
  while (true) {
    if (cancellation.stop_requested()) {
      return WaitResult::Cancelled;
    }
    const int timeout = remaining_milliseconds(deadline);
    if (timeout == 0) {
      return WaitResult::Timeout;
    }
    const int result =
        poll(&poll_descriptor, 1, std::min(timeout, kCancellationPollSliceMilliseconds));
    if (result > 0) {
      return cancellation.stop_requested() ? WaitResult::Cancelled : WaitResult::Ready;
    }
    if (result == 0) {
      continue;
    }
    if (errno != EINTR) {
      return WaitResult::Error;
    }
  }
}

enum class SendResult : std::uint8_t {
  Complete,
  Timeout,
  Cancelled,
  Error,
};

[[nodiscard]] SendResult send_request(int descriptor, std::span<const std::byte> request,
                                      Clock::time_point deadline, std::stop_token cancellation) {
  std::size_t offset = 0;
  while (offset < request.size()) {
    if (cancellation.stop_requested()) {
      return SendResult::Cancelled;
    }
    const auto sent =
        send(descriptor, request.data() + offset, request.size() - offset, MSG_NOSIGNAL);
    if (sent > 0) {
      offset += static_cast<std::size_t>(sent);
      continue;
    }
    if (sent < 0 && errno == EINTR) {
      continue;
    }
    if (sent < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
      const auto ready = wait_for_descriptor(descriptor, POLLOUT, deadline, cancellation);
      if (ready == WaitResult::Ready) {
        continue;
      }
      if (ready == WaitResult::Timeout) {
        return SendResult::Timeout;
      }
      return ready == WaitResult::Cancelled ? SendResult::Cancelled : SendResult::Error;
    }
    return SendResult::Error;
  }
  if (cancellation.stop_requested()) {
    return SendResult::Cancelled;
  }
  return shutdown(descriptor, SHUT_WR) == 0 ? SendResult::Complete : SendResult::Error;
}

enum class ReceiveResult : std::uint8_t {
  Complete,
  Timeout,
  Cancelled,
  TooLarge,
  Error,
};

[[nodiscard]] ReceiveResult receive_response(int descriptor, Clock::time_point deadline,
                                             std::vector<std::byte>& response,
                                             std::stop_token cancellation) {
  std::array<std::byte, 64U * 1024U> buffer{};
  while (true) {
    if (cancellation.stop_requested()) {
      return ReceiveResult::Cancelled;
    }
    const auto received = recv(descriptor, buffer.data(), buffer.size(), 0);
    if (received > 0) {
      const auto count = static_cast<std::size_t>(received);
      if (count > compiler_worker_protocol::kMaximumResponseBytes - response.size()) {
        return ReceiveResult::TooLarge;
      }
      response.insert(response.end(), buffer.begin(),
                      buffer.begin() + static_cast<std::ptrdiff_t>(count));
      continue;
    }
    if (received == 0) {
      return ReceiveResult::Complete;
    }
    if (errno == EINTR) {
      continue;
    }
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      const auto ready = wait_for_descriptor(descriptor, POLLIN | POLLHUP, deadline, cancellation);
      if (ready == WaitResult::Ready) {
        continue;
      }
      if (ready == WaitResult::Timeout) {
        return ReceiveResult::Timeout;
      }
      return ready == WaitResult::Cancelled ? ReceiveResult::Cancelled : ReceiveResult::Error;
    }
    return ReceiveResult::Error;
  }
}

enum class ExitResult : std::uint8_t {
  Exited,
  Timeout,
  Cancelled,
};

[[nodiscard]] ExitResult wait_for_exit(ChildProcess& child, Clock::time_point deadline,
                                       std::stop_token cancellation) {
  while (true) {
    if (child.try_reap()) {
      return ExitResult::Exited;
    }
    if (cancellation.stop_requested()) {
      return ExitResult::Cancelled;
    }
    const int remaining = remaining_milliseconds(deadline);
    if (remaining == 0) {
      return ExitResult::Timeout;
    }
    const int delay = std::min(remaining, kCancellationPollSliceMilliseconds);
    while (poll(nullptr, 0, delay) < 0 && errno == EINTR) {
      if (cancellation.stop_requested()) {
        return ExitResult::Cancelled;
      }
    }
  }
}

struct SpawnedWorker final {
  std::optional<ChildProcess> child;
  UniqueDescriptor request;
  UniqueDescriptor response;
  int error = 0;
};

[[nodiscard]] SpawnedWorker spawn_failure(int error) {
  SpawnedWorker result;
  result.error = error;
  return result;
}

[[nodiscard]] SpawnedWorker spawn_worker(const std::filesystem::path& executable) {
  int request_pair[2]{-1, -1};
  int response_pair[2]{-1, -1};
  if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, request_pair) != 0) {
    return spawn_failure(errno);
  }
  UniqueDescriptor request_parent(request_pair[0]);
  UniqueDescriptor request_child(request_pair[1]);
  if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, response_pair) != 0) {
    return spawn_failure(errno);
  }
  UniqueDescriptor response_parent(response_pair[0]);
  UniqueDescriptor response_child(response_pair[1]);
  if (!request_parent.move_above_standard_streams() ||
      !request_child.move_above_standard_streams() ||
      !response_parent.move_above_standard_streams() ||
      !response_child.move_above_standard_streams()) {
    return spawn_failure(errno == 0 ? EIO : errno);
  }

  posix_spawn_file_actions_t actions{};
  posix_spawnattr_t attributes{};
  int error = posix_spawn_file_actions_init(&actions);
  if (error != 0) {
    return spawn_failure(error);
  }
  error = posix_spawnattr_init(&attributes);
  if (error != 0) {
    static_cast<void>(posix_spawn_file_actions_destroy(&actions));
    return spawn_failure(error);
  }
  const auto cleanup = [&] {
    static_cast<void>(posix_spawnattr_destroy(&attributes));
    static_cast<void>(posix_spawn_file_actions_destroy(&actions));
  };

  const std::array action_errors{
      posix_spawn_file_actions_adddup2(&actions, request_child.get(), STDIN_FILENO),
      posix_spawn_file_actions_adddup2(&actions, response_child.get(), STDOUT_FILENO),
      posix_spawn_file_actions_addclose(&actions, request_parent.get()),
      posix_spawn_file_actions_addclose(&actions, response_parent.get()),
      posix_spawn_file_actions_addclose(&actions, request_child.get()),
      posix_spawn_file_actions_addclose(&actions, response_child.get()),
  };
  const auto failed_action = std::find_if(action_errors.begin(), action_errors.end(),
                                          [](int value) { return value != 0; });
  if (failed_action != action_errors.end()) {
    error = *failed_action;
    cleanup();
    return spawn_failure(error);
  }
  error = posix_spawnattr_setpgroup(&attributes, 0);
  if (error == 0) {
    error = posix_spawnattr_setflags(&attributes, POSIX_SPAWN_SETPGROUP);
  }
  if (error != 0) {
    cleanup();
    return spawn_failure(error);
  }

  const std::string executable_text = executable.string();
  const std::string parent_process_id = std::to_string(getpid());
  std::array<char*, 4> arguments{
      const_cast<char*>(executable_text.c_str()),
      const_cast<char*>("--compiler-worker-v1"),
      const_cast<char*>(parent_process_id.c_str()),
      nullptr,
  };
  pid_t process_id = -1;
  error = posix_spawn(&process_id, executable_text.c_str(), &actions, &attributes, arguments.data(),
                      environ);
  cleanup();
  if (error != 0) {
    return spawn_failure(error);
  }
  request_child.reset();
  response_child.reset();
  if (!set_nonblocking(request_parent.get()) || !set_nonblocking(response_parent.get())) {
    ChildProcess child(process_id);
    child.terminate_and_reap();
    return spawn_failure(errno == 0 ? EIO : errno);
  }
  return {.child = std::optional<ChildProcess>(std::in_place, process_id),
          .request = std::move(request_parent),
          .response = std::move(response_parent),
          .error = 0};
}

} // namespace

std::filesystem::path current_executable_path() {
  std::vector<char> buffer(4096U);
  while (buffer.size() <= 1024U * 1024U) {
    const auto length = readlink("/proc/self/exe", buffer.data(), buffer.size());
    if (length < 0) {
      return {};
    }
    if (static_cast<std::size_t>(length) < buffer.size()) {
      return std::filesystem::path(std::string(buffer.data(), static_cast<std::size_t>(length)));
    }
    buffer.resize(buffer.size() * 2U);
  }
  return {};
}

CompilerWorkerInvocation compile_kernel_in_worker(const CompilerWorkerPolicy& policy,
                                                  const compiler::Kernel& kernel) {
  return compile_kernel_in_worker(policy, kernel, std::stop_token{});
}

CompilerWorkerInvocation compile_kernel_in_worker(const CompilerWorkerPolicy& policy,
                                                  const compiler::Kernel& kernel,
                                                  std::stop_token cancellation) {
  try {
    if (cancellation.stop_requested()) {
      return invocation_failure(CompileError::Cancelled, "compiler worker cancelled");
    }
    if (policy.executable.empty() || !policy.executable.is_absolute()) {
      return invocation_failure(CompileError::Io,
                                "compiler worker executable must be an absolute path");
    }
    if (policy.deadline <= std::chrono::milliseconds::zero()) {
      return invocation_failure(CompileError::Cancelled,
                                "compiler worker deadline must be positive");
    }
    const auto request = compiler_worker_protocol::encode_request(kernel);
    if (!request.has_value()) {
      return invocation_failure(CompileError::ResourceLimit,
                                "compiler worker request exceeds the IPC size limit");
    }
    if (cancellation.stop_requested()) {
      return invocation_failure(CompileError::Cancelled, "compiler worker cancelled");
    }

    auto worker = spawn_worker(policy.executable);
    if (!worker.child.has_value()) {
      return invocation_failure(CompileError::Io, "compiler worker spawn failed: " +
                                                      std::string(std::strerror(worker.error)));
    }
    const auto process_id = worker.child->process_id();
    const auto result_process_id = static_cast<std::int64_t>(process_id);
    const auto deadline = Clock::now() + policy.deadline;
    const auto sent = send_request(worker.request.get(), *request, deadline, cancellation);
    worker.request.reset();
    if (sent == SendResult::Cancelled) {
      worker.response.reset();
      worker.child->terminate_and_reap();
      return invocation_failure(CompileError::Cancelled, "compiler worker cancelled",
                                result_process_id, true);
    }
    if (sent == SendResult::Timeout) {
      worker.child->terminate_and_reap();
      return invocation_failure(CompileError::Cancelled, "compiler worker deadline exceeded",
                                result_process_id, true);
    }
    if (sent == SendResult::Error) {
      worker.response.reset();
      worker.child->terminate_and_reap();
      return invocation_failure(CompileError::Io, "compiler worker request write failed",
                                result_process_id, true);
    }

    std::vector<std::byte> response;
    const auto received = receive_response(worker.response.get(), deadline, response, cancellation);
    worker.response.reset();
    if (received == ReceiveResult::Cancelled) {
      worker.child->terminate_and_reap();
      return invocation_failure(CompileError::Cancelled, "compiler worker cancelled",
                                result_process_id, true);
    }
    if (received == ReceiveResult::Timeout) {
      worker.child->terminate_and_reap();
      return invocation_failure(CompileError::Cancelled, "compiler worker deadline exceeded",
                                result_process_id, true);
    }
    if (received == ReceiveResult::TooLarge) {
      worker.child->terminate_and_reap();
      return invocation_failure(CompileError::ResourceLimit,
                                "compiler worker response exceeds the IPC size limit",
                                result_process_id, true);
    }
    if (received == ReceiveResult::Error) {
      worker.child->terminate_and_reap();
      return invocation_failure(CompileError::Io, "compiler worker response read failed",
                                result_process_id, true);
    }
    const auto exited = wait_for_exit(*worker.child, deadline, cancellation);
    if (exited == ExitResult::Cancelled) {
      worker.child->terminate_and_reap();
      return invocation_failure(CompileError::Cancelled, "compiler worker cancelled",
                                result_process_id, true);
    }
    if (exited == ExitResult::Timeout) {
      worker.child->terminate_and_reap();
      return invocation_failure(CompileError::Cancelled, "compiler worker deadline exceeded",
                                result_process_id, true);
    }

    const int status = worker.child->status();
    if (WIFSIGNALED(status)) {
      return invocation_failure(CompileError::Io,
                                "compiler worker terminated by signal " +
                                    std::to_string(WTERMSIG(status)),
                                result_process_id, true);
    }
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
      const auto exit_status = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
      return invocation_failure(CompileError::Io,
                                "compiler worker exited with status " + std::to_string(exit_status),
                                result_process_id, true);
    }

    std::string protocol_diagnostic;
    auto decoded = compiler_worker_protocol::decode_response(response, protocol_diagnostic);
    if (!decoded.has_value()) {
      return invocation_failure(CompileError::Io, std::move(protocol_diagnostic), result_process_id,
                                true);
    }
    if (decoded->process_id != result_process_id) {
      return invocation_failure(CompileError::Io, "compiler worker response PID mismatch",
                                result_process_id, true);
    }
    return {.compilation = std::move(decoded->compilation),
            .process_id = result_process_id,
            .launched = true};
  } catch (const std::bad_alloc&) {
    return invocation_failure(CompileError::ResourceLimit,
                              "compiler worker parent allocation failed");
  } catch (const std::filesystem::filesystem_error& error) {
    return invocation_failure(CompileError::Io, error.what());
  } catch (const std::system_error& error) {
    return invocation_failure(CompileError::Io, error.what());
  }
}

} // namespace metaflux::service
