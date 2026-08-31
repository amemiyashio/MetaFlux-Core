#include "../src/vulkan_device.hpp"

#include "metaflux/backend/vulkan_capability.hpp"

#include <cstdint>
#include <cstdio>

int main() {
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

  std::printf("vulkan device context: success queue-family=%u timeline=%llu\n",
              context.queue_family_index(),
              static_cast<unsigned long long>(context.last_completed_value()));
  context.reset();
  return !context.ready() && context.submit_signal(42U, 3U) == DeviceStatus::not_ready ? 0 : 7;
}
