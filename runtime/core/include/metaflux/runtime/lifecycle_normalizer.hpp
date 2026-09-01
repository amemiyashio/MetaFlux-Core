#ifndef METAFLUX_RUNTIME_LIFECYCLE_NORMALIZER_HPP
#define METAFLUX_RUNTIME_LIFECYCLE_NORMALIZER_HPP

#include <cstdint>

#include "metaflux/runtime/lifecycle.hpp"

namespace metaflux::runtime::lifecycle {

enum class ExternalEventKind : std::uint8_t {
  AdminAdd = 1,
  AdminRemove = 2,
  AdminReset = 3,
  VfioUserReset = 4,
  QmpAdd = 5,
  QmpRemove = 6,
  QmpFailure = 7,
  Disconnect = 8,
  DaemonRestart = 9,
  MemfdDisconnect = 10,
  CdevDisconnect = 11,
};

enum class NormalizationResult : std::uint8_t {
  Accepted = 0,
  Invalid = 1,
  Unsupported = 2,
};

struct ExternalEvent final {
  std::uint64_t request_id = 0U;
  std::uint64_t logical_device_id = 0U;
  std::uint64_t daemon_incarnation = 0U;
  std::uint64_t expected_identity_record_id = 0U;
  std::uint64_t expected_generation = 0U;
  std::uint64_t expected_epoch = 0U;
  std::uint64_t deadline_tick = 0U;
  ExternalEventKind kind = ExternalEventKind::AdminAdd;
};

// Capture the authority tuple at the point an external producer observes an event.
[[nodiscard]] ExternalEvent capture_external_event(ExternalEventKind kind, std::uint64_t request_id,
                                                   const Snapshot& snapshot,
                                                   std::uint64_t deadline_tick = 0U) noexcept;

class RequestNormalizer final {
public:
  [[nodiscard]] static NormalizationResult normalize(const ExternalEvent& event,
                                                     Request& out) noexcept;
};

} // namespace metaflux::runtime::lifecycle

#endif
