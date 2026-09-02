#include "vulkan_admission.hpp"

#include <algorithm>
#include <cstring>

namespace metaflux::backend::vulkan {
namespace {

constexpr std::uint32_t kRequiredFeatures = MF_VULKAN_FEATURE_TIMELINE_SEMAPHORE |
                                            MF_VULKAN_FEATURE_SYNCHRONIZATION2 |
                                            MF_VULKAN_FEATURE_BUFFER_DEVICE_ADDRESS;

bool nonzero_bytes(const std::uint8_t* bytes, std::size_t count) noexcept {
  return bytes != nullptr &&
         std::any_of(bytes, bytes + count, [](std::uint8_t byte) { return byte != 0U; });
}

bool zero_bytes(const std::uint8_t* bytes, std::size_t count) noexcept {
  return bytes != nullptr &&
         std::all_of(bytes, bytes + count, [](std::uint8_t byte) { return byte == 0U; });
}

bool terminated_string(const char* bytes, std::size_t count) noexcept {
  return bytes != nullptr && bytes[0] != '\0' && std::memchr(bytes, '\0', count) != nullptr;
}

bool valid_transport(VulkanTransport transport) noexcept {
  return transport == VulkanTransport::memfd || transport == VulkanTransport::local_cdev ||
         transport == VulkanTransport::vfio_user;
}

AdmissionStatus map_device_status(DeviceStatus status) noexcept {
  switch (status) {
  case DeviceStatus::success:
    return AdmissionStatus::success;
  case DeviceStatus::invalid_argument:
    return AdmissionStatus::invalid_argument;
  case DeviceStatus::stale_generation:
    return AdmissionStatus::stale_generation;
  case DeviceStatus::no_device:
    return AdmissionStatus::no_device;
  case DeviceStatus::unsupported_features:
    return AdmissionStatus::unsupported_features;
  case DeviceStatus::initialization_failed:
    return AdmissionStatus::initialization_failed;
  case DeviceStatus::busy:
    return AdmissionStatus::busy;
  case DeviceStatus::device_lost:
    return AdmissionStatus::device_lost;
  case DeviceStatus::not_ready:
  case DeviceStatus::invalid_timeline:
    return AdmissionStatus::initialization_failed;
  }
  return AdmissionStatus::initialization_failed;
}

} // namespace

bool valid_capability_profile(const mf_vulkan_capability_profile_v1& profile) noexcept {
  return profile.struct_size == sizeof(profile) &&
         profile.abi_version == MF_VULKAN_CAPABILITY_ABI_VERSION_1 &&
         profile.status == MF_VULKAN_PROBE_SUCCESS &&
         profile.api_version >= MF_VULKAN_API_VERSION_1_3 &&
         profile.queue_family_index != UINT32_MAX && profile.queue_count != 0U &&
         profile.subgroup_size_min != 0U &&
         profile.subgroup_size_min <= profile.subgroup_size_max &&
         profile.max_compute_workgroup_invocations != 0U &&
         profile.max_compute_workgroup_size[0] != 0U &&
         profile.max_compute_workgroup_size[1] != 0U &&
         profile.max_compute_workgroup_size[2] != 0U &&
         profile.max_storage_buffer_range != 0U && profile.max_uniform_buffer_range != 0U &&
         profile.vendor_id != 0U &&
         (profile.feature_flags & kRequiredFeatures) == kRequiredFeatures &&
         (profile.memory_tier_flags & MF_VULKAN_MEMORY_TIER_STAGING) != 0U &&
         profile.memory_heap_count != 0U && profile.memory_type_count != 0U &&
         profile.device_local_heap_bytes != 0U && profile.host_visible_heap_bytes != 0U &&
         (profile.feature_flags & ~MF_VULKAN_KNOWN_FEATURE_FLAGS) == 0U &&
         (profile.memory_tier_flags & ~MF_VULKAN_KNOWN_MEMORY_TIER_FLAGS) == 0U &&
         terminated_string(profile.target_environment, sizeof(profile.target_environment)) &&
         nonzero_bytes(profile.device_uuid, sizeof(profile.device_uuid)) &&
         nonzero_bytes(profile.driver_uuid, sizeof(profile.driver_uuid)) &&
         nonzero_bytes(profile.pipeline_cache_uuid, sizeof(profile.pipeline_cache_uuid)) &&
         nonzero_bytes(profile.target_digest, sizeof(profile.target_digest)) &&
         zero_bytes(profile.reserved, sizeof(profile.reserved));
}

AdmissionStatus VulkanBackendAdmission::admit(const mf_vulkan_capability_profile_v1& profile,
                                              std::uint64_t generation,
                                              VulkanTransport transport) noexcept {
  if (generation == 0U || !valid_capability_profile(profile) || !valid_transport(transport)) {
    return !valid_transport(transport) ? AdmissionStatus::unsupported_transport
                                       : AdmissionStatus::invalid_argument;
  }
  if (state_ == State::active) {
    return generation == generation_ ? AdmissionStatus::busy : AdmissionStatus::stale_generation;
  }
  if (state_ == State::retired) {
    return AdmissionStatus::stale_generation;
  }
  if (state_ == State::admitted) {
    if (generation != generation_) {
      return AdmissionStatus::stale_generation;
    }
    return std::memcmp(&profile_, &profile, sizeof(profile_)) == 0 && transport_ == transport
               ? AdmissionStatus::already_admitted
               : AdmissionStatus::invalid_argument;
  }
  profile_ = profile;
  generation_ = generation;
  transport_ = transport;
  context_.emplace(generation);
  state_ = State::admitted;
  return AdmissionStatus::success;
}

AdmissionStatus VulkanBackendAdmission::activate() noexcept {
  if (state_ == State::active) {
    return AdmissionStatus::busy;
  }
  if (state_ != State::admitted || !context_.has_value()) {
    return state_ == State::retired ? AdmissionStatus::stale_generation
                                    : AdmissionStatus::invalid_argument;
  }
  const AdmissionStatus status = map_device_status(context_->initialize(profile_));
  if (status == AdmissionStatus::success) {
    state_ = State::active;
  }
  return status;
}

AdmissionStatus VulkanBackendAdmission::retire() noexcept {
  if (state_ == State::empty) {
    return AdmissionStatus::invalid_argument;
  }
  if (state_ == State::retired) {
    return AdmissionStatus::stale_generation;
  }
  if (context_.has_value()) {
    context_->reset();
  }
  state_ = State::retired;
  return AdmissionStatus::success;
}

VulkanDeviceContext* VulkanBackendAdmission::context() noexcept {
  return state_ == State::active && context_.has_value() ? &context_.value() : nullptr;
}

const VulkanDeviceContext* VulkanBackendAdmission::context() const noexcept {
  return state_ == State::active && context_.has_value() ? &context_.value() : nullptr;
}

const char* admission_status_string(AdmissionStatus status) noexcept {
  switch (status) {
  case AdmissionStatus::success:
    return "success";
  case AdmissionStatus::invalid_argument:
    return "invalid-argument";
  case AdmissionStatus::already_admitted:
    return "already-admitted";
  case AdmissionStatus::stale_generation:
    return "stale-generation";
  case AdmissionStatus::unsupported_transport:
    return "unsupported-transport";
  case AdmissionStatus::no_device:
    return "no-device";
  case AdmissionStatus::unsupported_features:
    return "unsupported-features";
  case AdmissionStatus::initialization_failed:
    return "initialization-failed";
  case AdmissionStatus::busy:
    return "busy";
  case AdmissionStatus::device_lost:
    return "device-lost";
  }
  return "unknown";
}

} // namespace metaflux::backend::vulkan
