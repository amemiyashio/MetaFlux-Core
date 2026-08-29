#include "metaflux/runtime/recovery_model.hpp"

#include "metaflux/shared/device.h"

#include <algorithm>
#include <cstdint>
#include <iostream>

using metaflux::runtime::recovery::AdmissionToken;
using metaflux::runtime::recovery::AttemptState;
using metaflux::runtime::recovery::DeviceState;
using metaflux::runtime::recovery::LeaseState;
using metaflux::runtime::recovery::Limits;
using metaflux::runtime::recovery::Model;
using metaflux::runtime::recovery::PublishState;
using metaflux::runtime::recovery::RangeRecord;
using metaflux::runtime::recovery::RangeState;
using metaflux::runtime::recovery::RangeToken;
using metaflux::runtime::recovery::ReadSnapshot;
using metaflux::runtime::recovery::UpdateState;
using metaflux::runtime::recovery::ViewId;
using metaflux::runtime::recovery::ViewState;

static_assert(static_cast<std::uint32_t>(ViewState::Open) == MF_VIEW_ADMISSION_OPEN);
static_assert(static_cast<std::uint32_t>(ViewState::Closing) == MF_VIEW_ADMISSION_CLOSING);
static_assert(static_cast<std::uint32_t>(ViewState::Terminal) == MF_VIEW_ADMISSION_TERMINAL);
static_assert(static_cast<std::uint32_t>(ViewState::Quarantined) == MF_VIEW_ADMISSION_QUARANTINED);
static_assert(static_cast<std::uint32_t>(DeviceState::Open) == MF_DEVICE_ADMISSION_OPEN);
static_assert(static_cast<std::uint32_t>(DeviceState::Updating) == MF_DEVICE_ADMISSION_UPDATING);
static_assert(static_cast<std::uint32_t>(DeviceState::Closed) == MF_DEVICE_ADMISSION_CLOSED);

static_assert(static_cast<std::uint32_t>(AttemptState::Idle) == MF_ADMISSION_ATTEMPT_IDLE);
static_assert(static_cast<std::uint32_t>(AttemptState::Entering) == MF_ADMISSION_ATTEMPT_ENTERING);
static_assert(static_cast<std::uint32_t>(AttemptState::Claiming) == MF_ADMISSION_ATTEMPT_CLAIMING);
static_assert(static_cast<std::uint32_t>(AttemptState::Initializing) ==
              MF_ADMISSION_ATTEMPT_INITIALIZING);
static_assert(static_cast<std::uint32_t>(AttemptState::Ready) == MF_ADMISSION_ATTEMPT_READY);
static_assert(static_cast<std::uint32_t>(AttemptState::Exited) == MF_ADMISSION_ATTEMPT_EXITED);

static_assert(static_cast<std::uint32_t>(LeaseState::Free) == MF_ADMISSION_LEASE_FREE);
static_assert(static_cast<std::uint32_t>(LeaseState::Initializing) ==
              MF_ADMISSION_LEASE_INITIALIZING);
static_assert(static_cast<std::uint32_t>(LeaseState::Reserved) == MF_ADMISSION_LEASE_RESERVED);
static_assert(static_cast<std::uint32_t>(LeaseState::Committing) == MF_ADMISSION_LEASE_COMMITTING);
static_assert(static_cast<std::uint32_t>(LeaseState::Committed) == MF_ADMISSION_LEASE_COMMITTED);
static_assert(static_cast<std::uint32_t>(LeaseState::Published) == MF_ADMISSION_LEASE_PUBLISHED);
static_assert(static_cast<std::uint32_t>(LeaseState::Released) == MF_ADMISSION_LEASE_RELEASED);
static_assert(static_cast<std::uint32_t>(LeaseState::Revoked) == MF_ADMISSION_LEASE_REVOKED);
static_assert(static_cast<std::uint32_t>(LeaseState::Tombstoned) == MF_ADMISSION_LEASE_TOMBSTONED);
static_assert(static_cast<std::uint32_t>(LeaseState::Quarantined) ==
              MF_ADMISSION_LEASE_QUARANTINED);

static_assert(static_cast<std::uint32_t>(UpdateState::Free) == MF_DEVICE_UPDATE_FREE);
static_assert(static_cast<std::uint32_t>(UpdateState::Initializing) ==
              MF_DEVICE_UPDATE_INITIALIZING);
static_assert(static_cast<std::uint32_t>(UpdateState::Prepared) == MF_DEVICE_UPDATE_PREPARED);
static_assert(static_cast<std::uint32_t>(UpdateState::Linking) == MF_DEVICE_UPDATE_LINKING);
static_assert(static_cast<std::uint32_t>(UpdateState::Active) == MF_DEVICE_UPDATE_ACTIVE);
static_assert(static_cast<std::uint32_t>(UpdateState::FencePublished) ==
              MF_DEVICE_UPDATE_FENCE_PUBLISHED);
static_assert(static_cast<std::uint32_t>(UpdateState::Reopened) == MF_DEVICE_UPDATE_REOPENED);
static_assert(static_cast<std::uint32_t>(UpdateState::Aborted) == MF_DEVICE_UPDATE_ABORTED);
static_assert(static_cast<std::uint32_t>(UpdateState::Closed) == MF_DEVICE_UPDATE_CLOSED);
static_assert(static_cast<std::uint32_t>(UpdateState::Terminal) == MF_DEVICE_UPDATE_TERMINAL);
static_assert(static_cast<std::uint32_t>(UpdateState::Quarantined) == MF_DEVICE_UPDATE_QUARANTINED);

static_assert(static_cast<std::uint32_t>(PublishState::Free) == MF_VIEW_PUBLISH_FREE);
static_assert(static_cast<std::uint32_t>(PublishState::Initializing) ==
              MF_VIEW_PUBLISH_INITIALIZING);
static_assert(static_cast<std::uint32_t>(PublishState::Prepared) == MF_VIEW_PUBLISH_PREPARED);
static_assert(static_cast<std::uint32_t>(PublishState::Linking) == MF_VIEW_PUBLISH_LINKING);
static_assert(static_cast<std::uint32_t>(PublishState::Active) == MF_VIEW_PUBLISH_ACTIVE);
static_assert(static_cast<std::uint32_t>(PublishState::Published) == MF_VIEW_PUBLISH_PUBLISHED);
static_assert(static_cast<std::uint32_t>(PublishState::Aborted) == MF_VIEW_PUBLISH_ABORTED);
static_assert(static_cast<std::uint32_t>(PublishState::Terminal) == MF_VIEW_PUBLISH_TERMINAL);
static_assert(static_cast<std::uint32_t>(PublishState::Quarantined) == MF_VIEW_PUBLISH_QUARANTINED);
static_assert(static_cast<std::uint32_t>(PublishState::Active) == MF_TELEMETRY_PUBLISH_ACTIVE);
static_assert(static_cast<std::uint32_t>(PublishState::Published) ==
              MF_TELEMETRY_PUBLISH_PUBLISHED);

static_assert(static_cast<std::uint32_t>(RangeState::Free) == MF_LIFECYCLE_RANGE_FREE);
static_assert(static_cast<std::uint32_t>(RangeState::Initializing) ==
              MF_LIFECYCLE_RANGE_INITIALIZING);
static_assert(static_cast<std::uint32_t>(RangeState::Prepared) == MF_LIFECYCLE_RANGE_PREPARED);
static_assert(static_cast<std::uint32_t>(RangeState::Open) == MF_LIFECYCLE_RANGE_OPEN);
static_assert(static_cast<std::uint32_t>(RangeState::Retired) == MF_LIFECYCLE_RANGE_RETIRED);
static_assert(static_cast<std::uint32_t>(RangeState::Terminal) == MF_LIFECYCLE_RANGE_TERMINAL);
static_assert(static_cast<std::uint32_t>(RangeState::Quarantined) ==
              MF_LIFECYCLE_RANGE_QUARANTINED);

#define REQUIRE(condition)                                                                         \
  do {                                                                                             \
    if (!(condition)) {                                                                            \
      std::cerr << __func__ << ':' << __LINE__ << ": " #condition "\n";                            \
      return false;                                                                                \
    }                                                                                              \
  } while (false)

namespace {

constexpr Limits kLimits{
    .sequence_terminal = UINT64_C(15),
    .generation_terminal = UINT32_C(7),
    .tag_terminal = UINT32_C(7),
    .epoch_terminal = UINT64_C(15),
};

bool advance_lease_to_stage(Model& model, std::uint32_t stage) {
  REQUIRE(stage < UINT32_C(6));
  REQUIRE(model.enter_attempt(UINT32_C(1)));
  REQUIRE(model.claim_lease(UINT32_C(1), UINT32_C(1)));
  if (stage >= UINT32_C(1)) {
    REQUIRE(model.materialize_lease(UINT32_C(1), UINT32_C(1)));
  }
  if (stage >= UINT32_C(2)) {
    REQUIRE(model.begin_lease_commit(UINT32_C(1)));
  }
  if (stage >= UINT32_C(3)) {
    REQUIRE(model.finish_lease_commit(UINT32_C(1)));
  }
  if (stage >= UINT32_C(4)) {
    REQUIRE(model.write_lease_target(UINT32_C(1)));
  }
  if (stage >= UINT32_C(5)) {
    REQUIRE(model.publish_lease(UINT32_C(1)));
  }
  return true;
}

bool advance_update_to_stage(Model& model, std::uint32_t stage) {
  REQUIRE(stage < UINT32_C(6));
  REQUIRE(model.claim_device_update(UINT32_C(1)));
  if (stage >= UINT32_C(1)) {
    REQUIRE(model.materialize_device_update(UINT32_C(1)));
  }
  if (stage >= UINT32_C(2)) {
    REQUIRE(model.begin_device_update_link(UINT32_C(1)));
  }
  if (stage >= UINT32_C(3)) {
    REQUIRE(model.complete_device_update_link(UINT32_C(1)));
  }
  if (stage >= UINT32_C(4)) {
    REQUIRE(model.publish_device_update(UINT32_C(1)));
  }
  if (stage >= UINT32_C(5)) {
    REQUIRE(model.reopen_device_update(UINT32_C(1)));
  }
  return true;
}

bool advance_publication_to_stage(Model& model, std::uint32_t stage) {
  REQUIRE(stage < UINT32_C(6));
  if (stage >= UINT32_C(1)) {
    REQUIRE(model.prepare_publication(UINT32_C(1)));
  }
  if (stage >= UINT32_C(2)) {
    REQUIRE(model.link_publication(UINT32_C(1)));
  }
  if (stage >= UINT32_C(3)) {
    REQUIRE(model.activate_publication(UINT32_C(1)));
  }
  if (stage >= UINT32_C(4)) {
    REQUIRE(model.apply_publication(UINT32_C(1)));
  }
  if (stage >= UINT32_C(5)) {
    REQUIRE(model.mark_publication(UINT32_C(1)));
  }
  return true;
}

bool close_vs_lease_commit_is_exhaustive() {
  constexpr std::uint32_t lease_step_count = 6;
  for (std::uint32_t close_position = 0; close_position <= lease_step_count; ++close_position) {
    Model model(kLimits);
    bool close_started = false;
    bool finish_succeeded = false;
    std::uint32_t lease_step = 0;

    for (std::uint32_t schedule_step = 0; schedule_step <= lease_step_count; ++schedule_step) {
      if (schedule_step == close_position) {
        close_started = true;
        model.close_view();
      } else {
        bool result = false;
        switch (lease_step) {
        case 0:
          result = model.enter_attempt(UINT32_C(1));
          break;
        case 1:
          result = model.reserve_lease(UINT32_C(1), UINT32_C(1));
          break;
        case 2:
          result = model.begin_lease_commit(UINT32_C(1));
          break;
        case 3:
          result = model.finish_lease_commit(UINT32_C(1));
          finish_succeeded = result;
          REQUIRE(!result || !close_started);
          break;
        case 4:
          result = model.publish_lease(UINT32_C(1));
          break;
        case 5:
          result = model.release_lease(UINT32_C(1));
          break;
        default:
          REQUIRE(false);
        }
        (void)result;
        ++lease_step;
      }
      REQUIRE(model.invariants_hold());
    }

    REQUIRE(model.view_state() == ViewState::Terminal);
    REQUIRE(model.device_state() == DeviceState::Closed);
    REQUIRE(!model.has_live_lease());
    REQUIRE(model.committed_admissions() == (finish_succeeded ? UINT32_C(1) : UINT32_C(0)));
  }
  return true;
}

bool device_update_never_reopens_after_close() {
  constexpr std::uint32_t update_step_count = 5;
  for (std::uint32_t close_position = 0; close_position <= update_step_count; ++close_position) {
    Model model(kLimits);
    bool close_started = false;
    std::uint32_t update_step = 0;
    std::uint32_t previous_generation = model.device_generation();

    for (std::uint32_t schedule_step = 0; schedule_step <= update_step_count; ++schedule_step) {
      if (schedule_step == close_position) {
        close_started = true;
        model.close_device();
      } else {
        bool result = false;
        switch (update_step) {
        case 0:
          result = model.prepare_device_update(UINT32_C(1));
          break;
        case 1:
          result = model.link_device_update(UINT32_C(1));
          break;
        case 2:
          result = model.publish_device_update(UINT32_C(1));
          break;
        case 3:
          result = model.reopen_device_update(UINT32_C(1));
          REQUIRE(!result || !close_started);
          break;
        case 4:
          result = model.finish_device_update(UINT32_C(1));
          break;
        default:
          REQUIRE(false);
        }
        (void)result;
        ++update_step;
      }
      REQUIRE(model.device_generation() >= previous_generation);
      previous_generation = model.device_generation();
      REQUIRE(model.invariants_hold());
    }

    REQUIRE(model.device_state() == DeviceState::Closed);
    REQUIRE(model.device_generation() == kLimits.generation_terminal);
    REQUIRE(!model.reopen_device_update(UINT32_C(1)));
  }

  Model exhausted(
      {.sequence_terminal = 7, .generation_terminal = 3, .tag_terminal = 7, .epoch_terminal = 7});
  REQUIRE(exhausted.prepare_device_update(UINT32_C(1)));
  REQUIRE(exhausted.link_device_update(UINT32_C(1)));
  REQUIRE(exhausted.publish_device_update(UINT32_C(1)));
  REQUIRE(exhausted.reopen_device_update(UINT32_C(1)));
  REQUIRE(exhausted.finish_device_update(UINT32_C(1)));
  REQUIRE(exhausted.device_generation() == UINT32_C(2));
  REQUIRE(!exhausted.prepare_device_update(UINT32_C(2)));
  REQUIRE(exhausted.device_generation() == UINT32_C(2));

  Model old_lease(kLimits);
  REQUIRE(old_lease.enter_attempt(UINT32_C(1)));
  REQUIRE(old_lease.reserve_lease(UINT32_C(1), UINT32_C(1)));
  REQUIRE(old_lease.prepare_device_update(UINT32_C(1)));
  REQUIRE(old_lease.link_device_update(UINT32_C(1)));
  REQUIRE(old_lease.lease().state == LeaseState::Revoked);
  REQUIRE(old_lease.attempt().state == AttemptState::Exited);
  REQUIRE(!old_lease.has_live_lease());
  REQUIRE(old_lease.invariants_hold());
  return true;
}

bool dead_owners_are_helped_or_tombstoned() {
  for (std::uint32_t death_point = 0; death_point < UINT32_C(6); ++death_point) {
    Model model(kLimits);
    REQUIRE(advance_lease_to_stage(model, death_point));
    REQUIRE(model.recover_dead_lease(UINT32_C(1)));
    const bool completed = death_point >= UINT32_C(4);
    REQUIRE(model.lease().state == (completed ? LeaseState::Released : LeaseState::Tombstoned));
    REQUIRE(model.lease().publication_marker == completed);
    REQUIRE(model.attempt().state == AttemptState::Exited);
    REQUIRE(model.committed_admissions() ==
            (death_point >= UINT32_C(3) ? UINT32_C(1) : UINT32_C(0)));
    REQUIRE(!model.has_live_lease());
    REQUIRE(model.invariants_hold());
  }

  for (std::uint32_t expiry_point = 0; expiry_point < UINT32_C(6); ++expiry_point) {
    Model model(kLimits);
    REQUIRE(advance_lease_to_stage(model, expiry_point));
    REQUIRE(model.expire_live_lease(UINT32_C(1)));
    REQUIRE(model.lease().state == LeaseState::Quarantined);
    REQUIRE(model.view_state() == ViewState::Quarantined);
    REQUIRE(model.device_state() == DeviceState::Closed);
    REQUIRE(!model.has_live_lease());
    REQUIRE(model.invariants_hold());
  }

  for (std::uint32_t death_point = 0; death_point < UINT32_C(6); ++death_point) {
    Model model(kLimits);
    REQUIRE(advance_update_to_stage(model, death_point));
    REQUIRE(model.recover_dead_device_update(UINT32_C(1)));
    REQUIRE(model.update().state == UpdateState::Terminal);
    REQUIRE(model.device_state() == DeviceState::Open);
    REQUIRE(model.device_generation() == (death_point >= UINT32_C(2) ? UINT32_C(2) : UINT32_C(1)));
    REQUIRE(model.epoch() == (death_point >= UINT32_C(2) ? UINT64_C(2) : UINT64_C(1)));
    REQUIRE(model.invariants_hold());
  }

  for (std::uint32_t expiry_point = 0; expiry_point < UINT32_C(6); ++expiry_point) {
    Model model(kLimits);
    REQUIRE(advance_update_to_stage(model, expiry_point));
    REQUIRE(model.expire_live_device_update(UINT32_C(1)));
    REQUIRE(model.update().state == UpdateState::Quarantined);
    REQUIRE(model.view_state() == ViewState::Quarantined);
    REQUIRE(model.device_state() == DeviceState::Closed);
    REQUIRE(model.invariants_hold());
  }
  return true;
}

bool dead_publisher_recovery_covers_every_stage() {
  for (std::uint32_t death_point = 0; death_point < UINT32_C(6); ++death_point) {
    Model model(kLimits);
    REQUIRE(model.reserve_range(UINT32_C(1), UINT64_C(4)));
    REQUIRE(model.claim_publication(UINT32_C(1), true));
    if (death_point >= UINT32_C(1)) {
      REQUIRE(model.prepare_publication(UINT32_C(1)));
    }
    if (death_point >= UINT32_C(2)) {
      REQUIRE(model.link_publication(UINT32_C(1)));
    }
    if (death_point >= UINT32_C(3)) {
      REQUIRE(model.activate_publication(UINT32_C(1)));
    }
    if (death_point >= UINT32_C(4)) {
      REQUIRE(model.apply_publication(UINT32_C(1)));
    }
    if (death_point >= UINT32_C(5)) {
      REQUIRE(model.mark_publication(UINT32_C(1)));
    }

    REQUIRE(model.recover_dead_publication(UINT32_C(1)));
    REQUIRE(model.publication().state == PublishState::Terminal);
    REQUIRE(model.range().state == RangeState::Retired);
    REQUIRE(!model.intermediate_visible());
    if (death_point >= UINT32_C(2)) {
      REQUIRE(model.compensation_count() == UINT32_C(1));
    } else {
      REQUIRE(model.compensation_count() == UINT32_C(0));
    }
    REQUIRE(model.invariants_hold());
  }

  Model normal(kLimits);
  REQUIRE(normal.reserve_range(UINT32_C(1), UINT64_C(1)));
  REQUIRE(normal.claim_publication(UINT32_C(1), false));
  REQUIRE(normal.publication().state == PublishState::Initializing);
  REQUIRE(normal.prepare_publication(UINT32_C(1)));
  REQUIRE(normal.publication().state == PublishState::Prepared);
  REQUIRE(normal.link_publication(UINT32_C(1)));
  REQUIRE(normal.publication().state == PublishState::Linking);
  REQUIRE(normal.activate_publication(UINT32_C(1)));
  REQUIRE(normal.publication().state == PublishState::Active);
  REQUIRE(normal.apply_publication(UINT32_C(1)));
  REQUIRE(normal.mark_publication(UINT32_C(1)));
  REQUIRE(normal.publication().state == PublishState::Published);
  REQUIRE(normal.finish_publication(UINT32_C(1)));
  REQUIRE(normal.publication().state == PublishState::Terminal);

  Model aborted(kLimits);
  REQUIRE(aborted.reserve_range(UINT32_C(1), UINT64_C(2)));
  REQUIRE(aborted.claim_publication(UINT32_C(1), false));
  REQUIRE(aborted.abort_publication(UINT32_C(1)));
  REQUIRE(aborted.publication().state == PublishState::Aborted);
  REQUIRE(aborted.finish_publication(UINT32_C(1)));
  REQUIRE(aborted.publication().state == PublishState::Terminal);

  Model quarantined(kLimits);
  REQUIRE(quarantined.reserve_range(UINT32_C(1), UINT64_C(2)));
  REQUIRE(quarantined.claim_publication(UINT32_C(1), false));
  REQUIRE(quarantined.prepare_publication(UINT32_C(1)));
  REQUIRE(quarantined.link_publication(UINT32_C(1)));
  REQUIRE(quarantined.activate_publication(UINT32_C(1)));
  REQUIRE(quarantined.expire_live_publication(UINT32_C(1)));
  REQUIRE(quarantined.publication().state == PublishState::Quarantined);
  REQUIRE(quarantined.view_state() == ViewState::Quarantined);
  REQUIRE(quarantined.invariants_hold());
  return true;
}

bool head_only_publication_and_suffix_retirement_are_exhaustive() {
  for (std::uint32_t death_point = 0; death_point < UINT32_C(6); ++death_point) {
    Model model(kLimits);
    REQUIRE(model.reserve_range(UINT32_C(1), UINT64_C(4)));
    RangeToken first{};
    REQUIRE(model.current_range_token(first));
    REQUIRE(model.reserve_range(UINT32_C(2), UINT64_C(2)));
    RangeToken second{};
    REQUIRE(model.following_range_token(second));
    REQUIRE(model.range_token_valid(first));
    REQUIRE(model.range_token_valid(second));

    REQUIRE(!model.claim_publication(second, UINT32_C(1), false));
    REQUIRE(model.publication().state == PublishState::Free);
    REQUIRE(model.claim_publication(first, UINT32_C(1), true));
    REQUIRE(advance_publication_to_stage(model, death_point));
    REQUIRE(!model.claim_publication(second, UINT32_C(2), false));
    REQUIRE(model.recover_dead_publication(UINT32_C(1)));
    REQUIRE(model.publication().state == PublishState::Terminal);
    REQUIRE(model.range().state == RangeState::Retired);
    REQUIRE(!model.range_token_valid(first));

    const bool compensated = death_point >= UINT32_C(2);
    REQUIRE(model.publication_cursor() == (compensated ? UINT64_C(2) : UINT64_C(0)));
    REQUIRE(model.range().unused_suffix_begin == (compensated ? UINT64_C(3) : UINT64_C(1)));
    REQUIRE(model.range().unused_suffix_end == UINT64_C(4));
    REQUIRE(model.compensation_count() == (compensated ? UINT32_C(1) : UINT32_C(0)));

    REQUIRE(model.advance_range_head());
    REQUIRE(model.retired_range_count() == UINT32_C(1));
    RangeRecord first_retirement{};
    REQUIRE(model.retired_range(UINT32_C(0), first_retirement));
    REQUIRE(first_retirement.tag == UINT32_C(1));
    REQUIRE(first_retirement.unused_suffix_begin == (compensated ? UINT64_C(3) : UINT64_C(1)));
    REQUIRE(first_retirement.unused_suffix_end == UINT64_C(4));
    REQUIRE(!model.range_token_valid(first));
    REQUIRE(model.range_token_valid(second));

    REQUIRE(model.claim_publication(second, UINT32_C(2), false));
    REQUIRE(model.publication().target_sequence == UINT64_C(5));
    REQUIRE(!model.mark_publication(UINT32_C(1)));
    REQUIRE(model.prepare_publication(UINT32_C(2)));
    REQUIRE(model.link_publication(UINT32_C(2)));
    REQUIRE(model.activate_publication(UINT32_C(2)));
    REQUIRE(model.apply_publication(UINT32_C(2)));
    REQUIRE(model.mark_publication(UINT32_C(2)));
    REQUIRE(model.finish_publication(UINT32_C(2)));
    REQUIRE(!model.range_token_valid(second));
    RangeRecord retirement_after_second{};
    REQUIRE(model.retired_range(UINT32_C(0), retirement_after_second));
    REQUIRE(retirement_after_second.unused_suffix_begin == first_retirement.unused_suffix_begin);
    REQUIRE(retirement_after_second.unused_suffix_end == first_retirement.unused_suffix_end);
    REQUIRE(model.invariants_hold());
  }

  for (std::uint32_t expiry_point = 0; expiry_point < UINT32_C(6); ++expiry_point) {
    Model model(kLimits);
    REQUIRE(model.reserve_range(UINT32_C(1), UINT64_C(4)));
    RangeToken first{};
    REQUIRE(model.current_range_token(first));
    REQUIRE(model.reserve_range(UINT32_C(2), UINT64_C(2)));
    RangeToken second{};
    REQUIRE(model.following_range_token(second));
    REQUIRE(model.claim_publication(first, UINT32_C(1), true));
    REQUIRE(advance_publication_to_stage(model, expiry_point));
    REQUIRE(model.expire_live_publication(UINT32_C(1)));
    REQUIRE(model.view_state() == ViewState::Quarantined);
    REQUIRE(model.publication().state == PublishState::Quarantined);
    REQUIRE(model.range().state == RangeState::Quarantined);
    REQUIRE(model.following_range().state == RangeState::Quarantined);
    REQUIRE(!model.range_token_valid(first));
    REQUIRE(!model.range_token_valid(second));
    REQUIRE(!model.claim_publication(second, UINT32_C(2), false));
    REQUIRE(model.invariants_hold());
  }

  for (std::uint32_t loss_point = 0; loss_point < UINT32_C(6); ++loss_point) {
    Model model(kLimits);
    REQUIRE(model.reserve_range(UINT32_C(1), UINT64_C(4)));
    RangeToken first{};
    REQUIRE(model.current_range_token(first));
    REQUIRE(model.reserve_range(UINT32_C(2), UINT64_C(2)));
    RangeToken second{};
    REQUIRE(model.following_range_token(second));
    REQUIRE(model.claim_publication(first, UINT32_C(1), true));
    REQUIRE(advance_publication_to_stage(model, loss_point));
    model.close_view();
    REQUIRE(model.view_state() == ViewState::Quarantined);
    REQUIRE(model.publication().state == PublishState::Quarantined);
    REQUIRE(model.range().state == RangeState::Quarantined);
    REQUIRE(model.following_range().state == RangeState::Quarantined);
    REQUIRE(!model.prepare_publication(UINT32_C(1)));
    REQUIRE(!model.link_publication(UINT32_C(1)));
    REQUIRE(!model.activate_publication(UINT32_C(1)));
    REQUIRE(!model.apply_publication(UINT32_C(1)));
    REQUIRE(!model.mark_publication(UINT32_C(1)));
    REQUIRE(!model.finish_publication(UINT32_C(1)));
    REQUIRE(!model.claim_publication(second, UINT32_C(2), false));
    REQUIRE(model.invariants_hold());
  }

  Model compensation_exhausted(
      {.sequence_terminal = 3, .generation_terminal = 7, .tag_terminal = 7, .epoch_terminal = 7},
      UINT64_C(1));
  REQUIRE(compensation_exhausted.reserve_range(UINT32_C(1), UINT64_C(1)));
  REQUIRE(compensation_exhausted.claim_publication(UINT32_C(1), true));
  REQUIRE(compensation_exhausted.prepare_publication(UINT32_C(1)));
  REQUIRE(compensation_exhausted.link_publication(UINT32_C(1)));
  REQUIRE(compensation_exhausted.activate_publication(UINT32_C(1)));
  REQUIRE(compensation_exhausted.apply_publication(UINT32_C(1)));
  REQUIRE(compensation_exhausted.mark_publication(UINT32_C(1)));
  REQUIRE(!compensation_exhausted.recover_dead_publication(UINT32_C(1)));
  REQUIRE(compensation_exhausted.view_state() == ViewState::Quarantined);
  REQUIRE(compensation_exhausted.publication_cursor() == UINT64_C(2));
  REQUIRE(compensation_exhausted.invariants_hold());
  return true;
}

bool tag_reuse_rejects_stale_owners() {
  Model lease_model(kLimits);
  REQUIRE(lease_model.enter_attempt(UINT32_C(1)));
  REQUIRE(lease_model.reserve_lease(UINT32_C(1), UINT32_C(1)));
  AdmissionToken stale_admission{};
  REQUIRE(lease_model.current_admission_token(stale_admission));
  REQUIRE(lease_model.begin_lease_commit(UINT32_C(1)));
  REQUIRE(lease_model.finish_lease_commit(UINT32_C(1)));
  REQUIRE(lease_model.publish_lease(UINT32_C(1)));
  REQUIRE(lease_model.release_lease(UINT32_C(1)));
  REQUIRE(lease_model.reclaim_lease(UINT32_C(2)));
  REQUIRE(lease_model.lease().tag == UINT32_C(2));
  REQUIRE(lease_model.lease().state == LeaseState::Free);
  REQUIRE(!lease_model.begin_lease_commit(UINT32_C(1)));
  REQUIRE(!lease_model.publish_lease(UINT32_C(1)));
  REQUIRE(!lease_model.recover_dead_lease(UINT32_C(1)));
  REQUIRE(!lease_model.begin_lease_commit(stale_admission));
  REQUIRE(lease_model.lease().tag == UINT32_C(2));

  Model update_model(kLimits);
  REQUIRE(advance_update_to_stage(update_model, UINT32_C(5)));
  REQUIRE(update_model.finish_device_update(UINT32_C(1)));
  REQUIRE(update_model.claim_device_update(UINT32_C(2)));
  REQUIRE(update_model.update().tag == UINT32_C(2));
  REQUIRE(update_model.update().state == UpdateState::Initializing);
  REQUIRE(!update_model.publish_device_update(UINT32_C(1)));
  REQUIRE(!update_model.reopen_device_update(UINT32_C(1)));
  REQUIRE(!update_model.recover_dead_device_update(UINT32_C(1)));
  REQUIRE(update_model.update().tag == UINT32_C(2));
  REQUIRE(update_model.update().state == UpdateState::Initializing);

  Model publish_model(kLimits);
  REQUIRE(publish_model.reserve_range(UINT32_C(1), UINT64_C(1)));
  REQUIRE(publish_model.claim_publication(UINT32_C(1), false));
  REQUIRE(publish_model.prepare_publication(UINT32_C(1)));
  REQUIRE(publish_model.link_publication(UINT32_C(1)));
  REQUIRE(publish_model.activate_publication(UINT32_C(1)));
  REQUIRE(publish_model.apply_publication(UINT32_C(1)));
  REQUIRE(publish_model.mark_publication(UINT32_C(1)));
  REQUIRE(publish_model.finish_publication(UINT32_C(1)));
  REQUIRE(publish_model.reclaim_publication(UINT32_C(2)));
  REQUIRE(publish_model.publication().tag == UINT32_C(2));
  REQUIRE(publish_model.publication().state == PublishState::Free);
  REQUIRE(!publish_model.mark_publication(UINT32_C(1)));
  REQUIRE(!publish_model.recover_dead_publication(UINT32_C(1)));
  REQUIRE(publish_model.publication().tag == UINT32_C(2));
  REQUIRE(publish_model.invariants_hold());

  Model range_model(kLimits);
  REQUIRE(range_model.reserve_range(UINT32_C(1), UINT64_C(1)));
  RangeToken stale_range{};
  REQUIRE(range_model.current_range_token(stale_range));
  REQUIRE(range_model.claim_publication(stale_range, UINT32_C(1), false));
  REQUIRE(range_model.prepare_publication(UINT32_C(1)));
  REQUIRE(range_model.link_publication(UINT32_C(1)));
  REQUIRE(range_model.activate_publication(UINT32_C(1)));
  REQUIRE(range_model.apply_publication(UINT32_C(1)));
  REQUIRE(range_model.mark_publication(UINT32_C(1)));
  REQUIRE(range_model.finish_publication(UINT32_C(1)));
  REQUIRE(range_model.claim_range(UINT32_C(2), UINT64_C(1)));
  REQUIRE(range_model.prepare_range(UINT32_C(2)));
  REQUIRE(range_model.open_range(UINT32_C(2)));
  RangeToken current_range{};
  REQUIRE(range_model.current_range_token(current_range));
  REQUIRE(!range_model.range_token_valid(stale_range));
  REQUIRE(!range_model.claim_publication(stale_range, UINT32_C(2), false));
  REQUIRE(range_model.range_token_valid(current_range));
  REQUIRE(range_model.retired_range_count() == UINT32_C(1));
  RangeRecord retired{};
  REQUIRE(range_model.retired_range(UINT32_C(0), retired));
  REQUIRE(retired.tag == UINT32_C(1));
  REQUIRE(!range_model.retired_range(UINT32_C(1), retired));
  REQUIRE(range_model.invariants_hold());
  return true;
}

bool close_read_rechecks_and_old_view_replay_are_rejected() {
  Model original(kLimits, UINT64_C(0), UINT32_C(1), UINT64_C(1),
                 {.daemon_incarnation = UINT64_C(11), .view_serial = UINT64_C(7)});
  ReadSnapshot original_read{};
  REQUIRE(original.begin_read(original_read));
  REQUIRE(original.finish_read(original_read));
  REQUIRE(original.enter_attempt(UINT32_C(1)));
  REQUIRE(original.reserve_lease(UINT32_C(1), UINT32_C(1)));
  AdmissionToken original_admission{};
  REQUIRE(original.current_admission_token(original_admission));
  REQUIRE(original.reserve_range(UINT32_C(1), UINT64_C(2)));
  RangeToken original_range{};
  REQUIRE(original.current_range_token(original_range));

  Model replacement(kLimits, UINT64_C(0), UINT32_C(1), UINT64_C(1),
                    {.daemon_incarnation = UINT64_C(11), .view_serial = UINT64_C(8)});
  REQUIRE(replacement.enter_attempt(UINT32_C(1)));
  REQUIRE(replacement.reserve_lease(UINT32_C(1), UINT32_C(1)));
  REQUIRE(replacement.reserve_range(UINT32_C(1), UINT64_C(2)));
  REQUIRE(!replacement.finish_read(original_read));
  REQUIRE(!replacement.begin_lease_commit(original_admission));
  REQUIRE(!replacement.range_token_valid(original_range));
  REQUIRE(!replacement.claim_publication(original_range, UINT32_C(1), false));
  REQUIRE(replacement.invariants_hold());

  Model updated(kLimits);
  ReadSnapshot before_update{};
  REQUIRE(updated.begin_read(before_update));
  REQUIRE(advance_update_to_stage(updated, UINT32_C(5)));
  REQUIRE(updated.finish_device_update(UINT32_C(1)));
  REQUIRE(!updated.finish_read(before_update));
  ReadSnapshot after_update{};
  REQUIRE(updated.begin_read(after_update));
  REQUIRE(updated.finish_read(after_update));
  REQUIRE(after_update.device_generation == UINT32_C(2));
  REQUIRE(after_update.epoch == UINT64_C(2));

  Model stale_fence_writer(kLimits);
  ReadSnapshot before_loss{};
  REQUIRE(stale_fence_writer.begin_read(before_loss));
  REQUIRE(stale_fence_writer.prepare_device_update(UINT32_C(1)));
  REQUIRE(stale_fence_writer.link_device_update(UINT32_C(1)));
  REQUIRE(stale_fence_writer.device_state() == DeviceState::Updating);
  stale_fence_writer.close_device();
  REQUIRE(!stale_fence_writer.publish_device_update(UINT32_C(1)));
  REQUIRE(!stale_fence_writer.reopen_device_update(UINT32_C(1)));
  REQUIRE(!stale_fence_writer.expire_live_device_update(UINT32_C(1)));
  REQUIRE(!stale_fence_writer.finish_read(before_loss));
  REQUIRE(stale_fence_writer.device_state() == DeviceState::Closed);
  REQUIRE(stale_fence_writer.device_generation() == kLimits.generation_terminal);
  REQUIRE(stale_fence_writer.invariants_hold());

  Model stale_payload_writer(kLimits);
  REQUIRE(stale_payload_writer.reserve_range(UINT32_C(1), UINT64_C(2)));
  REQUIRE(stale_payload_writer.claim_publication(UINT32_C(1), false));
  REQUIRE(stale_payload_writer.prepare_publication(UINT32_C(1)));
  REQUIRE(stale_payload_writer.link_publication(UINT32_C(1)));
  REQUIRE(stale_payload_writer.activate_publication(UINT32_C(1)));
  ReadSnapshot before_payload_loss{};
  REQUIRE(stale_payload_writer.begin_read(before_payload_loss));
  stale_payload_writer.close_device();
  REQUIRE(stale_payload_writer.apply_publication(UINT32_C(1)));
  REQUIRE(stale_payload_writer.mark_publication(UINT32_C(1)));
  REQUIRE(stale_payload_writer.finish_publication(UINT32_C(1)));
  REQUIRE(stale_payload_writer.device_state() == DeviceState::Closed);
  REQUIRE(stale_payload_writer.device_generation() == kLimits.generation_terminal);
  REQUIRE(!stale_payload_writer.finish_read(before_payload_loss));
  REQUIRE(stale_payload_writer.invariants_hold());

  Model closed_view(kLimits);
  ReadSnapshot before_close{};
  REQUIRE(closed_view.begin_read(before_close));
  REQUIRE(closed_view.reserve_range(UINT32_C(1), UINT64_C(1)));
  RangeToken closing_token{};
  REQUIRE(closed_view.current_range_token(closing_token));
  closed_view.close_view();
  REQUIRE(!closed_view.finish_read(before_close));
  REQUIRE(!closed_view.range_token_valid(closing_token));
  REQUIRE(closed_view.view_state() == ViewState::Terminal);
  REQUIRE(closed_view.invariants_hold());

  Model invalidated_admission(kLimits);
  REQUIRE(invalidated_admission.enter_attempt(UINT32_C(1)));
  REQUIRE(invalidated_admission.reserve_lease(UINT32_C(1), UINT32_C(1)));
  AdmissionToken stale_admission{};
  REQUIRE(invalidated_admission.current_admission_token(stale_admission));
  REQUIRE(invalidated_admission.prepare_device_update(UINT32_C(1)));
  REQUIRE(invalidated_admission.link_device_update(UINT32_C(1)));
  REQUIRE(invalidated_admission.lease().state == LeaseState::Revoked);
  REQUIRE(!invalidated_admission.begin_lease_commit(stale_admission));
  REQUIRE(invalidated_admission.invariants_hold());
  return true;
}

bool generation_and_epoch_exhaustion_have_no_partial_effect() {
  for (std::uint32_t terminal = UINT32_C(3); terminal <= UINT32_C(15); ++terminal) {
    for (std::uint32_t initial = UINT32_C(1); initial < terminal; ++initial) {
      Model model({.sequence_terminal = UINT64_C(31),
                   .generation_terminal = terminal,
                   .tag_terminal = UINT32_C(31),
                   .epoch_terminal = UINT64_C(31)},
                  UINT64_C(0), initial);
      const bool expected = initial + UINT32_C(1) < terminal;
      const bool claimed = model.claim_range(UINT32_C(1), UINT64_C(1));
      REQUIRE(claimed == expected);
      if (claimed) {
        REQUIRE(model.prepare_range(UINT32_C(1)));
        REQUIRE(model.open_range(UINT32_C(1)));
        REQUIRE(model.range_token_generation() == initial + UINT32_C(1));
        REQUIRE(model.allocation_high_water() == UINT64_C(1));
      } else {
        REQUIRE(model.range().state == RangeState::Free);
        REQUIRE(model.range_token_generation() == initial);
        REQUIRE(model.allocation_high_water() == UINT64_C(0));
      }
      REQUIRE(model.invariants_hold());
    }
  }

  for (std::uint32_t generation_terminal = UINT32_C(3); generation_terminal <= UINT32_C(9);
       ++generation_terminal) {
    for (std::uint64_t epoch_terminal = UINT64_C(3); epoch_terminal <= UINT64_C(9);
         ++epoch_terminal) {
      Model model({.sequence_terminal = UINT64_C(31),
                   .generation_terminal = generation_terminal,
                   .tag_terminal = UINT32_C(31),
                   .epoch_terminal = epoch_terminal});
      const std::uint32_t expected_updates =
          std::min(generation_terminal - UINT32_C(2),
                   static_cast<std::uint32_t>(epoch_terminal - UINT64_C(2)));
      std::uint32_t completed = 0;
      for (std::uint32_t tag = UINT32_C(1); tag < UINT32_C(31); ++tag) {
        if (!model.prepare_device_update(tag)) {
          break;
        }
        REQUIRE(model.link_device_update(tag));
        REQUIRE(model.publish_device_update(tag));
        REQUIRE(model.reopen_device_update(tag));
        REQUIRE(model.finish_device_update(tag));
        ++completed;
      }
      REQUIRE(completed == expected_updates);
      const std::uint32_t prior_tag = model.update().tag;
      const UpdateState prior_state = model.update().state;
      const std::uint32_t prior_generation = model.device_generation();
      const std::uint64_t prior_epoch = model.epoch();
      REQUIRE(!model.claim_device_update(prior_tag + UINT32_C(1)));
      REQUIRE(model.update().tag == prior_tag);
      REQUIRE(model.update().state == prior_state);
      REQUIRE(model.device_generation() == prior_generation);
      REQUIRE(model.epoch() == prior_epoch);
      REQUIRE(model.device_generation() < generation_terminal);
      REQUIRE(model.epoch() < epoch_terminal);
      REQUIRE(model.invariants_hold());
    }
  }

  for (std::uint32_t tag_terminal = UINT32_C(3); tag_terminal <= UINT32_C(15); ++tag_terminal) {
    Model model({.sequence_terminal = UINT64_C(63),
                 .generation_terminal = UINT32_C(63),
                 .tag_terminal = tag_terminal,
                 .epoch_terminal = UINT64_C(63)});
    for (std::uint32_t tag = UINT32_C(1); tag < tag_terminal; ++tag) {
      REQUIRE(model.prepare_device_update(tag));
      REQUIRE(model.link_device_update(tag));
      REQUIRE(model.publish_device_update(tag));
      REQUIRE(model.reopen_device_update(tag));
      REQUIRE(model.finish_device_update(tag));
    }
    const std::uint32_t prior_tag = model.update().tag;
    const std::uint32_t prior_generation = model.device_generation();
    const std::uint64_t prior_epoch = model.epoch();
    REQUIRE(prior_tag == tag_terminal - UINT32_C(1));
    REQUIRE(!model.claim_device_update(tag_terminal));
    REQUIRE(model.update().tag == prior_tag);
    REQUIRE(model.device_generation() == prior_generation);
    REQUIRE(model.epoch() == prior_epoch);
    REQUIRE(model.invariants_hold());
  }
  return true;
}

bool short_sequence_exhaustion_has_no_partial_effect() {
  for (std::uint64_t terminal = UINT64_C(3); terminal <= UINT64_C(31); ++terminal) {
    for (std::uint64_t current = 0; current <= terminal; ++current) {
      for (std::uint64_t increment = 0; increment <= terminal + UINT64_C(1); ++increment) {
        std::uint64_t output = UINT64_C(0xfeed);
        const bool result = Model::checked_add_below_terminal(current, increment, terminal, output);
        const bool expected = current < terminal && increment < terminal - current;
        REQUIRE(result == expected);
        if (expected) {
          REQUIRE(output == current + increment);
          REQUIRE(output < terminal);
        } else {
          REQUIRE(output == UINT64_C(0xfeed));
        }
      }
    }
  }

  for (std::uint64_t terminal = UINT64_C(3); terminal <= UINT64_C(15); ++terminal) {
    for (std::uint64_t current = 0; current < terminal; ++current) {
      for (std::uint64_t length = UINT64_C(1); length <= terminal + UINT64_C(1); ++length) {
        Model model({.sequence_terminal = terminal,
                     .generation_terminal = UINT32_C(7),
                     .tag_terminal = UINT32_C(7),
                     .epoch_terminal = UINT64_C(7)},
                    current);
        const bool expected = length < terminal - current;
        const bool result = model.reserve_range(UINT32_C(1), length);
        REQUIRE(result == expected);
        if (expected) {
          REQUIRE(model.allocation_high_water() == current + length);
          REQUIRE(model.range().state == RangeState::Open);
        } else {
          REQUIRE(model.allocation_high_water() == current);
          REQUIRE(model.range().state == RangeState::Free);
        }
        REQUIRE(model.invariants_hold());
      }
    }
  }
  return true;
}

} // namespace

int main() {
  return close_vs_lease_commit_is_exhaustive() && device_update_never_reopens_after_close() &&
                 dead_owners_are_helped_or_tombstoned() &&
                 dead_publisher_recovery_covers_every_stage() &&
                 head_only_publication_and_suffix_retirement_are_exhaustive() &&
                 tag_reuse_rejects_stale_owners() &&
                 close_read_rechecks_and_old_view_replay_are_rejected() &&
                 generation_and_epoch_exhaustion_have_no_partial_effect() &&
                 short_sequence_exhaustion_has_no_partial_effect()
             ? 0
             : 1;
}
