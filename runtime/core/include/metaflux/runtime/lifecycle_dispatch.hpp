#ifndef METAFLUX_RUNTIME_LIFECYCLE_DISPATCH_HPP
#define METAFLUX_RUNTIME_LIFECYCLE_DISPATCH_HPP

#include "metaflux/runtime/lifecycle.hpp"
#include "metaflux/runtime/lifecycle_normalizer.hpp"

namespace metaflux::runtime::lifecycle {

// Normalize one external event and submit the resulting request to the authority.
[[nodiscard]] NormalizationResult submit_external_event(Coordinator& coordinator,
                                                        const ExternalEvent& event,
                                                        ResultDetails& out) noexcept;

// Capture and submit one immediate reset/loss/restart producer event. Delayed or
// pre-correlated events retain the ExternalEvent overload above.
[[nodiscard]] NormalizationResult capture_and_submit_external_event(Coordinator& coordinator,
                                                                    ExternalEventKind kind,
                                                                    std::uint64_t request_id,
                                                                    std::uint64_t deadline_tick,
                                                                    ResultDetails& out) noexcept;

// Allocate request IDs and capture producer observations from one authority.
// Immediate producers can submit through the same object; correlated producers
// pass the captured event to their transport-specific completion adapter.
class ProducerIngress final {
public:
  explicit ProducerIngress(Coordinator& coordinator,
                           std::uint64_t first_request_id = 1U) noexcept;

  [[nodiscard]] ExternalEvent capture(ExternalEventKind kind,
                                      std::uint64_t deadline_tick = 0U) noexcept;
  [[nodiscard]] NormalizationResult submit_immediate(ExternalEventKind kind,
                                                      std::uint64_t deadline_tick,
                                                      ResultDetails& out) noexcept;

  [[nodiscard]] Coordinator& coordinator() noexcept { return coordinator_; }
  [[nodiscard]] std::uint64_t next_request_id() const noexcept { return next_request_id_; }

private:
  Coordinator& coordinator_;
  std::uint64_t next_request_id_ = 0U;
};

} // namespace metaflux::runtime::lifecycle

#endif
