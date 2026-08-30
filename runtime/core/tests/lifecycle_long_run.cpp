#include "metaflux/runtime/lifecycle.hpp"

#include <array>
#include <cstdint>
#include <iostream>
#include <string_view>

using metaflux::runtime::lifecycle::Config;
using metaflux::runtime::lifecycle::Coordinator;
using metaflux::runtime::lifecycle::Mirror;
using metaflux::runtime::lifecycle::MirrorEvent;
using metaflux::runtime::lifecycle::MirrorKind;
using metaflux::runtime::lifecycle::MirrorStage;
using metaflux::runtime::lifecycle::Operation;
using metaflux::runtime::lifecycle::Request;
using metaflux::runtime::lifecycle::ResolveResult;
using metaflux::runtime::lifecycle::Result;
using metaflux::runtime::lifecycle::ResultDetails;
using metaflux::runtime::lifecycle::Snapshot;
using metaflux::runtime::lifecycle::Source;
using metaflux::runtime::lifecycle::State;

#define REQUIRE(condition)                                                                         \
  do {                                                                                             \
    if (!(condition)) {                                                                            \
      std::cerr << __func__ << ':' << __LINE__ << ": " #condition "\n";                            \
      return false;                                                                                \
    }                                                                                              \
  } while (false)

namespace {

constexpr std::uint32_t kCycles = 1000;

struct MirrorAudit final {
  std::array<std::uint64_t, 7> stage_calls{};
  std::uint64_t lost_calls = 0;
};

bool record_stage(void* context, const MirrorEvent& event) noexcept {
  auto* audit = static_cast<MirrorAudit*>(context);
  const auto stage = static_cast<std::size_t>(event.stage);
  if (stage >= audit->stage_calls.size()) {
    return false;
  }
  ++audit->stage_calls[stage];
  return true;
}

void record_lost(void* context, const MirrorEvent&) noexcept {
  auto* audit = static_cast<MirrorAudit*>(context);
  ++audit->lost_calls;
}

Mirror make_mirror(MirrorKind kind, std::string_view name, MirrorAudit& audit) {
  return Mirror{
      .kind = kind,
      .name = name,
      .context = &audit,
      .prepare = record_stage,
      .quiesce = record_stage,
      .drain = record_stage,
      .commit = record_stage,
      .abort = record_stage,
      .publish_lost = record_lost,
  };
}

Request make_request(std::uint64_t request_id, Operation operation, const Snapshot& snapshot,
                     Source source) {
  const bool add = operation == Operation::Add;
  return Request{
      .request_id = request_id,
      .logical_device_id = snapshot.logical_device_id,
      .daemon_incarnation = snapshot.daemon_incarnation,
      .expected_identity_record_id = add ? 0U : snapshot.identity_record_id,
      .expected_generation = add ? 0U : snapshot.generation,
      .expected_epoch = snapshot.epoch,
      .deadline_tick = 0,
      .source = source,
      .operation = operation,
  };
}

bool qualifies_one_thousand_reset_remove_add_cycles() {
  MirrorAudit memfd{};
  MirrorAudit cdev{};
  MirrorAudit vfio{};
  Config config{};
  config.logical_device_id = 7;
  config.daemon_incarnation = 11;
  config.initial_identity_record_id = 1;
  config.initial_generation = 1;
  config.initial_epoch = 1;
  config.generation_terminal = 4096;
  config.identity_record_terminal = 4096;
  config.epoch_terminal = 4096;
  Coordinator coordinator(config);
  REQUIRE(coordinator.valid());
  REQUIRE(coordinator.register_mirror(make_mirror(MirrorKind::Memfd, "memfd", memfd)));
  REQUIRE(coordinator.register_mirror(make_mirror(MirrorKind::Cdev, "cdev", cdev)));
  REQUIRE(coordinator.register_mirror(make_mirror(MirrorKind::VfioUser, "vfio-user", vfio)));

  std::array<std::uint64_t, kCycles * 2U> retired_generations{};
  Request first_reset{};
  std::uint64_t request_id = 1;
  for (std::uint32_t cycle = 0; cycle < kCycles; ++cycle) {
    const Snapshot before_reset = coordinator.snapshot();
    const Request reset = make_request(request_id++, Operation::Reset, before_reset, Source::Admin);
    if (cycle == 0U) {
      first_reset = reset;
    }
    ResultDetails details{};
    REQUIRE(coordinator.apply(reset, details) == Result::Accepted);
    const Snapshot after_reset = coordinator.snapshot();
    REQUIRE(after_reset.state == State::Online);
    REQUIRE(after_reset.generation == before_reset.generation + 1U);
    REQUIRE(after_reset.identity_record_id == before_reset.identity_record_id + 1U);
    REQUIRE(after_reset.epoch == before_reset.epoch + 1U);
    REQUIRE(coordinator.resolve(before_reset.generation) == ResolveResult::DeviceLost);
    retired_generations[cycle * 2U] = before_reset.generation;

    const Request remove = make_request(request_id++, Operation::Remove, after_reset, Source::Cdev);
    REQUIRE(coordinator.apply(remove, details) == Result::Accepted);
    const Snapshot after_remove = coordinator.snapshot();
    REQUIRE(after_remove.state == State::Absent);
    REQUIRE(after_remove.generation == 0U);
    REQUIRE(after_remove.identity_record_id == 0U);
    REQUIRE(after_remove.epoch == after_reset.epoch + 1U);
    REQUIRE(coordinator.resolve(after_reset.generation) == ResolveResult::DeviceLost);
    retired_generations[cycle * 2U + 1U] = after_reset.generation;

    const Request add = make_request(request_id++, Operation::Add, after_remove, Source::VfioUser);
    REQUIRE(coordinator.apply(add, details) == Result::Accepted);
    const Snapshot after_add = coordinator.snapshot();
    REQUIRE(after_add.state == State::Online);
    REQUIRE(after_add.generation == after_reset.generation + 1U);
    REQUIRE(after_add.identity_record_id == after_reset.identity_record_id + 1U);
    REQUIRE(after_add.epoch == after_remove.epoch);
  }

  const Snapshot final = coordinator.snapshot();
  REQUIRE(final.state == State::Online);
  REQUIRE(final.generation == 2001U);
  REQUIRE(final.identity_record_id == 2001U);
  REQUIRE(final.epoch == 2001U);
  REQUIRE(final.generation_high_water == 2001U);
  REQUIRE(final.identity_high_water == 2001U);
  REQUIRE(coordinator.tombstone_count() == kCycles * 2U);
  for (const std::uint64_t generation : retired_generations) {
    REQUIRE(coordinator.resolve(generation) == ResolveResult::DeviceLost);
  }

  const auto expected_stage_calls = static_cast<std::uint64_t>(kCycles) * 3U;
  for (const MirrorAudit* audit : {&memfd, &cdev, &vfio}) {
    REQUIRE(audit->stage_calls[static_cast<std::size_t>(MirrorStage::Prepare)] ==
            expected_stage_calls);
    REQUIRE(audit->stage_calls[static_cast<std::size_t>(MirrorStage::Quiesce)] ==
            expected_stage_calls);
    REQUIRE(audit->stage_calls[static_cast<std::size_t>(MirrorStage::Drain)] ==
            expected_stage_calls);
    REQUIRE(audit->stage_calls[static_cast<std::size_t>(MirrorStage::Commit)] ==
            expected_stage_calls);
    REQUIRE(audit->stage_calls[static_cast<std::size_t>(MirrorStage::Abort)] == 0U);
    REQUIRE(audit->lost_calls == 0U);
  }

  ResultDetails replay_details{};
  REQUIRE(coordinator.apply(first_reset, replay_details) == Result::Duplicate);
  REQUIRE(replay_details.candidate_generation == 2U);
  REQUIRE(replay_details.snapshot.generation == final.generation);
  Request conflict = first_reset;
  conflict.operation = Operation::Remove;
  REQUIRE(coordinator.apply(conflict) == Result::Conflict);
  REQUIRE(coordinator.snapshot().generation == final.generation);
  return true;
}

} // namespace

int main() { return qualifies_one_thousand_reset_remove_add_cycles() ? 0 : 1; }
