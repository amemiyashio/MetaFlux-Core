#ifndef METAFLUX_BACKEND_VULKAN_PIPELINE_CACHE_HPP
#define METAFLUX_BACKEND_VULKAN_PIPELINE_CACHE_HPP

#include "vulkan_device.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace metaflux::backend::vulkan {

enum class PipelineCacheStatus : std::uint32_t {
  success = 0,
  invalid_argument = 1,
  not_ready = 2,
  corrupt = 3,
  unsupported = 4,
  out_of_memory = 5,
  device_lost = 6,
  initialization_failed = 7,
};

// Owns opaque VkPipelineCache data for one generation-bound Vulkan device.
// Filesystem persistence and cache identity remain repository responsibilities.
class VulkanPipelineCache final {
public:
  explicit VulkanPipelineCache(VulkanDeviceContext& context) noexcept : context_(&context) {}
  ~VulkanPipelineCache() noexcept;

  VulkanPipelineCache(const VulkanPipelineCache&) = delete;
  VulkanPipelineCache& operator=(const VulkanPipelineCache&) = delete;
  VulkanPipelineCache(VulkanPipelineCache&&) = delete;
  VulkanPipelineCache& operator=(VulkanPipelineCache&&) = delete;

  [[nodiscard]] PipelineCacheStatus create(
      std::span<const std::uint8_t> initial_data = {}) noexcept;
  [[nodiscard]] PipelineCacheStatus export_data(std::vector<std::uint8_t>* out) const noexcept;
  void destroy() noexcept;

  [[nodiscard]] VkPipelineCache handle() const noexcept { return cache_; }
  [[nodiscard]] bool ready() const noexcept { return cache_ != VK_NULL_HANDLE; }

private:
  [[nodiscard]] static PipelineCacheStatus map_result(VkResult result) noexcept;

  VulkanDeviceContext* context_ = nullptr;
  VkPipelineCache cache_ = VK_NULL_HANDLE;
};

[[nodiscard]] const char* pipeline_cache_status_string(PipelineCacheStatus status) noexcept;

} // namespace metaflux::backend::vulkan

#endif
