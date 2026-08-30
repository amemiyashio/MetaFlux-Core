#include "metaflux/transport/qmp_lifecycle.hpp"

#include <cstdint>
#include <iostream>

using metaflux::runtime::lifecycle::Config;
using metaflux::runtime::lifecycle::Coordinator;
using metaflux::runtime::lifecycle::ExternalEvent;
using metaflux::runtime::lifecycle::ExternalEventKind;
using metaflux::runtime::lifecycle::Operation;
using metaflux::runtime::lifecycle::Request;
using metaflux::runtime::lifecycle::Result;
using metaflux::runtime::lifecycle::ResultDetails;
using metaflux::runtime::lifecycle::Source;
using metaflux::runtime::lifecycle::State;
using metaflux::transport::vfio_user::QmpCommand;
using metaflux::transport::vfio_user::QmpLifecycleAdapter;
using metaflux::transport::vfio_user::QmpReply;
using metaflux::transport::vfio_user::QmpReplyKind;
using metaflux::transport::vfio_user::QmpResult;

#define REQUIRE(condition)                                                                         \
  do {                                                                                             \
    if (!(condition)) {                                                                            \
      std::cerr << __func__ << ':' << __LINE__ << ": " #condition "\n";                            \
      return false;                                                                                \
    }                                                                                              \
  } while (false)

namespace {

ExternalEvent event(ExternalEventKind kind, std::uint64_t request_id,
                    std::uint64_t identity_record_id, std::uint64_t generation,
                    std::uint64_t epoch) {
  return ExternalEvent{
      .request_id = request_id,
      .logical_device_id = 7U,
      .daemon_incarnation = 11U,
      .expected_identity_record_id = identity_record_id,
      .expected_generation = generation,
      .expected_epoch = epoch,
      .kind = kind,
  };
}

bool correlates_qmp_add_and_remove() {
  QmpLifecycleAdapter adapter;
  REQUIRE(adapter.begin(
              {.command_id = 100U, .event = event(ExternalEventKind::QmpAdd, 41U, 0U, 0U, 2U)}) ==
          QmpResult::Accepted);
  REQUIRE(adapter.pending() && adapter.pending_command_id() == 100U);
  REQUIRE(adapter.begin({.command_id = 101U,
                         .event = event(ExternalEventKind::QmpRemove, 42U, 3U, 3U, 2U)}) ==
          QmpResult::Busy);

  Request request{};
  REQUIRE(adapter.complete({.command_id = 99U, .kind = QmpReplyKind::DeviceAdded}, request) ==
          QmpResult::Pending);
  REQUIRE(adapter.complete({.command_id = 100U, .kind = QmpReplyKind::DeviceDeleted}, request) ==
          QmpResult::Pending);
  REQUIRE(adapter.complete({.command_id = 100U, .kind = QmpReplyKind::DeviceAdded}, request) ==
          QmpResult::Accepted);
  REQUIRE(request.source == Source::Qmp && request.operation == Operation::Add &&
          request.request_id == 41U);
  REQUIRE(!adapter.pending());

  REQUIRE(adapter.begin({.command_id = 101U,
                         .event = event(ExternalEventKind::QmpRemove, 42U, 3U, 3U, 2U)}) ==
          QmpResult::Accepted);
  REQUIRE(adapter.complete({.command_id = 101U, .kind = QmpReplyKind::DeviceDeleted}, request) ==
          QmpResult::Accepted);
  REQUIRE(request.source == Source::Qmp && request.operation == Operation::Remove &&
          request.request_id == 42U);
  REQUIRE(!adapter.pending());
  return true;
}

bool maps_remove_failure_and_rejects_add_failure() {
  QmpLifecycleAdapter adapter;
  Request request{};
  REQUIRE(adapter.begin({.command_id = 200U,
                         .event = event(ExternalEventKind::QmpRemove, 51U, 3U, 3U, 2U)}) ==
          QmpResult::Accepted);
  REQUIRE(adapter.complete({.command_id = 200U, .kind = QmpReplyKind::CommandFailed}, request) ==
          QmpResult::Accepted);
  REQUIRE(request.source == Source::Qmp && request.operation == Operation::TransportLoss &&
          request.request_id == 51U);
  REQUIRE(!adapter.pending());

  REQUIRE(adapter.begin(
              {.command_id = 201U, .event = event(ExternalEventKind::QmpAdd, 52U, 0U, 0U, 2U)}) ==
          QmpResult::Accepted);
  REQUIRE(adapter.complete({.command_id = 201U, .kind = QmpReplyKind::CommandFailed}, request) ==
          QmpResult::Failed);
  REQUIRE(request.request_id == 0U && !adapter.pending());

  REQUIRE(adapter.begin(
              {.command_id = 0U, .event = event(ExternalEventKind::QmpAdd, 53U, 0U, 0U, 2U)}) ==
          QmpResult::Invalid);
  return true;
}

bool submits_qmp_completion_through_ingress() {
  Coordinator coordinator(Config{.logical_device_id = 7U,
                                 .daemon_incarnation = 11U,
                                 .initial_identity_record_id = 0U,
                                 .initial_generation = 0U,
                                 .initial_epoch = 1U,
                                 .initial_state = State::Absent,
                                 .identity_record_terminal = 32U,
                                 .generation_terminal = 32U,
                                 .epoch_terminal = 32U});
  QmpLifecycleAdapter adapter;
  const auto add = event(ExternalEventKind::QmpAdd, 61U, 0U, 0U, 1U);
  const auto add_command =
      QmpCommand::from_snapshot(300U, ExternalEventKind::QmpAdd, 61U, coordinator.snapshot());
  REQUIRE(add_command.event.logical_device_id == add.logical_device_id &&
          add_command.event.expected_epoch == add.expected_epoch);
  REQUIRE(adapter.begin(add_command) == QmpResult::Accepted);
  ResultDetails details{};
  REQUIRE(adapter.complete_and_submit({.command_id = 300U, .kind = QmpReplyKind::DeviceAdded},
                                      coordinator, details) == QmpResult::Accepted);
  REQUIRE(details.result == Result::Accepted && details.snapshot.state == State::Online &&
          details.snapshot.generation == 1U && details.snapshot.identity_record_id == 1U);

  REQUIRE(adapter.begin({.command_id = 301U, .event = add}) == QmpResult::Accepted);
  REQUIRE(adapter.complete_and_submit({.command_id = 301U, .kind = QmpReplyKind::DeviceAdded},
                                      coordinator, details) == QmpResult::Accepted);
  REQUIRE(details.result == Result::Duplicate && details.snapshot.state == State::Online);

  REQUIRE(adapter.begin({.command_id = 302U,
                         .event = event(ExternalEventKind::QmpRemove, 62U, 1U, 1U, 1U)}) ==
          QmpResult::Accepted);
  REQUIRE(adapter.complete_and_submit({.command_id = 302U, .kind = QmpReplyKind::CommandFailed},
                                      coordinator, details) == QmpResult::Accepted);
  REQUIRE(details.result == Result::Accepted && details.snapshot.state == State::Lost &&
          coordinator.resolve(1U) == metaflux::runtime::lifecycle::ResolveResult::DeviceLost);
  return true;
}

bool preserves_captured_tuple_when_authority_advances() {
  Coordinator coordinator(Config{.logical_device_id = 7U,
                                 .daemon_incarnation = 11U,
                                 .initial_identity_record_id = 1U,
                                 .initial_generation = 1U,
                                 .initial_epoch = 1U,
                                 .initial_state = State::Online,
                                 .identity_record_terminal = 32U,
                                 .generation_terminal = 32U,
                                 .epoch_terminal = 32U});
  QmpLifecycleAdapter adapter;
  const auto command =
      QmpCommand::from_snapshot(400U, ExternalEventKind::QmpRemove, 81U, coordinator.snapshot());
  REQUIRE(adapter.begin(command) == QmpResult::Accepted);
  const Request reset{
      .request_id = 82U,
      .logical_device_id = 7U,
      .daemon_incarnation = 11U,
      .expected_identity_record_id = 1U,
      .expected_generation = 1U,
      .expected_epoch = 1U,
      .source = Source::Admin,
      .operation = Operation::Reset,
  };
  REQUIRE(coordinator.apply(reset) == Result::Accepted);
  ResultDetails details{};
  REQUIRE(adapter.complete_and_submit({.command_id = 400U, .kind = QmpReplyKind::DeviceDeleted},
                                      coordinator, details) == QmpResult::Accepted);
  REQUIRE(details.result == Result::Stale && details.snapshot.generation == 2U);
  return true;
}

} // namespace

int main() {
  return correlates_qmp_add_and_remove() && maps_remove_failure_and_rejects_add_failure() &&
                 submits_qmp_completion_through_ingress() &&
                 preserves_captured_tuple_when_authority_advances()
             ? 0
             : 1;
}
