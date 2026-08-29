#ifndef METAFLUX_RUNTIME_RECOVERY_MODEL_HPP
#define METAFLUX_RUNTIME_RECOVERY_MODEL_HPP

#include <array>
#include <cstdint>

namespace metaflux::runtime::recovery {

enum class ViewState : std::uint8_t { Open = 1, Closing = 2, Terminal = 3, Quarantined = 4 };
enum class DeviceState : std::uint8_t { Open = 1, Updating = 2, Closed = 3 };
enum class OwnerState : std::uint8_t { Live, Dead };
enum class AttemptState : std::uint8_t {
  Idle,
  Entering,
  Claiming,
  Initializing,
  Ready,
  Exited,
};
enum class LeaseState : std::uint8_t {
  Free,
  Initializing,
  Reserved,
  Committing,
  Committed,
  Published,
  Released,
  Revoked,
  Tombstoned,
  Quarantined,
};
enum class UpdateState : std::uint8_t {
  Free,
  Initializing,
  Prepared,
  Linking,
  Active,
  FencePublished,
  Reopened,
  Aborted,
  Closed,
  Terminal,
  Quarantined,
};
enum class PublishState : std::uint8_t {
  Free,
  Initializing,
  Prepared,
  Linking,
  Active,
  Published,
  Aborted,
  Terminal,
  Quarantined,
};
enum class RangeState : std::uint8_t {
  Free,
  Initializing,
  Prepared,
  Open,
  Retired,
  Terminal,
  Quarantined,
};

struct Limits final {
  std::uint64_t sequence_terminal;
  std::uint32_t generation_terminal;
  std::uint32_t tag_terminal;
  std::uint64_t epoch_terminal;
};

struct ViewId final {
  std::uint64_t daemon_incarnation = 1;
  std::uint64_t view_serial = 1;
};

struct ReadSnapshot final {
  ViewId view_id{};
  std::uint32_t view_generation = 0;
  std::uint32_t device_generation = 0;
  std::uint64_t epoch = 0;
};

struct AdmissionToken final {
  ViewId view_id{};
  std::uint32_t view_generation = 0;
  std::uint32_t device_generation = 0;
  std::uint32_t attempt_tag = 0;
  std::uint32_t lease_tag = 0;
};

struct AttemptRecord final {
  std::uint32_t tag = 0;
  AttemptState state = AttemptState::Idle;
  OwnerState owner = OwnerState::Live;
  std::uint32_t view_generation = 0;
  std::uint32_t device_generation = 0;
  std::uint32_t target_lease_tag = 0;
};

struct LeaseRecord final {
  std::uint32_t tag = 0;
  LeaseState state = LeaseState::Free;
  OwnerState owner = OwnerState::Live;
  std::uint32_t view_generation = 0;
  std::uint32_t device_generation = 0;
  bool target_written = false;
  bool publication_marker = false;
};

struct UpdateRecord final {
  std::uint32_t tag = 0;
  UpdateState state = UpdateState::Free;
  OwnerState owner = OwnerState::Live;
  std::uint32_t old_generation = 0;
  std::uint32_t new_generation = 0;
  std::uint64_t old_epoch = 0;
  std::uint64_t new_epoch = 0;
  bool fence_published = false;
};

struct RangeRecord final {
  std::uint32_t tag = 0;
  RangeState state = RangeState::Free;
  std::uint64_t begin = 0;
  std::uint64_t end = 0;
  std::uint32_t token_generation = 0;
  std::uint64_t unused_suffix_begin = 0;
  std::uint64_t unused_suffix_end = 0;
};

struct RangeToken final {
  ViewId view_id{};
  std::uint32_t view_generation = 0;
  std::uint32_t token_generation = 0;
  std::uint32_t range_tag = 0;
  std::uint64_t range_begin = 0;
  std::uint64_t range_end = 0;
};

struct PublishRecord final {
  std::uint32_t tag = 0;
  PublishState state = PublishState::Free;
  OwnerState owner = OwnerState::Live;
  std::uint32_t range_tag = 0;
  std::uint32_t token_generation = 0;
  std::uint64_t range_begin = 0;
  std::uint64_t range_end = 0;
  std::uint64_t target_sequence = 0;
  bool payload_applied = false;
  bool requires_compensation = false;
};

class Model final {
public:
  explicit Model(Limits limits, std::uint64_t initial_high_water = 0,
                 std::uint32_t initial_generation = 1, std::uint64_t initial_epoch = 1,
                 ViewId view_id = {}) noexcept;

  [[nodiscard]] static bool checked_add_below_terminal(std::uint64_t current,
                                                       std::uint64_t increment,
                                                       std::uint64_t terminal,
                                                       std::uint64_t& out_value) noexcept;

  [[nodiscard]] bool enter_attempt(std::uint32_t attempt_tag) noexcept;
  [[nodiscard]] bool claim_lease(std::uint32_t attempt_tag, std::uint32_t lease_tag) noexcept;
  [[nodiscard]] bool materialize_lease(std::uint32_t attempt_tag, std::uint32_t lease_tag) noexcept;
  [[nodiscard]] bool reserve_lease(std::uint32_t attempt_tag, std::uint32_t lease_tag) noexcept;
  [[nodiscard]] bool current_admission_token(AdmissionToken& out_token) const noexcept;
  [[nodiscard]] bool begin_lease_commit(const AdmissionToken& token) noexcept;
  [[nodiscard]] bool begin_lease_commit(std::uint32_t lease_tag) noexcept;
  [[nodiscard]] bool finish_lease_commit(std::uint32_t lease_tag) noexcept;
  [[nodiscard]] bool write_lease_target(std::uint32_t lease_tag) noexcept;
  [[nodiscard]] bool publish_lease(std::uint32_t lease_tag) noexcept;
  [[nodiscard]] bool release_lease(std::uint32_t lease_tag) noexcept;
  [[nodiscard]] bool recover_dead_lease(std::uint32_t lease_tag) noexcept;
  [[nodiscard]] bool expire_live_lease(std::uint32_t lease_tag) noexcept;
  [[nodiscard]] bool reclaim_lease(std::uint32_t new_tag) noexcept;

  void close_view() noexcept;
  void close_device() noexcept;

  [[nodiscard]] bool claim_device_update(std::uint32_t update_tag) noexcept;
  [[nodiscard]] bool materialize_device_update(std::uint32_t update_tag) noexcept;
  [[nodiscard]] bool prepare_device_update(std::uint32_t update_tag) noexcept;
  [[nodiscard]] bool begin_device_update_link(std::uint32_t update_tag) noexcept;
  [[nodiscard]] bool complete_device_update_link(std::uint32_t update_tag) noexcept;
  [[nodiscard]] bool link_device_update(std::uint32_t update_tag) noexcept;
  [[nodiscard]] bool publish_device_update(std::uint32_t update_tag) noexcept;
  [[nodiscard]] bool reopen_device_update(std::uint32_t update_tag) noexcept;
  [[nodiscard]] bool finish_device_update(std::uint32_t update_tag) noexcept;
  [[nodiscard]] bool recover_dead_device_update(std::uint32_t update_tag) noexcept;
  [[nodiscard]] bool expire_live_device_update(std::uint32_t update_tag) noexcept;

  [[nodiscard]] bool claim_range(std::uint32_t range_tag, std::uint64_t length) noexcept;
  [[nodiscard]] bool prepare_range(std::uint32_t range_tag) noexcept;
  [[nodiscard]] bool open_range(std::uint32_t range_tag) noexcept;
  [[nodiscard]] bool reserve_range(std::uint32_t range_tag, std::uint64_t length) noexcept;
  [[nodiscard]] bool current_range_token(RangeToken& out_token) const noexcept;
  [[nodiscard]] bool following_range_token(RangeToken& out_token) const noexcept;
  [[nodiscard]] bool range_token_valid(const RangeToken& token) const noexcept;
  [[nodiscard]] bool advance_range_head() noexcept;

  [[nodiscard]] bool claim_publication(const RangeToken& token, std::uint32_t publish_tag,
                                       bool requires_compensation) noexcept;
  [[nodiscard]] bool claim_publication(std::uint32_t publish_tag,
                                       bool requires_compensation) noexcept;
  [[nodiscard]] bool prepare_publication(std::uint32_t publish_tag) noexcept;
  [[nodiscard]] bool link_publication(std::uint32_t publish_tag) noexcept;
  [[nodiscard]] bool activate_publication(std::uint32_t publish_tag) noexcept;
  [[nodiscard]] bool apply_publication(std::uint32_t publish_tag) noexcept;
  [[nodiscard]] bool mark_publication(std::uint32_t publish_tag) noexcept;
  [[nodiscard]] bool finish_publication(std::uint32_t publish_tag) noexcept;
  [[nodiscard]] bool abort_publication(std::uint32_t publish_tag) noexcept;
  [[nodiscard]] bool recover_dead_publication(std::uint32_t publish_tag) noexcept;
  [[nodiscard]] bool expire_live_publication(std::uint32_t publish_tag) noexcept;
  [[nodiscard]] bool reclaim_publication(std::uint32_t new_tag) noexcept;

  [[nodiscard]] bool begin_read(ReadSnapshot& out_snapshot) const noexcept;
  [[nodiscard]] bool finish_read(const ReadSnapshot& snapshot) const noexcept;

  [[nodiscard]] bool valid() const noexcept { return valid_; }
  [[nodiscard]] bool invariants_hold() const noexcept;
  [[nodiscard]] bool has_live_lease() const noexcept;
  [[nodiscard]] bool intermediate_visible() const noexcept { return intermediate_visible_; }
  [[nodiscard]] ViewState view_state() const noexcept { return view_state_; }
  [[nodiscard]] DeviceState device_state() const noexcept { return device_state_; }
  [[nodiscard]] std::uint32_t view_generation() const noexcept { return view_generation_; }
  [[nodiscard]] std::uint32_t device_generation() const noexcept { return device_generation_; }
  [[nodiscard]] std::uint32_t range_token_generation() const noexcept {
    return range_token_generation_;
  }
  [[nodiscard]] std::uint64_t epoch() const noexcept { return epoch_; }
  [[nodiscard]] std::uint64_t allocation_high_water() const noexcept {
    return allocation_high_water_;
  }
  [[nodiscard]] std::uint64_t publication_cursor() const noexcept { return publication_cursor_; }
  [[nodiscard]] std::uint32_t committed_admissions() const noexcept {
    return committed_admissions_;
  }
  [[nodiscard]] std::uint32_t compensation_count() const noexcept { return compensation_count_; }
  [[nodiscard]] const AttemptRecord& attempt() const noexcept { return attempt_; }
  [[nodiscard]] const LeaseRecord& lease() const noexcept { return lease_; }
  [[nodiscard]] const UpdateRecord& update() const noexcept { return update_; }
  [[nodiscard]] const RangeRecord& range() const noexcept { return range_; }
  [[nodiscard]] const RangeRecord& following_range() const noexcept { return following_range_; }
  [[nodiscard]] std::uint32_t retired_range_count() const noexcept { return retired_range_count_; }
  [[nodiscard]] bool retired_range(std::uint32_t index, RangeRecord& out_range) const noexcept;
  [[nodiscard]] const PublishRecord& publication() const noexcept { return publication_; }

private:
  [[nodiscard]] bool next_tag(std::uint32_t current, std::uint32_t proposed) const noexcept;
  [[nodiscard]] bool controls_match(std::uint32_t view_generation,
                                    std::uint32_t device_generation) const noexcept;
  [[nodiscard]] bool admission_token_matches(const AdmissionToken& token) const noexcept;
  [[nodiscard]] bool update_tuple_matches(std::uint32_t tag) const noexcept;
  [[nodiscard]] bool range_record_matches(const RangeRecord& range,
                                          const RangeToken& token) const noexcept;
  [[nodiscard]] bool publication_range_matches() const noexcept;
  [[nodiscard]] RangeRecord* initializing_range(std::uint32_t tag) noexcept;
  [[nodiscard]] bool archive_retired_range(const RangeRecord& range) noexcept;
  [[nodiscard]] bool retire_range() noexcept;
  [[nodiscard]] bool compensate() noexcept;
  void quarantine() noexcept;
  void settle_lease_for_close() noexcept;

  Limits limits_{};
  bool valid_ = false;
  ViewId view_id_{};
  ViewState view_state_ = ViewState::Open;
  DeviceState device_state_ = DeviceState::Open;
  std::uint32_t view_generation_ = 1;
  std::uint32_t device_generation_ = 1;
  std::uint32_t range_token_generation_ = 1;
  std::uint32_t range_tag_high_water_ = 0;
  std::uint32_t device_update_tag_ = 0;
  std::uint64_t epoch_ = 1;
  std::uint64_t allocation_high_water_ = 0;
  std::uint64_t publication_cursor_ = 0;
  bool intermediate_visible_ = false;
  std::uint32_t committed_admissions_ = 0;
  std::uint32_t compensation_count_ = 0;
  AttemptRecord attempt_{};
  LeaseRecord lease_{};
  UpdateRecord update_{};
  RangeRecord range_{};
  RangeRecord following_range_{};
  std::array<RangeRecord, 2> retired_ranges_{};
  std::uint32_t retired_range_count_ = 0;
  PublishRecord publication_{};
};

} // namespace metaflux::runtime::recovery

#endif
