#include "metaflux/runtime/lifecycle_dispatch.hpp"

namespace metaflux::runtime::lifecycle {
namespace {

bool is_immediate_producer_kind(ExternalEventKind kind) noexcept {
  switch (kind) {
  case ExternalEventKind::AdminReset:
  case ExternalEventKind::VfioUserReset:
  case ExternalEventKind::Disconnect:
  case ExternalEventKind::MemfdDisconnect:
  case ExternalEventKind::CdevDisconnect:
  case ExternalEventKind::DaemonRestart:
    return true;
  case ExternalEventKind::AdminAdd:
  case ExternalEventKind::AdminRemove:
  case ExternalEventKind::QmpAdd:
  case ExternalEventKind::QmpRemove:
  case ExternalEventKind::QmpFailure:
    return false;
  }
  return false;
}

} // namespace

ProducerIngress::ProducerIngress(Coordinator& coordinator,
                                 std::uint64_t first_request_id) noexcept
    : coordinator_(coordinator), next_request_id_(first_request_id) {}

ExternalEvent ProducerIngress::capture(ExternalEventKind kind,
                                       std::uint64_t deadline_tick) noexcept {
  if (next_request_id_ == 0U) {
    return capture_external_event(kind, 0U, coordinator_.snapshot(), deadline_tick);
  }
  const std::uint64_t request_id = next_request_id_;
  if (next_request_id_ != std::numeric_limits<std::uint64_t>::max()) {
    ++next_request_id_;
  } else {
    next_request_id_ = 0U;
  }
  return capture_external_event(kind, request_id, coordinator_.snapshot(), deadline_tick);
}

NormalizationResult ProducerIngress::submit_immediate(ExternalEventKind kind,
                                                      std::uint64_t deadline_tick,
                                                      ResultDetails& out) noexcept {
  if (!is_immediate_producer_kind(kind) || next_request_id_ == 0U) {
    out = ResultDetails{};
    out.snapshot = coordinator_.snapshot();
    return next_request_id_ == 0U ? NormalizationResult::Invalid
                                  : NormalizationResult::Unsupported;
  }
  return submit_external_event(coordinator_, capture(kind, deadline_tick), out);
}

NormalizationResult submit_external_event(Coordinator& coordinator, const ExternalEvent& event,
                                          ResultDetails& out) noexcept {
  out = ResultDetails{};
  Request request{};
  const auto normalized = RequestNormalizer::normalize(event, request);
  if (normalized != NormalizationResult::Accepted) {
    out.result = Result::Invalid;
    out.snapshot = coordinator.snapshot();
    return normalized;
  }
  (void)coordinator.apply(request, out);
  return normalized;
}

NormalizationResult capture_and_submit_external_event(Coordinator& coordinator,
                                                      ExternalEventKind kind,
                                                      std::uint64_t request_id,
                                                      std::uint64_t deadline_tick,
                                                      ResultDetails& out) noexcept {
  if (!is_immediate_producer_kind(kind)) {
    out = ResultDetails{};
    out.snapshot = coordinator.snapshot();
    return NormalizationResult::Unsupported;
  }
  const ExternalEvent event =
      capture_external_event(kind, request_id, coordinator.snapshot(), deadline_tick);
  return submit_external_event(coordinator, event, out);
}

} // namespace metaflux::runtime::lifecycle
