#include "metaflux/backend/vulkan.h"

#include "vulkan_device.hpp"

#include <vulkan/vulkan.h>

#include <algorithm>
#include <chrono>
#include <cstdint>

namespace {

using metaflux::backend::vulkan::DeviceStatus;
using metaflux::backend::vulkan::VulkanDeviceContext;

std::uint64_t steady_ns() noexcept {
  const auto now = std::chrono::steady_clock::now().time_since_epoch();
  return static_cast<std::uint64_t>(
      std::chrono::duration_cast<std::chrono::nanoseconds>(now).count());
}

} // namespace

extern "C" mf_vulkan_probe_status_v1 mf_vulkan_probe_execution_timestamps_v1(
    const mf_vulkan_capability_profile_v1* profile, const std::uint32_t* spirv_code,
    std::size_t spirv_size, mf_vulkan_execution_timestamps_v1* out_points) {
  if (profile == nullptr || spirv_code == nullptr || spirv_size == 0U ||
      (spirv_size % sizeof(std::uint32_t)) != 0U || out_points == nullptr) {
    return MF_VULKAN_PROBE_INVALID_ARGUMENT;
  }

  VulkanDeviceContext context(1U);
  const auto initialized = context.initialize(*profile);
  if (initialized != DeviceStatus::success) {
    switch (initialized) {
    case DeviceStatus::no_device:
      return MF_VULKAN_PROBE_NO_DEVICE;
    case DeviceStatus::unsupported_features:
      return MF_VULKAN_PROBE_UNSUPPORTED_DEVICE;
    case DeviceStatus::invalid_argument:
      return MF_VULKAN_PROBE_INVALID_ARGUMENT;
    default:
      return MF_VULKAN_PROBE_INITIALIZATION_FAILED;
    }
  }
  // Every exit from here owns live Vulkan handles; the guard keeps the
  // "no Vulkan handles after it returns" contract on failure paths too.
  struct ContextGuard {
    VulkanDeviceContext& context;
    ~ContextGuard() { context.reset(); }
  } const context_guard{context};
  VkDevice device = context.device_handle();

  VkShaderModuleCreateInfo module_info{};
  module_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  module_info.codeSize = spirv_size;
  module_info.pCode = spirv_code;
  VkShaderModule shader = VK_NULL_HANDLE;
  VkPipelineLayout layout = VK_NULL_HANDLE;
  VkPipeline pipeline = VK_NULL_HANDLE;
  VkQueryPool query_pool = VK_NULL_HANDLE;
  VkCommandPool command_pool = VK_NULL_HANDLE;
  VkCommandBuffer command_buffer = VK_NULL_HANDLE;
  const auto destroy_pending = [&]() noexcept {
    if (command_buffer != VK_NULL_HANDLE) {
      vkFreeCommandBuffers(device, command_pool, 1U, &command_buffer);
    }
    if (command_pool != VK_NULL_HANDLE) {
      vkDestroyCommandPool(device, command_pool, nullptr);
    }
    if (query_pool != VK_NULL_HANDLE) {
      vkDestroyQueryPool(device, query_pool, nullptr);
    }
    if (pipeline != VK_NULL_HANDLE) {
      vkDestroyPipeline(device, pipeline, nullptr);
    }
    if (layout != VK_NULL_HANDLE) {
      vkDestroyPipelineLayout(device, layout, nullptr);
    }
    if (shader != VK_NULL_HANDLE) {
      vkDestroyShaderModule(device, shader, nullptr);
    }
  };

  if (vkCreateShaderModule(device, &module_info, nullptr, &shader) != VK_SUCCESS) {
    destroy_pending();
    return MF_VULKAN_PROBE_INTERNAL_ERROR;
  }
  VkPipelineShaderStageCreateInfo stage{};
  stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
  stage.module = shader;
  stage.pName = "main";
  VkPipelineLayoutCreateInfo layout_info{};
  layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  if (vkCreatePipelineLayout(device, &layout_info, nullptr, &layout) != VK_SUCCESS) {
    destroy_pending();
    return MF_VULKAN_PROBE_INTERNAL_ERROR;
  }
  VkComputePipelineCreateInfo pipeline_info{};
  pipeline_info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
  pipeline_info.stage = stage;
  pipeline_info.layout = layout;
  if (vkCreateComputePipelines(device, VK_NULL_HANDLE, 1U, &pipeline_info, nullptr,
                               &pipeline) != VK_SUCCESS) {
    destroy_pending();
    return MF_VULKAN_PROBE_INTERNAL_ERROR;
  }

  VkQueryPoolCreateInfo query_info{};
  query_info.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
  query_info.queryType = VK_QUERY_TYPE_TIMESTAMP;
  query_info.queryCount = 2U;
  if (vkCreateQueryPool(device, &query_info, nullptr, &query_pool) != VK_SUCCESS) {
    destroy_pending();
    return MF_VULKAN_PROBE_INTERNAL_ERROR;
  }
  VkCommandPoolCreateInfo pool_info{};
  pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  pool_info.queueFamilyIndex = context.queue_family_index();
  if (vkCreateCommandPool(device, &pool_info, nullptr, &command_pool) != VK_SUCCESS) {
    destroy_pending();
    return MF_VULKAN_PROBE_INTERNAL_ERROR;
  }
  VkCommandBufferAllocateInfo allocate_info{};
  allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocate_info.commandPool = command_pool;
  allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocate_info.commandBufferCount = 1U;
  if (vkAllocateCommandBuffers(device, &allocate_info, &command_buffer) != VK_SUCCESS) {
    destroy_pending();
    return MF_VULKAN_PROBE_INTERNAL_ERROR;
  }

  const auto enqueue_anchor = steady_ns();
  VkCommandBufferBeginInfo begin_info{};
  begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  if (vkBeginCommandBuffer(command_buffer, &begin_info) != VK_SUCCESS) {
    destroy_pending();
    return MF_VULKAN_PROBE_INTERNAL_ERROR;
  }
  vkCmdResetQueryPool(command_buffer, query_pool, 0U, 2U);
  vkCmdWriteTimestamp(command_buffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, query_pool, 0U);
  vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
  vkCmdDispatch(command_buffer, 1U, 1U, 1U);
  vkCmdWriteTimestamp(command_buffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, query_pool, 1U);
  if (vkEndCommandBuffer(command_buffer) != VK_SUCCESS) {
    destroy_pending();
    return MF_VULKAN_PROBE_INTERNAL_ERROR;
  }
  const auto enqueue_ns = enqueue_anchor;

  if (context.submit_commands(1U, command_buffer, 0U, 1U) != DeviceStatus::success) {
    destroy_pending();
    return MF_VULKAN_PROBE_INTERNAL_ERROR;
  }
  const auto submit_ns = steady_ns();
  if (context.wait(1U, 1U, UINT64_C(5000000000)) != DeviceStatus::success) {
    destroy_pending();
    return MF_VULKAN_PROBE_INTERNAL_ERROR;
  }
  const auto completion_ns = steady_ns();

  std::uint64_t stamps[2] = {0U, 0U};
  if (vkGetQueryPoolResults(device, query_pool, 0U, 2U, sizeof(stamps), stamps,
                            sizeof(std::uint64_t),
                            VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT) != VK_SUCCESS) {
    destroy_pending();
    return MF_VULKAN_PROBE_INTERNAL_ERROR;
  }
  VkPhysicalDeviceProperties properties{};
  vkGetPhysicalDeviceProperties(context.physical_device_handle(), &properties);
  const double window_ns = static_cast<double>(stamps[1] - stamps[0]) *
                           static_cast<double>(properties.limits.timestampPeriod);
  destroy_pending();

  out_points->struct_size = static_cast<std::uint32_t>(sizeof(*out_points));
  out_points->abi_version = MF_VULKAN_CAPABILITY_ABI_VERSION_1;
  out_points->enqueue_ns = enqueue_ns;
  out_points->submit_ns = submit_ns;
  // Device timestamps run on their own clock; the dispatch window is anchored
  // to the host completion observation and clamped so ordering stays valid
  // when the host wait returns long after the dispatch finished.
  const auto anchored_start = static_cast<std::uint64_t>(std::max<double>(
      static_cast<double>(submit_ns) + 1.0,
      static_cast<double>(completion_ns) - window_ns));
  out_points->start_ns = anchored_start;
  out_points->completion_ns = std::max(completion_ns, anchored_start);
  return MF_VULKAN_PROBE_SUCCESS;
}
