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
