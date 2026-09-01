#ifndef METAFLUX_BACKEND_VULKAN_ADMISSION_HPP
#define METAFLUX_BACKEND_VULKAN_ADMISSION_HPP

#include "vulkan_device.hpp"

#include <cstdint>
#include <optional>

namespace metaflux::backend::vulkan {

enum class VulkanTransport : std::uint32_t {
  memfd = 1U,
  local_cdev = 2U,
  vfio_user = 3U,
};

enum class AdmissionStatus : std::uint32_t {
  success = 0,
  invalid_argument = 1,
  already_admitted = 2,
  stale_generation = 3,
  unsupported_transport = 4,
  no_device = 5,
  unsupported_features = 6,
  initialization_failed = 7,
  busy = 8,
  device_lost = 9,
};

[[nodiscard]] bool
valid_capability_profile(const mf_vulkan_capability_profile_v1& profile) noexcept;

// Fixes the capability profile, logical generation, and transport before a
// Vulkan handle can become visible to the backend.
class VulkanBackendAdmission final {
public:
  VulkanBackendAdmission() noexcept = default;
  ~VulkanBackendAdmission() noexcept = default;

  VulkanBackendAdmission(const VulkanBackendAdmission&) = delete;
  VulkanBackendAdmission& operator=(const VulkanBackendAdmission&) = delete;
  VulkanBackendAdmission(VulkanBackendAdmission&&) = delete;
  VulkanBackendAdmission& operator=(VulkanBackendAdmission&&) = delete;

  [[nodiscard]] AdmissionStatus admit(const mf_vulkan_capability_profile_v1& profile,
                                      std::uint64_t generation, VulkanTransport transport) noexcept;
  [[nodiscard]] AdmissionStatus activate() noexcept;
  [[nodiscard]] AdmissionStatus retire() noexcept;

  [[nodiscard]] const mf_vulkan_capability_profile_v1& profile() const noexcept { return profile_; }
  [[nodiscard]] std::uint64_t generation() const noexcept { return generation_; }
  [[nodiscard]] VulkanTransport transport() const noexcept { return transport_; }
  [[nodiscard]] VulkanDeviceContext* context() noexcept;
  [[nodiscard]] const VulkanDeviceContext* context() const noexcept;

private:
  enum class State : std::uint32_t {
    empty = 0U,
    admitted = 1U,
    active = 2U,
    retired = 3U,
  };

  mf_vulkan_capability_profile_v1 profile_{};
  std::optional<VulkanDeviceContext> context_{};
  std::uint64_t generation_ = 0U;
  VulkanTransport transport_ = VulkanTransport::memfd;
  State state_ = State::empty;
};

[[nodiscard]] const char* admission_status_string(AdmissionStatus status) noexcept;

} // namespace metaflux::backend::vulkan

#endif
