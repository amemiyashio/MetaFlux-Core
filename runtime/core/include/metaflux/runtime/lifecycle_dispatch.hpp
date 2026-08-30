#ifndef METAFLUX_RUNTIME_LIFECYCLE_DISPATCH_HPP
#define METAFLUX_RUNTIME_LIFECYCLE_DISPATCH_HPP

#include "metaflux/runtime/lifecycle.hpp"
#include "metaflux/runtime/lifecycle_normalizer.hpp"

namespace metaflux::runtime::lifecycle {

// Normalize one external event and submit the resulting request to the authority.
[[nodiscard]] NormalizationResult submit_external_event(Coordinator& coordinator,
                                                         const ExternalEvent& event,
                                                         ResultDetails& out) noexcept;

} // namespace metaflux::runtime::lifecycle

#endif
