#include <metaflux/transport/msix.hpp>

#include <algorithm>
#include <limits>

namespace metaflux::transport::vfio_user {

MsixStatus MsixNotificationLedger::configure(std::uint64_t generation,
                                             MsixInjectCallback inject,
                                             void* context) noexcept {
  if (generation == 0U) {
    return MsixStatus::invalid_argument;
  }
  generation_ = generation;
  inject_ = inject;
  context_ = context;
  online_ = true;
  vectors_ = {};
  return MsixStatus::success;
}

MsixStatus MsixNotificationLedger::check(std::uint64_t generation,
                                         std::uint32_t vector) const noexcept {
  if (vector >= kVectorCount || generation == 0U) {
    return MsixStatus::invalid_argument;
  }
  if (generation != generation_) {
    return MsixStatus::stale_generation;
  }
  return online_ ? MsixStatus::success : MsixStatus::device_lost;
}

MsixStatus MsixNotificationLedger::set_mask(std::uint64_t generation,
                                            std::uint32_t vector, bool masked) noexcept {
  const auto status = check(generation, vector);
  if (status != MsixStatus::success) {
    return status;
  }
  auto& state = vectors_[vector];
  state.masked = masked;
  if (masked || !state.pending) {
    return MsixStatus::success;
  }
  return deliver(vector);
}

MsixStatus MsixNotificationLedger::arm_completion(std::uint64_t generation,
                                                  std::uint64_t timeline) noexcept {
  const auto status = check(generation, kCompletionVector);
  if (status != MsixStatus::success || timeline == 0U) {
    return status == MsixStatus::success ? MsixStatus::invalid_argument : status;
  }
  auto& state = vectors_[kCompletionVector];
  if (timeline <= state.last_delivered_timeline ||
      (state.armed && timeline <= state.armed_timeline)) {
    return MsixStatus::already_armed;
  }
  state.armed = true;
  state.armed_timeline = timeline;
  return MsixStatus::success;
}

MsixStatus MsixNotificationLedger::notify(std::uint64_t generation, std::uint32_t vector,
                                          std::uint64_t timeline) noexcept {
  const auto status = check(generation, vector);
  if (status != MsixStatus::success || timeline == 0U) {
    return status == MsixStatus::success ? MsixStatus::invalid_argument : status;
  }
  auto& state = vectors_[vector];
  if (vector == kCompletionVector) {
    if (!state.armed || timeline < state.armed_timeline) {
      return MsixStatus::disarmed;
    }
  }
  state.pending = true;
  state.pending_timeline = std::max(state.pending_timeline, timeline);
  if (state.masked) {
    return MsixStatus::masked;
  }
  return deliver(vector);
}

MsixStatus MsixNotificationLedger::retry_pending(std::uint64_t generation,
                                                 std::uint32_t vector) noexcept {
  const auto status = check(generation, vector);
  if (status != MsixStatus::success) {
    return status;
  }
  return vectors_[vector].pending ? deliver(vector) : MsixStatus::no_pending;
}

MsixStatus MsixNotificationLedger::deliver(std::uint32_t vector) noexcept {
  auto& state = vectors_[vector];
  if (!state.pending) {
    return MsixStatus::no_pending;
  }
  if (state.masked) {
    return MsixStatus::masked;
  }
  if (inject_ == nullptr || !inject_(context_, vector)) {
    return MsixStatus::injection_failed;
  }
  state.last_delivered_timeline =
      std::max(state.last_delivered_timeline, state.pending_timeline);
  if (state.delivered_count != std::numeric_limits<std::uint64_t>::max()) {
    ++state.delivered_count;
  }
  state.pending = false;
  state.pending_timeline = 0U;
  if (vector == kCompletionVector) {
    state.armed = false;
    state.armed_timeline = 0U;
  }
  return MsixStatus::success;
}

MsixStatus MsixNotificationLedger::mark_lost(std::uint64_t generation) noexcept {
  if (generation == 0U) {
    return MsixStatus::invalid_argument;
  }
  if (generation != generation_) {
    return MsixStatus::stale_generation;
  }
  online_ = false;
  return MsixStatus::success;
}

MsixStatus MsixNotificationLedger::snapshot(std::uint64_t generation, std::uint32_t vector,
                                            MsixVectorState* out_state) const noexcept {
  if (out_state == nullptr) {
    return MsixStatus::invalid_argument;
  }
  const auto status = check(generation, vector);
  if (status != MsixStatus::success) {
    return status;
  }
  *out_state = vectors_[vector];
  return MsixStatus::success;
}

const char* msix_status_string(MsixStatus status) noexcept {
  switch (status) {
  case MsixStatus::success:
    return "success";
  case MsixStatus::invalid_argument:
    return "invalid-argument";
  case MsixStatus::stale_generation:
    return "stale-generation";
  case MsixStatus::device_lost:
    return "device-lost";
  case MsixStatus::masked:
    return "masked";
  case MsixStatus::disarmed:
    return "disarmed";
  case MsixStatus::injection_failed:
    return "injection-failed";
  case MsixStatus::no_pending:
    return "no-pending";
  case MsixStatus::already_armed:
    return "already-armed";
  }
  return "unknown";
}

} // namespace metaflux::transport::vfio_user
