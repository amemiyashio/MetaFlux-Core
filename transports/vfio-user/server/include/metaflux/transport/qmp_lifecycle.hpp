#ifndef METAFLUX_TRANSPORT_QMP_LIFECYCLE_HPP
#define METAFLUX_TRANSPORT_QMP_LIFECYCLE_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

#include "metaflux/runtime/lifecycle_dispatch.hpp"

namespace metaflux::transport::vfio_user {

enum class QmpReplyKind : std::uint8_t {
  DeviceAdded = 1,
  DeviceDeleted = 2,
  CommandFailed = 3,
};

enum class QmpResult : std::uint8_t {
  Accepted = 0,
  Invalid = 1,
  Busy = 2,
  Pending = 3,
  Failed = 4,
};

struct QmpCommand final {
  std::uint64_t command_id = 0U;
  metaflux::runtime::lifecycle::ExternalEvent event{};

  [[nodiscard]] static QmpCommand
  from_snapshot(std::uint64_t command_id, metaflux::runtime::lifecycle::ExternalEventKind kind,
                std::uint64_t request_id, const metaflux::runtime::lifecycle::Snapshot& snapshot,
                std::uint64_t deadline_tick = 0U) noexcept;
  [[nodiscard]] static QmpCommand
  from_ingress(std::uint64_t command_id, metaflux::runtime::lifecycle::ExternalEventKind kind,
               metaflux::runtime::lifecycle::ProducerIngress& ingress,
               std::uint64_t deadline_tick = 0U) noexcept;
};

struct QmpReply final {
  std::uint64_t command_id = 0U;
  QmpReplyKind kind = QmpReplyKind::DeviceAdded;
};

enum class QmpWireKind : std::uint8_t {
  Greeting = 0,
  Reply = 1,
  DeviceAdded = 2,
  DeviceDeleted = 3,
  CommandFailed = 4,
  OtherEvent = 5,
};

struct QmpWireMessage final {
  QmpWireKind kind = QmpWireKind::OtherEvent;
  std::uint64_t command_id = 0U;
};

enum class QmpSocketResult : std::uint8_t {
  Ok = 0,
  Invalid = 1,
  WouldBlock = 2,
  Timeout = 3,
  Malformed = 4,
  Closed = 5,
  IoError = 6,
  Unexpected = 7,
};

struct QmpLifecycleSocketOutcome final {
  QmpSocketResult transport = QmpSocketResult::Invalid;
  QmpResult lifecycle = QmpResult::Invalid;
};

class QmpSocket final {
public:
  static constexpr std::size_t kMaximumMessageBytes = 64U * 1024U;

  explicit QmpSocket(int fd = -1) noexcept : fd_(fd) {}
  ~QmpSocket();

  QmpSocket(const QmpSocket&) = delete;
  QmpSocket& operator=(const QmpSocket&) = delete;
  QmpSocket(QmpSocket&& other) noexcept;
  QmpSocket& operator=(QmpSocket&& other) noexcept;

  [[nodiscard]] static QmpSocketResult connect(const char* path, QmpSocket& out,
                                               std::uint32_t timeout_ms = 1000U) noexcept;
  [[nodiscard]] QmpSocketResult send_command(std::uint64_t command_id, std::string_view execute,
                                             std::string_view arguments = "{}") noexcept;
  [[nodiscard]] QmpSocketResult receive(QmpWireMessage& out) noexcept;
  [[nodiscard]] QmpSocketResult receive_lifecycle_reply(std::uint64_t pending_command_id,
                                                        QmpReply& out) noexcept;

  void close() noexcept;
  [[nodiscard]] bool is_open() const noexcept { return fd_ >= 0; }
  [[nodiscard]] int fd() const noexcept { return fd_; }

private:
  [[nodiscard]] QmpSocketResult set_timeout(std::uint32_t timeout_ms) noexcept;
  [[nodiscard]] QmpSocketResult send_bytes(const char* bytes, std::size_t size) noexcept;
  [[nodiscard]] QmpSocketResult read_json_object(std::array<char, kMaximumMessageBytes>& object,
                                                 std::size_t& size) noexcept;
  [[nodiscard]] static QmpSocketResult classify_json(const char* bytes, std::size_t size,
                                                     QmpWireMessage& out) noexcept;

  int fd_ = -1;
};

class QmpLifecycleAdapter final {
public:
  [[nodiscard]] QmpResult begin(const QmpCommand& command) noexcept;
  [[nodiscard]] QmpResult complete(const QmpReply& reply,
                                   metaflux::runtime::lifecycle::Request& out) noexcept;
  [[nodiscard]] QmpResult
  complete_and_submit(const QmpReply& reply, metaflux::runtime::lifecycle::Coordinator& coordinator,
                      metaflux::runtime::lifecycle::ResultDetails& out) noexcept;
  [[nodiscard]] QmpLifecycleSocketOutcome
  receive_and_submit(QmpSocket& socket,
                     metaflux::runtime::lifecycle::Coordinator& coordinator,
                     metaflux::runtime::lifecycle::ResultDetails& out) noexcept;

  [[nodiscard]] bool pending() const noexcept { return pending_; }
  [[nodiscard]] std::uint64_t pending_command_id() const noexcept {
    return pending_ ? pending_command_id_ : 0U;
  }

private:
  void clear_pending() noexcept;

  bool pending_ = false;
  std::uint64_t pending_command_id_ = 0U;
  QmpReplyKind expected_reply_ = QmpReplyKind::DeviceAdded;
  metaflux::runtime::lifecycle::ExternalEvent pending_event_{};
  metaflux::runtime::lifecycle::Request pending_request_{};
};

} // namespace metaflux::transport::vfio_user

#endif
