#include "vulkan_staging.hpp"

#include <algorithm>
#include <limits>

namespace metaflux::backend::vulkan {
namespace {

bool power_of_two(VkDeviceSize value) noexcept {
  return value != 0U && (value & (value - 1U)) == 0U;
}

bool round_up(VkDeviceSize value, VkDeviceSize alignment, VkDeviceSize* out) noexcept {
  if (out == nullptr || alignment == 0U) {
    return false;
  }
  const VkDeviceSize remainder = value % alignment;
  const VkDeviceSize increment = remainder == 0U ? 0U : alignment - remainder;
  if (value > std::numeric_limits<VkDeviceSize>::max() - increment) {
    return false;
  }
  *out = value + increment;
  return true;
}

} // namespace

VulkanStagingBuffer::~VulkanStagingBuffer() noexcept { destroy(); }

AllocationStatus VulkanStagingBuffer::map_result(VkResult result) noexcept {
  switch (result) {
  case VK_SUCCESS:
    return AllocationStatus::success;
  case VK_ERROR_OUT_OF_HOST_MEMORY:
  case VK_ERROR_OUT_OF_DEVICE_MEMORY:
    return AllocationStatus::out_of_memory;
  case VK_ERROR_DEVICE_LOST:
    return AllocationStatus::device_lost;
  case VK_ERROR_FEATURE_NOT_PRESENT:
    return AllocationStatus::unsupported;
  default:
    return AllocationStatus::invalid_argument;
  }
}

std::uint32_t
VulkanStagingBuffer::select_memory_type(const VkPhysicalDeviceMemoryProperties& properties,
                                        std::uint32_t type_bits, VkMemoryPropertyFlags required,
                                        VkMemoryPropertyFlags preferred,
                                        VkMemoryPropertyFlags* out_flags) noexcept {
  if (out_flags == nullptr) {
    return UINT32_MAX;
  }
  for (std::uint32_t index = 0U; index < properties.memoryTypeCount; ++index) {
    if ((type_bits & (UINT32_C(1) << index)) == 0U) {
      continue;
    }
    const auto flags = properties.memoryTypes[index].propertyFlags;
    if ((flags & required) == required && (flags & preferred) == preferred) {
      *out_flags = flags;
      return index;
    }
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

AllocationStatus VulkanStagingBuffer::allocate(VkDeviceSize size, VkDeviceSize alignment) noexcept {
  if (context_ == nullptr || !context_->ready()) {
    return context_ != nullptr && context_->lost() ? AllocationStatus::device_lost
                                                   : AllocationStatus::not_ready;
  }
  if (size == 0U || !power_of_two(alignment)) {
    return AllocationStatus::invalid_argument;
  }
  destroy();

  VkBufferCreateInfo buffer_info{};
  buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  buffer_info.size = size;
  buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
  buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  VkBuffer buffer = VK_NULL_HANDLE;
  AllocationStatus status =
      map_result(vkCreateBuffer(context_->device_handle(), &buffer_info, nullptr, &buffer));
  if (status != AllocationStatus::success) {
    return status;
  }

  VkMemoryRequirements requirements{};
  vkGetBufferMemoryRequirements(context_->device_handle(), buffer, &requirements);
  if (requirements.size == 0U || requirements.memoryTypeBits == 0U ||
      requirements.alignment == 0U || requirements.size < size) {
    vkDestroyBuffer(context_->device_handle(), buffer, nullptr);
    return AllocationStatus::unsupported;
  }
  VkPhysicalDeviceMemoryProperties properties{};
  vkGetPhysicalDeviceMemoryProperties(context_->physical_device_handle(), &properties);
  VkMemoryPropertyFlags memory_properties = 0U;
  const std::uint32_t memory_type = select_memory_type(
      properties, requirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
      VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &memory_properties);
  if (memory_type == UINT32_MAX) {
    vkDestroyBuffer(context_->device_handle(), buffer, nullptr);
    return AllocationStatus::unsupported;
  }

  VkMemoryAllocateInfo allocation_info{};
  allocation_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  allocation_info.allocationSize = requirements.size;
  allocation_info.memoryTypeIndex = memory_type;
  VkDeviceMemory memory = VK_NULL_HANDLE;
  status =
      map_result(vkAllocateMemory(context_->device_handle(), &allocation_info, nullptr, &memory));
  if (status != AllocationStatus::success) {
    vkDestroyBuffer(context_->device_handle(), buffer, nullptr);
    return status;
  }
  const VkResult bind_result = vkBindBufferMemory(context_->device_handle(), buffer, memory, 0U);
  status = map_result(bind_result);
  if (status != AllocationStatus::success) {
    vkFreeMemory(context_->device_handle(), memory, nullptr);
    vkDestroyBuffer(context_->device_handle(), buffer, nullptr);
    return status;
  }
  allocation_ = {.buffer = buffer,
                 .memory = memory,
                 .requested_size = size,
                 .allocation_size = requirements.size,
                 .alignment = std::max(alignment, requirements.alignment),
                 .atom_size = std::max<VkDeviceSize>(context_->non_coherent_atom_size(), 1U),
                 .memory_properties = memory_properties,
                 .mapped = nullptr};
  return AllocationStatus::success;
}

AllocationStatus VulkanStagingBuffer::map() noexcept {
  if (context_ == nullptr || !ready()) {
    return context_ != nullptr && context_->lost() ? AllocationStatus::device_lost
                                                   : AllocationStatus::not_ready;
  }
  if (allocation_.mapped != nullptr) {
    return AllocationStatus::success;
  }
  return map_result(vkMapMemory(context_->device_handle(), allocation_.memory, 0U,
                                allocation_.allocation_size, 0U, &allocation_.mapped));
}

AllocationStatus VulkanStagingBuffer::normalize_range(VkDeviceSize offset, VkDeviceSize size,
                                                      VkDeviceSize* out_offset,
                                                      VkDeviceSize* out_size) const noexcept {
  if (context_ == nullptr || !ready() || allocation_.mapped == nullptr || out_offset == nullptr ||
      out_size == nullptr || size == 0U || offset > allocation_.requested_size ||
      size > allocation_.requested_size - offset) {
    return (offset > allocation_.requested_size || size == 0U ||
            (offset <= allocation_.requested_size && size > allocation_.requested_size - offset))
               ? AllocationStatus::range_out_of_bounds
               : AllocationStatus::not_ready;
  }
  if ((allocation_.memory_properties & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) != 0U) {
    *out_offset = offset;
    *out_size = size;
    return AllocationStatus::success;
  }
  const VkDeviceSize atom = std::max<VkDeviceSize>(allocation_.atom_size, 1U);
  const VkDeviceSize aligned_offset = offset - offset % atom;
  VkDeviceSize end = 0U;
  VkDeviceSize aligned_end = 0U;
  if (offset > std::numeric_limits<VkDeviceSize>::max() - size ||
      !round_up(offset + size, atom, &aligned_end)) {
    return AllocationStatus::range_out_of_bounds;
  }
  end = std::min(aligned_end, allocation_.allocation_size);
  if (end <= aligned_offset) {
    return AllocationStatus::range_out_of_bounds;
  }
  *out_offset = aligned_offset;
  *out_size = end - aligned_offset;
  return AllocationStatus::success;
}

AllocationStatus VulkanStagingBuffer::flush(VkDeviceSize offset, VkDeviceSize size) noexcept {
  VkDeviceSize normalized_offset = 0U;
  VkDeviceSize normalized_size = 0U;
  const AllocationStatus status =
      normalize_range(offset, size, &normalized_offset, &normalized_size);
  if (status != AllocationStatus::success ||
      (allocation_.memory_properties & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) != 0U) {
    return status;
  }
  VkMappedMemoryRange range{};
  range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
  range.memory = allocation_.memory;
  range.offset = normalized_offset;
  range.size = normalized_size;
  return map_result(vkFlushMappedMemoryRanges(context_->device_handle(), 1U, &range));
}

AllocationStatus VulkanStagingBuffer::invalidate(VkDeviceSize offset, VkDeviceSize size) noexcept {
  VkDeviceSize normalized_offset = 0U;
  VkDeviceSize normalized_size = 0U;
  const AllocationStatus status =
      normalize_range(offset, size, &normalized_offset, &normalized_size);
  if (status != AllocationStatus::success ||
      (allocation_.memory_properties & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) != 0U) {
    return status;
  }
  VkMappedMemoryRange range{};
  range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
  range.memory = allocation_.memory;
  range.offset = normalized_offset;
  range.size = normalized_size;
  return map_result(vkInvalidateMappedMemoryRanges(context_->device_handle(), 1U, &range));
}

void VulkanStagingBuffer::destroy() noexcept {
  if (context_ == nullptr || context_->device_handle() == VK_NULL_HANDLE) {
    allocation_ = {};
    return;
  }
  if (allocation_.mapped != nullptr && allocation_.memory != VK_NULL_HANDLE) {
    vkUnmapMemory(context_->device_handle(), allocation_.memory);
  }
  if (allocation_.memory != VK_NULL_HANDLE) {
    vkFreeMemory(context_->device_handle(), allocation_.memory, nullptr);
  }
  if (allocation_.buffer != VK_NULL_HANDLE) {
    vkDestroyBuffer(context_->device_handle(), allocation_.buffer, nullptr);
  }
  allocation_ = {};
}

const char* allocation_status_string(AllocationStatus status) noexcept {
  switch (status) {
  case AllocationStatus::success:
    return "success";
  case AllocationStatus::invalid_argument:
    return "invalid-argument";
  case AllocationStatus::not_ready:
    return "not-ready";
  case AllocationStatus::unsupported:
    return "unsupported";
  case AllocationStatus::out_of_memory:
    return "out-of-memory";
  case AllocationStatus::device_lost:
    return "device-lost";
  case AllocationStatus::range_out_of_bounds:
    return "range-out-of-bounds";
  }
  return "unknown";
}

} // namespace metaflux::backend::vulkan
