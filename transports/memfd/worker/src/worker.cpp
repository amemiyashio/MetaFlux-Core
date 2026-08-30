#include "metaflux/transport/memfd_worker.hpp"

namespace metaflux::transport::memfd {
namespace {

using metaflux::runtime::lifecycle::MirrorEvent;
using metaflux::runtime::lifecycle::Operation;
using metaflux::runtime::lifecycle::State;

} // namespace

MemfdWorker::MemfdWorker(std::uint64_t identity_record_id, std::uint64_t generation,
                         std::uint64_t epoch) noexcept
    : identity_record_id_(identity_record_id), generation_(generation), epoch_(epoch),
      online_(identity_record_id != 0U && generation != 0U),
      accepting_(online_) {}

SubmitResult MemfdWorker::submit(std::uint64_t request_generation) noexcept {
  if (!online_) {
    return SubmitResult::DeviceLost;
  }
  if (request_generation != generation_) {
    return SubmitResult::StaleGeneration;
  }
  if (!accepting_) {
    return SubmitResult::Quiescing;
  }
  if (in_flight_ == UINT32_MAX) {
    return SubmitResult::Quiescing;
  }
  ++in_flight_;
  return SubmitResult::Accepted;
}

bool MemfdWorker::complete_one() noexcept {
  if (in_flight_ == 0U) {
    return false;
  }
  --in_flight_;
  return true;
}

bool MemfdWorker::validate_candidate(const MirrorEvent& event) const noexcept {
  if (event.old_identity_record_id != identity_record_id_ ||
      event.old_generation != generation_ || event.old_epoch != epoch_) {
    return false;
  }

  const auto& candidate = event.candidate;
  switch (event.request.operation) {
  case Operation::Add:
    return event.state_before == State::Absent && !online_ && identity_record_id_ == 0U &&
           generation_ == 0U && candidate.identity_record_id != 0U && candidate.generation != 0U &&
           candidate.epoch == epoch_;
  case Operation::Remove:
    return (event.state_before == State::Online || event.state_before == State::Lost) &&
           generation_ != 0U && candidate.identity_record_id == 0U && candidate.generation == 0U &&
           candidate.epoch > epoch_;
  case Operation::Reset:
    return event.state_before == State::Online && online_ && candidate.identity_record_id != 0U &&
           candidate.generation > generation_ && candidate.epoch > epoch_;
  case Operation::Recover:
    return event.state_before == State::Lost && !online_ && generation_ != 0U &&
           candidate.identity_record_id != 0U && candidate.generation > generation_ &&
           candidate.epoch > epoch_;
  case Operation::TransportLoss:
    break;
  }
  return false;
}

void MemfdWorker::clear_transaction() noexcept {
  transaction_active_ = false;
  transaction_quiesced_ = false;
  transaction_drained_ = false;
  staged_candidate_ = {};
  staged_operation_ = Operation::Add;
}

bool MemfdWorker::lifecycle_prepare(void* context, const MirrorEvent& event) noexcept {
  auto* worker = static_cast<MemfdWorker*>(context);
  if (worker == nullptr || worker->transaction_active_ || !worker->validate_candidate(event)) {
    return false;
  }
  worker->previous_identity_record_id_ = worker->identity_record_id_;
  worker->previous_generation_ = worker->generation_;
  worker->previous_epoch_ = worker->epoch_;
  worker->previous_online_ = worker->online_;
  worker->previous_accepting_ = worker->accepting_;
  worker->staged_candidate_ = event.candidate;
  worker->staged_operation_ = event.request.operation;
  worker->transaction_active_ = true;
  return true;
}

bool MemfdWorker::lifecycle_quiesce(void* context, const MirrorEvent&) noexcept {
  auto* worker = static_cast<MemfdWorker*>(context);
  if (worker == nullptr || !worker->transaction_active_ || worker->transaction_quiesced_) {
    return false;
  }
  worker->accepting_ = false;
  worker->transaction_quiesced_ = true;
  return true;
}

bool MemfdWorker::lifecycle_drain(void* context, const MirrorEvent&) noexcept {
  auto* worker = static_cast<MemfdWorker*>(context);
  if (worker == nullptr || !worker->transaction_active_ || !worker->transaction_quiesced_ ||
      worker->transaction_drained_) {
    return false;
  }
  if (worker->in_flight_ != 0U) {
    return false;
  }
  worker->transaction_drained_ = true;
  return true;
}

bool MemfdWorker::lifecycle_commit(void* context, const MirrorEvent& event) noexcept {
  auto* worker = static_cast<MemfdWorker*>(context);
  if (worker == nullptr || !worker->transaction_active_ || !worker->transaction_drained_ ||
      event.request.operation != worker->staged_operation_ ||
      event.candidate.generation != worker->staged_candidate_.generation ||
      event.candidate.identity_record_id != worker->staged_candidate_.identity_record_id ||
      event.candidate.epoch != worker->staged_candidate_.epoch) {
    return false;
  }
  if (event.state_after == State::Absent) {
    if (worker->staged_operation_ != Operation::Remove ||
        worker->staged_candidate_.generation != 0U || worker->staged_candidate_.identity_record_id != 0U) {
      return false;
    }
    worker->identity_record_id_ = 0U;
    worker->generation_ = 0U;
    worker->epoch_ = worker->staged_candidate_.epoch;
    worker->online_ = false;
    worker->accepting_ = false;
  } else {
    if (worker->staged_candidate_.generation == 0U ||
        worker->staged_candidate_.identity_record_id == 0U) {
      return false;
    }
    worker->identity_record_id_ = worker->staged_candidate_.identity_record_id;
    worker->generation_ = worker->staged_candidate_.generation;
    worker->epoch_ = worker->staged_candidate_.epoch;
    worker->online_ = event.state_after == State::Online;
    worker->accepting_ = worker->online_;
  }
  worker->clear_transaction();
  return true;
}

bool MemfdWorker::lifecycle_abort(void* context, const MirrorEvent&) noexcept {
  auto* worker = static_cast<MemfdWorker*>(context);
  if (worker == nullptr) {
    return false;
  }
  if (worker->transaction_active_) {
    worker->identity_record_id_ = worker->previous_identity_record_id_;
    worker->generation_ = worker->previous_generation_;
    worker->epoch_ = worker->previous_epoch_;
    worker->online_ = worker->previous_online_;
    worker->accepting_ = worker->previous_accepting_;
    worker->clear_transaction();
  }
  return true;
}

void MemfdWorker::lifecycle_lost(void* context, const MirrorEvent&) noexcept {
  auto* worker = static_cast<MemfdWorker*>(context);
  if (worker != nullptr) {
    worker->online_ = false;
    worker->accepting_ = false;
  }
}

bool MemfdWorker::attach_lifecycle(
    metaflux::runtime::lifecycle::Coordinator& coordinator) noexcept {
  return coordinator.register_mirror(lifecycle_mirror());
}

metaflux::runtime::lifecycle::Mirror MemfdWorker::lifecycle_mirror() noexcept {
  return metaflux::runtime::lifecycle::Mirror{
      .kind = metaflux::runtime::lifecycle::MirrorKind::Memfd,
      .name = "memfd",
      .context = this,
      .prepare = lifecycle_prepare,
      .quiesce = lifecycle_quiesce,
      .drain = lifecycle_drain,
      .commit = lifecycle_commit,
      .abort = lifecycle_abort,
      .publish_lost = lifecycle_lost,
  };
}

} // namespace metaflux::transport::memfd
