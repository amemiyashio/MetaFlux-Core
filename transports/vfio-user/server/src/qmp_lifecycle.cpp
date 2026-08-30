#include "metaflux/transport/qmp_lifecycle.hpp"

#include "metaflux/runtime/lifecycle_dispatch.hpp"

namespace metaflux::transport::vfio_user {

using metaflux::runtime::lifecycle::ExternalEventKind;
using metaflux::runtime::lifecycle::NormalizationResult;
using metaflux::runtime::lifecycle::RequestNormalizer;

QmpCommand QmpCommand::from_snapshot(std::uint64_t command_id, ExternalEventKind kind,
                                     std::uint64_t request_id,
                                     const metaflux::runtime::lifecycle::Snapshot& snapshot,
                                     std::uint64_t deadline_tick) noexcept {
  return QmpCommand{
      .command_id = command_id,
      .event = metaflux::runtime::lifecycle::capture_external_event(kind, request_id, snapshot,
                                                                    deadline_tick),
  };
}

void QmpLifecycleAdapter::clear_pending() noexcept {
  pending_ = false;
  pending_command_id_ = 0U;
  expected_reply_ = QmpReplyKind::DeviceAdded;
  pending_event_ = {};
  pending_request_ = {};
}

QmpResult QmpLifecycleAdapter::begin(const QmpCommand& command) noexcept {
  if (pending_) {
    return QmpResult::Busy;
  }
  if (command.command_id == 0U || (command.event.kind != ExternalEventKind::QmpAdd &&
                                   command.event.kind != ExternalEventKind::QmpRemove)) {
    return QmpResult::Invalid;
  }

  metaflux::runtime::lifecycle::Request normalized{};
  if (RequestNormalizer::normalize(command.event, normalized) != NormalizationResult::Accepted) {
    return QmpResult::Invalid;
  }

  pending_ = true;
  pending_command_id_ = command.command_id;
  expected_reply_ = command.event.kind == ExternalEventKind::QmpAdd ? QmpReplyKind::DeviceAdded
                                                                    : QmpReplyKind::DeviceDeleted;
  pending_event_ = command.event;
  pending_request_ = normalized;
  return QmpResult::Accepted;
}

QmpResult QmpLifecycleAdapter::complete(const QmpReply& reply,
                                        metaflux::runtime::lifecycle::Request& out) noexcept {
  out = {};
  if (!pending_ || reply.command_id == 0U || reply.command_id != pending_command_id_) {
    return pending_ ? QmpResult::Pending : QmpResult::Invalid;
  }
  if (reply.kind == QmpReplyKind::CommandFailed) {
    if (pending_event_.kind == ExternalEventKind::QmpAdd) {
      clear_pending();
      return QmpResult::Failed;
    }
    auto failed_event = pending_event_;
    failed_event.kind = ExternalEventKind::QmpFailure;
    const auto result = RequestNormalizer::normalize(failed_event, out);
    clear_pending();
    return result == NormalizationResult::Accepted ? QmpResult::Accepted : QmpResult::Invalid;
  }
  if (reply.kind != expected_reply_) {
    return QmpResult::Pending;
  }
  out = pending_request_;
  clear_pending();
  return QmpResult::Accepted;
}

QmpResult QmpLifecycleAdapter::complete_and_submit(
    const QmpReply& reply, metaflux::runtime::lifecycle::Coordinator& coordinator,
    metaflux::runtime::lifecycle::ResultDetails& out) noexcept {
  out = metaflux::runtime::lifecycle::ResultDetails{};
  if (!pending_) {
    out.result = metaflux::runtime::lifecycle::Result::Invalid;
    out.snapshot = coordinator.snapshot();
    return QmpResult::Invalid;
  }
  auto event = pending_event_;
  metaflux::runtime::lifecycle::Request request{};
  const QmpResult result = complete(reply, request);
  if (result != QmpResult::Accepted) {
    out.result = metaflux::runtime::lifecycle::Result::Invalid;
    out.snapshot = coordinator.snapshot();
    return result;
  }
  if (reply.kind == QmpReplyKind::CommandFailed && event.kind == ExternalEventKind::QmpRemove) {
    event.kind = ExternalEventKind::QmpFailure;
  }
  if (submit_external_event(coordinator, event, out) != NormalizationResult::Accepted) {
    out.result = metaflux::runtime::lifecycle::Result::Invalid;
    out.snapshot = coordinator.snapshot();
    return QmpResult::Invalid;
  }
  return QmpResult::Accepted;
}

} // namespace metaflux::transport::vfio_user
