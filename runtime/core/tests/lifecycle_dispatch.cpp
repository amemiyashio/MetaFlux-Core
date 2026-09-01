#include "metaflux/runtime/lifecycle_dispatch.hpp"

#include <cstdint>
#include <iostream>

using metaflux::runtime::lifecycle::capture_and_submit_external_event;
using metaflux::runtime::lifecycle::capture_external_event;
using metaflux::runtime::lifecycle::Config;
using metaflux::runtime::lifecycle::Coordinator;
using metaflux::runtime::lifecycle::ExternalEvent;
using metaflux::runtime::lifecycle::ExternalEventKind;
using metaflux::runtime::lifecycle::NormalizationResult;
using metaflux::runtime::lifecycle::Result;
using metaflux::runtime::lifecycle::ResultDetails;
using metaflux::runtime::lifecycle::Snapshot;
using metaflux::runtime::lifecycle::State;
using metaflux::runtime::lifecycle::submit_external_event;

#define REQUIRE(condition)                                                                         \
  do {                                                                                             \
    if (!(condition)) {                                                                            \
      std::cerr << __func__ << ':' << __LINE__ << ": " #condition "\n";                            \
      return false;                                                                                \
    }                                                                                              \
  } while (false)

namespace {

Coordinator absent_coordinator() {
  Config config{};
  config.logical_device_id = 7U;
  config.daemon_incarnation = 11U;
  config.initial_identity_record_id = 0U;
  config.initial_generation = 0U;
  config.initial_epoch = 1U;
  config.initial_state = State::Absent;
  config.identity_record_terminal = 32U;
  config.generation_terminal = 32U;
  config.epoch_terminal = 32U;
  return Coordinator(config);
}

Coordinator online_coordinator() {
  Config config{};
  config.logical_device_id = 7U;
  config.daemon_incarnation = 11U;
  config.initial_identity_record_id = 1U;
  config.initial_generation = 1U;
  config.initial_epoch = 1U;
  config.initial_state = State::Online;
  config.identity_record_terminal = 32U;
  config.generation_terminal = 32U;
  config.epoch_terminal = 32U;
  return Coordinator(config);
}

ExternalEvent add_event(std::uint64_t request_id) {
  return ExternalEvent{
      .request_id = request_id,
      .logical_device_id = 7U,
      .daemon_incarnation = 11U,
      .expected_epoch = 1U,
      .kind = ExternalEventKind::AdminAdd,
  };
}

bool rejects_before_authority_submission() {
  Coordinator coordinator = absent_coordinator();
  ResultDetails details{
      .result = Result::Accepted,
      .candidate_generation = 99U,
  };
  ExternalEvent malformed = add_event(0U);
  REQUIRE(submit_external_event(coordinator, malformed, details) == NormalizationResult::Invalid);
  REQUIRE(details.result == Result::Invalid && details.candidate_generation == 0U);
  REQUIRE(details.snapshot.state == State::Absent);

  malformed.request_id = 1U;
  malformed.kind = static_cast<ExternalEventKind>(255U);
  REQUIRE(submit_external_event(coordinator, malformed, details) ==
          NormalizationResult::Unsupported);
  REQUIRE(details.result == Result::Invalid && details.snapshot.state == State::Absent);
  return true;
}

bool submits_only_normalized_requests_and_replays_idempotently() {
  Coordinator coordinator = absent_coordinator();
  ResultDetails details{};
  const ExternalEvent event = add_event(41U);
  REQUIRE(submit_external_event(coordinator, event, details) == NormalizationResult::Accepted);
  REQUIRE(details.result == Result::Accepted);
  REQUIRE(details.snapshot.state == State::Online && details.snapshot.generation == 1U);
  REQUIRE(details.candidate_generation == 1U);

  REQUIRE(submit_external_event(coordinator, event, details) == NormalizationResult::Accepted);
  REQUIRE(details.result == Result::Duplicate);
  REQUIRE(details.snapshot.state == State::Online && details.snapshot.generation == 1U);
  return true;
}

bool captures_and_submits_immediate_producers() {
  constexpr std::uint64_t deadline = 77U;

  for (const ExternalEventKind kind :
       {ExternalEventKind::AdminReset, ExternalEventKind::VfioUserReset}) {
    Coordinator coordinator = online_coordinator();
    ResultDetails details{};
    REQUIRE(capture_and_submit_external_event(coordinator, kind, 51U, deadline, details) ==
            NormalizationResult::Accepted);
    REQUIRE(details.result == Result::Accepted && details.snapshot.state == State::Online &&
            details.snapshot.generation == 2U && details.snapshot.epoch == 2U &&
            details.candidate_generation == 2U);
  }

  for (const ExternalEventKind kind : {ExternalEventKind::Disconnect,
                                       ExternalEventKind::MemfdDisconnect}) {
    Coordinator coordinator = online_coordinator();
    ResultDetails details{};
    REQUIRE(capture_and_submit_external_event(coordinator, kind, 61U, deadline, details) ==
            NormalizationResult::Accepted);
    REQUIRE(details.result == Result::Accepted && details.snapshot.state == State::Lost &&
            details.snapshot.generation == 1U && details.snapshot.epoch == 1U);
  }

  Coordinator coordinator = online_coordinator();
  ResultDetails details{};
  REQUIRE(capture_and_submit_external_event(coordinator, ExternalEventKind::CdevDisconnect, 61U,
                                            deadline, details) == NormalizationResult::Accepted);
  REQUIRE(details.result == Result::Accepted && details.snapshot.state == State::Lost &&
          details.snapshot.generation == 1U && details.snapshot.epoch == 1U);
  REQUIRE(capture_and_submit_external_event(coordinator, ExternalEventKind::DaemonRestart, 62U,
                                            deadline, details) == NormalizationResult::Accepted);
  REQUIRE(details.result == Result::Accepted && details.snapshot.state == State::Online &&
          details.snapshot.generation == 2U && details.snapshot.epoch == 2U &&
          details.candidate_generation == 2U);

  const Snapshot before = coordinator.snapshot();
  REQUIRE(capture_and_submit_external_event(coordinator, ExternalEventKind::AdminReset, 0U,
                                            deadline, details) == NormalizationResult::Invalid);
  REQUIRE(details.result == Result::Invalid);
  const Snapshot after_malformed = coordinator.snapshot();
  REQUIRE(after_malformed.state == before.state &&
          after_malformed.generation == before.generation && after_malformed.epoch == before.epoch);

  REQUIRE(capture_and_submit_external_event(coordinator, ExternalEventKind::QmpRemove, 63U,
                                            deadline, details) == NormalizationResult::Unsupported);
  REQUIRE(details.result == Result::Invalid);
  REQUIRE(capture_and_submit_external_event(coordinator, static_cast<ExternalEventKind>(255U), 64U,
                                            deadline, details) == NormalizationResult::Unsupported);
  REQUIRE(details.result == Result::Invalid);
  const Snapshot after = coordinator.snapshot();
  REQUIRE(after.state == before.state && after.generation == before.generation &&
          after.epoch == before.epoch);
  return true;
}

bool preserves_a_stale_observation_instead_of_recapturing() {
  Coordinator coordinator = online_coordinator();
  const ExternalEvent observed =
      capture_external_event(ExternalEventKind::Disconnect, 71U, coordinator.snapshot(), 77U);
  ResultDetails details{};
  REQUIRE(capture_and_submit_external_event(coordinator, ExternalEventKind::AdminReset, 72U, 77U,
                                            details) == NormalizationResult::Accepted);
  REQUIRE(details.result == Result::Accepted && details.snapshot.generation == 2U &&
          details.snapshot.epoch == 2U);

  REQUIRE(submit_external_event(coordinator, observed, details) == NormalizationResult::Accepted);
  REQUIRE(details.result == Result::Stale && details.snapshot.state == State::Online &&
          details.snapshot.generation == 2U && details.snapshot.epoch == 2U);
  return true;
}

} // namespace

int main() {
  return rejects_before_authority_submission() &&
                 submits_only_normalized_requests_and_replays_idempotently() &&
                 captures_and_submits_immediate_producers() &&
                 preserves_a_stale_observation_instead_of_recapturing()
             ? 0
             : 1;
}
