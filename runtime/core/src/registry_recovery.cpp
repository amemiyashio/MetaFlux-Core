#include "metaflux/runtime/core.hpp"

#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <time.h>

namespace metaflux::runtime {
namespace {

template <typename Record> class RecordHazard final {
public:
  RecordHazard(Record& record, std::uint64_t Record::* control_word,
               std::uint64_t expected_word) noexcept
      : record_(&record) {
    const std::uint32_t previous = mf_atomic_fetch_add_u32_acq_rel(&record.hazard_references, 1U);
    if (previous == std::numeric_limits<std::uint32_t>::max()) {
      (void)mf_atomic_fetch_add_u32_acq_rel(&record.hazard_references,
                                            std::numeric_limits<std::uint32_t>::max());
      return;
    }
    held_ = true;
    if (mf_atomic_load_u64_seq_cst(&(record.*control_word)) != expected_word) {
      release();
      return;
    }
  }

  RecordHazard(const RecordHazard&) = delete;
  RecordHazard& operator=(const RecordHazard&) = delete;

  ~RecordHazard() { release(); }

  [[nodiscard]] bool held() const noexcept { return held_; }

private:
  void release() noexcept {
    if (record_ != nullptr && held_) {
      (void)mf_atomic_fetch_add_u32_acq_rel(&record_->hazard_references,
                                            std::numeric_limits<std::uint32_t>::max());
      held_ = false;
    } else if (record_ != nullptr) {
      record_ = nullptr;
    }
  }

  Record* record_ = nullptr;
  bool held_ = false;
};

template <typename Record, typename Reusable, typename Pack>
[[nodiscard]] mf_shared_status_v1
claim_record(Record* records, std::uint32_t capacity, std::uint64_t Record::* control_word,
             std::uint32_t initial_state, std::uint32_t auxiliary, Reusable reusable, Pack pack,
             std::uint32_t& out_slot, std::uint32_t& out_tag) noexcept {
  bool exhausted = false;
  for (std::uint32_t index = 0; index < capacity; ++index) {
    auto* word = &(records[index].*control_word);
    std::uint64_t observed = mf_atomic_load_u64_seq_cst(word);
    if (!reusable(mf_tagged_record_state_v1(observed))) {
      continue;
    }
    if (mf_atomic_load_u32_acquire(&records[index].holder_references) != 0U ||
        mf_atomic_load_u32_acquire(&records[index].hazard_references) != 0U) {
      continue;
    }
    std::uint32_t next_tag = 0;
    if (!mf_shared_checked_next_record_tag_v1(mf_tagged_record_tag_v1(observed), &next_tag)) {
      exhausted = true;
      continue;
    }
    const std::uint64_t desired = pack(next_tag, initial_state, auxiliary);
    if (mf_atomic_compare_exchange_u64_seq_cst(word, &observed, desired)) {
      out_slot = index;
      out_tag = next_tag;
      return MF_SHARED_SUCCESS;
    }
  }
  return exhausted ? MF_SHARED_RESOURCE_EXHAUSTED : MF_SHARED_WOULD_BLOCK;
}

[[nodiscard]] bool same_owner(mf_owner_identity_v1 left, mf_owner_identity_v1 right) noexcept {
  return left.pid == right.pid && left.start_time_ticks == right.start_time_ticks &&
         left.reserved == 0U && right.reserved == 0U;
}

[[nodiscard]] bool same_token_view(mf_registry_view_id_v1 left,
                                   mf_registry_view_id_v1 right) noexcept {
  return mf_registry_view_id_equal_v1(left, right) != 0;
}

[[nodiscard]] bool read_start_time(std::uint32_t pid, std::uint64_t& out_start) noexcept {
  char path[64]{};
  char buffer[4096]{};
  if (std::snprintf(path, sizeof(path), "/proc/%u/stat", pid) <= 0) {
    return false;
  }
  std::FILE* file = std::fopen(path, "r");
  if (file == nullptr) {
    return false;
  }
  const bool read = std::fgets(buffer, static_cast<int>(sizeof(buffer)), file) != nullptr;
  (void)std::fclose(file);
  if (!read) {
    return false;
  }
  char* cursor = std::strrchr(buffer, ')');
  if (cursor == nullptr) {
    return false;
  }
  ++cursor;
  for (std::uint32_t field = 3U; field < 22U; ++field) {
    while (*cursor == ' ') {
      ++cursor;
    }
    if (*cursor == '\0') {
      return false;
    }
    while (*cursor != '\0' && *cursor != ' ') {
      ++cursor;
    }
  }
  while (*cursor == ' ') {
    ++cursor;
  }
  char* end = nullptr;
  const unsigned long long parsed = std::strtoull(cursor, &end, 10);
  if (end == cursor || parsed == 0U) {
    return false;
  }
  out_start = static_cast<std::uint64_t>(parsed);
  return true;
}

[[nodiscard]] bool process_is_alive(mf_owner_identity_v1 owner) noexcept {
  std::uint64_t observed = 0;
  return owner.pid != 0U && owner.reserved == 0U && read_start_time(owner.pid, observed) &&
         observed == owner.start_time_ticks;
}

[[nodiscard]] std::uint64_t monotonic_now_ns() noexcept {
  timespec now{};
  if (::clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
    return 0U;
  }
  return static_cast<std::uint64_t>(now.tv_sec) * UINT64_C(1000000000) +
         static_cast<std::uint64_t>(now.tv_nsec);
}

[[nodiscard]] bool deadline_expired(std::uint64_t deadline) noexcept {
  const std::uint64_t now = monotonic_now_ns();
  return deadline != 0U && now != 0U && now >= deadline;
}

[[nodiscard]] mf_shared_status_v1 acquire_publisher(std::uint64_t* control, std::uint32_t tag,
                                                    std::uint32_t slot) noexcept {
  std::uint64_t observed = mf_atomic_load_u64_seq_cst(control);
  if (mf_tagged_record_state_v1(observed) != MF_PUBLISHER_IDLE) {
    return MF_SHARED_WOULD_BLOCK;
  }
  const std::uint64_t desired = mf_publisher_control_pack_v1(tag, MF_PUBLISHER_WRITING, slot);
  return mf_atomic_compare_exchange_u64_seq_cst(control, &observed, desired) ? MF_SHARED_SUCCESS
                                                                             : MF_SHARED_RETRY;
}

void release_publisher(std::uint64_t* control, std::uint32_t tag, std::uint32_t slot) noexcept {
  std::uint64_t expected = mf_publisher_control_pack_v1(tag, MF_PUBLISHER_WRITING, slot);
  (void)mf_atomic_compare_exchange_u64_seq_cst(
      control, &expected,
      mf_publisher_control_pack_v1(tag, MF_PUBLISHER_IDLE, MF_SHARED_RECORD_SLOT_NONE));
}

[[nodiscard]] mf_shared_status_v1 settle_view_publication(mf_view_publish_record_v1& publication,
                                                          mf_view_publisher_control_v1& publisher,
                                                          std::uint32_t tag, std::uint32_t slot,
                                                          std::uint32_t range_slot,
                                                          std::uint32_t settled_state) noexcept {
  std::uint64_t current = mf_atomic_load_u64_seq_cst(&publication.tagged_state);
  if (mf_tagged_record_tag_v1(current) != tag) {
    return MF_SHARED_STALE_HANDLE;
  }
  if (mf_tagged_record_state_v1(current) != MF_VIEW_PUBLISH_TERMINAL &&
      mf_tagged_record_state_v1(current) != settled_state) {
    if (!mf_atomic_compare_exchange_u64_seq_cst(
            &publication.tagged_state, &current,
            mf_view_publish_state_pack_v1(tag, settled_state, range_slot)) &&
        (mf_tagged_record_tag_v1(current) != tag ||
         mf_tagged_record_state_v1(current) != settled_state)) {
      return MF_SHARED_RETRY;
    }
  }
  release_publisher(&publisher.tagged_owner, tag, slot);
  return MF_SHARED_SUCCESS;
}

[[nodiscard]] mf_shared_status_v1
terminalize_view_publication(mf_view_publish_record_v1& publication, std::uint32_t tag,
                             std::uint32_t range_slot, std::uint32_t settled_state) noexcept {
  std::uint64_t current = mf_atomic_load_u64_seq_cst(&publication.tagged_state);
  if (mf_tagged_record_tag_v1(current) != tag) {
    return MF_SHARED_STALE_HANDLE;
  }
  if (mf_tagged_record_state_v1(current) == MF_VIEW_PUBLISH_TERMINAL) {
    return MF_SHARED_SUCCESS;
  }
  if (mf_tagged_record_state_v1(current) != settled_state) {
    return MF_SHARED_RETRY;
  }
  return mf_atomic_compare_exchange_u64_seq_cst(
             &publication.tagged_state, &current,
             mf_view_publish_state_pack_v1(tag, MF_VIEW_PUBLISH_TERMINAL, range_slot))
             ? MF_SHARED_SUCCESS
             : MF_SHARED_RETRY;
}

[[nodiscard]] mf_shared_status_v1 finish_view_publication(mf_view_publish_record_v1& publication,
                                                          mf_view_publisher_control_v1& publisher,
                                                          std::uint32_t tag, std::uint32_t slot,
                                                          std::uint32_t range_slot,
                                                          std::uint32_t settled_state) noexcept {
  const mf_shared_status_v1 settled =
      settle_view_publication(publication, publisher, tag, slot, range_slot, settled_state);
  if (settled != MF_SHARED_SUCCESS) {
    return settled;
  }
  return terminalize_view_publication(publication, tag, range_slot, settled_state);
}

[[nodiscard]] mf_shared_status_v1
terminalize_telemetry_publication(mf_telemetry_publish_record_v1& publication, std::uint32_t tag,
                                  std::uint32_t target_bank, std::uint32_t settled_state) noexcept {
  std::uint64_t current = mf_atomic_load_u64_seq_cst(&publication.tagged_state);
  if (mf_tagged_record_tag_v1(current) != tag) {
    return MF_SHARED_STALE_HANDLE;
  }
  if (mf_tagged_record_state_v1(current) == MF_TELEMETRY_PUBLISH_TERMINAL) {
    return MF_SHARED_SUCCESS;
  }
  if (mf_tagged_record_state_v1(current) != settled_state) {
    return MF_SHARED_RETRY;
  }
  return mf_atomic_compare_exchange_u64_seq_cst(
             &publication.tagged_state, &current,
             mf_telemetry_publish_state_pack_v1(tag, MF_TELEMETRY_PUBLISH_TERMINAL, target_bank))
             ? MF_SHARED_SUCCESS
             : MF_SHARED_RETRY;
}

[[nodiscard]] mf_shared_status_v1
finish_telemetry_publication(mf_telemetry_publish_record_v1& publication,
                             mf_telemetry_publisher_control_v1& publisher, std::uint32_t tag,
                             std::uint32_t slot, std::uint32_t target_bank,
                             std::uint32_t settled_state) noexcept {
  std::uint64_t current = mf_atomic_load_u64_seq_cst(&publication.tagged_state);
  if (mf_tagged_record_tag_v1(current) != tag) {
    return MF_SHARED_STALE_HANDLE;
  }
  if (mf_tagged_record_state_v1(current) != MF_TELEMETRY_PUBLISH_TERMINAL &&
      mf_tagged_record_state_v1(current) != settled_state) {
    if (!mf_atomic_compare_exchange_u64_seq_cst(
            &publication.tagged_state, &current,
            mf_telemetry_publish_state_pack_v1(tag, settled_state, target_bank)) &&
        (mf_tagged_record_tag_v1(current) != tag ||
         mf_tagged_record_state_v1(current) != settled_state)) {
      return MF_SHARED_RETRY;
    }
  }
  release_publisher(&publisher.tagged_owner, tag, slot);
  return terminalize_telemetry_publication(publication, tag, target_bank, settled_state);
}

[[nodiscard]] mf_shared_status_v1 stable_high_water(mf_registry_view_control_v1* control,
                                                    std::uint64_t& out_high_water) noexcept {
  for (std::uint32_t attempt = 0; attempt < 8U; ++attempt) {
    const std::uint64_t first = mf_atomic_load_u64_acquire(&control->control_latch_sequence);
    if ((first & 1U) != 0U) {
      continue;
    }
    const std::uint64_t value = mf_atomic_load_u64_relaxed(&control->allocation_high_water);
    mf_atomic_signal_fence_seq_cst();
    if (mf_atomic_load_u64_acquire(&control->control_latch_sequence) == first) {
      out_high_water = value;
      return MF_SHARED_SUCCESS;
    }
  }
  return MF_SHARED_RETRY;
}

[[nodiscard]] std::uint64_t fence_fingerprint(const FenceSnapshot& fence) noexcept {
  std::uint64_t value = fence.identity_record_id ^ fence.lifecycle_sequence ^ fence.epoch;
  value ^= fence.effective_quota_bytes + UINT64_C(0x9e3779b97f4a7c15);
  value ^= fence.policy_bits + (static_cast<std::uint64_t>(fence.device_state) << 32U);
  return value;
}

} // namespace

mf_shared_status_v1 RegistryView::begin_admission(std::uint32_t device_index,
                                                  std::uint64_t deadline_ns,
                                                  std::uint64_t operation_cookie,
                                                  std::uint64_t target_cookie,
                                                  AdmissionLeaseToken& out_token) noexcept {
  if (extension_ == nullptr) {
    return MF_SHARED_NOT_SUPPORTED;
  }
  if (device_index >= device_count_ || operation_cookie == 0U || target_cookie == 0U) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  if (mf_view_admission_state_v1(mf_atomic_load_u64_seq_cst(&view_admission_->state_generation)) !=
      MF_VIEW_ADMISSION_OPEN) {
    return MF_SHARED_TERMINAL_VIEW;
  }
  const std::uint32_t initial_device_state = mf_device_admission_state_v1(
      mf_atomic_load_u64_seq_cst(&device_admission_[device_index].state_generation_tag));
  if (initial_device_state != MF_DEVICE_ADMISSION_OPEN) {
    return initial_device_state == MF_DEVICE_ADMISSION_CLOSED ? MF_SHARED_DEVICE_LOST
                                                              : MF_SHARED_RETRY;
  }
  mf_owner_identity_v1 owner{};
  if (current_owner_identity(owner) != MF_SHARED_SUCCESS) {
    return MF_SHARED_SYSTEM_ERROR;
  }

  std::uint32_t attempt_slot = 0;
  std::uint32_t attempt_tag = 0;
  const mf_shared_status_v1 attempt_status = claim_record(
      admission_attempts_, extension_->admission_attempt_capacity,
      &mf_admission_attempt_record_v1::tagged_phase, MF_ADMISSION_ATTEMPT_ENTERING, 0U,
      [](std::uint32_t state) {
        return state == MF_ADMISSION_ATTEMPT_IDLE || state == MF_ADMISSION_ATTEMPT_EXITED;
      },
      [](std::uint32_t tag, std::uint32_t state, std::uint32_t) {
        return mf_admission_attempt_phase_pack_v1(tag, state);
      },
      attempt_slot, attempt_tag);
  if (attempt_status != MF_SHARED_SUCCESS) {
    return attempt_status;
  }
  if (fault_point_ == RecoveryFaultPoint::AttemptClaimed) {
    (void)::raise(SIGSTOP);
  }

  auto& attempt = admission_attempts_[attempt_slot];
  attempt.registry_view_id = view_id_;
  attempt.identity_record_id = identities_[device_index].identity_record_id;
  attempt.owner = owner;
  attempt.deadline_ns = deadline_ns;
  attempt.target_lease_slot = MF_SHARED_RECORD_SLOT_NONE;
  attempt.target_lease_tag = 0U;
  std::uint64_t attempt_expected =
      mf_admission_attempt_phase_pack_v1(attempt_tag, MF_ADMISSION_ATTEMPT_ENTERING);
  if (!mf_atomic_compare_exchange_u64_seq_cst(
          &attempt.tagged_phase, &attempt_expected,
          mf_admission_attempt_phase_pack_v1(attempt_tag, MF_ADMISSION_ATTEMPT_INITIALIZING))) {
    return MF_SHARED_RETRY;
  }
  if (fault_point_ == RecoveryFaultPoint::AttemptInitializing) {
    return MF_SHARED_INTERRUPTED;
  }

  const std::uint64_t view_control = mf_atomic_load_u64_seq_cst(&view_admission_->state_generation);
  const std::uint64_t device_control =
      mf_atomic_load_u64_seq_cst(&device_admission_[device_index].state_generation_tag);
  if (mf_view_admission_state_v1(view_control) != MF_VIEW_ADMISSION_OPEN ||
      mf_device_admission_state_v1(device_control) != MF_DEVICE_ADMISSION_OPEN) {
    attempt_expected =
        mf_admission_attempt_phase_pack_v1(attempt_tag, MF_ADMISSION_ATTEMPT_INITIALIZING);
    (void)mf_atomic_compare_exchange_u64_seq_cst(
        &attempt.tagged_phase, &attempt_expected,
        mf_admission_attempt_phase_pack_v1(attempt_tag, MF_ADMISSION_ATTEMPT_EXITED));
    return mf_view_admission_state_v1(view_control) == MF_VIEW_ADMISSION_OPEN
               ? MF_SHARED_RETRY
               : MF_SHARED_TERMINAL_VIEW;
  }
  attempt.view_validation_generation = mf_view_admission_generation_v1(view_control);
  attempt.device_validation_generation = mf_device_admission_generation_v1(device_control);
  attempt_expected =
      mf_admission_attempt_phase_pack_v1(attempt_tag, MF_ADMISSION_ATTEMPT_INITIALIZING);
  if (!mf_atomic_compare_exchange_u64_seq_cst(
          &attempt.tagged_phase, &attempt_expected,
          mf_admission_attempt_phase_pack_v1(attempt_tag, MF_ADMISSION_ATTEMPT_CLAIMING))) {
    return MF_SHARED_RETRY;
  }

  std::uint32_t lease_slot = 0;
  std::uint32_t lease_tag = 0;
  const mf_shared_status_v1 lease_status = claim_record(
      admission_leases_, extension_->admission_lease_capacity,
      &mf_admission_lease_record_v1::tagged_state, MF_ADMISSION_LEASE_INITIALIZING, attempt_tag,
      [](std::uint32_t state) {
        return state == MF_ADMISSION_LEASE_FREE || state == MF_ADMISSION_LEASE_RELEASED ||
               state == MF_ADMISSION_LEASE_REVOKED || state == MF_ADMISSION_LEASE_TOMBSTONED;
      },
      [](std::uint32_t tag, std::uint32_t state, std::uint32_t auxiliary) {
        return mf_admission_lease_state_pack_v1(tag, state, auxiliary);
      },
      lease_slot, lease_tag);
  if (lease_status != MF_SHARED_SUCCESS) {
    attempt_expected =
        mf_admission_attempt_phase_pack_v1(attempt_tag, MF_ADMISSION_ATTEMPT_CLAIMING);
    (void)mf_atomic_compare_exchange_u64_seq_cst(
        &attempt.tagged_phase, &attempt_expected,
        mf_admission_attempt_phase_pack_v1(attempt_tag, MF_ADMISSION_ATTEMPT_EXITED));
    return lease_status;
  }
  if (fault_point_ == RecoveryFaultPoint::LeaseClaimed) {
    (void)::raise(SIGSTOP);
  }

  auto& lease = admission_leases_[lease_slot];
  lease.registry_view_id = view_id_;
  lease.identity_record_id = identities_[device_index].identity_record_id;
  lease.lease_id = (static_cast<std::uint64_t>(lease_tag) << 32U) | lease_slot;
  lease.view_validation_generation = mf_view_admission_generation_v1(view_control);
  lease.owner = owner;
  lease.deadline_ns = deadline_ns;
  lease.operation_cookie = operation_cookie;
  lease.target_cookie = target_cookie;
  lease.publication_marker = 0U;
  lease.device_validation_generation = mf_device_admission_generation_v1(device_control);
  lease.recovery_plan = MF_RECOVERY_PLAN_TOMBSTONE;
  lease.attempt_slot = attempt_slot;
  lease.attempt_tag = attempt_tag;
  attempt.target_lease_slot = lease_slot;
  attempt.target_lease_tag = lease_tag;
  mf_atomic_thread_fence_release();
  std::uint64_t lease_expected =
      mf_admission_lease_state_pack_v1(lease_tag, MF_ADMISSION_LEASE_INITIALIZING, attempt_tag);
  if (!mf_atomic_compare_exchange_u64_seq_cst(
          &lease.tagged_state, &lease_expected,
          mf_admission_lease_state_pack_v1(lease_tag, MF_ADMISSION_LEASE_RESERVED, attempt_tag))) {
    attempt_expected =
        mf_admission_attempt_phase_pack_v1(attempt_tag, MF_ADMISSION_ATTEMPT_CLAIMING);
    (void)mf_atomic_compare_exchange_u64_seq_cst(
        &attempt.tagged_phase, &attempt_expected,
        mf_admission_attempt_phase_pack_v1(attempt_tag, MF_ADMISSION_ATTEMPT_EXITED));
    return MF_SHARED_RETRY;
  }

  if (mf_atomic_load_u64_seq_cst(&view_admission_->state_generation) != view_control ||
      mf_atomic_load_u64_seq_cst(&device_admission_[device_index].state_generation_tag) !=
          device_control) {
    std::uint64_t expected =
        mf_admission_lease_state_pack_v1(lease_tag, MF_ADMISSION_LEASE_RESERVED, attempt_tag);
    (void)mf_atomic_compare_exchange_u64_seq_cst(
        &lease.tagged_state, &expected,
        mf_admission_lease_state_pack_v1(lease_tag, MF_ADMISSION_LEASE_REVOKED, attempt_tag));
    attempt_expected =
        mf_admission_attempt_phase_pack_v1(attempt_tag, MF_ADMISSION_ATTEMPT_CLAIMING);
    (void)mf_atomic_compare_exchange_u64_seq_cst(
        &attempt.tagged_phase, &attempt_expected,
        mf_admission_attempt_phase_pack_v1(attempt_tag, MF_ADMISSION_ATTEMPT_EXITED));
    return MF_SHARED_RETRY;
  }

  attempt_expected = mf_admission_attempt_phase_pack_v1(attempt_tag, MF_ADMISSION_ATTEMPT_CLAIMING);
  if (!mf_atomic_compare_exchange_u64_seq_cst(
          &attempt.tagged_phase, &attempt_expected,
          mf_admission_attempt_phase_pack_v1(attempt_tag, MF_ADMISSION_ATTEMPT_READY))) {
    lease_expected =
        mf_admission_lease_state_pack_v1(lease_tag, MF_ADMISSION_LEASE_RESERVED, attempt_tag);
    (void)mf_atomic_compare_exchange_u64_seq_cst(
        &lease.tagged_state, &lease_expected,
        mf_admission_lease_state_pack_v1(lease_tag, MF_ADMISSION_LEASE_REVOKED, attempt_tag));
    return MF_SHARED_RETRY;
  }
  out_token = {
      .registry_view_id = view_id_,
      .identity_record_id = lease.identity_record_id,
      .view_validation_generation = lease.view_validation_generation,
      .device_validation_generation = lease.device_validation_generation,
      .attempt_slot = attempt_slot,
      .attempt_tag = attempt_tag,
      .lease_slot = lease_slot,
      .lease_tag = lease_tag,
  };
  return MF_SHARED_SUCCESS;
}

mf_shared_status_v1 RegistryView::commit_admission(const AdmissionLeaseToken& token) noexcept {
  if (extension_ == nullptr) {
    return MF_SHARED_NOT_SUPPORTED;
  }
  if (!same_token_view(token.registry_view_id, view_id_) ||
      token.attempt_slot >= extension_->admission_attempt_capacity ||
      token.lease_slot >= extension_->admission_lease_capacity) {
    return MF_SHARED_STALE_HANDLE;
  }
  auto& attempt = admission_attempts_[token.attempt_slot];
  auto& lease = admission_leases_[token.lease_slot];
  if (mf_atomic_load_u64_seq_cst(&attempt.tagged_phase) !=
          mf_admission_attempt_phase_pack_v1(token.attempt_tag, MF_ADMISSION_ATTEMPT_READY) ||
      lease.identity_record_id != token.identity_record_id ||
      lease.attempt_slot != token.attempt_slot || lease.attempt_tag != token.attempt_tag) {
    return MF_SHARED_STALE_HANDLE;
  }
  const std::uint32_t device_index = find_identity(token.identity_record_id);
  if (device_index == device_count_) {
    return MF_SHARED_STALE_HANDLE;
  }
  const std::uint64_t expected_view =
      mf_view_admission_pack_v1(token.view_validation_generation, MF_VIEW_ADMISSION_OPEN);
  const std::uint64_t expected_device =
      mf_device_admission_pack_v1(token.device_validation_generation, MF_DEVICE_ADMISSION_OPEN, 0U);
  if (mf_atomic_load_u64_seq_cst(&view_admission_->state_generation) != expected_view ||
      mf_atomic_load_u64_seq_cst(&device_admission_[device_index].state_generation_tag) !=
          expected_device) {
    return MF_SHARED_RETRY;
  }

  std::uint64_t expected = mf_admission_lease_state_pack_v1(
      token.lease_tag, MF_ADMISSION_LEASE_RESERVED, token.attempt_tag);
  if (!mf_atomic_compare_exchange_u64_seq_cst(
          &lease.tagged_state, &expected,
          mf_admission_lease_state_pack_v1(token.lease_tag, MF_ADMISSION_LEASE_COMMITTING,
                                           token.attempt_tag))) {
    return mf_tagged_record_tag_v1(expected) == token.lease_tag &&
                   (mf_tagged_record_state_v1(expected) == MF_ADMISSION_LEASE_COMMITTED ||
                    mf_tagged_record_state_v1(expected) == MF_ADMISSION_LEASE_PUBLISHED)
               ? MF_SHARED_SUCCESS
               : MF_SHARED_STALE_HANDLE;
  }
  if (fault_point_ == RecoveryFaultPoint::LeaseCommitting) {
    return MF_SHARED_INTERRUPTED;
  }
  if (mf_atomic_load_u64_seq_cst(&view_admission_->state_generation) != expected_view ||
      mf_atomic_load_u64_seq_cst(&device_admission_[device_index].state_generation_tag) !=
          expected_device) {
    expected = mf_admission_lease_state_pack_v1(token.lease_tag, MF_ADMISSION_LEASE_COMMITTING,
                                                token.attempt_tag);
    (void)mf_atomic_compare_exchange_u64_seq_cst(
        &lease.tagged_state, &expected,
        mf_admission_lease_state_pack_v1(token.lease_tag, MF_ADMISSION_LEASE_TOMBSTONED,
                                         token.attempt_tag));
    return MF_SHARED_RETRY;
  }
  expected = mf_admission_lease_state_pack_v1(token.lease_tag, MF_ADMISSION_LEASE_COMMITTING,
                                              token.attempt_tag);
  if (!mf_atomic_compare_exchange_u64_seq_cst(
          &lease.tagged_state, &expected,
          mf_admission_lease_state_pack_v1(token.lease_tag, MF_ADMISSION_LEASE_COMMITTED,
                                           token.attempt_tag))) {
    return MF_SHARED_STALE_HANDLE;
  }
  mf_atomic_store_u64_seq_cst(&lease.publication_marker, 1U);
  expected = mf_admission_lease_state_pack_v1(token.lease_tag, MF_ADMISSION_LEASE_COMMITTED,
                                              token.attempt_tag);
  (void)mf_atomic_compare_exchange_u64_seq_cst(
      &lease.tagged_state, &expected,
      mf_admission_lease_state_pack_v1(token.lease_tag, MF_ADMISSION_LEASE_PUBLISHED,
                                       token.attempt_tag));
  return MF_SHARED_SUCCESS;
}

mf_shared_status_v1 RegistryView::release_admission(const AdmissionLeaseToken& token) noexcept {
  if (extension_ == nullptr) {
    return MF_SHARED_NOT_SUPPORTED;
  }
  if (!same_token_view(token.registry_view_id, view_id_) ||
      token.attempt_slot >= extension_->admission_attempt_capacity ||
      token.lease_slot >= extension_->admission_lease_capacity) {
    return MF_SHARED_STALE_HANDLE;
  }
  auto& lease = admission_leases_[token.lease_slot];
  std::uint64_t observed = mf_atomic_load_u64_seq_cst(&lease.tagged_state);
  if (mf_tagged_record_tag_v1(observed) != token.lease_tag) {
    return MF_SHARED_STALE_HANDLE;
  }
  for (;;) {
    const std::uint32_t state = mf_tagged_record_state_v1(observed);
    if (state == MF_ADMISSION_LEASE_RELEASED || state == MF_ADMISSION_LEASE_REVOKED ||
        state == MF_ADMISSION_LEASE_TOMBSTONED) {
      break;
    }
    const std::uint32_t target = state == MF_ADMISSION_LEASE_RESERVED ? MF_ADMISSION_LEASE_REVOKED
                                                                      : MF_ADMISSION_LEASE_RELEASED;
    const std::uint64_t desired =
        mf_admission_lease_state_pack_v1(token.lease_tag, target, token.attempt_tag);
    if (mf_atomic_compare_exchange_u64_seq_cst(&lease.tagged_state, &observed, desired)) {
      break;
    }
    if (mf_tagged_record_tag_v1(observed) != token.lease_tag) {
      return MF_SHARED_STALE_HANDLE;
    }
  }
  auto& attempt = admission_attempts_[token.attempt_slot];
  std::uint64_t attempt_word = mf_atomic_load_u64_seq_cst(&attempt.tagged_phase);
  while (mf_tagged_record_tag_v1(attempt_word) == token.attempt_tag &&
         mf_tagged_record_state_v1(attempt_word) != MF_ADMISSION_ATTEMPT_EXITED) {
    if (mf_atomic_compare_exchange_u64_seq_cst(
            &attempt.tagged_phase, &attempt_word,
            mf_admission_attempt_phase_pack_v1(token.attempt_tag, MF_ADMISSION_ATTEMPT_EXITED))) {
      break;
    }
  }
  return MF_SHARED_SUCCESS;
}

mf_shared_status_v1 RegistryView::help_admission(const AdmissionLeaseToken& token,
                                                 bool owner_may_still_run) noexcept {
  if (extension_ == nullptr || token.lease_slot >= extension_->admission_lease_capacity) {
    return MF_SHARED_NOT_SUPPORTED;
  }
  auto& lease = admission_leases_[token.lease_slot];
  const std::uint64_t state = mf_atomic_load_u64_seq_cst(&lease.tagged_state);
  if (mf_tagged_record_tag_v1(state) != token.lease_tag ||
      !same_token_view(token.registry_view_id, view_id_)) {
    return MF_SHARED_STALE_HANDLE;
  }
  mf_owner_identity_v1 owner{};
  {
    RecordHazard hazard(lease, &mf_admission_lease_record_v1::tagged_state, state);
    if (!hazard.held()) {
      return MF_SHARED_RETRY;
    }
    owner = lease.owner;
  }
  return recover_owner(owner, owner_may_still_run);
}

mf_shared_status_v1 RegistryView::recover_owner(mf_owner_identity_v1 owner,
                                                bool owner_may_still_run) noexcept {
  if (extension_ == nullptr) {
    return MF_SHARED_NOT_SUPPORTED;
  }
  if (owner.pid == 0U || owner.start_time_ticks == 0U || owner.reserved != 0U) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  if (owner_may_still_run) {
    return quarantine();
  }

  // Claim states expose only the new tag; their owner and payload still belong to the old tag.
  for (std::uint32_t slot = 0; slot < extension_->admission_lease_capacity; ++slot) {
    auto& lease = admission_leases_[slot];
    std::uint64_t observed = mf_atomic_load_u64_seq_cst(&lease.tagged_state);
    RecordHazard hazard(lease, &mf_admission_lease_record_v1::tagged_state, observed);
    if (!hazard.held()) {
      continue;
    }
    for (;;) {
      const std::uint32_t tag = mf_tagged_record_tag_v1(observed);
      const std::uint32_t state = mf_tagged_record_state_v1(observed);
      const std::uint32_t attempt_tag = mf_tagged_record_auxiliary_v1(observed);
      std::uint32_t target = state;
      if (state == MF_ADMISSION_LEASE_INITIALIZING) {
        break;
      }
      if (!same_owner(lease.owner, owner)) {
        break;
      }
      if (state == MF_ADMISSION_LEASE_RESERVED || state == MF_ADMISSION_LEASE_COMMITTING) {
        target = MF_ADMISSION_LEASE_TOMBSTONED;
      } else if (state == MF_ADMISSION_LEASE_COMMITTED || state == MF_ADMISSION_LEASE_PUBLISHED) {
        target = MF_ADMISSION_LEASE_RELEASED;
      } else {
        break;
      }
      if (mf_atomic_compare_exchange_u64_seq_cst(
              &lease.tagged_state, &observed,
              mf_admission_lease_state_pack_v1(tag, target, attempt_tag))) {
        break;
      }
    }
  }
  for (std::uint32_t slot = 0; slot < extension_->admission_attempt_capacity; ++slot) {
    auto& attempt = admission_attempts_[slot];
    std::uint64_t observed = mf_atomic_load_u64_seq_cst(&attempt.tagged_phase);
    RecordHazard hazard(attempt, &mf_admission_attempt_record_v1::tagged_phase, observed);
    if (!hazard.held()) {
      continue;
    }
    const std::uint32_t tag = mf_tagged_record_tag_v1(observed);
    const std::uint32_t state = mf_tagged_record_state_v1(observed);
    if (state != MF_ADMISSION_ATTEMPT_ENTERING && state != MF_ADMISSION_ATTEMPT_IDLE &&
        state != MF_ADMISSION_ATTEMPT_EXITED && same_owner(attempt.owner, owner)) {
      (void)mf_atomic_compare_exchange_u64_seq_cst(
          &attempt.tagged_phase, &observed,
          mf_admission_attempt_phase_pack_v1(tag, MF_ADMISSION_ATTEMPT_EXITED));
    }
  }

  mf_shared_status_v1 status = recover_device_updates(owner);
  if (status != MF_SHARED_SUCCESS) {
    return status;
  }
  status = recover_telemetry_publish(owner);
  if (status != MF_SHARED_SUCCESS) {
    return status;
  }

  for (std::uint32_t slot = 0; slot < extension_->ordinary_view_publish_capacity; ++slot) {
    auto& publication = view_publish_records_[slot];
    const std::uint64_t sampled = mf_atomic_load_u64_seq_cst(&publication.tagged_state);
    const std::uint32_t state = mf_tagged_record_state_v1(sampled);
    if (state == MF_VIEW_PUBLISH_FREE || state == MF_VIEW_PUBLISH_INITIALIZING ||
        state == MF_VIEW_PUBLISH_TERMINAL) {
      continue;
    }
    RecordHazard hazard(publication, &mf_view_publish_record_v1::tagged_state, sampled);
    if (!hazard.held() || !same_owner(publication.owner, owner)) {
      continue;
    }
    const std::uint32_t tag = mf_tagged_record_tag_v1(sampled);
    std::uint32_t settled_state = state;
    if (state != MF_VIEW_PUBLISH_PUBLISHED && state != MF_VIEW_PUBLISH_ABORTED) {
      const std::uint64_t publisher = mf_atomic_load_u64_seq_cst(&view_publisher_->tagged_owner);
      if (publisher == mf_publisher_control_pack_v1(tag, MF_PUBLISHER_WRITING, slot) &&
          (mf_atomic_load_u64_seq_cst(&view_control_->control_latch_sequence) & 1U) != 0U) {
        return quarantine();
      }
      settled_state = MF_VIEW_PUBLISH_ABORTED;
    }
    status = finish_view_publication(publication, *view_publisher_, tag, slot,
                                     publication.range_slot, settled_state);
    if (status == MF_SHARED_STALE_HANDLE) {
      return quarantine();
    }
    if (status != MF_SHARED_SUCCESS) {
      return status;
    }
  }

  for (std::uint32_t slot = 0; slot < extension_->lifecycle_range_capacity; ++slot) {
    auto& range = lifecycle_ranges_[slot];
    const std::uint64_t observed = mf_atomic_load_u64_seq_cst(&range.tagged_state);
    RecordHazard hazard(range, &mf_lifecycle_range_record_v1::tagged_state, observed);
    if (!hazard.held()) {
      continue;
    }
    const std::uint32_t state = mf_tagged_record_state_v1(observed);
    if (state == MF_LIFECYCLE_RANGE_INITIALIZING) {
      continue;
    }
    if (!same_owner(range.owner, owner)) {
      continue;
    }
    if (state == MF_LIFECYCLE_RANGE_OPEN) {
      const LifecycleRangeToken token{
          .registry_view_id = view_id_,
          .range_begin = range.range_begin,
          .range_end = range.range_end,
          .token_generation = range.token_generation,
          .range_slot = slot,
          .range_tag = mf_tagged_record_tag_v1(observed),
      };
      status = retire_lifecycle_range(token, token.range_begin - 1U);
      if (status != MF_SHARED_SUCCESS && status != MF_SHARED_STALE_HANDLE) {
        return status;
      }
    } else if (state == MF_LIFECYCLE_RANGE_PREPARED) {
      std::uint64_t expected = observed;
      (void)mf_atomic_compare_exchange_u64_seq_cst(
          &range.tagged_state, &expected,
          mf_lifecycle_range_state_pack_v1(mf_tagged_record_tag_v1(observed),
                                           MF_LIFECYCLE_RANGE_RETIRED,
                                           MF_LIFECYCLE_RANGE_DISPOSITION_WHOLE));
    }
  }
  return MF_SHARED_SUCCESS;
}

mf_shared_status_v1 RegistryView::recover() noexcept {
  if (extension_ == nullptr) {
    return MF_SHARED_NOT_SUPPORTED;
  }
  for (std::uint32_t slot = 0; slot < extension_->admission_attempt_capacity; ++slot) {
    auto& record = admission_attempts_[slot];
    const std::uint64_t sampled = mf_atomic_load_u64_seq_cst(&record.tagged_phase);
    const std::uint32_t state = mf_tagged_record_state_v1(sampled);
    if (state == MF_ADMISSION_ATTEMPT_IDLE || state == MF_ADMISSION_ATTEMPT_ENTERING ||
        state == MF_ADMISSION_ATTEMPT_EXITED) {
      continue;
    }
    RecordHazard hazard(record, &mf_admission_attempt_record_v1::tagged_phase, sampled);
    if (!hazard.held() || record.owner.pid == 0U) {
      continue;
    }
    const bool alive = process_is_alive(record.owner);
    if (!alive || deadline_expired(record.deadline_ns)) {
      const mf_shared_status_v1 status = recover_owner(record.owner, alive);
      if (status != MF_SHARED_SUCCESS) {
        return status;
      }
    }
  }
  for (std::uint32_t slot = 0; slot < extension_->device_update_capacity; ++slot) {
    auto& record = device_updates_[slot];
    const std::uint64_t sampled = mf_atomic_load_u64_seq_cst(&record.tagged_state);
    const std::uint32_t state = mf_tagged_record_state_v1(sampled);
    if (state == MF_DEVICE_UPDATE_FREE || state == MF_DEVICE_UPDATE_INITIALIZING ||
        state == MF_DEVICE_UPDATE_TERMINAL) {
      continue;
    }
    RecordHazard hazard(record, &mf_device_validation_update_record_v1::tagged_state, sampled);
    if (!hazard.held() || record.owner.pid == 0U) {
      continue;
    }
    const bool alive = process_is_alive(record.owner);
    if (!alive || deadline_expired(record.deadline_ns)) {
      const mf_shared_status_v1 status = recover_owner(record.owner, alive);
      if (status != MF_SHARED_SUCCESS) {
        return status;
      }
    }
  }
  for (std::uint32_t slot = 0; slot < extension_->ordinary_telemetry_publish_capacity; ++slot) {
    auto& record = telemetry_publish_records_[slot];
    const std::uint64_t sampled = mf_atomic_load_u64_seq_cst(&record.tagged_state);
    const std::uint32_t state = mf_tagged_record_state_v1(sampled);
    if (state == MF_TELEMETRY_PUBLISH_FREE || state == MF_TELEMETRY_PUBLISH_INITIALIZING ||
        state == MF_TELEMETRY_PUBLISH_TERMINAL) {
      continue;
    }
    RecordHazard hazard(record, &mf_telemetry_publish_record_v1::tagged_state, sampled);
    if (!hazard.held() || record.owner.pid == 0U) {
      continue;
    }
    const bool alive = process_is_alive(record.owner);
    if (!alive || deadline_expired(record.deadline_ns)) {
      const mf_shared_status_v1 status = recover_owner(record.owner, alive);
      if (status != MF_SHARED_SUCCESS) {
        return status;
      }
    }
  }
  for (std::uint32_t slot = 0; slot < extension_->ordinary_view_publish_capacity; ++slot) {
    auto& record = view_publish_records_[slot];
    const std::uint64_t sampled = mf_atomic_load_u64_seq_cst(&record.tagged_state);
    const std::uint32_t state = mf_tagged_record_state_v1(sampled);
    if (state == MF_VIEW_PUBLISH_FREE || state == MF_VIEW_PUBLISH_INITIALIZING ||
        state == MF_VIEW_PUBLISH_TERMINAL) {
      continue;
    }
    RecordHazard hazard(record, &mf_view_publish_record_v1::tagged_state, sampled);
    if (!hazard.held() || record.owner.pid == 0U) {
      continue;
    }
    const bool alive = process_is_alive(record.owner);
    if (!alive || deadline_expired(record.deadline_ns)) {
      const mf_shared_status_v1 status = recover_owner(record.owner, alive);
      if (status != MF_SHARED_SUCCESS) {
        return status;
      }
    }
  }
  for (std::uint32_t slot = 0; slot < extension_->lifecycle_range_capacity; ++slot) {
    auto& record = lifecycle_ranges_[slot];
    const std::uint64_t sampled = mf_atomic_load_u64_seq_cst(&record.tagged_state);
    const std::uint32_t state = mf_tagged_record_state_v1(sampled);
    if (state == MF_LIFECYCLE_RANGE_FREE || state == MF_LIFECYCLE_RANGE_INITIALIZING ||
        state == MF_LIFECYCLE_RANGE_RETIRED) {
      continue;
    }
    RecordHazard hazard(record, &mf_lifecycle_range_record_v1::tagged_state, sampled);
    if (!hazard.held() || record.owner.pid == 0U) {
      continue;
    }
    const bool alive = process_is_alive(record.owner);
    if (!alive || deadline_expired(record.deadline_ns)) {
      const mf_shared_status_v1 status = recover_owner(record.owner, alive);
      if (status != MF_SHARED_SUCCESS) {
        return status;
      }
    }
  }
  return MF_SHARED_SUCCESS;
}

mf_shared_status_v1 RegistryView::reserve_lifecycle_range(std::uint64_t length,
                                                          std::uint64_t deadline_ns,
                                                          LifecycleRangeToken& out_token) noexcept {
  if (extension_ == nullptr) {
    return MF_SHARED_NOT_SUPPORTED;
  }
  if (length == 0U) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  mf_owner_identity_v1 owner{};
  if (current_owner_identity(owner) != MF_SHARED_SUCCESS) {
    return MF_SHARED_SYSTEM_ERROR;
  }

  std::uint32_t range_slot = 0;
  std::uint32_t range_tag = 0;
  mf_shared_status_v1 status = claim_record(
      lifecycle_ranges_, extension_->lifecycle_range_capacity,
      &mf_lifecycle_range_record_v1::tagged_state, MF_LIFECYCLE_RANGE_INITIALIZING,
      MF_LIFECYCLE_RANGE_DISPOSITION_NONE,
      [](std::uint32_t state) {
        return state == MF_LIFECYCLE_RANGE_FREE || state == MF_LIFECYCLE_RANGE_RETIRED;
      },
      [](std::uint32_t tag, std::uint32_t state, std::uint32_t disposition) {
        return mf_lifecycle_range_state_pack_v1(tag, state, disposition);
      },
      range_slot, range_tag);
  if (status != MF_SHARED_SUCCESS) {
    return status;
  }

  std::uint32_t publish_slot = 0;
  std::uint32_t publish_tag = 0;
  status = claim_record(
      view_publish_records_, extension_->ordinary_view_publish_capacity,
      &mf_view_publish_record_v1::tagged_state, MF_VIEW_PUBLISH_INITIALIZING, range_slot,
      [](std::uint32_t state) {
        return state == MF_VIEW_PUBLISH_FREE || state == MF_VIEW_PUBLISH_TERMINAL;
      },
      [](std::uint32_t tag, std::uint32_t state, std::uint32_t range) {
        return mf_view_publish_state_pack_v1(tag, state, range);
      },
      publish_slot, publish_tag);
  if (status != MF_SHARED_SUCCESS) {
    std::uint64_t range_expected = mf_lifecycle_range_state_pack_v1(
        range_tag, MF_LIFECYCLE_RANGE_INITIALIZING, MF_LIFECYCLE_RANGE_DISPOSITION_NONE);
    (void)mf_atomic_compare_exchange_u64_seq_cst(
        &lifecycle_ranges_[range_slot].tagged_state, &range_expected,
        mf_lifecycle_range_state_pack_v1(range_tag, MF_LIFECYCLE_RANGE_RETIRED,
                                         MF_LIFECYCLE_RANGE_DISPOSITION_WHOLE));
    return status;
  }

  auto& range = lifecycle_ranges_[range_slot];
  auto& publication = view_publish_records_[publish_slot];
  const auto finish_publication = [&](std::uint32_t settled_state) noexcept {
    return finish_view_publication(publication, *view_publisher_, publish_tag, publish_slot,
                                   range_slot, settled_state);
  };
  const auto retire_prepared_range = [&]() noexcept {
    std::uint64_t expected = mf_lifecycle_range_state_pack_v1(
        range_tag, MF_LIFECYCLE_RANGE_PREPARED, MF_LIFECYCLE_RANGE_DISPOSITION_NONE);
    return mf_atomic_compare_exchange_u64_seq_cst(
               &range.tagged_state, &expected,
               mf_lifecycle_range_state_pack_v1(range_tag, MF_LIFECYCLE_RANGE_RETIRED,
                                                MF_LIFECYCLE_RANGE_DISPOSITION_WHOLE)) != 0 ||
           (mf_tagged_record_tag_v1(expected) == range_tag &&
            mf_tagged_record_state_v1(expected) == MF_LIFECYCLE_RANGE_RETIRED);
  };
  range.registry_view_id = view_id_;
  range.owner = owner;
  range.deadline_ns = deadline_ns;
  range.transaction_id = (static_cast<std::uint64_t>(range_tag) << 32U) | range_slot;
  range.next_tail_slot = MF_SHARED_RECORD_SLOT_NONE;
  range.retirement_disposition = MF_LIFECYCLE_RANGE_DISPOSITION_NONE;
  publication.registry_view_id = view_id_;
  publication.owner = owner;
  publication.deadline_ns = deadline_ns;
  publication.transaction_id = range.transaction_id;
  publication.range_slot = range_slot;
  publication.operation_kind = MF_VIEW_PUBLISH_KIND_RANGE_RESERVE;
  publication.recovery_plan = MF_RECOVERY_PLAN_CANCEL;
  std::uint64_t range_expected = mf_lifecycle_range_state_pack_v1(
      range_tag, MF_LIFECYCLE_RANGE_INITIALIZING, MF_LIFECYCLE_RANGE_DISPOSITION_NONE);
  if (!mf_atomic_compare_exchange_u64_seq_cst(
          &range.tagged_state, &range_expected,
          mf_lifecycle_range_state_pack_v1(range_tag, MF_LIFECYCLE_RANGE_PREPARED,
                                           MF_LIFECYCLE_RANGE_DISPOSITION_NONE))) {
    std::uint64_t publication_expected =
        mf_view_publish_state_pack_v1(publish_tag, MF_VIEW_PUBLISH_INITIALIZING, range_slot);
    (void)mf_atomic_compare_exchange_u64_seq_cst(
        &publication.tagged_state, &publication_expected,
        mf_view_publish_state_pack_v1(publish_tag, MF_VIEW_PUBLISH_ABORTED, range_slot));
    (void)finish_publication(MF_VIEW_PUBLISH_ABORTED);
    return MF_SHARED_RETRY;
  }
  std::uint64_t publication_expected =
      mf_view_publish_state_pack_v1(publish_tag, MF_VIEW_PUBLISH_INITIALIZING, range_slot);
  if (!mf_atomic_compare_exchange_u64_seq_cst(
          &publication.tagged_state, &publication_expected,
          mf_view_publish_state_pack_v1(publish_tag, MF_VIEW_PUBLISH_PREPARED, range_slot))) {
    range_expected = mf_lifecycle_range_state_pack_v1(range_tag, MF_LIFECYCLE_RANGE_PREPARED,
                                                      MF_LIFECYCLE_RANGE_DISPOSITION_NONE);
    (void)mf_atomic_compare_exchange_u64_seq_cst(
        &range.tagged_state, &range_expected,
        mf_lifecycle_range_state_pack_v1(range_tag, MF_LIFECYCLE_RANGE_RETIRED,
                                         MF_LIFECYCLE_RANGE_DISPOSITION_WHOLE));
    (void)finish_publication(MF_VIEW_PUBLISH_ABORTED);
    return MF_SHARED_RETRY;
  }

  status = acquire_publisher(&view_publisher_->tagged_owner, publish_tag, publish_slot);
  if (status != MF_SHARED_SUCCESS) {
    (void)retire_prepared_range();
    (void)finish_publication(MF_VIEW_PUBLISH_ABORTED);
    return status;
  }

  std::uint64_t even = mf_atomic_load_u64_seq_cst(&view_control_->control_latch_sequence);
  if ((even & 1U) != 0U || even > std::numeric_limits<std::uint64_t>::max() - 2U) {
    (void)retire_prepared_range();
    (void)finish_publication(MF_VIEW_PUBLISH_ABORTED);
    return (even & 1U) != 0U ? MF_SHARED_RETRY : MF_SHARED_OVERFLOW;
  }
  std::uint64_t expected_latch = even;
  if (!mf_atomic_compare_exchange_u64_seq_cst(&view_control_->control_latch_sequence,
                                              &expected_latch, even + 1U)) {
    (void)retire_prepared_range();
    (void)finish_publication(MF_VIEW_PUBLISH_ABORTED);
    return MF_SHARED_RETRY;
  }
  const std::uint64_t view_admission =
      mf_atomic_load_u64_seq_cst(&view_admission_->state_generation);
  const std::uint64_t predecessor =
      mf_atomic_load_u64_relaxed(&view_control_->reservation_tail_slot);
  const std::uint64_t high_water =
      mf_atomic_load_u64_relaxed(&view_control_->allocation_high_water);
  std::uint64_t range_end = 0;
  if (mf_view_admission_state_v1(view_admission) != MF_VIEW_ADMISSION_OPEN ||
      !mf_shared_checked_add_u64_below_terminal_v1(
          high_water, length, std::numeric_limits<std::uint64_t>::max(), &range_end)) {
    mf_atomic_store_u64_release(&view_control_->control_latch_sequence, even + 2U);
    (void)retire_prepared_range();
    (void)finish_publication(MF_VIEW_PUBLISH_ABORTED);
    return mf_view_admission_state_v1(view_admission) == MF_VIEW_ADMISSION_OPEN
               ? MF_SHARED_OVERFLOW
               : MF_SHARED_TERMINAL_VIEW;
  }

  range.range_begin = high_water + 1U;
  range.range_end = range_end;
  range.unused_suffix_begin = 0U;
  range.unused_suffix_end = 0U;
  range.token_generation = mf_atomic_load_u64_relaxed(&view_control_->gate_generation) + 1U;
  range.predecessor_high_water = high_water;
  range.predecessor_tail_slot = predecessor;
  publication.range_begin = range.range_begin;
  publication.range_end = range.range_end;
  publication.expected_cursor = mf_atomic_load_u64_relaxed(&view_control_->publication_cursor);
  publication.target_cursor = publication.expected_cursor;
  publication.gate_generation = range.token_generation;
  publication.token_generation = range.token_generation;
  if (predecessor != MF_SHARED_RECORD_SLOT_NONE &&
      predecessor < extension_->lifecycle_range_capacity) {
    lifecycle_ranges_[predecessor].next_tail_slot = range_slot;
  }
  mf_atomic_store_u64_relaxed(&view_control_->allocation_high_water, range_end);
  if (predecessor == MF_SHARED_RECORD_SLOT_NONE) {
    mf_atomic_store_u64_relaxed(&view_control_->reservation_head_slot, range_slot);
    mf_atomic_store_u64_relaxed(&view_control_->next_committable_slot, range_slot);
  }
  mf_atomic_store_u64_relaxed(&view_control_->reservation_tail_slot, range_slot);
  mf_atomic_store_u64_relaxed(&view_control_->gate_generation, range.token_generation);
  mf_atomic_store_u64_release(
      &range.tagged_state, mf_lifecycle_range_state_pack_v1(range_tag, MF_LIFECYCLE_RANGE_OPEN,
                                                            MF_LIFECYCLE_RANGE_DISPOSITION_NONE));
  mf_atomic_store_u64_release(&view_control_->control_latch_sequence, even + 2U);
  status = finish_publication(MF_VIEW_PUBLISH_PUBLISHED);
  if (status != MF_SHARED_SUCCESS) {
    return status;
  }

  out_token = {
      .registry_view_id = view_id_,
      .range_begin = range.range_begin,
      .range_end = range.range_end,
      .token_generation = range.token_generation,
      .range_slot = range_slot,
      .range_tag = range_tag,
  };
  return MF_SHARED_SUCCESS;
}

mf_shared_status_v1 RegistryView::retire_lifecycle_range(const LifecycleRangeToken& token,
                                                         std::uint64_t used_through) noexcept {
  if (extension_ == nullptr) {
    return MF_SHARED_NOT_SUPPORTED;
  }
  if (!same_token_view(token.registry_view_id, view_id_) ||
      token.range_slot >= extension_->lifecycle_range_capacity ||
      used_through < token.range_begin - 1U || used_through > token.range_end) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  auto& range = lifecycle_ranges_[token.range_slot];
  if (mf_atomic_load_u64_seq_cst(&range.tagged_state) !=
          mf_lifecycle_range_state_pack_v1(token.range_tag, MF_LIFECYCLE_RANGE_OPEN,
                                           MF_LIFECYCLE_RANGE_DISPOSITION_NONE) ||
      range.range_begin != token.range_begin || range.range_end != token.range_end ||
      range.token_generation != token.token_generation) {
    return MF_SHARED_STALE_HANDLE;
  }

  std::uint32_t publish_slot = 0;
  std::uint32_t publish_tag = 0;
  mf_shared_status_v1 status = claim_record(
      view_publish_records_, extension_->ordinary_view_publish_capacity,
      &mf_view_publish_record_v1::tagged_state, MF_VIEW_PUBLISH_INITIALIZING, token.range_slot,
      [](std::uint32_t state) {
        return state == MF_VIEW_PUBLISH_FREE || state == MF_VIEW_PUBLISH_TERMINAL;
      },
      [](std::uint32_t tag, std::uint32_t state, std::uint32_t range_slot) {
        return mf_view_publish_state_pack_v1(tag, state, range_slot);
      },
      publish_slot, publish_tag);
  if (status != MF_SHARED_SUCCESS) {
    return status;
  }
  auto& publication = view_publish_records_[publish_slot];
  const auto finish_publication = [&](std::uint32_t settled_state) noexcept {
    return finish_view_publication(publication, *view_publisher_, publish_tag, publish_slot,
                                   token.range_slot, settled_state);
  };
  publication.registry_view_id = view_id_;
  publication.owner = range.owner;
  publication.transaction_id = range.transaction_id;
  publication.range_begin = token.range_begin;
  publication.range_end = token.range_end;
  publication.range_slot = token.range_slot;
  publication.operation_kind = MF_VIEW_PUBLISH_KIND_RANGE_RETIRE;
  publication.recovery_plan = MF_RECOVERY_PLAN_COMPLETE;
  std::uint64_t publication_expected =
      mf_view_publish_state_pack_v1(publish_tag, MF_VIEW_PUBLISH_INITIALIZING, token.range_slot);
  if (!mf_atomic_compare_exchange_u64_seq_cst(
          &publication.tagged_state, &publication_expected,
          mf_view_publish_state_pack_v1(publish_tag, MF_VIEW_PUBLISH_PREPARED, token.range_slot))) {
    (void)finish_publication(MF_VIEW_PUBLISH_ABORTED);
    return MF_SHARED_RETRY;
  }
  status = acquire_publisher(&view_publisher_->tagged_owner, publish_tag, publish_slot);
  if (status != MF_SHARED_SUCCESS) {
    (void)finish_publication(MF_VIEW_PUBLISH_ABORTED);
    return status;
  }

  std::uint64_t even = mf_atomic_load_u64_seq_cst(&view_control_->control_latch_sequence);
  std::uint64_t expected_latch = even;
  if ((even & 1U) != 0U || even > std::numeric_limits<std::uint64_t>::max() - 2U ||
      !mf_atomic_compare_exchange_u64_seq_cst(&view_control_->control_latch_sequence,
                                              &expected_latch, even + 1U)) {
    (void)finish_publication(MF_VIEW_PUBLISH_ABORTED);
    return even > std::numeric_limits<std::uint64_t>::max() - 2U ? MF_SHARED_OVERFLOW
                                                                 : MF_SHARED_RETRY;
  }
  if (mf_atomic_load_u64_relaxed(&view_control_->reservation_head_slot) != token.range_slot) {
    mf_atomic_store_u64_release(&view_control_->control_latch_sequence, even + 2U);
    (void)finish_publication(MF_VIEW_PUBLISH_ABORTED);
    return MF_SHARED_WOULD_BLOCK;
  }

  const std::uint32_t disposition =
      used_through < token.range_begin
          ? MF_LIFECYCLE_RANGE_DISPOSITION_WHOLE
          : (used_through < token.range_end ? MF_LIFECYCLE_RANGE_DISPOSITION_SUFFIX
                                            : MF_LIFECYCLE_RANGE_DISPOSITION_COMPLETE);
  range.unused_suffix_begin = used_through < token.range_end ? used_through + 1U : 0U;
  range.unused_suffix_end = used_through < token.range_end ? token.range_end : 0U;
  range.retirement_disposition = disposition;
  const std::uint64_t next = range.next_tail_slot;
  mf_atomic_store_u64_relaxed(&view_control_->publication_cursor, token.range_end);
  mf_atomic_store_u64_relaxed(&view_control_->reservation_head_slot, next);
  mf_atomic_store_u64_relaxed(&view_control_->next_committable_slot, next);
  if (next == MF_SHARED_RECORD_SLOT_NONE) {
    mf_atomic_store_u64_relaxed(&view_control_->reservation_tail_slot, MF_SHARED_RECORD_SLOT_NONE);
  }
  mf_atomic_store_u64_seq_cst(
      &range.tagged_state,
      mf_lifecycle_range_state_pack_v1(token.range_tag, MF_LIFECYCLE_RANGE_RETIRED, disposition));
  mf_atomic_store_u64_release(&view_control_->control_latch_sequence, even + 2U);
  return finish_publication(MF_VIEW_PUBLISH_PUBLISHED);
}

mf_shared_status_v1 RegistryView::quarantine() noexcept {
  if (view_admission_ == nullptr) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  mf_atomic_store_u64_seq_cst(
      &view_admission_->state_generation,
      mf_view_admission_pack_v1(MF_VIEW_GENERATION_TERMINAL, MF_VIEW_ADMISSION_QUARANTINED));
  for (std::uint32_t index = 0; index < device_count_; ++index) {
    mf_atomic_store_u64_seq_cst(
        &device_admission_[index].state_generation_tag,
        mf_device_admission_pack_v1(MF_DEVICE_GENERATION_TERMINAL, MF_DEVICE_ADMISSION_CLOSED, 0U));
  }
  std::uint64_t even = mf_atomic_load_u64_seq_cst(&view_control_->control_latch_sequence);
  if ((even & 1U) == 0U && even <= std::numeric_limits<std::uint64_t>::max() - 2U) {
    std::uint64_t expected = even;
    if (mf_atomic_compare_exchange_u64_seq_cst(&view_control_->control_latch_sequence, &expected,
                                               even + 1U)) {
      mf_atomic_store_u32_relaxed(&view_control_->gate_state, MF_VIEW_GATE_TERMINAL);
      mf_atomic_store_u32_relaxed(&view_control_->mapping_terminal, 1U);
      mf_atomic_store_u64_release(&view_control_->control_latch_sequence, even + 2U);
    }
  }
  return MF_SHARED_SUCCESS;
}

mf_shared_status_v1 RegistryView::recover_device_updates(mf_owner_identity_v1 owner) noexcept {
  if (extension_ == nullptr) {
    return MF_SHARED_NOT_SUPPORTED;
  }
  for (std::uint32_t slot = 0; slot < extension_->device_update_capacity; ++slot) {
    auto& update = device_updates_[slot];
    std::uint64_t observed = mf_atomic_load_u64_seq_cst(&update.tagged_state);
    const std::uint32_t tag = mf_tagged_record_tag_v1(observed);
    std::uint32_t state = mf_tagged_record_state_v1(observed);
    if (state == MF_DEVICE_UPDATE_FREE || state == MF_DEVICE_UPDATE_INITIALIZING ||
        state == MF_DEVICE_UPDATE_TERMINAL) {
      continue;
    }
    RecordHazard update_hazard(update, &mf_device_validation_update_record_v1::tagged_state,
                               observed);
    if (!update_hazard.held()) {
      continue;
    }
    if (!same_owner(update.owner, owner)) {
      continue;
    }
    if (update.view_publish_slot >= extension_->ordinary_view_publish_capacity ||
        update.device_index >= device_count_ ||
        !same_token_view(update.registry_view_id, view_id_)) {
      return quarantine();
    }
    auto& publication = view_publish_records_[update.view_publish_slot];
    const std::uint64_t publish_state = mf_atomic_load_u64_seq_cst(&publication.tagged_state);
    RecordHazard publication_hazard(publication, &mf_view_publish_record_v1::tagged_state,
                                    publish_state);
    if (!publication_hazard.held()) {
      continue;
    }
    if (mf_tagged_record_tag_v1(publish_state) != update.publish_tag ||
        publication.range_slot >= extension_->lifecycle_range_capacity ||
        !same_token_view(publication.registry_view_id, view_id_)) {
      return quarantine();
    }
    auto& range = lifecycle_ranges_[publication.range_slot];
    const std::uint64_t range_state = mf_atomic_load_u64_seq_cst(&range.tagged_state);
    RecordHazard range_hazard(range, &mf_lifecycle_range_record_v1::tagged_state, range_state);
    if (!range_hazard.held()) {
      continue;
    }
    if (!same_token_view(range.registry_view_id, view_id_) ||
        range.transaction_id != publication.transaction_id) {
      return quarantine();
    }
    LifecycleRangeToken range_token{
        .registry_view_id = view_id_,
        .range_begin = range.range_begin,
        .range_end = range.range_end,
        .token_generation = range.token_generation,
        .range_slot = publication.range_slot,
        .range_tag = mf_tagged_record_tag_v1(range_state),
    };
    if (fault_point_ == RecoveryFaultPoint::RecoveryPayloadHazardHeld) {
      (void)::raise(SIGSTOP);
    }
    if (mf_atomic_load_u64_seq_cst(&update.tagged_state) != observed ||
        mf_atomic_load_u64_seq_cst(&publication.tagged_state) != publish_state ||
        mf_atomic_load_u64_seq_cst(&range.tagged_state) != range_state) {
      continue;
    }

    const auto settle_publication = [&](std::uint32_t target_state) -> mf_shared_status_v1 {
      const mf_shared_status_v1 status =
          settle_view_publication(publication, *view_publisher_, update.publish_tag,
                                  update.view_publish_slot, publication.range_slot, target_state);
      return status == MF_SHARED_STALE_HANDLE ? quarantine() : status;
    };

    const auto finish_publication = [&](std::uint32_t settled_state) -> mf_shared_status_v1 {
      const mf_shared_status_v1 status = terminalize_view_publication(
          publication, update.publish_tag, publication.range_slot, settled_state);
      return status == MF_SHARED_STALE_HANDLE ? quarantine() : status;
    };

    const auto retire_range = [&](std::uint64_t used_through,
                                  std::uint32_t expected_disposition) -> mf_shared_status_v1 {
      const std::uint64_t current = mf_atomic_load_u64_seq_cst(&range.tagged_state);
      if (mf_tagged_record_tag_v1(current) != range_token.range_tag) {
        return quarantine();
      }
      if (mf_tagged_record_state_v1(current) == MF_LIFECYCLE_RANGE_RETIRED) {
        return mf_tagged_record_auxiliary_v1(current) == expected_disposition ? MF_SHARED_SUCCESS
                                                                              : quarantine();
      }
      if (mf_tagged_record_state_v1(current) != MF_LIFECYCLE_RANGE_OPEN) {
        return MF_SHARED_RETRY;
      }
      const mf_shared_status_v1 retired = retire_lifecycle_range(range_token, used_through);
      if (retired == MF_SHARED_SUCCESS) {
        return MF_SHARED_SUCCESS;
      }
      const std::uint64_t after = mf_atomic_load_u64_seq_cst(&range.tagged_state);
      return mf_tagged_record_tag_v1(after) == range_token.range_tag &&
                     mf_tagged_record_state_v1(after) == MF_LIFECYCLE_RANGE_RETIRED &&
                     mf_tagged_record_auxiliary_v1(after) == expected_disposition
                 ? MF_SHARED_SUCCESS
                 : retired;
    };

    const auto finish_update = [&](std::uint32_t expected_state) -> mf_shared_status_v1 {
      std::uint64_t current =
          mf_device_update_state_pack_v1(tag, expected_state, update.view_publish_slot);
      if (mf_atomic_compare_exchange_u64_seq_cst(
              &update.tagged_state, &current,
              mf_device_update_state_pack_v1(tag, MF_DEVICE_UPDATE_TERMINAL,
                                             update.view_publish_slot))) {
        return MF_SHARED_SUCCESS;
      }
      return mf_tagged_record_tag_v1(current) == tag &&
                     mf_tagged_record_state_v1(current) == MF_DEVICE_UPDATE_TERMINAL
                 ? MF_SHARED_SUCCESS
                 : MF_SHARED_RETRY;
    };

    if (state == MF_DEVICE_UPDATE_PREPARED) {
      observed = mf_device_update_state_pack_v1(tag, state, update.view_publish_slot);
      if (mf_atomic_compare_exchange_u64_seq_cst(
              &update.tagged_state, &observed,
              mf_device_update_state_pack_v1(tag, MF_DEVICE_UPDATE_ABORTED,
                                             update.view_publish_slot))) {
        state = MF_DEVICE_UPDATE_ABORTED;
      } else if (mf_tagged_record_tag_v1(observed) == tag) {
        state = mf_tagged_record_state_v1(observed);
      } else {
        return quarantine();
      }
    }

    if (state == MF_DEVICE_UPDATE_LINKING) {
      std::uint64_t device_control =
          mf_atomic_load_u64_seq_cst(&device_admission_[update.device_index].state_generation_tag);
      if (device_control == update.expected_device_control) {
        (void)mf_atomic_compare_exchange_u64_seq_cst(
            &device_admission_[update.device_index].state_generation_tag, &device_control,
            update.target_device_control);
      }
      if (device_control != update.expected_device_control &&
          device_control != update.target_device_control) {
        observed =
            mf_device_update_state_pack_v1(tag, MF_DEVICE_UPDATE_LINKING, update.view_publish_slot);
        if (mf_device_admission_state_v1(device_control) == MF_DEVICE_ADMISSION_CLOSED &&
            mf_atomic_compare_exchange_u64_seq_cst(
                &update.tagged_state, &observed,
                mf_device_update_state_pack_v1(tag, MF_DEVICE_UPDATE_CLOSED,
                                               update.view_publish_slot))) {
          state = MF_DEVICE_UPDATE_CLOSED;
        } else {
          return quarantine();
        }
      } else {
        observed =
            mf_device_update_state_pack_v1(tag, MF_DEVICE_UPDATE_LINKING, update.view_publish_slot);
      }
      if (state == MF_DEVICE_UPDATE_LINKING) {
        if (mf_atomic_compare_exchange_u64_seq_cst(
                &update.tagged_state, &observed,
                mf_device_update_state_pack_v1(tag, MF_DEVICE_UPDATE_ACTIVE,
                                               update.view_publish_slot))) {
          state = MF_DEVICE_UPDATE_ACTIVE;
        } else if (mf_tagged_record_tag_v1(observed) == tag) {
          state = mf_tagged_record_state_v1(observed);
        } else {
          return quarantine();
        }
      }
    }
    if (state == MF_DEVICE_UPDATE_ACTIVE) {
      const FenceSnapshot intended{
          .identity_record_id = update.identity_record_id,
          .lifecycle_sequence = update.intended_lifecycle_sequence,
          .epoch = update.intended_epoch,
          .effective_quota_bytes = update.intended_effective_quota_bytes,
          .policy_bits = update.intended_policy_bits,
          .device_state = update.intended_device_state,
      };
      if (fence_fingerprint(intended) != update.intended_fence_hash ||
          (update.expected_fence_latch_sequence & 1U) != 0U ||
          update.expected_fence_latch_sequence > std::numeric_limits<std::uint64_t>::max() - 2U ||
          update.target_fence_latch_sequence != update.expected_fence_latch_sequence + 2U) {
        return quarantine();
      }
      auto& fence = fences_[update.device_index];
      const std::uint64_t latch = mf_atomic_load_u64_acquire(&fence.fence_latch_sequence);
      // Reconcile the one recorded commit without consuming another latch pair.
      if (latch == update.expected_fence_latch_sequence) {
        if (fault_point_ == RecoveryFaultPoint::RecoveryFenceBeforeExactCas) {
          (void)::raise(SIGSTOP);
        }
        const mf_shared_status_v1 write_status =
            write_fence(update.device_index, intended, update.target_device_control,
                        update.expected_fence_latch_sequence);
        if (write_status != MF_SHARED_SUCCESS) {
          return write_status;
        }
      } else if (latch == update.expected_fence_latch_sequence + 1U) {
        if (mf_atomic_load_u64_seq_cst(
                &device_admission_[update.device_index].state_generation_tag) !=
            update.target_device_control) {
          return quarantine();
        }
        mf_atomic_store_u64_relaxed(&fence.identity_record_id, intended.identity_record_id);
        mf_atomic_store_u64_relaxed(&fence.lifecycle_sequence, intended.lifecycle_sequence);
        mf_atomic_store_u64_relaxed(&fence.epoch, intended.epoch);
        mf_atomic_store_u64_relaxed(&fence.effective_quota_bytes, intended.effective_quota_bytes);
        mf_atomic_store_u64_relaxed(&fence.policy_bits, intended.policy_bits);
        mf_atomic_store_u32_relaxed(&fence.device_state, intended.device_state);
        mf_atomic_thread_fence_release();
        mf_atomic_store_u64_release(&fence.fence_latch_sequence,
                                    update.target_fence_latch_sequence);
      } else if (latch == update.target_fence_latch_sequence) {
        FenceSnapshot committed{};
        std::uint64_t committed_latch = 0U;
        if (stable_fence(update.device_index, committed, committed_latch) != MF_SHARED_SUCCESS ||
            committed_latch != update.target_fence_latch_sequence ||
            committed.identity_record_id != intended.identity_record_id ||
            committed.lifecycle_sequence != intended.lifecycle_sequence ||
            committed.epoch != intended.epoch ||
            committed.effective_quota_bytes != intended.effective_quota_bytes ||
            committed.policy_bits != intended.policy_bits ||
            committed.device_state != intended.device_state) {
          return quarantine();
        }
      } else {
        return quarantine();
      }
      mf_atomic_store_u64_seq_cst(&update.publication_marker, 1U);
      observed =
          mf_device_update_state_pack_v1(tag, MF_DEVICE_UPDATE_ACTIVE, update.view_publish_slot);
      if (mf_atomic_compare_exchange_u64_seq_cst(
              &update.tagged_state, &observed,
              mf_device_update_state_pack_v1(tag, MF_DEVICE_UPDATE_FENCE_PUBLISHED,
                                             update.view_publish_slot))) {
        state = MF_DEVICE_UPDATE_FENCE_PUBLISHED;
      } else if (mf_tagged_record_tag_v1(observed) == tag) {
        state = mf_tagged_record_state_v1(observed);
      } else {
        return quarantine();
      }
    }
    if (state == MF_DEVICE_UPDATE_FENCE_PUBLISHED) {
      std::uint64_t device_expected = update.target_device_control;
      const std::uint64_t reopened = mf_device_admission_pack_v1(update.new_validation_generation,
                                                                 MF_DEVICE_ADMISSION_OPEN, 0U);
      if (!mf_atomic_compare_exchange_u64_seq_cst(
              &device_admission_[update.device_index].state_generation_tag, &device_expected,
              reopened) &&
          device_expected != reopened) {
        return quarantine();
      }
      observed = mf_device_update_state_pack_v1(tag, MF_DEVICE_UPDATE_FENCE_PUBLISHED,
                                                update.view_publish_slot);
      if (mf_atomic_compare_exchange_u64_seq_cst(
              &update.tagged_state, &observed,
              mf_device_update_state_pack_v1(tag, MF_DEVICE_UPDATE_REOPENED,
                                             update.view_publish_slot))) {
        state = MF_DEVICE_UPDATE_REOPENED;
      } else if (mf_tagged_record_tag_v1(observed) == tag) {
        state = mf_tagged_record_state_v1(observed);
      } else {
        return quarantine();
      }
    }
    if (state == MF_DEVICE_UPDATE_REOPENED) {
      mf_shared_status_v1 status = settle_publication(MF_VIEW_PUBLISH_PUBLISHED);
      if (status != MF_SHARED_SUCCESS) {
        return status;
      }
      status = retire_range(range_token.range_end, MF_LIFECYCLE_RANGE_DISPOSITION_COMPLETE);
      if (status != MF_SHARED_SUCCESS) {
        return status;
      }
      status = finish_update(MF_DEVICE_UPDATE_REOPENED);
      if (status != MF_SHARED_SUCCESS) {
        return status;
      }
      status = finish_publication(MF_VIEW_PUBLISH_PUBLISHED);
      if (status != MF_SHARED_SUCCESS) {
        return status;
      }
    } else if (state == MF_DEVICE_UPDATE_ABORTED || state == MF_DEVICE_UPDATE_CLOSED) {
      mf_shared_status_v1 status = settle_publication(MF_VIEW_PUBLISH_ABORTED);
      if (status != MF_SHARED_SUCCESS) {
        return status;
      }
      status = retire_range(range_token.range_begin - 1U, MF_LIFECYCLE_RANGE_DISPOSITION_WHOLE);
      if (status != MF_SHARED_SUCCESS) {
        return status;
      }
      status = finish_update(state);
      if (status != MF_SHARED_SUCCESS) {
        return status;
      }
      status = finish_publication(MF_VIEW_PUBLISH_ABORTED);
      if (status != MF_SHARED_SUCCESS) {
        return status;
      }
    }
  }
  return MF_SHARED_SUCCESS;
}

mf_shared_status_v1 RegistryView::recover_telemetry_publish(mf_owner_identity_v1 owner) noexcept {
  if (extension_ == nullptr) {
    return MF_SHARED_NOT_SUPPORTED;
  }
  for (std::uint32_t slot = 0; slot < extension_->ordinary_telemetry_publish_capacity; ++slot) {
    auto& publication = telemetry_publish_records_[slot];
    const std::uint64_t sampled = mf_atomic_load_u64_seq_cst(&publication.tagged_state);
    const std::uint32_t tag = mf_tagged_record_tag_v1(sampled);
    const std::uint32_t state = mf_tagged_record_state_v1(sampled);
    if (state == MF_TELEMETRY_PUBLISH_FREE || state == MF_TELEMETRY_PUBLISH_INITIALIZING ||
        state == MF_TELEMETRY_PUBLISH_TERMINAL) {
      continue;
    }
    RecordHazard hazard(publication, &mf_telemetry_publish_record_v1::tagged_state, sampled);
    if (!hazard.held()) {
      continue;
    }
    if (!same_owner(publication.owner, owner)) {
      continue;
    }
    if (!same_token_view(publication.registry_view_id, view_id_) ||
        publication.operation_kind != MF_TELEMETRY_PUBLISH_KIND_NORMAL ||
        publication.target_bank > 1U || (publication.expected_latch_sequence & 1U) != 0U ||
        publication.expected_latch_sequence > std::numeric_limits<std::uint64_t>::max() - 2U ||
        publication.target_latch_sequence != publication.expected_latch_sequence + 2U ||
        publication.expected_snapshot_sequence == std::numeric_limits<std::uint64_t>::max() ||
        publication.target_snapshot_sequence != publication.expected_snapshot_sequence + 1U ||
        publication.expected_control_hash == std::numeric_limits<std::uint64_t>::max() ||
        publication.target_control_hash != publication.expected_control_hash + 1U ||
        mf_telemetry_active_bank_v1(publication.target_bank_state) != publication.target_bank ||
        mf_telemetry_state_v1(publication.target_bank_state) != MF_TELEMETRY_STATE_READY) {
      return quarantine();
    }
    const std::uint64_t latch =
        mf_atomic_load_u64_seq_cst(&telemetry_control_->telemetry_latch_sequence);
    std::uint32_t settled_state = state;
    if (state == MF_TELEMETRY_PUBLISH_PUBLISHED) {
      if (latch != publication.target_latch_sequence ||
          mf_atomic_load_u64_relaxed(&telemetry_control_->snapshot_sequence) !=
              publication.target_snapshot_sequence ||
          mf_atomic_load_u64_relaxed(&telemetry_control_->active_bank_state) !=
              publication.target_bank_state ||
          mf_atomic_load_u64_relaxed(&telemetry_control_->publish_generation) !=
              publication.target_control_hash) {
        return quarantine();
      }
    } else if (state == MF_TELEMETRY_PUBLISH_ABORTED) {
      if (latch != publication.expected_latch_sequence) {
        return quarantine();
      }
    } else if (state == MF_TELEMETRY_PUBLISH_PREPARED || state == MF_TELEMETRY_PUBLISH_LINKING) {
      if (latch != publication.expected_latch_sequence) {
        return quarantine();
      }
      settled_state = MF_TELEMETRY_PUBLISH_ABORTED;
    } else if (state == MF_TELEMETRY_PUBLISH_ACTIVE &&
               latch == publication.expected_latch_sequence) {
      settled_state = MF_TELEMETRY_PUBLISH_ABORTED;
    } else if (state == MF_TELEMETRY_PUBLISH_ACTIVE &&
               latch == publication.expected_latch_sequence + 1U) {
      if (mf_atomic_load_u64_acquire(&publication.publication_marker) != 0U) {
        mf_atomic_store_u64_relaxed(&telemetry_control_->snapshot_sequence,
                                    publication.target_snapshot_sequence);
        mf_atomic_store_u64_relaxed(&telemetry_control_->active_bank_state,
                                    publication.target_bank_state);
        mf_atomic_store_u64_relaxed(&telemetry_control_->publish_generation,
                                    publication.target_control_hash);
        mf_atomic_store_u64_release(&telemetry_control_->telemetry_latch_sequence,
                                    publication.target_latch_sequence);
        settled_state = MF_TELEMETRY_PUBLISH_PUBLISHED;
      } else {
        mf_atomic_store_u64_relaxed(&telemetry_control_->snapshot_sequence,
                                    publication.expected_snapshot_sequence);
        mf_atomic_store_u64_relaxed(&telemetry_control_->active_bank_state,
                                    publication.old_bank_state);
        mf_atomic_store_u64_relaxed(&telemetry_control_->publish_generation,
                                    publication.expected_control_hash);
        mf_atomic_store_u64_release(&telemetry_control_->telemetry_latch_sequence,
                                    publication.expected_latch_sequence);
        settled_state = MF_TELEMETRY_PUBLISH_ABORTED;
      }
    } else if (state == MF_TELEMETRY_PUBLISH_ACTIVE && latch == publication.target_latch_sequence) {
      if (mf_atomic_load_u64_relaxed(&telemetry_control_->snapshot_sequence) !=
              publication.target_snapshot_sequence ||
          mf_atomic_load_u64_relaxed(&telemetry_control_->active_bank_state) !=
              publication.target_bank_state ||
          mf_atomic_load_u64_relaxed(&telemetry_control_->publish_generation) !=
              publication.target_control_hash) {
        return quarantine();
      }
      settled_state = MF_TELEMETRY_PUBLISH_PUBLISHED;
    } else {
      return quarantine();
    }
    const mf_shared_status_v1 finished = finish_telemetry_publication(
        publication, *telemetry_publisher_, tag, slot, publication.target_bank, settled_state);
    if (finished == MF_SHARED_STALE_HANDLE) {
      return quarantine();
    }
    if (finished != MF_SHARED_SUCCESS) {
      return finished;
    }
  }
  return MF_SHARED_SUCCESS;
}

mf_shared_status_v1 RegistryView::publish_fence_recovery(
    std::uint32_t device_index, std::uint32_t expected_validation_generation,
    const FenceSnapshot& intended_fence, std::uint32_t& out_validation_generation) noexcept {
  if (extension_ == nullptr) {
    return MF_SHARED_NOT_SUPPORTED;
  }
  if (device_index >= device_count_ || intended_fence.device_state != MF_DEVICE_STATE_ONLINE ||
      intended_fence.identity_record_id != identities_[device_index].identity_record_id ||
      expected_validation_generation == 0U ||
      expected_validation_generation >= MF_DEVICE_GENERATION_MAX_NORMAL) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  std::uint64_t high_water = 0;
  mf_shared_status_v1 status = stable_high_water(view_control_, high_water);
  if (status != MF_SHARED_SUCCESS) {
    return status;
  }
  if (intended_fence.lifecycle_sequence <= high_water) {
    return MF_SHARED_RETRY;
  }
  LifecycleRangeToken range_token{};
  status = reserve_lifecycle_range(intended_fence.lifecycle_sequence - high_water, 0U, range_token);
  if (status != MF_SHARED_SUCCESS) {
    return status;
  }
  if (range_token.range_end != intended_fence.lifecycle_sequence) {
    (void)retire_lifecycle_range(range_token, range_token.range_begin - 1U);
    return MF_SHARED_RETRY;
  }

  mf_owner_identity_v1 owner{};
  if (current_owner_identity(owner) != MF_SHARED_SUCCESS) {
    (void)retire_lifecycle_range(range_token, range_token.range_begin - 1U);
    return MF_SHARED_SYSTEM_ERROR;
  }
  std::uint32_t publish_slot = 0;
  std::uint32_t publish_tag = 0;
  status = claim_record(
      view_publish_records_, extension_->ordinary_view_publish_capacity,
      &mf_view_publish_record_v1::tagged_state, MF_VIEW_PUBLISH_INITIALIZING,
      range_token.range_slot,
      [](std::uint32_t state) {
        return state == MF_VIEW_PUBLISH_FREE || state == MF_VIEW_PUBLISH_TERMINAL;
      },
      [](std::uint32_t tag, std::uint32_t state, std::uint32_t range_slot) {
        return mf_view_publish_state_pack_v1(tag, state, range_slot);
      },
      publish_slot, publish_tag);
  if (status != MF_SHARED_SUCCESS) {
    (void)retire_lifecycle_range(range_token, range_token.range_begin - 1U);
    return status;
  }
  auto& publication = view_publish_records_[publish_slot];
  publication.registry_view_id = view_id_;
  publication.owner = owner;
  publication.deadline_ns = 0U;
  publication.transaction_id = lifecycle_ranges_[range_token.range_slot].transaction_id;
  publication.range_begin = range_token.range_begin;
  publication.range_end = range_token.range_end;
  publication.expected_cursor = high_water;
  publication.target_cursor = intended_fence.lifecycle_sequence;
  publication.gate_generation = range_token.token_generation;
  publication.token_generation = range_token.token_generation;
  publication.target_fence_hash = fence_fingerprint(intended_fence);
  publication.range_slot = range_token.range_slot;
  publication.operation_kind = MF_VIEW_PUBLISH_KIND_NORMAL;
  publication.recovery_plan = MF_RECOVERY_PLAN_COMPLETE;
  std::uint64_t publication_initial_expected = mf_view_publish_state_pack_v1(
      publish_tag, MF_VIEW_PUBLISH_INITIALIZING, range_token.range_slot);
  if (!mf_atomic_compare_exchange_u64_seq_cst(
          &publication.tagged_state, &publication_initial_expected,
          mf_view_publish_state_pack_v1(publish_tag, MF_VIEW_PUBLISH_PREPARED,
                                        range_token.range_slot))) {
    (void)retire_lifecycle_range(range_token, range_token.range_begin - 1U);
    (void)finish_view_publication(publication, *view_publisher_, publish_tag, publish_slot,
                                  range_token.range_slot, MF_VIEW_PUBLISH_ABORTED);
    return MF_SHARED_RETRY;
  }

  std::uint32_t update_slot = 0;
  std::uint32_t update_tag = 0;
  status = claim_record(
      device_updates_, extension_->device_update_capacity,
      &mf_device_validation_update_record_v1::tagged_state, MF_DEVICE_UPDATE_INITIALIZING,
      publish_slot,
      [](std::uint32_t state) {
        return state == MF_DEVICE_UPDATE_FREE || state == MF_DEVICE_UPDATE_TERMINAL;
      },
      [](std::uint32_t tag, std::uint32_t state, std::uint32_t publish_slot) {
        return mf_device_update_state_pack_v1(tag, state, publish_slot);
      },
      update_slot, update_tag);
  if (status != MF_SHARED_SUCCESS) {
    std::uint64_t publication_expected = mf_view_publish_state_pack_v1(
        publish_tag, MF_VIEW_PUBLISH_PREPARED, range_token.range_slot);
    (void)mf_atomic_compare_exchange_u64_seq_cst(
        &publication.tagged_state, &publication_expected,
        mf_view_publish_state_pack_v1(publish_tag, MF_VIEW_PUBLISH_ABORTED,
                                      range_token.range_slot));
    (void)retire_lifecycle_range(range_token, range_token.range_begin - 1U);
    (void)finish_view_publication(publication, *view_publisher_, publish_tag, publish_slot,
                                  range_token.range_slot, MF_VIEW_PUBLISH_ABORTED);
    return status;
  }

  const std::uint32_t next_generation = expected_validation_generation + 1U;
  const std::uint64_t expected_device =
      mf_device_admission_pack_v1(expected_validation_generation, MF_DEVICE_ADMISSION_OPEN, 0U);
  const std::uint64_t target_device =
      mf_device_admission_pack_v1(next_generation, MF_DEVICE_ADMISSION_UPDATING, update_tag);
  auto& update = device_updates_[update_slot];
  update.registry_view_id = view_id_;
  update.identity_record_id = intended_fence.identity_record_id;
  update.owner = owner;
  update.deadline_ns = 0U;
  update.expected_device_control = expected_device;
  update.target_device_control = target_device;
  update.intended_fence_hash = publication.target_fence_hash;
  update.publication_marker = 0U;
  update.view_publish_slot = publish_slot;
  update.publish_tag = publish_tag;
  update.old_validation_generation = expected_validation_generation;
  update.new_validation_generation = next_generation;
  update.link_recovery_plan = MF_RECOVERY_PLAN_CANCEL;
  update.reopen_recovery_plan = MF_RECOVERY_PLAN_COMPLETE;
  update.intended_lifecycle_sequence = intended_fence.lifecycle_sequence;
  update.intended_epoch = intended_fence.epoch;
  update.intended_effective_quota_bytes = intended_fence.effective_quota_bytes;
  update.intended_policy_bits = intended_fence.policy_bits;
  update.intended_device_state = intended_fence.device_state;
  update.device_index = device_index;
  const auto cleanup_aborted_update = [&](std::uint32_t expected_state) -> mf_shared_status_v1 {
    std::uint64_t current =
        mf_device_update_state_pack_v1(update_tag, expected_state, publish_slot);
    if (!mf_atomic_compare_exchange_u64_seq_cst(
            &update.tagged_state, &current,
            mf_device_update_state_pack_v1(update_tag, MF_DEVICE_UPDATE_ABORTED, publish_slot)) &&
        (mf_tagged_record_tag_v1(current) != update_tag ||
         (mf_tagged_record_state_v1(current) != MF_DEVICE_UPDATE_ABORTED &&
          mf_tagged_record_state_v1(current) != MF_DEVICE_UPDATE_TERMINAL))) {
      return MF_SHARED_RETRY;
    }
    mf_shared_status_v1 cleanup =
        settle_view_publication(publication, *view_publisher_, publish_tag, publish_slot,
                                range_token.range_slot, MF_VIEW_PUBLISH_ABORTED);
    if (cleanup != MF_SHARED_SUCCESS) {
      return cleanup;
    }
    cleanup = retire_lifecycle_range(range_token, range_token.range_begin - 1U);
    if (cleanup != MF_SHARED_SUCCESS && cleanup != MF_SHARED_STALE_HANDLE) {
      return cleanup;
    }
    current = mf_device_update_state_pack_v1(update_tag, MF_DEVICE_UPDATE_ABORTED, publish_slot);
    if (!mf_atomic_compare_exchange_u64_seq_cst(
            &update.tagged_state, &current,
            mf_device_update_state_pack_v1(update_tag, MF_DEVICE_UPDATE_TERMINAL, publish_slot)) &&
        (mf_tagged_record_tag_v1(current) != update_tag ||
         mf_tagged_record_state_v1(current) != MF_DEVICE_UPDATE_TERMINAL)) {
      return MF_SHARED_RETRY;
    }
    return terminalize_view_publication(publication, publish_tag, range_token.range_slot,
                                        MF_VIEW_PUBLISH_ABORTED);
  };
  update.expected_fence_latch_sequence =
      mf_atomic_load_u64_acquire(&fences_[device_index].fence_latch_sequence);
  if ((update.expected_fence_latch_sequence & 1U) != 0U ||
      update.expected_fence_latch_sequence > std::numeric_limits<std::uint64_t>::max() - 2U) {
    (void)cleanup_aborted_update(MF_DEVICE_UPDATE_INITIALIZING);
    return (update.expected_fence_latch_sequence & 1U) != 0U ? MF_SHARED_RETRY : MF_SHARED_OVERFLOW;
  }
  update.target_fence_latch_sequence = update.expected_fence_latch_sequence + 2U;
  std::uint64_t update_initial_expected =
      mf_device_update_state_pack_v1(update_tag, MF_DEVICE_UPDATE_INITIALIZING, publish_slot);
  if (!mf_atomic_compare_exchange_u64_seq_cst(
          &update.tagged_state, &update_initial_expected,
          mf_device_update_state_pack_v1(update_tag, MF_DEVICE_UPDATE_PREPARED, publish_slot))) {
    std::uint64_t publication_expected = mf_view_publish_state_pack_v1(
        publish_tag, MF_VIEW_PUBLISH_PREPARED, range_token.range_slot);
    (void)mf_atomic_compare_exchange_u64_seq_cst(
        &publication.tagged_state, &publication_expected,
        mf_view_publish_state_pack_v1(publish_tag, MF_VIEW_PUBLISH_ABORTED,
                                      range_token.range_slot));
    (void)retire_lifecycle_range(range_token, range_token.range_begin - 1U);
    std::uint64_t update_initial =
        mf_device_update_state_pack_v1(update_tag, MF_DEVICE_UPDATE_INITIALIZING, publish_slot);
    (void)mf_atomic_compare_exchange_u64_seq_cst(
        &update.tagged_state, &update_initial,
        mf_device_update_state_pack_v1(update_tag, MF_DEVICE_UPDATE_TERMINAL, publish_slot));
    (void)finish_view_publication(publication, *view_publisher_, publish_tag, publish_slot,
                                  range_token.range_slot, MF_VIEW_PUBLISH_ABORTED);
    return MF_SHARED_RETRY;
  }

  status = acquire_publisher(&view_publisher_->tagged_owner, publish_tag, publish_slot);
  if (status != MF_SHARED_SUCCESS) {
    (void)cleanup_aborted_update(MF_DEVICE_UPDATE_PREPARED);
    return status;
  }
  std::uint64_t update_expected =
      mf_device_update_state_pack_v1(update_tag, MF_DEVICE_UPDATE_PREPARED, publish_slot);
  if (!mf_atomic_compare_exchange_u64_seq_cst(
          &update.tagged_state, &update_expected,
          mf_device_update_state_pack_v1(update_tag, MF_DEVICE_UPDATE_LINKING, publish_slot))) {
    release_publisher(&view_publisher_->tagged_owner, publish_tag, publish_slot);
    return MF_SHARED_RETRY;
  }
  std::uint64_t device_expected = expected_device;
  if (!mf_atomic_compare_exchange_u64_seq_cst(&device_admission_[device_index].state_generation_tag,
                                              &device_expected, target_device)) {
    (void)cleanup_aborted_update(MF_DEVICE_UPDATE_LINKING);
    return mf_device_admission_state_v1(device_expected) == MF_DEVICE_ADMISSION_CLOSED
               ? MF_SHARED_DEVICE_LOST
               : MF_SHARED_RETRY;
  }
  mf_atomic_store_u64_seq_cst(
      &publication.tagged_state,
      mf_view_publish_state_pack_v1(publish_tag, MF_VIEW_PUBLISH_ACTIVE, range_token.range_slot));
  mf_atomic_store_u64_seq_cst(
      &update.tagged_state,
      mf_device_update_state_pack_v1(update_tag, MF_DEVICE_UPDATE_ACTIVE, publish_slot));
  if (fault_point_ == RecoveryFaultPoint::FenceActive) {
    return MF_SHARED_INTERRUPTED;
  }

  status = write_fence(device_index, intended_fence, target_device,
                       update.expected_fence_latch_sequence);
  if (status != MF_SHARED_SUCCESS) {
    return status;
  }
  mf_atomic_store_u64_seq_cst(&update.publication_marker, 1U);
  mf_atomic_store_u64_seq_cst(
      &update.tagged_state,
      mf_device_update_state_pack_v1(update_tag, MF_DEVICE_UPDATE_FENCE_PUBLISHED, publish_slot));
  device_expected = target_device;
  if (!mf_atomic_compare_exchange_u64_seq_cst(
          &device_admission_[device_index].state_generation_tag, &device_expected,
          mf_device_admission_pack_v1(next_generation, MF_DEVICE_ADMISSION_OPEN, 0U))) {
    return MF_SHARED_DEVICE_LOST;
  }
  update_expected =
      mf_device_update_state_pack_v1(update_tag, MF_DEVICE_UPDATE_FENCE_PUBLISHED, publish_slot);
  if (!mf_atomic_compare_exchange_u64_seq_cst(
          &update.tagged_state, &update_expected,
          mf_device_update_state_pack_v1(update_tag, MF_DEVICE_UPDATE_REOPENED, publish_slot))) {
    return MF_SHARED_RETRY;
  }
  if (fault_point_ == RecoveryFaultPoint::UpdateReopened) {
    return MF_SHARED_INTERRUPTED;
  }
  std::uint64_t publication_expected =
      mf_view_publish_state_pack_v1(publish_tag, MF_VIEW_PUBLISH_ACTIVE, range_token.range_slot);
  if (!mf_atomic_compare_exchange_u64_seq_cst(
          &publication.tagged_state, &publication_expected,
          mf_view_publish_state_pack_v1(publish_tag, MF_VIEW_PUBLISH_PUBLISHED,
                                        range_token.range_slot))) {
    return MF_SHARED_RETRY;
  }
  if (fault_point_ == RecoveryFaultPoint::UpdatePublicationPublished) {
    return MF_SHARED_INTERRUPTED;
  }
  release_publisher(&view_publisher_->tagged_owner, publish_tag, publish_slot);
  if (fault_point_ == RecoveryFaultPoint::UpdatePublisherReleased) {
    return MF_SHARED_INTERRUPTED;
  }
  status = retire_lifecycle_range(range_token, range_token.range_end);
  if (status != MF_SHARED_SUCCESS) {
    return status;
  }
  if (fault_point_ == RecoveryFaultPoint::UpdateRangeRetired) {
    return MF_SHARED_INTERRUPTED;
  }
  update_expected =
      mf_device_update_state_pack_v1(update_tag, MF_DEVICE_UPDATE_REOPENED, publish_slot);
  if (!mf_atomic_compare_exchange_u64_seq_cst(
          &update.tagged_state, &update_expected,
          mf_device_update_state_pack_v1(update_tag, MF_DEVICE_UPDATE_TERMINAL, publish_slot))) {
    return MF_SHARED_RETRY;
  }
  if (fault_point_ == RecoveryFaultPoint::UpdateTerminal) {
    return MF_SHARED_INTERRUPTED;
  }
  status = terminalize_view_publication(publication, publish_tag, range_token.range_slot,
                                        MF_VIEW_PUBLISH_PUBLISHED);
  if (status != MF_SHARED_SUCCESS) {
    return status;
  }
  out_validation_generation = next_generation;
  return MF_SHARED_SUCCESS;
}

mf_shared_status_v1 RegistryView::publish_telemetry_recovery(
    std::span<const mf_virtual_device_telemetry_v1> rows) noexcept {
  if (extension_ == nullptr) {
    return MF_SHARED_NOT_SUPPORTED;
  }
  if (rows.size() != device_count_ ||
      mf_view_admission_state_v1(mf_atomic_load_u64_seq_cst(&view_admission_->state_generation)) !=
          MF_VIEW_ADMISSION_OPEN) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  for (std::uint32_t index = 0; index < device_count_; ++index) {
    if (rows[index].identity_record_id != identities_[index].identity_record_id ||
        rows[index].observed_lifecycle_sequence == 0U) {
      return MF_SHARED_INVALID_ARGUMENT;
    }
  }
  mf_owner_identity_v1 owner{};
  if (current_owner_identity(owner) != MF_SHARED_SUCCESS) {
    return MF_SHARED_SYSTEM_ERROR;
  }
  std::uint32_t publish_slot = 0;
  std::uint32_t publish_tag = 0;
  mf_shared_status_v1 status = claim_record(
      telemetry_publish_records_, extension_->ordinary_telemetry_publish_capacity,
      &mf_telemetry_publish_record_v1::tagged_state, MF_TELEMETRY_PUBLISH_INITIALIZING, 0U,
      [](std::uint32_t state) {
        return state == MF_TELEMETRY_PUBLISH_FREE || state == MF_TELEMETRY_PUBLISH_TERMINAL;
      },
      [](std::uint32_t tag, std::uint32_t state, std::uint32_t bank) {
        return mf_telemetry_publish_state_pack_v1(tag, state, bank);
      },
      publish_slot, publish_tag);
  if (status != MF_SHARED_SUCCESS) {
    return status;
  }
  auto& publication = telemetry_publish_records_[publish_slot];
  std::uint64_t even = mf_atomic_load_u64_seq_cst(&telemetry_control_->telemetry_latch_sequence);
  const std::uint64_t previous_snapshot =
      mf_atomic_load_u64_relaxed(&telemetry_control_->snapshot_sequence);
  const std::uint64_t old_bank_state =
      mf_atomic_load_u64_relaxed(&telemetry_control_->active_bank_state);
  const std::uint64_t previous_publish_generation =
      mf_atomic_load_u64_relaxed(&telemetry_control_->publish_generation);
  if ((even & 1U) != 0U || even > std::numeric_limits<std::uint64_t>::max() - 2U ||
      previous_snapshot == std::numeric_limits<std::uint64_t>::max() ||
      previous_publish_generation == std::numeric_limits<std::uint64_t>::max()) {
    std::uint64_t publication_expected =
        mf_telemetry_publish_state_pack_v1(publish_tag, MF_TELEMETRY_PUBLISH_INITIALIZING, 0U);
    (void)mf_atomic_compare_exchange_u64_seq_cst(
        &publication.tagged_state, &publication_expected,
        mf_telemetry_publish_state_pack_v1(publish_tag, MF_TELEMETRY_PUBLISH_TERMINAL, 0U));
    return (even & 1U) != 0U ? MF_SHARED_RETRY : MF_SHARED_OVERFLOW;
  }
  const std::uint32_t target_bank = mf_telemetry_active_bank_v1(old_bank_state) ^ 1U;
  publication.registry_view_id = view_id_;
  publication.owner = owner;
  publication.deadline_ns = 0U;
  publication.expected_latch_sequence = even;
  publication.target_latch_sequence = even + 2U;
  publication.expected_snapshot_sequence = previous_snapshot;
  publication.target_snapshot_sequence = previous_snapshot + 1U;
  publication.old_bank_state = old_bank_state;
  publication.target_bank_state =
      mf_telemetry_bank_state_pack_v1(target_bank, MF_TELEMETRY_STATE_READY);
  publication.staging_offset =
      target_bank == 0U ? header_->telemetry_bank0_offset : header_->telemetry_bank1_offset;
  publication.staging_size =
      static_cast<std::uint64_t>(rows.size()) * sizeof(mf_virtual_device_telemetry_v1);
  publication.expected_control_hash = previous_publish_generation;
  publication.target_control_hash = previous_publish_generation + 1U;
  publication.publication_marker = 0U;
  publication.view_validation_generation = mf_view_admission_generation_v1(
      mf_atomic_load_u64_seq_cst(&view_admission_->state_generation));
  publication.operation_kind = MF_TELEMETRY_PUBLISH_KIND_NORMAL;
  publication.recovery_plan = MF_RECOVERY_PLAN_RECONCILE;
  publication.target_bank = target_bank;
  std::uint64_t publication_expected =
      mf_telemetry_publish_state_pack_v1(publish_tag, MF_TELEMETRY_PUBLISH_INITIALIZING, 0U);
  if (!mf_atomic_compare_exchange_u64_seq_cst(
          &publication.tagged_state, &publication_expected,
          mf_telemetry_publish_state_pack_v1(publish_tag, MF_TELEMETRY_PUBLISH_PREPARED,
                                             target_bank))) {
    return MF_SHARED_RETRY;
  }
  status = acquire_publisher(&telemetry_publisher_->tagged_owner, publish_tag, publish_slot);
  if (status != MF_SHARED_SUCCESS) {
    (void)finish_telemetry_publication(publication, *telemetry_publisher_, publish_tag,
                                       publish_slot, target_bank, MF_TELEMETRY_PUBLISH_ABORTED);
    return status;
  }
  std::uint64_t record_expected =
      mf_telemetry_publish_state_pack_v1(publish_tag, MF_TELEMETRY_PUBLISH_PREPARED, target_bank);
  if (!mf_atomic_compare_exchange_u64_seq_cst(
          &publication.tagged_state, &record_expected,
          mf_telemetry_publish_state_pack_v1(publish_tag, MF_TELEMETRY_PUBLISH_LINKING,
                                             target_bank))) {
    (void)finish_telemetry_publication(publication, *telemetry_publisher_, publish_tag,
                                       publish_slot, target_bank, MF_TELEMETRY_PUBLISH_ABORTED);
    return MF_SHARED_RETRY;
  }
  mf_atomic_store_u64_seq_cst(
      &publication.tagged_state,
      mf_telemetry_publish_state_pack_v1(publish_tag, MF_TELEMETRY_PUBLISH_ACTIVE, target_bank));
  std::uint64_t expected_latch = even;
  if (!mf_atomic_compare_exchange_u64_seq_cst(&telemetry_control_->telemetry_latch_sequence,
                                              &expected_latch, even + 1U)) {
    (void)finish_telemetry_publication(publication, *telemetry_publisher_, publish_tag,
                                       publish_slot, target_bank, MF_TELEMETRY_PUBLISH_ABORTED);
    return MF_SHARED_RETRY;
  }
  if (fault_point_ == RecoveryFaultPoint::TelemetryActiveOdd) {
    return MF_SHARED_INTERRUPTED;
  }
  for (std::uint32_t index = 0; index < device_count_; ++index) {
    auto& destination = telemetry_banks_[target_bank][index];
    mf_atomic_store_u64_relaxed(&destination.identity_record_id, rows[index].identity_record_id);
    mf_atomic_store_u64_relaxed(&destination.observed_lifecycle_sequence,
                                rows[index].observed_lifecycle_sequence);
    mf_atomic_store_u64_relaxed(&destination.committed_work_items,
                                rows[index].committed_work_items);
    mf_atomic_store_u64_relaxed(&destination.completed_work_items,
                                rows[index].completed_work_items);
    mf_atomic_store_u64_relaxed(&destination.active_time_ns, rows[index].active_time_ns);
    mf_atomic_store_u64_relaxed(&destination.memory_active_time_ns,
                                rows[index].memory_active_time_ns);
    mf_atomic_store_u64_relaxed(&destination.memory_used_bytes, rows[index].memory_used_bytes);
    mf_atomic_store_u64_relaxed(&destination.memory_capacity_bytes,
                                rows[index].memory_capacity_bytes);
    mf_atomic_store_u64_relaxed(&destination.sample_time_ns, rows[index].sample_time_ns);
  }
  mf_atomic_store_u64_release(&publication.publication_marker, 1U);
  mf_atomic_store_u64_relaxed(&telemetry_control_->snapshot_sequence,
                              publication.target_snapshot_sequence);
  mf_atomic_store_u64_relaxed(&telemetry_control_->active_bank_state,
                              publication.target_bank_state);
  mf_atomic_store_u64_relaxed(&telemetry_control_->publish_generation,
                              publication.target_control_hash);
  mf_atomic_store_u64_release(&telemetry_control_->telemetry_latch_sequence,
                              publication.target_latch_sequence);
  return finish_telemetry_publication(publication, *telemetry_publisher_, publish_tag, publish_slot,
                                      target_bank, MF_TELEMETRY_PUBLISH_PUBLISHED);
}

mf_shared_status_v1 RegistryView::close_recovery() noexcept {
  if (extension_ == nullptr) {
    return MF_SHARED_NOT_SUPPORTED;
  }
  const std::uint32_t current_state =
      mf_view_admission_state_v1(mf_atomic_load_u64_seq_cst(&view_admission_->state_generation));
  if (current_state == MF_VIEW_ADMISSION_TERMINAL ||
      current_state == MF_VIEW_ADMISSION_QUARANTINED) {
    return MF_SHARED_SUCCESS;
  }
  const mf_shared_status_v1 recovered = recover();
  if (recovered != MF_SHARED_SUCCESS) {
    return recovered;
  }
  mf_owner_identity_v1 owner{};
  if (current_owner_identity(owner) != MF_SHARED_SUCCESS) {
    return MF_SHARED_SYSTEM_ERROR;
  }

  auto& closing_record = view_publish_records_[extension_->close_closing_slot];
  closing_record.owner = owner;
  mf_atomic_store_u64_seq_cst(&closing_record.tagged_state,
                              mf_view_publish_state_pack_v1(MF_SHARED_RECORD_TAG_TERMINAL,
                                                            MF_VIEW_PUBLISH_ACTIVE,
                                                            MF_SHARED_RECORD_SLOT_NONE));
  mf_shared_status_v1 status =
      acquire_publisher(&view_publisher_->tagged_owner, MF_SHARED_RECORD_TAG_TERMINAL,
                        extension_->close_closing_slot);
  if (status != MF_SHARED_SUCCESS) {
    return status;
  }
  std::uint64_t admission = mf_atomic_load_u64_seq_cst(&view_admission_->state_generation);
  for (;;) {
    if (mf_view_admission_state_v1(admission) == MF_VIEW_ADMISSION_CLOSING) {
      break;
    }
    if (mf_view_admission_state_v1(admission) != MF_VIEW_ADMISSION_OPEN) {
      release_publisher(&view_publisher_->tagged_owner, MF_SHARED_RECORD_TAG_TERMINAL,
                        extension_->close_closing_slot);
      return MF_SHARED_TERMINAL_VIEW;
    }
    if (mf_atomic_compare_exchange_u64_seq_cst(
            &view_admission_->state_generation, &admission,
            mf_view_admission_pack_v1(MF_VIEW_GENERATION_TERMINAL, MF_VIEW_ADMISSION_CLOSING))) {
      break;
    }
  }
  mf_atomic_store_u64_seq_cst(&closing_record.tagged_state,
                              mf_view_publish_state_pack_v1(MF_SHARED_RECORD_TAG_TERMINAL,
                                                            MF_VIEW_PUBLISH_PUBLISHED,
                                                            MF_SHARED_RECORD_SLOT_NONE));
  release_publisher(&view_publisher_->tagged_owner, MF_SHARED_RECORD_TAG_TERMINAL,
                    extension_->close_closing_slot);

  // Seal every reusable claim word after CLOSING. A claimant wins its CAS first and is
  // observed here, or its stale CAS fails against the terminal tag installed here.
  for (std::uint32_t slot = 0; slot < extension_->admission_attempt_capacity; ++slot) {
    auto& attempt = admission_attempts_[slot];
    std::uint64_t observed = mf_atomic_load_u64_seq_cst(&attempt.tagged_phase);
    for (;;) {
      const std::uint32_t state = mf_tagged_record_state_v1(observed);
      if (state == MF_ADMISSION_ATTEMPT_ENTERING) {
        return quarantine();
      }
      if (state != MF_ADMISSION_ATTEMPT_IDLE && state != MF_ADMISSION_ATTEMPT_EXITED) {
        break;
      }
      const std::uint64_t sealed = mf_admission_attempt_phase_pack_v1(MF_SHARED_RECORD_TAG_TERMINAL,
                                                                      MF_ADMISSION_ATTEMPT_EXITED);
      if (observed == sealed ||
          mf_atomic_compare_exchange_u64_seq_cst(&attempt.tagged_phase, &observed, sealed)) {
        break;
      }
    }
  }
  for (std::uint32_t slot = 0; slot < extension_->admission_lease_capacity; ++slot) {
    auto& lease = admission_leases_[slot];
    std::uint64_t observed = mf_atomic_load_u64_seq_cst(&lease.tagged_state);
    for (;;) {
      const std::uint32_t state = mf_tagged_record_state_v1(observed);
      if (state == MF_ADMISSION_LEASE_INITIALIZING) {
        return quarantine();
      }
      if (state != MF_ADMISSION_LEASE_FREE && state != MF_ADMISSION_LEASE_RELEASED &&
          state != MF_ADMISSION_LEASE_REVOKED && state != MF_ADMISSION_LEASE_TOMBSTONED) {
        break;
      }
      const std::uint64_t sealed = mf_admission_lease_state_pack_v1(
          MF_SHARED_RECORD_TAG_TERMINAL, MF_ADMISSION_LEASE_TOMBSTONED,
          mf_tagged_record_auxiliary_v1(observed));
      if (observed == sealed ||
          mf_atomic_compare_exchange_u64_seq_cst(&lease.tagged_state, &observed, sealed)) {
        break;
      }
    }
  }

  for (std::uint32_t slot = 0; slot < extension_->admission_lease_capacity; ++slot) {
    auto& lease = admission_leases_[slot];
    std::uint64_t observed = mf_atomic_load_u64_seq_cst(&lease.tagged_state);
    for (;;) {
      const std::uint32_t state = mf_tagged_record_state_v1(observed);
      const std::uint32_t tag = mf_tagged_record_tag_v1(observed);
      if (tag == MF_SHARED_RECORD_TAG_TERMINAL) {
        break;
      }
      std::uint32_t target = state;
      if (state == MF_ADMISSION_LEASE_INITIALIZING) {
        return quarantine();
      }
      if (state == MF_ADMISSION_LEASE_RESERVED || state == MF_ADMISSION_LEASE_COMMITTING) {
        target = MF_ADMISSION_LEASE_TOMBSTONED;
      } else if (state == MF_ADMISSION_LEASE_COMMITTED || state == MF_ADMISSION_LEASE_PUBLISHED) {
        target = MF_ADMISSION_LEASE_RELEASED;
      } else if (state != MF_ADMISSION_LEASE_FREE && state != MF_ADMISSION_LEASE_RELEASED &&
                 state != MF_ADMISSION_LEASE_REVOKED && state != MF_ADMISSION_LEASE_TOMBSTONED) {
        break;
      }
      if (mf_atomic_compare_exchange_u64_seq_cst(
              &lease.tagged_state, &observed,
              mf_admission_lease_state_pack_v1(MF_SHARED_RECORD_TAG_TERMINAL, target,
                                               mf_tagged_record_auxiliary_v1(observed)))) {
        break;
      }
    }
  }
  for (std::uint32_t slot = 0; slot < extension_->admission_attempt_capacity; ++slot) {
    auto& attempt = admission_attempts_[slot];
    std::uint64_t observed = mf_atomic_load_u64_seq_cst(&attempt.tagged_phase);
    for (;;) {
      if (mf_tagged_record_tag_v1(observed) == MF_SHARED_RECORD_TAG_TERMINAL) {
        break;
      }
      if (mf_tagged_record_state_v1(observed) == MF_ADMISSION_ATTEMPT_ENTERING) {
        return quarantine();
      }
      if (mf_atomic_compare_exchange_u64_seq_cst(
              &attempt.tagged_phase, &observed,
              mf_admission_attempt_phase_pack_v1(MF_SHARED_RECORD_TAG_TERMINAL,
                                                 MF_ADMISSION_ATTEMPT_EXITED))) {
        break;
      }
    }
  }
  for (std::uint32_t index = 0; index < device_count_; ++index) {
    mf_atomic_store_u64_seq_cst(
        &device_admission_[index].state_generation_tag,
        mf_device_admission_pack_v1(MF_DEVICE_GENERATION_TERMINAL, MF_DEVICE_ADMISSION_CLOSED, 0U));
  }

  for (;;) {
    const std::uint64_t head = mf_atomic_load_u64_seq_cst(&view_control_->reservation_head_slot);
    if (head == MF_SHARED_RECORD_SLOT_NONE) {
      break;
    }
    if (head >= extension_->lifecycle_range_capacity) {
      return quarantine();
    }
    auto& range = lifecycle_ranges_[head];
    const std::uint64_t word = mf_atomic_load_u64_seq_cst(&range.tagged_state);
    if (mf_tagged_record_state_v1(word) != MF_LIFECYCLE_RANGE_OPEN) {
      return quarantine();
    }
    const LifecycleRangeToken token{
        .registry_view_id = view_id_,
        .range_begin = range.range_begin,
        .range_end = range.range_end,
        .token_generation = range.token_generation,
        .range_slot = static_cast<std::uint32_t>(head),
        .range_tag = mf_tagged_record_tag_v1(word),
    };
    status = retire_lifecycle_range(token, token.range_begin - 1U);
    if (status != MF_SHARED_SUCCESS) {
      return status;
    }
  }

  auto& telemetry_terminal = telemetry_publish_records_[extension_->telemetry_terminal_slot];
  telemetry_terminal.owner = owner;
  status = acquire_publisher(&telemetry_publisher_->tagged_owner, MF_SHARED_RECORD_TAG_TERMINAL,
                             extension_->telemetry_terminal_slot);
  if (status != MF_SHARED_SUCCESS) {
    return status;
  }
  mf_atomic_store_u64_seq_cst(&telemetry_terminal.tagged_state,
                              mf_telemetry_publish_state_pack_v1(MF_SHARED_RECORD_TAG_TERMINAL,
                                                                 MF_TELEMETRY_PUBLISH_ACTIVE, 0U));
  std::uint64_t telemetry_even =
      mf_atomic_load_u64_seq_cst(&telemetry_control_->telemetry_latch_sequence);
  if ((telemetry_even & 1U) != 0U ||
      telemetry_even > std::numeric_limits<std::uint64_t>::max() - 2U) {
    release_publisher(&telemetry_publisher_->tagged_owner, MF_SHARED_RECORD_TAG_TERMINAL,
                      extension_->telemetry_terminal_slot);
    return MF_SHARED_RETRY;
  }
  std::uint64_t telemetry_expected = telemetry_even;
  if (!mf_atomic_compare_exchange_u64_seq_cst(&telemetry_control_->telemetry_latch_sequence,
                                              &telemetry_expected, telemetry_even + 1U)) {
    release_publisher(&telemetry_publisher_->tagged_owner, MF_SHARED_RECORD_TAG_TERMINAL,
                      extension_->telemetry_terminal_slot);
    return MF_SHARED_RETRY;
  }
  const std::uint64_t bank_state =
      mf_atomic_load_u64_relaxed(&telemetry_control_->active_bank_state);
  mf_atomic_store_u64_relaxed(
      &telemetry_control_->active_bank_state,
      mf_telemetry_bank_state_pack_v1(mf_telemetry_active_bank_v1(bank_state),
                                      MF_TELEMETRY_STATE_TERMINAL));
  mf_atomic_store_u32_relaxed(&telemetry_control_->row_count, 0U);
  mf_atomic_store_u64_release(&telemetry_control_->telemetry_latch_sequence, telemetry_even + 2U);
  mf_atomic_store_u64_seq_cst(&telemetry_terminal.tagged_state,
                              mf_telemetry_publish_state_pack_v1(MF_SHARED_RECORD_TAG_TERMINAL,
                                                                 MF_TELEMETRY_PUBLISH_TERMINAL,
                                                                 0U));
  mf_atomic_store_u64_seq_cst(&telemetry_publisher_->tagged_owner,
                              mf_publisher_control_pack_v1(MF_SHARED_RECORD_TAG_TERMINAL,
                                                           MF_PUBLISHER_QUARANTINED,
                                                           extension_->telemetry_terminal_slot));

  auto& terminal_record = view_publish_records_[extension_->close_terminal_slot];
  terminal_record.owner = owner;
  status = acquire_publisher(&view_publisher_->tagged_owner, MF_SHARED_RECORD_TAG_TERMINAL,
                             extension_->close_terminal_slot);
  if (status != MF_SHARED_SUCCESS) {
    return status;
  }
  mf_atomic_store_u64_seq_cst(&terminal_record.tagged_state,
                              mf_view_publish_state_pack_v1(MF_SHARED_RECORD_TAG_TERMINAL,
                                                            MF_VIEW_PUBLISH_ACTIVE,
                                                            MF_SHARED_RECORD_SLOT_NONE));
  std::uint64_t even = mf_atomic_load_u64_seq_cst(&view_control_->control_latch_sequence);
  std::uint64_t expected = even;
  if ((even & 1U) != 0U || even > std::numeric_limits<std::uint64_t>::max() - 2U ||
      !mf_atomic_compare_exchange_u64_seq_cst(&view_control_->control_latch_sequence, &expected,
                                              even + 1U)) {
    release_publisher(&view_publisher_->tagged_owner, MF_SHARED_RECORD_TAG_TERMINAL,
                      extension_->close_terminal_slot);
    return MF_SHARED_RETRY;
  }
  mf_atomic_store_u64_relaxed(&view_control_->publication_cursor,
                              std::numeric_limits<std::uint64_t>::max());
  mf_atomic_store_u64_relaxed(&view_control_->gate_generation,
                              std::numeric_limits<std::uint64_t>::max());
  mf_atomic_store_u32_relaxed(&view_control_->gate_state, MF_VIEW_GATE_TERMINAL);
  mf_atomic_store_u32_relaxed(&view_control_->mapping_terminal, 1U);
  mf_atomic_store_u64_release(&view_control_->control_latch_sequence, even + 2U);
  mf_atomic_store_u64_seq_cst(&terminal_record.tagged_state,
                              mf_view_publish_state_pack_v1(MF_SHARED_RECORD_TAG_TERMINAL,
                                                            MF_VIEW_PUBLISH_TERMINAL,
                                                            MF_SHARED_RECORD_SLOT_NONE));
  mf_atomic_store_u64_seq_cst(&view_publisher_->tagged_owner,
                              mf_publisher_control_pack_v1(MF_SHARED_RECORD_TAG_TERMINAL,
                                                           MF_PUBLISHER_QUARANTINED,
                                                           extension_->close_terminal_slot));
  mf_atomic_store_u64_seq_cst(
      &view_admission_->state_generation,
      mf_view_admission_pack_v1(MF_VIEW_GENERATION_TERMINAL, MF_VIEW_ADMISSION_TERMINAL));
  return MF_SHARED_SUCCESS;
}

} // namespace metaflux::runtime
