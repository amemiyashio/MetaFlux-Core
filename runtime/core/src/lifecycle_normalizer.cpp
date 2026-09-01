#include "metaflux/runtime/lifecycle_normalizer.hpp"

namespace metaflux::runtime::lifecycle {
namespace {

struct Mapping final {
  Source source;
  Operation operation;
  bool add;
};

bool mapping_for(ExternalEventKind kind, Mapping& out) noexcept {
  switch (kind) {
  case ExternalEventKind::AdminAdd:
    out = {Source::Admin, Operation::Add, true};
    return true;
  case ExternalEventKind::AdminRemove:
    out = {Source::Admin, Operation::Remove, false};
    return true;
  case ExternalEventKind::AdminReset:
    out = {Source::Admin, Operation::Reset, false};
    return true;
  case ExternalEventKind::VfioUserReset:
    out = {Source::VfioUser, Operation::Reset, false};
    return true;
  case ExternalEventKind::QmpAdd:
    out = {Source::Qmp, Operation::Add, true};
    return true;
  case ExternalEventKind::QmpRemove:
    out = {Source::Qmp, Operation::Remove, false};
    return true;
  case ExternalEventKind::QmpFailure:
    out = {Source::Qmp, Operation::TransportLoss, false};
    return true;
  case ExternalEventKind::Disconnect:
    out = {Source::Disconnect, Operation::TransportLoss, false};
    return true;
  case ExternalEventKind::MemfdDisconnect:
    out = {Source::Memfd, Operation::TransportLoss, false};
    return true;
  case ExternalEventKind::CdevDisconnect:
    out = {Source::Cdev, Operation::TransportLoss, false};
    return true;
  case ExternalEventKind::DaemonRestart:
    out = {Source::Restart, Operation::Recover, false};
    return true;
  }
  return false;
}

} // namespace

ExternalEvent capture_external_event(ExternalEventKind kind, std::uint64_t request_id,
                                     const Snapshot& snapshot,
                                     std::uint64_t deadline_tick) noexcept {
  return ExternalEvent{
      .request_id = request_id,
      .logical_device_id = snapshot.logical_device_id,
      .daemon_incarnation = snapshot.daemon_incarnation,
      .expected_identity_record_id = snapshot.identity_record_id,
      .expected_generation = snapshot.generation,
      .expected_epoch = snapshot.epoch,
      .deadline_tick = deadline_tick,
      .kind = kind,
  };
}

NormalizationResult RequestNormalizer::normalize(const ExternalEvent& event,
                                                 Request& out) noexcept {
  out = Request{};
  Mapping mapping{};
  if (!mapping_for(event.kind, mapping)) {
    return NormalizationResult::Unsupported;
  }
  if (event.request_id == 0U || event.logical_device_id == 0U || event.daemon_incarnation == 0U ||
      event.expected_epoch == 0U ||
      (mapping.add &&
       (event.expected_identity_record_id != 0U || event.expected_generation != 0U)) ||
      (!mapping.add &&
       (event.expected_identity_record_id == 0U || event.expected_generation == 0U))) {
    return NormalizationResult::Invalid;
  }

  out = Request{
      .request_id = event.request_id,
      .logical_device_id = event.logical_device_id,
      .daemon_incarnation = event.daemon_incarnation,
      .expected_identity_record_id = event.expected_identity_record_id,
      .expected_generation = event.expected_generation,
      .expected_epoch = event.expected_epoch,
      .deadline_tick = event.deadline_tick,
      .source = mapping.source,
      .operation = mapping.operation,
  };
  return NormalizationResult::Accepted;
}

} // namespace metaflux::runtime::lifecycle
