#include "metaflux/transport/memfd_worker.hpp"

#include <cstdint>
#include <iostream>

using metaflux::runtime::lifecycle::Config;
using metaflux::runtime::lifecycle::Coordinator;
using metaflux::runtime::lifecycle::Operation;
using metaflux::runtime::lifecycle::NormalizationResult;
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

Request request(std::uint64_t id, Operation operation, std::uint64_t identity_record_id,
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

Coordinator coordinator() {
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

bool drain_replacement_and_recover() {
  MemfdWorker worker;
  Coordinator lifecycle = coordinator();
  REQUIRE(worker.attach_lifecycle(lifecycle));
  REQUIRE(worker.submit(1U) == SubmitResult::Accepted);

  REQUIRE(lifecycle.apply(request(1U, Operation::Reset, 1U, 1U, 1U)) ==
          Result::CallbackRejected);
  REQUIRE(worker.lifecycle_online() && worker.lifecycle_accepting());
  REQUIRE(worker.identity_record_id() == 1U && worker.generation() == 1U && worker.epoch() == 1U);
  REQUIRE(worker.in_flight() == 1U);
  REQUIRE(worker.complete_one());

  REQUIRE(lifecycle.apply(request(2U, Operation::Reset, 1U, 1U, 1U)) == Result::Accepted);
  REQUIRE(worker.lifecycle_online() && worker.lifecycle_accepting());
  REQUIRE(worker.identity_record_id() == 3U && worker.generation() == 3U && worker.epoch() == 2U);
  REQUIRE(worker.submit(1U) == SubmitResult::StaleGeneration);
  REQUIRE(worker.submit(3U) == SubmitResult::Accepted);
  REQUIRE(worker.complete_one());

  REQUIRE(lifecycle.apply(request(3U, Operation::TransportLoss, 3U, 3U, 2U,
                                  Source::Disconnect)) == Result::Accepted);
  REQUIRE(!worker.lifecycle_online() && !worker.lifecycle_accepting());
  REQUIRE(worker.submit(3U) == SubmitResult::DeviceLost);

  REQUIRE(lifecycle.apply(request(4U, Operation::Recover, 3U, 3U, 2U, Source::Restart)) ==
          Result::Accepted);
  REQUIRE(worker.lifecycle_online() && worker.lifecycle_accepting());
  REQUIRE(worker.identity_record_id() == 4U && worker.generation() == 4U && worker.epoch() == 3U);
  return true;
}

bool remove_and_add_retire_local_identity() {
  MemfdWorker worker;
  Coordinator lifecycle = coordinator();
  REQUIRE(worker.attach_lifecycle(lifecycle));
  REQUIRE(lifecycle.apply(request(5U, Operation::Remove, 1U, 1U, 1U, Source::Admin)) ==
          Result::Accepted);
  REQUIRE(!worker.lifecycle_online() && worker.generation() == 0U &&
          worker.identity_record_id() == 0U && worker.epoch() == 2U);
  REQUIRE(lifecycle.apply(request(6U, Operation::Add, 0U, 0U, 2U, Source::Admin)) ==
          Result::Accepted);
  REQUIRE(worker.lifecycle_online() && worker.lifecycle_accepting());
  REQUIRE(worker.identity_record_id() == 2U && worker.generation() == 2U && worker.epoch() == 2U);
  REQUIRE(worker.submit(1U) == SubmitResult::StaleGeneration);
  REQUIRE(worker.submit(2U) == SubmitResult::Accepted);
  REQUIRE(worker.complete_one());
  return true;
}

bool reports_transport_disconnect_through_authority() {
  MemfdWorker worker;
  Coordinator lifecycle = coordinator();
  ResultDetails details{};
  REQUIRE(worker.attach_lifecycle(lifecycle));
  REQUIRE(worker.report_disconnect(lifecycle, 7U, 19U, details) ==
          NormalizationResult::Accepted);
  REQUIRE(details.result == Result::Accepted && details.snapshot.state ==
          metaflux::runtime::lifecycle::State::Lost);
  REQUIRE(!worker.lifecycle_online() && !worker.lifecycle_accepting());
  REQUIRE(worker.report_disconnect(lifecycle, 0U, 19U, details) ==
          NormalizationResult::Invalid);
  REQUIRE(details.result == Result::Invalid && details.snapshot.state ==
          metaflux::runtime::lifecycle::State::Lost);
  return true;
}

} // namespace

int main() {
  return drain_replacement_and_recover() && remove_and_add_retire_local_identity() &&
                 reports_transport_disconnect_through_authority()
             ? 0
             : 1;
}
