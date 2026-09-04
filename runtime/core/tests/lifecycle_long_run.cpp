#include "metaflux/runtime/lifecycle.hpp"

#include <array>
#include <atomic>
#include <cstdint>
#include <iostream>
#include <string_view>
#include <thread>

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

bool qualifies_concurrent_observers_and_replay() {
  constexpr std::uint32_t kConcurrentCycles = 128U;
  MirrorAudit memfd{};
  MirrorAudit cdev{};
  MirrorAudit vfio{};
  Config config{};
  config.logical_device_id = 7U;
  config.daemon_incarnation = 11U;
  config.initial_identity_record_id = 1U;
  config.initial_generation = 1U;
  config.initial_epoch = 1U;
  config.generation_terminal = 4096U;
  config.identity_record_terminal = 4096U;
  config.epoch_terminal = 4096U;
  Coordinator coordinator(config);
  REQUIRE(coordinator.valid());
  REQUIRE(coordinator.register_mirror(make_mirror(MirrorKind::Memfd, "memfd", memfd)));
  REQUIRE(coordinator.register_mirror(make_mirror(MirrorKind::Cdev, "cdev", cdev)));
  REQUIRE(coordinator.register_mirror(make_mirror(MirrorKind::VfioUser, "vfio-user", vfio)));

  const Request replay =
      make_request(9001U, Operation::Reset, coordinator.snapshot(), Source::Admin);
  std::array<Result, 4> replay_results{};
  std::array<std::thread, 4> replayers{};
  for (std::size_t index = 0; index < replayers.size(); ++index) {
    replayers[index] = std::thread([&, index] {
      ResultDetails details{};
      replay_results[index] = coordinator.apply(replay, details);
    });
  }
  for (auto& thread : replayers) {
    thread.join();
  }
  std::size_t accepted = 0U;
  std::size_t duplicate = 0U;
  for (const Result result : replay_results) {
    accepted += result == Result::Accepted ? 1U : 0U;
    duplicate += result == Result::Duplicate ? 1U : 0U;
  }
  REQUIRE(accepted == 1U);
  REQUIRE(duplicate == replay_results.size() - 1U);
  REQUIRE(coordinator.snapshot().generation == 2U);

  std::atomic<bool> stop{false};
  std::atomic<bool> failed{false};
  auto mark_invalid = [&failed](const Snapshot& snapshot) {
    const auto state = static_cast<std::uint8_t>(snapshot.state);
    if (snapshot.logical_device_id != 7U || snapshot.daemon_incarnation != 11U ||
        state > static_cast<std::uint8_t>(State::Lost)) {
      failed.store(true, std::memory_order_relaxed);
    }
  };
  std::thread open_activity([&] {
    while (!stop.load(std::memory_order_relaxed)) {
      if (!coordinator.valid() || coordinator.mirror_count() != 3U) {
        failed.store(true, std::memory_order_relaxed);
      }
      std::this_thread::yield();
    }
  });
  std::thread mmap_activity([&] {
    while (!stop.load(std::memory_order_relaxed)) {
      mark_invalid(coordinator.snapshot());
      std::this_thread::yield();
    }
  });
  std::thread telemetry_activity([&] {
    while (!stop.load(std::memory_order_relaxed)) {
      const Snapshot snapshot = coordinator.snapshot();
      mark_invalid(snapshot);
      const auto resolved = coordinator.resolve(snapshot.generation);
      if (resolved != ResolveResult::Online && resolved != ResolveResult::DeviceLost &&
          resolved != ResolveResult::Absent && resolved != ResolveResult::Unknown) {
        failed.store(true, std::memory_order_relaxed);
      }
      if (snapshot.generation != 0U && !coordinator.generation_consumed(snapshot.generation)) {
        failed.store(true, std::memory_order_relaxed);
      }
      std::this_thread::yield();
    }
  });
  std::thread submit_activity([&] {
    while (!stop.load(std::memory_order_relaxed)) {
      ResultDetails details{};
      if (coordinator.apply(replay, details) != Result::Duplicate) {
        failed.store(true, std::memory_order_relaxed);
      }
      std::this_thread::yield();
    }
  });

  std::uint64_t request_id = 10000U;
  bool writer_ok = true;
  for (std::uint32_t cycle = 0U; cycle < kConcurrentCycles; ++cycle) {
    const Snapshot before_reset = coordinator.snapshot();
    ResultDetails details{};
    if (coordinator.apply(make_request(request_id++, Operation::Reset, before_reset, Source::Admin),
                          details) != Result::Accepted) {
      writer_ok = false;
      break;
    }
    const Snapshot after_reset = coordinator.snapshot();
    if (after_reset.state != State::Online ||
        coordinator.apply(make_request(request_id++, Operation::Remove, after_reset, Source::Cdev),
                          details) != Result::Accepted) {
      writer_ok = false;
      break;
    }
    const Snapshot after_remove = coordinator.snapshot();
    if (after_remove.state != State::Absent ||
        coordinator.apply(
            make_request(request_id++, Operation::Add, after_remove, Source::VfioUser), details) !=
            Result::Accepted ||
        coordinator.snapshot().state != State::Online) {
      writer_ok = false;
      break;
    }
  }

  stop.store(true, std::memory_order_relaxed);
  open_activity.join();
  mmap_activity.join();
  telemetry_activity.join();
  submit_activity.join();
  REQUIRE(writer_ok);
  REQUIRE(!failed.load(std::memory_order_relaxed));

  const Snapshot final = coordinator.snapshot();
  REQUIRE(final.state == State::Online);
  REQUIRE(final.generation == 2U + 2U * kConcurrentCycles);
  REQUIRE(final.identity_record_id == final.generation);
  REQUIRE(final.epoch == final.generation);
  REQUIRE(coordinator.tombstone_count() == 2U * kConcurrentCycles + 1U);
  REQUIRE(coordinator.resolve(1U) == ResolveResult::DeviceLost);
  REQUIRE(coordinator.resolve(2U) == ResolveResult::DeviceLost);
  return true;
}

bool qualifies_three_transport_and_qmp_sources_under_load() {
  // work-item-0.1.2.3: 1,000 reset/remove/add cycles with memfd, cdev, vfio-user,
  // and guest-QMP sources rotating under concurrent open/mmap/submit observers.
  constexpr std::uint32_t kTransportCycles = 1000U;
  static constexpr Source kSources[] = {Source::Memfd, Source::Cdev, Source::VfioUser,
                                        Source::Qmp};
  MirrorAudit memfd{};
  MirrorAudit cdev{};
  MirrorAudit vfio{};
  Config config{};
  config.logical_device_id = 7U;
  config.daemon_incarnation = 11U;
  config.initial_identity_record_id = 1U;
  config.initial_generation = 1U;
  config.initial_epoch = 1U;
  config.generation_terminal = 4096U;
  config.identity_record_terminal = 4096U;
  config.epoch_terminal = 4096U;
  Coordinator coordinator(config);
  REQUIRE(coordinator.valid());
  REQUIRE(coordinator.register_mirror(make_mirror(MirrorKind::Memfd, "memfd", memfd)));
  REQUIRE(coordinator.register_mirror(make_mirror(MirrorKind::Cdev, "cdev", cdev)));
  REQUIRE(coordinator.register_mirror(make_mirror(MirrorKind::VfioUser, "vfio-user", vfio)));

  std::atomic<bool> stop{false};
  std::atomic<bool> failed{false};
  auto mark_invalid = [&failed](const Snapshot& snapshot) {
    const auto state = static_cast<std::uint8_t>(snapshot.state);
    if (snapshot.logical_device_id != 7U || snapshot.daemon_incarnation != 11U ||
        state > static_cast<std::uint8_t>(State::Lost)) {
      failed.store(true, std::memory_order_relaxed);
    }
  };
  std::thread open_activity([&] {
    while (!stop.load(std::memory_order_relaxed)) {
      if (!coordinator.valid() || coordinator.mirror_count() != 3U) {
        failed.store(true, std::memory_order_relaxed);
      }
      std::this_thread::yield();
    }
  });
  std::thread mmap_activity([&] {
    while (!stop.load(std::memory_order_relaxed)) {
      mark_invalid(coordinator.snapshot());
      std::this_thread::yield();
    }
  });
  std::thread submit_activity([&] {
    while (!stop.load(std::memory_order_relaxed)) {
      const Snapshot snapshot = coordinator.snapshot();
      mark_invalid(snapshot);
      const auto resolved = coordinator.resolve(snapshot.generation == 0U ? 1U : snapshot.generation);
      if (resolved != ResolveResult::Online && resolved != ResolveResult::DeviceLost &&
          resolved != ResolveResult::Absent && resolved != ResolveResult::Unknown) {
        failed.store(true, std::memory_order_relaxed);
      }
      std::this_thread::yield();
    }
  });

  std::array<std::uint64_t, kTransportCycles * 2U> retired{};
  std::uint64_t request_id = 1U;
  bool writer_ok = true;
  for (std::uint32_t cycle = 0U; cycle < kTransportCycles; ++cycle) {
    const Source reset_source = kSources[cycle % 4U];
    const Source remove_source = kSources[(cycle + 1U) % 4U];
    const Source add_source = kSources[(cycle + 2U) % 4U];
    const Snapshot before_reset = coordinator.snapshot();
    ResultDetails details{};
    if (coordinator.apply(make_request(request_id++, Operation::Reset, before_reset, reset_source),
                          details) != Result::Accepted ||
        coordinator.snapshot().state != State::Online) {
      writer_ok = false;
      break;
    }
    REQUIRE(coordinator.resolve(before_reset.generation) == ResolveResult::DeviceLost);
    retired[cycle * 2U] = before_reset.generation;

    const Snapshot after_reset = coordinator.snapshot();
    if (coordinator.apply(make_request(request_id++, Operation::Remove, after_reset, remove_source),
                          details) != Result::Accepted ||
        coordinator.snapshot().state != State::Absent) {
      writer_ok = false;
      break;
    }
    REQUIRE(coordinator.resolve(after_reset.generation) == ResolveResult::DeviceLost);
    retired[cycle * 2U + 1U] = after_reset.generation;

    const Snapshot after_remove = coordinator.snapshot();
    if (coordinator.apply(make_request(request_id++, Operation::Add, after_remove, add_source),
                          details) != Result::Accepted ||
        coordinator.snapshot().state != State::Online) {
      writer_ok = false;
      break;
    }
  }

  stop.store(true, std::memory_order_relaxed);
  open_activity.join();
  mmap_activity.join();
  submit_activity.join();
  REQUIRE(writer_ok);
  REQUIRE(!failed.load(std::memory_order_relaxed));

  const Snapshot final = coordinator.snapshot();
  REQUIRE(final.state == State::Online);
  REQUIRE(final.generation == 2001U);
  REQUIRE(final.identity_record_id == 2001U);
  REQUIRE(final.epoch == 2001U);
  REQUIRE(coordinator.tombstone_count() == kTransportCycles * 2U);
  for (const std::uint64_t generation : retired) {
    REQUIRE(coordinator.resolve(generation) == ResolveResult::DeviceLost);
  }
  const auto expected_stage_calls = static_cast<std::uint64_t>(kTransportCycles) * 3U;
  for (const MirrorAudit* audit : {&memfd, &cdev, &vfio}) {
    REQUIRE(audit->stage_calls[static_cast<std::size_t>(MirrorStage::Prepare)] ==
            expected_stage_calls);
    REQUIRE(audit->stage_calls[static_cast<std::size_t>(MirrorStage::Commit)] ==
            expected_stage_calls);
    REQUIRE(audit->lost_calls == 0U);
  }
  return true;
}

} // namespace

int main() {
  return qualifies_one_thousand_reset_remove_add_cycles() &&
                 qualifies_concurrent_observers_and_replay() &&
                 qualifies_three_transport_and_qmp_sources_under_load()
             ? 0
             : 1;
}
