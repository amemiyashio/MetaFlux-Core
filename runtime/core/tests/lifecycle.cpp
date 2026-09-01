#include "metaflux/runtime/lifecycle.hpp"

#include <array>
#include <cstdint>
#include <iostream>
#include <string_view>

using metaflux::runtime::lifecycle::Candidate;
using metaflux::runtime::lifecycle::Config;
using metaflux::runtime::lifecycle::Coordinator;
using metaflux::runtime::lifecycle::Mirror;
using metaflux::runtime::lifecycle::MirrorEvent;
using metaflux::runtime::lifecycle::MirrorKind;
using metaflux::runtime::lifecycle::MirrorStage;
using metaflux::runtime::lifecycle::Operation;
using metaflux::runtime::lifecycle::Request;
using metaflux::runtime::lifecycle::ResultDetails;
using metaflux::runtime::lifecycle::ResolveResult;
using metaflux::runtime::lifecycle::Result;
using metaflux::runtime::lifecycle::Source;
using metaflux::runtime::lifecycle::State;

#define REQUIRE(condition)                                                                         \
  do {                                                                                             \
    if (!(condition)) {                                                                            \
      std::cerr << __func__ << ':' << __LINE__ << ": " #condition "\n";                         \
      return false;                                                                                \
    }                                                                                              \
  } while (false)

namespace {

struct MirrorLog final {
  std::array<MirrorStage, 32> stages{};
  std::array<std::uint64_t, 32> generations{};
  std::array<std::uint64_t, 32> candidates{};
  std::uint32_t count = 0;
  std::uint32_t fail_stage = 0;
  std::uint32_t fail_after_commits = 0;
  std::uint32_t commit_count = 0;
  std::uint32_t lost_count = 0;
  std::uint64_t* clock_value = nullptr;
};

bool record(void* context, const MirrorEvent& event) noexcept {
  auto* log = static_cast<MirrorLog*>(context);
  if (log->count < log->stages.size()) {
    log->stages[log->count] = event.stage;
    log->generations[log->count] = event.old_generation;
    log->candidates[log->count] = event.candidate.generation;
    ++log->count;
  }
  if (event.stage == MirrorStage::Commit) {
    ++log->commit_count;
    if (log->fail_after_commits != 0U && log->commit_count > log->fail_after_commits) {
      return false;
    }
  }
  if (log->clock_value != nullptr) {
    ++(*log->clock_value);
  }
  return static_cast<std::uint32_t>(event.stage) != log->fail_stage;
}

void record_lost(void* context, const MirrorEvent&) noexcept {
  auto* log = static_cast<MirrorLog*>(context);
  ++log->lost_count;
}

Mirror make_mirror(MirrorKind kind, std::string_view name, MirrorLog& log) {
  return Mirror{
      .kind = kind,
      .name = name,
      .context = &log,
      .prepare = record,
      .quiesce = record,
      .drain = record,
      .commit = record,
      .abort = record,
      .publish_lost = record_lost,
  };
}

Request request(std::uint64_t id, Operation operation, std::uint64_t generation,
                std::uint64_t epoch, Source source = Source::Admin) {
  return Request{
      .request_id = id,
      .logical_device_id = 7,
      .daemon_incarnation = 11,
      .expected_identity_record_id = generation,
      .expected_generation = generation,
      .expected_epoch = epoch,
      .deadline_tick = 0,
      .source = source,
      .operation = operation,
  };
}

Coordinator make_coordinator(MirrorLog& memfd, MirrorLog& cdev, MirrorLog& vfio) {
  Config config{};
  config.logical_device_id = 7;
  config.daemon_incarnation = 11;
  config.initial_identity_record_id = 1;
  config.initial_generation = 1;
  config.initial_epoch = 1;
  config.generation_terminal = 32;
  config.identity_record_terminal = 32;
  config.epoch_terminal = 32;
  Coordinator coordinator(config);
  (void)coordinator.register_mirror(make_mirror(MirrorKind::Memfd, "memfd", memfd));
  (void)coordinator.register_mirror(make_mirror(MirrorKind::Cdev, "cdev", cdev));
  (void)coordinator.register_mirror(make_mirror(MirrorKind::VfioUser, "vfio-user", vfio));
  return coordinator;
}

bool reset_commits_one_generation_and_tombstones_old() {
  MirrorLog memfd{};
  MirrorLog cdev{};
  MirrorLog vfio{};
  Coordinator coordinator = make_coordinator(memfd, cdev, vfio);
  REQUIRE(coordinator.valid());
  REQUIRE(coordinator.mirror_count() == 3U);

  const Request reset = request(1, Operation::Reset, 1, 1, Source::Admin);
  ResultDetails details{};
  REQUIRE(coordinator.apply(reset, details) == Result::Accepted);
  REQUIRE(details.candidate_generation == 2U);
  REQUIRE(details.candidate_identity_record_id == 2U);
  REQUIRE(details.snapshot.state == State::Online);
  REQUIRE(details.snapshot.generation == 2U);
  REQUIRE(details.snapshot.epoch == 2U);
  REQUIRE(details.snapshot.generation_high_water == 2U);
  REQUIRE(coordinator.resolve(1U) == ResolveResult::DeviceLost);
  REQUIRE(coordinator.resolve(2U) == ResolveResult::Online);
  REQUIRE(coordinator.tombstone_count() == 1U);
  REQUIRE(memfd.commit_count == 1U && cdev.commit_count == 1U && vfio.commit_count == 1U);
  REQUIRE(memfd.count == 4U && memfd.stages[0] == MirrorStage::Prepare &&
          memfd.stages[1] == MirrorStage::Quiesce && memfd.stages[2] == MirrorStage::Drain &&
          memfd.stages[3] == MirrorStage::Commit);
  return true;
}

bool duplicate_and_conflicting_request_have_no_side_effect() {
  MirrorLog memfd{};
  MirrorLog cdev{};
  MirrorLog vfio{};
  Coordinator coordinator = make_coordinator(memfd, cdev, vfio);
  const Request reset = request(2, Operation::Reset, 1, 1);
  REQUIRE(coordinator.apply(reset) == Result::Accepted);
  const auto before = coordinator.snapshot();
  const std::uint32_t commits = memfd.commit_count;
  REQUIRE(coordinator.apply(reset) == Result::Duplicate);
  REQUIRE(coordinator.snapshot().generation == before.generation);
  REQUIRE(coordinator.snapshot().epoch == before.epoch);
  REQUIRE(memfd.commit_count == commits);
  const Request conflict = request(2, Operation::Remove, 2, 2);
  REQUIRE(coordinator.apply(conflict) == Result::Conflict);
  REQUIRE(coordinator.snapshot().generation == before.generation);
  REQUIRE(coordinator.snapshot().epoch == before.epoch);
  return true;
}

bool mirror_can_be_detached_before_owner_destruction() {
  MirrorLog memfd{};
  MirrorLog cdev{};
  MirrorLog vfio{};
  Coordinator coordinator = make_coordinator(memfd, cdev, vfio);
  REQUIRE(coordinator.mirror_count() == 3U);
  REQUIRE(coordinator.unregister_mirror(MirrorKind::Cdev, &cdev));
  REQUIRE(coordinator.mirror_count() == 2U);
  REQUIRE(!coordinator.unregister_mirror(MirrorKind::Cdev, &cdev));

  ResultDetails details{};
  REQUIRE(coordinator.apply(request(21, Operation::Reset, 1, 1), details) == Result::Accepted);
  REQUIRE(memfd.commit_count == 1U && vfio.commit_count == 1U && cdev.commit_count == 0U);
  return true;
}

bool precommit_failure_consumes_candidate_without_retiring_old() {
  MirrorLog memfd{};
  MirrorLog cdev{};
  MirrorLog vfio{};
  cdev.fail_stage = static_cast<std::uint32_t>(MirrorStage::Drain);
  Coordinator coordinator = make_coordinator(memfd, cdev, vfio);
  ResultDetails details{};
  REQUIRE(coordinator.apply(request(3, Operation::Reset, 1, 1), details) ==
          Result::CallbackRejected);
  REQUIRE(details.candidate_generation == 2U);
  REQUIRE(details.snapshot.state == State::Online);
  REQUIRE(details.snapshot.generation == 1U);
  REQUIRE(details.snapshot.epoch == 1U);
  REQUIRE(details.snapshot.generation_high_water == 2U);
  REQUIRE(coordinator.resolve(1U) == ResolveResult::Online);
  REQUIRE(coordinator.resolve(2U) == ResolveResult::Unknown);

  cdev.fail_stage = 0U;
  REQUIRE(coordinator.apply(request(4, Operation::Reset, 1, 1), details) == Result::Accepted);
  REQUIRE(details.candidate_generation == 3U);
  REQUIRE(details.snapshot.generation == 3U);
  REQUIRE(coordinator.resolve(2U) == ResolveResult::Unknown);
  return true;
}

bool transport_loss_and_recovery_preserve_then_advance_epoch() {
  MirrorLog memfd{};
  MirrorLog cdev{};
  MirrorLog vfio{};
  Coordinator coordinator = make_coordinator(memfd, cdev, vfio);
  REQUIRE(coordinator.apply(request(5, Operation::TransportLoss, 1, 1, Source::Disconnect)) ==
          Result::Accepted);
  REQUIRE(coordinator.snapshot().state == State::Lost);
  REQUIRE(coordinator.snapshot().generation == 1U);
  REQUIRE(coordinator.snapshot().epoch == 1U);
  REQUIRE(coordinator.resolve(1U) == ResolveResult::DeviceLost);
  REQUIRE(coordinator.apply(request(6, Operation::Recover, 1, 1, Source::Restart)) ==
          Result::Accepted);
  REQUIRE(coordinator.snapshot().state == State::Online);
  REQUIRE(coordinator.snapshot().generation == 2U);
  REQUIRE(coordinator.snapshot().epoch == 2U);
  REQUIRE(coordinator.resolve(1U) == ResolveResult::DeviceLost);
  REQUIRE(coordinator.resolve(2U) == ResolveResult::Online);
  return true;
}

bool partial_commit_never_restores_retired_generation() {
  MirrorLog memfd{};
  MirrorLog cdev{};
  MirrorLog vfio{};
  cdev.fail_after_commits = 0U;
  vfio.fail_stage = static_cast<std::uint32_t>(MirrorStage::Commit);
  Coordinator coordinator = make_coordinator(memfd, cdev, vfio);
  ResultDetails details{};
  REQUIRE(coordinator.apply(request(7, Operation::Reset, 1, 1), details) ==
          Result::CallbackRejected);
  REQUIRE(details.snapshot.state == State::Lost);
  REQUIRE(details.snapshot.generation == 2U);
  REQUIRE(details.snapshot.epoch == 2U);
  REQUIRE(coordinator.resolve(1U) == ResolveResult::DeviceLost);
  REQUIRE(coordinator.resolve(2U) == ResolveResult::DeviceLost);
  REQUIRE(memfd.lost_count == 1U && cdev.lost_count == 1U && vfio.lost_count == 1U);
  return true;
}

bool remove_then_add_uses_new_generation_and_epoch() {
  MirrorLog memfd{};
  MirrorLog cdev{};
  MirrorLog vfio{};
  Coordinator coordinator = make_coordinator(memfd, cdev, vfio);
  REQUIRE(coordinator.apply(request(8, Operation::Remove, 1, 1, Source::Cdev)) ==
          Result::Accepted);
  REQUIRE(coordinator.snapshot().state == State::Absent);
  REQUIRE(coordinator.snapshot().generation == 0U);
  REQUIRE(coordinator.snapshot().epoch == 2U);
  REQUIRE(coordinator.resolve(1U) == ResolveResult::DeviceLost);
  ResultDetails details{};
  REQUIRE(coordinator.apply(request(9, Operation::Add, 0, 2, Source::VfioUser), details) ==
          Result::Accepted);
  REQUIRE(details.snapshot.state == State::Online);
  REQUIRE(details.snapshot.generation == 2U);
  REQUIRE(details.snapshot.epoch == 2U);
  REQUIRE(coordinator.resolve(1U) == ResolveResult::DeviceLost);
  return true;
}

bool exhaustion_rejects_before_side_effect() {
  MirrorLog memfd{};
  MirrorLog cdev{};
  MirrorLog vfio{};
  Config config{};
  config.logical_device_id = 7;
  config.daemon_incarnation = 11;
  config.initial_identity_record_id = 1;
  config.initial_generation = 1;
  config.initial_epoch = 1;
  config.generation_terminal = 2;
  config.identity_record_terminal = 2;
  config.epoch_terminal = 2;
  Coordinator coordinator(config);
  (void)coordinator.register_mirror(make_mirror(MirrorKind::Memfd, "memfd", memfd));
  (void)coordinator.register_mirror(make_mirror(MirrorKind::Cdev, "cdev", cdev));
  (void)coordinator.register_mirror(make_mirror(MirrorKind::VfioUser, "vfio-user", vfio));
  REQUIRE(coordinator.apply(request(10, Operation::Reset, 1, 1)) == Result::ResourceExhausted);
  REQUIRE(coordinator.snapshot().generation == 1U);
  REQUIRE(coordinator.snapshot().epoch == 1U);
  REQUIRE(coordinator.snapshot().generation_high_water == 1U);
  REQUIRE(memfd.count == 0U && cdev.count == 0U && vfio.count == 0U);
  return true;
}

bool every_mirror_stage_failure_is_bounded() {
  constexpr std::array<MirrorStage, 4> stages{
      MirrorStage::Prepare, MirrorStage::Quiesce, MirrorStage::Drain, MirrorStage::Commit};
  for (std::size_t failing_mirror = 0U; failing_mirror < 3U; ++failing_mirror) {
    for (const auto stage : stages) {
      MirrorLog memfd{};
      MirrorLog cdev{};
      MirrorLog vfio{};
      MirrorLog* logs[] = {&memfd, &cdev, &vfio};
      logs[failing_mirror]->fail_stage = static_cast<std::uint32_t>(stage);
      Coordinator coordinator = make_coordinator(memfd, cdev, vfio);
      ResultDetails details{};
      REQUIRE(coordinator.apply(request(100U + failing_mirror * 10U +
                                              static_cast<std::uint64_t>(stage),
                                        Operation::Reset, 1U, 1U),
                                details) == Result::CallbackRejected);
      const bool partial_commit = stage == MirrorStage::Commit && failing_mirror != 0U;
      REQUIRE(details.snapshot.state == (partial_commit ? State::Lost : State::Online));
      REQUIRE(details.snapshot.generation == (partial_commit ? 2U : 1U));
      REQUIRE(details.snapshot.identity_record_id == (partial_commit ? 2U : 1U));
      REQUIRE(details.snapshot.epoch == (partial_commit ? 2U : 1U));
      REQUIRE(details.snapshot.generation_high_water == 2U);
      REQUIRE(coordinator.resolve(1U) ==
              (partial_commit ? ResolveResult::DeviceLost : ResolveResult::Online));
      REQUIRE(coordinator.resolve(2U) ==
              (partial_commit ? ResolveResult::DeviceLost : ResolveResult::Unknown));
      REQUIRE(memfd.lost_count == (partial_commit ? 1U : 0U) &&
              cdev.lost_count == (partial_commit ? 1U : 0U) &&
              vfio.lost_count == (partial_commit ? 1U : 0U));
    }
  }
  return true;
}

bool stale_identity_and_expired_deadline_are_rejected_without_callbacks() {
  MirrorLog memfd{};
  MirrorLog cdev{};
  MirrorLog vfio{};
  Coordinator coordinator = make_coordinator(memfd, cdev, vfio);

  Request stale_daemon = request(11, Operation::Reset, 1, 1);
  stale_daemon.daemon_incarnation = 99;
  REQUIRE(coordinator.apply(stale_daemon) == Result::Stale);
  REQUIRE(memfd.count == 0U && cdev.count == 0U && vfio.count == 0U);

  REQUIRE(coordinator.apply(request(12, Operation::Reset, 99, 1)) == Result::Stale);
  REQUIRE(memfd.count == 0U && cdev.count == 0U && vfio.count == 0U);

  MirrorLog deadline_memfd{};
  MirrorLog deadline_cdev{};
  MirrorLog deadline_vfio{};
  Config config{};
  config.logical_device_id = 7;
  config.daemon_incarnation = 11;
  config.initial_identity_record_id = 1;
  config.initial_generation = 1;
  config.initial_epoch = 1;
  config.generation_terminal = 32;
  config.identity_record_terminal = 32;
  config.epoch_terminal = 32;
  static std::uint64_t now = 100;
  config.clock = []() noexcept -> std::uint64_t { return now; };
  Coordinator deadline_coordinator(config);
  deadline_memfd.clock_value = &now;
  deadline_cdev.clock_value = &now;
  deadline_vfio.clock_value = &now;
  (void)deadline_coordinator.register_mirror(
      make_mirror(MirrorKind::Memfd, "memfd", deadline_memfd));
  (void)deadline_coordinator.register_mirror(
      make_mirror(MirrorKind::Cdev, "cdev", deadline_cdev));
  (void)deadline_coordinator.register_mirror(
      make_mirror(MirrorKind::VfioUser, "vfio-user", deadline_vfio));
  Request expired = request(13, Operation::Reset, 1, 1);
  expired.deadline_tick = now + 2U;
  REQUIRE(deadline_coordinator.apply(expired) == Result::Timeout);
  REQUIRE(deadline_coordinator.snapshot().generation == 1U);
  REQUIRE(deadline_coordinator.snapshot().epoch == 1U);
  REQUIRE(deadline_coordinator.snapshot().generation_high_water == 2U);
  REQUIRE(deadline_memfd.count != 0U && deadline_cdev.count != 0U && deadline_vfio.count == 0U);
  REQUIRE(deadline_coordinator.apply(request(14, Operation::Reset, 1, 1)) == Result::Accepted);
  REQUIRE(deadline_coordinator.snapshot().generation == 3U);
  return true;
}

} // namespace

int main() {
  const bool ok = reset_commits_one_generation_and_tombstones_old() &&
                  duplicate_and_conflicting_request_have_no_side_effect() &&
                  mirror_can_be_detached_before_owner_destruction() &&
                  precommit_failure_consumes_candidate_without_retiring_old() &&
                  transport_loss_and_recovery_preserve_then_advance_epoch() &&
                  partial_commit_never_restores_retired_generation() &&
                  remove_then_add_uses_new_generation_and_epoch() &&
                  exhaustion_rejects_before_side_effect() &&
                  every_mirror_stage_failure_is_bounded() &&
                  stale_identity_and_expired_deadline_are_rejected_without_callbacks();
  return ok ? 0 : 1;
}
