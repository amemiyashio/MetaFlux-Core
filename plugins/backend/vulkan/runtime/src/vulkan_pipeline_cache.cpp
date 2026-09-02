#include "vulkan_pipeline_cache.hpp"

#include <array>
#include <cstring>

namespace metaflux::backend::vulkan {

namespace {

constexpr std::size_t kPipelineCacheHeaderSize = 32U;

std::uint32_t read_u32(const std::uint8_t* bytes) noexcept {
  std::uint32_t value = 0U;
  std::memcpy(&value, bytes, sizeof(value));
  return value;
}

bool valid_initial_data(const VulkanDeviceContext& context,
                        std::span<const std::uint8_t> data) noexcept {
  if (data.empty()) {
    return true;
  }
  if (data.size() < kPipelineCacheHeaderSize) {
    return false;
  }
  const auto header_size = static_cast<std::size_t>(read_u32(data.data()));
  const auto header_version = read_u32(data.data() + sizeof(std::uint32_t));
  if (header_size < kPipelineCacheHeaderSize || header_size > data.size() ||
      header_version != VK_PIPELINE_CACHE_HEADER_VERSION_ONE) {
    return false;
  }

  VkPhysicalDeviceProperties properties{};
  vkGetPhysicalDeviceProperties(context.physical_device_handle(), &properties);
  if (read_u32(data.data() + 2U * sizeof(std::uint32_t)) != properties.vendorID ||
      read_u32(data.data() + 3U * sizeof(std::uint32_t)) != properties.deviceID) {
    return false;
  }
  return std::memcmp(data.data() + 4U * sizeof(std::uint32_t), properties.pipelineCacheUUID,
                     VK_UUID_SIZE) == 0;
}

} // namespace

VulkanPipelineCache::~VulkanPipelineCache() noexcept { destroy(); }

PipelineCacheStatus VulkanPipelineCache::map_result(VkResult result) noexcept {
  switch (result) {
  case VK_SUCCESS:
  case VK_INCOMPLETE:
    return PipelineCacheStatus::success;
  case VK_ERROR_OUT_OF_HOST_MEMORY:
  case VK_ERROR_OUT_OF_DEVICE_MEMORY:
    return PipelineCacheStatus::out_of_memory;
  case VK_ERROR_DEVICE_LOST:
    return PipelineCacheStatus::device_lost;
  case VK_ERROR_FEATURE_NOT_PRESENT:
    return PipelineCacheStatus::unsupported;
  default:
    return PipelineCacheStatus::initialization_failed;
  }
}

PipelineCacheStatus VulkanPipelineCache::create(
    std::span<const std::uint8_t> initial_data) noexcept {
  if (context_ == nullptr || !context_->ready()) {
    return context_ != nullptr && context_->lost() ? PipelineCacheStatus::device_lost
                                                    : PipelineCacheStatus::not_ready;
  }
  if (!valid_initial_data(*context_, initial_data)) {
    return PipelineCacheStatus::corrupt;
  }

  VkPipelineCache replacement_cache = VK_NULL_HANDLE;
  try {
    VkPipelineCacheCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
    info.initialDataSize = initial_data.size();
    info.pInitialData = initial_data.empty() ? nullptr : initial_data.data();
    const auto status = map_result(
        vkCreatePipelineCache(context_->device_handle(), &info, nullptr, &replacement_cache));
    if (status != PipelineCacheStatus::success) {
      if (replacement_cache != VK_NULL_HANDLE) {
        vkDestroyPipelineCache(context_->device_handle(), replacement_cache, nullptr);
      }
      return status;
    }
    const VkPipelineCache previous_cache = cache_;
    cache_ = replacement_cache;
    if (previous_cache != VK_NULL_HANDLE) {
      vkDestroyPipelineCache(context_->device_handle(), previous_cache, nullptr);
    }
    return PipelineCacheStatus::success;
  } catch (...) {
    if (replacement_cache != VK_NULL_HANDLE) {
      vkDestroyPipelineCache(context_->device_handle(), replacement_cache, nullptr);
    }
    return PipelineCacheStatus::out_of_memory;
  }
}

PipelineCacheStatus VulkanPipelineCache::export_data(std::vector<std::uint8_t>* out) const noexcept {
  if (out == nullptr) {
    return PipelineCacheStatus::invalid_argument;
  }
  out->clear();
  if (context_ == nullptr || !context_->ready()) {
    return context_ != nullptr && context_->lost() ? PipelineCacheStatus::device_lost
                                                    : PipelineCacheStatus::not_ready;
  }
  if (cache_ == VK_NULL_HANDLE) {
    return PipelineCacheStatus::invalid_argument;
  }

  try {
    std::size_t size = 0U;
    auto status = map_result(vkGetPipelineCacheData(context_->device_handle(), cache_, &size,
                                                     nullptr));
    if (status != PipelineCacheStatus::success || size == 0U) {
      return status;
    }
    out->resize(size);
    status = map_result(vkGetPipelineCacheData(context_->device_handle(), cache_, &size,
                                               out->data()));
    if (status != PipelineCacheStatus::success) {
      out->clear();
      return status;
    }
    out->resize(size);
    return PipelineCacheStatus::success;
  } catch (...) {
    out->clear();
    return PipelineCacheStatus::out_of_memory;
  }
}

void VulkanPipelineCache::destroy() noexcept {
  if (cache_ != VK_NULL_HANDLE && context_ != nullptr &&
      context_->device_handle() != VK_NULL_HANDLE) {
    vkDestroyPipelineCache(context_->device_handle(), cache_, nullptr);
  }
  cache_ = VK_NULL_HANDLE;
}

const char* pipeline_cache_status_string(PipelineCacheStatus status) noexcept {
  switch (status) {
  case PipelineCacheStatus::success:
    return "success";
  case PipelineCacheStatus::invalid_argument:
    return "invalid-argument";
  case PipelineCacheStatus::not_ready:
    return "not-ready";
  case PipelineCacheStatus::corrupt:
    return "corrupt";
  case PipelineCacheStatus::unsupported:
    return "unsupported";
  case PipelineCacheStatus::out_of_memory:
    return "out-of-memory";
  case PipelineCacheStatus::device_lost:
    return "device-lost";
  case PipelineCacheStatus::initialization_failed:
    return "initialization-failed";
  }
  return "unknown";
}

} // namespace metaflux::backend::vulkan
