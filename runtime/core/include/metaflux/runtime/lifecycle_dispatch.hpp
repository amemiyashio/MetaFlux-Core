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

} // namespace metaflux::runtime::lifecycle

#endif
