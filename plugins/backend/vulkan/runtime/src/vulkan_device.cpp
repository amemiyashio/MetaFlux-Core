#include "vulkan_device.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <vector>

namespace metaflux::backend::vulkan {

namespace {

constexpr std::uint32_t kRequiredFeatures = MF_VULKAN_FEATURE_TIMELINE_SEMAPHORE |
                                            MF_VULKAN_FEATURE_SYNCHRONIZATION2 |
                                            MF_VULKAN_FEATURE_BUFFER_DEVICE_ADDRESS;

bool nonzero_uuid(const std::uint8_t* uuid) noexcept {
  return std::any_of(uuid, uuid + VK_UUID_SIZE, [](std::uint8_t byte) { return byte != 0U; });
}

bool nonzero_digest(const std::uint8_t* digest) noexcept {
  return std::any_of(digest, digest + 32U, [](std::uint8_t byte) { return byte != 0U; });
}

bool compute_queue_matches(VkPhysicalDevice device, std::uint32_t family_index,
                           std::uint32_t expected_count) {
  std::uint32_t count = 0U;
  vkGetPhysicalDeviceQueueFamilyProperties(device, &count, nullptr);
  if (family_index >= count) {
    return false;
  }
  std::vector<VkQueueFamilyProperties> families(count);
  vkGetPhysicalDeviceQueueFamilyProperties(device, &count, families.data());
  const auto& family = families[family_index];
  return (family.queueFlags & VK_QUEUE_COMPUTE_BIT) != 0U && family.queueCount != 0U &&
         family.queueCount == expected_count;
}

void memory_totals(VkPhysicalDevice device, std::uint32_t* out_heap_count,
                   std::uint32_t* out_type_count, std::uint64_t* out_device_local,
                   std::uint64_t* out_host_visible) noexcept {
  VkPhysicalDeviceMemoryProperties memory{};
  vkGetPhysicalDeviceMemoryProperties(device, &memory);
  std::array<bool, VK_MAX_MEMORY_HEAPS> host_visible{};
  for (std::uint32_t index = 0U; index < memory.memoryTypeCount; ++index) {
    const auto& type = memory.memoryTypes[index];
    if (type.heapIndex < VK_MAX_MEMORY_HEAPS &&
        (type.propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0U) {
      host_visible[type.heapIndex] = true;
    }
  }
  *out_heap_count = memory.memoryHeapCount;
  *out_type_count = memory.memoryTypeCount;
  *out_device_local = 0U;
  *out_host_visible = 0U;
  for (std::uint32_t index = 0U; index < memory.memoryHeapCount; ++index) {
    const auto& heap = memory.memoryHeaps[index];
    if ((heap.flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) != 0U) {
      *out_device_local += heap.size;
    }
    if (host_visible[index]) {
      *out_host_visible += heap.size;
    }
  }
}

enum class SelectionResult : std::uint32_t {
  success = 0,
  no_device = 1,
  unsupported = 2,
};

SelectionResult select_profile_device(VkInstance instance,
                                      const mf_vulkan_capability_profile_v1& profile,
                                      VkPhysicalDevice* out_device) {
  std::uint32_t device_count = 0U;
  const auto enumerate_result = vkEnumeratePhysicalDevices(instance, &device_count, nullptr);
  if (enumerate_result != VK_SUCCESS || device_count == 0U) {
    return SelectionResult::no_device;
  }

  std::vector<VkPhysicalDevice> devices(device_count);
  if (vkEnumeratePhysicalDevices(instance, &device_count, devices.data()) != VK_SUCCESS) {
    return SelectionResult::no_device;
  }

  bool identity_match = false;
  for (const auto device : devices) {
    VkPhysicalDeviceIDProperties ids{};
    ids.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES;
    VkPhysicalDeviceSubgroupProperties subgroup{};
    subgroup.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES;
    subgroup.pNext = &ids;
    VkPhysicalDeviceProperties2 properties{};
    properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    properties.pNext = &subgroup;
    vkGetPhysicalDeviceProperties2(device, &properties);

    if (properties.properties.vendorID != profile.vendor_id ||
        properties.properties.deviceID != profile.device_id || !nonzero_uuid(profile.device_uuid) ||
        std::memcmp(ids.deviceUUID, profile.device_uuid, VK_UUID_SIZE) != 0) {
      continue;
    }
    identity_match = true;

    VkPhysicalDeviceVulkan12Features features12{};
    features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    VkPhysicalDeviceVulkan13Features features13{};
    features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    features12.pNext = &features13;
    VkPhysicalDeviceFeatures2 features{};
    features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    features.pNext = &features12;
    vkGetPhysicalDeviceFeatures2(device, &features);

    if (properties.properties.apiVersion != profile.api_version ||
        properties.properties.driverVersion != profile.driver_version ||
        features12.timelineSemaphore == VK_FALSE || features13.synchronization2 == VK_FALSE ||
        features12.bufferDeviceAddress == VK_FALSE ||
        !compute_queue_matches(device, profile.queue_family_index, profile.queue_count) ||
        subgroup.subgroupSize != profile.subgroup_size_min ||
        subgroup.subgroupSize != profile.subgroup_size_max ||
        properties.properties.limits.maxComputeWorkGroupInvocations !=
            profile.max_compute_workgroup_invocations ||
        properties.properties.limits.maxComputeWorkGroupSize[0] !=
            profile.max_compute_workgroup_size[0] ||
        properties.properties.limits.maxComputeWorkGroupSize[1] !=
            profile.max_compute_workgroup_size[1] ||
        properties.properties.limits.maxComputeWorkGroupSize[2] !=
            profile.max_compute_workgroup_size[2] ||
        properties.properties.limits.maxStorageBufferRange != profile.max_storage_buffer_range ||
        properties.properties.limits.maxUniformBufferRange != profile.max_uniform_buffer_range) {
      continue;
    }

    std::uint32_t heap_count = 0U;
    std::uint32_t type_count = 0U;
    std::uint64_t device_local_bytes = 0U;
    std::uint64_t host_visible_bytes = 0U;
    memory_totals(device, &heap_count, &type_count, &device_local_bytes, &host_visible_bytes);
    const auto staging = device_local_bytes != 0U && host_visible_bytes != 0U
                             ? MF_VULKAN_MEMORY_TIER_STAGING
                             : UINT32_C(0);
    if (heap_count != profile.memory_heap_count || type_count != profile.memory_type_count ||
        device_local_bytes != profile.device_local_heap_bytes ||
        host_visible_bytes != profile.host_visible_heap_bytes ||
        (profile.memory_tier_flags & MF_VULKAN_MEMORY_TIER_STAGING) != staging) {
      continue;
    }
    *out_device = device;
    return SelectionResult::success;
  }

  return identity_match ? SelectionResult::unsupported : SelectionResult::no_device;
}

bool valid_profile(const mf_vulkan_capability_profile_v1& profile) noexcept {
  return profile.struct_size == sizeof(profile) &&
         profile.abi_version == MF_VULKAN_CAPABILITY_ABI_VERSION_1 &&
         profile.status == MF_VULKAN_PROBE_SUCCESS && profile.api_version >= VK_API_VERSION_1_3 &&
         profile.queue_count != 0U && profile.vendor_id != 0U &&
         (profile.feature_flags & kRequiredFeatures) == kRequiredFeatures &&
         (profile.memory_tier_flags & MF_VULKAN_MEMORY_TIER_STAGING) != 0U &&
         profile.target_environment[0] != '\0' && nonzero_uuid(profile.device_uuid) &&
         nonzero_digest(profile.target_digest);
}

} // namespace

VulkanDeviceContext::~VulkanDeviceContext() noexcept { destroy_handles(); }

void VulkanDeviceContext::destroy_handles() noexcept {
  if (timeline_ != VK_NULL_HANDLE && device_ != VK_NULL_HANDLE) {
    vkDestroySemaphore(device_, timeline_, nullptr);
  }
  timeline_ = VK_NULL_HANDLE;
  if (device_ != VK_NULL_HANDLE) {
    vkDestroyDevice(device_, nullptr);
  }
  device_ = VK_NULL_HANDLE;
  queue_ = VK_NULL_HANDLE;
  physical_device_ = VK_NULL_HANDLE;
  queue_family_index_ = UINT32_MAX;
  non_coherent_atom_size_ = 1U;
  if (instance_ != VK_NULL_HANDLE) {
    vkDestroyInstance(instance_, nullptr);
  }
  instance_ = VK_NULL_HANDLE;
  last_submitted_ = 0U;
  last_completed_ = 0U;
}

void VulkanDeviceContext::reset() noexcept { destroy_handles(); }

DeviceStatus VulkanDeviceContext::map_initialization_result(VkResult result) noexcept {
  switch (result) {
  case VK_SUCCESS:
    return DeviceStatus::success;
  case VK_ERROR_INCOMPATIBLE_DRIVER:
  case VK_ERROR_INITIALIZATION_FAILED:
    return DeviceStatus::no_device;
  case VK_ERROR_FEATURE_NOT_PRESENT:
    return DeviceStatus::unsupported_features;
  case VK_ERROR_OUT_OF_HOST_MEMORY:
  case VK_ERROR_OUT_OF_DEVICE_MEMORY:
    return DeviceStatus::busy;
  default:
    return DeviceStatus::initialization_failed;
  }
}

DeviceStatus VulkanDeviceContext::map_runtime_result(VkResult result) noexcept {
  switch (result) {
  case VK_SUCCESS:
    return DeviceStatus::success;
  case VK_TIMEOUT:
    return DeviceStatus::busy;
  case VK_ERROR_DEVICE_LOST:
    lost_ = true;
    return DeviceStatus::device_lost;
  case VK_ERROR_OUT_OF_HOST_MEMORY:
  case VK_ERROR_OUT_OF_DEVICE_MEMORY:
    return DeviceStatus::busy;
  default:
    return DeviceStatus::initialization_failed;
  }
}

DeviceStatus
VulkanDeviceContext::initialize(const mf_vulkan_capability_profile_v1& profile) noexcept {
  if (generation_ == 0U || !valid_profile(profile)) {
    return DeviceStatus::invalid_argument;
  }
  if (lost_) {
    return DeviceStatus::device_lost;
  }
  if (ready()) {
    return DeviceStatus::busy;
  }
  destroy_handles();

  try {
    std::uint32_t loader_api_version = VK_API_VERSION_1_0;
    const auto loader_result = vkEnumerateInstanceVersion(&loader_api_version);
    if (loader_result != VK_SUCCESS) {
      const auto status = map_initialization_result(loader_result);
      return status == DeviceStatus::success ? DeviceStatus::initialization_failed : status;
    }
    if (loader_api_version < VK_API_VERSION_1_3) {
      return DeviceStatus::unsupported_features;
    }

    VkApplicationInfo application{};
    application.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    application.pApplicationName = "MetaFlux Vulkan backend";
    application.applicationVersion = 1U;
    application.pEngineName = "MetaFlux";
    application.engineVersion = 1U;
    application.apiVersion = VK_API_VERSION_1_3;

    VkInstanceCreateInfo instance_info{};
    instance_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instance_info.pApplicationInfo = &application;
    const auto instance_result = vkCreateInstance(&instance_info, nullptr, &instance_);
    const auto instance_status = map_initialization_result(instance_result);
    if (instance_status != DeviceStatus::success) {
      destroy_handles();
      return instance_status;
    }

    const auto selection = select_profile_device(instance_, profile, &physical_device_);
    if (selection != SelectionResult::success) {
      const auto status = selection == SelectionResult::no_device
                              ? DeviceStatus::no_device
                              : DeviceStatus::unsupported_features;
      destroy_handles();
      return status;
    }
    queue_family_index_ = profile.queue_family_index;
    VkPhysicalDeviceProperties selected_properties{};
    vkGetPhysicalDeviceProperties(physical_device_, &selected_properties);
    non_coherent_atom_size_ =
        std::max<VkDeviceSize>(selected_properties.limits.nonCoherentAtomSize, 1U);

    const float queue_priority = 1.0F;
    VkDeviceQueueCreateInfo queue_info{};
    queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_info.queueFamilyIndex = queue_family_index_;
    queue_info.queueCount = 1U;
    queue_info.pQueuePriorities = &queue_priority;

    VkPhysicalDeviceVulkan13Features enabled13{};
    enabled13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    enabled13.synchronization2 = VK_TRUE;
    VkPhysicalDeviceVulkan12Features enabled12{};
    enabled12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    enabled12.pNext = &enabled13;
    enabled12.timelineSemaphore = VK_TRUE;
    enabled12.bufferDeviceAddress = VK_TRUE;

    VkDeviceCreateInfo device_info{};
    device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_info.pNext = &enabled12;
    device_info.queueCreateInfoCount = 1U;
    device_info.pQueueCreateInfos = &queue_info;
    const auto device_result = vkCreateDevice(physical_device_, &device_info, nullptr, &device_);
    const auto device_status = map_initialization_result(device_result);
    if (device_status != DeviceStatus::success) {
      destroy_handles();
      return device_status;
    }
    vkGetDeviceQueue(device_, queue_family_index_, 0U, &queue_);
    if (queue_ == VK_NULL_HANDLE) {
      destroy_handles();
      return DeviceStatus::initialization_failed;
    }

    VkSemaphoreTypeCreateInfo type_info{};
    type_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
    type_info.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
    type_info.initialValue = 0U;
    VkSemaphoreCreateInfo semaphore_info{};
    semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    semaphore_info.pNext = &type_info;
    const auto semaphore_result = vkCreateSemaphore(device_, &semaphore_info, nullptr, &timeline_);
    const auto semaphore_status = map_initialization_result(semaphore_result);
    if (semaphore_status != DeviceStatus::success) {
      destroy_handles();
      return semaphore_status;
    }
    return DeviceStatus::success;
  } catch (...) {
    destroy_handles();
    return DeviceStatus::initialization_failed;
  }
}

DeviceStatus VulkanDeviceContext::submit_signal(std::uint64_t generation,
                                                std::uint64_t value) noexcept {
  if (generation == 0U) {
    return DeviceStatus::invalid_argument;
  }
  if (generation != generation_) {
    return DeviceStatus::stale_generation;
  }
  if (lost_) {
    return DeviceStatus::device_lost;
  }
  if (!ready()) {
    return DeviceStatus::not_ready;
  }
  if (value == 0U || value == std::numeric_limits<std::uint64_t>::max() ||
      value <= last_submitted_ || value <= last_completed_) {
    return DeviceStatus::invalid_timeline;
  }

  VkSemaphoreSubmitInfo signal_info{};
  signal_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
  signal_info.semaphore = timeline_;
  signal_info.value = value;
  signal_info.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
  VkSubmitInfo2 submit_info{};
  submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
  submit_info.signalSemaphoreInfoCount = 1U;
  submit_info.pSignalSemaphoreInfos = &signal_info;
  const auto result = vkQueueSubmit2(queue_, 1U, &submit_info, VK_NULL_HANDLE);
  const auto status = map_runtime_result(result);
  if (status == DeviceStatus::success) {
    last_submitted_ = value;
  }
  return status;
}

DeviceStatus VulkanDeviceContext::wait(std::uint64_t generation, std::uint64_t value,
                                       std::uint64_t timeout_ns) noexcept {
  if (generation == 0U) {
    return DeviceStatus::invalid_argument;
  }
  if (generation != generation_) {
    return DeviceStatus::stale_generation;
  }
  if (lost_) {
    return DeviceStatus::device_lost;
  }
  if (!ready()) {
    return DeviceStatus::not_ready;
  }
  if (value == 0U || value > last_submitted_) {
    return DeviceStatus::invalid_timeline;
  }
  if (value <= last_completed_) {
    return DeviceStatus::success;
  }

  VkSemaphoreWaitInfo wait_info{};
  wait_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
  wait_info.semaphoreCount = 1U;
  wait_info.pSemaphores = &timeline_;
  wait_info.pValues = &value;
  const auto status = map_runtime_result(vkWaitSemaphores(device_, &wait_info, timeout_ns));
  if (status == DeviceStatus::success) {
    last_completed_ = std::max(last_completed_, value);
  }
  return status;
}

DeviceStatus VulkanDeviceContext::poll(std::uint64_t generation,
                                       std::uint64_t* out_value) noexcept {
  if (out_value == nullptr || generation == 0U) {
    return DeviceStatus::invalid_argument;
  }
  if (generation != generation_) {
    return DeviceStatus::stale_generation;
  }
  if (lost_) {
    return DeviceStatus::device_lost;
  }
  if (!ready()) {
    return DeviceStatus::not_ready;
  }

  std::uint64_t observed = 0U;
  const auto status = map_runtime_result(vkGetSemaphoreCounterValue(device_, timeline_, &observed));
  if (status != DeviceStatus::success) {
    return status;
  }
  if (observed > last_submitted_) {
    return DeviceStatus::invalid_timeline;
  }
  last_completed_ = std::max(last_completed_, observed);
  *out_value = last_completed_;
  return DeviceStatus::success;
}

const char* device_status_string(DeviceStatus status) noexcept {
  switch (status) {
  case DeviceStatus::success:
    return "success";
  case DeviceStatus::invalid_argument:
    return "invalid-argument";
  case DeviceStatus::stale_generation:
    return "stale-generation";
  case DeviceStatus::no_device:
    return "no-device";
  case DeviceStatus::unsupported_features:
    return "unsupported-features";
  case DeviceStatus::initialization_failed:
    return "initialization-failed";
  case DeviceStatus::not_ready:
    return "not-ready";
  case DeviceStatus::busy:
    return "busy";
  case DeviceStatus::invalid_timeline:
    return "invalid-timeline";
  case DeviceStatus::device_lost:
    return "device-lost";
  }
  return "unknown";
}

} // namespace metaflux::backend::vulkan
