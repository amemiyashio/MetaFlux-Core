#include "vulkan_pipeline.hpp"

#include <algorithm>
#include <limits>
#include <string>

namespace metaflux::backend::vulkan {
namespace {

bool valid_entry_point(std::string_view entry_point) noexcept {
  return !entry_point.empty() && entry_point.size() <= 255U &&
         std::all_of(entry_point.begin(), entry_point.end(), [](char value) {
           const auto byte = static_cast<unsigned char>(value);
           return byte >= 0x21U && byte <= 0x7eU;
         });
}

} // namespace

PipelineStatus validate_spirv_binary(std::span<const std::uint32_t> spirv) noexcept {
  if (spirv.size() < 5U || spirv[0] != UINT32_C(0x07230203) ||
      spirv[1] < UINT32_C(0x00010000) || (spirv[1] >> 16U) != 1U || spirv[3] == 0U ||
      spirv[4] != 0U) {
    return PipelineStatus::invalid_module;
  }
  return PipelineStatus::success;
}

VulkanComputePipeline::~VulkanComputePipeline() noexcept { destroy(); }

PipelineStatus VulkanComputePipeline::map_result(VkResult result) noexcept {
  switch (result) {
  case VK_SUCCESS:
    return PipelineStatus::success;
  case VK_ERROR_OUT_OF_HOST_MEMORY:
  case VK_ERROR_OUT_OF_DEVICE_MEMORY:
    return PipelineStatus::out_of_memory;
  case VK_ERROR_DEVICE_LOST:
    return PipelineStatus::device_lost;
  case VK_ERROR_FEATURE_NOT_PRESENT:
    return PipelineStatus::unsupported;
#ifdef VK_PIPELINE_COMPILE_REQUIRED
  case VK_PIPELINE_COMPILE_REQUIRED:
    return PipelineStatus::compile_required;
#endif
#ifdef VK_ERROR_INVALID_SHADER_NV
  case VK_ERROR_INVALID_SHADER_NV:
    return PipelineStatus::invalid_module;
#endif
  default:
    return PipelineStatus::initialization_failed;
  }
}

PipelineStatus VulkanComputePipeline::create(std::span<const std::uint32_t> spirv,
                                             std::string_view entry_point,
                                             VkPipelineLayout layout,
                                             VkPipelineCache cache) noexcept {
  if (context_ == nullptr || !context_->ready()) {
    return context_ != nullptr && context_->lost() ? PipelineStatus::device_lost
                                                    : PipelineStatus::not_ready;
  }
  if (layout == VK_NULL_HANDLE || !valid_entry_point(entry_point) ||
      spirv.size() > std::numeric_limits<std::size_t>::max() / sizeof(std::uint32_t) ||
      validate_spirv_binary(spirv) != PipelineStatus::success) {
    return PipelineStatus::invalid_argument;
  }

  VkShaderModule replacement_shader_module = VK_NULL_HANDLE;
  VkPipeline replacement_pipeline = VK_NULL_HANDLE;
  try {
    VkShaderModuleCreateInfo module_info{};
    module_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    module_info.codeSize = spirv.size_bytes();
    module_info.pCode = spirv.data();
    PipelineStatus status = map_result(vkCreateShaderModule(
        context_->device_handle(), &module_info, nullptr, &replacement_shader_module));
    if (status != PipelineStatus::success) {
      return status;
    }

    const std::string name(entry_point);
    VkPipelineShaderStageCreateInfo stage{};
    stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stage.module = replacement_shader_module;
    stage.pName = name.c_str();
    VkComputePipelineCreateInfo pipeline_info{};
    pipeline_info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipeline_info.stage = stage;
    pipeline_info.layout = layout;
    status = map_result(vkCreateComputePipelines(context_->device_handle(), cache, 1U,
                                                  &pipeline_info, nullptr, &replacement_pipeline));
    if (status != PipelineStatus::success) {
      if (replacement_pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(context_->device_handle(), replacement_pipeline, nullptr);
      }
      if (replacement_shader_module != VK_NULL_HANDLE) {
        vkDestroyShaderModule(context_->device_handle(), replacement_shader_module, nullptr);
      }
      return status;
    }
    const VkPipeline previous_pipeline = pipeline_;
    const VkShaderModule previous_shader_module = shader_module_;
    pipeline_ = replacement_pipeline;
    shader_module_ = replacement_shader_module;
    if (previous_pipeline != VK_NULL_HANDLE) {
      vkDestroyPipeline(context_->device_handle(), previous_pipeline, nullptr);
    }
    if (previous_shader_module != VK_NULL_HANDLE) {
      vkDestroyShaderModule(context_->device_handle(), previous_shader_module, nullptr);
    }
    return PipelineStatus::success;
  } catch (...) {
    if (replacement_pipeline != VK_NULL_HANDLE) {
      vkDestroyPipeline(context_->device_handle(), replacement_pipeline, nullptr);
    }
    if (replacement_shader_module != VK_NULL_HANDLE) {
      vkDestroyShaderModule(context_->device_handle(), replacement_shader_module, nullptr);
    }
    return PipelineStatus::out_of_memory;
  }
}

PipelineStatus VulkanComputePipeline::create_validated(
    const mf_vulkan_capability_profile_v1& profile, const SpirvModuleRequirements& module,
    const SpirvReflection& reflection, std::span<const std::uint32_t> spirv,
    VkPipelineLayout layout, VkPipelineCache cache) noexcept {
  const auto status = validate_spirv_reflection(profile, module, reflection);
  if (status != TargetStatus::success) {
    return status == TargetStatus::target_mismatch ? PipelineStatus::target_mismatch
                                                    : PipelineStatus::invalid_module;
  }
  if (reflection.entry_point.empty()) {
    return PipelineStatus::invalid_module;
  }
  return create(spirv, reflection.entry_point, layout, cache);
}

PipelineStatus VulkanComputePipeline::bind(VkCommandBuffer command_buffer) const noexcept {
  if (context_ == nullptr || !context_->ready()) {
    return context_ != nullptr && context_->lost() ? PipelineStatus::device_lost
                                                    : PipelineStatus::not_ready;
  }
  if (pipeline_ == VK_NULL_HANDLE || command_buffer == VK_NULL_HANDLE) {
    return PipelineStatus::invalid_argument;
  }
  vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_);
  return PipelineStatus::success;
}

PipelineStatus VulkanComputePipeline::dispatch(VkCommandBuffer command_buffer,
                                               std::uint32_t groups_x,
                                               std::uint32_t groups_y,
                                               std::uint32_t groups_z) const noexcept {
  if (context_ == nullptr || !context_->ready()) {
    return context_ != nullptr && context_->lost() ? PipelineStatus::device_lost
                                                    : PipelineStatus::not_ready;
  }
  if (pipeline_ == VK_NULL_HANDLE || command_buffer == VK_NULL_HANDLE || groups_x == 0U ||
      groups_y == 0U || groups_z == 0U) {
    return PipelineStatus::invalid_argument;
  }
  vkCmdDispatch(command_buffer, groups_x, groups_y, groups_z);
  return PipelineStatus::success;
}

void VulkanComputePipeline::destroy() noexcept {
  if (context_ == nullptr || context_->device_handle() == VK_NULL_HANDLE) {
    pipeline_ = VK_NULL_HANDLE;
    shader_module_ = VK_NULL_HANDLE;
    return;
  }
  if (pipeline_ != VK_NULL_HANDLE) {
    vkDestroyPipeline(context_->device_handle(), pipeline_, nullptr);
  }
  pipeline_ = VK_NULL_HANDLE;
  if (shader_module_ != VK_NULL_HANDLE) {
    vkDestroyShaderModule(context_->device_handle(), shader_module_, nullptr);
  }
  shader_module_ = VK_NULL_HANDLE;
}

const char* pipeline_status_string(PipelineStatus status) noexcept {
  switch (status) {
  case PipelineStatus::success:
    return "success";
  case PipelineStatus::invalid_argument:
    return "invalid-argument";
  case PipelineStatus::not_ready:
    return "not-ready";
  case PipelineStatus::invalid_module:
    return "invalid-module";
  case PipelineStatus::target_mismatch:
    return "target-mismatch";
  case PipelineStatus::unsupported:
    return "unsupported";
  case PipelineStatus::out_of_memory:
    return "out-of-memory";
  case PipelineStatus::device_lost:
    return "device-lost";
  case PipelineStatus::compile_required:
    return "compile-required";
  case PipelineStatus::initialization_failed:
    return "initialization-failed";
  }
  return "unknown";
}

} // namespace metaflux::backend::vulkan
