#ifndef METAFLUX_RUNTIME_LIFECYCLE_HPP
#define METAFLUX_RUNTIME_LIFECYCLE_HPP

#include <array>
#include <cstdint>
#include <limits>
#include <mutex>
#include <string_view>

namespace metaflux::runtime::lifecycle {

enum class State : std::uint8_t {
  Absent = 0,
  Present = 1,
  Online = 2,
  Quiescing = 3,
  Draining = 4,
  Resetting = 5,
  Lost = 6,
};

enum class Operation : std::uint8_t {
  Add = 1,
  Remove = 2,
  Reset = 3,
  TransportLoss = 4,
  Recover = 5,
};

enum class Source : std::uint8_t {
  Admin = 1,
  Memfd = 2,
  Cdev = 3,
  VfioUser = 4,
  Qmp = 5,
  Disconnect = 6,
  Restart = 7,
};

enum class Result : std::uint8_t {
  Accepted = 0,
  Duplicate = 1,
  Stale = 2,
  Conflict = 3,
  Invalid = 4,
  ResourceExhausted = 5,
  CallbackRejected = 6,
  DeviceLost = 7,
  Timeout = 8,
};

enum class ResolveResult : std::uint8_t {
  Online = 0,
  DeviceLost = 1,
  Absent = 2,
  Unknown = 3,
};

enum class MirrorKind : std::uint8_t {
  Memfd = 1,
  Cdev = 2,
  VfioUser = 3,
};

enum class MirrorStage : std::uint8_t {
  Prepare = 1,
  Quiesce = 2,
  Drain = 3,
  Commit = 4,
  Abort = 5,
  PublishLost = 6,
};

struct Request final {
  std::uint64_t request_id = 0;
  std::uint64_t logical_device_id = 0;
  std::uint64_t daemon_incarnation = 0;
  std::uint64_t expected_identity_record_id = 0;
  std::uint64_t expected_generation = 0;
  std::uint64_t expected_epoch = 0;
  std::uint64_t deadline_tick = 0;
  Source source = Source::Admin;
  Operation operation = Operation::Add;
};

struct Candidate final {
  std::uint64_t generation = 0;
  std::uint64_t identity_record_id = 0;
  std::uint64_t epoch = 0;
};

struct Snapshot final {
  State state = State::Absent;
  std::uint64_t logical_device_id = 0;
  std::uint64_t identity_record_id = 0;
  std::uint64_t generation = 0;
  std::uint64_t epoch = 0;
  std::uint64_t generation_high_water = 0;
  std::uint64_t identity_high_water = 0;
  std::uint64_t daemon_incarnation = 0;
};

struct MirrorEvent final {
  Request request{};
  MirrorStage stage = MirrorStage::Prepare;
  State state_before = State::Absent;
  State state_after = State::Absent;
  std::uint64_t old_generation = 0;
  std::uint64_t old_identity_record_id = 0;
  std::uint64_t old_epoch = 0;
  Candidate candidate{};
};

using MirrorCallback = bool (*)(void* context, const MirrorEvent& event) noexcept;
using MirrorLostCallback = void (*)(void* context, const MirrorEvent& event) noexcept;
using Clock = std::uint64_t (*)() noexcept;

struct Mirror final {
  MirrorKind kind = MirrorKind::Memfd;
  std::string_view name{};
  void* context = nullptr;
  MirrorCallback prepare = nullptr;
  MirrorCallback quiesce = nullptr;
  MirrorCallback drain = nullptr;
  MirrorCallback commit = nullptr;
  MirrorCallback abort = nullptr;
  MirrorLostCallback publish_lost = nullptr;
};

struct Config final {
  std::uint64_t logical_device_id = 1;
  std::uint64_t daemon_incarnation = 1;
  std::uint64_t initial_identity_record_id = 1;
  std::uint64_t initial_generation = 1;
  std::uint64_t initial_epoch = 1;
  State initial_state = State::Online;
  std::uint64_t identity_record_terminal = std::numeric_limits<std::uint64_t>::max();
  std::uint64_t generation_terminal = std::numeric_limits<std::uint64_t>::max();
  std::uint64_t epoch_terminal = std::numeric_limits<std::uint64_t>::max();
  Clock clock = nullptr;
};

struct ResultDetails final {
  Result result = Result::Invalid;
  Snapshot snapshot{};
  std::uint64_t candidate_generation = 0;
  std::uint64_t candidate_identity_record_id = 0;
};

class Coordinator final {
public:
  static constexpr std::uint32_t kMaxMirrors = 3;
  // work-item-0.1.2.3's 1,000 reset/remove/add envelope retains three request records and
  // two immutable retirement tombstones per cycle. Records are never evicted
  // because replay and stale-object resolution are authority semantics.
  static constexpr std::uint32_t kRequestCapacity = 4096;
  static constexpr std::uint32_t kTombstoneCapacity = 2048;

  explicit Coordinator(Config config = {}) noexcept;
  Coordinator(const Coordinator&) = delete;
  Coordinator& operator=(const Coordinator&) = delete;
  Coordinator(Coordinator&& other) noexcept;
  Coordinator& operator=(Coordinator&& other) = delete;

  [[nodiscard]] bool valid() const noexcept;

  [[nodiscard]] bool register_mirror(const Mirror& mirror) noexcept;

  // Remove a mirror before its owner is destroyed. The coordinator serializes
  // removal with lifecycle submissions so no transaction can retain a stale
  // callback context after this returns.
  [[nodiscard]] bool unregister_mirror(MirrorKind kind, void* context) noexcept;

  [[nodiscard]] Result apply(const Request& request, ResultDetails& out) noexcept;

  [[nodiscard]] Result apply(const Request& request) noexcept;

  [[nodiscard]] Snapshot snapshot() const noexcept;

  [[nodiscard]] ResolveResult resolve(std::uint64_t generation) const noexcept;

  [[nodiscard]] bool generation_consumed(std::uint64_t generation) const noexcept;

  [[nodiscard]] std::uint32_t mirror_count() const noexcept;

  [[nodiscard]] std::uint32_t tombstone_count() const noexcept;

private:
  struct RequestRecord final {
    bool used = false;
    Request request{};
    Result result = Result::Invalid;
    std::uint64_t candidate_generation = 0;
    std::uint64_t candidate_identity_record_id = 0;
  };

  struct Tombstone final {
    std::uint64_t generation = 0;
    std::uint64_t identity_record_id = 0;
    std::uint64_t epoch = 0;
  };

  struct Transaction final {
    Request request{};
    Candidate candidate{};
    State target_state = State::Absent;
    State old_state = State::Absent;
    std::uint64_t old_generation = 0;
    std::uint64_t old_identity_record_id = 0;
    std::uint64_t old_epoch = 0;
    bool has_candidate = false;
    std::uint32_t touched_mask = 0;
    std::uint32_t committed_mask = 0;
    std::uint32_t committed_count = 0;
  };

  [[nodiscard]] bool valid_request(const Request& request) const noexcept;
  [[nodiscard]] bool same_request(const Request& left, const Request& right) const noexcept;
  [[nodiscard]] RequestRecord* find_request(std::uint64_t request_id) noexcept;
  [[nodiscard]] const RequestRecord* find_request(std::uint64_t request_id) const noexcept;
  [[nodiscard]] RequestRecord* free_request() noexcept;
  void remember_request(const Request& request, Result result,
                        const ResultDetails& details) noexcept;

  [[nodiscard]] bool reserve_candidate(Candidate& out) noexcept;
  [[nodiscard]] bool reserve_epoch(std::uint64_t& out_epoch) const noexcept;
  [[nodiscard]] bool reserve_tombstone() const noexcept;
  void append_tombstone(std::uint64_t generation, std::uint64_t identity_record_id,
                        std::uint64_t epoch) noexcept;

  [[nodiscard]] bool invoke(Transaction& transaction, MirrorStage stage,
                            MirrorCallback callback) noexcept;
  [[nodiscard]] State event_state(const Transaction& transaction, MirrorStage stage) const noexcept;
  void abort(Transaction& transaction) noexcept;
  void publish_lost(const Transaction& transaction) noexcept;
  void fill_details(Result result, const Transaction& transaction,
                    ResultDetails& out) const noexcept;

  [[nodiscard]] Result apply_add(const Request& request, ResultDetails& out) noexcept;
  [[nodiscard]] Result apply_remove(const Request& request, ResultDetails& out) noexcept;
  [[nodiscard]] Result apply_reset_or_recover(const Request& request, ResultDetails& out) noexcept;
  [[nodiscard]] Result apply_transport_loss(const Request& request, ResultDetails& out) noexcept;

  Config config_{};
  bool valid_ = false;
  State state_ = State::Absent;
  std::uint64_t logical_device_id_ = 0;
  std::uint64_t daemon_incarnation_ = 0;
  std::uint64_t identity_record_id_ = 0;
  std::uint64_t generation_ = 0;
  std::uint64_t epoch_ = 0;
  std::uint64_t identity_high_water_ = 0;
  std::uint64_t generation_high_water_ = 0;
  std::array<Mirror, kMaxMirrors> mirrors_{};
  std::uint32_t mirror_count_ = 0;
  std::array<RequestRecord, kRequestCapacity> requests_{};
  std::array<Tombstone, kTombstoneCapacity> tombstones_{};
  std::uint32_t tombstone_count_ = 0;
  mutable std::recursive_mutex mutex_;
};

} // namespace metaflux::runtime::lifecycle

#endif
