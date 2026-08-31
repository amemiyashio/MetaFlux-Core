#ifndef METAFLUX_BACKEND_VULKAN_DEVICE_COPY_HPP
#define METAFLUX_BACKEND_VULKAN_DEVICE_COPY_HPP

#include "vulkan_staging.hpp"

#include <cstdint>

namespace metaflux::backend::vulkan {

// Owns one host-visible staging buffer and one device-local transfer buffer.
// This source-local adapter is the physical Tier 3 copy boundary; it does not
// cross mf_backend_api_v1 or claim external-memory support.
class VulkanDeviceLocalCopy final {
public:
  explicit VulkanDeviceLocalCopy(VulkanDeviceContext& context) noexcept
      : context_(&context), staging_(context) {}
  ~VulkanDeviceLocalCopy() noexcept;

  VulkanDeviceLocalCopy(const VulkanDeviceLocalCopy&) = delete;
  VulkanDeviceLocalCopy& operator=(const VulkanDeviceLocalCopy&) = delete;
  VulkanDeviceLocalCopy(VulkanDeviceLocalCopy&&) = delete;
  VulkanDeviceLocalCopy& operator=(VulkanDeviceLocalCopy&&) = delete;

  [[nodiscard]] AllocationStatus allocate(VkDeviceSize size, VkDeviceSize alignment) noexcept;
  [[nodiscard]] AllocationStatus map() noexcept { return staging_.map(); }
  [[nodiscard]] AllocationStatus upload(VkDeviceSize offset, VkDeviceSize size,
                                        std::uint64_t timeout_ns) noexcept;
  [[nodiscard]] AllocationStatus download(VkDeviceSize offset, VkDeviceSize size,
                                          std::uint64_t timeout_ns) noexcept;
  void destroy() noexcept;

  [[nodiscard]] bool ready() const noexcept {
    return staging_.ready() && device_.buffer != VK_NULL_HANDLE &&
           device_.memory != VK_NULL_HANDLE && command_pool_ != VK_NULL_HANDLE &&
           command_buffer_ != VK_NULL_HANDLE;
  }
  [[nodiscard]] const VulkanBufferAllocation& host_allocation() const noexcept {
    return staging_.allocation();
  }
  [[nodiscard]] const VulkanBufferAllocation& device_allocation() const noexcept { return device_; }

private:
  [[nodiscard]] static AllocationStatus map_result(VkResult result) noexcept;
  [[nodiscard]] static AllocationStatus map_device_status(DeviceStatus status) noexcept;
  [[nodiscard]] static std::uint32_t
  select_memory_type(const VkPhysicalDeviceMemoryProperties& properties, std::uint32_t type_bits,
                     VkMemoryPropertyFlags required, VkMemoryPropertyFlags* out_flags) noexcept;
  [[nodiscard]] AllocationStatus record_copy(VkBuffer source, VkBuffer destination,
                                             VkDeviceSize offset, VkDeviceSize size) noexcept;
  [[nodiscard]] AllocationStatus submit_copy(std::uint64_t timeout_ns) noexcept;

  VulkanDeviceContext* context_ = nullptr;
  VulkanStagingBuffer staging_;
  VulkanBufferAllocation device_{};
  VkCommandPool command_pool_ = VK_NULL_HANDLE;
  VkCommandBuffer command_buffer_ = VK_NULL_HANDLE;
};

} // namespace metaflux::backend::vulkan

#endif
