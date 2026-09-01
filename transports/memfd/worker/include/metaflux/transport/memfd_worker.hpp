#ifndef METAFLUX_TRANSPORT_MEMFD_WORKER_HPP
#define METAFLUX_TRANSPORT_MEMFD_WORKER_HPP

#include <cstdint>

#include "metaflux/runtime/lifecycle_dispatch.hpp"

namespace metaflux::transport::memfd {

enum class SubmitResult : std::uint8_t {
  Accepted = 0,
  DeviceLost = 1,
  StaleGeneration = 2,
  Quiescing = 3,
};

class MemfdWorker final {
public:
  explicit MemfdWorker(std::uint64_t identity_record_id = 1U,
                       std::uint64_t generation = 1U,
                       std::uint64_t epoch = 1U) noexcept;

  [[nodiscard]] SubmitResult submit(std::uint64_t request_generation) noexcept;
  [[nodiscard]] bool complete_one() noexcept;
  [[nodiscard]] std::uint32_t in_flight() const noexcept { return in_flight_; }

  [[nodiscard]] bool attach_lifecycle(
      metaflux::runtime::lifecycle::Coordinator& coordinator) noexcept;
  [[nodiscard]] metaflux::runtime::lifecycle::NormalizationResult report_disconnect(
      metaflux::runtime::lifecycle::Coordinator& coordinator, std::uint64_t request_id,
      std::uint64_t deadline_tick,
      metaflux::runtime::lifecycle::ResultDetails& out) noexcept;
  [[nodiscard]] metaflux::runtime::lifecycle::Mirror lifecycle_mirror() noexcept;

  [[nodiscard]] bool lifecycle_online() const noexcept { return online_; }
  [[nodiscard]] bool lifecycle_accepting() const noexcept { return accepting_; }
  [[nodiscard]] std::uint64_t identity_record_id() const noexcept { return identity_record_id_; }
  [[nodiscard]] std::uint64_t generation() const noexcept { return generation_; }
  [[nodiscard]] std::uint64_t epoch() const noexcept { return epoch_; }

private:
  static bool lifecycle_prepare(
      void* context, const metaflux::runtime::lifecycle::MirrorEvent& event) noexcept;
  static bool lifecycle_quiesce(
      void* context, const metaflux::runtime::lifecycle::MirrorEvent& event) noexcept;
  static bool lifecycle_drain(
      void* context, const metaflux::runtime::lifecycle::MirrorEvent& event) noexcept;
  static bool lifecycle_commit(
      void* context, const metaflux::runtime::lifecycle::MirrorEvent& event) noexcept;
  static bool lifecycle_abort(
      void* context, const metaflux::runtime::lifecycle::MirrorEvent& event) noexcept;
  static void lifecycle_lost(
      void* context, const metaflux::runtime::lifecycle::MirrorEvent& event) noexcept;

  [[nodiscard]] bool validate_candidate(
      const metaflux::runtime::lifecycle::MirrorEvent& event) const noexcept;
  void clear_transaction() noexcept;

  std::uint64_t identity_record_id_ = 0U;
  std::uint64_t generation_ = 0U;
  std::uint64_t epoch_ = 0U;
  std::uint32_t in_flight_ = 0U;
  bool online_ = false;
  bool accepting_ = false;
  bool transaction_active_ = false;
  bool transaction_quiesced_ = false;
  bool transaction_drained_ = false;
  bool previous_online_ = false;
  bool previous_accepting_ = false;
  std::uint64_t previous_identity_record_id_ = 0U;
  std::uint64_t previous_generation_ = 0U;
  std::uint64_t previous_epoch_ = 0U;
  metaflux::runtime::lifecycle::Candidate staged_candidate_{};
  metaflux::runtime::lifecycle::Operation staged_operation_ =
      metaflux::runtime::lifecycle::Operation::Add;
};

} // namespace metaflux::transport::memfd

#endif
