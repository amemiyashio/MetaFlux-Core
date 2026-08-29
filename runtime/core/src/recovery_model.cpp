#include "metaflux/runtime/recovery_model.hpp"

#include <algorithm>
#include <limits>

namespace metaflux::runtime::recovery {

Model::Model(Limits limits, std::uint64_t initial_high_water, std::uint32_t initial_generation,
             std::uint64_t initial_epoch, ViewId view_id) noexcept
    : limits_(limits), view_id_(view_id), view_generation_(initial_generation),
      device_generation_(initial_generation), range_token_generation_(initial_generation),
      epoch_(initial_epoch), allocation_high_water_(initial_high_water),
      publication_cursor_(initial_high_water) {
  valid_ = limits_.sequence_terminal >= 3U && limits_.generation_terminal >= 3U &&
           limits_.tag_terminal >= 3U && limits_.epoch_terminal >= 3U &&
           initial_high_water < limits_.sequence_terminal && initial_generation > 0U &&
           initial_generation < limits_.generation_terminal && initial_epoch > 0U &&
           initial_epoch < limits_.epoch_terminal && view_id_.daemon_incarnation != 0U &&
           view_id_.view_serial != 0U;
  if (!valid_) {
    view_state_ = ViewState::Quarantined;
    device_state_ = DeviceState::Closed;
  }
}

bool Model::checked_add_below_terminal(std::uint64_t current, std::uint64_t increment,
                                       std::uint64_t terminal, std::uint64_t& out_value) noexcept {
  if (current >= terminal || increment >= terminal - current) {
    return false;
  }
  out_value = current + increment;
  return true;
}

bool Model::next_tag(std::uint32_t current, std::uint32_t proposed) const noexcept {
  return proposed > 0U && proposed < limits_.tag_terminal && current < proposed &&
         proposed - current == 1U;
}

bool Model::controls_match(std::uint32_t view_generation,
                           std::uint32_t device_generation) const noexcept {
  return valid_ && view_state_ == ViewState::Open && device_state_ == DeviceState::Open &&
         view_generation_ == view_generation && device_generation_ == device_generation;
}

bool Model::admission_token_matches(const AdmissionToken& token) const noexcept {
  return token.view_id.daemon_incarnation == view_id_.daemon_incarnation &&
         token.view_id.view_serial == view_id_.view_serial && token.attempt_tag == attempt_.tag &&
         token.lease_tag == lease_.tag && attempt_.target_lease_tag == lease_.tag &&
         attempt_.state == AttemptState::Ready && lease_.state == LeaseState::Reserved &&
         controls_match(token.view_generation, token.device_generation) &&
         token.view_generation == attempt_.view_generation &&
         token.device_generation == attempt_.device_generation &&
         token.view_generation == lease_.view_generation &&
         token.device_generation == lease_.device_generation;
}

bool Model::update_tuple_matches(std::uint32_t tag) const noexcept {
  return device_state_ == DeviceState::Updating && device_generation_ == update_.new_generation &&
         device_update_tag_ == tag && update_.tag == tag;
}

bool Model::enter_attempt(std::uint32_t attempt_tag) noexcept {
  if (!valid_ || view_state_ != ViewState::Open || device_state_ != DeviceState::Open ||
      (attempt_.state != AttemptState::Idle && attempt_.state != AttemptState::Exited) ||
      !next_tag(attempt_.tag, attempt_tag)) {
    return false;
  }
  attempt_ = {
      .tag = attempt_tag,
      .state = AttemptState::Entering,
      .owner = OwnerState::Live,
      .view_generation = view_generation_,
      .device_generation = device_generation_,
      .target_lease_tag = 0,
  };
  return true;
}

bool Model::claim_lease(std::uint32_t attempt_tag, std::uint32_t lease_tag) noexcept {
  if (attempt_.tag != attempt_tag || attempt_.state != AttemptState::Entering ||
      lease_.state != LeaseState::Free || !next_tag(lease_.tag, lease_tag) ||
      !controls_match(attempt_.view_generation, attempt_.device_generation)) {
    return false;
  }

  attempt_.state = AttemptState::Claiming;
  attempt_.target_lease_tag = lease_tag;
  lease_ = {
      .tag = lease_tag,
      .state = LeaseState::Initializing,
      .owner = OwnerState::Live,
      .view_generation = attempt_.view_generation,
      .device_generation = attempt_.device_generation,
      .target_written = false,
      .publication_marker = false,
  };
  attempt_.state = AttemptState::Initializing;
  return true;
}

bool Model::materialize_lease(std::uint32_t attempt_tag, std::uint32_t lease_tag) noexcept {
  if (attempt_.tag != attempt_tag || attempt_.state != AttemptState::Initializing ||
      attempt_.target_lease_tag != lease_tag || lease_.tag != lease_tag ||
      lease_.state != LeaseState::Initializing ||
      !controls_match(lease_.view_generation, lease_.device_generation)) {
    return false;
  }
  lease_.state = LeaseState::Reserved;
  attempt_.state = AttemptState::Ready;
  return true;
}

bool Model::reserve_lease(std::uint32_t attempt_tag, std::uint32_t lease_tag) noexcept {
  return claim_lease(attempt_tag, lease_tag) && materialize_lease(attempt_tag, lease_tag);
}

bool Model::current_admission_token(AdmissionToken& out_token) const noexcept {
  if (attempt_.state != AttemptState::Ready || lease_.state != LeaseState::Reserved ||
      attempt_.target_lease_tag != lease_.tag) {
    return false;
  }
  out_token = {
      .view_id = view_id_,
      .view_generation = lease_.view_generation,
      .device_generation = lease_.device_generation,
      .attempt_tag = attempt_.tag,
      .lease_tag = lease_.tag,
  };
  return true;
}

bool Model::begin_lease_commit(const AdmissionToken& token) noexcept {
  return admission_token_matches(token) && begin_lease_commit(token.lease_tag);
}

bool Model::begin_lease_commit(std::uint32_t lease_tag) noexcept {
  if (lease_.tag != lease_tag || lease_.state != LeaseState::Reserved ||
      attempt_.state != AttemptState::Ready ||
      !controls_match(lease_.view_generation, lease_.device_generation)) {
    return false;
  }
  lease_.state = LeaseState::Committing;
  return true;
}

bool Model::finish_lease_commit(std::uint32_t lease_tag) noexcept {
  if (lease_.tag != lease_tag || lease_.state != LeaseState::Committing) {
    return false;
  }
  lease_.state = LeaseState::Committed;
  ++committed_admissions_;
  return true;
}

bool Model::write_lease_target(std::uint32_t lease_tag) noexcept {
  if (lease_.tag != lease_tag || lease_.state != LeaseState::Committed) {
    return false;
  }
  lease_.target_written = true;
  return true;
}

bool Model::publish_lease(std::uint32_t lease_tag) noexcept {
  if (lease_.tag != lease_tag || lease_.state != LeaseState::Committed) {
    return false;
  }
  lease_.target_written = true;
  lease_.publication_marker = true;
  lease_.state = LeaseState::Published;
  return true;
}

bool Model::release_lease(std::uint32_t lease_tag) noexcept {
  if (lease_.tag != lease_tag || lease_.state != LeaseState::Published) {
    return false;
  }
  lease_.state = LeaseState::Released;
  if (attempt_.target_lease_tag == lease_tag) {
    attempt_.state = AttemptState::Exited;
  }
  return true;
}

bool Model::recover_dead_lease(std::uint32_t lease_tag) noexcept {
  if (lease_.tag != lease_tag) {
    return false;
  }
  lease_.owner = OwnerState::Dead;
  switch (lease_.state) {
  case LeaseState::Initializing:
  case LeaseState::Reserved:
  case LeaseState::Committing:
    lease_.state = LeaseState::Tombstoned;
    break;
  case LeaseState::Committed:
    if (lease_.target_written) {
      lease_.publication_marker = true;
      lease_.state = LeaseState::Released;
    } else {
      lease_.state = LeaseState::Tombstoned;
    }
    break;
  case LeaseState::Published:
    lease_.state = LeaseState::Released;
    break;
  case LeaseState::Released:
  case LeaseState::Revoked:
  case LeaseState::Tombstoned:
    break;
  case LeaseState::Free:
  case LeaseState::Quarantined:
    return false;
  }
  if (attempt_.target_lease_tag == lease_tag) {
    attempt_.owner = OwnerState::Dead;
    attempt_.state = AttemptState::Exited;
  }
  return true;
}

bool Model::expire_live_lease(std::uint32_t lease_tag) noexcept {
  if (lease_.tag != lease_tag || lease_.owner != OwnerState::Live || !has_live_lease()) {
    return false;
  }
  lease_.state = LeaseState::Quarantined;
  quarantine();
  return true;
}

bool Model::reclaim_lease(std::uint32_t new_tag) noexcept {
  if (lease_.state != LeaseState::Released && lease_.state != LeaseState::Revoked &&
      lease_.state != LeaseState::Tombstoned) {
    return false;
  }
  if (!next_tag(lease_.tag, new_tag) || attempt_.state != AttemptState::Exited) {
    return false;
  }
  lease_ = {.tag = new_tag};
  return true;
}

void Model::settle_lease_for_close() noexcept {
  switch (lease_.state) {
  case LeaseState::Initializing:
  case LeaseState::Committing:
    lease_.state = LeaseState::Tombstoned;
    break;
  case LeaseState::Reserved:
    lease_.state = LeaseState::Revoked;
    break;
  case LeaseState::Committed:
    if (lease_.target_written) {
      lease_.publication_marker = true;
      lease_.state = LeaseState::Released;
    } else {
      lease_.state = LeaseState::Tombstoned;
    }
    break;
  case LeaseState::Published:
    lease_.state = LeaseState::Released;
    break;
  case LeaseState::Free:
  case LeaseState::Released:
  case LeaseState::Revoked:
  case LeaseState::Tombstoned:
  case LeaseState::Quarantined:
    break;
  }
  if (attempt_.state != AttemptState::Idle && attempt_.state != AttemptState::Exited) {
    attempt_.state = AttemptState::Exited;
  }
}

void Model::close_view() noexcept {
  if (!valid_ || view_state_ == ViewState::Terminal || view_state_ == ViewState::Quarantined) {
    return;
  }
  if (publication_.state != PublishState::Free && publication_.state != PublishState::Terminal) {
    quarantine();
    return;
  }
  view_state_ = ViewState::Closing;
  view_generation_ = limits_.generation_terminal;
  close_device();
  settle_lease_for_close();
  view_state_ = ViewState::Terminal;
}

void Model::close_device() noexcept {
  if (!valid_ ||
      (device_state_ == DeviceState::Closed && device_generation_ == limits_.generation_terminal)) {
    return;
  }
  device_state_ = DeviceState::Closed;
  device_generation_ = limits_.generation_terminal;
  device_update_tag_ = 0;
  switch (update_.state) {
  case UpdateState::Initializing:
  case UpdateState::Prepared:
  case UpdateState::Linking:
  case UpdateState::Active:
  case UpdateState::FencePublished:
  case UpdateState::Reopened:
    update_.state = UpdateState::Closed;
    break;
  case UpdateState::Free:
  case UpdateState::Aborted:
  case UpdateState::Closed:
  case UpdateState::Terminal:
  case UpdateState::Quarantined:
    break;
  }
  settle_lease_for_close();
}

bool Model::claim_device_update(std::uint32_t update_tag) noexcept {
  std::uint64_t next_generation = 0;
  std::uint64_t next_epoch = 0;
  if (!valid_ || view_state_ != ViewState::Open || device_state_ != DeviceState::Open ||
      (update_.state != UpdateState::Free && update_.state != UpdateState::Terminal) ||
      !next_tag(update_.tag, update_tag) ||
      !checked_add_below_terminal(device_generation_, 1U, limits_.generation_terminal,
                                  next_generation) ||
      next_generation > std::numeric_limits<std::uint32_t>::max() ||
      !checked_add_below_terminal(epoch_, 1U, limits_.epoch_terminal, next_epoch)) {
    return false;
  }
  update_ = {
      .tag = update_tag,
      .state = UpdateState::Initializing,
      .owner = OwnerState::Live,
      .old_generation = device_generation_,
      .new_generation = static_cast<std::uint32_t>(next_generation),
      .old_epoch = epoch_,
      .new_epoch = next_epoch,
      .fence_published = false,
  };
  return true;
}

bool Model::materialize_device_update(std::uint32_t update_tag) noexcept {
  if (update_.tag != update_tag || update_.state != UpdateState::Initializing) {
    return false;
  }
  update_.state = UpdateState::Prepared;
  return true;
}

bool Model::prepare_device_update(std::uint32_t update_tag) noexcept {
  return claim_device_update(update_tag) && materialize_device_update(update_tag);
}

bool Model::begin_device_update_link(std::uint32_t update_tag) noexcept {
  if (update_.tag != update_tag || update_.state != UpdateState::Prepared) {
    return false;
  }
  update_.state = UpdateState::Linking;
  return true;
}

bool Model::complete_device_update_link(std::uint32_t update_tag) noexcept {
  if (update_.tag != update_tag || update_.state != UpdateState::Linking) {
    return false;
  }
  if (view_state_ != ViewState::Open || device_state_ != DeviceState::Open ||
      device_generation_ != update_.old_generation || epoch_ != update_.old_epoch) {
    update_.state = UpdateState::Closed;
    return false;
  }
  device_state_ = DeviceState::Updating;
  device_generation_ = update_.new_generation;
  epoch_ = update_.new_epoch;
  device_update_tag_ = update_tag;
  settle_lease_for_close();
  update_.state = UpdateState::Active;
  return true;
}

bool Model::link_device_update(std::uint32_t update_tag) noexcept {
  return begin_device_update_link(update_tag) && complete_device_update_link(update_tag);
}

bool Model::publish_device_update(std::uint32_t update_tag) noexcept {
  if (update_.tag != update_tag || update_.state != UpdateState::Active ||
      !update_tuple_matches(update_tag)) {
    return false;
  }
  update_.fence_published = true;
  update_.state = UpdateState::FencePublished;
  return true;
}

bool Model::reopen_device_update(std::uint32_t update_tag) noexcept {
  if (update_.tag != update_tag || update_.state != UpdateState::FencePublished ||
      !update_.fence_published || !update_tuple_matches(update_tag)) {
    return false;
  }
  device_state_ = DeviceState::Open;
  device_update_tag_ = 0;
  update_.state = UpdateState::Reopened;
  return true;
}

bool Model::finish_device_update(std::uint32_t update_tag) noexcept {
  if (update_.tag != update_tag ||
      (update_.state != UpdateState::Reopened && update_.state != UpdateState::Aborted &&
       update_.state != UpdateState::Closed)) {
    return false;
  }
  update_.state = UpdateState::Terminal;
  return true;
}

bool Model::recover_dead_device_update(std::uint32_t update_tag) noexcept {
  if (update_.tag != update_tag) {
    return false;
  }
  update_.owner = OwnerState::Dead;
  if (update_.state == UpdateState::Initializing || update_.state == UpdateState::Prepared) {
    update_.state = UpdateState::Aborted;
  } else if (update_.state == UpdateState::Linking) {
    if (view_state_ == ViewState::Open && device_state_ == DeviceState::Open &&
        device_generation_ == update_.old_generation && epoch_ == update_.old_epoch) {
      device_state_ = DeviceState::Updating;
      device_generation_ = update_.new_generation;
      epoch_ = update_.new_epoch;
      device_update_tag_ = update_tag;
      settle_lease_for_close();
      update_.state = UpdateState::Active;
    } else {
      update_.state = UpdateState::Closed;
    }
  }
  if (update_.state == UpdateState::Active && !publish_device_update(update_tag)) {
    update_.state = UpdateState::Closed;
  }
  if (update_.state == UpdateState::FencePublished && !reopen_device_update(update_tag)) {
    update_.state = UpdateState::Closed;
  }
  if (update_.state == UpdateState::Reopened || update_.state == UpdateState::Aborted ||
      update_.state == UpdateState::Closed) {
    update_.state = UpdateState::Terminal;
  }
  return update_.state == UpdateState::Terminal;
}

bool Model::expire_live_device_update(std::uint32_t update_tag) noexcept {
  if (update_.tag != update_tag || update_.owner != OwnerState::Live ||
      update_.state == UpdateState::Free || update_.state == UpdateState::Aborted ||
      update_.state == UpdateState::Closed || update_.state == UpdateState::Terminal ||
      update_.state == UpdateState::Quarantined) {
    return false;
  }
  update_.state = UpdateState::Quarantined;
  quarantine();
  return true;
}

RangeRecord* Model::initializing_range(std::uint32_t tag) noexcept {
  if (range_.tag == tag &&
      (range_.state == RangeState::Initializing || range_.state == RangeState::Prepared)) {
    return &range_;
  }
  if (following_range_.tag == tag && (following_range_.state == RangeState::Initializing ||
                                      following_range_.state == RangeState::Prepared)) {
    return &following_range_;
  }
  return nullptr;
}

bool Model::archive_retired_range(const RangeRecord& range) noexcept {
  if (range.state != RangeState::Retired || retired_range_count_ >= retired_ranges_.size()) {
    return false;
  }
  retired_ranges_[retired_range_count_] = range;
  ++retired_range_count_;
  return true;
}

bool Model::retired_range(std::uint32_t index, RangeRecord& out_range) const noexcept {
  if (index >= retired_range_count_) {
    return false;
  }
  out_range = retired_ranges_[index];
  return true;
}

bool Model::range_record_matches(const RangeRecord& range, const RangeToken& token) const noexcept {
  return token.view_id.daemon_incarnation == view_id_.daemon_incarnation &&
         token.view_id.view_serial == view_id_.view_serial &&
         token.view_generation == view_generation_ &&
         token.token_generation == range.token_generation && token.range_tag == range.tag &&
         token.range_begin == range.begin && token.range_end == range.end;
}

bool Model::claim_range(std::uint32_t range_tag, std::uint64_t length) noexcept {
  std::uint64_t target = 0;
  std::uint64_t next_token_generation = 0;
  if (!valid_ || length == 0U || view_state_ != ViewState::Open ||
      !next_tag(range_tag_high_water_, range_tag) ||
      !checked_add_below_terminal(allocation_high_water_, length, limits_.sequence_terminal,
                                  target) ||
      !checked_add_below_terminal(range_token_generation_, 1U, limits_.generation_terminal,
                                  next_token_generation) ||
      next_token_generation > std::numeric_limits<std::uint32_t>::max()) {
    return false;
  }

  RangeRecord* destination = nullptr;
  if (range_.state == RangeState::Free) {
    destination = &range_;
  } else if ((range_.state == RangeState::Retired || range_.state == RangeState::Terminal) &&
             following_range_.state == RangeState::Free) {
    if (range_.state == RangeState::Retired && !archive_retired_range(range_)) {
      return false;
    }
    range_ = {};
    destination = &range_;
  } else if (range_.state == RangeState::Open && following_range_.state == RangeState::Free) {
    destination = &following_range_;
  } else {
    return false;
  }

  range_tag_high_water_ = range_tag;
  *destination = {
      .tag = range_tag,
      .state = RangeState::Initializing,
      .begin = allocation_high_water_ + 1U,
      .end = target,
      .token_generation = static_cast<std::uint32_t>(next_token_generation),
      .unused_suffix_begin = 0,
      .unused_suffix_end = 0,
  };
  return true;
}

bool Model::prepare_range(std::uint32_t range_tag) noexcept {
  RangeRecord* candidate = initializing_range(range_tag);
  if (candidate == nullptr || candidate->state != RangeState::Initializing) {
    return false;
  }
  candidate->state = RangeState::Prepared;
  return true;
}

bool Model::open_range(std::uint32_t range_tag) noexcept {
  RangeRecord* candidate = initializing_range(range_tag);
  std::uint64_t next_token_generation = 0;
  if (candidate == nullptr || candidate->state != RangeState::Prepared || candidate->begin == 0U ||
      candidate->begin - 1U != allocation_high_water_ ||
      candidate->end >= limits_.sequence_terminal ||
      !checked_add_below_terminal(range_token_generation_, 1U, limits_.generation_terminal,
                                  next_token_generation) ||
      candidate->token_generation != next_token_generation ||
      (candidate == &following_range_ && range_.state != RangeState::Open)) {
    return false;
  }
  allocation_high_water_ = candidate->end;
  range_token_generation_ = candidate->token_generation;
  candidate->state = RangeState::Open;
  return true;
}

bool Model::reserve_range(std::uint32_t range_tag, std::uint64_t length) noexcept {
  return claim_range(range_tag, length) && prepare_range(range_tag) && open_range(range_tag);
}

bool Model::current_range_token(RangeToken& out_token) const noexcept {
  if (range_.state != RangeState::Open) {
    return false;
  }
  out_token = {
      .view_id = view_id_,
      .view_generation = view_generation_,
      .token_generation = range_.token_generation,
      .range_tag = range_.tag,
      .range_begin = range_.begin,
      .range_end = range_.end,
  };
  return true;
}

bool Model::following_range_token(RangeToken& out_token) const noexcept {
  if (following_range_.state != RangeState::Open) {
    return false;
  }
  out_token = {
      .view_id = view_id_,
      .view_generation = view_generation_,
      .token_generation = following_range_.token_generation,
      .range_tag = following_range_.tag,
      .range_begin = following_range_.begin,
      .range_end = following_range_.end,
  };
  return true;
}

bool Model::range_token_valid(const RangeToken& token) const noexcept {
  return valid_ && view_state_ == ViewState::Open && token.view_generation == view_generation_ &&
         ((range_.state == RangeState::Open && range_record_matches(range_, token)) ||
          (following_range_.state == RangeState::Open &&
           range_record_matches(following_range_, token)));
}

bool Model::advance_range_head() noexcept {
  if (range_.state != RangeState::Retired || following_range_.state != RangeState::Open ||
      (publication_.state != PublishState::Free && publication_.state != PublishState::Terminal) ||
      !archive_retired_range(range_)) {
    return false;
  }
  range_ = following_range_;
  following_range_ = {};
  return true;
}

bool Model::claim_publication(const RangeToken& token, std::uint32_t publish_tag,
                              bool requires_compensation) noexcept {
  std::uint64_t next_cursor = 0;
  if (!range_token_valid(token) || !range_record_matches(range_, token) ||
      (publication_.state != PublishState::Free && publication_.state != PublishState::Terminal) ||
      !next_tag(publication_.tag, publish_tag) ||
      !checked_add_below_terminal(publication_cursor_, 1U, limits_.sequence_terminal,
                                  next_cursor)) {
    return false;
  }
  const std::uint64_t target = std::max(range_.begin, next_cursor);
  if (target > range_.end) {
    return false;
  }
  publication_ = {
      .tag = publish_tag,
      .state = PublishState::Initializing,
      .owner = OwnerState::Live,
      .range_tag = token.range_tag,
      .token_generation = token.token_generation,
      .range_begin = token.range_begin,
      .range_end = token.range_end,
      .target_sequence = target,
      .payload_applied = false,
      .requires_compensation = requires_compensation,
  };
  return true;
}

bool Model::claim_publication(std::uint32_t publish_tag, bool requires_compensation) noexcept {
  RangeToken token{};
  return current_range_token(token) && claim_publication(token, publish_tag, requires_compensation);
}

bool Model::publication_range_matches() const noexcept {
  return range_.state == RangeState::Open && publication_.range_tag == range_.tag &&
         publication_.token_generation == range_.token_generation &&
         publication_.range_begin == range_.begin && publication_.range_end == range_.end;
}

bool Model::prepare_publication(std::uint32_t publish_tag) noexcept {
  if (publication_.tag != publish_tag || publication_.state != PublishState::Initializing) {
    return false;
  }
  publication_.state = PublishState::Prepared;
  return true;
}

bool Model::link_publication(std::uint32_t publish_tag) noexcept {
  if (publication_.tag != publish_tag || publication_.state != PublishState::Prepared) {
    return false;
  }
  publication_.state = PublishState::Linking;
  return true;
}

bool Model::activate_publication(std::uint32_t publish_tag) noexcept {
  if (publication_.tag != publish_tag || publication_.state != PublishState::Linking ||
      view_state_ != ViewState::Open || !publication_range_matches()) {
    return false;
  }
  publication_.state = PublishState::Active;
  return true;
}

bool Model::apply_publication(std::uint32_t publish_tag) noexcept {
  std::uint64_t next_cursor = 0;
  if (publication_.tag != publish_tag || publication_.state != PublishState::Active ||
      publication_.payload_applied || view_state_ != ViewState::Open ||
      !publication_range_matches() ||
      !checked_add_below_terminal(publication_cursor_, 1U, limits_.sequence_terminal,
                                  next_cursor) ||
      publication_.target_sequence != std::max(range_.begin, next_cursor)) {
    return false;
  }
  publication_cursor_ = publication_.target_sequence;
  publication_.payload_applied = true;
  intermediate_visible_ = publication_.requires_compensation;
  return true;
}

bool Model::mark_publication(std::uint32_t publish_tag) noexcept {
  if (publication_.tag != publish_tag || publication_.state != PublishState::Active ||
      !publication_.payload_applied || view_state_ != ViewState::Open ||
      !publication_range_matches() || publication_cursor_ != publication_.target_sequence) {
    return false;
  }
  publication_.state = PublishState::Published;
  return true;
}

bool Model::retire_range() noexcept {
  if (!publication_range_matches()) {
    return false;
  }
  if (publication_cursor_ < range_.begin) {
    range_.unused_suffix_begin = range_.begin;
    range_.unused_suffix_end = range_.end;
  } else if (publication_cursor_ < range_.end) {
    range_.unused_suffix_begin = publication_cursor_ + 1U;
    range_.unused_suffix_end = range_.end;
  } else {
    range_.unused_suffix_begin = 0;
    range_.unused_suffix_end = 0;
  }
  range_.state = RangeState::Retired;
  return true;
}

bool Model::finish_publication(std::uint32_t publish_tag) noexcept {
  if (publication_.tag != publish_tag) {
    return false;
  }
  if (publication_.state == PublishState::Aborted) {
    if (!retire_range()) {
      return false;
    }
    publication_.state = PublishState::Terminal;
    return true;
  }
  if (publication_.state != PublishState::Published || intermediate_visible_) {
    return false;
  }
  if (!retire_range()) {
    return false;
  }
  publication_.state = PublishState::Terminal;
  return true;
}

bool Model::abort_publication(std::uint32_t publish_tag) noexcept {
  if (publication_.tag != publish_tag || (publication_.state != PublishState::Initializing &&
                                          publication_.state != PublishState::Prepared)) {
    return false;
  }
  publication_.state = PublishState::Aborted;
  return true;
}

bool Model::compensate() noexcept {
  std::uint64_t target = 0;
  if (!intermediate_visible_ || view_state_ != ViewState::Open || !publication_range_matches() ||
      !checked_add_below_terminal(publication_cursor_, 1U, limits_.sequence_terminal, target) ||
      target > range_.end) {
    return false;
  }
  publication_cursor_ = target;
  intermediate_visible_ = false;
  ++compensation_count_;
  return true;
}

bool Model::recover_dead_publication(std::uint32_t publish_tag) noexcept {
  if (publication_.tag != publish_tag) {
    return false;
  }
  publication_.owner = OwnerState::Dead;
  if (publication_.state == PublishState::Initializing ||
      publication_.state == PublishState::Prepared) {
    publication_.state = PublishState::Aborted;
  }
  if (publication_.state == PublishState::Aborted) {
    return finish_publication(publish_tag);
  }
  if (publication_.state == PublishState::Linking) {
    if (!activate_publication(publish_tag)) {
      quarantine();
      publication_.state = PublishState::Quarantined;
      return false;
    }
  }
  if (publication_.state == PublishState::Active && !publication_.payload_applied &&
      !apply_publication(publish_tag)) {
    quarantine();
    publication_.state = PublishState::Quarantined;
    return false;
  }
  if (publication_.state == PublishState::Active && !mark_publication(publish_tag)) {
    quarantine();
    publication_.state = PublishState::Quarantined;
    return false;
  }
  if (publication_.state == PublishState::Published && intermediate_visible_ && !compensate()) {
    quarantine();
    publication_.state = PublishState::Quarantined;
    return false;
  }
  if (publication_.state == PublishState::Published) {
    return finish_publication(publish_tag);
  }
  return publication_.state == PublishState::Terminal;
}

void Model::quarantine() noexcept {
  view_state_ = ViewState::Quarantined;
  view_generation_ = limits_.generation_terminal;
  device_state_ = DeviceState::Closed;
  device_generation_ = limits_.generation_terminal;
  device_update_tag_ = 0;
  if (has_live_lease()) {
    lease_.state = LeaseState::Quarantined;
  }
  if (update_.state != UpdateState::Free && update_.state != UpdateState::Terminal &&
      update_.state != UpdateState::Closed && update_.state != UpdateState::Aborted) {
    update_.state = UpdateState::Quarantined;
  }
  if (publication_.state != PublishState::Free && publication_.state != PublishState::Terminal &&
      publication_.state != PublishState::Aborted) {
    publication_.state = PublishState::Quarantined;
  }
  if (range_.state == RangeState::Initializing || range_.state == RangeState::Prepared ||
      range_.state == RangeState::Open) {
    range_.state = RangeState::Quarantined;
  }
  if (following_range_.state == RangeState::Initializing ||
      following_range_.state == RangeState::Prepared ||
      following_range_.state == RangeState::Open) {
    following_range_.state = RangeState::Quarantined;
  }
  if (attempt_.state != AttemptState::Idle && attempt_.state != AttemptState::Exited) {
    attempt_.state = AttemptState::Exited;
  }
}

bool Model::expire_live_publication(std::uint32_t publish_tag) noexcept {
  if (publication_.tag != publish_tag || publication_.owner != OwnerState::Live ||
      publication_.state == PublishState::Free || publication_.state == PublishState::Aborted ||
      publication_.state == PublishState::Terminal ||
      publication_.state == PublishState::Quarantined) {
    return false;
  }
  publication_.state = PublishState::Quarantined;
  range_.state = RangeState::Quarantined;
  quarantine();
  return true;
}

bool Model::reclaim_publication(std::uint32_t new_tag) noexcept {
  if (publication_.state != PublishState::Terminal || range_.state != RangeState::Retired ||
      !next_tag(publication_.tag, new_tag)) {
    return false;
  }
  publication_ = {.tag = new_tag};
  return true;
}

bool Model::begin_read(ReadSnapshot& out_snapshot) const noexcept {
  if (!controls_match(view_generation_, device_generation_)) {
    return false;
  }
  out_snapshot = {
      .view_id = view_id_,
      .view_generation = view_generation_,
      .device_generation = device_generation_,
      .epoch = epoch_,
  };
  return true;
}

bool Model::finish_read(const ReadSnapshot& snapshot) const noexcept {
  return snapshot.view_id.daemon_incarnation == view_id_.daemon_incarnation &&
         snapshot.view_id.view_serial == view_id_.view_serial && snapshot.epoch == epoch_ &&
         controls_match(snapshot.view_generation, snapshot.device_generation);
}

bool Model::has_live_lease() const noexcept {
  switch (lease_.state) {
  case LeaseState::Initializing:
  case LeaseState::Reserved:
  case LeaseState::Committing:
  case LeaseState::Committed:
  case LeaseState::Published:
    return true;
  case LeaseState::Free:
  case LeaseState::Released:
  case LeaseState::Revoked:
  case LeaseState::Tombstoned:
  case LeaseState::Quarantined:
    return false;
  }
  return false;
}

bool Model::invariants_hold() const noexcept {
  if (!valid_ || allocation_high_water_ >= limits_.sequence_terminal ||
      publication_cursor_ > allocation_high_water_ || view_generation_ == 0U ||
      view_generation_ > limits_.generation_terminal || device_generation_ == 0U ||
      device_generation_ > limits_.generation_terminal || range_token_generation_ == 0U ||
      range_token_generation_ >= limits_.generation_terminal || epoch_ == 0U ||
      epoch_ >= limits_.epoch_terminal || attempt_.tag >= limits_.tag_terminal ||
      lease_.tag >= limits_.tag_terminal || update_.tag >= limits_.tag_terminal ||
      range_tag_high_water_ >= limits_.tag_terminal || range_.tag >= limits_.tag_terminal ||
      following_range_.tag >= limits_.tag_terminal || publication_.tag >= limits_.tag_terminal ||
      retired_range_count_ > retired_ranges_.size()) {
    return false;
  }
  if ((view_state_ == ViewState::Terminal || view_state_ == ViewState::Quarantined) &&
      (device_state_ != DeviceState::Closed || device_generation_ != limits_.generation_terminal ||
       has_live_lease())) {
    return false;
  }
  if (device_state_ == DeviceState::Updating &&
      (device_update_tag_ == 0U || update_.tag != device_update_tag_ ||
       (update_.state != UpdateState::Active && update_.state != UpdateState::FencePublished))) {
    return false;
  }
  if ((update_.state == UpdateState::Active || update_.state == UpdateState::FencePublished) &&
      (update_.new_generation != device_generation_ || update_.new_epoch != epoch_)) {
    return false;
  }
  if (intermediate_visible_ &&
      (!publication_.payload_applied || (publication_.state != PublishState::Active &&
                                         publication_.state != PublishState::Published &&
                                         publication_.state != PublishState::Quarantined))) {
    return false;
  }
  if (intermediate_visible_ && view_state_ != ViewState::Open &&
      view_state_ != ViewState::Quarantined) {
    return false;
  }
  const auto valid_range = [this](const RangeRecord& range) {
    if (range.state == RangeState::Free) {
      return range.begin == 0U && range.end == 0U;
    }
    if (range.begin == 0U || range.begin > range.end || range.end >= limits_.sequence_terminal ||
        range.token_generation == 0U || range.token_generation >= limits_.generation_terminal) {
      return false;
    }
    if (range.state != RangeState::Retired) {
      return range.unused_suffix_begin == 0U && range.unused_suffix_end == 0U;
    }
    return (range.unused_suffix_begin == 0U && range.unused_suffix_end == 0U) ||
           (range.unused_suffix_begin >= range.begin && range.unused_suffix_begin <= range.end &&
            range.unused_suffix_end == range.end);
  };
  if (!valid_range(range_) || !valid_range(following_range_)) {
    return false;
  }
  if (following_range_.state != RangeState::Free && range_.state != RangeState::Open &&
      range_.state != RangeState::Retired && range_.state != RangeState::Quarantined) {
    return false;
  }
  if (following_range_.state != RangeState::Free && range_.end + 1U != following_range_.begin) {
    return false;
  }
  for (std::uint32_t index = 0; index < retired_range_count_; ++index) {
    if (retired_ranges_[index].state != RangeState::Retired ||
        !valid_range(retired_ranges_[index])) {
      return false;
    }
  }
  if ((publication_.state == PublishState::Initializing ||
       publication_.state == PublishState::Prepared ||
       publication_.state == PublishState::Linking || publication_.state == PublishState::Active ||
       publication_.state == PublishState::Published) &&
      !publication_range_matches()) {
    return false;
  }
  return true;
}

} // namespace metaflux::runtime::recovery
