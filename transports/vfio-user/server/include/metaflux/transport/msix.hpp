#ifndef METAFLUX_TRANSPORT_MSIX_HPP
#define METAFLUX_TRANSPORT_MSIX_HPP

#include <array>
#include <cstdint>

namespace metaflux::transport::vfio_user {

using MsixInjectCallback = bool (*)(void* context, std::uint32_t vector);

enum class MsixStatus : std::uint32_t {
  success = 0U,
  invalid_argument = 1U,
  stale_generation = 2U,
  device_lost = 3U,
  masked = 4U,
  disarmed = 5U,
  injection_failed = 6U,
  no_pending = 7U,
  already_armed = 8U,
};

struct MsixVectorState final {
  bool masked = false;
  bool armed = false;
  bool pending = false;
  std::uint64_t armed_timeline = 0U;
  std::uint64_t pending_timeline = 0U;
  std::uint64_t last_delivered_timeline = 0U;
  std::uint64_t delivered_count = 0U;
};

// Models the two-vector BAR4 notification contract. Vector 0 is admin/fatal
// and vector 1 is completion-only; callbacks represent the host injection
// boundary and are never invoked while a vector is masked or disarmed.
class MsixNotificationLedger final {
public:
  static constexpr std::uint32_t kVectorCount = 2U;
  static constexpr std::uint32_t kAdminVector = 0U;
  static constexpr std::uint32_t kCompletionVector = 1U;

  MsixNotificationLedger() = default;
  explicit MsixNotificationLedger(std::uint64_t generation,
                                  MsixInjectCallback inject = nullptr,
                                  void* context = nullptr) noexcept {
    (void)configure(generation, inject, context);
  }

  [[nodiscard]] MsixStatus configure(std::uint64_t generation,
                                     MsixInjectCallback inject,
                                     void* context) noexcept;
  [[nodiscard]] MsixStatus set_mask(std::uint64_t generation, std::uint32_t vector,
                                     bool masked) noexcept;
  [[nodiscard]] MsixStatus arm_completion(std::uint64_t generation,
                                           std::uint64_t timeline) noexcept;
  [[nodiscard]] MsixStatus notify(std::uint64_t generation, std::uint32_t vector,
                                  std::uint64_t timeline) noexcept;
  [[nodiscard]] MsixStatus retry_pending(std::uint64_t generation,
                                          std::uint32_t vector) noexcept;
  [[nodiscard]] MsixStatus mark_lost(std::uint64_t generation) noexcept;
  [[nodiscard]] MsixStatus snapshot(std::uint64_t generation, std::uint32_t vector,
                                    MsixVectorState* out_state) const noexcept;
  [[nodiscard]] std::uint64_t generation() const noexcept { return generation_; }
  [[nodiscard]] bool online() const noexcept { return online_; }

private:
  [[nodiscard]] MsixStatus deliver(std::uint32_t vector) noexcept;
  [[nodiscard]] MsixStatus check(std::uint64_t generation, std::uint32_t vector) const noexcept;

  std::uint64_t generation_ = 0U;
  MsixInjectCallback inject_ = nullptr;
  void* context_ = nullptr;
  bool online_ = false;
  std::array<MsixVectorState, kVectorCount> vectors_{};
};

[[nodiscard]] const char* msix_status_string(MsixStatus status) noexcept;

} // namespace metaflux::transport::vfio_user

#endif
