#include "metaflux/runtime/lifecycle_dispatch.hpp"

namespace metaflux::runtime::lifecycle {

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

} // namespace metaflux::runtime::lifecycle
