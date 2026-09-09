#include "server.hpp"

#include "execution.hpp"
#include "vulkan_execution.hpp"

#include "metaflux/backend/cpu.h"
#include "metaflux/client/fastpath.h"
#include "metaflux/compiler/kernel_ir.hpp"
#include "metaflux/compiler/ptx_frontend.hpp"
#include "metaflux/runtime/core.hpp"
#include "metaflux/runtime/lifecycle_dispatch.hpp"
#include "metaflux/transport/cdev_worker.hpp"
#include "metaflux/uapi/transport.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <charconv>
#include <chrono>
#include <csignal>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <fcntl.h>
#include <iostream>
#include <limits>
#include <linux/magic.h>
#include <linux/memfd.h>
#include <memory>
#include <mutex>
#include <new>
#include <optional>
#include <poll.h>
#include <span>
#include <stop_token>
#include <string>
#include <string_view>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/un.h>
#include <sys/vfs.h>
#include <thread>
#include <unordered_map>
#include <unistd.h>
#include <utility>
#include <vector>

#ifndef METAFLUX_DAEMON_CDEV_BACKEND
#define METAFLUX_DAEMON_CDEV_BACKEND 0
#endif

namespace metaflux::service {
namespace {

constexpr std::uint32_t kRingCapacity = 256U;
constexpr std::uint64_t kIdentityRecordId = 1U;
constexpr std::uint64_t kDeviceGeneration = 1U;
constexpr std::uint64_t kObjectGeneration = 1U;
constexpr std::uint64_t kFirstObjectId = 1024U;
constexpr std::uint64_t kMemoryCapacity = 256U * 1024U * 1024U;
constexpr std::uint64_t kMaximumObjectSize = 64U * 1024U * 1024U;
constexpr std::uint64_t kMaximumMappedBytes = 256U * 1024U * 1024U;
constexpr std::uint64_t kUtilizationWindowNs = 100U * 1000U * 1000U;
constexpr std::size_t kMaximumObjects = 4096U;
constexpr std::size_t kMaximumSessions = 64U;
constexpr std::size_t kMaximumUidSessions = 16U;
constexpr std::uint64_t kMaximumGlobalMappedBytes = 1024U * 1024U * 1024U;
constexpr std::size_t kMaximumGlobalMappedObjects = 16384U;
constexpr int kSessionPollMilliseconds = 2;
constexpr int kAcceptPollMilliseconds = 25;
constexpr std::uint64_t kHandshakeTimeoutNs = 1000U * 1000U * 1000U;
constexpr std::uint64_t kPrunedSessionDrainTimeoutNs = 100U * 1000U * 1000U;
constexpr std::uint64_t kRuntimeCapabilities =
    MF_CLIENT_CAP_SHARED_DEVICE_V1 | MF_CLIENT_CAP_MEMFD_RING_V1 | MF_CLIENT_CAP_FUTEX_DOORBELL_V1 |
    MF_CLIENT_CAP_TIMELINE_V1 | MF_CLIENT_CAP_TELEMETRY_V1 | MF_CLIENT_CAP_PROCESS_SNAPSHOT_V1 |
    MF_CLIENT_CAP_LIVE_CONTEXT_ACCOUNTING_V1 | MF_CLIENT_CAP_COPY_REGION_V1 |
    MF_CLIENT_CAP_POLICY_SETTERS_V1 | MF_CLIENT_CAP_DIRECT_HOST_COPY_V1 |
    MF_CLIENT_CAP_KERNEL_REQUEST_V1
#if METAFLUX_DAEMON_CDEV_BACKEND
    | MF_CLIENT_CAP_CDEV_BINDING_V1
#endif
    ;

volatile std::sig_atomic_t shutdown_requested = 0;

#if METAFLUX_DAEMON_CDEV_BACKEND
[[nodiscard]] mf_shared_status_v1 cdev_backend_status(mf_backend_status_v1 status) noexcept {
  switch (status) {
  case MF_BACKEND_SUCCESS:
    return MF_SHARED_SUCCESS;
  case MF_BACKEND_INVALID_ARGUMENT:
    return MF_SHARED_INVALID_ARGUMENT;
  case MF_BACKEND_UNSUPPORTED:
    return MF_SHARED_NOT_SUPPORTED;
  case MF_BACKEND_OUT_OF_MEMORY:
    return MF_SHARED_RESOURCE_EXHAUSTED;
  case MF_BACKEND_DEVICE_LOST:
    return MF_SHARED_DEVICE_LOST;
  case MF_BACKEND_TIMEOUT:
    return MF_SHARED_TIMEOUT;
  case MF_BACKEND_BUSY:
    return MF_SHARED_WOULD_BLOCK;
  default:
    return MF_SHARED_SYSTEM_ERROR;
  }
}

[[nodiscard]] mf_shared_status_v1
cdev_retain_reference(const metaflux::transport::cdev::CdevBackendMemoryReference& reference) noexcept {
  if (reference.handle == 0U || reference.retain == nullptr || reference.release == nullptr) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  return reference.retain(reference.context, reference.handle);
}

void cdev_release_reference(
    const metaflux::transport::cdev::CdevBackendMemoryReference& reference) noexcept {
  if (reference.handle != 0U && reference.release != nullptr) {
    reference.release(reference.context, reference.handle);
  }
}
#endif

[[nodiscard]] std::uint64_t monotonic_time_ns() noexcept {
  const auto now = std::chrono::steady_clock::now().time_since_epoch();
  const auto nanoseconds = std::chrono::duration_cast<std::chrono::nanoseconds>(now).count();
  return nanoseconds <= 0 ? UINT64_C(0) : static_cast<std::uint64_t>(nanoseconds);
}

void signal_handler(int signal_number) {
  if (signal_number == SIGINT || signal_number == SIGTERM) {
    shutdown_requested = 1;
  }
}

class UniqueFd final {
public:
  UniqueFd() = default;
  explicit UniqueFd(int fd) noexcept : fd_(fd) {}
  ~UniqueFd() { reset(); }

  UniqueFd(const UniqueFd&) = delete;
  UniqueFd& operator=(const UniqueFd&) = delete;

  UniqueFd(UniqueFd&& other) noexcept : fd_(std::exchange(other.fd_, -1)) {}
  UniqueFd& operator=(UniqueFd&& other) noexcept {
    if (this != &other) {
      reset(std::exchange(other.fd_, -1));
    }
    return *this;
  }

  [[nodiscard]] int get() const noexcept { return fd_; }
  [[nodiscard]] bool valid() const noexcept { return fd_ >= 0; }
  [[nodiscard]] int release() noexcept { return std::exchange(fd_, -1); }

  void reset(int replacement = -1) noexcept {
    if (fd_ >= 0) {
      (void)close(fd_);
    }
    fd_ = replacement;
  }

private:
  int fd_ = -1;
};

[[nodiscard]] bool is_peer_memory_descriptor(int fd, pid_t peer_pid) noexcept {
  std::array<char, 64> path{};
  struct stat descriptor_attributes{};
  struct stat path_attributes{};
  struct statfs filesystem_attributes{};
  const int length =
      std::snprintf(path.data(), path.size(), "/proc/%ld/mem", static_cast<long>(peer_pid));
  const int descriptor_flags = fcntl(fd, F_GETFL);
  return fd >= 0 && peer_pid > 0 && length > 0 && static_cast<std::size_t>(length) < path.size() &&
         descriptor_flags >= 0 && (descriptor_flags & O_ACCMODE) == O_RDWR &&
         fstat(fd, &descriptor_attributes) == 0 && stat(path.data(), &path_attributes) == 0 &&
         fstatfs(fd, &filesystem_attributes) == 0 &&
         filesystem_attributes.f_type == PROC_SUPER_MAGIC &&
         descriptor_attributes.st_dev == path_attributes.st_dev &&
         descriptor_attributes.st_ino == path_attributes.st_ino;
}

[[nodiscard]] mf_shared_status_v1 copy_peer_memory(int fd, std::uint64_t address,
                                                   std::uint8_t* bytes, std::uint64_t byte_count,
                                                   bool read_from_peer) noexcept {
  const auto maximum_offset = static_cast<std::uint64_t>(std::numeric_limits<off_t>::max());
  const auto maximum_transfer = static_cast<std::uint64_t>(std::numeric_limits<ssize_t>::max());
  std::uint64_t completed = 0U;
  if (fd < 0 || address == 0U || bytes == nullptr || byte_count == 0U || address > maximum_offset ||
      byte_count > maximum_offset - address) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  while (completed < byte_count) {
    const auto transfer_size =
        static_cast<std::size_t>(std::min(byte_count - completed, maximum_transfer));
    const auto offset = static_cast<off_t>(address + completed);
    ssize_t transferred = -1;
    do {
      transferred =
          read_from_peer
              ? pread(fd, bytes + static_cast<std::size_t>(completed), transfer_size, offset)
              : pwrite(fd, bytes + static_cast<std::size_t>(completed), transfer_size, offset);
    } while (transferred < 0 && errno == EINTR);
    if (transferred <= 0) {
      return MF_SHARED_INVALID_ARGUMENT;
    }
    completed += static_cast<std::uint64_t>(transferred);
  }
  return MF_SHARED_SUCCESS;
}

enum class PacketStatus { kOk, kEof, kSystemError, kMalformed };

struct ReceivedPacket final {
  std::array<std::uint8_t, MF_CLIENT_PROTOCOL_WIRE_SIZE_V1> bytes{};
  std::array<UniqueFd, 3> descriptors{};
  std::uint32_t descriptor_count = 0;
};

[[nodiscard]] PacketStatus receive_packet(int socket_fd, std::uint32_t maximum_descriptors,
                                          ReceivedPacket& out_packet) noexcept {
  union {
    cmsghdr alignment;
    std::array<std::uint8_t, CMSG_SPACE(sizeof(int) * 3U)> bytes;
  } control{};
  iovec vector{};
  msghdr message{};
  ssize_t received = -1;
  std::array<int, 3> raw_descriptors{-1, -1, -1};
  std::uint32_t descriptor_count = 0;

  if (socket_fd < 0 || maximum_descriptors > raw_descriptors.size()) {
    return PacketStatus::kMalformed;
  }
  out_packet = ReceivedPacket{};
  for (;;) {
    control = {};
    message = {};
    vector.iov_base = out_packet.bytes.data();
    vector.iov_len = out_packet.bytes.size();
    message.msg_iov = &vector;
    message.msg_iovlen = 1;
    message.msg_control = control.bytes.data();
    message.msg_controllen = control.bytes.size();
    received = recvmsg(socket_fd, &message, MSG_CMSG_CLOEXEC);
    if (received < 0 && errno == EINTR && shutdown_requested == 0) {
      continue;
    }
    break;
  }
  if (received == 0) {
    return PacketStatus::kEof;
  }
  if (received < 0) {
    return PacketStatus::kSystemError;
  }

  for (cmsghdr* header = CMSG_FIRSTHDR(&message); header != nullptr;
       header = CMSG_NXTHDR(&message, header)) {
    if (header->cmsg_level != SOL_SOCKET || header->cmsg_type != SCM_RIGHTS ||
        header->cmsg_len < CMSG_LEN(0)) {
      for (std::uint32_t index = 0; index < descriptor_count; ++index) {
        (void)close(raw_descriptors[index]);
      }
      return PacketStatus::kMalformed;
    }
    const std::size_t payload_size = header->cmsg_len - CMSG_LEN(0);
    if (payload_size % sizeof(int) != 0U ||
        payload_size / sizeof(int) > raw_descriptors.size() - descriptor_count) {
      std::array<int, 3> incoming_descriptors{-1, -1, -1};
      const std::size_t incoming_count =
          std::min(payload_size / sizeof(int), incoming_descriptors.size());
      std::memcpy(incoming_descriptors.data(), CMSG_DATA(header), incoming_count * sizeof(int));
      for (std::size_t index = 0; index < incoming_count; ++index) {
        (void)close(incoming_descriptors[index]);
      }
      for (std::uint32_t index = 0; index < descriptor_count; ++index) {
        (void)close(raw_descriptors[index]);
      }
      return PacketStatus::kMalformed;
    }
    const auto incoming = static_cast<std::uint32_t>(payload_size / sizeof(int));
    std::memcpy(raw_descriptors.data() + descriptor_count, CMSG_DATA(header), payload_size);
    descriptor_count += incoming;
  }
  if ((message.msg_flags & (MSG_TRUNC | MSG_CTRUNC)) != 0 ||
      received != static_cast<ssize_t>(out_packet.bytes.size()) ||
      descriptor_count > maximum_descriptors) {
    for (std::uint32_t index = 0; index < descriptor_count; ++index) {
      (void)close(raw_descriptors[index]);
    }
    return PacketStatus::kMalformed;
  }
  for (std::uint32_t index = 0; index < descriptor_count; ++index) {
    out_packet.descriptors[index].reset(raw_descriptors[index]);
  }
  out_packet.descriptor_count = descriptor_count;
  return PacketStatus::kOk;
}

[[nodiscard]] bool send_packet(int socket_fd, std::span<const std::uint8_t> bytes,
                               std::span<const int> descriptors) noexcept {
  union {
    cmsghdr alignment;
    std::array<std::uint8_t, CMSG_SPACE(sizeof(int) * 3U)> bytes;
  } control{};
  iovec vector{};
  msghdr message{};
  ssize_t sent = -1;
  if (socket_fd < 0 || bytes.empty() || descriptors.size() > 3U) {
    return false;
  }
  vector.iov_base = const_cast<std::uint8_t*>(bytes.data());
  vector.iov_len = bytes.size();
  message.msg_iov = &vector;
  message.msg_iovlen = 1;
  if (!descriptors.empty()) {
    auto* header = reinterpret_cast<cmsghdr*>(control.bytes.data());
    message.msg_control = control.bytes.data();
    message.msg_controllen = CMSG_SPACE(sizeof(int) * descriptors.size());
    header->cmsg_level = SOL_SOCKET;
    header->cmsg_type = SCM_RIGHTS;
    header->cmsg_len = CMSG_LEN(sizeof(int) * descriptors.size());
    std::memcpy(CMSG_DATA(header), descriptors.data(), sizeof(int) * descriptors.size());
  }
  do {
    sent = sendmsg(socket_fd, &message, MSG_NOSIGNAL);
  } while (sent < 0 && errno == EINTR && shutdown_requested == 0);
  return sent == static_cast<ssize_t>(bytes.size());
}

class PayloadMapping final {
public:
  PayloadMapping() = default;
  ~PayloadMapping() { reset(); }
  PayloadMapping(const PayloadMapping&) = delete;
  PayloadMapping& operator=(const PayloadMapping&) = delete;

  PayloadMapping(PayloadMapping&& other) noexcept
      : fd_(std::move(other.fd_)), mapping_(std::exchange(other.mapping_, nullptr)),
        size_(std::exchange(other.size_, 0U)), writable_(other.writable_) {}

  PayloadMapping& operator=(PayloadMapping&& other) noexcept {
    if (this != &other) {
      reset();
      fd_ = std::move(other.fd_);
      mapping_ = std::exchange(other.mapping_, nullptr);
      size_ = std::exchange(other.size_, 0U);
      writable_ = other.writable_;
    }
    return *this;
  }

  [[nodiscard]] static mf_shared_status_v1 map(UniqueFd fd, std::uint64_t expected_size,
                                               bool writable, bool immutable,
                                               PayloadMapping& out_mapping) noexcept {
    struct stat attributes{};
    if (!fd.valid() || expected_size == 0U || expected_size > kMaximumObjectSize ||
        expected_size > std::numeric_limits<std::size_t>::max() ||
        fstat(fd.get(), &attributes) != 0 || !S_ISREG(attributes.st_mode) ||
        attributes.st_size < 0 || static_cast<std::uint64_t>(attributes.st_size) != expected_size) {
      return MF_SHARED_INVALID_ARGUMENT;
    }
    const int seals = fcntl(fd.get(), F_GET_SEALS);
    const int required_seals = F_SEAL_GROW | F_SEAL_SHRINK | F_SEAL_SEAL;
    if (seals < 0 || (seals & required_seals) != required_seals ||
        (immutable && (seals & F_SEAL_WRITE) == 0) || (writable && (seals & F_SEAL_WRITE) != 0)) {
      return MF_SHARED_INVALID_ARGUMENT;
    }
    const int protection = writable ? PROT_READ | PROT_WRITE : PROT_READ;
    void* mapping =
        mmap(nullptr, static_cast<std::size_t>(expected_size), protection, MAP_SHARED, fd.get(), 0);
    if (mapping == MAP_FAILED) {
      return MF_SHARED_SYSTEM_ERROR;
    }
    out_mapping.reset();
    out_mapping.fd_ = std::move(fd);
    out_mapping.mapping_ = mapping;
    out_mapping.size_ = expected_size;
    out_mapping.writable_ = writable;
    return MF_SHARED_SUCCESS;
  }

  [[nodiscard]] int fd() const noexcept { return fd_.get(); }
  [[nodiscard]] std::uint64_t size() const noexcept { return size_; }
  [[nodiscard]] const std::uint8_t* data() const noexcept {
    return static_cast<const std::uint8_t*>(mapping_);
  }
  [[nodiscard]] std::uint8_t* mutable_data() noexcept {
    return writable_ ? static_cast<std::uint8_t*>(mapping_) : nullptr;
  }

  void reset() noexcept {
    if (mapping_ != nullptr && size_ <= std::numeric_limits<std::size_t>::max()) {
      (void)munmap(mapping_, static_cast<std::size_t>(size_));
    }
    mapping_ = nullptr;
    size_ = 0;
    writable_ = false;
    fd_.reset();
  }

private:
  UniqueFd fd_;
  void* mapping_ = nullptr;
  std::uint64_t size_ = 0;
  bool writable_ = false;
};

struct ProcessSnapshotPayload final {
  UniqueFd fd;
  std::uint64_t revision = 0;
  std::uint64_t size = 0;
};

struct ProcessAdmission final {
  std::atomic_bool admitted{true};
  std::atomic<std::uint64_t> tombstone_deadline_ns{0U};
  std::stop_source cancellation;

  [[nodiscard]] bool tombstone() noexcept {
    const std::uint64_t now = monotonic_time_ns();
    const std::uint64_t deadline =
        now > std::numeric_limits<std::uint64_t>::max() - kPrunedSessionDrainTimeoutNs
            ? std::numeric_limits<std::uint64_t>::max()
            : now + kPrunedSessionDrainTimeoutNs;
    std::uint64_t expected = 0U;
    if (!tombstone_deadline_ns.compare_exchange_strong(expected, deadline,
                                                       std::memory_order_acq_rel)) {
      return false;
    }
    admitted.store(false, std::memory_order_release);
    (void)cancellation.request_stop();
    return true;
  }
};

struct ProcessSessionRecord final {
  std::uint64_t session_id = 0;
  std::uint32_t pid = 0;
  std::uint64_t start_time_ticks = 0;
  std::uint64_t used_memory_bytes = 0;
  std::uint32_t live_contexts = 0;
  std::shared_ptr<ProcessAdmission> admission;
  std::uint32_t name_length = 0;
  std::array<std::uint8_t, MF_CLIENT_PROCESS_NAME_SIZE_V1> name{};
};

[[nodiscard]] std::optional<std::uint64_t> process_start_time(std::uint32_t pid) noexcept {
  std::array<char, 4096> bytes{};
  std::array<char, 64> path{};
  const int length = std::snprintf(path.data(), path.size(), "/proc/%u/stat", pid);
  if (length <= 0 || static_cast<std::size_t>(length) >= path.size()) {
    return std::nullopt;
  }
  UniqueFd fd(open(path.data(), O_RDONLY | O_CLOEXEC));
  if (!fd.valid()) {
    return std::nullopt;
  }
  ssize_t count = -1;
  do {
    count = read(fd.get(), bytes.data(), bytes.size() - 1U);
  } while (count < 0 && errno == EINTR);
  if (count <= 0) {
    return std::nullopt;
  }
  const std::string_view text(bytes.data(), static_cast<std::size_t>(count));
  const std::size_t close = text.rfind(')');
  if (close == std::string_view::npos || close + 2U >= text.size()) {
    return std::nullopt;
  }
  std::size_t cursor = close + 2U;
  for (std::uint32_t field = 3U; field <= 22U; ++field) {
    const std::size_t end = text.find(' ', cursor);
    const std::size_t token_end = end == std::string_view::npos ? text.size() : end;
    if (token_end <= cursor) {
      return std::nullopt;
    }
    if (field == 22U) {
      std::uint64_t value = 0;
      const char* begin = text.data() + cursor;
      const char* finish = text.data() + token_end;
      const auto [position, error] = std::from_chars(begin, finish, value);
      if (error != std::errc{} || position != finish || value == 0U) {
        return std::nullopt;
      }
      return value;
    }
    if (end == std::string_view::npos) {
      return std::nullopt;
    }
    cursor = end + 1U;
  }
  return std::nullopt;
}

[[nodiscard]] bool process_name(std::uint32_t pid,
                                std::array<std::uint8_t, MF_CLIENT_PROCESS_NAME_SIZE_V1>& out_name,
                                std::uint32_t& out_length) noexcept {
  std::array<char, 4096> target{};
  std::array<char, 64> path{};
  int length = std::snprintf(path.data(), path.size(), "/proc/%u/exe", pid);
  if (length <= 0 || static_cast<std::size_t>(length) >= path.size()) {
    return false;
  }
  ssize_t target_length = readlink(path.data(), target.data(), target.size() - 1U);
  std::string_view name;
  if (target_length > 0) {
    const std::string_view full(target.data(), static_cast<std::size_t>(target_length));
    const std::size_t slash = full.rfind('/');
    name = slash == std::string_view::npos ? full : full.substr(slash + 1U);
  } else {
    length = std::snprintf(path.data(), path.size(), "/proc/%u/comm", pid);
    if (length <= 0 || static_cast<std::size_t>(length) >= path.size()) {
      return false;
    }
    UniqueFd fd(open(path.data(), O_RDONLY | O_CLOEXEC));
    if (!fd.valid()) {
      return false;
    }
    do {
      target_length = read(fd.get(), target.data(), target.size() - 1U);
    } while (target_length < 0 && errno == EINTR);
    if (target_length <= 0) {
      return false;
    }
    name = std::string_view(target.data(), static_cast<std::size_t>(target_length));
    while (!name.empty() && (name.back() == '\n' || name.back() == '\r')) {
      name.remove_suffix(1U);
    }
  }
  if (name.empty()) {
    return false;
  }
  const std::size_t copied =
      std::min(name.size(), static_cast<std::size_t>(MF_CLIENT_PROCESS_NAME_SIZE_V1 - 1U));
  std::fill(out_name.begin(), out_name.end(), UINT8_C(0));
  std::memcpy(out_name.data(), name.data(), copied);
  out_length = static_cast<std::uint32_t>(copied);
  return true;
}

class RegistryAuthority final {
public:
  ~RegistryAuthority();

  [[nodiscard]] mf_shared_status_v1 initialize(mf_registry_view_id_v1 view_id) noexcept;
  [[nodiscard]] mf_shared_status_v1 adjust_memory(std::uint64_t byte_count, bool allocate) noexcept;
  [[nodiscard]] mf_shared_status_v1 record_work(std::uint64_t start_ns, std::uint64_t end_ns,
                                                bool memory_active) noexcept;
  [[nodiscard]] mf_shared_status_v1 refresh_telemetry() noexcept;
  [[nodiscard]] mf_shared_status_v1 update_policy(std::uint64_t identity_record_id,
                                                  std::uint64_t policy_mask,
                                                  std::uint64_t policy_value,
                                                  std::uint64_t& out_lifecycle_sequence) noexcept;
  [[nodiscard]] int borrow_fd() const noexcept { return fd_.get(); }
  [[nodiscard]] mf_registry_view_id_v1 view_id() const noexcept { return view_id_; }
  [[nodiscard]] std::uint64_t device_generation() const noexcept;
#if METAFLUX_DAEMON_CDEV_BACKEND
  [[nodiscard]] bool attach_cdev_worker(
      metaflux::transport::cdev::CdevWorker& worker) noexcept;
  [[nodiscard]] bool detach_cdev_worker(
      metaflux::transport::cdev::CdevWorker& worker) noexcept;
  [[nodiscard]] mf_shared_status_v1 report_cdev_loss() noexcept;
#endif

private:
  struct WorkInterval final {
    std::uint64_t start_ns;
    std::uint64_t end_ns;
  };

  static void insert_interval_locked(std::vector<WorkInterval>& intervals, WorkInterval interval);
  static std::uint64_t window_active_time_locked(std::vector<WorkInterval>& intervals,
                                                 std::uint64_t window_start,
                                                 std::uint64_t now_ns) noexcept;
  [[nodiscard]] mf_shared_status_v1 publish_locked(std::uint64_t now_ns) noexcept;

  mutable std::mutex mutex_;
  UniqueFd fd_;
  void* mapping_ = nullptr;
  std::uint64_t mapping_size_ = 0;
  mf_registry_view_id_v1 view_id_{};
  runtime::RegistryView view_;
  std::uint64_t memory_used_ = 0;
  std::uint64_t committed_work_items_ = 0;
  std::uint64_t completed_work_items_ = 0;
  std::uint64_t lifecycle_sequence_ = 1U;
  std::optional<runtime::lifecycle::Coordinator> lifecycle_;
  std::optional<runtime::lifecycle::ProducerIngress> lifecycle_ingress_;
  std::vector<WorkInterval> compute_intervals_;
  std::vector<WorkInterval> memory_intervals_;
  bool initialized_ = false;
};

RegistryAuthority::~RegistryAuthority() {
  const std::scoped_lock lock(mutex_);
  if (initialized_) {
    (void)view_.close();
    initialized_ = false;
  }
  if (mapping_ != nullptr && mapping_size_ <= std::numeric_limits<std::size_t>::max()) {
    (void)munmap(mapping_, static_cast<std::size_t>(mapping_size_));
  }
  mapping_ = nullptr;
  fd_.reset();
}

mf_shared_status_v1 RegistryAuthority::initialize(mf_registry_view_id_v1 view_id) noexcept {
  const std::scoped_lock lock(mutex_);
  if (initialized_ ||
      runtime::RegistryView::required_recovery_mapping_size(1U, mapping_size_) !=
          MF_SHARED_SUCCESS ||
      mapping_size_ > static_cast<std::uint64_t>(std::numeric_limits<off_t>::max())) {
    return MF_SHARED_SYSTEM_ERROR;
  }
  const long created =
      syscall(SYS_memfd_create, "metaflux-registry-v1", MFD_CLOEXEC | MFD_ALLOW_SEALING);
  if (created < 0 || created > std::numeric_limits<int>::max()) {
    return MF_SHARED_SYSTEM_ERROR;
  }
  fd_.reset(static_cast<int>(created));
  if (ftruncate(fd_.get(), static_cast<off_t>(mapping_size_)) != 0) {
    return MF_SHARED_SYSTEM_ERROR;
  }
  mapping_ = mmap(nullptr, static_cast<std::size_t>(mapping_size_), PROT_READ | PROT_WRITE,
                  MAP_SHARED, fd_.get(), 0);
  if (mapping_ == MAP_FAILED) {
    mapping_ = nullptr;
    return MF_SHARED_SYSTEM_ERROR;
  }

  std::array<mf_virtual_device_identity_v1, 1> identities{};
  identities[0].identity_record_id = kIdentityRecordId;
  constexpr std::array<std::uint8_t, 16> logical_id{
      0x4d, 0x46, 0x58, 0x2d, 0x43, 0x50, 0x55, 0x2d,
      0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x31,
  };
  constexpr std::array<std::uint8_t, 16> gpu_uuid{
      0x4d, 0x46, 0x58, 0x43, 0x50, 0x55, 0x00, 0x01,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
  };
  std::copy(logical_id.begin(), logical_id.end(), identities[0].logical_device_id);
  std::copy(gpu_uuid.begin(), gpu_uuid.end(), identities[0].gpu_uuid);
  constexpr char display_name[] = "MetaFlux Virtual Compute Device";
  std::memcpy(identities[0].display_name, display_name, sizeof(display_name));
  identities[0].committed_generation = kDeviceGeneration;
  identities[0].capability_bits = 1U;
  identities[0].backend_id = 1U;
  identities[0].virtual_compute_capability = 70U;
  identities[0].pci_domain = 0U;
  identities[0].pci_bus = 0U;
  identities[0].pci_device = 1U;
  identities[0].pci_function = 0U;
  const std::array<runtime::FenceSnapshot, 1> fences{{
      {kIdentityRecordId, 1U, 1U, kMemoryCapacity, 0U, MF_DEVICE_STATE_ONLINE},
  }};
  if (runtime::RegistryView::initialize(mapping_, mapping_size_, view_id, view_id.view_serial,
                                        identities, fences, view_) != MF_SHARED_SUCCESS ||
      fcntl(fd_.get(), F_ADD_SEALS,
            F_SEAL_GROW | F_SEAL_SHRINK | F_SEAL_FUTURE_WRITE | F_SEAL_SEAL) != 0) {
    return MF_SHARED_SYSTEM_ERROR;
  }
  view_id_ = view_id;
  initialized_ = true;
  runtime::lifecycle::Config lifecycle_config{};
  lifecycle_config.logical_device_id = kIdentityRecordId;
  lifecycle_config.daemon_incarnation = view_id.daemon_incarnation;
  lifecycle_config.initial_identity_record_id = kIdentityRecordId;
  lifecycle_config.initial_generation = kDeviceGeneration;
  lifecycle_config.initial_epoch = 1U;
  lifecycle_.emplace(lifecycle_config);
  if (!lifecycle_->valid()) {
    return MF_SHARED_SYSTEM_ERROR;
  }
  lifecycle_ingress_.emplace(*lifecycle_);
  return publish_locked(monotonic_time_ns());
}

std::uint64_t RegistryAuthority::device_generation() const noexcept {
  const std::scoped_lock lock(mutex_);
  return lifecycle_.has_value() ? lifecycle_->snapshot().generation : kDeviceGeneration;
}

#if METAFLUX_DAEMON_CDEV_BACKEND
bool RegistryAuthority::attach_cdev_worker(
    metaflux::transport::cdev::CdevWorker& worker) noexcept {
  const std::scoped_lock lock(mutex_);
  return initialized_ && lifecycle_.has_value() && worker.attach_lifecycle(*lifecycle_);
}

bool RegistryAuthority::detach_cdev_worker(
    metaflux::transport::cdev::CdevWorker& worker) noexcept {
  const std::scoped_lock lock(mutex_);
  return lifecycle_.has_value() && lifecycle_->unregister_mirror(
                                      metaflux::runtime::lifecycle::MirrorKind::Cdev, &worker);
}

mf_shared_status_v1 RegistryAuthority::report_cdev_loss() noexcept {
  const std::scoped_lock lock(mutex_);
  if (!initialized_ || !lifecycle_.has_value() || !lifecycle_ingress_.has_value()) {
    return MF_SHARED_SYSTEM_ERROR;
  }
  metaflux::runtime::lifecycle::ResultDetails details{};
  if (lifecycle_ingress_->submit_immediate(
          metaflux::runtime::lifecycle::ExternalEventKind::CdevDisconnect, 0U, details) !=
      metaflux::runtime::lifecycle::NormalizationResult::Accepted) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  switch (details.result) {
  case metaflux::runtime::lifecycle::Result::Accepted:
  case metaflux::runtime::lifecycle::Result::Duplicate:
    return MF_SHARED_SUCCESS;
  case metaflux::runtime::lifecycle::Result::DeviceLost:
    return MF_SHARED_DEVICE_LOST;
  case metaflux::runtime::lifecycle::Result::Stale:
    return MF_SHARED_STALE_HANDLE;
  case metaflux::runtime::lifecycle::Result::Timeout:
    return MF_SHARED_TIMEOUT;
  case metaflux::runtime::lifecycle::Result::ResourceExhausted:
    return MF_SHARED_RESOURCE_EXHAUSTED;
  default:
    return MF_SHARED_SYSTEM_ERROR;
  }
}
#endif

void RegistryAuthority::insert_interval_locked(std::vector<WorkInterval>& intervals,
                                               WorkInterval interval) {
  auto position = std::lower_bound(intervals.begin(), intervals.end(), interval.start_ns,
                                   [](const WorkInterval& existing, std::uint64_t start_ns) {
                                     return existing.start_ns < start_ns;
                                   });
  if (position != intervals.begin() && std::prev(position)->end_ns >= interval.start_ns) {
    --position;
    interval.start_ns = std::min(interval.start_ns, position->start_ns);
    interval.end_ns = std::max(interval.end_ns, position->end_ns);
    position = intervals.erase(position);
  }
  while (position != intervals.end() && position->start_ns <= interval.end_ns) {
    interval.end_ns = std::max(interval.end_ns, position->end_ns);
    position = intervals.erase(position);
  }
  intervals.insert(position, interval);
}

std::uint64_t RegistryAuthority::window_active_time_locked(std::vector<WorkInterval>& intervals,
                                                           std::uint64_t window_start,
                                                           std::uint64_t now_ns) noexcept {
  std::erase_if(intervals, [window_start](const WorkInterval& interval) {
    return interval.end_ns <= window_start;
  });
  std::uint64_t active_time_ns = 0U;
  for (const WorkInterval& interval : intervals) {
    const std::uint64_t start = std::max(interval.start_ns, window_start);
    const std::uint64_t end = std::min(interval.end_ns, now_ns);
    if (end > start) {
      active_time_ns += end - start;
    }
  }
  return std::min(active_time_ns, kUtilizationWindowNs);
}

mf_shared_status_v1 RegistryAuthority::publish_locked(std::uint64_t now_ns) noexcept {
  if (!initialized_ || memory_used_ > kMemoryCapacity || now_ns == 0U) {
    return MF_SHARED_SYSTEM_ERROR;
  }
  const std::uint64_t window_start =
      now_ns > kUtilizationWindowNs ? now_ns - kUtilizationWindowNs : 0U;
  const std::uint64_t active_time_ns =
      window_active_time_locked(compute_intervals_, window_start, now_ns);
  const std::uint64_t memory_active_time_ns =
      window_active_time_locked(memory_intervals_, window_start, now_ns);
  mf_virtual_device_telemetry_v1 row{};
  row.identity_record_id = kIdentityRecordId;
  row.observed_lifecycle_sequence = lifecycle_sequence_;
  row.committed_work_items = committed_work_items_;
  row.completed_work_items = completed_work_items_;
  row.active_time_ns = active_time_ns;
  row.memory_active_time_ns = memory_active_time_ns;
  row.memory_used_bytes = memory_used_;
  row.memory_capacity_bytes = kMemoryCapacity;
  row.sample_time_ns = now_ns;
  return view_.publish_telemetry(std::span<const mf_virtual_device_telemetry_v1>(&row, 1U));
}

mf_shared_status_v1 RegistryAuthority::adjust_memory(std::uint64_t byte_count,
                                                     bool allocate) noexcept {
  if (byte_count == 0U) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  const std::scoped_lock lock(mutex_);
  if ((allocate && byte_count > kMemoryCapacity - memory_used_) ||
      (!allocate && byte_count > memory_used_)) {
    return MF_SHARED_OVERFLOW;
  }
  const std::uint64_t previous = memory_used_;
  memory_used_ = allocate ? memory_used_ + byte_count : memory_used_ - byte_count;
  const mf_shared_status_v1 status = publish_locked(monotonic_time_ns());
  if (status != MF_SHARED_SUCCESS) {
    memory_used_ = previous;
  }
  return status;
}

mf_shared_status_v1 RegistryAuthority::record_work(std::uint64_t start_ns, std::uint64_t end_ns,
                                                   bool memory_active) noexcept {
  if (start_ns == 0U || end_ns < start_ns) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  const std::scoped_lock lock(mutex_);
  if (committed_work_items_ == UINT64_MAX || completed_work_items_ == UINT64_MAX) {
    return MF_SHARED_OVERFLOW;
  }
  if (end_ns == start_ns) {
    if (end_ns == UINT64_MAX) {
      return MF_SHARED_OVERFLOW;
    }
    ++end_ns;
  }
  try {
    compute_intervals_.reserve(compute_intervals_.size() + 1U);
    if (memory_active) {
      memory_intervals_.reserve(memory_intervals_.size() + 1U);
    }
    insert_interval_locked(compute_intervals_, {start_ns, end_ns});
    if (memory_active) {
      insert_interval_locked(memory_intervals_, {start_ns, end_ns});
    }
  } catch (const std::bad_alloc&) {
    return MF_SHARED_RESOURCE_EXHAUSTED;
  }
  ++committed_work_items_;
  ++completed_work_items_;
  return publish_locked(end_ns);
}

mf_shared_status_v1 RegistryAuthority::refresh_telemetry() noexcept {
  const std::scoped_lock lock(mutex_);
  return publish_locked(monotonic_time_ns());
}

mf_shared_status_v1
RegistryAuthority::update_policy(std::uint64_t identity_record_id, std::uint64_t policy_mask,
                                 std::uint64_t policy_value,
                                 std::uint64_t& out_lifecycle_sequence) noexcept {
  const std::scoped_lock lock(mutex_);
  if (!initialized_ || identity_record_id != kIdentityRecordId || policy_mask == 0U ||
      (policy_mask & ~MF_DEVICE_POLICY_KNOWN_BITS_V1) != 0U ||
      (policy_value & ~policy_mask) != 0U) {
    return MF_SHARED_INVALID_ARGUMENT;
  }

  const std::uint64_t generation =
      lifecycle_.has_value() ? lifecycle_->snapshot().generation : kDeviceGeneration;
  if (generation == 0U) {
    return MF_SHARED_DEVICE_LOST;
  }
  mf_generation_handle_v1 handle{};
  runtime::FenceSnapshot observed{};
  mf_shared_status_v1 status =
      view_.make_handle(0U, kIdentityRecordId, generation, MF_OBJECT_TYPE_CONTEXT, handle);
  if (status == MF_SHARED_SUCCESS) {
    status = view_.validate_device(handle, observed);
  }
  if (status != MF_SHARED_SUCCESS) {
    return status;
  }
  lifecycle_sequence_ = observed.lifecycle_sequence;
  if ((observed.policy_bits & policy_mask) == policy_value) {
    out_lifecycle_sequence = observed.lifecycle_sequence;
    return MF_SHARED_SUCCESS;
  }
  if (observed.lifecycle_sequence == std::numeric_limits<std::uint64_t>::max()) {
    return MF_SHARED_OVERFLOW;
  }

  const auto* header = static_cast<const mf_shared_registry_header_v1*>(mapping_);
  const auto* device_admission = reinterpret_cast<const mf_device_admission_control_v1*>(
      static_cast<const std::uint8_t*>(mapping_) + header->device_admission_offset);
  const std::uint64_t admission =
      mf_atomic_load_u64_acquire(&device_admission[0].state_generation_tag);
  if (mf_device_admission_state_v1(admission) != MF_DEVICE_ADMISSION_OPEN) {
    return mf_device_admission_state_v1(admission) == MF_DEVICE_ADMISSION_UPDATING
               ? MF_SHARED_RETRY
               : MF_SHARED_DEVICE_LOST;
  }

  runtime::FenceSnapshot intended = observed;
  intended.lifecycle_sequence = observed.lifecycle_sequence + 1U;
  intended.policy_bits = (observed.policy_bits & ~policy_mask) | policy_value;
  std::uint32_t next_validation_generation = 0U;
  status = view_.publish_fence(0U, mf_device_admission_generation_v1(admission), intended,
                               next_validation_generation);
  if (status != MF_SHARED_SUCCESS || next_validation_generation == 0U) {
    return status == MF_SHARED_SUCCESS ? MF_SHARED_SYSTEM_ERROR : status;
  }
  lifecycle_sequence_ = intended.lifecycle_sequence;
  (void)publish_locked(monotonic_time_ns());

  status =
      view_.make_handle(0U, kIdentityRecordId, generation, MF_OBJECT_TYPE_CONTEXT, handle);
  if (status == MF_SHARED_SUCCESS) {
    status = view_.validate_device(handle, observed);
  }
  if (status != MF_SHARED_SUCCESS || observed.lifecycle_sequence < intended.lifecycle_sequence ||
      (observed.policy_bits & policy_mask) != policy_value) {
    return status == MF_SHARED_SUCCESS ? MF_SHARED_SYSTEM_ERROR : status;
  }
  out_lifecycle_sequence = observed.lifecycle_sequence;
  return MF_SHARED_SUCCESS;
}

class ResourceAuthority final {
public:
  [[nodiscard]] mf_shared_status_v1 reserve_session(std::uint32_t uid) noexcept;
  void release_session(std::uint32_t uid) noexcept;
  [[nodiscard]] mf_shared_status_v1 reserve_mapping(std::uint32_t uid,
                                                    std::uint64_t byte_count) noexcept;
  void release_mapping(std::uint32_t uid, std::uint64_t byte_count) noexcept;

private:
  struct UidUsage final {
    std::uint32_t uid = 0;
    std::size_t sessions = 0;
    std::uint64_t mapped_bytes = 0;
    std::size_t mapped_objects = 0;
  };

  void erase_empty_locked() {
    std::erase_if(by_uid_, [](const UidUsage& usage) {
      return usage.sessions == 0U && usage.mapped_bytes == 0U && usage.mapped_objects == 0U;
    });
  }

  std::mutex mutex_;
  std::vector<UidUsage> by_uid_;
  std::size_t global_sessions_ = 0;
  std::uint64_t global_mapped_bytes_ = 0;
  std::size_t global_mapped_objects_ = 0;
};

mf_shared_status_v1 ResourceAuthority::reserve_session(std::uint32_t uid) noexcept {
  const std::scoped_lock lock(mutex_);
  auto found = std::find_if(by_uid_.begin(), by_uid_.end(),
                            [uid](const UidUsage& usage) { return usage.uid == uid; });
  if (global_sessions_ >= kMaximumSessions ||
      (found != by_uid_.end() && found->sessions >= kMaximumUidSessions)) {
    return MF_SHARED_RESOURCE_EXHAUSTED;
  }
  if (found == by_uid_.end()) {
    try {
      by_uid_.push_back(UidUsage{.uid = uid});
    } catch (const std::bad_alloc&) {
      return MF_SHARED_RESOURCE_EXHAUSTED;
    }
    found = std::prev(by_uid_.end());
  }
  ++found->sessions;
  ++global_sessions_;
  return MF_SHARED_SUCCESS;
}

void ResourceAuthority::release_session(std::uint32_t uid) noexcept {
  const std::scoped_lock lock(mutex_);
  const auto found = std::find_if(by_uid_.begin(), by_uid_.end(),
                                  [uid](const UidUsage& usage) { return usage.uid == uid; });
  if (found != by_uid_.end() && found->sessions != 0U && global_sessions_ != 0U) {
    --found->sessions;
    --global_sessions_;
    erase_empty_locked();
  }
}

mf_shared_status_v1 ResourceAuthority::reserve_mapping(std::uint32_t uid,
                                                       std::uint64_t byte_count) noexcept {
  if (byte_count == 0U || byte_count > kMaximumMappedBytes) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  const std::scoped_lock lock(mutex_);
  auto found = std::find_if(by_uid_.begin(), by_uid_.end(),
                            [uid](const UidUsage& usage) { return usage.uid == uid; });
  const std::uint64_t uid_bytes = found == by_uid_.end() ? 0U : found->mapped_bytes;
  const std::size_t uid_objects = found == by_uid_.end() ? 0U : found->mapped_objects;
  if (byte_count > kMaximumMappedBytes - uid_bytes ||
      byte_count > kMaximumGlobalMappedBytes - global_mapped_bytes_ ||
      uid_objects >= kMaximumObjects || global_mapped_objects_ >= kMaximumGlobalMappedObjects) {
    return MF_SHARED_RESOURCE_EXHAUSTED;
  }
  if (found == by_uid_.end()) {
    try {
      by_uid_.push_back(UidUsage{.uid = uid});
    } catch (const std::bad_alloc&) {
      return MF_SHARED_RESOURCE_EXHAUSTED;
    }
    found = std::prev(by_uid_.end());
  }
  found->mapped_bytes += byte_count;
  ++found->mapped_objects;
  global_mapped_bytes_ += byte_count;
  ++global_mapped_objects_;
  return MF_SHARED_SUCCESS;
}

void ResourceAuthority::release_mapping(std::uint32_t uid, std::uint64_t byte_count) noexcept {
  if (byte_count == 0U) {
    return;
  }
  const std::scoped_lock lock(mutex_);
  const auto found = std::find_if(by_uid_.begin(), by_uid_.end(),
                                  [uid](const UidUsage& usage) { return usage.uid == uid; });
  if (found != by_uid_.end() && byte_count <= found->mapped_bytes && found->mapped_objects != 0U &&
      byte_count <= global_mapped_bytes_ && global_mapped_objects_ != 0U) {
    found->mapped_bytes -= byte_count;
    --found->mapped_objects;
    global_mapped_bytes_ -= byte_count;
    --global_mapped_objects_;
    erase_empty_locked();
  }
}

class SessionLease final {
public:
  SessionLease() = default;
  ~SessionLease() { reset(); }

  SessionLease(const SessionLease&) = delete;
  SessionLease& operator=(const SessionLease&) = delete;

  SessionLease(SessionLease&& other) noexcept
      : authority_(std::move(other.authority_)), uid_(std::exchange(other.uid_, 0U)) {}
  SessionLease& operator=(SessionLease&& other) noexcept {
    if (this != &other) {
      reset();
      authority_ = std::move(other.authority_);
      uid_ = std::exchange(other.uid_, 0U);
    }
    return *this;
  }

  [[nodiscard]] static std::optional<SessionLease>
  acquire(std::shared_ptr<ResourceAuthority> authority, std::uint32_t uid) noexcept {
    if (authority == nullptr || authority->reserve_session(uid) != MF_SHARED_SUCCESS) {
      return std::nullopt;
    }
    return SessionLease(std::move(authority), uid);
  }

  [[nodiscard]] bool valid_for(std::uint32_t uid) const noexcept {
    return authority_ != nullptr && uid_ == uid;
  }

private:
  SessionLease(std::shared_ptr<ResourceAuthority> authority, std::uint32_t uid) noexcept
      : authority_(std::move(authority)), uid_(uid) {}

  void reset() noexcept {
    if (authority_ != nullptr) {
      authority_->release_session(uid_);
      authority_.reset();
      uid_ = 0U;
    }
  }

  std::shared_ptr<ResourceAuthority> authority_;
  std::uint32_t uid_ = 0;
};

class ProcessAuthority final {
public:
  explicit ProcessAuthority(std::shared_ptr<RegistryAuthority> registry_authority) noexcept
      : registry_authority_(std::move(registry_authority)) {}

  [[nodiscard]] mf_shared_status_v1
  register_session(std::uint32_t pid, bool live_context_accounting, std::uint64_t& out_session_id,
                   std::shared_ptr<ProcessAdmission>& out_admission) noexcept;
  void unregister_session(std::uint64_t session_id) noexcept;
  [[nodiscard]] mf_shared_status_v1 allocate(std::uint64_t session_id,
                                             std::uint64_t byte_count) noexcept;
  [[nodiscard]] mf_shared_status_v1 release(std::uint64_t session_id,
                                            std::uint64_t byte_count) noexcept;
  [[nodiscard]] mf_shared_status_v1 context_acquire(std::uint64_t session_id) noexcept;
  [[nodiscard]] mf_shared_status_v1 context_release(std::uint64_t session_id) noexcept;
  [[nodiscard]] mf_shared_status_v1 snapshot(ProcessSnapshotPayload& out_payload) noexcept;

private:
  void advance_revision_locked() noexcept {
    if (revision_ != std::numeric_limits<std::uint64_t>::max()) {
      ++revision_;
    }
  }
  void prune_dead_locked() noexcept;

  std::mutex mutex_;
  std::shared_ptr<RegistryAuthority> registry_authority_;
  std::vector<ProcessSessionRecord> sessions_;
  std::uint64_t revision_ = 1U;
  std::uint64_t next_session_id_ = 1U;
};

mf_shared_status_v1
ProcessAuthority::register_session(std::uint32_t pid, bool live_context_accounting,
                                   std::uint64_t& out_session_id,
                                   std::shared_ptr<ProcessAdmission>& out_admission) noexcept {
  const std::optional<std::uint64_t> start = process_start_time(pid);
  ProcessSessionRecord record{};
  if (pid == 0U || !start.has_value() || !process_name(pid, record.name, record.name_length)) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  record.pid = pid;
  record.start_time_ticks = *start;
  record.live_contexts = live_context_accounting ? 0U : 1U;
  try {
    record.admission = std::make_shared<ProcessAdmission>();
    const std::scoped_lock lock(mutex_);
    prune_dead_locked();
    const std::optional<std::uint64_t> current = process_start_time(pid);
    if (!current.has_value() || *current != *start) {
      return MF_SHARED_STALE_HANDLE;
    }
    if (sessions_.size() >= kMaximumSessions ||
        next_session_id_ == std::numeric_limits<std::uint64_t>::max() ||
        revision_ == std::numeric_limits<std::uint64_t>::max()) {
      return MF_SHARED_RESOURCE_EXHAUSTED;
    }
    record.session_id = next_session_id_;
    sessions_.push_back(record);
    out_session_id = next_session_id_++;
    out_admission = std::move(record.admission);
    advance_revision_locked();
    return MF_SHARED_SUCCESS;
  } catch (const std::bad_alloc&) {
    return MF_SHARED_RESOURCE_EXHAUSTED;
  }
}

void ProcessAuthority::unregister_session(std::uint64_t session_id) noexcept {
  const std::scoped_lock lock(mutex_);
  const auto found = std::find_if(sessions_.begin(), sessions_.end(),
                                  [&](const auto& row) { return row.session_id == session_id; });
  if (found != sessions_.end()) {
    (void)found->admission->tombstone();
    if (found->used_memory_bytes != 0U) {
      (void)registry_authority_->adjust_memory(found->used_memory_bytes, false);
    }
    sessions_.erase(found);
    advance_revision_locked();
  }
}

mf_shared_status_v1 ProcessAuthority::allocate(std::uint64_t session_id,
                                               std::uint64_t byte_count) noexcept {
  if (byte_count == 0U) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  const std::scoped_lock lock(mutex_);
  const auto found = std::find_if(sessions_.begin(), sessions_.end(),
                                  [&](const auto& row) { return row.session_id == session_id; });
  if (found == sessions_.end() || !found->admission->admitted.load(std::memory_order_acquire)) {
    return MF_SHARED_STALE_HANDLE;
  }
  std::uint64_t aggregate = 0;
  for (const auto& row : sessions_) {
    if (row.pid == found->pid && row.start_time_ticks == found->start_time_ticks) {
      if (row.used_memory_bytes > std::numeric_limits<std::uint64_t>::max() - aggregate) {
        return MF_SHARED_OVERFLOW;
      }
      aggregate += row.used_memory_bytes;
    }
  }
  if (byte_count > std::numeric_limits<std::uint64_t>::max() - aggregate ||
      byte_count > std::numeric_limits<std::uint64_t>::max() - found->used_memory_bytes ||
      revision_ == std::numeric_limits<std::uint64_t>::max()) {
    return MF_SHARED_OVERFLOW;
  }
  const mf_shared_status_v1 telemetry_status = registry_authority_->adjust_memory(byte_count, true);
  if (telemetry_status != MF_SHARED_SUCCESS) {
    return telemetry_status;
  }
  found->used_memory_bytes += byte_count;
  advance_revision_locked();
  return MF_SHARED_SUCCESS;
}

mf_shared_status_v1 ProcessAuthority::release(std::uint64_t session_id,
                                              std::uint64_t byte_count) noexcept {
  if (byte_count == 0U) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  const std::scoped_lock lock(mutex_);
  const auto found = std::find_if(sessions_.begin(), sessions_.end(),
                                  [&](const auto& row) { return row.session_id == session_id; });
  if (found == sessions_.end() || !found->admission->admitted.load(std::memory_order_acquire)) {
    return MF_SHARED_STALE_HANDLE;
  }
  if (byte_count > found->used_memory_bytes ||
      revision_ == std::numeric_limits<std::uint64_t>::max()) {
    return MF_SHARED_OVERFLOW;
  }
  const mf_shared_status_v1 telemetry_status =
      registry_authority_->adjust_memory(byte_count, false);
  if (telemetry_status != MF_SHARED_SUCCESS) {
    return telemetry_status;
  }
  found->used_memory_bytes -= byte_count;
  advance_revision_locked();
  return MF_SHARED_SUCCESS;
}

mf_shared_status_v1 ProcessAuthority::context_acquire(std::uint64_t session_id) noexcept {
  const std::scoped_lock lock(mutex_);
  const auto found = std::find_if(sessions_.begin(), sessions_.end(),
                                  [&](const auto& row) { return row.session_id == session_id; });
  if (found == sessions_.end() || !found->admission->admitted.load(std::memory_order_acquire)) {
    return MF_SHARED_STALE_HANDLE;
  }
  if (found->live_contexts == std::numeric_limits<std::uint32_t>::max() ||
      revision_ == std::numeric_limits<std::uint64_t>::max()) {
    return MF_SHARED_RESOURCE_EXHAUSTED;
  }
  ++found->live_contexts;
  advance_revision_locked();
  return MF_SHARED_SUCCESS;
}

mf_shared_status_v1 ProcessAuthority::context_release(std::uint64_t session_id) noexcept {
  const std::scoped_lock lock(mutex_);
  const auto found = std::find_if(sessions_.begin(), sessions_.end(),
                                  [&](const auto& row) { return row.session_id == session_id; });
  if (found == sessions_.end() || !found->admission->admitted.load(std::memory_order_acquire)) {
    return MF_SHARED_STALE_HANDLE;
  }
  if (found->live_contexts == 0U) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  if (revision_ == std::numeric_limits<std::uint64_t>::max()) {
    return MF_SHARED_RESOURCE_EXHAUSTED;
  }
  --found->live_contexts;
  advance_revision_locked();
  return MF_SHARED_SUCCESS;
}

void ProcessAuthority::prune_dead_locked() noexcept {
  bool changed = false;
  for (auto& row : sessions_) {
    const std::optional<std::uint64_t> current = process_start_time(row.pid);
    if (current.has_value() && *current == row.start_time_ticks) {
      continue;
    }
    changed = row.admission->tombstone() || changed;
  }
  if (changed) {
    advance_revision_locked();
  }
}

mf_shared_status_v1 ProcessAuthority::snapshot(ProcessSnapshotPayload& out_payload) noexcept {
  struct Aggregate final {
    std::uint32_t pid = 0;
    std::uint64_t start_time_ticks = 0;
    std::uint64_t used_memory_bytes = 0;
    bool has_live_context = false;
    std::uint32_t name_length = 0;
    std::array<std::uint8_t, MF_CLIENT_PROCESS_NAME_SIZE_V1> name{};
  };
  std::vector<Aggregate> aggregates;
  std::uint64_t revision = 0;
  try {
    const std::scoped_lock lock(mutex_);
    prune_dead_locked();
    for (const auto& session : sessions_) {
      if (!session.admission->admitted.load(std::memory_order_acquire)) {
        continue;
      }
      auto found = std::find_if(aggregates.begin(), aggregates.end(), [&](const auto& row) {
        return row.pid == session.pid && row.start_time_ticks == session.start_time_ticks;
      });
      if (found == aggregates.end()) {
        aggregates.push_back(Aggregate{session.pid, session.start_time_ticks,
                                       session.used_memory_bytes, session.live_contexts != 0U,
                                       session.name_length, session.name});
      } else {
        if (session.used_memory_bytes >
            std::numeric_limits<std::uint64_t>::max() - found->used_memory_bytes) {
          return MF_SHARED_OVERFLOW;
        }
        found->used_memory_bytes += session.used_memory_bytes;
        found->has_live_context = found->has_live_context || session.live_contexts != 0U;
      }
    }
    std::erase_if(aggregates, [](const auto& row) { return !row.has_live_context; });
    if (aggregates.size() > MF_CLIENT_PROCESS_SNAPSHOT_CAPACITY_V1) {
      return MF_SHARED_RESOURCE_EXHAUSTED;
    }
    revision = revision_;
  } catch (const std::bad_alloc&) {
    return MF_SHARED_RESOURCE_EXHAUSTED;
  }
  std::sort(aggregates.begin(), aggregates.end(),
            [](const auto& left, const auto& right) { return left.pid < right.pid; });
  std::uint64_t byte_count = 0;
  if (mf_client_process_snapshot_size_v1(static_cast<std::uint32_t>(aggregates.size()),
                                         &byte_count) != MF_CLIENT_CONTROL_OK ||
      byte_count > static_cast<std::uint64_t>(std::numeric_limits<off_t>::max())) {
    return MF_SHARED_OVERFLOW;
  }
  const long created =
      syscall(SYS_memfd_create, "metaflux-process-snapshot-v1", MFD_CLOEXEC | MFD_ALLOW_SEALING);
  if (created < 0 || created > std::numeric_limits<int>::max()) {
    return MF_SHARED_SYSTEM_ERROR;
  }
  UniqueFd fd(static_cast<int>(created));
  if (ftruncate(fd.get(), static_cast<off_t>(byte_count)) != 0) {
    return MF_SHARED_SYSTEM_ERROR;
  }
  void* mapping = mmap(nullptr, static_cast<std::size_t>(byte_count), PROT_READ | PROT_WRITE,
                       MAP_SHARED, fd.get(), 0);
  if (mapping == MAP_FAILED) {
    return MF_SHARED_SYSTEM_ERROR;
  }
  auto* bytes = static_cast<std::uint8_t*>(mapping);
  const std::uint64_t generation = registry_authority_->device_generation();
  if (generation == 0U) {
    (void)munmap(mapping, static_cast<std::size_t>(byte_count));
    return MF_SHARED_DEVICE_LOST;
  }
  mf_client_process_snapshot_header_init_v1(
      reinterpret_cast<mf_client_process_snapshot_header_v1*>(bytes), revision,
      static_cast<std::uint32_t>(aggregates.size()));
  for (std::uint32_t index = 0; index < aggregates.size(); ++index) {
    const Aggregate& source = aggregates[index];
    mf_client_process_snapshot_row_init_v1(
        mf_client_process_snapshot_mutable_row_v1_at(bytes, index), source.pid,
        MF_CLIENT_PROCESS_KIND_COMPUTE_V1, source.start_time_ticks, kIdentityRecordId,
        generation, source.used_memory_bytes, source.name.data(), source.name_length);
  }
  const bool valid =
      mf_client_process_snapshot_validate_v1(bytes, byte_count) == MF_CLIENT_CONTROL_OK;
  if (munmap(mapping, static_cast<std::size_t>(byte_count)) != 0 || !valid ||
      fcntl(fd.get(), F_ADD_SEALS, F_SEAL_GROW | F_SEAL_SHRINK | F_SEAL_WRITE | F_SEAL_SEAL) != 0) {
    return MF_SHARED_SYSTEM_ERROR;
  }
  out_payload.fd = std::move(fd);
  out_payload.revision = revision;
  out_payload.size = byte_count;
  return MF_SHARED_SUCCESS;
}

enum class ObjectKind : std::uint32_t {
  kDeviceMemory = MF_OBJECT_TYPE_DEVICE_MEMORY,
  kHostMemory = MF_OBJECT_TYPE_HOST_MEMORY,
  kArtifact = MF_OBJECT_TYPE_ARTIFACT,
  kArgumentBlock = MF_OBJECT_TYPE_ARGUMENT_BLOCK,
  kModule = MF_OBJECT_TYPE_MODULE,
};

struct ArtifactSourceRange final {
  std::uint64_t offset = 0U;
  std::uint64_t size = 0U;
};

struct Object final {
  std::uint64_t id = 0;
  std::uint64_t generation = 0;
  ObjectKind kind = ObjectKind::kDeviceMemory;
  bool alive = false;
  std::uint32_t access_flags = 0;
  ArtifactSourceRange artifact_source{};
  std::vector<std::uint8_t> owned_bytes;
  PayloadMapping mapped_bytes;
  std::unique_ptr<PreparedModule> prepared_module;
  std::unique_ptr<VulkanKernelModule> vulkan_module;
  uint32_t kernel_operation = 0U;
#if METAFLUX_DAEMON_CDEV_BACKEND
  mf_backend_memory_v1 cdev_backend_memory = 0U;
  mf_backend_module_v1 cdev_backend_module = 0U;
#endif

  [[nodiscard]] std::uint64_t byte_size() const noexcept {
    return mapped_bytes.size() != 0U ? mapped_bytes.size()
                                     : static_cast<std::uint64_t>(owned_bytes.size());
  }
  [[nodiscard]] const std::uint8_t* data() const noexcept {
    return mapped_bytes.size() != 0U ? mapped_bytes.data() : owned_bytes.data();
  }
  [[nodiscard]] std::uint8_t* mutable_data() noexcept {
    return mapped_bytes.size() != 0U ? mapped_bytes.mutable_data() : owned_bytes.data();
  }
};

struct ControlReply final {
  mf_client_control_response_v1 response{};
  int payload_fd = -1;
  UniqueFd owned_payload;
};

struct CopyPathStatistics final {
  std::atomic<std::uint64_t> address_space_registrations{0U};
  std::atomic<std::uint64_t> direct_host_source_operations{0U};
  std::atomic<std::uint64_t> direct_host_source_bytes{0U};
  std::atomic<std::uint64_t> direct_host_destination_operations{0U};
  std::atomic<std::uint64_t> direct_host_destination_bytes{0U};
  std::atomic<std::uint64_t> staged_host_source_operations{0U};
  std::atomic<std::uint64_t> staged_host_source_bytes{0U};
  std::atomic<std::uint64_t> staged_host_destination_operations{0U};
  std::atomic<std::uint64_t> staged_host_destination_bytes{0U};
};

class Session;

class DataPlaneWorker {
public:
  virtual ~DataPlaneWorker() = default;
  [[nodiscard]] virtual mf_shared_status_v1 pump_once() noexcept = 0;
};

class EmbeddedCpuWorker final : public DataPlaneWorker {
public:
  explicit EmbeddedCpuWorker(Session& session) noexcept : session_(session) {}
  [[nodiscard]] mf_shared_status_v1 pump_once() noexcept override;

private:
  Session& session_;
};

#if METAFLUX_DAEMON_CDEV_BACKEND
class CdevDataPlaneWorker final : public DataPlaneWorker {
public:
  explicit CdevDataPlaneWorker(Session& session) noexcept : session_(session) {}
  [[nodiscard]] mf_shared_status_v1 pump_once() noexcept override;

private:
  Session& session_;
};
#endif

class Session final {
public:
  Session(UniqueFd peer, ucred credentials, std::shared_ptr<CpuExecutionEngine> execution,
          std::shared_ptr<RegistryAuthority> registry_authority,
          std::shared_ptr<ProcessAuthority> process_authority,
          std::shared_ptr<ResourceAuthority> resource_authority,
          std::shared_ptr<CopyPathStatistics> copy_path_statistics, SessionLease session_lease,
          bool observer) noexcept
      : peer_(std::move(peer)), credentials_(credentials), view_id_(registry_authority->view_id()),
        execution_(std::move(execution)), registry_authority_(std::move(registry_authority)),
        process_authority_(std::move(process_authority)),
        resource_authority_(std::move(resource_authority)),
        copy_path_statistics_(std::move(copy_path_statistics)),
        session_lease_(std::move(session_lease)), observer_(observer) {
    submission_.owned_fd = -1;
    completion_.owned_fd = -1;
  }
  ~Session();

  Session(const Session&) = delete;
  Session& operator=(const Session&) = delete;

  [[nodiscard]] mf_shared_status_v1 initialize() noexcept;
  void serve(const mf_client_negotiation_request_v1& request, std::stop_token stop_token) noexcept;

private:
  friend class EmbeddedCpuWorker;
#if METAFLUX_DAEMON_CDEV_BACKEND
  friend class CdevDataPlaneWorker;
#endif
  [[nodiscard]] Object* find(std::uint64_t id) noexcept;
  [[nodiscard]] mf_shared_status_v1 resolve(std::uint64_t id, std::uint64_t generation,
                                            ObjectKind kind, Object*& out_object) noexcept;
  [[nodiscard]] mf_shared_status_v1 resolve_memory(std::uint64_t id, std::uint64_t generation,
                                                   bool for_write, Object*& out_object) noexcept;
  [[nodiscard]] mf_shared_status_v1 add_owned_object(ObjectKind kind, std::uint64_t byte_count,
                                                     std::uint64_t& out_id,
                                                     std::uint64_t& out_generation) noexcept;
  [[nodiscard]] mf_shared_status_v1 add_mapped_object(ObjectKind kind, std::uint32_t access_flags,
                                                      PayloadMapping mapping,
                                                      ArtifactSourceRange artifact_source,
                                                      std::uint64_t& out_id,
                                                      std::uint64_t& out_generation) noexcept;
  [[nodiscard]] mf_shared_status_v1 add_module(std::unique_ptr<PreparedModule> module,
                                               std::uint64_t& out_id,
                                               std::uint64_t& out_generation) noexcept;
  [[nodiscard]] mf_shared_status_v1 release_object(std::uint64_t id, std::uint64_t generation,
                                                   ObjectKind kind) noexcept;
  [[nodiscard]] bool handle_control_packet(ReceivedPacket packet) noexcept;
  [[nodiscard]] ControlReply control(const mf_client_control_request_v1& request, UniqueFd payload,
                                     bool process_was_admitted) noexcept;
#if METAFLUX_DAEMON_CDEV_BACKEND
  [[nodiscard]] static mf_shared_status_v1
  cdev_object_lookup(void* context, std::uint64_t object_id, std::uint64_t object_generation,
                     std::uint32_t expected_kind, bool for_write,
                     metaflux::transport::cdev::CdevObjectTableView* out) noexcept;
  [[nodiscard]] static mf_shared_status_v1
  cdev_memory_import(void* context, mf_backend_instance_v1 instance,
                     mf_backend_context_v1 backend_context, void* address, std::uint64_t byte_count,
                     metaflux::transport::cdev::CdevBackendMemoryReference* out) noexcept;
  [[nodiscard]] static mf_shared_status_v1
  cdev_launch_resolve(void* context, const mf_ring_descriptor_v1* request,
                      metaflux::transport::cdev::CdevLaunchResolution* out) noexcept;
  [[nodiscard]] static mf_shared_status_v1 cdev_memory_retain(void* context,
                                                              mf_backend_memory_v1 memory) noexcept;
  static void cdev_memory_release(void* context, mf_backend_memory_v1 memory) noexcept;
  [[nodiscard]] static mf_shared_status_v1 cdev_worker_lease_acquire(void* context) noexcept;
  static void cdev_worker_lease_release(void* context) noexcept;
  [[nodiscard]] static bool cdev_worker_rebind(void* context, std::uint64_t generation,
                                                metaflux::transport::cdev::WorkerQueueView* out_view,
                                                metaflux::transport::cdev::CdevBackendBinding* out_backend) noexcept;
  static void cdev_worker_retire(void* context) noexcept;
  [[nodiscard]] mf_shared_status_v1 initialize_cdev_backend() noexcept;
  [[nodiscard]] mf_shared_status_v1 bind_cdev_worker() noexcept;
  [[nodiscard]] mf_shared_status_v1 report_cdev_loss() noexcept;
  [[nodiscard]] mf_shared_status_v1 ensure_cdev_object_memory(
      Object& object, metaflux::transport::cdev::CdevBackendMemoryReference* out) noexcept;
  void release_cdev_payload_memory() noexcept;
  void retire_cdev_object_memory(Object& object) noexcept;
  void retire_cdev_module(Object& object) noexcept;
  void release_all_cdev_memories() noexcept;
  void destroy_cdev_backend() noexcept;
#endif
#if METAFLUX_DAEMON_CDEV_BACKEND
  [[nodiscard]] mf_shared_status_v1 pump_cdev_once() noexcept;
#endif
  [[nodiscard]] mf_shared_status_v1 pump_once() noexcept;
  [[nodiscard]] mf_shared_status_v1 process_command(const mf_ring_descriptor_v1& command,
                                                    std::uint64_t& result_id,
                                                    std::uint64_t& result_generation,
                                                    std::uint64_t& timeline, std::uint64_t& detail,
                                                    bool& memory_active);
  [[nodiscard]] mf_shared_status_v1 process_launch(const mf_ring_descriptor_v1& command,
                                                   bool& memory_active);
  [[nodiscard]] bool process_admitted() const noexcept {
    return process_registered_ && process_admission_ != nullptr &&
           process_admission_->admitted.load(std::memory_order_acquire);
  }
  [[nodiscard]] bool process_tombstoned() const noexcept {
    return process_registered_ && process_admission_ != nullptr && !process_admitted();
  }
  [[nodiscard]] std::stop_token process_stop_token() const noexcept {
    return process_admission_ == nullptr ? std::stop_token{}
                                         : process_admission_->cancellation.get_token();
  }
  [[nodiscard]] std::uint64_t tombstone_deadline_ns() const noexcept {
    return process_tombstoned()
               ? process_admission_->tombstone_deadline_ns.load(std::memory_order_relaxed)
               : UINT64_C(0);
  }
  [[nodiscard]] bool tombstone_deadline_expired() const noexcept {
    const std::uint64_t deadline = tombstone_deadline_ns();
    return deadline != UINT64_C(0) && monotonic_time_ns() >= deadline;
  }
  [[nodiscard]] int tombstone_poll_timeout_ms() const noexcept {
    const std::uint64_t deadline = tombstone_deadline_ns();
    const std::uint64_t now = monotonic_time_ns();
    if (deadline == UINT64_C(0) || now >= deadline) {
      return 0;
    }
    constexpr std::uint64_t nanoseconds_per_millisecond = UINT64_C(1000000);
    const std::uint64_t remaining = deadline - now;
    const std::uint64_t rounded =
        remaining / nanoseconds_per_millisecond +
        (remaining % nanoseconds_per_millisecond != UINT64_C(0) ? 1U : 0U);
    return static_cast<int>(std::min<std::uint64_t>(rounded, kSessionPollMilliseconds));
  }
  [[nodiscard]] std::uint64_t device_generation() const noexcept {
    return registry_authority_->device_generation();
  }

  UniqueFd peer_;
  ucred credentials_{};
  mf_registry_view_id_v1 view_id_{};
  std::shared_ptr<CpuExecutionEngine> execution_;
  std::shared_ptr<VulkanExecutionRoute> vulkan_route_;
#if METAFLUX_DAEMON_CDEV_BACKEND
  const mf_backend_api_v1* cdev_backend_api_ = nullptr;
  mf_backend_instance_v1 cdev_backend_instance_ = 0U;
  mf_backend_context_v1 cdev_backend_context_ = 0U;
  mf_backend_queue_v1 cdev_backend_queue_ = 0U;
  metaflux::transport::cdev::CdevWorkerSession cdev_worker_session_;
  metaflux::transport::cdev::CdevObjectTableResolver cdev_resolver_;
  std::unique_ptr<metaflux::transport::cdev::CdevWorker> cdev_worker_;
  mf_backend_memory_v1 cdev_payload_backend_memory_ = 0U;
  bool cdev_lifecycle_attached_ = false;
#endif
  std::shared_ptr<RegistryAuthority> registry_authority_;
  std::shared_ptr<ProcessAuthority> process_authority_;
  std::shared_ptr<ResourceAuthority> resource_authority_;
  std::shared_ptr<CopyPathStatistics> copy_path_statistics_;
  SessionLease session_lease_;
  bool process_registered_ = false;
  bool observer_ = false;
  bool terminate_after_response_ = false;
  std::uint64_t negotiated_capabilities_ = 0;
  UniqueFd host_address_space_;
  mf_client_ring_v1 submission_{};
  mf_client_ring_v1 completion_{};
  std::vector<std::unique_ptr<Object>> objects_;
  std::optional<mf_ring_descriptor_v1> pending_completion_;
  std::optional<ReceivedPacket> deferred_control_;
  std::uint64_t next_object_id_ = kFirstObjectId;
  std::uint64_t event_timeline_ = 0;
  std::uint64_t process_session_id_ = 0;
  std::shared_ptr<ProcessAdmission> process_admission_;
#if METAFLUX_DAEMON_CDEV_BACKEND
  // Persistent object entries outlive resolver calls; temporary imports are
  // reclaimed when their last operation reference is released.
  struct CdevMemoryEntry final {
    bool persistent = false;
    bool retired = false;
    std::uint64_t active_references = 0U;
    metaflux::transport::cdev::CdevRegisteredMemory registered{};
  };
  std::unordered_map<mf_backend_memory_v1, CdevMemoryEntry> cdev_memories_;
#endif
};

Session::~Session() {
  if (process_registered_) {
    process_authority_->unregister_session(process_session_id_);
    process_registered_ = false;
    process_admission_.reset();
  }
#if METAFLUX_DAEMON_CDEV_BACKEND
  if (cdev_worker_ != nullptr && cdev_lifecycle_attached_) {
    (void)registry_authority_->detach_cdev_worker(*cdev_worker_);
    cdev_lifecycle_attached_ = false;
  }
  cdev_worker_.reset();
#endif
  mf_client_ring_close_v1(&completion_);
  mf_client_ring_close_v1(&submission_);
  for (const auto& object : objects_) {
    if (object->alive && object->mapped_bytes.size() != 0U) {
      resource_authority_->release_mapping(static_cast<std::uint32_t>(credentials_.uid),
                                           object->mapped_bytes.size());
    }
  }
#if METAFLUX_DAEMON_CDEV_BACKEND
  for (const auto& object : objects_) {
    if (object->cdev_backend_memory != 0U) {
      retire_cdev_object_memory(*object);
    }
  }
  for (const auto& object : objects_) {
    retire_cdev_module(*object);
  }
  release_cdev_payload_memory();
  destroy_cdev_backend();
  cdev_worker_session_.close();
#endif
  objects_.clear();
}

#if METAFLUX_DAEMON_CDEV_BACKEND
mf_shared_status_v1
Session::cdev_object_lookup(void* context, std::uint64_t object_id, std::uint64_t object_generation,
                            std::uint32_t expected_kind, bool for_write,
                            metaflux::transport::cdev::CdevObjectTableView* out) noexcept {
  if (context == nullptr || out == nullptr || object_id == 0U || object_generation == 0U) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  auto* session = static_cast<Session*>(context);
  Object* object = nullptr;
  mf_shared_status_v1 status = MF_SHARED_INVALID_ARGUMENT;
  if (expected_kind == 0U) {
    status = session->resolve_memory(object_id, object_generation, for_write, object);
  } else {
    ObjectKind kind = ObjectKind::kDeviceMemory;
    switch (expected_kind) {
    case MF_OBJECT_TYPE_DEVICE_MEMORY:
      kind = ObjectKind::kDeviceMemory;
      break;
    case MF_OBJECT_TYPE_HOST_MEMORY:
      kind = ObjectKind::kHostMemory;
      break;
    case MF_OBJECT_TYPE_ARTIFACT:
      kind = ObjectKind::kArtifact;
      break;
    case MF_OBJECT_TYPE_ARGUMENT_BLOCK:
      kind = ObjectKind::kArgumentBlock;
      break;
    case MF_OBJECT_TYPE_MODULE:
      kind = ObjectKind::kModule;
      break;
    default:
      return MF_SHARED_INVALID_ARGUMENT;
    }
    status = session->resolve(object_id, object_generation, kind, object);
  }
  if (status != MF_SHARED_SUCCESS || object == nullptr || object->data() == nullptr ||
      object->byte_size() == 0U) {
    return status == MF_SHARED_SUCCESS ? MF_SHARED_MALFORMED : status;
  }
  if (for_write && object->mutable_data() == nullptr) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  *out = {};
  out->object_id = object->id;
  out->object_generation = object->generation;
  out->object_kind = static_cast<std::uint32_t>(object->kind);
  if ((object->access_flags & MF_CLIENT_CONTROL_FLAG_READ) != 0U) {
    out->access_flags |= MF_ARGUMENT_BUFFER_READ;
  }
  if ((object->access_flags & MF_CLIENT_CONTROL_FLAG_WRITE) != 0U) {
    out->access_flags |= MF_ARGUMENT_BUFFER_WRITE;
  }
  out->data = object->data();
  out->address = for_write ? object->mutable_data() : const_cast<std::uint8_t*>(object->data());
  out->byte_size = object->byte_size();
  if (object->kind == ObjectKind::kDeviceMemory || object->kind == ObjectKind::kHostMemory) {
    const mf_shared_status_v1 memory_status =
        session->ensure_cdev_object_memory(*object, &out->backend_reference);
    if (memory_status != MF_SHARED_SUCCESS) {
      return memory_status;
    }
  }
  return MF_SHARED_SUCCESS;
}

mf_shared_status_v1
Session::cdev_memory_import(void* context, mf_backend_instance_v1 instance,
                            mf_backend_context_v1 backend_context, void* address,
                            std::uint64_t byte_count,
                            metaflux::transport::cdev::CdevBackendMemoryReference* out) noexcept {
  if (context == nullptr || out == nullptr || address == nullptr || byte_count == 0U) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  auto* session = static_cast<Session*>(context);
  if (session->cdev_backend_api_ == nullptr || instance != session->cdev_backend_instance_ ||
      backend_context != session->cdev_backend_context_) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  metaflux::transport::cdev::CdevRegisteredMemory registered{};
  const mf_shared_status_v1 register_status = session->cdev_worker_session_.register_memory(
      address, byte_count,
      MF_UAPI_MEMORY_REGISTER_FLAG_READ_V0 | MF_UAPI_MEMORY_REGISTER_FLAG_WRITE_V0, registered);
  if (register_status != MF_SHARED_SUCCESS) {
    return register_status;
  }
  mf_backend_memory_v1 handle = 0U;
  const mf_backend_status_v1 status =
      mf_cpu_backend_import_host_memory_v1(instance, backend_context, address, byte_count, &handle);
  if (status != MF_BACKEND_SUCCESS) {
    session->cdev_worker_session_.close_registered_memory(registered);
    return cdev_backend_status(status);
  }
  if (handle == 0U) {
    session->cdev_worker_session_.close_registered_memory(registered);
    return MF_SHARED_SYSTEM_ERROR;
  }
  try {
    const auto [entry, inserted] = session->cdev_memories_.try_emplace(
        handle, CdevMemoryEntry{.persistent = false,
                                 .retired = false,
                                 .active_references = 0U,
                                 .registered = registered});
    if (!inserted || entry->second.persistent) {
      if (session->cdev_backend_api_->free_memory != nullptr) {
        session->cdev_backend_api_->free_memory(instance, handle);
      }
      session->cdev_worker_session_.close_registered_memory(registered);
      return MF_SHARED_SYSTEM_ERROR;
    }
  } catch (const std::bad_alloc&) {
    if (session->cdev_backend_api_->free_memory != nullptr) {
      session->cdev_backend_api_->free_memory(instance, handle);
    }
    session->cdev_worker_session_.close_registered_memory(registered);
    return MF_SHARED_RESOURCE_EXHAUSTED;
  }
  *out = {
      .handle = handle,
      .retain = &Session::cdev_memory_retain,
      .release = &Session::cdev_memory_release,
      .context = session,
  };
  return MF_SHARED_SUCCESS;
}

mf_shared_status_v1 Session::cdev_memory_retain(void* context,
                                                mf_backend_memory_v1 memory) noexcept {
  if (context == nullptr || memory == 0U) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  auto* session = static_cast<Session*>(context);
  if (session->cdev_backend_api_ == nullptr || session->cdev_backend_instance_ == 0U) {
    return MF_SHARED_STALE_HANDLE;
  }
  const auto found = session->cdev_memories_.find(memory);
  if (found == session->cdev_memories_.end() || found->second.retired ||
      found->second.active_references == std::numeric_limits<std::uint64_t>::max()) {
    return MF_SHARED_STALE_HANDLE;
  }
  ++found->second.active_references;
  return MF_SHARED_SUCCESS;
}

void Session::cdev_memory_release(void* context, mf_backend_memory_v1 memory) noexcept {
  if (context == nullptr || memory == 0U) {
    return;
  }
  auto* session = static_cast<Session*>(context);
  const auto found = session->cdev_memories_.find(memory);
  if (found == session->cdev_memories_.end()) {
    return;
  }
  if (found->second.active_references != 0U) {
    --found->second.active_references;
  }
  const bool reclaim = !found->second.persistent ||
                       (found->second.retired && found->second.active_references == 0U);
  if (!reclaim) {
    return;
  }
  if (session->cdev_backend_api_ != nullptr && session->cdev_backend_instance_ != 0U &&
      session->cdev_backend_api_->free_memory != nullptr) {
    session->cdev_backend_api_->free_memory(session->cdev_backend_instance_, memory);
  }
  session->cdev_worker_session_.close_registered_memory(found->second.registered);
  session->cdev_memories_.erase(found);
}

mf_shared_status_v1 Session::cdev_launch_resolve(
    void* context, const mf_ring_descriptor_v1* request,
    metaflux::transport::cdev::CdevLaunchResolution* out) noexcept {
  constexpr std::uint64_t kLaunchPayloadOffset = 0U;
  constexpr std::uint32_t kDefaultBlockSize = 64U;
  auto* session = static_cast<Session*>(context);
  if (session == nullptr || request == nullptr || out == nullptr ||
      request->opcode != MF_RING_OPCODE_LAUNCH || request->flags != 0U ||
      request->target_id != session->device_generation() || request->arguments[0] == 0U ||
      request->arguments[1] == 0U || request->arguments[2] == 0U || request->arguments[3] == 0U) {
    return MF_SHARED_MALFORMED;
  }
  Object* module = nullptr;
  Object* argument_block = nullptr;
  if (session->resolve(request->arguments[0], request->arguments[1], ObjectKind::kModule, module) !=
          MF_SHARED_SUCCESS ||
      session->resolve(request->arguments[2], request->arguments[3], ObjectKind::kArgumentBlock,
                       argument_block) != MF_SHARED_SUCCESS) {
    return MF_SHARED_STALE_HANDLE;
  }
  if (module == nullptr || module->cdev_backend_module == 0U || argument_block == nullptr ||
      argument_block->data() == nullptr || argument_block->byte_size() == 0U) {
    return MF_SHARED_NOT_SUPPORTED;
  }
  const mf_shared_status_v1 argument_status = mf_client_argument_block_validate_v1(
      argument_block->data(), argument_block->byte_size());
  if (argument_status != MF_SHARED_SUCCESS) {
    return argument_status;
  }
  const auto* source_header =
      reinterpret_cast<const mf_argument_block_header_v1*>(argument_block->data());
  if (source_header->entry_count == 0U ||
      source_header->entry_count > MF_CPU_BACKEND_MAX_ARGUMENTS_V1 ||
      source_header->flags == MF_ARGUMENT_BLOCK_FLAG_COPY_REGION_V1 ||
      (source_header->flags & ~MF_ARGUMENT_BLOCK_FLAG_LAUNCH_DIMENSIONS_XY_V1) != 0U) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  const std::uint64_t argument_size =
      sizeof(mf_cpu_backend_argument_block_header_v1) +
      static_cast<std::uint64_t>(source_header->entry_count) *
          sizeof(mf_cpu_backend_argument_v1);
  if (argument_size > session->cdev_worker_session_.payload_mapping_size() ||
      kLaunchPayloadOffset >
          session->cdev_worker_session_.payload_mapping_size() - argument_size ||
      argument_size > SIZE_MAX || session->cdev_worker_session_.payload_mapping() == nullptr) {
    return MF_SHARED_RESOURCE_EXHAUSTED;
  }
  *out = {};
  auto* destination = session->cdev_worker_session_.payload_mapping() + kLaunchPayloadOffset;
  auto* destination_header =
      reinterpret_cast<mf_cpu_backend_argument_block_header_v1*>(destination);
  destination_header->magic = MF_CPU_BACKEND_ARGUMENT_BLOCK_MAGIC_V1;
  destination_header->version = MF_CPU_BACKEND_ARGUMENT_BLOCK_VERSION_V1;
  destination_header->header_size = sizeof(*destination_header);
  destination_header->entry_size = sizeof(mf_cpu_backend_argument_v1);
  destination_header->entry_count = source_header->entry_count;
  destination_header->reserved_word = 0U;
  destination_header->total_size = argument_size;
  std::memset(destination_header->reserved, 0, sizeof(destination_header->reserved));

  const auto* source_entries = reinterpret_cast<const mf_argument_entry_v1*>(
      argument_block->data() + sizeof(mf_argument_block_header_v1));
  auto* destination_entries = reinterpret_cast<mf_cpu_backend_argument_v1*>(
      destination + sizeof(mf_cpu_backend_argument_block_header_v1));
  std::uint32_t launch_count = 1U;
  std::uint32_t memory_reference_count = 0U;
  for (std::uint32_t index = 0U; index < source_header->entry_count; ++index) {
    const mf_argument_entry_v1& source_entry = source_entries[index];
    mf_cpu_backend_argument_v1& destination_entry = destination_entries[index];
    destination_entry = {};
    if (source_entry.kind == MF_ARGUMENT_KIND_U32) {
      if (source_entry.flags != 0U || source_entry.object_id != 0U ||
          source_entry.object_generation != 0U || source_entry.value > UINT32_MAX) {
        return MF_SHARED_INVALID_ARGUMENT;
      }
      destination_entry.kind = MF_CPU_BACKEND_ARGUMENT_KIND_U32_V1;
      destination_entry.value = source_entry.value;
      launch_count = static_cast<std::uint32_t>(source_entry.value);
      continue;
    }
    if (source_entry.kind != MF_ARGUMENT_KIND_BUFFER ||
        (source_entry.flags & ~MF_ARGUMENT_BUFFER_KNOWN_FLAGS) != 0U ||
        (source_entry.flags & MF_ARGUMENT_BUFFER_KNOWN_FLAGS) == 0U ||
        memory_reference_count >=
            metaflux::transport::cdev::kCdevLaunchMemoryReferenceCapacity) {
      return MF_SHARED_INVALID_ARGUMENT;
    }
    const bool writable = (source_entry.flags & MF_ARGUMENT_BUFFER_WRITE) != 0U;
    metaflux::transport::cdev::CdevObjectTableView view{};
    const mf_shared_status_v1 view_status = cdev_object_lookup(
        session, source_entry.object_id, source_entry.object_generation, 0U, writable, &view);
    if (view_status != MF_SHARED_SUCCESS) {
      return view_status;
    }
    if ((source_entry.flags & MF_ARGUMENT_BUFFER_READ) != 0U &&
        view.object_kind == MF_OBJECT_TYPE_HOST_MEMORY &&
        (view.access_flags & MF_CLIENT_CONTROL_FLAG_READ) == 0U) {
      return MF_SHARED_INVALID_ARGUMENT;
    }
    if (view.address == nullptr || source_entry.value >= view.byte_size ||
        (view.byte_size - source_entry.value) % sizeof(std::uint32_t) != 0U ||
        reinterpret_cast<std::uintptr_t>(view.address) >
            std::numeric_limits<std::uintptr_t>::max() - source_entry.value ||
        (reinterpret_cast<std::uintptr_t>(view.address) + source_entry.value) %
                alignof(std::uint32_t) != 0U) {
      return MF_SHARED_INVALID_ARGUMENT;
    }
    const auto& reference = view.backend_reference;
    if (reference.handle == 0U || reference.retain == nullptr || reference.release == nullptr) {
      return MF_SHARED_NOT_SUPPORTED;
    }
    destination_entry.kind = MF_CPU_BACKEND_ARGUMENT_KIND_BUFFER_V1;
    destination_entry.flags = writable ? MF_CPU_BACKEND_ARGUMENT_BUFFER_WRITE_V1
                                       : MF_CPU_BACKEND_ARGUMENT_BUFFER_READ_V1;
    destination_entry.memory = reference.handle;
    destination_entry.offset = source_entry.value;
    destination_entry.byte_count = view.byte_size - source_entry.value;
    out->memory_references[memory_reference_count] = reference;
    ++memory_reference_count;
  }

  out->module = module->cdev_backend_module;
  out->kernel_id = MF_KERNEL_PRIMARY_ENTRY_ID;
  out->argument_offset = kLaunchPayloadOffset;
  out->argument_size = argument_size;
  out->grid[0] = 1U;
  out->grid[1] = 1U;
  out->grid[2] = 1U;
  out->block[0] = kDefaultBlockSize;
  out->block[1] = 1U;
  out->block[2] = 1U;
  if ((source_header->flags & MF_ARGUMENT_BLOCK_FLAG_LAUNCH_DIMENSIONS_XY_V1) != 0U) {
    if (source_header->reserved[MF_ARGUMENT_BLOCK_LAUNCH_GRID_X_INDEX_V1] > UINT32_MAX ||
        source_header->reserved[MF_ARGUMENT_BLOCK_LAUNCH_GRID_Y_INDEX_V1] > UINT32_MAX ||
        source_header->reserved[MF_ARGUMENT_BLOCK_LAUNCH_BLOCK_X_INDEX_V1] > UINT32_MAX ||
        source_header->reserved[MF_ARGUMENT_BLOCK_LAUNCH_BLOCK_Y_INDEX_V1] > UINT32_MAX) {
      return MF_SHARED_INVALID_ARGUMENT;
    }
    out->grid[0] = static_cast<std::uint32_t>(
        source_header->reserved[MF_ARGUMENT_BLOCK_LAUNCH_GRID_X_INDEX_V1]);
    out->grid[1] = static_cast<std::uint32_t>(
        source_header->reserved[MF_ARGUMENT_BLOCK_LAUNCH_GRID_Y_INDEX_V1]);
    out->block[0] = static_cast<std::uint32_t>(
        source_header->reserved[MF_ARGUMENT_BLOCK_LAUNCH_BLOCK_X_INDEX_V1]);
    out->block[1] = static_cast<std::uint32_t>(
        source_header->reserved[MF_ARGUMENT_BLOCK_LAUNCH_BLOCK_Y_INDEX_V1]);
  } else {
    out->grid[0] = static_cast<std::uint32_t>(std::max<std::uint64_t>(
        1U, (static_cast<std::uint64_t>(launch_count) + kDefaultBlockSize - 1U) /
                kDefaultBlockSize));
  }
  if (out->grid[0] == 0U || out->grid[1] == 0U || out->block[0] == 0U || out->block[1] == 0U) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  out->memory_reference_count = memory_reference_count;
  return MF_SHARED_SUCCESS;
}

mf_shared_status_v1 Session::cdev_worker_lease_acquire(void* context) noexcept {
  auto* session = static_cast<Session*>(context);
  return session != nullptr && session->cdev_worker_ != nullptr ? MF_SHARED_SUCCESS
                                                                  : MF_SHARED_STALE_HANDLE;
}

void Session::cdev_worker_lease_release(void* context) noexcept { (void)context; }

bool Session::cdev_worker_rebind(void* context, std::uint64_t generation,
                                  metaflux::transport::cdev::WorkerQueueView* out_view,
                                  metaflux::transport::cdev::CdevBackendBinding* out_backend) noexcept {
  auto* session = static_cast<Session*>(context);
  if (session == nullptr || out_view == nullptr || out_backend == nullptr || generation == 0U) {
    return false;
  }
  // Re-open the live kernel cdev lease. The static module still owns a single
  // generation; discover its current view rather than requiring the daemon
  // registry incarnation to match.
  metaflux::transport::cdev::CdevWorkerSession new_session{};
  const mf_shared_status_v1 open_status =
      metaflux::transport::cdev::CdevWorkerSession::open_current(nullptr, new_session);
  if (open_status != MF_SHARED_SUCCESS) {
    return false;
  }
  (void)generation;
  const mf_shared_status_v1 map_status = new_session.ensure_payload(64U * 1024U);
  if (map_status != MF_SHARED_SUCCESS) {
    new_session.close();
    return false;
  }
  // Import payload into the backend.
  mf_backend_memory_v1 payload_memory = 0U;
  const mf_backend_status_v1 import_status = mf_cpu_backend_import_host_memory_v1(
      session->cdev_backend_instance_, session->cdev_backend_context_,
      new_session.payload_mapping(), new_session.payload_mapping_size(), &payload_memory);
  if (import_status != MF_BACKEND_SUCCESS || payload_memory == 0U) {
    new_session.close();
    return false;
  }
  // Swap the new session into the persistent field so out_view pointers remain valid.
  session->cdev_worker_session_ = std::move(new_session);
  // Track the new payload memory in the ownership map.
  try {
    const auto [entry, inserted] = session->cdev_memories_.try_emplace(
        payload_memory, CdevMemoryEntry{.persistent = true, .retired = false, .active_references = 0U});
    if (!inserted || !entry->second.persistent) {
      if (session->cdev_backend_api_->free_memory != nullptr) {
        session->cdev_backend_api_->free_memory(session->cdev_backend_instance_, payload_memory);
      }
      return false;
    }
  } catch (const std::bad_alloc&) {
    if (session->cdev_backend_api_->free_memory != nullptr) {
      session->cdev_backend_api_->free_memory(session->cdev_backend_instance_, payload_memory);
    }
    return false;
  }
  session->cdev_payload_backend_memory_ = payload_memory;
  // Populate the complete backend binding (matching bind_cdev_worker).
  *out_view = session->cdev_worker_session_.queue_view();
  *out_backend = {};
  out_backend->api = session->cdev_backend_api_;
  out_backend->instance = session->cdev_backend_instance_;
  out_backend->queue = session->cdev_backend_queue_;
  out_backend->memory = payload_memory;
  out_backend->memory_reference = {
      .handle = payload_memory,
      .retain = &Session::cdev_memory_retain,
      .release = &Session::cdev_memory_release,
      .context = session,
  };
  out_backend->copy_resolver = &metaflux::transport::cdev::CdevObjectTableResolver::callback;
  out_backend->copy_context = &session->cdev_resolver_;
  out_backend->launch_resolver = &Session::cdev_launch_resolve;
  out_backend->launch_context = session;
  out_backend->lease_acquire = &Session::cdev_worker_lease_acquire;
  out_backend->lease_release = &Session::cdev_worker_lease_release;
  out_backend->lease_context = session;
  out_backend->rebind = &Session::cdev_worker_rebind;
  out_backend->rebind_context = session;
  out_backend->retire = &Session::cdev_worker_retire;
  out_backend->retire_context = session;
  out_backend->generation = session->cdev_worker_session_.lease().generation;
  // Clear stale generation-bound object memory handles; the resolver will
  // re-import them through the new session on next use.
  for (const auto& object : session->objects_) {
    if (object != nullptr && object->cdev_backend_memory != 0U) {
      object->cdev_backend_memory = 0U;
    }
  }
  session->cdev_resolver_.configure(session, &Session::cdev_object_lookup, session,
                                    &Session::cdev_memory_import, session->cdev_backend_instance_,
                                    session->cdev_backend_context_);
  // Retire and release the old payload memory.
  const mf_backend_memory_v1 old_payload = session->cdev_payload_backend_memory_;
  if (old_payload != 0U && old_payload != payload_memory) {
    const auto found = session->cdev_memories_.find(old_payload);
    if (found != session->cdev_memories_.end()) {
      found->second.retired = true;
    }
    session->cdev_memory_release(session, old_payload);
  }
  return true;
}

void Session::cdev_worker_retire(void* context) noexcept {
  auto* session = static_cast<Session*>(context);
  if (session == nullptr) {
    return;
  }
  // Release the retired payload memory.
  session->release_cdev_payload_memory();
}

mf_shared_status_v1 Session::initialize_cdev_backend() noexcept {
  if (cdev_backend_api_ != nullptr) {
    return MF_SHARED_SUCCESS;
  }
  const auto* api = mf_cpu_backend_get_api_v1();
  constexpr std::uint32_t required_copy_size = static_cast<std::uint32_t>(
      offsetof(mf_backend_api_v1, copy) + sizeof(((mf_backend_api_v1*)nullptr)->copy));
  constexpr std::uint32_t required_launch_size = static_cast<std::uint32_t>(
      offsetof(mf_backend_api_v1, submit) + sizeof(((mf_backend_api_v1*)nullptr)->submit));
  if (api == nullptr ||
      mf_backend_api_validate_v1(api, required_copy_size, MF_BACKEND_CAP_COPY) !=
          MF_BACKEND_SUCCESS ||
      mf_backend_api_validate_v1(api, required_launch_size, MF_BACKEND_CAP_LAUNCH) !=
          MF_BACKEND_SUCCESS ||
      api->create_instance == nullptr || api->destroy_instance == nullptr ||
      api->create_context == nullptr || api->destroy_context == nullptr ||
      api->create_queue == nullptr || api->destroy_queue == nullptr || api->copy == nullptr ||
      api->load_module == nullptr || api->unload_module == nullptr || api->submit == nullptr) {
    return MF_SHARED_NOT_SUPPORTED;
  }
  mf_backend_instance_v1 instance = 0U;
  mf_backend_context_v1 backend_context = 0U;
  mf_backend_queue_v1 queue = 0U;
  mf_backend_status_v1 status = api->create_instance(nullptr, &instance);
  if (status != MF_BACKEND_SUCCESS) {
    return cdev_backend_status(status);
  }
  if (instance == 0U) {
    return MF_SHARED_SYSTEM_ERROR;
  }
  status = api->create_context(instance, 0U, &backend_context);
  if (status != MF_BACKEND_SUCCESS) {
    api->destroy_instance(instance);
    return cdev_backend_status(status);
  }
  if (backend_context == 0U) {
    api->destroy_instance(instance);
    return MF_SHARED_SYSTEM_ERROR;
  }
  status = api->create_queue(instance, backend_context, &queue);
  if (status != MF_BACKEND_SUCCESS) {
    api->destroy_context(instance, backend_context);
    api->destroy_instance(instance);
    return cdev_backend_status(status);
  }
  if (queue == 0U) {
    api->destroy_context(instance, backend_context);
    api->destroy_instance(instance);
    return MF_SHARED_SYSTEM_ERROR;
  }
  cdev_backend_api_ = api;
  cdev_backend_instance_ = instance;
  cdev_backend_context_ = backend_context;
  cdev_backend_queue_ = queue;
  return MF_SHARED_SUCCESS;
}

mf_shared_status_v1 Session::bind_cdev_worker() noexcept {
  if (cdev_worker_ != nullptr) {
    return MF_SHARED_SUCCESS;
  }
  const std::uint64_t generation = device_generation();
  if (generation == 0U) {
    return MF_SHARED_DEVICE_LOST;
  }
  mf_shared_status_v1 status = initialize_cdev_backend();
  if (status != MF_SHARED_SUCCESS) {
    return status;
  }
  // The static kernel cdev publishes a fixed view/generation independent of the
  // daemon's random incarnation. Discover the live lease instead of requiring
  // the daemon registry view to match the kernel fixture.
  status = metaflux::transport::cdev::CdevWorkerSession::open_current(nullptr,
                                                                      cdev_worker_session_);
  if (status != MF_SHARED_SUCCESS) {
    return status;
  }
  status = cdev_worker_session_.ensure_payload(64U * 1024U);
  if (status != MF_SHARED_SUCCESS) {
    cdev_worker_session_.close();
    return status;
  }

  mf_backend_memory_v1 payload_memory = 0U;
  const mf_backend_status_v1 import_status = mf_cpu_backend_import_host_memory_v1(
      cdev_backend_instance_, cdev_backend_context_, cdev_worker_session_.payload_mapping(),
      cdev_worker_session_.payload_mapping_size(), &payload_memory);
  if (import_status != MF_BACKEND_SUCCESS || payload_memory == 0U) {
    cdev_worker_session_.close();
    return import_status == MF_BACKEND_SUCCESS ? MF_SHARED_SYSTEM_ERROR
                                               : cdev_backend_status(import_status);
  }
  try {
    const auto [entry, inserted] = cdev_memories_.try_emplace(
        payload_memory, CdevMemoryEntry{.persistent = true, .retired = false, .active_references = 0U});
    if (!inserted || !entry->second.persistent) {
      if (cdev_backend_api_->free_memory != nullptr) {
        cdev_backend_api_->free_memory(cdev_backend_instance_, payload_memory);
      }
      cdev_worker_session_.close();
      return MF_SHARED_SYSTEM_ERROR;
    }
  } catch (const std::bad_alloc&) {
    if (cdev_backend_api_->free_memory != nullptr) {
      cdev_backend_api_->free_memory(cdev_backend_instance_, payload_memory);
    }
    cdev_worker_session_.close();
    return MF_SHARED_RESOURCE_EXHAUSTED;
  }
  cdev_payload_backend_memory_ = payload_memory;
  cdev_resolver_.configure(this, &Session::cdev_object_lookup, this,
                           &Session::cdev_memory_import, cdev_backend_instance_,
                           cdev_backend_context_);

  metaflux::transport::cdev::CdevBackendBinding binding{};
  binding.api = cdev_backend_api_;
  binding.instance = cdev_backend_instance_;
  binding.queue = cdev_backend_queue_;
  binding.memory = cdev_payload_backend_memory_;
  binding.memory_reference = {
      .handle = cdev_payload_backend_memory_,
      .retain = &Session::cdev_memory_retain,
      .release = &Session::cdev_memory_release,
      .context = this,
  };
  binding.copy_resolver = &metaflux::transport::cdev::CdevObjectTableResolver::callback;
  binding.copy_context = &cdev_resolver_;
  binding.launch_resolver = &Session::cdev_launch_resolve;
  binding.launch_context = this;
  binding.lease_acquire = &Session::cdev_worker_lease_acquire;
  binding.lease_release = &Session::cdev_worker_lease_release;
  binding.lease_context = this;
  binding.rebind = &Session::cdev_worker_rebind;
  binding.rebind_context = this;
  binding.retire = &Session::cdev_worker_retire;
  binding.retire_context = this;
  binding.generation = cdev_worker_session_.lease().generation;
  try {
    cdev_worker_ = std::make_unique<metaflux::transport::cdev::CdevWorker>(
        cdev_worker_session_.queue_view(), binding);
  } catch (const std::bad_alloc&) {
    release_cdev_payload_memory();
    cdev_worker_session_.close();
    return MF_SHARED_RESOURCE_EXHAUSTED;
  }
  if (!cdev_worker_->backend_bound()) {
    cdev_worker_.reset();
    release_cdev_payload_memory();
    cdev_worker_session_.close();
    return MF_SHARED_NOT_SUPPORTED;
  }
  if (!registry_authority_->attach_cdev_worker(*cdev_worker_)) {
    cdev_worker_.reset();
    release_cdev_payload_memory();
    cdev_worker_session_.close();
    return MF_SHARED_WOULD_BLOCK;
  }
  cdev_lifecycle_attached_ = true;
  return MF_SHARED_SUCCESS;
}

mf_shared_status_v1 Session::report_cdev_loss() noexcept {
  return registry_authority_->report_cdev_loss();
}

void Session::release_cdev_payload_memory() noexcept {
  const mf_backend_memory_v1 handle = cdev_payload_backend_memory_;
  cdev_payload_backend_memory_ = 0U;
  if (handle == 0U) {
    return;
  }
  const auto found = cdev_memories_.find(handle);
  if (found != cdev_memories_.end()) {
    found->second.retired = true;
  }
  cdev_memory_release(this, handle);
}

mf_shared_status_v1 Session::ensure_cdev_object_memory(
    Object& object, metaflux::transport::cdev::CdevBackendMemoryReference* out) noexcept {
  if (out == nullptr || object.data() == nullptr || object.byte_size() == 0U ||
      cdev_backend_api_ == nullptr || cdev_backend_instance_ == 0U ||
      cdev_backend_context_ == 0U) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  if (object.cdev_backend_memory == 0U) {
    std::uint32_t registration_flags =
        MF_UAPI_MEMORY_REGISTER_FLAG_READ_V0 | MF_UAPI_MEMORY_REGISTER_FLAG_WRITE_V0;
    if (object.kind == ObjectKind::kHostMemory) {
      registration_flags = 0U;
      if ((object.access_flags & MF_CLIENT_CONTROL_FLAG_READ) != 0U) {
        registration_flags |= MF_UAPI_MEMORY_REGISTER_FLAG_READ_V0;
      }
      if ((object.access_flags & MF_CLIENT_CONTROL_FLAG_WRITE) != 0U) {
        registration_flags |= MF_UAPI_MEMORY_REGISTER_FLAG_WRITE_V0;
      }
    }
    metaflux::transport::cdev::CdevRegisteredMemory registered{};
    const mf_shared_status_v1 register_status = cdev_worker_session_.register_memory(
        const_cast<std::uint8_t*>(object.data()), object.byte_size(), registration_flags,
        registered);
    if (register_status != MF_SHARED_SUCCESS) {
      return register_status;
    }
    mf_backend_memory_v1 handle = 0U;
    const mf_backend_status_v1 status = mf_cpu_backend_import_host_memory_v1(
        cdev_backend_instance_, cdev_backend_context_, const_cast<std::uint8_t*>(object.data()),
        object.byte_size(), &handle);
    if (status != MF_BACKEND_SUCCESS) {
      cdev_worker_session_.close_registered_memory(registered);
      return cdev_backend_status(status);
    }
    if (handle == 0U) {
      cdev_worker_session_.close_registered_memory(registered);
      return MF_SHARED_SYSTEM_ERROR;
    }
    try {
      const auto [entry, inserted] = cdev_memories_.try_emplace(
          handle, CdevMemoryEntry{.persistent = true,
                                  .retired = false,
                                  .active_references = 0U,
                                  .registered = registered});
      if (!inserted || !entry->second.persistent) {
        if (cdev_backend_api_->free_memory != nullptr) {
          cdev_backend_api_->free_memory(cdev_backend_instance_, handle);
        }
        cdev_worker_session_.close_registered_memory(registered);
        return MF_SHARED_SYSTEM_ERROR;
      }
      object.cdev_backend_memory = handle;
    } catch (const std::bad_alloc&) {
      if (cdev_backend_api_->free_memory != nullptr) {
        cdev_backend_api_->free_memory(cdev_backend_instance_, handle);
      }
      cdev_worker_session_.close_registered_memory(registered);
      return MF_SHARED_RESOURCE_EXHAUSTED;
    }
  }
  if (cdev_memories_.find(object.cdev_backend_memory) == cdev_memories_.end()) {
    object.cdev_backend_memory = 0U;
    return MF_SHARED_STALE_HANDLE;
  }
  *out = {
      .handle = object.cdev_backend_memory,
      .retain = &Session::cdev_memory_retain,
      .release = &Session::cdev_memory_release,
      .context = this,
  };
  return MF_SHARED_SUCCESS;
}

void Session::retire_cdev_object_memory(Object& object) noexcept {
  const mf_backend_memory_v1 handle = object.cdev_backend_memory;
  object.cdev_backend_memory = 0U;
  if (handle == 0U) {
    return;
  }
  const auto found = cdev_memories_.find(handle);
  if (found == cdev_memories_.end()) {
    return;
  }
  found->second.retired = true;
  if (found->second.active_references != 0U) {
    return;
  }
  cdev_memory_release(this, handle);
}

void Session::retire_cdev_module(Object& object) noexcept {
  const mf_backend_module_v1 module = object.cdev_backend_module;
  object.cdev_backend_module = 0U;
  if (module == 0U || cdev_backend_api_ == nullptr || cdev_backend_instance_ == 0U ||
      cdev_backend_api_->unload_module == nullptr) {
    return;
  }
  cdev_backend_api_->unload_module(cdev_backend_instance_, module);
}

void Session::release_all_cdev_memories() noexcept {
  if (cdev_backend_api_ != nullptr && cdev_backend_api_->free_memory != nullptr) {
    for (auto& [handle, entry] : cdev_memories_) {
      cdev_backend_api_->free_memory(cdev_backend_instance_, handle);
      cdev_worker_session_.close_registered_memory(entry.registered);
    }
  } else {
    for (auto& [handle, entry] : cdev_memories_) {
      (void)handle;
      cdev_worker_session_.close_registered_memory(entry.registered);
    }
  }
  cdev_memories_.clear();
}

void Session::destroy_cdev_backend() noexcept {
  if (cdev_backend_api_ == nullptr) {
    return;
  }
  release_all_cdev_memories();
  if (cdev_backend_queue_ != 0U && cdev_backend_api_->destroy_queue != nullptr) {
    cdev_backend_api_->destroy_queue(cdev_backend_instance_, cdev_backend_queue_);
  }
  if (cdev_backend_context_ != 0U && cdev_backend_api_->destroy_context != nullptr) {
    cdev_backend_api_->destroy_context(cdev_backend_instance_, cdev_backend_context_);
  }
  if (cdev_backend_instance_ != 0U && cdev_backend_api_->destroy_instance != nullptr) {
    cdev_backend_api_->destroy_instance(cdev_backend_instance_);
  }
  cdev_backend_queue_ = 0U;
  cdev_backend_context_ = 0U;
  cdev_backend_instance_ = 0U;
  cdev_backend_api_ = nullptr;
}
#endif

mf_shared_status_v1 Session::initialize() noexcept {
  if (credentials_.pid <= 0 || registry_authority_->borrow_fd() < 0 ||
      !session_lease_.valid_for(static_cast<std::uint32_t>(credentials_.uid))) {
    return MF_SHARED_SYSTEM_ERROR;
  }
#if METAFLUX_DAEMON_CDEV_BACKEND
  if (initialize_cdev_backend() != MF_SHARED_SUCCESS) {
    return MF_SHARED_SYSTEM_ERROR;
  }
#endif
  if (mf_client_ring_create_v1(kRingCapacity, view_id_, MF_CLIENT_SUBMISSION_QUEUE_ID_V1,
                               MF_CLIENT_QUEUE_GENERATION_V1, &submission_) != MF_SHARED_SUCCESS ||
      mf_client_ring_create_v1(kRingCapacity, view_id_, MF_CLIENT_COMPLETION_QUEUE_ID_V1,
                               MF_CLIENT_QUEUE_GENERATION_V1, &completion_) != MF_SHARED_SUCCESS) {
    return MF_SHARED_SYSTEM_ERROR;
  }
  return MF_SHARED_SUCCESS;
}

Object* Session::find(std::uint64_t id) noexcept {
  for (auto& object : objects_) {
    if (object->id == id) {
      return object.get();
    }
  }
  return nullptr;
}

mf_shared_status_v1 Session::resolve(std::uint64_t id, std::uint64_t generation, ObjectKind kind,
                                     Object*& out_object) noexcept {
  Object* object = find(id);
  if (object == nullptr || !object->alive || object->generation != generation) {
    return MF_SHARED_STALE_HANDLE;
  }
  if (object->kind != kind) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  out_object = object;
  return MF_SHARED_SUCCESS;
}

mf_shared_status_v1 Session::resolve_memory(std::uint64_t id, std::uint64_t generation,
                                            bool for_write, Object*& out_object) noexcept {
  Object* object = find(id);
  if (object == nullptr || !object->alive || object->generation != generation) {
    return MF_SHARED_STALE_HANDLE;
  }
  if (object->kind != ObjectKind::kDeviceMemory && object->kind != ObjectKind::kHostMemory) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  if (object->kind == ObjectKind::kHostMemory) {
    const std::uint32_t required =
        for_write ? MF_CLIENT_CONTROL_FLAG_WRITE : MF_CLIENT_CONTROL_FLAG_READ;
    if ((object->access_flags & required) == 0U) {
      return MF_SHARED_INVALID_ARGUMENT;
    }
  }
  out_object = object;
  return MF_SHARED_SUCCESS;
}

mf_shared_status_v1 Session::add_owned_object(ObjectKind kind, std::uint64_t byte_count,
                                              std::uint64_t& out_id,
                                              std::uint64_t& out_generation) noexcept {
  if (byte_count == 0U || byte_count > kMaximumObjectSize ||
      byte_count > std::numeric_limits<std::size_t>::max() ||
      next_object_id_ == std::numeric_limits<std::uint64_t>::max() ||
      objects_.size() >= kMaximumObjects) {
    return MF_SHARED_RESOURCE_EXHAUSTED;
  }
  std::uint64_t allocated = 0;
  for (const auto& object : objects_) {
    if (object->alive && object->kind == ObjectKind::kDeviceMemory) {
      const std::uint64_t size = object->byte_size();
      if (size > kMemoryCapacity - allocated) {
        return MF_SHARED_RESOURCE_EXHAUSTED;
      }
      allocated += size;
    }
  }
  if (kind == ObjectKind::kDeviceMemory && byte_count > kMemoryCapacity - allocated) {
    return MF_SHARED_RESOURCE_EXHAUSTED;
  }
  bool process_accounted = false;
  try {
    auto object = std::make_unique<Object>();
    object->id = next_object_id_;
    object->generation = kObjectGeneration;
    object->kind = kind;
    object->alive = true;
    object->owned_bytes.resize(static_cast<std::size_t>(byte_count));
    if (kind == ObjectKind::kDeviceMemory) {
      const mf_shared_status_v1 account_status =
          process_authority_->allocate(process_session_id_, byte_count);
      if (account_status != MF_SHARED_SUCCESS) {
        return account_status;
      }
      process_accounted = true;
    }
    out_id = object->id;
    out_generation = object->generation;
    objects_.push_back(std::move(object));
    ++next_object_id_;
    return MF_SHARED_SUCCESS;
  } catch (const std::bad_alloc&) {
    if (process_accounted) {
      (void)process_authority_->release(process_session_id_, byte_count);
    }
    return MF_SHARED_RESOURCE_EXHAUSTED;
  }
}

mf_shared_status_v1 Session::add_mapped_object(ObjectKind kind, std::uint32_t access_flags,
                                               PayloadMapping mapping,
                                               ArtifactSourceRange artifact_source,
                                               std::uint64_t& out_id,
                                               std::uint64_t& out_generation) noexcept {
  if (mapping.size() == 0U || next_object_id_ == std::numeric_limits<std::uint64_t>::max()) {
    return MF_SHARED_RESOURCE_EXHAUSTED;
  }
  if ((kind == ObjectKind::kArtifact &&
       (artifact_source.size == 0U || artifact_source.offset > mapping.size() ||
        artifact_source.size > mapping.size() - artifact_source.offset)) ||
      (kind != ObjectKind::kArtifact &&
       (artifact_source.offset != 0U || artifact_source.size != 0U))) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  std::uint64_t mapped_bytes = 0;
  for (const auto& object : objects_) {
    if (object->alive && object->mapped_bytes.size() != 0U) {
      if (object->mapped_bytes.size() > kMaximumMappedBytes - mapped_bytes) {
        return MF_SHARED_RESOURCE_EXHAUSTED;
      }
      mapped_bytes += object->mapped_bytes.size();
    }
  }
  if (objects_.size() >= kMaximumObjects || mapping.size() > kMaximumMappedBytes - mapped_bytes) {
    return MF_SHARED_RESOURCE_EXHAUSTED;
  }
  const std::uint64_t mapping_size = mapping.size();
  const mf_shared_status_v1 reserve_status = resource_authority_->reserve_mapping(
      static_cast<std::uint32_t>(credentials_.uid), mapping_size);
  if (reserve_status != MF_SHARED_SUCCESS) {
    return reserve_status;
  }
  try {
    auto object = std::make_unique<Object>();
    object->id = next_object_id_;
    object->generation = kObjectGeneration;
    object->kind = kind;
    object->alive = true;
    object->access_flags = access_flags;
    object->artifact_source = artifact_source;
    object->mapped_bytes = std::move(mapping);
    out_id = object->id;
    out_generation = object->generation;
    objects_.push_back(std::move(object));
    ++next_object_id_;
    return MF_SHARED_SUCCESS;
  } catch (const std::bad_alloc&) {
    resource_authority_->release_mapping(static_cast<std::uint32_t>(credentials_.uid),
                                         mapping_size);
    return MF_SHARED_RESOURCE_EXHAUSTED;
  }
}

mf_shared_status_v1 Session::add_module(std::unique_ptr<PreparedModule> module,
                                        std::uint64_t& out_id,
                                        std::uint64_t& out_generation) noexcept {
  if (module == nullptr || next_object_id_ == std::numeric_limits<std::uint64_t>::max()) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  if (objects_.size() >= kMaximumObjects) {
    return MF_SHARED_RESOURCE_EXHAUSTED;
  }
  mf_backend_module_v1 cdev_module = 0U;
#if METAFLUX_DAEMON_CDEV_BACKEND
  if (cdev_backend_api_ == nullptr || cdev_backend_api_->load_module == nullptr ||
      cdev_backend_api_->unload_module == nullptr || module->canonical_kernel_ir().empty()) {
    return MF_SHARED_NOT_SUPPORTED;
  }
  const std::string_view canonical_kernel_ir = module->canonical_kernel_ir();
  const mf_backend_status_v1 load_status = cdev_backend_api_->load_module(
      cdev_backend_instance_, 0U,
      reinterpret_cast<const std::uint8_t*>(canonical_kernel_ir.data()),
      static_cast<std::uint64_t>(canonical_kernel_ir.size()), &cdev_module);
  if (load_status != MF_BACKEND_SUCCESS) {
    return cdev_backend_status(load_status);
  }
  if (cdev_module == 0U) {
    return MF_SHARED_SYSTEM_ERROR;
  }
#endif
  try {
    auto object = std::make_unique<Object>();
    object->id = next_object_id_;
    object->generation = kObjectGeneration;
    object->kind = ObjectKind::kModule;
    object->alive = true;
    object->prepared_module = std::move(module);
#if METAFLUX_DAEMON_CDEV_BACKEND
    object->cdev_backend_module = cdev_module;
#endif
    out_id = object->id;
    out_generation = object->generation;
    objects_.push_back(std::move(object));
    ++next_object_id_;
    return MF_SHARED_SUCCESS;
  } catch (const std::bad_alloc&) {
#if METAFLUX_DAEMON_CDEV_BACKEND
    cdev_backend_api_->unload_module(cdev_backend_instance_, cdev_module);
#endif
    return MF_SHARED_RESOURCE_EXHAUSTED;
  }
}

mf_shared_status_v1 Session::release_object(std::uint64_t id, std::uint64_t generation,
                                            ObjectKind kind) noexcept {
  Object* object = nullptr;
  const mf_shared_status_v1 status = resolve(id, generation, kind, object);
  if (status != MF_SHARED_SUCCESS) {
    return status;
  }
  if (kind == ObjectKind::kDeviceMemory) {
    const mf_shared_status_v1 account_status =
        process_authority_->release(process_session_id_, object->byte_size());
    if (account_status != MF_SHARED_SUCCESS) {
      return account_status;
    }
  }
  if (object->mapped_bytes.size() != 0U) {
    resource_authority_->release_mapping(static_cast<std::uint32_t>(credentials_.uid),
                                         object->mapped_bytes.size());
  }
#if METAFLUX_DAEMON_CDEV_BACKEND
  retire_cdev_object_memory(*object);
  retire_cdev_module(*object);
#endif
  std::erase_if(objects_, [id](const auto& candidate) { return candidate->id == id; });
  return MF_SHARED_SUCCESS;
}

[[nodiscard]] std::uint32_t shared_to_control_status(mf_shared_status_v1 status) noexcept {
  switch (status) {
  case MF_SHARED_SUCCESS:
    return MF_CLIENT_CONTROL_OK;
  case MF_SHARED_STALE_HANDLE:
    return MF_CLIENT_CONTROL_STALE_GENERATION;
  case MF_SHARED_INVALID_ARGUMENT:
    return MF_CLIENT_CONTROL_INVALID_ARGUMENT;
  case MF_SHARED_RESOURCE_EXHAUSTED:
    return MF_CLIENT_CONTROL_RESOURCE_EXHAUSTED;
  case MF_SHARED_NOT_SUPPORTED:
    return MF_CLIENT_CONTROL_UNSUPPORTED;
  case MF_SHARED_PERMISSION_DENIED:
    return MF_CLIENT_CONTROL_NO_PERMISSION;
  case MF_SHARED_MALFORMED:
    return MF_CLIENT_CONTROL_MALFORMED;
  default:
    return MF_CLIENT_CONTROL_INTERNAL_ERROR;
  }
}

[[nodiscard]] mf_shared_status_v1 module_preparation_status(ModulePreparationError error) noexcept {
  switch (error) {
  case ModulePreparationError::None:
    return MF_SHARED_SUCCESS;
  case ModulePreparationError::CacheMiss:
  case ModulePreparationError::NotSupported:
    return MF_SHARED_NOT_SUPPORTED;
  case ModulePreparationError::Malformed:
    return MF_SHARED_MALFORMED;
  case ModulePreparationError::ResourceExhausted:
    return MF_SHARED_RESOURCE_EXHAUSTED;
  case ModulePreparationError::System:
    return MF_SHARED_SYSTEM_ERROR;
  }
  return MF_SHARED_SYSTEM_ERROR;
}

ControlReply Session::control(const mf_client_control_request_v1& request, UniqueFd payload,
                              bool process_was_admitted) noexcept {
  const std::uint16_t opcode = mf_client_load_le16_v1(request.bytes + 12);
  const std::uint16_t flags = mf_client_load_le16_v1(request.bytes + 14);
  const std::uint64_t request_id = mf_client_load_le64_v1(request.bytes + 24);
  const mf_registry_view_id_v1 requested_view{
      mf_client_load_le64_v1(request.bytes + 32),
      mf_client_load_le64_v1(request.bytes + 40),
  };
  const std::uint64_t object_id = mf_client_load_le64_v1(request.bytes + 48);
  const std::uint64_t argument = mf_client_load_le64_v1(request.bytes + 56);
  const std::uint64_t generation = device_generation();
  ControlReply reply{};
  std::uint64_t response_id = object_id;
  std::uint64_t response_generation = argument;
  std::uint32_t response_flags = 0;
  mf_shared_status_v1 status = MF_SHARED_SUCCESS;

  if (!observer_ && !process_was_admitted) {
    status = MF_SHARED_STALE_HANDLE;
    terminate_after_response_ = true;
  } else if (mf_registry_view_id_equal_v1(requested_view, view_id_) == 0) {
    status = MF_SHARED_STALE_HANDLE;
  } else if ((observer_ && opcode != MF_CLIENT_CONTROL_PROCESS_SNAPSHOT_V1 &&
              opcode != MF_CLIENT_CONTROL_DEVICE_SET_PERSISTENCE_MODE_V1 &&
              opcode != MF_CLIENT_CONTROL_DEVICE_SET_COMPUTE_MODE_V1) ||
             (!observer_ && (opcode == MF_CLIENT_CONTROL_PROCESS_SNAPSHOT_V1 ||
                             opcode == MF_CLIENT_CONTROL_DEVICE_SET_PERSISTENCE_MODE_V1 ||
                             opcode == MF_CLIENT_CONTROL_DEVICE_SET_COMPUTE_MODE_V1))) {
    status = MF_SHARED_NOT_SUPPORTED;
  } else {
    switch (opcode) {
    case MF_CLIENT_CONTROL_CDEV_BIND_V1:
#if METAFLUX_DAEMON_CDEV_BACKEND
      if ((negotiated_capabilities_ & MF_CLIENT_CAP_CDEV_BINDING_V1) == 0U) {
        status = MF_SHARED_NOT_SUPPORTED;
      } else if (flags != 0U || payload.valid() ||
                 object_id != MF_CLIENT_RUNTIME_CONTEXT_ID_V1 || argument != generation) {
        status = MF_SHARED_INVALID_ARGUMENT;
      } else {
        status = bind_cdev_worker();
      }
#else
      status = MF_SHARED_NOT_SUPPORTED;
#endif
      break;
    case MF_CLIENT_CONTROL_HOST_ADDRESS_SPACE_REGISTER_V1:
      if ((negotiated_capabilities_ & MF_CLIENT_CAP_DIRECT_HOST_COPY_V1) == 0U) {
        status = MF_SHARED_NOT_SUPPORTED;
      } else if (flags != (MF_CLIENT_CONTROL_FLAG_PAYLOAD_FD | MF_CLIENT_CONTROL_FLAG_READ |
                           MF_CLIENT_CONTROL_FLAG_WRITE) ||
                 object_id != MF_CLIENT_RUNTIME_CONTEXT_ID_V1 || argument != 0U ||
                 !payload.valid() || host_address_space_.valid() ||
                 !is_peer_memory_descriptor(payload.get(), credentials_.pid)) {
        status = MF_SHARED_INVALID_ARGUMENT;
      } else {
        host_address_space_ = std::move(payload);
        (void)copy_path_statistics_->address_space_registrations.fetch_add(
            1U, std::memory_order_relaxed);
        response_id = object_id;
        response_generation = kObjectGeneration;
      }
      break;
    case MF_CLIENT_CONTROL_DEVICE_MEMORY_ALLOC_V1:
      if (flags != 0U || payload.valid() || object_id != MF_CLIENT_RUNTIME_CONTEXT_ID_V1 ||
          argument == 0U) {
        status = MF_SHARED_INVALID_ARGUMENT;
      } else {
        status =
            add_owned_object(ObjectKind::kDeviceMemory, argument, response_id, response_generation);
      }
      break;
    case MF_CLIENT_CONTROL_DEVICE_MEMORY_FREE_V1:
      status = flags == 0U && !payload.valid()
                   ? release_object(object_id, argument, ObjectKind::kDeviceMemory)
                   : MF_SHARED_INVALID_ARGUMENT;
      break;
    case MF_CLIENT_CONTROL_HOST_MEMORY_REGISTER_V1: {
      const bool writable = (flags & MF_CLIENT_CONTROL_FLAG_WRITE) != 0U;
      const std::uint16_t access =
          flags & (MF_CLIENT_CONTROL_FLAG_READ | MF_CLIENT_CONTROL_FLAG_WRITE);
      if ((flags & MF_CLIENT_CONTROL_FLAG_PAYLOAD_FD) == 0U || access == 0U ||
          (flags & MF_CLIENT_CONTROL_FLAG_PTX) != 0U ||
          object_id != MF_CLIENT_RUNTIME_CONTEXT_ID_V1 || !payload.valid()) {
        status = MF_SHARED_INVALID_ARGUMENT;
        break;
      }
      PayloadMapping mapping;
      status = PayloadMapping::map(std::move(payload), argument, writable, !writable, mapping);
      if (status == MF_SHARED_SUCCESS) {
        status = add_mapped_object(ObjectKind::kHostMemory, access, std::move(mapping),
                                   ArtifactSourceRange{}, response_id, response_generation);
      }
      break;
    }
    case MF_CLIENT_CONTROL_HOST_MEMORY_RELEASE_V1:
      status = flags == 0U && !payload.valid()
                   ? release_object(object_id, argument, ObjectKind::kHostMemory)
                   : MF_SHARED_INVALID_ARGUMENT;
      break;
    case MF_CLIENT_CONTROL_ARTIFACT_REGISTER_V1: {
      if (flags != (MF_CLIENT_CONTROL_FLAG_PAYLOAD_FD | MF_CLIENT_CONTROL_FLAG_PTX) ||
          object_id != MF_CLIENT_RUNTIME_CONTEXT_ID_V1 || !payload.valid()) {
        status = MF_SHARED_INVALID_ARGUMENT;
        break;
      }
      PayloadMapping mapping;
      status = PayloadMapping::map(std::move(payload), argument, false, true, mapping);
      if (status == MF_SHARED_SUCCESS) {
        const ArtifactSourceRange artifact_source{0U, mapping.size()};
        status = add_mapped_object(ObjectKind::kArtifact, 0U, std::move(mapping), artifact_source,
                                   response_id, response_generation);
      }
      break;
    }
    case MF_CLIENT_CONTROL_KERNEL_REQUEST_REGISTER_V1: {
      if ((negotiated_capabilities_ & MF_CLIENT_CAP_KERNEL_REQUEST_V1) == 0U) {
        status = MF_SHARED_NOT_SUPPORTED;
        break;
      }
      if (flags != MF_CLIENT_CONTROL_FLAG_PAYLOAD_FD ||
          object_id != MF_CLIENT_RUNTIME_CONTEXT_ID_V1 || !payload.valid()) {
        status = MF_SHARED_INVALID_ARGUMENT;
        break;
      }
      PayloadMapping mapping;
      status = PayloadMapping::map(std::move(payload), argument, false, true, mapping);
      if (status == MF_SHARED_SUCCESS &&
          mf_client_kernel_request_validate_v1(mapping.data(), mapping.size()) !=
              MF_CLIENT_CONTROL_OK) {
        status = MF_SHARED_INVALID_ARGUMENT;
      }
      if (status == MF_SHARED_SUCCESS) {
        const auto* kernel_request =
            reinterpret_cast<const mf_client_kernel_request_v1*>(mapping.data());
        const uint32_t kernel_operation =
            mf_client_load_le32_v1(kernel_request->bytes + 20);
        status = add_mapped_object(
            ObjectKind::kArtifact, 0U, std::move(mapping),
            ArtifactSourceRange{mf_client_kernel_request_payload_offset_v1(kernel_request),
                                mf_client_kernel_request_payload_size_v1(kernel_request)},
            response_id, response_generation);
        if (status == MF_SHARED_SUCCESS) {
          Object* stored_artifact = nullptr;
          if (resolve(response_id, response_generation, ObjectKind::kArtifact,
                      stored_artifact) == MF_SHARED_SUCCESS) {
            stored_artifact->kernel_operation = kernel_operation;
          }
        }
      }
      break;
    }
    case MF_CLIENT_CONTROL_ARTIFACT_RESOLVE_V1: {
      Object* artifact = nullptr;
      status = flags == 0U && !payload.valid()
                   ? resolve(object_id, argument, ObjectKind::kArtifact, artifact)
                   : MF_SHARED_INVALID_ARGUMENT;
      if (status == MF_SHARED_SUCCESS) {
        reply.payload_fd = artifact->mapped_bytes.fd();
        response_flags = MF_CLIENT_CONTROL_FLAG_PAYLOAD_FD;
      }
      break;
    }
    case MF_CLIENT_CONTROL_ARTIFACT_RELEASE_V1:
      status = flags == 0U && !payload.valid()
                   ? release_object(object_id, argument, ObjectKind::kArtifact)
                   : MF_SHARED_INVALID_ARGUMENT;
      break;
    case MF_CLIENT_CONTROL_ARGUMENT_BLOCK_REGISTER_V1: {
      if (flags != MF_CLIENT_CONTROL_FLAG_PAYLOAD_FD ||
          object_id != MF_CLIENT_RUNTIME_CONTEXT_ID_V1 || !payload.valid()) {
        status = MF_SHARED_INVALID_ARGUMENT;
        break;
      }
      PayloadMapping mapping;
      status = PayloadMapping::map(std::move(payload), argument, false, true, mapping);
      if (status == MF_SHARED_SUCCESS && mapping.size() >= sizeof(mf_argument_block_header_v1)) {
        const auto* header = reinterpret_cast<const mf_argument_block_header_v1*>(mapping.data());
        if (header->flags == MF_ARGUMENT_BLOCK_FLAG_COPY_REGION_V1 &&
            (negotiated_capabilities_ & MF_CLIENT_CAP_COPY_REGION_V1) == 0U) {
          status = MF_SHARED_NOT_SUPPORTED;
        }
      }
      if (status == MF_SHARED_SUCCESS) {
        status = mf_client_argument_block_validate_v1(mapping.data(), mapping.size());
      }
      if (status == MF_SHARED_SUCCESS) {
        status = add_mapped_object(ObjectKind::kArgumentBlock, 0U, std::move(mapping),
                                   ArtifactSourceRange{}, response_id, response_generation);
      }
      break;
    }
    case MF_CLIENT_CONTROL_ARGUMENT_BLOCK_RELEASE_V1:
      status = flags == 0U && !payload.valid()
                   ? release_object(object_id, argument, ObjectKind::kArgumentBlock)
                   : MF_SHARED_INVALID_ARGUMENT;
      break;
    case MF_CLIENT_CONTROL_CONTEXT_ACQUIRE_V1:
      if ((negotiated_capabilities_ & MF_CLIENT_CAP_LIVE_CONTEXT_ACCOUNTING_V1) == 0U) {
        status = MF_SHARED_NOT_SUPPORTED;
      } else {
        status = flags == 0U && !payload.valid() && process_registered_ &&
                         object_id == MF_CLIENT_RUNTIME_CONTEXT_ID_V1 &&
                         argument == generation
                     ? process_authority_->context_acquire(process_session_id_)
                     : MF_SHARED_INVALID_ARGUMENT;
      }
      break;
    case MF_CLIENT_CONTROL_CONTEXT_RELEASE_V1:
      if ((negotiated_capabilities_ & MF_CLIENT_CAP_LIVE_CONTEXT_ACCOUNTING_V1) == 0U) {
        status = MF_SHARED_NOT_SUPPORTED;
      } else {
        status = flags == 0U && !payload.valid() && process_registered_ &&
                         object_id == MF_CLIENT_RUNTIME_CONTEXT_ID_V1 &&
                         argument == generation
                     ? process_authority_->context_release(process_session_id_)
                     : MF_SHARED_INVALID_ARGUMENT;
      }
      break;
    case MF_CLIENT_CONTROL_PROCESS_SNAPSHOT_V1: {
      ProcessSnapshotPayload snapshot;
      if (flags != 0U || payload.valid() || object_id != MF_CLIENT_PROCESS_SNAPSHOT_VERSION_V1 ||
          argument != MF_CLIENT_PROCESS_SNAPSHOT_CAPACITY_V1) {
        status = MF_SHARED_INVALID_ARGUMENT;
        break;
      }
      status = process_authority_->snapshot(snapshot);
      if (status == MF_SHARED_SUCCESS) {
        response_id = snapshot.revision;
        response_generation = snapshot.size;
        response_flags = MF_CLIENT_CONTROL_FLAG_PAYLOAD_FD;
        reply.owned_payload = std::move(snapshot.fd);
        reply.payload_fd = reply.owned_payload.get();
      }
      break;
    }
    case MF_CLIENT_CONTROL_DEVICE_SET_PERSISTENCE_MODE_V1:
    case MF_CLIENT_CONTROL_DEVICE_SET_COMPUTE_MODE_V1: {
      const bool persistence = opcode == MF_CLIENT_CONTROL_DEVICE_SET_PERSISTENCE_MODE_V1;
      const std::uint64_t maximum_value =
          persistence ? UINT64_C(1) : MF_DEVICE_POLICY_COMPUTE_MODE_EXCLUSIVE_PROCESS_V1;
      if ((negotiated_capabilities_ & MF_CLIENT_CAP_POLICY_SETTERS_V1) == 0U) {
        status = MF_SHARED_NOT_SUPPORTED;
      } else if (flags != 0U || payload.valid() || object_id != kIdentityRecordId ||
                 argument > maximum_value) {
        status = MF_SHARED_INVALID_ARGUMENT;
      } else if (credentials_.uid != 0U && credentials_.uid != static_cast<uid_t>(geteuid())) {
        status = MF_SHARED_PERMISSION_DENIED;
      } else {
        const std::uint64_t policy_mask = persistence ? MF_DEVICE_POLICY_PERSISTENCE_ENABLED_V1
                                                      : MF_DEVICE_POLICY_COMPUTE_MODE_MASK_V1;
        const std::uint64_t policy_value =
            persistence ? argument : argument << MF_DEVICE_POLICY_COMPUTE_MODE_SHIFT_V1;
        status = registry_authority_->update_policy(object_id, policy_mask, policy_value,
                                                    response_generation);
      }
      break;
    }
    default:
      status = MF_SHARED_NOT_SUPPORTED;
      break;
    }
  }

  if (!observer_ && !process_admitted()) {
    status = MF_SHARED_STALE_HANDLE;
    response_flags = 0U;
    reply.payload_fd = -1;
    terminate_after_response_ = true;
  }

  mf_client_control_response_init_v1(&reply.response, shared_to_control_status(status),
                                     response_flags, request_id, view_id_.daemon_incarnation,
                                     view_id_.view_serial,
                                     status == MF_SHARED_SUCCESS ? response_id : 0U,
                                     status == MF_SHARED_SUCCESS ? response_generation : 0U);
  return reply;
}

bool Session::handle_control_packet(ReceivedPacket packet) noexcept {
  mf_client_control_request_v1 request{};
  std::memcpy(request.bytes, packet.bytes.data(), sizeof(request.bytes));
  if (mf_client_control_request_validate_v1(&request) != MF_CLIENT_CONTROL_OK) {
    return false;
  }
  const std::uint16_t flags = mf_client_load_le16_v1(request.bytes + 14);
  const bool expects_payload = (flags & MF_CLIENT_CONTROL_FLAG_PAYLOAD_FD) != 0U;
  if (packet.descriptor_count != (expects_payload ? 1U : 0U)) {
    return false;
  }
  const bool process_was_admitted = observer_ || process_admitted();
  if (!observer_ && !process_was_admitted && pending_completion_.has_value()) {
    if (deferred_control_.has_value()) {
      return false;
    }
    deferred_control_.emplace(std::move(packet));
    return true;
  }
  UniqueFd payload;
  if (expects_payload) {
    payload = std::move(packet.descriptors[0]);
  }
  const ControlReply reply = control(request, std::move(payload), process_was_admitted);
  const std::array<int, 1> response_fd{reply.payload_fd};
  const bool sent = send_packet(peer_.get(), reply.response.bytes,
                                reply.payload_fd >= 0 ? std::span<const int>(response_fd)
                                                      : std::span<const int>{});
  return sent && !terminate_after_response_;
}

mf_shared_status_v1 Session::process_launch(const mf_ring_descriptor_v1& command,
                                            bool& memory_active) {
  Object* module = nullptr;
  Object* argument_block = nullptr;
  if (resolve(command.target_id, command.arguments[0], ObjectKind::kModule, module) !=
          MF_SHARED_SUCCESS ||
      resolve(command.arguments[2], command.arguments[3], ObjectKind::kArgumentBlock,
              argument_block) != MF_SHARED_SUCCESS) {
    return MF_SHARED_STALE_HANDLE;
  }
  if (command.arguments[1] != MF_KERNEL_PRIMARY_ENTRY_ID ||
      mf_client_argument_block_validate_v1(argument_block->data(), argument_block->byte_size()) !=
          MF_SHARED_SUCCESS) {
    return MF_SHARED_INVALID_ARGUMENT;
  }

  const auto* header = reinterpret_cast<const mf_argument_block_header_v1*>(argument_block->data());
  const auto* entries = reinterpret_cast<const mf_argument_entry_v1*>(
      argument_block->data() + sizeof(mf_argument_block_header_v1));
  if (header->flags == MF_ARGUMENT_BLOCK_FLAG_COPY_REGION_V1) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  std::vector<backend::cpu::Argument> arguments;
  arguments.reserve(header->entry_count);
  std::uint32_t launch_count = 1U;
  for (std::uint32_t index = 0; index < header->entry_count; ++index) {
    const mf_argument_entry_v1& entry = entries[index];
    if (entry.kind == MF_ARGUMENT_KIND_U32) {
      const auto value = static_cast<std::uint32_t>(entry.value);
      arguments.emplace_back(value);
      launch_count = value;
      continue;
    }
    if (entry.kind != MF_ARGUMENT_KIND_BUFFER) {
      return MF_SHARED_INVALID_ARGUMENT;
    }

    Object* memory = nullptr;
    const bool writable = (entry.flags & MF_ARGUMENT_BUFFER_WRITE) != 0U;
    mf_shared_status_v1 status =
        resolve_memory(entry.object_id, entry.object_generation, writable, memory);
    if (status == MF_SHARED_SUCCESS && (entry.flags & MF_ARGUMENT_BUFFER_READ) != 0U &&
        memory->kind == ObjectKind::kHostMemory &&
        (memory->access_flags & MF_CLIENT_CONTROL_FLAG_READ) == 0U) {
      status = MF_SHARED_INVALID_ARGUMENT;
    }
    if (status != MF_SHARED_SUCCESS) {
      return status;
    }
    if (entry.value > memory->byte_size()) {
      return MF_SHARED_INVALID_ARGUMENT;
    }
    auto* start = const_cast<std::uint8_t*>(memory->data()) + entry.value;
    if (reinterpret_cast<std::uintptr_t>(start) % alignof(std::uint32_t) != 0U) {
      return MF_SHARED_INVALID_ARGUMENT;
    }
    const auto word_count =
        static_cast<std::size_t>((memory->byte_size() - entry.value) / sizeof(std::uint32_t));
    arguments.emplace_back(backend::cpu::BufferArgument{
        .words = std::span<std::uint32_t>(reinterpret_cast<std::uint32_t*>(start), word_count),
        .writable = writable,
    });
  }
  backend::cpu::LaunchDimensions dimensions{};
  if ((header->flags & MF_ARGUMENT_BLOCK_FLAG_LAUNCH_DIMENSIONS_XY_V1) != 0U) {
    dimensions.grid_x =
        static_cast<std::uint32_t>(header->reserved[MF_ARGUMENT_BLOCK_LAUNCH_GRID_X_INDEX_V1]);
    dimensions.grid_y =
        static_cast<std::uint32_t>(header->reserved[MF_ARGUMENT_BLOCK_LAUNCH_GRID_Y_INDEX_V1]);
    dimensions.block_x =
        static_cast<std::uint32_t>(header->reserved[MF_ARGUMENT_BLOCK_LAUNCH_BLOCK_X_INDEX_V1]);
    dimensions.block_y =
        static_cast<std::uint32_t>(header->reserved[MF_ARGUMENT_BLOCK_LAUNCH_BLOCK_Y_INDEX_V1]);
  } else {
    constexpr std::uint32_t block_size = 64U;
    dimensions.grid_x = static_cast<std::uint32_t>(std::max<std::uint64_t>(
        1U, (static_cast<std::uint64_t>(launch_count) + block_size - 1U) / block_size));
    dimensions.block_x = block_size;
  }
  if (module->prepared_module == nullptr) {
    return MF_SHARED_MALFORMED;
  }
  if (module->kernel_operation != 0U) {
    const uint32_t operation = module->kernel_operation;
    bool native = true;
    if (operation == MF_CLIENT_KERNEL_REQUEST_OPERATION_CAST_COPY_I64_V1) {
      if (arguments.size() < 3U || arguments[0].index() != 2U || arguments[1].index() != 2U) {
        return MF_SHARED_INVALID_ARGUMENT;
      }
      const auto& destination = std::get<backend::cpu::BufferArgument>(arguments[0]);
      const auto& source = std::get<backend::cpu::BufferArgument>(arguments[1]);
      const auto element_count = std::get<uint32_t>(arguments.back());
      for (uint32_t index = 0; index < element_count; ++index) {
        const int64_t widened = static_cast<int32_t>(source.words[index]);
        if (2U * index + 1U >= destination.words.size()) {
          return MF_SHARED_INVALID_ARGUMENT;
        }
        destination.words[2U * index] = static_cast<uint32_t>(static_cast<uint64_t>(widened));
        destination.words[2U * index + 1U] =
            static_cast<uint32_t>(static_cast<uint64_t>(widened) >> 32U);
      }
    } else if (operation == MF_CLIENT_KERNEL_REQUEST_OPERATION_REDUCE_SUM_I32_V1 ||
               operation == MF_CLIENT_KERNEL_REQUEST_OPERATION_REDUCE_MAX_I32_V1 ||
               operation == MF_CLIENT_KERNEL_REQUEST_OPERATION_REDUCE_MIN_I32_V1) {
      if (arguments.size() < 4U || arguments[0].index() != 2U || arguments[1].index() != 2U ||
          arguments[1].index() != 2U) {
        return MF_SHARED_INVALID_ARGUMENT;
      }
      const auto& destination = std::get<backend::cpu::BufferArgument>(arguments[0]);
      const auto& source = std::get<backend::cpu::BufferArgument>(arguments[1]);
      const auto element_count = std::get<uint32_t>(arguments.back());
      int64_t accumulator = 0;
      if (operation == MF_CLIENT_KERNEL_REQUEST_OPERATION_REDUCE_MAX_I32_V1) {
        accumulator = INT32_MIN;
      } else if (operation == MF_CLIENT_KERNEL_REQUEST_OPERATION_REDUCE_MIN_I32_V1) {
        accumulator = INT32_MAX;
      }
      for (uint32_t index = 0; index < element_count && index < source.words.size(); ++index) {
        const int64_t value = static_cast<int32_t>(source.words[index]);
        if (operation == MF_CLIENT_KERNEL_REQUEST_OPERATION_REDUCE_MAX_I32_V1) {
          accumulator = accumulator > value ? accumulator : value;
        } else if (operation == MF_CLIENT_KERNEL_REQUEST_OPERATION_REDUCE_MIN_I32_V1) {
          accumulator = accumulator < value ? accumulator : value;
        } else {
          accumulator += value;
        }
      }
      if (destination.words.empty()) {
        return MF_SHARED_INVALID_ARGUMENT;
      }
      if (operation == MF_CLIENT_KERNEL_REQUEST_OPERATION_REDUCE_SUM_I32_V1 &&
          element_count > 0U) {
        // torch int32 sum promotes to the int64 accumulator output.
        destination.words[0] = static_cast<uint32_t>(static_cast<uint64_t>(accumulator));
        destination.words[1] = static_cast<uint32_t>(static_cast<uint64_t>(accumulator) >> 32U);
      } else {
        destination.words[0] = static_cast<uint32_t>(static_cast<uint64_t>(accumulator));
      }
    } else if (operation == MF_CLIENT_KERNEL_REQUEST_OPERATION_REDUCE_SUM_I64_V1) {
      if (arguments.size() < 4U || arguments[0].index() != 2U || arguments[1].index() != 2U) {
        return MF_SHARED_INVALID_ARGUMENT;
      }
      const auto& destination = std::get<backend::cpu::BufferArgument>(arguments[0]);
      const auto& source = std::get<backend::cpu::BufferArgument>(arguments[1]);
      const auto element_count = std::get<uint32_t>(arguments.back());
      int64_t accumulator = 0;
      for (uint32_t index = 0; index < element_count; ++index) {
        const uint32_t low_index = 2U * index;
        const uint32_t high_index = 2U * index + 1U;
        if (high_index >= source.words.size()) {
          break;
        }
        const uint64_t bits =
            static_cast<uint64_t>(source.words[low_index]) |
            (static_cast<uint64_t>(source.words[high_index]) << 32U);
        accumulator += static_cast<int64_t>(bits);
      }
      if (destination.words.size() < 2U) {
        return MF_SHARED_INVALID_ARGUMENT;
      }
      destination.words[0] = static_cast<uint32_t>(static_cast<uint64_t>(accumulator));
      destination.words[1] = static_cast<uint32_t>(static_cast<uint64_t>(accumulator) >> 32U);
    } else if (operation == MF_CLIENT_KERNEL_REQUEST_OPERATION_STRIDED_COPY_U32_V1) {
      if (arguments.size() < 4U || arguments[0].index() != 2U || arguments[1].index() != 2U) {
        return MF_SHARED_INVALID_ARGUMENT;
      }
      const auto& destination = std::get<backend::cpu::BufferArgument>(arguments[0]);
      const auto& source = std::get<backend::cpu::BufferArgument>(arguments[1]);
      const auto dims = std::get<uint32_t>(arguments[2]);
      if (dims == 0U || dims > 4U || 4U + 3U * dims > arguments.size()) {
        return MF_SHARED_INVALID_ARGUMENT;
      }
      std::array<uint32_t, 4> sizes{};
      std::array<uint32_t, 4> out_strides{};
      std::array<uint32_t, 4> in_strides{};
      for (uint32_t dim = 0U; dim < dims; ++dim) {
        sizes[dim] = std::get<uint32_t>(arguments[4U + dim]);
        out_strides[dim] = std::get<uint32_t>(arguments[4U + dims + dim]);
        in_strides[dim] = std::get<uint32_t>(arguments[4U + 2U * dims + dim]);
      }
      uint64_t total = 1U;
      for (uint32_t dim = 0U; dim < dims; ++dim) {
        total *= sizes[dim];
      }
      const auto element_count = std::get<uint32_t>(arguments[3]);
      for (uint64_t index = 0; index < total && index < (uint64_t)element_count; ++index) {
        uint64_t remaining = index;
        uint64_t out_byte = 0U;
        uint64_t in_byte = 0U;
        for (uint32_t dim = dims; dim-- > 0U;) {
          const uint32_t coordinate = remaining % sizes[dim];
          remaining /= sizes[dim];
          out_byte += (uint64_t)coordinate * out_strides[dim];
          in_byte += (uint64_t)coordinate * in_strides[dim];
        }
        const uint64_t out_word = out_byte / 4U;
        const uint64_t in_word = in_byte / 4U;
        if (out_byte % 4U != 0U || in_byte % 4U != 0U || out_word >= destination.words.size() ||
            in_word >= source.words.size()) {
          continue;
        }
        destination.words[out_word] = source.words[in_word];
      }
    } else if (operation == MF_CLIENT_KERNEL_REQUEST_OPERATION_CAST_TO_F32_V1) {
      if (arguments.size() < 3U || arguments[0].index() != 2U || arguments[1].index() != 2U) {
        return MF_SHARED_INVALID_ARGUMENT;
      }
      const auto& destination = std::get<backend::cpu::BufferArgument>(arguments[0]);
      const auto& source = std::get<backend::cpu::BufferArgument>(arguments[1]);
      const auto element_count = std::get<uint32_t>(arguments.back());
      for (uint32_t index = 0; index < element_count && index < source.words.size() &&
                               index < destination.words.size();
           ++index) {
        const float widened = static_cast<float>(static_cast<int32_t>(source.words[index]));
        std::memcpy(&destination.words[index], &widened, sizeof(widened));
      }
    } else if (operation == MF_CLIENT_KERNEL_REQUEST_OPERATION_CONCAT_U32_V1) {
      if (arguments.size() < 6U || arguments[0].index() != 2U ||
          ((arguments.size() - 2U) % 2U) != 0U) {
        return MF_SHARED_INVALID_ARGUMENT;
      }
      auto& destination = std::get<backend::cpu::BufferArgument>(arguments[0]);
      const auto source_count = static_cast<std::size_t>((arguments.size() - 2U) / 2U);
      if (source_count < 2U || !std::holds_alternative<std::uint32_t>(arguments.back())) {
        return MF_SHARED_INVALID_ARGUMENT;
      }
      const auto element_count = std::get<uint32_t>(arguments[arguments.size() - 1U]);
      std::uint64_t total = 0U;
      for (std::size_t index = 0U; index < source_count; ++index) {
        if (arguments[1U + index].index() != 2U ||
            !std::holds_alternative<std::uint32_t>(arguments[1U + source_count + index])) {
          return MF_SHARED_INVALID_ARGUMENT;
        }
        const auto length = static_cast<std::uint32_t>(
            std::get<std::uint32_t>(arguments[1U + source_count + index]));
        total += length;
        const auto& source = std::get<backend::cpu::BufferArgument>(arguments[1U + index]);
        if (length > source.words.size()) {
          return MF_SHARED_INVALID_ARGUMENT;
        }
      }
      if (total != element_count || total > destination.words.size()) {
        return MF_SHARED_INVALID_ARGUMENT;
      }
      std::size_t cursor = 0U;
      for (std::size_t index = 0U; index < source_count; ++index) {
        const auto& source = std::get<backend::cpu::BufferArgument>(arguments[1U + index]);
        const auto length = static_cast<std::uint32_t>(
            std::get<std::uint32_t>(arguments[1U + source_count + index]));
        for (std::size_t word = 0U; word < length; ++word) {
          destination.words[cursor++] = source.words[word];
        }
      }
    } else if (operation == MF_CLIENT_KERNEL_REQUEST_OPERATION_REDUCE_SUM_F32_V1 ||
               operation == MF_CLIENT_KERNEL_REQUEST_OPERATION_REDUCE_MEAN_F32_V1) {
      if (arguments.size() < 4U || arguments[0].index() != 2U || arguments[1].index() != 2U) {
        return MF_SHARED_INVALID_ARGUMENT;
      }
      const auto& destination = std::get<backend::cpu::BufferArgument>(arguments[0]);
      const auto& source = std::get<backend::cpu::BufferArgument>(arguments[1]);
      const auto element_count = std::get<uint32_t>(arguments.back());
      float accumulator = 0.0F;
      for (uint32_t index = 0; index < element_count && index < source.words.size(); ++index) {
        float value = 0.0F;
        static_assert(sizeof(value) == sizeof(uint32_t));
        std::memcpy(&value, &source.words[index], sizeof(value));
        accumulator += value;
      }
      if (operation == MF_CLIENT_KERNEL_REQUEST_OPERATION_REDUCE_MEAN_F32_V1 &&
          element_count != 0U) {
        accumulator /= static_cast<float>(element_count);
      }
      if (destination.words.empty()) {
        return MF_SHARED_INVALID_ARGUMENT;
      }
      std::memcpy(&destination.words[0], &accumulator, sizeof(accumulator));
    } else {
      native = false;
    }
    if (native) {
      memory_active = true;
      return MF_SHARED_SUCCESS;
    }
  }
#if METAFLUX_DAEMON_VULKAN_EXECUTION
  if (vulkan_route_ == nullptr) {
    vulkan_route_ = global_vulkan_execution_route();
  }
  if (vulkan_route_ != nullptr && module->vulkan_module != nullptr &&
      module->vulkan_module->dispatchable()) {
    std::vector<VulkanLaunchBuffer> launch_buffers;
    launch_buffers.reserve(arguments.size());
    std::uint32_t element_count = 0U;
    for (const auto& argument : arguments) {
      if (const auto* buffer = std::get_if<backend::cpu::BufferArgument>(&argument)) {
        launch_buffers.push_back(VulkanLaunchBuffer{
            reinterpret_cast<std::byte*>(buffer->words.data()),
            buffer->words.size() * sizeof(std::uint32_t), buffer->writable});
      } else if (const auto* scalar = std::get_if<std::uint32_t>(&argument)) {
        // The lowered shaders take every kernel parameter, scalars included,
        // as a one-element storage buffer; the view aliases the local copy.
        element_count = *scalar;
        launch_buffers.push_back(VulkanLaunchBuffer{
            reinterpret_cast<std::byte*>(&element_count), sizeof(element_count), false});
      }
    }
    std::string vulkan_diagnostic;
    if (vulkan_route_->launch(*module->vulkan_module, launch_buffers, element_count,
                              vulkan_diagnostic)) {
      memory_active = true;
      return MF_SHARED_SUCCESS;
    }
    std::cerr << "metafluxd: vulkan launch failed diagnostic=" << vulkan_diagnostic << '\n';
    return MF_SHARED_DEVICE_LOST;
  }
#endif
  const backend::cpu::ExecutionResult result =
      module->prepared_module->launch(arguments, dimensions, process_stop_token());
  if (result.ok()) {
    memory_active = module->prepared_module->accesses_global_memory();
    return MF_SHARED_SUCCESS;
  }
  if (result.diagnostic->error == backend::cpu::ExecutionError::UnsupportedOperation) {
    return MF_SHARED_NOT_SUPPORTED;
  }
  if (result.diagnostic->error == backend::cpu::ExecutionError::InvalidArtifact ||
      result.diagnostic->error == backend::cpu::ExecutionError::SchemaMismatch) {
    return MF_SHARED_MALFORMED;
  }
  if (result.diagnostic->error == backend::cpu::ExecutionError::PlacementUnavailable ||
      result.diagnostic->error == backend::cpu::ExecutionError::PlacementPinLost) {
    return MF_SHARED_DEVICE_LOST;
  }
  if (result.diagnostic->error == backend::cpu::ExecutionError::Cancelled) {
    return MF_SHARED_STALE_HANDLE;
  }
  if (result.diagnostic->error == backend::cpu::ExecutionError::System) {
    return MF_SHARED_SYSTEM_ERROR;
  }
  return MF_SHARED_INVALID_ARGUMENT;
}

mf_shared_status_v1 Session::process_command(const mf_ring_descriptor_v1& command,
                                             std::uint64_t& result_id,
                                             std::uint64_t& result_generation,
                                             std::uint64_t& timeline, std::uint64_t& detail,
                                             bool& memory_active) {
  result_id = command.target_id;
  result_generation = 0U;
  timeline = 0U;
  detail = 0U;
  memory_active = false;
  if (observer_) {
    return MF_SHARED_NOT_SUPPORTED;
  }
  if (command.request_id == 0U || command.target_id == 0U) {
    return MF_SHARED_MALFORMED;
  }

  switch (command.opcode) {
  case MF_RING_OPCODE_NOOP:
    result_generation = 1U;
    return MF_SHARED_SUCCESS;
  case MF_RING_OPCODE_MEMORY_ALLOC:
    if (command.target_id != MF_CLIENT_RUNTIME_CONTEXT_ID_V1 || command.arguments[0] == 0U ||
        command.arguments[1] == 0U || (command.arguments[1] & (command.arguments[1] - 1U)) != 0U) {
      return MF_SHARED_INVALID_ARGUMENT;
    }
    return add_owned_object(ObjectKind::kDeviceMemory, command.arguments[0], result_id,
                            result_generation);
  case MF_RING_OPCODE_MEMORY_FREE:
    result_generation = command.arguments[0];
    return release_object(command.target_id, command.arguments[0], ObjectKind::kDeviceMemory);
  case MF_RING_OPCODE_MODULE_LOAD: {
    Object* artifact = nullptr;
    if (resolve(command.target_id, command.arguments[0], ObjectKind::kArtifact, artifact) !=
        MF_SHARED_SUCCESS) {
      return MF_SHARED_STALE_HANDLE;
    }
    if (artifact->artifact_source.size == 0U ||
        artifact->artifact_source.offset > artifact->byte_size() ||
        artifact->artifact_source.size > artifact->byte_size() - artifact->artifact_source.offset) {
      return MF_SHARED_MALFORMED;
    }
    const std::string_view ptx(
        reinterpret_cast<const char*>(artifact->data() + artifact->artifact_source.offset),
        static_cast<std::size_t>(artifact->artifact_source.size));
    const compiler::ptx::ParseResult parsed = compiler::ptx::parse(ptx);
    if (!parsed.ok()) {
      return MF_SHARED_MALFORMED;
    }
    compiler::SerializationResult serialized = compiler::serialize_kernel(*parsed.kernel);
    if (!serialized.ok()) {
      return MF_SHARED_MALFORMED;
    }
    const std::stop_token cancellation = process_stop_token();
    auto prepared = execution_->prepare(static_cast<std::uint32_t>(credentials_.uid),
                                        *parsed.kernel, std::move(serialized.text), cancellation);
    if (cancellation.stop_requested()) {
      return MF_SHARED_STALE_HANDLE;
    }
    if (!prepared.ok()) {
      std::cerr << "metafluxd: module preparation failed mode="
                << cpu_execution_mode_name(execution_->mode()) << " peer-uid=" << credentials_.uid
                << " diagnostic=" << prepared.diagnostic << '\n';
      return module_preparation_status(prepared.error);
    }
    const mf_shared_status_v1 added =
        add_module(std::move(prepared.module), result_id, result_generation);
    if (added != MF_SHARED_SUCCESS) {
      return added;
    }
    if (vulkan_route_ == nullptr) {
      vulkan_route_ = global_vulkan_execution_route();
    }
    Object* module_object = nullptr;
    if (resolve(result_id, result_generation, ObjectKind::kModule, module_object) ==
        MF_SHARED_SUCCESS) {
      Object* loaded_artifact = nullptr;
      if (resolve(command.target_id, command.arguments[0], ObjectKind::kArtifact,
                  loaded_artifact) == MF_SHARED_SUCCESS) {
        module_object->kernel_operation = loaded_artifact->kernel_operation;
      }
    }
    if (vulkan_route_ != nullptr && module_object != nullptr &&
        resolve(result_id, result_generation, ObjectKind::kModule,
                module_object) == MF_SHARED_SUCCESS &&
        module_object->vulkan_module == nullptr) {
      std::string vulkan_diagnostic;
      module_object->vulkan_module =
          vulkan_route_->prepare(*parsed.kernel, vulkan_diagnostic);
      if (module_object->vulkan_module != nullptr) {
        std::cerr << "metafluxd: vulkan route prepared artifact entry="
                  << module_object->vulkan_module->entry_point() << '\n';
      }
    }
    return MF_SHARED_SUCCESS;
  }
  case MF_RING_OPCODE_MODULE_UNLOAD:
    result_generation = command.arguments[0];
    return release_object(command.target_id, command.arguments[0], ObjectKind::kModule);
  case MF_RING_OPCODE_COPY: {
    Object* destination = nullptr;
    Object* source = nullptr;
    std::uint64_t destination_offset = 0U;
    std::uint64_t source_offset = 0U;
    std::uint64_t byte_count = 0U;
    result_generation = command.arguments[0];
    mf_shared_status_v1 destination_status = MF_SHARED_SUCCESS;
    mf_shared_status_v1 source_status = MF_SHARED_SUCCESS;
    if (command.flags == MF_RING_COPY_FLAG_DIRECT_HOST_SOURCE_V1 ||
        command.flags == MF_RING_COPY_FLAG_DIRECT_HOST_DESTINATION_V1) {
      Object* device_memory = nullptr;
      const bool host_is_source = command.flags == MF_RING_COPY_FLAG_DIRECT_HOST_SOURCE_V1;
      if ((negotiated_capabilities_ & MF_CLIENT_CAP_DIRECT_HOST_COPY_V1) == 0U) {
        return MF_SHARED_NOT_SUPPORTED;
      }
      if (!host_address_space_.valid()) {
        return MF_SHARED_INVALID_ARGUMENT;
      }
      const mf_shared_status_v1 resolve_status =
          resolve_memory(command.target_id, command.arguments[0], host_is_source, device_memory);
      if (resolve_status != MF_SHARED_SUCCESS) {
        return resolve_status;
      }
      const std::uint64_t host_address = command.arguments[1];
      const std::uint64_t device_offset = command.arguments[2];
      byte_count = command.arguments[3];
      if (device_memory->kind != ObjectKind::kDeviceMemory || byte_count == 0U ||
          device_offset > device_memory->byte_size() ||
          byte_count > device_memory->byte_size() - device_offset ||
          (host_is_source && device_memory->mutable_data() == nullptr)) {
        return MF_SHARED_INVALID_ARGUMENT;
      }
      std::uint8_t* device_bytes =
          host_is_source ? device_memory->mutable_data() + static_cast<std::size_t>(device_offset)
                         : const_cast<std::uint8_t*>(device_memory->data()) +
                               static_cast<std::size_t>(device_offset);
      const mf_shared_status_v1 copy_status = copy_peer_memory(
          host_address_space_.get(), host_address, device_bytes, byte_count, host_is_source);
      if (copy_status == MF_SHARED_SUCCESS) {
        if (host_is_source) {
          (void)copy_path_statistics_->direct_host_source_operations.fetch_add(
              1U, std::memory_order_relaxed);
          (void)copy_path_statistics_->direct_host_source_bytes.fetch_add(
              byte_count, std::memory_order_relaxed);
        } else {
          (void)copy_path_statistics_->direct_host_destination_operations.fetch_add(
              1U, std::memory_order_relaxed);
          (void)copy_path_statistics_->direct_host_destination_bytes.fetch_add(
              byte_count, std::memory_order_relaxed);
        }
        memory_active = true;
      }
      return copy_status;
    }
    if (command.flags == MF_RING_COPY_FLAG_REGION_ARGUMENT_BLOCK_V1) {
      Object* argument_block = nullptr;
      if ((negotiated_capabilities_ & MF_CLIENT_CAP_COPY_REGION_V1) == 0U) {
        return MF_SHARED_NOT_SUPPORTED;
      }
      if (command.arguments[1] != 0U || command.arguments[2] != 0U || command.arguments[3] != 0U) {
        return MF_SHARED_MALFORMED;
      }
      const mf_shared_status_v1 argument_status = resolve(
          command.target_id, command.arguments[0], ObjectKind::kArgumentBlock, argument_block);
      if (argument_status != MF_SHARED_SUCCESS) {
        return argument_status;
      }
      const mf_shared_status_v1 validation_status =
          mf_client_copy_region_argument_block_validate_v1(argument_block->data(),
                                                           argument_block->byte_size());
      if (validation_status != MF_SHARED_SUCCESS) {
        return validation_status;
      }
      const auto* entries = reinterpret_cast<const mf_argument_entry_v1*>(
          argument_block->data() + sizeof(mf_argument_block_header_v1));
      const mf_argument_entry_v1& destination_entry = entries[MF_COPY_REGION_DESTINATION_INDEX_V1];
      const mf_argument_entry_v1& source_entry = entries[MF_COPY_REGION_SOURCE_INDEX_V1];
#if METAFLUX_DAEMON_CDEV_BACKEND
      if (cdev_worker_ != nullptr) {
      metaflux::transport::cdev::CdevObjectTableResolver resolver(
          this, &Session::cdev_object_lookup, this, &Session::cdev_memory_import,
          cdev_backend_instance_, cdev_backend_context_);
      metaflux::transport::cdev::CdevCopyResolution resolution{};
      const mf_shared_status_v1 resolve_status = resolver.resolve_copy(&command, &resolution);
      if (resolve_status != MF_SHARED_SUCCESS) {
        return resolve_status;
      }
      mf_backend_copy_v1 backend_copy{};
      backend_copy.struct_size = sizeof(backend_copy);
      backend_copy.destination = resolution.destination;
      backend_copy.destination_offset = resolution.destination_offset;
      backend_copy.source = resolution.source;
      backend_copy.source_offset = resolution.source_offset;
      backend_copy.byte_count = resolution.byte_count;
      const mf_shared_status_v1 destination_retain =
          cdev_retain_reference(resolution.destination_reference);
      const mf_shared_status_v1 source_retain =
          destination_retain == MF_SHARED_SUCCESS
              ? cdev_retain_reference(resolution.source_reference)
              : destination_retain;
      if (destination_retain != MF_SHARED_SUCCESS || source_retain != MF_SHARED_SUCCESS) {
        cdev_release_reference(resolution.destination_reference);
        cdev_release_reference(resolution.source_reference);
        return source_retain;
      }
      const mf_shared_status_v1 copy_status = cdev_backend_status(
          cdev_backend_api_->copy(cdev_backend_instance_, cdev_backend_queue_, &backend_copy, 0U));
      cdev_release_reference(resolution.destination_reference);
      cdev_release_reference(resolution.source_reference);
      if (copy_status != MF_SHARED_SUCCESS) {
        return copy_status;
      }
      metaflux::transport::cdev::CdevObjectTableView destination_view{};
      metaflux::transport::cdev::CdevObjectTableView source_view{};
      const mf_shared_status_v1 destination_view_status =
          cdev_object_lookup(this, destination_entry.object_id, destination_entry.object_generation,
                             0U, true, &destination_view);
      const mf_shared_status_v1 source_view_status = cdev_object_lookup(
          this, source_entry.object_id, source_entry.object_generation, 0U, false, &source_view);
      byte_count = resolution.byte_count;
      if (destination_view_status == MF_SHARED_SUCCESS &&
          destination_view.object_kind == MF_OBJECT_TYPE_HOST_MEMORY) {
        (void)copy_path_statistics_->staged_host_destination_operations.fetch_add(
            1U, std::memory_order_relaxed);
        (void)copy_path_statistics_->staged_host_destination_bytes.fetch_add(
            byte_count, std::memory_order_relaxed);
      }
      if (source_view_status == MF_SHARED_SUCCESS &&
          source_view.object_kind == MF_OBJECT_TYPE_HOST_MEMORY) {
        (void)copy_path_statistics_->staged_host_source_operations.fetch_add(
            1U, std::memory_order_relaxed);
        (void)copy_path_statistics_->staged_host_source_bytes.fetch_add(byte_count,
                                                                        std::memory_order_relaxed);
      }
      memory_active = true;
      return MF_SHARED_SUCCESS;
      }
#endif
      destination_status = resolve_memory(destination_entry.object_id,
                                          destination_entry.object_generation, true, destination);
      source_status =
          resolve_memory(source_entry.object_id, source_entry.object_generation, false, source);
      destination_offset = destination_entry.value;
      source_offset = source_entry.value;
      byte_count = entries[MF_COPY_REGION_BYTE_COUNT_INDEX_V1].value;
    } else if (command.flags == 0U) {
      destination_status =
          resolve_memory(command.target_id, command.arguments[0], true, destination);
      source_status = resolve_memory(command.arguments[1], command.arguments[2], false, source);
      byte_count = command.arguments[3];
    } else {
      return MF_SHARED_MALFORMED;
    }
    if (destination_status != MF_SHARED_SUCCESS) {
      return destination_status;
    }
    if (source_status != MF_SHARED_SUCCESS) {
      return source_status;
    }
    if (byte_count == 0U || destination_offset > destination->byte_size() ||
        source_offset > source->byte_size() ||
        byte_count > destination->byte_size() - destination_offset ||
        byte_count > source->byte_size() - source_offset ||
        destination->mutable_data() == nullptr) {
      return MF_SHARED_INVALID_ARGUMENT;
    }
    std::memmove(destination->mutable_data() + static_cast<std::size_t>(destination_offset),
                 source->data() + static_cast<std::size_t>(source_offset),
                 static_cast<std::size_t>(byte_count));
    if (source->kind == ObjectKind::kHostMemory) {
      (void)copy_path_statistics_->staged_host_source_operations.fetch_add(
          1U, std::memory_order_relaxed);
      (void)copy_path_statistics_->staged_host_source_bytes.fetch_add(byte_count,
                                                                      std::memory_order_relaxed);
    }
    if (destination->kind == ObjectKind::kHostMemory) {
      (void)copy_path_statistics_->staged_host_destination_operations.fetch_add(
          1U, std::memory_order_relaxed);
      (void)copy_path_statistics_->staged_host_destination_bytes.fetch_add(
          byte_count, std::memory_order_relaxed);
    }
    memory_active = true;
    return MF_SHARED_SUCCESS;
  }
  case MF_RING_OPCODE_LAUNCH:
    result_generation = command.arguments[0];
    return process_launch(command, memory_active);
  case MF_RING_OPCODE_EVENT_RECORD:
    result_generation = command.arguments[0];
    if (command.target_id != MF_CLIENT_RUNTIME_EVENT_ID_V1 ||
        command.arguments[0] != MF_CLIENT_RUNTIME_EVENT_GENERATION_V1 ||
        command.arguments[1] == 0U) {
      return MF_SHARED_STALE_HANDLE;
    }
    event_timeline_ = command.arguments[1];
    timeline = event_timeline_;
    return MF_SHARED_SUCCESS;
  case MF_RING_OPCODE_EVENT_WAIT:
    result_generation = command.arguments[0];
    if (command.target_id != MF_CLIENT_RUNTIME_EVENT_ID_V1 ||
        command.arguments[0] != MF_CLIENT_RUNTIME_EVENT_GENERATION_V1) {
      return MF_SHARED_STALE_HANDLE;
    }
    if (event_timeline_ < command.arguments[1]) {
      return MF_SHARED_WOULD_BLOCK;
    }
    timeline = event_timeline_;
    return MF_SHARED_SUCCESS;
  case MF_RING_OPCODE_QUEUE_SYNCHRONIZE:
  case MF_RING_OPCODE_QUEUE_CANCEL:
    result_generation = MF_CLIENT_QUEUE_GENERATION_V1;
    return command.target_id == MF_CLIENT_SUBMISSION_QUEUE_ID_V1 ? MF_SHARED_SUCCESS
                                                                 : MF_SHARED_STALE_HANDLE;
  default:
    return MF_SHARED_NOT_SUPPORTED;
  }
}

mf_shared_status_v1 Session::pump_once() noexcept {
  if (pending_completion_.has_value()) {
    const mf_shared_status_v1 status =
        mf_client_ring_try_submit_v1(&completion_, &*pending_completion_);
    if (status == MF_SHARED_SUCCESS) {
      pending_completion_.reset();
      if (terminate_after_response_) {
        return MF_SHARED_STALE_HANDLE;
      }
    }
    return status;
  }

  mf_ring_descriptor_v1 command{};
  const mf_shared_status_v1 consume_status = mf_client_ring_try_consume_v1(&submission_, &command);
  if (consume_status != MF_SHARED_SUCCESS) {
    return consume_status;
  }
  std::uint64_t result_id = command.target_id;
  std::uint64_t result_generation = 0U;
  std::uint64_t timeline = 0U;
  std::uint64_t detail = 0U;
  mf_shared_status_v1 command_status = MF_SHARED_SYSTEM_ERROR;
  if (!process_admitted()) {
    command_status = MF_SHARED_STALE_HANDLE;
    terminate_after_response_ = true;
  } else {
    const std::uint64_t work_start_ns = monotonic_time_ns();
    bool memory_active = false;
    try {
      command_status =
          process_command(command, result_id, result_generation, timeline, detail, memory_active);
    } catch (const std::bad_alloc&) {
      command_status = MF_SHARED_RESOURCE_EXHAUSTED;
    } catch (...) {
      command_status = MF_SHARED_SYSTEM_ERROR;
    }
    (void)registry_authority_->record_work(work_start_ns, monotonic_time_ns(), memory_active);
    if (!process_admitted()) {
      command_status = MF_SHARED_STALE_HANDLE;
      terminate_after_response_ = true;
    }
  }

  mf_ring_descriptor_v1 completion{};
  completion.opcode = MF_RING_OPCODE_COMPLETION;
  completion.flags = command.flags;
  completion.request_id = command.request_id;
  completion.target_id = result_id;
  completion.arguments[0] = static_cast<std::uint32_t>(command_status);
  completion.arguments[1] = result_generation;
  completion.arguments[2] = timeline;
  completion.arguments[3] = detail;
  const mf_shared_status_v1 submit_status = mf_client_ring_try_submit_v1(&completion_, &completion);
  if (submit_status == MF_SHARED_WOULD_BLOCK) {
    pending_completion_ = completion;
  }
  if (submit_status == MF_SHARED_SUCCESS && terminate_after_response_) {
    return MF_SHARED_STALE_HANDLE;
  }
  return submit_status;
}

mf_shared_status_v1 EmbeddedCpuWorker::pump_once() noexcept { return session_.pump_once(); }

#if METAFLUX_DAEMON_CDEV_BACKEND
mf_shared_status_v1 CdevDataPlaneWorker::pump_once() noexcept {
  const auto result = session_.cdev_worker_ == nullptr
                          ? metaflux::transport::cdev::WorkerResult::Idle
                          : session_.cdev_worker_->consume_once();
  switch (result) {
  case metaflux::transport::cdev::WorkerResult::Completed:
    return MF_SHARED_SUCCESS;
  case metaflux::transport::cdev::WorkerResult::Idle:
  case metaflux::transport::cdev::WorkerResult::Backpressure:
    return MF_SHARED_WOULD_BLOCK;
  case metaflux::transport::cdev::WorkerResult::Malformed:
    (void)session_.report_cdev_loss();
    return MF_SHARED_DEVICE_LOST;
  }
  return MF_SHARED_SYSTEM_ERROR;
}
#endif

void Session::serve(const mf_client_negotiation_request_v1& request,
                    std::stop_token stop_token) noexcept {
  mf_client_negotiation_response_v1 response{};
  const std::uint32_t negotiation = mf_client_negotiate_v1(
      &request, 1U, 1U, kRuntimeCapabilities, MF_SHARED_DEVICE_ABI_VERSION_1,
      MF_SHARED_DEVICE_ABI_VERSION_1, view_id_.daemon_incarnation, view_id_.view_serial, &response);
  if (negotiation != MF_CLIENT_NEGOTIATION_OK) {
    (void)send_packet(peer_.get(), response.bytes, {});
    return;
  }
  negotiated_capabilities_ = mf_client_load_le64_v1(response.bytes + 24);
  if (!observer_) {
    const mf_shared_status_v1 register_status = process_authority_->register_session(
        static_cast<std::uint32_t>(credentials_.pid),
        (negotiated_capabilities_ & MF_CLIENT_CAP_LIVE_CONTEXT_ACCOUNTING_V1) != 0U,
        process_session_id_, process_admission_);
    if (register_status != MF_SHARED_SUCCESS) {
      mf_client_negotiation_response_init_v1(&response, MF_CLIENT_NEGOTIATION_MALFORMED, 0U, 0U, 0U,
                                             0U, 0U, 0U);
      (void)send_packet(peer_.get(), response.bytes, {});
      return;
    }
    process_registered_ = true;
  }
  const std::array<int, 3> descriptors{
      registry_authority_->borrow_fd(),
      mf_client_ring_borrow_fd_v1(&submission_),
      mf_client_ring_borrow_fd_v1(&completion_),
  };
  if (!send_packet(peer_.get(), response.bytes, descriptors)) {
    return;
  }

  EmbeddedCpuWorker embedded_worker(*this);
#if METAFLUX_DAEMON_CDEV_BACKEND
  CdevDataPlaneWorker cdev_worker(*this);
#endif
  while (shutdown_requested == 0 && !stop_token.stop_requested()) {
    if (tombstone_deadline_expired()) {
      return;
    }
    bool did_work = false;
    for (std::uint32_t count = 0; count < kRingCapacity; ++count) {
      bool round_work = false;
      const mf_shared_status_v1 unix_status = embedded_worker.pump_once();
      if (unix_status == MF_SHARED_SUCCESS) {
        round_work = true;
      } else if (unix_status != MF_SHARED_WOULD_BLOCK && unix_status != MF_SHARED_RETRY) {
        return;
      }
#if METAFLUX_DAEMON_CDEV_BACKEND
      if (cdev_worker_ != nullptr) {
        const mf_shared_status_v1 cdev_status = cdev_worker.pump_once();
        if (cdev_status == MF_SHARED_SUCCESS) {
          round_work = true;
        } else if (cdev_status != MF_SHARED_WOULD_BLOCK && cdev_status != MF_SHARED_RETRY) {
          return;
        }
      }
#endif
      if (round_work) {
        did_work = true;
        continue;
      }
      break;
    }

    if (tombstone_deadline_expired()) {
      return;
    }
    const bool tombstoned = process_tombstoned();
    if (tombstoned && !pending_completion_.has_value() && deferred_control_.has_value()) {
      ReceivedPacket packet = std::move(*deferred_control_);
      deferred_control_.reset();
      if (!handle_control_packet(std::move(packet))) {
        return;
      }
      continue;
    }

    const bool draining_completion = tombstoned && pending_completion_.has_value();
    pollfd descriptor{.fd = peer_.get(),
                      .events = static_cast<short>(draining_completion ? 0 : POLLIN),
                      .revents = 0};
    int poll_result = -1;
    const int poll_timeout = draining_completion
                                 ? tombstone_poll_timeout_ms()
                                 : (tombstoned ? 0 : (did_work ? 0 : kSessionPollMilliseconds));
    do {
      poll_result = poll(&descriptor, 1U, poll_timeout);
    } while (poll_result < 0 && errno == EINTR && shutdown_requested == 0);
    if (poll_result < 0) {
      return;
    }
    if ((descriptor.revents & (POLLERR | POLLHUP | POLLNVAL)) != 0) {
      return;
    }
    if (draining_completion) {
      continue;
    }
    if (poll_result == 0) {
      if (tombstoned) {
        return;
      }
      continue;
    }
    if ((descriptor.revents & POLLIN) != 0) {
      ReceivedPacket packet;
      const PacketStatus packet_status = receive_packet(peer_.get(), 1U, packet);
      if (packet_status != PacketStatus::kOk || !handle_control_packet(std::move(packet))) {
        return;
      }
    }
    if (tombstoned) {
      return;
    }
  }
}

class Listener final {
public:
  Listener() = default;
  Listener(UniqueFd fd, std::string owned_path) noexcept
      : fd_(std::move(fd)), owned_path_(std::move(owned_path)) {}
  ~Listener() {
    fd_.reset();
    if (!owned_path_.empty()) {
      (void)unlink(owned_path_.c_str());
    }
  }
  Listener(const Listener&) = delete;
  Listener& operator=(const Listener&) = delete;
  Listener(Listener&& other) noexcept
      : fd_(std::move(other.fd_)), owned_path_(std::exchange(other.owned_path_, {})) {}
  Listener& operator=(Listener&& other) noexcept {
    if (this != &other) {
      fd_ = std::move(other.fd_);
      if (!owned_path_.empty()) {
        (void)unlink(owned_path_.c_str());
      }
      owned_path_ = std::exchange(other.owned_path_, {});
    }
    return *this;
  }

  [[nodiscard]] int fd() const noexcept { return fd_.get(); }
  [[nodiscard]] bool valid() const noexcept { return fd_.valid(); }

private:
  UniqueFd fd_;
  std::string owned_path_;
};

[[nodiscard]] bool parse_decimal(std::string_view text, std::uint64_t& out_value) noexcept {
  if (text.empty()) {
    return false;
  }
  const char* begin = text.data();
  const char* end = begin + text.size();
  const auto [position, error] = std::from_chars(begin, end, out_value);
  return error == std::errc{} && position == end;
}

[[nodiscard]] bool validate_listener(int fd) noexcept {
  int socket_type = 0;
  int accepting = 0;
  socklen_t size = sizeof(socket_type);
  if (fd < 0 || getsockopt(fd, SOL_SOCKET, SO_TYPE, &socket_type, &size) != 0 ||
      size != sizeof(socket_type) || socket_type != SOCK_SEQPACKET) {
    return false;
  }
  size = sizeof(accepting);
  if (getsockopt(fd, SOL_SOCKET, SO_ACCEPTCONN, &accepting, &size) != 0 || accepting == 0) {
    return false;
  }
  const int descriptor_flags = fcntl(fd, F_GETFD);
  const int status_flags = fcntl(fd, F_GETFL);
  return descriptor_flags >= 0 && status_flags >= 0 &&
         fcntl(fd, F_SETFD, descriptor_flags | FD_CLOEXEC) == 0 &&
         fcntl(fd, F_SETFL, status_flags | O_NONBLOCK) == 0;
}

struct ActivationResult final {
  bool configured = false;
  std::optional<Listener> listener;
};

[[nodiscard]] ActivationResult systemd_listener() {
  const char* listen_pid = std::getenv("LISTEN_PID");
  const char* listen_fds = std::getenv("LISTEN_FDS");
  ActivationResult result{};
  if (listen_pid == nullptr && listen_fds == nullptr) {
    return result;
  }
  result.configured = true;
  std::uint64_t pid = 0;
  std::uint64_t count = 0;
  if (listen_pid == nullptr || listen_fds == nullptr || !parse_decimal(listen_pid, pid) ||
      !parse_decimal(listen_fds, count) || pid != static_cast<std::uint64_t>(getpid()) ||
      count != 1U || !validate_listener(3)) {
    return result;
  }
  (void)unsetenv("LISTEN_PID");
  (void)unsetenv("LISTEN_FDS");
  (void)unsetenv("LISTEN_FDNAMES");
  result.listener.emplace(UniqueFd(3), std::string{});
  return result;
}

[[nodiscard]] std::optional<Listener> standalone_listener(std::string_view path) {
  if (path.empty() || path.size() >= sizeof(sockaddr_un::sun_path) || path.front() != '/') {
    return std::nullopt;
  }
  struct stat existing{};
  const std::string owned_path(path);
  if (lstat(owned_path.c_str(), &existing) == 0 || errno != ENOENT) {
    return std::nullopt;
  }
  UniqueFd listener(socket(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC | SOCK_NONBLOCK, 0));
  if (!listener.valid()) {
    return std::nullopt;
  }
  sockaddr_un address{};
  address.sun_family = AF_UNIX;
  std::memcpy(address.sun_path, path.data(), path.size());
  address.sun_path[path.size()] = '\0';
  const auto address_size =
      static_cast<socklen_t>(offsetof(sockaddr_un, sun_path) + path.size() + 1U);
  if (bind(listener.get(), reinterpret_cast<const sockaddr*>(&address), address_size) != 0) {
    return std::nullopt;
  }
  if (chmod(owned_path.c_str(), S_IRUSR | S_IWUSR) != 0 || listen(listener.get(), 32) != 0) {
    (void)unlink(owned_path.c_str());
    return std::nullopt;
  }
  return Listener(std::move(listener), owned_path);
}

[[nodiscard]] std::optional<std::uint64_t> daemon_incarnation() noexcept {
  std::uint64_t value = 0;
  std::size_t filled = 0;
  while (filled < sizeof(value)) {
    const long result = syscall(SYS_getrandom, reinterpret_cast<std::uint8_t*>(&value) + filled,
                                sizeof(value) - filled, 0);
    if (result < 0 && errno == EINTR) {
      continue;
    }
    if (result <= 0) {
      return std::nullopt;
    }
    filled += static_cast<std::size_t>(result);
  }
  if (value == 0U) {
    return std::nullopt;
  }
  return value;
}

struct ActiveSession final {
  std::shared_ptr<std::atomic_bool> finished;
  std::jthread thread;
};

void serve_peer(UniqueFd peer, ucred credentials, std::shared_ptr<CpuExecutionEngine> execution,
                std::shared_ptr<RegistryAuthority> registry_authority,
                std::shared_ptr<ProcessAuthority> process_authority,
                std::shared_ptr<ResourceAuthority> resource_authority,
                std::shared_ptr<CopyPathStatistics> copy_path_statistics,
                SessionLease session_lease, const std::shared_ptr<std::atomic_bool>& finished,
                std::stop_token stop_token) noexcept {
  try {
    const std::uint64_t started = monotonic_time_ns();
    const std::uint64_t handshake_deadline =
        started > std::numeric_limits<std::uint64_t>::max() - kHandshakeTimeoutNs
            ? std::numeric_limits<std::uint64_t>::max()
            : started + kHandshakeTimeoutNs;
    pollfd descriptor{.fd = peer.get(), .events = POLLIN, .revents = 0};
    while (shutdown_requested == 0 && !stop_token.stop_requested()) {
      const std::uint64_t now = monotonic_time_ns();
      if (now >= handshake_deadline) {
        finished->store(true, std::memory_order_release);
        return;
      }
      constexpr std::uint64_t nanoseconds_per_millisecond = UINT64_C(1000000);
      const std::uint64_t remaining = handshake_deadline - now;
      const std::uint64_t rounded =
          remaining / nanoseconds_per_millisecond +
          (remaining % nanoseconds_per_millisecond != UINT64_C(0) ? 1U : 0U);
      const int poll_timeout =
          static_cast<int>(std::min<std::uint64_t>(rounded, kAcceptPollMilliseconds));
      int result = -1;
      do {
        result = poll(&descriptor, 1U, poll_timeout);
      } while (result < 0 && errno == EINTR && shutdown_requested == 0);
      if (result < 0 || (descriptor.revents & (POLLERR | POLLHUP | POLLNVAL)) != 0) {
        finished->store(true, std::memory_order_release);
        return;
      }
      if (result == 0) {
        continue;
      }
      if (monotonic_time_ns() >= handshake_deadline) {
        finished->store(true, std::memory_order_release);
        return;
      }
      ReceivedPacket packet;
      if (receive_packet(peer.get(), 0U, packet) != PacketStatus::kOk) {
        finished->store(true, std::memory_order_release);
        return;
      }
      mf_client_negotiation_request_v1 request{};
      std::memcpy(request.bytes, packet.bytes.data(), sizeof(request.bytes));
      if (mf_client_negotiation_request_validate_v1(&request) != MF_CLIENT_NEGOTIATION_OK) {
        mf_client_negotiation_response_v1 response{};
        mf_client_negotiation_response_init_v1(&response, MF_CLIENT_NEGOTIATION_MALFORMED, 0U, 0U,
                                               0U, 0U, 0U, 0U);
        (void)send_packet(peer.get(), response.bytes, {});
        finished->store(true, std::memory_order_release);
        return;
      }
      const std::uint64_t requested_capabilities =
          mf_client_load_le64_v1(request.bytes + 16) | mf_client_load_le64_v1(request.bytes + 24);
      const bool observer =
          (requested_capabilities & MF_CLIENT_CAP_PROCESS_SNAPSHOT_V1) != UINT64_C(0);
      Session session(std::move(peer), credentials, std::move(execution),
                      std::move(registry_authority), std::move(process_authority),
                      std::move(resource_authority), std::move(copy_path_statistics),
                      std::move(session_lease), observer);
      if (session.initialize() == MF_SHARED_SUCCESS) {
        session.serve(request, stop_token);
      }
      finished->store(true, std::memory_order_release);
      return;
    }
  } catch (...) {
  }
  finished->store(true, std::memory_order_release);
}

[[nodiscard]] bool install_signal_handlers() noexcept {
  struct sigaction action{};
  action.sa_handler = signal_handler;
  if (sigemptyset(&action.sa_mask) != 0 || sigaction(SIGINT, &action, nullptr) != 0 ||
      sigaction(SIGTERM, &action, nullptr) != 0) {
    return false;
  }
  struct sigaction ignore{};
  ignore.sa_handler = SIG_IGN;
  return sigemptyset(&ignore.sa_mask) == 0 && sigaction(SIGPIPE, &ignore, nullptr) == 0;
}

} // namespace

int run(std::string_view socket_path) {
  shutdown_requested = 0;
  CpuExecutionConfigurationResult execution_configuration;
  try {
    execution_configuration = cpu_execution_configuration_from_environment();
  } catch (const std::exception& error) {
    std::cerr << "metafluxd: CPU execution configuration failed: " << error.what() << '\n';
    return 64;
  }
  if (!execution_configuration.ok()) {
    std::cerr << "metafluxd: CPU execution configuration failed: "
              << execution_configuration.diagnostic << '\n';
    return 64;
  }
  std::shared_ptr<CpuExecutionEngine> execution;
  try {
    execution =
        std::make_shared<CpuExecutionEngine>(std::move(*execution_configuration.configuration));
  } catch (const std::bad_alloc&) {
    std::cerr << "metafluxd: CPU execution engine allocation failed\n";
    return 1;
  }
  std::string execution_diagnostic;
  if (!execution->initialize(execution_diagnostic)) {
    std::cerr << "metafluxd: CPU execution initialization failed: " << execution_diagnostic << '\n';
    return 1;
  }
  if (!install_signal_handlers()) {
    std::cerr << "metafluxd: signal setup failed\n";
    return 1;
  }
  const std::optional<std::uint64_t> incarnation = daemon_incarnation();
  if (!incarnation.has_value()) {
    std::cerr << "metafluxd: daemon incarnation allocation failed\n";
    return 1;
  }

  ActivationResult activation = systemd_listener();
  std::optional<Listener> listener;
  if (activation.configured) {
    listener = std::move(activation.listener);
  } else {
    listener = standalone_listener(socket_path);
  }
  if (!listener.has_value() || !listener->valid()) {
    std::cerr << "metafluxd: listener setup failed\n";
    return 1;
  }

  std::vector<ActiveSession> sessions;
  std::shared_ptr<RegistryAuthority> registry_authority;
  std::shared_ptr<ProcessAuthority> process_authority;
  std::shared_ptr<ResourceAuthority> resource_authority;
  std::shared_ptr<CopyPathStatistics> copy_path_statistics;
  try {
    registry_authority = std::make_shared<RegistryAuthority>();
    process_authority = std::make_shared<ProcessAuthority>(registry_authority);
    resource_authority = std::make_shared<ResourceAuthority>();
    copy_path_statistics = std::make_shared<CopyPathStatistics>();
  } catch (const std::bad_alloc&) {
    std::cerr << "metafluxd: registry authority allocation failed\n";
    return 1;
  }
  const mf_registry_view_id_v1 canonical_view_id{*incarnation, 1U};
  if (registry_authority->initialize(canonical_view_id) != MF_SHARED_SUCCESS) {
    std::cerr << "metafluxd: registry authority initialization failed\n";
    return 1;
  }
  while (shutdown_requested == 0) {
    std::erase_if(sessions, [](const ActiveSession& session) {
      return session.finished->load(std::memory_order_acquire);
    });
    pollfd descriptor{.fd = listener->fd(), .events = POLLIN, .revents = 0};
    int poll_result = -1;
    do {
      poll_result = poll(&descriptor, 1U, kAcceptPollMilliseconds);
    } while (poll_result < 0 && errno == EINTR && shutdown_requested == 0);
    if (poll_result < 0) {
      if (shutdown_requested != 0) {
        break;
      }
      std::cerr << "metafluxd: listener poll failed\n";
      return 1;
    }
    if (registry_authority->refresh_telemetry() != MF_SHARED_SUCCESS) {
      std::cerr << "metafluxd: telemetry refresh failed\n";
      return 1;
    }
    if (poll_result == 0) {
      continue;
    }
    if ((descriptor.revents & (POLLERR | POLLHUP | POLLNVAL)) != 0) {
      std::cerr << "metafluxd: listener became unusable\n";
      return 1;
    }
    if ((descriptor.revents & POLLIN) == 0) {
      continue;
    }

    for (;;) {
      std::erase_if(sessions, [](const ActiveSession& session) {
        return session.finished->load(std::memory_order_acquire);
      });
      UniqueFd peer(accept4(listener->fd(), nullptr, nullptr, SOCK_CLOEXEC | SOCK_NONBLOCK));
      if (!peer.valid()) {
        if (errno == EINTR) {
          continue;
        }
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
          break;
        }
        std::cerr << "metafluxd: accept failed\n";
        return 1;
      }
      ucred credentials{};
      socklen_t credentials_size = sizeof(credentials);
      if (getsockopt(peer.get(), SOL_SOCKET, SO_PEERCRED, &credentials, &credentials_size) != 0 ||
          credentials_size != sizeof(credentials) || credentials.pid <= 0 ||
          static_cast<std::uint64_t>(credentials.uid) > std::numeric_limits<std::uint32_t>::max()) {
        continue;
      }
      auto session_lease =
          SessionLease::acquire(resource_authority, static_cast<std::uint32_t>(credentials.uid));
      if (!session_lease.has_value()) {
        continue;
      }
      auto finished = std::make_shared<std::atomic_bool>(false);
      sessions.push_back(ActiveSession{
          .finished = finished,
          .thread = std::jthread([peer = std::move(peer), credentials, execution,
                                  registry_authority, process_authority, resource_authority,
                                  copy_path_statistics, session_lease = std::move(*session_lease),
                                  finished](std::stop_token stop_token) mutable {
            serve_peer(std::move(peer), credentials, std::move(execution),
                       std::move(registry_authority), std::move(process_authority),
                       std::move(resource_authority), std::move(copy_path_statistics),
                       std::move(session_lease), finished, stop_token);
          }),
      });
    }
  }

  sessions.clear();
  const auto statistics = execution->statistics();
  std::cerr
      << "metafluxd: cpu-execution mode=" << cpu_execution_mode_name(execution->mode())
      << " compiler-requests=" << statistics.compiler_requests
      << " cache-hits=" << statistics.cache_hits << " cache-misses=" << statistics.cache_misses
      << " loaded-modules=" << statistics.loaded_modules << " host-address-space-registrations="
      << copy_path_statistics->address_space_registrations.load(std::memory_order_relaxed)
      << " direct-host-source-operations="
      << copy_path_statistics->direct_host_source_operations.load(std::memory_order_relaxed)
      << " direct-host-source-bytes="
      << copy_path_statistics->direct_host_source_bytes.load(std::memory_order_relaxed)
      << " direct-host-destination-operations="
      << copy_path_statistics->direct_host_destination_operations.load(std::memory_order_relaxed)
      << " direct-host-destination-bytes="
      << copy_path_statistics->direct_host_destination_bytes.load(std::memory_order_relaxed)
      << " staged-host-source-operations="
      << copy_path_statistics->staged_host_source_operations.load(std::memory_order_relaxed)
      << " staged-host-source-bytes="
      << copy_path_statistics->staged_host_source_bytes.load(std::memory_order_relaxed)
      << " staged-host-destination-operations="
      << copy_path_statistics->staged_host_destination_operations.load(std::memory_order_relaxed)
      << " staged-host-destination-bytes="
      << copy_path_statistics->staged_host_destination_bytes.load(std::memory_order_relaxed)
      << '\n';
  return 0;
}

} // namespace metaflux::service
