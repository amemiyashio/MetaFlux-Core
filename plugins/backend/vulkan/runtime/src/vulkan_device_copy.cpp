#include "vulkan_device_copy.hpp"

#include <limits>

namespace metaflux::backend::vulkan {
VulkanDeviceLocalCopy::~VulkanDeviceLocalCopy() noexcept { destroy(); }

AllocationStatus VulkanDeviceLocalCopy::map_result(VkResult result) noexcept {
  switch (result) {
  case VK_SUCCESS:
    return AllocationStatus::success;
  case VK_ERROR_OUT_OF_HOST_MEMORY:
  case VK_ERROR_OUT_OF_DEVICE_MEMORY:
  case VK_ERROR_MEMORY_MAP_FAILED:
    return AllocationStatus::out_of_memory;
  case VK_ERROR_DEVICE_LOST:
    return AllocationStatus::device_lost;
  case VK_ERROR_FEATURE_NOT_PRESENT:
    return AllocationStatus::unsupported;
  default:
    return AllocationStatus::invalid_argument;
  }
}

AllocationStatus VulkanDeviceLocalCopy::map_device_status(DeviceStatus status) noexcept {
  switch (status) {
  case DeviceStatus::success:
    return AllocationStatus::success;
  case DeviceStatus::device_lost:
    return AllocationStatus::device_lost;
  case DeviceStatus::not_ready:
  case DeviceStatus::busy:
    return AllocationStatus::not_ready;
  case DeviceStatus::unsupported_features:
    return AllocationStatus::unsupported;
  case DeviceStatus::stale_generation:
  case DeviceStatus::invalid_argument:
  case DeviceStatus::invalid_timeline:
    return AllocationStatus::invalid_argument;
  case DeviceStatus::no_device:
  case DeviceStatus::initialization_failed:
    return AllocationStatus::unsupported;
  }
  return AllocationStatus::invalid_argument;
}

std::uint32_t
VulkanDeviceLocalCopy::select_memory_type(const VkPhysicalDeviceMemoryProperties& properties,
                                          std::uint32_t type_bits, VkMemoryPropertyFlags required,
                                          VkMemoryPropertyFlags* out_flags) noexcept {
  if (out_flags == nullptr) {
    return UINT32_MAX;
  }
  for (std::uint32_t index = 0U; index < properties.memoryTypeCount; ++index) {
    if ((type_bits & (UINT32_C(1) << index)) == 0U) {
      continue;
    }
    const auto flags = properties.memoryTypes[index].propertyFlags;
    if ((flags & required) == required) {
      *out_flags = flags;
      return index;
    }
  }
  return UINT32_MAX;
}

AllocationStatus VulkanDeviceLocalCopy::allocate(VkDeviceSize size,
                                                 VkDeviceSize alignment) noexcept {
  if (context_ == nullptr || !context_->ready()) {
    return context_ != nullptr && context_->lost() ? AllocationStatus::device_lost
                                                   : AllocationStatus::not_ready;
  }
  if (size == 0U || alignment == 0U || (alignment & (alignment - 1U)) != 0U) {
    return AllocationStatus::invalid_argument;
  }
  destroy();
  AllocationStatus status = staging_.allocate(size, alignment);
  if (status != AllocationStatus::success) {
    return status;
  }

  VkBufferCreateInfo buffer_info{};
  buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  buffer_info.size = size;
  buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
  buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  status =
      map_result(vkCreateBuffer(context_->device_handle(), &buffer_info, nullptr, &device_.buffer));
  if (status != AllocationStatus::success) {
    staging_.destroy();
    return status;
  }

  VkMemoryRequirements requirements{};
  vkGetBufferMemoryRequirements(context_->device_handle(), device_.buffer, &requirements);
  if (requirements.size == 0U || requirements.memoryTypeBits == 0U ||
      requirements.alignment == 0U || requirements.size < size) {
    destroy();
    return AllocationStatus::unsupported;
  }
  VkPhysicalDeviceMemoryProperties properties{};
  vkGetPhysicalDeviceMemoryProperties(context_->physical_device_handle(), &properties);
  VkMemoryPropertyFlags memory_properties = 0U;
  const std::uint32_t memory_type =
      select_memory_type(properties, requirements.memoryTypeBits,
                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &memory_properties);
  if (memory_type == UINT32_MAX) {
    destroy();
    return AllocationStatus::unsupported;
  }

  VkMemoryAllocateInfo allocation_info{};
  allocation_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  allocation_info.allocationSize = requirements.size;
  allocation_info.memoryTypeIndex = memory_type;
  status = map_result(
      vkAllocateMemory(context_->device_handle(), &allocation_info, nullptr, &device_.memory));
  if (status != AllocationStatus::success) {
    destroy();
    return status;
  }
  status =
      map_result(vkBindBufferMemory(context_->device_handle(), device_.buffer, device_.memory, 0U));
  if (status != AllocationStatus::success) {
    destroy();
    return status;
  }
  device_.requested_size = size;
  device_.allocation_size = requirements.size;
  device_.alignment = requirements.alignment;
  device_.atom_size = context_->non_coherent_atom_size();
  device_.memory_properties = memory_properties;

  VkCommandPoolCreateInfo pool_info{};
  pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  pool_info.queueFamilyIndex = context_->queue_family_index();
  status = map_result(
      vkCreateCommandPool(context_->device_handle(), &pool_info, nullptr, &command_pool_));
  if (status != AllocationStatus::success) {
    destroy();
    return status;
  }
  VkCommandBufferAllocateInfo command_info{};
  command_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  command_info.commandPool = command_pool_;
  command_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  command_info.commandBufferCount = 1U;
  status = map_result(
      vkAllocateCommandBuffers(context_->device_handle(), &command_info, &command_buffer_));
  if (status != AllocationStatus::success) {
    destroy();
  }
  return status;
}

AllocationStatus VulkanDeviceLocalCopy::record_copy(VkBuffer source, VkBuffer destination,
                                                    VkDeviceSize offset,
                                                    VkDeviceSize size) noexcept {
  if (!ready() || source == VK_NULL_HANDLE || destination == VK_NULL_HANDLE || size == 0U) {
    return AllocationStatus::not_ready;
  }
  AllocationStatus status = map_result(vkResetCommandBuffer(command_buffer_, 0U));
  if (status != AllocationStatus::success) {
    return status;
  }
  VkCommandBufferBeginInfo begin_info{};
  begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  status = map_result(vkBeginCommandBuffer(command_buffer_, &begin_info));
  if (status != AllocationStatus::success) {
    return status;
  }
  VkBufferCopy region{};
  region.srcOffset = offset;
  region.dstOffset = offset;
  region.size = size;
  vkCmdCopyBuffer(command_buffer_, source, destination, 1U, &region);
  return map_result(vkEndCommandBuffer(command_buffer_));
}

AllocationStatus VulkanDeviceLocalCopy::submit_copy(std::uint64_t timeout_ns) noexcept {
  if (context_ == nullptr || !ready()) {
    return AllocationStatus::not_ready;
  }
  if (context_->last_submitted_value() == std::numeric_limits<std::uint64_t>::max()) {
    return AllocationStatus::invalid_argument;
  }
  const std::uint64_t signal_value = context_->last_submitted_value() + 1U;
  const auto submit_status = context_->submit_commands(
      context_->generation(), command_buffer_, context_->last_completed_value(), signal_value);
  const AllocationStatus status = map_device_status(submit_status);
  if (status != AllocationStatus::success) {
    return status;
  }
  return map_device_status(context_->wait(context_->generation(), signal_value, timeout_ns));
}

AllocationStatus VulkanDeviceLocalCopy::upload(VkDeviceSize offset, VkDeviceSize size,
                                               std::uint64_t timeout_ns) noexcept {
  if (!staging_.ready() || staging_.allocation().mapped == nullptr ||
      device_.buffer == VK_NULL_HANDLE) {
    return AllocationStatus::not_ready;
  }
  if (size == 0U || offset > device_.requested_size || size > device_.requested_size - offset) {
    return AllocationStatus::range_out_of_bounds;
  }
  AllocationStatus status = staging_.flush(offset, size);
  if (status != AllocationStatus::success) {
    return status;
  }
  status = record_copy(staging_.allocation().buffer, device_.buffer, offset, size);
  return status == AllocationStatus::success ? submit_copy(timeout_ns) : status;
}

AllocationStatus VulkanDeviceLocalCopy::download(VkDeviceSize offset, VkDeviceSize size,
                                                 std::uint64_t timeout_ns) noexcept {
  if (!staging_.ready() || staging_.allocation().mapped == nullptr ||
      device_.buffer == VK_NULL_HANDLE) {
    return AllocationStatus::not_ready;
  }
  if (size == 0U || offset > device_.requested_size || size > device_.requested_size - offset) {
    return AllocationStatus::range_out_of_bounds;
  }
  AllocationStatus status = record_copy(device_.buffer, staging_.allocation().buffer, offset, size);
  if (status != AllocationStatus::success) {
    return status;
  }
  status = submit_copy(timeout_ns);
  return status == AllocationStatus::success ? staging_.invalidate(offset, size) : status;
}

void VulkanDeviceLocalCopy::destroy() noexcept {
  if (context_ == nullptr || context_->device_handle() == VK_NULL_HANDLE) {
    command_pool_ = VK_NULL_HANDLE;
    command_buffer_ = VK_NULL_HANDLE;
    device_ = {};
    staging_.destroy();
    return;
  }
  if (command_pool_ != VK_NULL_HANDLE) {
    vkDestroyCommandPool(context_->device_handle(), command_pool_, nullptr);
  }
  command_pool_ = VK_NULL_HANDLE;
  command_buffer_ = VK_NULL_HANDLE;
  if (device_.memory != VK_NULL_HANDLE) {
    vkFreeMemory(context_->device_handle(), device_.memory, nullptr);
  }
  if (device_.buffer != VK_NULL_HANDLE) {
    vkDestroyBuffer(context_->device_handle(), device_.buffer, nullptr);
  }
  device_ = {};
  staging_.destroy();
}

} // namespace metaflux::backend::vulkan
