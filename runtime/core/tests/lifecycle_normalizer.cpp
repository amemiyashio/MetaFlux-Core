#include "metaflux/runtime/lifecycle_normalizer.hpp"

#include <array>
#include <cstdint>
#include <iostream>

using metaflux::runtime::lifecycle::ExternalEvent;
using metaflux::runtime::lifecycle::ExternalEventKind;
using metaflux::runtime::lifecycle::NormalizationResult;
using metaflux::runtime::lifecycle::Operation;
using metaflux::runtime::lifecycle::Request;
using metaflux::runtime::lifecycle::RequestNormalizer;
using metaflux::runtime::lifecycle::Source;

#define REQUIRE(condition)                                                                         \
  do {                                                                                             \
    if (!(condition)) {                                                                            \
      std::cerr << __func__ << ':' << __LINE__ << ": " #condition "\n";                         \
      return false;                                                                                \
    }                                                                                              \
  } while (false)

namespace {

struct ExpectedMapping final {
  ExternalEventKind kind;
  Source source;
  Operation operation;
  bool add;
};

bool maps_fixed_external_events() {
  constexpr std::array<ExpectedMapping, 9> expected{{
      {ExternalEventKind::AdminAdd, Source::Admin, Operation::Add, true},
      {ExternalEventKind::AdminRemove, Source::Admin, Operation::Remove, false},
      {ExternalEventKind::AdminReset, Source::Admin, Operation::Reset, false},
      {ExternalEventKind::VfioUserReset, Source::VfioUser, Operation::Reset, false},
      {ExternalEventKind::QmpAdd, Source::Qmp, Operation::Add, true},
      {ExternalEventKind::QmpRemove, Source::Qmp, Operation::Remove, false},
      {ExternalEventKind::QmpFailure, Source::Qmp, Operation::TransportLoss, false},
      {ExternalEventKind::Disconnect, Source::Disconnect, Operation::TransportLoss, false},
      {ExternalEventKind::DaemonRestart, Source::Restart, Operation::Recover, false},
  }};

  for (const ExpectedMapping& mapping : expected) {
    const ExternalEvent event{
        .request_id = 41U,
        .logical_device_id = 7U,
        .daemon_incarnation = 11U,
        .expected_identity_record_id = mapping.add ? 0U : 3U,
        .expected_generation = mapping.add ? 0U : 3U,
        .expected_epoch = 2U,
        .deadline_tick = 99U,
        .kind = mapping.kind,
    };
    Request request{};
    REQUIRE(RequestNormalizer::normalize(event, request) == NormalizationResult::Accepted);
    REQUIRE(request.request_id == event.request_id);
    REQUIRE(request.logical_device_id == event.logical_device_id);
    REQUIRE(request.daemon_incarnation == event.daemon_incarnation);
    REQUIRE(request.expected_identity_record_id == event.expected_identity_record_id);
    REQUIRE(request.expected_generation == event.expected_generation);
    REQUIRE(request.expected_epoch == event.expected_epoch);
    REQUIRE(request.deadline_tick == event.deadline_tick);
    REQUIRE(request.source == mapping.source && request.operation == mapping.operation);
  }
  return true;
}

bool rejects_malformed_and_unknown_events() {
  Request request{
      .request_id = 99U,
      .logical_device_id = 99U,
      .daemon_incarnation = 99U,
      .expected_identity_record_id = 99U,
      .expected_generation = 99U,
      .expected_epoch = 99U,
  };
  ExternalEvent malformed{
      .request_id = 0U,
      .logical_device_id = 7U,
      .daemon_incarnation = 11U,
      .expected_epoch = 2U,
      .kind = ExternalEventKind::AdminAdd,
  };
  REQUIRE(RequestNormalizer::normalize(malformed, request) == NormalizationResult::Invalid);
  REQUIRE(request.request_id == 0U && request.expected_epoch == 0U);

  malformed.request_id = 41U;
  malformed.expected_generation = 3U;
  malformed.expected_identity_record_id = 3U;
  REQUIRE(RequestNormalizer::normalize(malformed, request) == NormalizationResult::Invalid);
  REQUIRE(request.request_id == 0U);

  malformed.kind = static_cast<ExternalEventKind>(255U);
  malformed.expected_generation = 0U;
  malformed.expected_identity_record_id = 0U;
  REQUIRE(RequestNormalizer::normalize(malformed, request) == NormalizationResult::Unsupported);
  REQUIRE(request.request_id == 0U);
  return true;
}

} // namespace

int main() { return maps_fixed_external_events() && rejects_malformed_and_unknown_events() ? 0 : 1; }
