#ifndef METAFLUX_TRANSPORT_QMP_LIFECYCLE_HPP
#define METAFLUX_TRANSPORT_QMP_LIFECYCLE_HPP

#include <cstdint>

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
};

struct QmpReply final {
  std::uint64_t command_id = 0U;
  QmpReplyKind kind = QmpReplyKind::DeviceAdded;
};

class QmpLifecycleAdapter final {
public:
  [[nodiscard]] QmpResult begin(const QmpCommand& command) noexcept;
  [[nodiscard]] QmpResult complete(const QmpReply& reply,
                                   metaflux::runtime::lifecycle::Request& out) noexcept;
  [[nodiscard]] QmpResult
  complete_and_submit(const QmpReply& reply, metaflux::runtime::lifecycle::Coordinator& coordinator,
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
