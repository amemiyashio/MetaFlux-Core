#include "metaflux/backend/vulkan_capability.hpp"

#include <vulkan/vulkan.h>

#include <array>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr std::array<std::uint32_t, 64> kSha256Constants{
    0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U, 0x3956c25bU, 0x59f111f1U,
    0x923f82a4U, 0xab1c5ed5U, 0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U,
    0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U, 0xc19bf174U, 0xe49b69c1U, 0xefbe4786U,
    0x0fc19dc6U, 0x240ca1ccU, 0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU,
    0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U, 0xc6e00bf3U, 0xd5a79147U,
    0x06ca6351U, 0x14292967U, 0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU, 0x53380d13U,
    0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U, 0xa2bfe8a1U, 0xa81a664bU,
    0xc24b8b70U, 0xc76c51a3U, 0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U,
    0x19a4c116U, 0x1e376c08U, 0x2748774cU, 0x34b0bcb5U, 0x391c0cb3U, 0x4ed8aa4aU,
    0x5b9cca4fU, 0x682e6ff3U, 0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U,
    0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U,
};

constexpr std::uint32_t rotate_right(std::uint32_t value, std::uint32_t amount) noexcept {
  return (value >> amount) | (value << (32U - amount));
}

std::array<std::uint8_t, 32> sha256(std::string_view input) {
  std::vector<std::uint8_t> bytes(input.begin(), input.end());
  const auto bit_length = static_cast<std::uint64_t>(bytes.size()) * 8U;
  bytes.push_back(0x80U);
  while (bytes.size() % 64U != 56U) {
    bytes.push_back(0U);
  }
  for (std::uint32_t shift = 56U;; shift -= 8U) {
    bytes.push_back(static_cast<std::uint8_t>((bit_length >> shift) & 0xffU));
    if (shift == 0U) {
      break;
    }
  }

  std::array<std::uint32_t, 8> state{
      0x6a09e667U, 0xbb67ae85U, 0x3c6ef372U, 0xa54ff53aU,
      0x510e527fU, 0x9b05688cU, 0x1f83d9abU, 0x5be0cd19U,
  };
  std::array<std::uint32_t, 64> words{};
  for (std::size_t block = 0; block < bytes.size(); block += 64U) {
    for (std::size_t index = 0; index < 16U; ++index) {
      const auto offset = block + index * 4U;
      words[index] = (static_cast<std::uint32_t>(bytes[offset]) << 24U) |
                     (static_cast<std::uint32_t>(bytes[offset + 1U]) << 16U) |
                     (static_cast<std::uint32_t>(bytes[offset + 2U]) << 8U) |
                     static_cast<std::uint32_t>(bytes[offset + 3U]);
    }
    for (std::size_t index = 16U; index < words.size(); ++index) {
      const auto value0 = words[index - 15U];
      const auto value1 = words[index - 2U];
      const auto sigma0 = rotate_right(value0, 7U) ^ rotate_right(value0, 18U) ^ (value0 >> 3U);
      const auto sigma1 = rotate_right(value1, 17U) ^ rotate_right(value1, 19U) ^ (value1 >> 10U);
      words[index] = words[index - 16U] + sigma0 + words[index - 7U] + sigma1;
    }

    auto a = state[0];
    auto b = state[1];
    auto c = state[2];
    auto d = state[3];
    auto e = state[4];
    auto f = state[5];
    auto g = state[6];
    auto h = state[7];
    for (std::size_t index = 0; index < words.size(); ++index) {
      const auto sum1 = rotate_right(e, 6U) ^ rotate_right(e, 11U) ^ rotate_right(e, 25U);
      const auto choose = (e & f) ^ ((~e) & g);
      const auto temporary1 = h + sum1 + choose + kSha256Constants[index] + words[index];
      const auto sum0 = rotate_right(a, 2U) ^ rotate_right(a, 13U) ^ rotate_right(a, 22U);
      const auto majority = (a & b) ^ (a & c) ^ (b & c);
      const auto temporary2 = sum0 + majority;
      h = g;
      g = f;
      f = e;
      e = d + temporary1;
      d = c;
      c = b;
      b = a;
      a = temporary1 + temporary2;
    }
    const std::array<std::uint32_t, 8> work{a, b, c, d, e, f, g, h};
    for (std::size_t index = 0; index < state.size(); ++index) {
      state[index] += work[index];
    }
  }

  std::array<std::uint8_t, 32> digest{};
  for (std::size_t index = 0; index < state.size(); ++index) {
    const auto word = state[index];
    digest[index * 4U] = static_cast<std::uint8_t>(word >> 24U);
    digest[index * 4U + 1U] = static_cast<std::uint8_t>(word >> 16U);
    digest[index * 4U + 2U] = static_cast<std::uint8_t>(word >> 8U);
    digest[index * 4U + 3U] = static_cast<std::uint8_t>(word);
  }
  return digest;
}

std::string hex_bytes(const std::uint8_t* bytes, std::size_t count) {
  constexpr char digits[] = "0123456789abcdef";
  std::string result;
  result.reserve(count * 2U);
  for (std::size_t index = 0; index < count; ++index) {
    result.push_back(digits[bytes[index] >> 4U]);
    result.push_back(digits[bytes[index] & 0x0fU]);
  }
  return result;
}

template <std::size_t N>
void copy_string(char (&destination)[N], std::string_view source) noexcept {
  const auto length = std::min(source.size(), N - 1U);
  std::memcpy(destination, source.data(), length);
  destination[length] = '\0';
}

bool has_compute_queue(VkPhysicalDevice device, std::uint32_t* out_index,
                       std::uint32_t* out_count) {
  std::uint32_t count = 0U;
  vkGetPhysicalDeviceQueueFamilyProperties(device, &count, nullptr);
  if (count == 0U) {
    return false;
  }
  std::vector<VkQueueFamilyProperties> properties(count);
  vkGetPhysicalDeviceQueueFamilyProperties(device, &count, properties.data());
  for (std::uint32_t index = 0U; index < count; ++index) {
    if ((properties[index].queueFlags & VK_QUEUE_COMPUTE_BIT) != 0U &&
        properties[index].queueCount > 0U) {
      *out_index = index;
      *out_count = properties[index].queueCount;
      return true;
    }
  }
  return false;
}

void populate_memory(const VkPhysicalDeviceMemoryProperties& memory,
                     mf_vulkan_capability_profile_v1* profile) noexcept {
  std::array<bool, VK_MAX_MEMORY_HEAPS> host_visible{};
  for (std::uint32_t index = 0U; index < memory.memoryTypeCount; ++index) {
    const auto& type = memory.memoryTypes[index];
    if ((type.propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0U &&
        type.heapIndex < VK_MAX_MEMORY_HEAPS) {
      host_visible[type.heapIndex] = true;
    }
  }
  for (std::uint32_t index = 0U; index < memory.memoryHeapCount; ++index) {
    const auto& heap = memory.memoryHeaps[index];
    if ((heap.flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) != 0U) {
      profile->device_local_heap_bytes += heap.size;
    }
    if (host_visible[index]) {
      profile->host_visible_heap_bytes += heap.size;
    }
  }
  profile->memory_heap_count = memory.memoryHeapCount;
  profile->memory_type_count = memory.memoryTypeCount;
  if (profile->device_local_heap_bytes != 0U && profile->host_visible_heap_bytes != 0U) {
    profile->memory_tier_flags |= MF_VULKAN_MEMORY_TIER_STAGING;
  }
}

std::string target_environment(const mf_vulkan_capability_profile_v1& profile) {
  std::ostringstream stream;
  stream << "schema=metaflux.vulkan.target.v1"
         << ";api=" << profile.api_version << ";driver=" << profile.driver_version
         << ";vendor=" << profile.vendor_id << ";device=" << profile.device_id
         << ";type=" << profile.device_type << ";queue-family=" << profile.queue_family_index
         << ";queue-count=" << profile.queue_count << ";subgroup-min=" << profile.subgroup_size_min
         << ";subgroup-max=" << profile.subgroup_size_max
         << ";workgroup-invocations=" << profile.max_compute_workgroup_invocations
         << ";workgroup-size=" << profile.max_compute_workgroup_size[0] << ','
         << profile.max_compute_workgroup_size[1] << ',' << profile.max_compute_workgroup_size[2]
         << ";features=" << profile.feature_flags << ";memory-tiers=" << profile.memory_tier_flags
         << ";device-local-bytes=" << profile.device_local_heap_bytes
         << ";host-visible-bytes=" << profile.host_visible_heap_bytes
         << ";device-uuid=" << hex_bytes(profile.device_uuid, sizeof(profile.device_uuid))
         << ";driver-uuid=" << hex_bytes(profile.driver_uuid, sizeof(profile.driver_uuid))
         << ";pipeline-cache-uuid="
         << hex_bytes(profile.pipeline_cache_uuid, sizeof(profile.pipeline_cache_uuid));
  return stream.str();
}

bool compatible_device(VkPhysicalDevice device, mf_vulkan_capability_profile_v1* profile) {
  VkPhysicalDeviceVulkan12Features features12{};
  features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
  VkPhysicalDeviceVulkan13Features features13{};
  features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
  features12.pNext = &features13;
  VkPhysicalDeviceFeatures2 features{};
  features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
  features.pNext = &features12;
  vkGetPhysicalDeviceFeatures2(device, &features);

  VkPhysicalDeviceIDProperties ids{};
  ids.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES;
  VkPhysicalDeviceSubgroupProperties subgroup{};
  subgroup.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES;
  subgroup.pNext = &ids;
  VkPhysicalDeviceProperties2 properties{};
  properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
  properties.pNext = &subgroup;
  vkGetPhysicalDeviceProperties2(device, &properties);

  if (properties.properties.apiVersion < VK_API_VERSION_1_3 ||
      features12.timelineSemaphore == VK_FALSE || features13.synchronization2 == VK_FALSE ||
      features12.bufferDeviceAddress == VK_FALSE || subgroup.subgroupSize == 0U ||
      !has_compute_queue(device, &profile->queue_family_index, &profile->queue_count)) {
    return false;
  }

  VkPhysicalDeviceMemoryProperties memory{};
  vkGetPhysicalDeviceMemoryProperties(device, &memory);
  profile->status = MF_VULKAN_PROBE_SUCCESS;
  profile->api_version = properties.properties.apiVersion;
  profile->driver_version = properties.properties.driverVersion;
  profile->vendor_id = properties.properties.vendorID;
  profile->device_id = properties.properties.deviceID;
  profile->device_type = static_cast<std::uint32_t>(properties.properties.deviceType);
  profile->subgroup_size_min = subgroup.subgroupSize;
  profile->subgroup_size_max = subgroup.subgroupSize;
  profile->max_compute_workgroup_invocations = properties.properties.limits.maxComputeWorkGroupInvocations;
  for (std::size_t index = 0U; index < 3U; ++index) {
    profile->max_compute_workgroup_size[index] = properties.properties.limits.maxComputeWorkGroupSize[index];
  }
  profile->max_storage_buffer_range = properties.properties.limits.maxStorageBufferRange;
  profile->max_uniform_buffer_range = properties.properties.limits.maxUniformBufferRange;
  profile->feature_flags = MF_VULKAN_FEATURE_TIMELINE_SEMAPHORE |
                           MF_VULKAN_FEATURE_SYNCHRONIZATION2 |
                           MF_VULKAN_FEATURE_BUFFER_DEVICE_ADDRESS;
  std::memcpy(profile->device_uuid, ids.deviceUUID, sizeof(profile->device_uuid));
  std::memcpy(profile->driver_uuid, ids.driverUUID, sizeof(profile->driver_uuid));
  std::memcpy(profile->pipeline_cache_uuid, properties.properties.pipelineCacheUUID,
              sizeof(profile->pipeline_cache_uuid));
  copy_string(profile->device_name, properties.properties.deviceName);
  populate_memory(memory, profile);
  const auto environment = target_environment(*profile);
  if (environment.size() >= sizeof(profile->target_environment)) {
    return false;
  }
  copy_string(profile->target_environment, environment);
  const auto digest = sha256(environment);
  std::memcpy(profile->target_digest, digest.data(), digest.size());
  return true;
}

} // namespace

extern "C" mf_vulkan_probe_status_v1 mf_vulkan_probe_capabilities_v1(
    mf_vulkan_capability_profile_v1* out_profile) {
  if (out_profile == nullptr) {
    return MF_VULKAN_PROBE_INVALID_ARGUMENT;
  }
  std::memset(out_profile, 0, sizeof(*out_profile));
  out_profile->struct_size = sizeof(*out_profile);
  out_profile->abi_version = MF_VULKAN_CAPABILITY_ABI_VERSION_1;

  VkInstance instance = VK_NULL_HANDLE;
  try {
    const auto enumerate_instance_version = reinterpret_cast<PFN_vkEnumerateInstanceVersion>(
        vkGetInstanceProcAddr(nullptr, "vkEnumerateInstanceVersion"));
    if (enumerate_instance_version == nullptr) {
      out_profile->status = MF_VULKAN_PROBE_LOADER_UNAVAILABLE;
      return MF_VULKAN_PROBE_LOADER_UNAVAILABLE;
    }
    std::uint32_t instance_version = VK_API_VERSION_1_0;
    if (enumerate_instance_version(&instance_version) != VK_SUCCESS ||
        instance_version < VK_API_VERSION_1_3) {
      out_profile->status = MF_VULKAN_PROBE_LOADER_UNAVAILABLE;
      return MF_VULKAN_PROBE_LOADER_UNAVAILABLE;
    }

    const VkApplicationInfo application{
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pNext = nullptr,
        .pApplicationName = "MetaFlux Vulkan capability probe",
        .applicationVersion = VK_MAKE_API_VERSION(0, 0, 1, 0),
        .pEngineName = "MetaFlux",
        .engineVersion = VK_MAKE_API_VERSION(0, 0, 1, 0),
        .apiVersion = VK_API_VERSION_1_3,
    };
    const VkInstanceCreateInfo create_info{
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0U,
        .pApplicationInfo = &application,
        .enabledLayerCount = 0U,
        .ppEnabledLayerNames = nullptr,
        .enabledExtensionCount = 0U,
        .ppEnabledExtensionNames = nullptr,
    };
    const auto create_result = vkCreateInstance(&create_info, nullptr, &instance);
    if (create_result != VK_SUCCESS) {
      // A loader with no discoverable ICD reports an incompatible driver. This
      // is a host qualification result, not a product initialization failure.
      if (create_result == VK_ERROR_INCOMPATIBLE_DRIVER) {
        out_profile->status = MF_VULKAN_PROBE_NO_DEVICE;
        return MF_VULKAN_PROBE_NO_DEVICE;
      }
      out_profile->status = MF_VULKAN_PROBE_INITIALIZATION_FAILED;
      return MF_VULKAN_PROBE_INITIALIZATION_FAILED;
    }

    std::uint32_t device_count = 0U;
    auto result = vkEnumeratePhysicalDevices(instance, &device_count, nullptr);
    if (result != VK_SUCCESS || device_count == 0U) {
      vkDestroyInstance(instance, nullptr);
      instance = VK_NULL_HANDLE;
      out_profile->status = result == VK_SUCCESS || result == VK_ERROR_INCOMPATIBLE_DRIVER
                                ? MF_VULKAN_PROBE_NO_DEVICE
                                : MF_VULKAN_PROBE_INITIALIZATION_FAILED;
      return static_cast<mf_vulkan_probe_status_v1>(out_profile->status);
    }
    std::vector<VkPhysicalDevice> devices(device_count);
    result = vkEnumeratePhysicalDevices(instance, &device_count, devices.data());
    if (result != VK_SUCCESS) {
      vkDestroyInstance(instance, nullptr);
      instance = VK_NULL_HANDLE;
      out_profile->status = MF_VULKAN_PROBE_INITIALIZATION_FAILED;
      return MF_VULKAN_PROBE_INITIALIZATION_FAILED;
    }
    bool found = false;
    for (const auto device : devices) {
      if (compatible_device(device, out_profile)) {
        found = true;
        break;
      }
    }
    vkDestroyInstance(instance, nullptr);
    instance = VK_NULL_HANDLE;
    if (!found) {
      std::memset(out_profile->target_environment, 0, sizeof(out_profile->target_environment));
      std::memset(out_profile->target_digest, 0, sizeof(out_profile->target_digest));
      out_profile->status = MF_VULKAN_PROBE_UNSUPPORTED_DEVICE;
      return MF_VULKAN_PROBE_UNSUPPORTED_DEVICE;
    }
    return MF_VULKAN_PROBE_SUCCESS;
  } catch (...) {
    if (instance != VK_NULL_HANDLE) {
      vkDestroyInstance(instance, nullptr);
    }
    out_profile->status = MF_VULKAN_PROBE_INTERNAL_ERROR;
    return MF_VULKAN_PROBE_INTERNAL_ERROR;
  }
}

extern "C" const char* mf_vulkan_probe_status_string_v1(mf_vulkan_probe_status_v1 status) {
  switch (status) {
  case MF_VULKAN_PROBE_SUCCESS:
    return "success";
  case MF_VULKAN_PROBE_INVALID_ARGUMENT:
    return "invalid-argument";
  case MF_VULKAN_PROBE_LOADER_UNAVAILABLE:
    return "loader-unavailable";
  case MF_VULKAN_PROBE_INITIALIZATION_FAILED:
    return "initialization-failed";
  case MF_VULKAN_PROBE_NO_DEVICE:
    return "no-device";
  case MF_VULKAN_PROBE_UNSUPPORTED_DEVICE:
    return "unsupported-device";
  case MF_VULKAN_PROBE_INTERNAL_ERROR:
    return "internal-error";
  }
  return "unknown";
}
