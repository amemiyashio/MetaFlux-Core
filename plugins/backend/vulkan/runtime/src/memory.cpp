#include "metaflux/backend/vulkan_memory.hpp"

#include <algorithm>
#include <limits>

namespace metaflux::backend::vulkan {

StagingLedger::StagingLedger(const mf_vulkan_capability_profile_v1& profile,
                             std::uint64_t generation) noexcept {
  (void)configure(profile, generation);
}

MemoryStatus StagingLedger::configure(const mf_vulkan_capability_profile_v1& profile,
                                      std::uint64_t generation) noexcept {
  allocations_.clear();
  active_bytes_ = 0;
  next_id_ = 1;
  capacity_ = 0;
  generation_ = generation;
  if (profile.status != MF_VULKAN_PROBE_SUCCESS || generation == 0U ||
      (profile.memory_tier_flags & MF_VULKAN_MEMORY_TIER_STAGING) == 0U ||
      profile.device_local_heap_bytes == 0U || profile.host_visible_heap_bytes == 0U) {
    return MemoryStatus::unsupported;
  }
  capacity_ = std::min(profile.device_local_heap_bytes, profile.host_visible_heap_bytes);
  return capacity_ == 0U ? MemoryStatus::unsupported : MemoryStatus::success;
}

bool StagingLedger::is_power_of_two(std::uint64_t value) noexcept {
  return value != 0U && (value & (value - 1U)) == 0U;
}

bool StagingLedger::round_up(std::uint64_t value, std::uint64_t alignment,
                             std::uint64_t* out) noexcept {
  if (out == nullptr || !is_power_of_two(alignment)) {
    return false;
  }
  const auto remainder = value & (alignment - 1U);
  const auto increment = remainder == 0U ? 0U : alignment - remainder;
  if (value > std::numeric_limits<std::uint64_t>::max() - increment) {
    return false;
  }
  *out = value + increment;
  return true;
}

MemoryStatus StagingLedger::allocate(std::uint64_t size, std::uint64_t alignment,
                                     std::uint64_t generation,
                                     StagingAllocation* out_allocation) {
  if (out_allocation == nullptr || size == 0U || generation == 0U || generation != generation_ ||
      capacity_ == 0U || !is_power_of_two(alignment)) {
    return generation != generation_ && generation != 0U ? MemoryStatus::stale_generation
                                                          : MemoryStatus::invalid_argument;
  }

  std::uint64_t candidate = 0U;
  for (const auto& allocation : allocations_) {
    if (!round_up(candidate, alignment, &candidate) || candidate > capacity_ ||
        size > capacity_ - candidate) {
      return MemoryStatus::out_of_memory;
    }
    const auto end = candidate + size;
    if (end <= allocation.offset) {
      break;
    }
    if (allocation.offset > std::numeric_limits<std::uint64_t>::max() - allocation.size) {
      return MemoryStatus::out_of_memory;
    }
    candidate = allocation.offset + allocation.size;
  }
  if (!round_up(candidate, alignment, &candidate) || candidate > capacity_ ||
      size > capacity_ - candidate) {
    return MemoryStatus::out_of_memory;
  }

  StagingAllocation allocation{
      .id = next_id_++,
      .generation = generation_,
      .offset = candidate,
      .size = size,
      .alignment = alignment,
  };
  const auto position = std::lower_bound(
      allocations_.begin(), allocations_.end(), allocation.offset,
      [](const StagingAllocation& current, std::uint64_t offset) { return current.offset < offset; });
  allocations_.insert(position, allocation);
  active_bytes_ += size;
  *out_allocation = allocation;
  return MemoryStatus::success;
}

MemoryStatus StagingLedger::release(const StagingAllocation& allocation) noexcept {
  if (allocation.generation == 0U || allocation.generation != generation_) {
    return MemoryStatus::stale_generation;
  }
  const auto position = std::find_if(
      allocations_.begin(), allocations_.end(), [&allocation](const StagingAllocation& current) {
        return current.id == allocation.id && current.generation == allocation.generation &&
               current.offset == allocation.offset && current.size == allocation.size;
      });
  if (position == allocations_.end()) {
    return MemoryStatus::not_found;
  }
  active_bytes_ -= position->size;
  allocations_.erase(position);
  return MemoryStatus::success;
}

MemoryStatus StagingLedger::validate(const StagingAllocation& allocation) const noexcept {
  if (allocation.generation == 0U || allocation.generation != generation_) {
    return MemoryStatus::stale_generation;
  }
  const auto position = std::find_if(
      allocations_.begin(), allocations_.end(), [&allocation](const StagingAllocation& current) {
        return current.id == allocation.id && current.generation == allocation.generation &&
               current.offset == allocation.offset && current.size == allocation.size &&
               current.alignment == allocation.alignment;
      });
  return position == allocations_.end() ? MemoryStatus::not_found : MemoryStatus::success;
}

MemoryStatus TimelineGate::submit(std::uint64_t generation,
                                  TimelineSubmission* out_submission) noexcept {
  if (out_submission == nullptr || generation == 0U || generation != generation_) {
    return generation != generation_ && generation != 0U ? MemoryStatus::stale_generation
                                                          : MemoryStatus::invalid_argument;
  }
  if (next_value_ == std::numeric_limits<std::uint64_t>::max()) {
    return MemoryStatus::busy;
  }
  *out_submission = TimelineSubmission{.value = next_value_++, .generation = generation_};
  return MemoryStatus::success;
}

MemoryStatus TimelineGate::complete(std::uint64_t generation, std::uint64_t value) noexcept {
  if (generation == 0U || generation != generation_) {
    return MemoryStatus::stale_generation;
  }
  if (value == 0U || value >= next_value_) {
    return MemoryStatus::invalid_argument;
  }
  if (value < completed_value_) {
    return MemoryStatus::invalid_argument;
  }
  completed_value_ = value;
  return MemoryStatus::success;
}

MemoryStatus TimelineGate::wait(std::uint64_t generation, std::uint64_t value) const noexcept {
  if (generation == 0U || generation != generation_) {
    return MemoryStatus::stale_generation;
  }
  if (value == 0U || value >= next_value_) {
    return MemoryStatus::invalid_argument;
  }
  return value <= completed_value_ ? MemoryStatus::success : MemoryStatus::busy;
}

const char* memory_status_string(MemoryStatus status) noexcept {
  switch (status) {
  case MemoryStatus::success:
    return "success";
  case MemoryStatus::invalid_argument:
    return "invalid-argument";
  case MemoryStatus::unsupported:
    return "unsupported";
  case MemoryStatus::out_of_memory:
    return "out-of-memory";
  case MemoryStatus::stale_generation:
    return "stale-generation";
  case MemoryStatus::not_found:
    return "not-found";
  case MemoryStatus::busy:
    return "busy";
  }
  return "unknown";
}

} // namespace metaflux::backend::vulkan
