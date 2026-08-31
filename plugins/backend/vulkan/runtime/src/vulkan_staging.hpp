#ifndef METAFLUX_BACKEND_VULKAN_STAGING_HPP
#define METAFLUX_BACKEND_VULKAN_STAGING_HPP

#include "vulkan_device.hpp"

#include <cstdint>

namespace metaflux::backend::vulkan {

enum class AllocationStatus : std::uint32_t {
  success = 0,
  invalid_argument = 1,
  not_ready = 2,
  unsupported = 3,
  out_of_memory = 4,
  device_lost = 5,
  range_out_of_bounds = 6,
};

struct VulkanBufferAllocation final {
  VkBuffer buffer = VK_NULL_HANDLE;
  VkDeviceMemory memory = VK_NULL_HANDLE;
  VkDeviceSize requested_size = 0U;
  VkDeviceSize allocation_size = 0U;
  VkDeviceSize alignment = 0U;
  VkDeviceSize atom_size = 1U;
  VkMemoryPropertyFlags memory_properties = 0U;
  void* mapped = nullptr;
};

// Owns one host-visible staging buffer and its bound device memory.
class VulkanStagingBuffer final {
public:
  explicit VulkanStagingBuffer(VulkanDeviceContext& context) noexcept : context_(&context) {}
  ~VulkanStagingBuffer() noexcept;

  VulkanStagingBuffer(const VulkanStagingBuffer&) = delete;
  VulkanStagingBuffer& operator=(const VulkanStagingBuffer&) = delete;
  VulkanStagingBuffer(VulkanStagingBuffer&&) = delete;
  VulkanStagingBuffer& operator=(VulkanStagingBuffer&&) = delete;

  [[nodiscard]] AllocationStatus allocate(VkDeviceSize size, VkDeviceSize alignment) noexcept;
  [[nodiscard]] AllocationStatus map() noexcept;
  [[nodiscard]] AllocationStatus flush(VkDeviceSize offset, VkDeviceSize size) noexcept;
  [[nodiscard]] AllocationStatus invalidate(VkDeviceSize offset, VkDeviceSize size) noexcept;
  void destroy() noexcept;

  [[nodiscard]] const VulkanBufferAllocation& allocation() const noexcept { return allocation_; }
  [[nodiscard]] bool ready() const noexcept {
    return allocation_.buffer != VK_NULL_HANDLE && allocation_.memory != VK_NULL_HANDLE;
  }

private:
  [[nodiscard]] AllocationStatus normalize_range(VkDeviceSize offset, VkDeviceSize size,
                                                 VkDeviceSize* out_offset,
                                                 VkDeviceSize* out_size) const noexcept;
  [[nodiscard]] static AllocationStatus map_result(VkResult result) noexcept;
  [[nodiscard]] static std::uint32_t
  select_memory_type(const VkPhysicalDeviceMemoryProperties& properties, std::uint32_t type_bits,
                     VkMemoryPropertyFlags required, VkMemoryPropertyFlags preferred,
                     VkMemoryPropertyFlags* out_flags) noexcept;

  VulkanDeviceContext* context_ = nullptr;
  VulkanBufferAllocation allocation_{};
};

[[nodiscard]] const char* allocation_status_string(AllocationStatus status) noexcept;

} // namespace metaflux::backend::vulkan

#endif
