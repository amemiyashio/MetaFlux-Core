#include "metaflux/runtime/lifecycle.hpp"

#include <limits>

namespace metaflux::runtime::lifecycle {
namespace {

[[nodiscard]] bool is_valid_source(Source source) noexcept {
  const auto value = static_cast<std::uint8_t>(source);
  return value >= static_cast<std::uint8_t>(Source::Admin) &&
         value <= static_cast<std::uint8_t>(Source::Restart);
}

[[nodiscard]] bool is_valid_operation(Operation operation) noexcept {
  const auto value = static_cast<std::uint8_t>(operation);
  return value >= static_cast<std::uint8_t>(Operation::Add) &&
         value <= static_cast<std::uint8_t>(Operation::Recover);
}

[[nodiscard]] bool is_valid_mirror_kind(MirrorKind kind) noexcept {
  const auto value = static_cast<std::uint8_t>(kind);
  return value >= static_cast<std::uint8_t>(MirrorKind::Memfd) &&
         value <= static_cast<std::uint8_t>(MirrorKind::VfioUser);
}

} // namespace

Coordinator::Coordinator(Config config) noexcept : config_(config) {
  logical_device_id_ = config_.logical_device_id;
  daemon_incarnation_ = config_.daemon_incarnation;
  identity_record_id_ = config_.initial_identity_record_id;
  generation_ = config_.initial_generation;
  epoch_ = config_.initial_epoch;
  identity_high_water_ = config_.initial_identity_record_id;
  generation_high_water_ = config_.initial_generation;
  state_ = config_.initial_state;

  const bool terminals_valid = config_.identity_record_terminal > 1U &&
                               config_.generation_terminal > 1U && config_.epoch_terminal > 1U;
  const bool initial_values_valid = logical_device_id_ != 0U && daemon_incarnation_ != 0U &&
                                    epoch_ != 0U && epoch_ < config_.epoch_terminal;
  const bool state_values_valid =
      (state_ == State::Absent && generation_ == 0U && identity_record_id_ == 0U) ||
      (state_ != State::Absent && generation_ != 0U && generation_ < config_.generation_terminal &&
       identity_record_id_ != 0U && identity_record_id_ < config_.identity_record_terminal);
  const bool high_water_valid = generation_high_water_ < config_.generation_terminal &&
                                identity_high_water_ < config_.identity_record_terminal &&
                                generation_high_water_ >= generation_ &&
                                identity_high_water_ >= identity_record_id_;
  valid_ = terminals_valid && initial_values_valid && state_values_valid && high_water_valid;
}

Coordinator::Coordinator(Coordinator&& other) noexcept {
  std::lock_guard<std::recursive_mutex> lock(other.mutex_);
  config_ = other.config_;
  valid_ = other.valid_;
  state_ = other.state_;
  logical_device_id_ = other.logical_device_id_;
  daemon_incarnation_ = other.daemon_incarnation_;
  identity_record_id_ = other.identity_record_id_;
  generation_ = other.generation_;
  epoch_ = other.epoch_;
  identity_high_water_ = other.identity_high_water_;
  generation_high_water_ = other.generation_high_water_;
  mirrors_ = other.mirrors_;
  mirror_count_ = other.mirror_count_;
  requests_ = other.requests_;
  tombstones_ = other.tombstones_;
  tombstone_count_ = other.tombstone_count_;
  other.valid_ = false;
  other.mirror_count_ = 0;
  other.tombstone_count_ = 0;
}

State Coordinator::event_state(const Transaction& transaction, MirrorStage stage) const noexcept {
  if (stage == MirrorStage::Abort) {
    return transaction.target_state;
  }
  if (stage == MirrorStage::PublishLost) {
    return State::Lost;
  }
  if (transaction.request.operation == Operation::Add) {
    switch (stage) {
    case MirrorStage::Prepare:
      return State::Present;
    case MirrorStage::Quiesce:
      return State::Quiescing;
    case MirrorStage::Drain:
      return State::Draining;
    case MirrorStage::Commit:
      return State::Online;
    default:
      break;
    }
  }
  if (transaction.request.operation == Operation::Remove) {
    switch (stage) {
    case MirrorStage::Prepare:
      return State::Quiescing;
    case MirrorStage::Quiesce:
      return State::Draining;
    case MirrorStage::Drain:
    case MirrorStage::Commit:
      return State::Absent;
    default:
      break;
    }
  }
  if (transaction.request.operation == Operation::Reset ||
      transaction.request.operation == Operation::Recover) {
    switch (stage) {
    case MirrorStage::Prepare:
      return State::Quiescing;
    case MirrorStage::Quiesce:
      return State::Draining;
    case MirrorStage::Drain:
      return State::Resetting;
    case MirrorStage::Commit:
      return State::Online;
    default:
      break;
    }
  }
  return transaction.target_state;
}

bool Coordinator::register_mirror(const Mirror& mirror) noexcept {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  if (!valid_ || mirror_count_ >= kMaxMirrors || !is_valid_mirror_kind(mirror.kind) ||
      mirror.name.empty()) {
    return false;
  }
  for (std::uint32_t index = 0; index < mirror_count_; ++index) {
    if (mirrors_[index].kind == mirror.kind) {
      return false;
    }
  }
  mirrors_[mirror_count_] = mirror;
  ++mirror_count_;
  return true;
}

bool Coordinator::unregister_mirror(MirrorKind kind, void* context) noexcept {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  if (!valid_ || context == nullptr || !is_valid_mirror_kind(kind)) {
    return false;
  }
  for (std::uint32_t index = 0; index < mirror_count_; ++index) {
    if (mirrors_[index].kind != kind || mirrors_[index].context != context) {
      continue;
    }
    const std::uint32_t last = mirror_count_ - 1U;
    mirrors_[index] = mirrors_[last];
    mirrors_[last] = {};
    --mirror_count_;
    return true;
  }
  return false;
}

bool Coordinator::same_request(const Request& left, const Request& right) const noexcept {
  return left.request_id == right.request_id && left.logical_device_id == right.logical_device_id &&
         left.daemon_incarnation == right.daemon_incarnation &&
         left.expected_identity_record_id == right.expected_identity_record_id &&
         left.expected_generation == right.expected_generation &&
         left.expected_epoch == right.expected_epoch && left.deadline_tick == right.deadline_tick &&
         left.source == right.source && left.operation == right.operation;
}

bool Coordinator::valid_request(const Request& request) const noexcept {
  if (request.request_id == 0U || request.logical_device_id != logical_device_id_ ||
      request.daemon_incarnation != daemon_incarnation_ || !is_valid_source(request.source) ||
      !is_valid_operation(request.operation)) {
    return false;
  }
  if (config_.clock != nullptr && request.deadline_tick != 0U &&
      config_.clock() >= request.deadline_tick) {
    return false;
  }
  return request.expected_epoch == epoch_ &&
         request.expected_identity_record_id == identity_record_id_ &&
         request.expected_generation == generation_;
}

Coordinator::RequestRecord* Coordinator::find_request(std::uint64_t request_id) noexcept {
  for (RequestRecord& record : requests_) {
    if (record.used && record.request.request_id == request_id) {
      return &record;
    }
  }
  return nullptr;
}

const Coordinator::RequestRecord*
Coordinator::find_request(std::uint64_t request_id) const noexcept {
  for (const RequestRecord& record : requests_) {
    if (record.used && record.request.request_id == request_id) {
      return &record;
    }
  }
  return nullptr;
}

Coordinator::RequestRecord* Coordinator::free_request() noexcept {
  for (RequestRecord& record : requests_) {
    if (!record.used) {
      return &record;
    }
  }
  return nullptr;
}

void Coordinator::remember_request(const Request& request, Result result,
                                   const ResultDetails& details) noexcept {
  RequestRecord* record = free_request();
  if (record == nullptr) {
    return;
  }
  record->used = true;
  record->request = request;
  record->result = result;
  record->candidate_generation = details.candidate_generation;
  record->candidate_identity_record_id = details.candidate_identity_record_id;
}

bool Coordinator::reserve_candidate(Candidate& out) noexcept {
  if (generation_high_water_ >= config_.generation_terminal - 1U ||
      identity_high_water_ >= config_.identity_record_terminal - 1U) {
    return false;
  }
  const std::uint64_t next_generation = generation_high_water_ + 1U;
  const std::uint64_t next_identity = identity_high_water_ + 1U;
  generation_high_water_ = next_generation;
  identity_high_water_ = next_identity;
  out.generation = next_generation;
  out.identity_record_id = next_identity;
  out.epoch = epoch_;
  return true;
}

bool Coordinator::reserve_epoch(std::uint64_t& out_epoch) const noexcept {
  if (epoch_ >= config_.epoch_terminal - 1U) {
    return false;
  }
  out_epoch = epoch_ + 1U;
  return true;
}

bool Coordinator::reserve_tombstone() const noexcept {
  return tombstone_count_ < kTombstoneCapacity;
}

void Coordinator::append_tombstone(std::uint64_t generation, std::uint64_t identity_record_id,
                                   std::uint64_t epoch) noexcept {
  if (tombstone_count_ >= kTombstoneCapacity) {
    return;
  }
  tombstones_[tombstone_count_] = Tombstone{
      .generation = generation,
      .identity_record_id = identity_record_id,
      .epoch = epoch,
  };
  ++tombstone_count_;
}

bool Coordinator::invoke(Transaction& transaction, MirrorStage stage,
                         MirrorCallback callback) noexcept {
  bool success = true;
  for (std::uint32_t index = 0; index < mirror_count_; ++index) {
    if (transaction.request.deadline_tick != 0U && config_.clock != nullptr &&
        config_.clock() >= transaction.request.deadline_tick) {
      transaction.timed_out = true;
      return false;
    }
    Mirror& mirror = mirrors_[index];
    MirrorCallback selected = callback;
    if (stage == MirrorStage::Prepare) {
      selected = mirror.prepare;
    } else if (stage == MirrorStage::Quiesce) {
      selected = mirror.quiesce;
    } else if (stage == MirrorStage::Drain) {
      selected = mirror.drain;
    } else if (stage == MirrorStage::Commit) {
      selected = mirror.commit;
    } else if (stage == MirrorStage::Abort) {
      selected = mirror.abort;
    }
    if (selected == nullptr) {
      continue;
    }
    transaction.touched_mask |= (UINT32_C(1) << index);
    const MirrorEvent event{
        .request = transaction.request,
        .stage = stage,
        .state_before = transaction.old_state,
        .state_after = event_state(transaction, stage),
        .old_generation = transaction.old_generation,
        .old_identity_record_id = transaction.old_identity_record_id,
        .old_epoch = transaction.old_epoch,
        .candidate = transaction.candidate,
    };
    if (!selected(mirror.context, event)) {
      success = false;
      break;
    }
    if (stage == MirrorStage::Commit) {
      transaction.committed_mask |= (UINT32_C(1) << index);
      ++transaction.committed_count;
    }
  }
  return success;
}

void Coordinator::abort(Transaction& transaction) noexcept {
  for (std::uint32_t index = 0; index < mirror_count_; ++index) {
    const std::uint32_t bit = UINT32_C(1) << index;
    if ((transaction.touched_mask & bit) == 0U || (transaction.committed_mask & bit) != 0U) {
      continue;
    }
    Mirror& mirror = mirrors_[index];
    if (mirror.abort == nullptr) {
      continue;
    }
    const MirrorEvent event{
        .request = transaction.request,
        .stage = MirrorStage::Abort,
        .state_before = transaction.old_state,
        .state_after = event_state(transaction, MirrorStage::Abort),
        .old_generation = transaction.old_generation,
        .old_identity_record_id = transaction.old_identity_record_id,
        .old_epoch = transaction.old_epoch,
        .candidate = transaction.candidate,
    };
    mirror.abort(mirror.context, event);
  }
}

void Coordinator::publish_lost(const Transaction& transaction) noexcept {
  for (std::uint32_t index = 0; index < mirror_count_; ++index) {
    Mirror& mirror = mirrors_[index];
    if (mirror.publish_lost == nullptr) {
      continue;
    }
    const MirrorEvent event{
        .request = transaction.request,
        .stage = MirrorStage::PublishLost,
        .state_before = transaction.old_state,
        .state_after = event_state(transaction, MirrorStage::PublishLost),
        .old_generation = transaction.old_generation,
        .old_identity_record_id = transaction.old_identity_record_id,
        .old_epoch = transaction.old_epoch,
        .candidate = transaction.candidate,
    };
    mirror.publish_lost(mirror.context, event);
  }
}

void Coordinator::fill_details(Result result, const Transaction& transaction,
                               ResultDetails& out) const noexcept {
  out.result = result;
  out.snapshot = snapshot();
  out.candidate_generation = transaction.candidate.generation;
  out.candidate_identity_record_id = transaction.candidate.identity_record_id;
}

Result Coordinator::apply_add(const Request& request, ResultDetails& out) noexcept {
  if (state_ != State::Absent || generation_ != 0U || identity_record_id_ != 0U) {
    out.result = Result::Invalid;
    out.snapshot = snapshot();
    return out.result;
  }
  Candidate candidate{};
  if (!reserve_candidate(candidate)) {
    out.result = Result::ResourceExhausted;
    out.snapshot = snapshot();
    return out.result;
  }

  Transaction transaction{
      .request = request,
      .candidate = candidate,
      .target_state = State::Online,
      .old_state = state_,
      .old_generation = generation_,
      .old_identity_record_id = identity_record_id_,
      .old_epoch = epoch_,
      .has_candidate = true,
  };
  if (!invoke(transaction, MirrorStage::Prepare, nullptr) ||
      !invoke(transaction, MirrorStage::Quiesce, nullptr) ||
      !invoke(transaction, MirrorStage::Drain, nullptr)) {
    abort(transaction);
    fill_details(transaction.timed_out ? Result::Timeout : Result::CallbackRejected, transaction,
                 out);
    return out.result;
  }
  if (!invoke(transaction, MirrorStage::Commit, nullptr)) {
    if (transaction.committed_count == 0U) {
      abort(transaction);
      fill_details(transaction.timed_out ? Result::Timeout : Result::CallbackRejected, transaction,
                   out);
      return out.result;
    }
    abort(transaction);
    generation_ = candidate.generation;
    identity_record_id_ = candidate.identity_record_id;
    state_ = State::Lost;
    publish_lost(transaction);
    fill_details(transaction.timed_out ? Result::Timeout : Result::CallbackRejected, transaction,
                 out);
    return out.result;
  }
  generation_ = candidate.generation;
  identity_record_id_ = candidate.identity_record_id;
  state_ = State::Online;
  fill_details(Result::Accepted, transaction, out);
  return out.result;
}

Result Coordinator::apply_remove(const Request& request, ResultDetails& out) noexcept {
  if ((state_ != State::Online && state_ != State::Lost) || generation_ == 0U ||
      !reserve_tombstone()) {
    out.result = state_ == State::Lost ? Result::DeviceLost : Result::Invalid;
    out.snapshot = snapshot();
    return out.result;
  }
  std::uint64_t next_epoch = 0;
  if (!reserve_epoch(next_epoch)) {
    out.result = Result::ResourceExhausted;
    out.snapshot = snapshot();
    return out.result;
  }

  Transaction transaction{
      .request = request,
      .target_state = State::Absent,
      .old_state = state_,
      .old_generation = generation_,
      .old_identity_record_id = identity_record_id_,
      .old_epoch = epoch_,
  };
  transaction.candidate.epoch = next_epoch;
  if (!invoke(transaction, MirrorStage::Prepare, nullptr) ||
      !invoke(transaction, MirrorStage::Quiesce, nullptr) ||
      !invoke(transaction, MirrorStage::Drain, nullptr)) {
    abort(transaction);
    // Removal has already reserved its retirement epoch and tombstone. Once
    // accepted, authority must not leave a live identity behind because a
    // mirror could not finish local cleanup; keep the mirror fenced and
    // settle the authority as ABSENT.
    state_ = State::Lost;
    publish_lost(transaction);
    append_tombstone(generation_, identity_record_id_, next_epoch);
    epoch_ = next_epoch;
    generation_ = 0U;
    identity_record_id_ = 0U;
    state_ = State::Absent;
    fill_details(transaction.timed_out ? Result::Timeout : Result::CallbackRejected, transaction, out);
    return out.result;
  }
  if (!invoke(transaction, MirrorStage::Commit, nullptr)) {
    abort(transaction);
    publish_lost(transaction);
    append_tombstone(generation_, identity_record_id_, next_epoch);
    epoch_ = next_epoch;
    generation_ = 0U;
    identity_record_id_ = 0U;
    state_ = State::Absent;
    fill_details(transaction.timed_out ? Result::Timeout : Result::CallbackRejected, transaction,
                 out);
    return out.result;
  }
  append_tombstone(generation_, identity_record_id_, next_epoch);
  epoch_ = next_epoch;
  generation_ = 0U;
  identity_record_id_ = 0U;
  state_ = State::Absent;
  fill_details(Result::Accepted, transaction, out);
  return out.result;
}

Result Coordinator::apply_reset_or_recover(const Request& request, ResultDetails& out) noexcept {
  const bool recover = request.operation == Operation::Recover;
  const bool state_allowed = recover ? state_ == State::Lost : state_ == State::Online;
  if (!state_allowed || generation_ == 0U || !reserve_tombstone()) {
    out.result = state_ == State::Lost ? Result::DeviceLost : Result::Invalid;
    out.snapshot = snapshot();
    return out.result;
  }
  std::uint64_t next_epoch = 0;
  if (!reserve_epoch(next_epoch)) {
    out.result = Result::ResourceExhausted;
    out.snapshot = snapshot();
    return out.result;
  }
  Candidate candidate{};
  if (!reserve_candidate(candidate)) {
    out.result = Result::ResourceExhausted;
    out.snapshot = snapshot();
    return out.result;
  }
  candidate.epoch = next_epoch;

  Transaction transaction{
      .request = request,
      .candidate = candidate,
      .target_state = State::Online,
      .old_state = state_,
      .old_generation = generation_,
      .old_identity_record_id = identity_record_id_,
      .old_epoch = epoch_,
      .has_candidate = true,
  };
  if (!invoke(transaction, MirrorStage::Prepare, nullptr) ||
      !invoke(transaction, MirrorStage::Quiesce, nullptr) ||
      !invoke(transaction, MirrorStage::Drain, nullptr)) {
    abort(transaction);
    fill_details(transaction.timed_out ? Result::Timeout : Result::CallbackRejected, transaction,
                 out);
    return out.result;
  }
  if (!invoke(transaction, MirrorStage::Commit, nullptr)) {
    if (transaction.committed_count == 0U) {
      abort(transaction);
      fill_details(transaction.timed_out ? Result::Timeout : Result::CallbackRejected, transaction,
                   out);
      return out.result;
    }
    abort(transaction);
    append_tombstone(generation_, identity_record_id_, next_epoch);
    epoch_ = next_epoch;
    generation_ = candidate.generation;
    identity_record_id_ = candidate.identity_record_id;
    state_ = State::Lost;
    publish_lost(transaction);
    fill_details(transaction.timed_out ? Result::Timeout : Result::CallbackRejected, transaction,
                 out);
    return out.result;
  }
  append_tombstone(generation_, identity_record_id_, next_epoch);
  epoch_ = next_epoch;
  generation_ = candidate.generation;
  identity_record_id_ = candidate.identity_record_id;
  state_ = State::Online;
  fill_details(Result::Accepted, transaction, out);
  return out.result;
}

Result Coordinator::apply_transport_loss(const Request& request, ResultDetails& out) noexcept {
  if (state_ == State::Absent || generation_ == 0U) {
    out.result = Result::Invalid;
    out.snapshot = snapshot();
    return out.result;
  }
  if (state_ == State::Lost) {
    out.result = Result::DeviceLost;
    out.snapshot = snapshot();
    return out.result;
  }
  Transaction transaction{
      .request = request,
      .target_state = State::Lost,
      .old_state = state_,
      .old_generation = generation_,
      .old_identity_record_id = identity_record_id_,
      .old_epoch = epoch_,
  };
  state_ = State::Lost;
  publish_lost(transaction);
  fill_details(Result::Accepted, transaction, out);
  return out.result;
}

Result Coordinator::apply(const Request& request, ResultDetails& out) noexcept {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  out = ResultDetails{};
  if (!valid_) {
    out.result = Result::Invalid;
    out.snapshot = snapshot();
    return out.result;
  }
  const RequestRecord* previous = find_request(request.request_id);
  if (previous != nullptr) {
    if (!same_request(previous->request, request)) {
      out.result = Result::Conflict;
      out.snapshot = snapshot();
      return out.result;
    }
    out.result = Result::Duplicate;
    out.snapshot = snapshot();
    out.candidate_generation = previous->candidate_generation;
    out.candidate_identity_record_id = previous->candidate_identity_record_id;
    return out.result;
  }
  if (free_request() == nullptr) {
    out.result = Result::ResourceExhausted;
    out.snapshot = snapshot();
    return out.result;
  }
  if (request.deadline_tick != 0U && config_.clock != nullptr &&
      config_.clock() >= request.deadline_tick) {
    out.result = Result::Timeout;
    out.snapshot = snapshot();
    return out.result;
  }
  if (!valid_request(request)) {
    out.result = (request.daemon_incarnation != daemon_incarnation_ ||
                  request.expected_identity_record_id != identity_record_id_ ||
                  request.expected_generation != generation_ || request.expected_epoch != epoch_)
                     ? Result::Stale
                     : Result::Invalid;
    out.snapshot = snapshot();
    return out.result;
  }

  Result result = Result::Invalid;
  switch (request.operation) {
  case Operation::Add:
    result = apply_add(request, out);
    break;
  case Operation::Remove:
    result = apply_remove(request, out);
    break;
  case Operation::Reset:
  case Operation::Recover:
    result = apply_reset_or_recover(request, out);
    break;
  case Operation::TransportLoss:
    result = apply_transport_loss(request, out);
    break;
  }
  remember_request(request, result, out);
  return result;
}

Result Coordinator::apply(const Request& request) noexcept {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  ResultDetails details{};
  return apply(request, details);
}

bool Coordinator::valid() const noexcept {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  return valid_;
}

Snapshot Coordinator::snapshot() const noexcept {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  return Snapshot{
      .state = state_,
      .logical_device_id = logical_device_id_,
      .identity_record_id = identity_record_id_,
      .generation = generation_,
      .epoch = epoch_,
      .generation_high_water = generation_high_water_,
      .identity_high_water = identity_high_water_,
      .daemon_incarnation = daemon_incarnation_,
  };
}

ResolveResult Coordinator::resolve(std::uint64_t generation) const noexcept {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  if (generation == 0U) {
    return ResolveResult::Absent;
  }
  if (generation == generation_) {
    return state_ == State::Online ? ResolveResult::Online : ResolveResult::DeviceLost;
  }
  for (std::uint32_t index = 0; index < tombstone_count_; ++index) {
    if (tombstones_[index].generation == generation) {
      return ResolveResult::DeviceLost;
    }
  }
  return ResolveResult::Unknown;
}

bool Coordinator::generation_consumed(std::uint64_t generation) const noexcept {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  return generation != 0U && generation <= generation_high_water_;
}

std::uint32_t Coordinator::mirror_count() const noexcept {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  return mirror_count_;
}

std::uint32_t Coordinator::tombstone_count() const noexcept {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  return tombstone_count_;
}

} // namespace metaflux::runtime::lifecycle
