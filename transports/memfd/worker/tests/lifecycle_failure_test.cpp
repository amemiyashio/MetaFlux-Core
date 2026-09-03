#include "metaflux/transport/memfd_worker.hpp"

#include <cstdint>
#include <iostream>

using metaflux::runtime::lifecycle::Config;
using metaflux::runtime::lifecycle::Coordinator;
using metaflux::runtime::lifecycle::NormalizationResult;
using metaflux::runtime::lifecycle::Operation;
using metaflux::runtime::lifecycle::Request;
using metaflux::runtime::lifecycle::Result;
using metaflux::runtime::lifecycle::ResultDetails;
using metaflux::runtime::lifecycle::Source;
using metaflux::transport::memfd::MemfdWorker;
using metaflux::transport::memfd::SubmitResult;

#define REQUIRE(condition)                                                                         \
  do {                                                                                             \
    if (!(condition)) {                                                                            \
      std::cerr << __func__ << ':' << __LINE__ << ": " #condition "\n";                         \
      return false;                                                                                \
    }                                                                                              \
  } while (false)

namespace {

Request make_request(std::uint64_t id, Operation operation, std::uint64_t identity_record_id,
                    std::uint64_t generation, std::uint64_t epoch, Source source = Source::Memfd) {
  return Request{
      .request_id = id,
      .logical_device_id = 7U,
      .daemon_incarnation = 11U,
      .expected_identity_record_id = identity_record_id,
      .expected_generation = generation,
      .expected_epoch = epoch,
      .source = source,
      .operation = operation,
  };
}

Coordinator make_coordinator() {
  Config config{};
  config.logical_device_id = 7U;
  config.daemon_incarnation = 11U;
  config.initial_identity_record_id = 1U;
  config.initial_generation = 1U;
  config.initial_epoch = 1U;
  config.generation_terminal = 32U;
  config.identity_record_terminal = 32U;
  config.epoch_terminal = 32U;
  return Coordinator(config);
}

// Each failed Reset still reserves a candidate via reserve_candidate(), advancing
// generation_high_water.  Two failed attempts + one successful Reset therefore
// yields identity=4, generation=4 (not 3, 3).
bool drain_blocks_until_all_in_flight_complete() {
  MemfdWorker worker;
  Coordinator lifecycle = make_coordinator();
  REQUIRE(worker.attach_lifecycle(lifecycle));
  REQUIRE(worker.submit(1U) == SubmitResult::Accepted);
  REQUIRE(worker.submit(1U) == SubmitResult::Accepted);
  REQUIRE(worker.submit(1U) == SubmitResult::Accepted);
  REQUIRE(worker.in_flight() == 3U);

  // Drain can't complete with 3 in-flight.
  REQUIRE(lifecycle.apply(make_request(1U, Operation::Reset, 1U, 1U, 1U)) ==
          Result::CallbackRejected);
  REQUIRE(worker.lifecycle_online() && worker.lifecycle_accepting());
  REQUIRE(worker.in_flight() == 3U);

  // Complete one -- still 2 in-flight, drain still blocked.
  REQUIRE(worker.complete_one());
  REQUIRE(lifecycle.apply(make_request(2U, Operation::Reset, 1U, 1U, 1U)) ==
          Result::CallbackRejected);
  REQUIRE(worker.in_flight() == 2U);

  // Complete remaining two -- drain succeeds.
  REQUIRE(worker.complete_one());
  REQUIRE(worker.complete_one());
  REQUIRE(worker.in_flight() == 0U);
  REQUIRE(lifecycle.apply(make_request(3U, Operation::Reset, 1U, 1U, 1U)) == Result::Accepted);
  REQUIRE(worker.identity_record_id() == 4U && worker.generation() == 4U && worker.epoch() == 2U);
  return true;
}

bool transport_loss_during_drain_transitions_to_lost() {
  MemfdWorker worker;
  Coordinator lifecycle = make_coordinator();
  REQUIRE(worker.attach_lifecycle(lifecycle));
  REQUIRE(worker.submit(1U) == SubmitResult::Accepted);
  REQUIRE(worker.submit(1U) == SubmitResult::Accepted);
  REQUIRE(worker.in_flight() == 2U);

  // Drain blocked by in-flight.
  REQUIRE(lifecycle.apply(make_request(1U, Operation::Reset, 1U, 1U, 1U)) ==
          Result::CallbackRejected);
  REQUIRE(worker.lifecycle_online());

  // Transport loss while drain is blocked.
  ResultDetails details{};
  REQUIRE(worker.report_disconnect(lifecycle, 2U, 19U, details) == NormalizationResult::Accepted);
  REQUIRE(details.snapshot.state == metaflux::runtime::lifecycle::State::Lost);
  REQUIRE(!worker.lifecycle_online() && !worker.lifecycle_accepting());
  return true;
}

bool stale_generation_rejected_after_lifecycle_commit() {
  MemfdWorker worker;
  Coordinator lifecycle = make_coordinator();
  REQUIRE(worker.attach_lifecycle(lifecycle));

  // Successful Reset: submit and complete in-flight first.
  REQUIRE(worker.submit(1U) == SubmitResult::Accepted);
  REQUIRE(worker.complete_one());
  REQUIRE(lifecycle.apply(make_request(1U, Operation::Reset, 1U, 1U, 1U)) == Result::Accepted);
  REQUIRE(worker.generation() == 2U);

  // Old generation rejected, new generation accepted.
  REQUIRE(worker.submit(1U) == SubmitResult::StaleGeneration);
  REQUIRE(worker.submit(2U) == SubmitResult::Accepted);
  REQUIRE(worker.complete_one());
  return true;
}

bool consecutive_resets_advance_generation_correctly() {
  MemfdWorker worker;
  Coordinator lifecycle = make_coordinator();
  REQUIRE(worker.attach_lifecycle(lifecycle));

  std::uint64_t prev_gen = worker.generation();
  std::uint64_t prev_epoch = worker.epoch();

  for (std::uint64_t i = 0; i < 3; ++i) {
    REQUIRE(worker.submit(worker.generation()) == SubmitResult::Accepted);
    REQUIRE(worker.complete_one());
    REQUIRE(lifecycle.apply(make_request(i * 2 + 1U, Operation::Reset,
                                         worker.identity_record_id(),
                                         worker.generation(), worker.epoch())) == Result::Accepted);
    REQUIRE(worker.generation() > prev_gen);
    REQUIRE(worker.epoch() > prev_epoch);
    prev_gen = worker.generation();
    prev_epoch = worker.epoch();
  }
  return true;
}

// apply_remove forces the Absent transition even when the drain callback
// rejects (the authority retires the identity regardless).  The result is
// CallbackRejected but the worker transitions offline.
bool remove_forces_absent_despite_drain_failure() {
  MemfdWorker worker;
  Coordinator lifecycle = make_coordinator();
  REQUIRE(worker.attach_lifecycle(lifecycle));
  REQUIRE(worker.submit(1U) == SubmitResult::Accepted);
  REQUIRE(worker.in_flight() == 1U);

  // Remove with in-flight: drain rejects, but authority forces Absent.
  REQUIRE(lifecycle.apply(make_request(1U, Operation::Remove, 1U, 1U, 1U, Source::Admin)) ==
          Result::CallbackRejected);
  REQUIRE(!worker.lifecycle_online() && !worker.lifecycle_accepting());
  return true;
}

} // namespace

int main() {
  return drain_blocks_until_all_in_flight_complete() &&
                 transport_loss_during_drain_transitions_to_lost() &&
                 stale_generation_rejected_after_lifecycle_commit() &&
                 consecutive_resets_advance_generation_correctly() &&
                 remove_forces_absent_despite_drain_failure()
             ? 0
             : 1;
}
