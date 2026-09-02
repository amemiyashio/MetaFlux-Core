#ifndef METAFLUX_BACKEND_VULKAN_PIPELINE_HPP
#define METAFLUX_BACKEND_VULKAN_PIPELINE_HPP

#include "vulkan_device.hpp"
#include "metaflux/backend/vulkan_target.hpp"

#include <cstdint>
#include <span>
#include <string_view>

namespace metaflux::backend::vulkan {

enum class PipelineStatus : std::uint32_t {
  success = 0,
  invalid_argument = 1,
  not_ready = 2,
  invalid_module = 3,
  target_mismatch = 4,
  unsupported = 5,
  out_of_memory = 6,
  device_lost = 7,
  compile_required = 8,
  initialization_failed = 9,
};

// Owns one cold-path compute shader module and pipeline. Pipeline layouts are
// supplied by the caller because descriptor ownership belongs to the execution
// resource model; neither layout nor Vulkan handles cross the C ABI.
class VulkanComputePipeline final {
public:
  explicit VulkanComputePipeline(VulkanDeviceContext& context) noexcept : context_(&context) {}
  ~VulkanComputePipeline() noexcept;

  VulkanComputePipeline(const VulkanComputePipeline&) = delete;
  VulkanComputePipeline& operator=(const VulkanComputePipeline&) = delete;
  VulkanComputePipeline(VulkanComputePipeline&&) = delete;
  VulkanComputePipeline& operator=(VulkanComputePipeline&&) = delete;

  [[nodiscard]] PipelineStatus create(std::span<const std::uint32_t> spirv,
                                      std::string_view entry_point,
                                      VkPipelineLayout layout,
                                      VkPipelineCache cache = VK_NULL_HANDLE) noexcept;
  [[nodiscard]] PipelineStatus create_validated(
      const mf_vulkan_capability_profile_v1& profile, const SpirvModuleRequirements& module,
      const SpirvReflection& reflection, std::span<const std::uint32_t> spirv,
      VkPipelineLayout layout, VkPipelineCache cache = VK_NULL_HANDLE) noexcept;
  [[nodiscard]] PipelineStatus bind(VkCommandBuffer command_buffer) const noexcept;
  [[nodiscard]] PipelineStatus dispatch(VkCommandBuffer command_buffer, std::uint32_t groups_x,
                                        std::uint32_t groups_y,
                                        std::uint32_t groups_z) const noexcept;
  void destroy() noexcept;

  [[nodiscard]] VkPipeline handle() const noexcept { return pipeline_; }
  [[nodiscard]] bool ready() const noexcept { return pipeline_ != VK_NULL_HANDLE; }
  [[nodiscard]] bool uses_context(const VulkanDeviceContext& context) const noexcept {
    return context_ == &context;
  }

private:
  [[nodiscard]] static PipelineStatus map_result(VkResult result) noexcept;

  VulkanDeviceContext* context_ = nullptr;
  VkShaderModule shader_module_ = VK_NULL_HANDLE;
  VkPipeline pipeline_ = VK_NULL_HANDLE;
};

[[nodiscard]] const char* pipeline_status_string(PipelineStatus status) noexcept;

[[nodiscard]] PipelineStatus validate_spirv_binary(
    std::span<const std::uint32_t> spirv) noexcept;

} // namespace metaflux::backend::vulkan

#endif
