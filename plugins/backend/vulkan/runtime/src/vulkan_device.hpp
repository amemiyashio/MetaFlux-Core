#ifndef METAFLUX_BACKEND_VULKAN_DEVICE_HPP
#define METAFLUX_BACKEND_VULKAN_DEVICE_HPP

#include "metaflux/backend/vulkan.h"

#include <vulkan/vulkan.h>

#include <cstdint>

namespace metaflux::backend::vulkan {

enum class DeviceStatus : std::uint32_t {
  success = 0,
  invalid_argument = 1,
  stale_generation = 2,
  no_device = 3,
  unsupported_features = 4,
  initialization_failed = 5,
  not_ready = 6,
  busy = 7,
  invalid_timeline = 8,
  device_lost = 9,
};

// Owns the Vulkan handles for one generation-bound logical compute context.
// This header is intentionally source-local: no Vulkan type crosses the stable
// mf_backend_api_v1 boundary.
class VulkanDeviceContext final {
public:
  explicit VulkanDeviceContext(std::uint64_t generation = 0U) noexcept : generation_(generation) {}
  ~VulkanDeviceContext() noexcept;

  VulkanDeviceContext(const VulkanDeviceContext&) = delete;
  VulkanDeviceContext& operator=(const VulkanDeviceContext&) = delete;
  VulkanDeviceContext(VulkanDeviceContext&&) = delete;
  VulkanDeviceContext& operator=(VulkanDeviceContext&&) = delete;

  [[nodiscard]] DeviceStatus initialize(const mf_vulkan_capability_profile_v1& profile) noexcept;
  [[nodiscard]] DeviceStatus submit_signal(std::uint64_t generation, std::uint64_t value) noexcept;
  [[nodiscard]] DeviceStatus wait(std::uint64_t generation, std::uint64_t value,
                                  std::uint64_t timeout_ns) noexcept;
  [[nodiscard]] DeviceStatus poll(std::uint64_t generation, std::uint64_t* out_value) noexcept;

  // A healthy context may be torn down once. A device-loss transition remains
  // retired, so a new generation must create a new context object.
  void reset() noexcept;

  [[nodiscard]] bool ready() const noexcept {
    return instance_ != VK_NULL_HANDLE && physical_device_ != VK_NULL_HANDLE &&
           device_ != VK_NULL_HANDLE && queue_ != VK_NULL_HANDLE && timeline_ != VK_NULL_HANDLE &&
           !lost_;
  }
  [[nodiscard]] bool lost() const noexcept { return lost_; }
  [[nodiscard]] std::uint64_t generation() const noexcept { return generation_; }
  [[nodiscard]] std::uint32_t queue_family_index() const noexcept { return queue_family_index_; }
  [[nodiscard]] std::uint64_t last_submitted_value() const noexcept { return last_submitted_; }
  [[nodiscard]] std::uint64_t last_completed_value() const noexcept { return last_completed_; }

private:
  [[nodiscard]] static DeviceStatus map_initialization_result(VkResult result) noexcept;
  [[nodiscard]] DeviceStatus map_runtime_result(VkResult result) noexcept;
  void destroy_handles() noexcept;

  VkInstance instance_ = VK_NULL_HANDLE;
  VkPhysicalDevice physical_device_ = VK_NULL_HANDLE;
  VkDevice device_ = VK_NULL_HANDLE;
  VkQueue queue_ = VK_NULL_HANDLE;
  VkSemaphore timeline_ = VK_NULL_HANDLE;
  std::uint32_t queue_family_index_ = UINT32_MAX;
  std::uint64_t generation_ = 0U;
  std::uint64_t last_submitted_ = 0U;
  std::uint64_t last_completed_ = 0U;
  bool lost_ = false;
};

[[nodiscard]] const char* device_status_string(DeviceStatus status) noexcept;

} // namespace metaflux::backend::vulkan

#endif
