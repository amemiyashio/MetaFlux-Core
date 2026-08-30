#include "metaflux/transport/qmp_lifecycle.hpp"

#include <cstdint>
#include <iostream>

using metaflux::runtime::lifecycle::ExternalEvent;
using metaflux::runtime::lifecycle::ExternalEventKind;
using metaflux::runtime::lifecycle::Operation;
using metaflux::runtime::lifecycle::Request;
using metaflux::runtime::lifecycle::Source;
using metaflux::transport::vfio_user::QmpCommand;
using metaflux::transport::vfio_user::QmpLifecycleAdapter;
using metaflux::transport::vfio_user::QmpReply;
using metaflux::transport::vfio_user::QmpReplyKind;
using metaflux::transport::vfio_user::QmpResult;

#define REQUIRE(condition)                                                                         \
  do {                                                                                             \
    if (!(condition)) {                                                                            \
      std::cerr << __func__ << ':' << __LINE__ << ": " #condition "\n";                         \
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
  REQUIRE(adapter.begin({.command_id = 100U,
                         .event = event(ExternalEventKind::QmpAdd, 41U, 0U, 0U, 2U)}) ==
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

  REQUIRE(adapter.begin({.command_id = 201U,
                         .event = event(ExternalEventKind::QmpAdd, 52U, 0U, 0U, 2U)}) ==
          QmpResult::Accepted);
  REQUIRE(adapter.complete({.command_id = 201U, .kind = QmpReplyKind::CommandFailed}, request) ==
          QmpResult::Failed);
  REQUIRE(request.request_id == 0U && !adapter.pending());

  REQUIRE(adapter.begin({.command_id = 0U,
                         .event = event(ExternalEventKind::QmpAdd, 53U, 0U, 0U, 2U)}) ==
          QmpResult::Invalid);
  return true;
}

} // namespace

int main() {
  return correlates_qmp_add_and_remove() && maps_remove_failure_and_rejects_add_failure() ? 0 : 1;
}
