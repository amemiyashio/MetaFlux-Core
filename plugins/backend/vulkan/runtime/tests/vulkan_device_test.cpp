#include "../src/vulkan_device.hpp"
#include "../src/vulkan_staging.hpp"

#include "../src/vulkan_admission.hpp"
#include "../src/vulkan_device_copy.hpp"
#include "metaflux/backend/vulkan_capability.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <atomic>
#include <limits>
#include <thread>

namespace {

mf_vulkan_capability_profile_v1 admission_profile() {
  mf_vulkan_capability_profile_v1 profile{};
  profile.struct_size = sizeof(profile);
  profile.abi_version = MF_VULKAN_CAPABILITY_ABI_VERSION_1;
  profile.status = MF_VULKAN_PROBE_SUCCESS;
  profile.api_version = MF_VULKAN_API_VERSION_1_3;
  profile.vendor_id = 1U;
  profile.queue_family_index = 0U;
  profile.queue_count = 1U;
  profile.subgroup_size_min = 32U;
  profile.subgroup_size_max = 32U;
  profile.max_compute_workgroup_invocations = 1024U;
  profile.max_compute_workgroup_size[0] = 1024U;
  profile.max_compute_workgroup_size[1] = 1024U;
  profile.max_compute_workgroup_size[2] = 64U;
  profile.max_storage_buffer_range = 1U;
  profile.max_uniform_buffer_range = 1U;
  profile.feature_flags = MF_VULKAN_FEATURE_TIMELINE_SEMAPHORE |
                          MF_VULKAN_FEATURE_SYNCHRONIZATION2 |
                          MF_VULKAN_FEATURE_BUFFER_DEVICE_ADDRESS;
  profile.memory_tier_flags = MF_VULKAN_MEMORY_TIER_STAGING;
  profile.memory_heap_count = 1U;
  profile.memory_type_count = 1U;
  profile.device_local_heap_bytes = 1U;
  profile.host_visible_heap_bytes = 1U;
  profile.target_environment[0] = 'x';
  profile.device_uuid[0] = 1U;
  profile.driver_uuid[0] = 1U;
  profile.pipeline_cache_uuid[0] = 1U;
  profile.target_digest[0] = 1U;
  return profile;
}

bool admission_guards() {
  using metaflux::backend::vulkan::AdmissionStatus;
  using metaflux::backend::vulkan::VulkanBackendAdmission;
  using metaflux::backend::vulkan::VulkanTransport;
  const auto profile = admission_profile();

  auto incomplete = profile;
  incomplete.max_compute_workgroup_size[0] = 0U;
  VulkanBackendAdmission incomplete_admission;
  if (incomplete_admission.admit(incomplete, 42U, VulkanTransport::local_cdev) !=
      AdmissionStatus::invalid_argument) {
    return false;
  }
  auto missing_identity = profile;
  missing_identity.driver_uuid[0] = 0U;
  VulkanBackendAdmission missing_identity_admission;
  if (missing_identity_admission.admit(missing_identity, 42U, VulkanTransport::local_cdev) !=
      AdmissionStatus::invalid_argument) {
    return false;
  }
  auto reserved = profile;
  reserved.reserved[0] = 1U;
  VulkanBackendAdmission reserved_admission;
  if (reserved_admission.admit(reserved, 42U, VulkanTransport::local_cdev) !=
      AdmissionStatus::invalid_argument) {
    return false;
  }
  auto unknown_feature = profile;
  unknown_feature.feature_flags |= (UINT32_C(1) << 31U);
  VulkanBackendAdmission unknown_feature_admission;
  if (unknown_feature_admission.admit(unknown_feature, 42U, VulkanTransport::local_cdev) !=
      AdmissionStatus::invalid_argument) {
    return false;
  }
  auto unknown_memory_tier = profile;
  unknown_memory_tier.memory_tier_flags |= (UINT32_C(1) << 31U);
  VulkanBackendAdmission unknown_memory_tier_admission;
  if (unknown_memory_tier_admission.admit(unknown_memory_tier, 42U,
                                          VulkanTransport::local_cdev) !=
      AdmissionStatus::invalid_argument) {
    return false;
  }

  VulkanBackendAdmission admission;
  if (admission.admit(profile, 42U, VulkanTransport::local_cdev) != AdmissionStatus::success ||
      admission.admit(profile, 43U, VulkanTransport::local_cdev) !=
          AdmissionStatus::stale_generation ||
      admission.admit(profile, 42U, VulkanTransport::local_cdev) !=
          AdmissionStatus::already_admitted ||
      admission.context() != nullptr) {
    return false;
  }

  VulkanBackendAdmission invalid_transport;
  if (invalid_transport.admit(profile, 42U, static_cast<VulkanTransport>(UINT32_C(99))) !=
      AdmissionStatus::unsupported_transport) {
    return false;
  }

  const auto activation = admission.activate();
  if (activation != AdmissionStatus::success && activation != AdmissionStatus::no_device &&
      activation != AdmissionStatus::unsupported_features &&
      activation != AdmissionStatus::initialization_failed) {
    return false;
  }
  if (activation == AdmissionStatus::success &&
      (admission.context() == nullptr || admission.context()->generation() != 42U)) {
    return false;
  }
  if (admission.retire() != AdmissionStatus::success || admission.context() != nullptr ||
      admission.retire() != AdmissionStatus::stale_generation) {
    return false;
  }
  return true;
}

} // namespace

int main() {
  if (!admission_guards()) {
    return 1;
  }
  mf_vulkan_capability_profile_v1 profile{};
  const auto probe_status = metaflux::backend::vulkan::probe(&profile);
  if (probe_status == MF_VULKAN_PROBE_INVALID_ARGUMENT) {
    return 1;
  }
  if (probe_status == MF_VULKAN_PROBE_LOADER_UNAVAILABLE ||
      probe_status == MF_VULKAN_PROBE_NO_DEVICE ||
      probe_status == MF_VULKAN_PROBE_UNSUPPORTED_DEVICE) {
    std::printf("vulkan device context: %s (not qualified on this host)\n",
                mf_vulkan_probe_status_string_v1(probe_status));
    return 0;
  }
  if (probe_status != MF_VULKAN_PROBE_SUCCESS) {
    return 2;
  }

  using metaflux::backend::vulkan::DeviceStatus;
  using metaflux::backend::vulkan::VulkanDeviceContext;
  VulkanDeviceContext context(42U);
  mf_vulkan_capability_profile_v1 invalid = profile;
  invalid.vendor_id ^= 1U;
  const auto invalid_context = VulkanDeviceContext(43U).initialize(invalid);
  if (invalid_context != DeviceStatus::no_device &&
      invalid_context != DeviceStatus::unsupported_features) {
    return 3;
  }

  const auto initialized = context.initialize(profile);
  if (initialized == DeviceStatus::no_device || initialized == DeviceStatus::unsupported_features) {
    std::printf("vulkan device context: %s (profile unavailable on this host)\n",
                metaflux::backend::vulkan::device_status_string(initialized));
    return 0;
  }
  if (initialized != DeviceStatus::success || !context.ready() || context.generation() != 42U ||
      context.queue_family_index() != profile.queue_family_index ||
      context.queue_count() != profile.queue_count ||
      context.last_submitted_value() != 0U || context.last_completed_value() != 0U ||
      context.initialize(profile) != DeviceStatus::busy) {
    return 4;
  }

  std::uint64_t observed = 99U;
  if (context.poll(41U, &observed) != DeviceStatus::stale_generation ||
      context.poll(42U, nullptr) != DeviceStatus::invalid_argument ||
      context.submit_signal(42U, 0U) != DeviceStatus::invalid_timeline ||
      context.wait(42U, 1U, 0U) != DeviceStatus::invalid_timeline ||
      context.submit_signal(42U, 1U) != DeviceStatus::success ||
      context.submit_signal(42U, 1U) != DeviceStatus::invalid_timeline ||
      context.submit_signal(41U, 2U) != DeviceStatus::stale_generation) {
    return 5;
  }
  if (context.poll(42U, &observed) != DeviceStatus::success || observed > 1U ||
      context.wait(42U, 1U, UINT64_C(5000000000)) != DeviceStatus::success ||
      context.last_completed_value() < 1U ||
      context.poll(42U, &observed) != DeviceStatus::success || observed < 1U ||
      context.submit_signal(42U, 2U) != DeviceStatus::success ||
      context.wait(42U, 2U, UINT64_C(5000000000)) != DeviceStatus::success ||
      context.last_completed_value() < 2U) {
    return 6;
  }

  {
    metaflux::backend::vulkan::VulkanStagingBuffer staging(context);
    if (staging.allocate(4096U, 256U) != metaflux::backend::vulkan::AllocationStatus::success ||
        staging.allocation().buffer == VK_NULL_HANDLE ||
        staging.allocation().memory == VK_NULL_HANDLE ||
        staging.map() != metaflux::backend::vulkan::AllocationStatus::success ||
        staging.allocation().mapped == nullptr ||
        staging.flush(0U, 256U) != metaflux::backend::vulkan::AllocationStatus::success ||
        staging.invalidate(128U, 128U) != metaflux::backend::vulkan::AllocationStatus::success ||
        staging.flush(4096U, 1U) !=
            metaflux::backend::vulkan::AllocationStatus::range_out_of_bounds ||
        staging.allocate(0U, 256U) !=
            metaflux::backend::vulkan::AllocationStatus::invalid_argument) {
      return 7;
    }
    const VkBuffer original_buffer = staging.allocation().buffer;
    const VkDeviceMemory original_memory = staging.allocation().memory;
    if (staging.allocate(std::numeric_limits<VkDeviceSize>::max(), 256U) ==
            metaflux::backend::vulkan::AllocationStatus::success ||
        staging.allocation().buffer != original_buffer ||
        staging.allocation().memory != original_memory || !staging.ready()) {
      return 15;
    }
  }

  {
    metaflux::backend::vulkan::VulkanDeviceLocalCopy transfer(context);
    if (transfer.allocate(4096U, 256U) != metaflux::backend::vulkan::AllocationStatus::success ||
        !transfer.ready() ||
        transfer.map() != metaflux::backend::vulkan::AllocationStatus::success) {
      return 9;
    }
    auto* host = static_cast<std::uint8_t*>(transfer.host_allocation().mapped);
    for (std::size_t index = 0U; index < 4096U; ++index) {
      host[index] = static_cast<std::uint8_t>((index * 17U) & 0xffU);
    }
    if (transfer.upload(0U, 4096U, UINT64_C(5000000000)) !=
            metaflux::backend::vulkan::AllocationStatus::success ||
        transfer.download(0U, 4096U, UINT64_C(5000000000)) !=
            metaflux::backend::vulkan::AllocationStatus::success) {
      return 10;
    }
    for (std::size_t index = 0U; index < 4096U; ++index) {
      if (host[index] != static_cast<std::uint8_t>((index * 17U) & 0xffU)) {
        return 11;
      }
    }
    std::atomic<bool> concurrent_uploads_ok{true};
    std::thread first_upload([&] {
      if (transfer.upload(0U, 2048U, UINT64_C(5000000000)) !=
          metaflux::backend::vulkan::AllocationStatus::success) {
        concurrent_uploads_ok.store(false, std::memory_order_relaxed);
      }
    });
    std::thread second_upload([&] {
      if (transfer.upload(2048U, 2048U, UINT64_C(5000000000)) !=
          metaflux::backend::vulkan::AllocationStatus::success) {
        concurrent_uploads_ok.store(false, std::memory_order_relaxed);
      }
    });
    first_upload.join();
    second_upload.join();
    if (!concurrent_uploads_ok.load(std::memory_order_relaxed)) {
      return 12;
    }
    const VkBuffer original_device_buffer = transfer.device_allocation().buffer;
    const VkDeviceMemory original_device_memory = transfer.device_allocation().memory;
    const VkBuffer original_host_buffer = transfer.host_allocation().buffer;
    if (transfer.allocate(std::numeric_limits<VkDeviceSize>::max(), 256U) ==
            metaflux::backend::vulkan::AllocationStatus::success ||
        transfer.device_allocation().buffer != original_device_buffer ||
        transfer.device_allocation().memory != original_device_memory ||
        transfer.host_allocation().buffer != original_host_buffer || !transfer.ready()) {
      return 16;
    }
    std::atomic<bool> concurrent_downloads_ok{true};
    std::thread first_download([&] {
      if (transfer.download(0U, 2048U, UINT64_C(5000000000)) !=
          metaflux::backend::vulkan::AllocationStatus::success) {
        concurrent_downloads_ok.store(false, std::memory_order_relaxed);
      }
    });
    std::thread second_download([&] {
      if (transfer.download(2048U, 2048U, UINT64_C(5000000000)) !=
          metaflux::backend::vulkan::AllocationStatus::success) {
        concurrent_downloads_ok.store(false, std::memory_order_relaxed);
      }
    });
    first_download.join();
    second_download.join();
    if (!concurrent_downloads_ok.load(std::memory_order_relaxed)) {
      return 13;
    }
    if (transfer.upload(4096U, 1U, UINT64_C(5000000000)) !=
        metaflux::backend::vulkan::AllocationStatus::range_out_of_bounds) {
      return 14;
    }
  }

  std::printf("vulkan device context: success queue-family=%u timeline=%llu\n",
              context.queue_family_index(),
              static_cast<unsigned long long>(context.last_completed_value()));
  context.reset();
  return !context.ready() && context.submit_signal(42U, 3U) == DeviceStatus::not_ready ? 0 : 8;
}
