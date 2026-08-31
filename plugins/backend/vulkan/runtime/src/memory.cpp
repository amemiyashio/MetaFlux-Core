#include "metaflux/backend/vulkan_memory.hpp"

#include <algorithm>
#include <limits>
#include <utility>

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
                                     std::uint64_t generation, StagingAllocation* out_allocation) {
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
  const auto position =
      std::lower_bound(allocations_.begin(), allocations_.end(), allocation.offset,
                       [](const StagingAllocation& current, std::uint64_t offset) {
                         return current.offset < offset;
                       });
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

MemoryVisibilityLedger::MemoryVisibilityLedger(std::uint64_t generation,
                                               std::uint64_t non_coherent_atom_size) noexcept {
  (void)configure(generation, non_coherent_atom_size);
}

VisibilityStatus MemoryVisibilityLedger::configure(std::uint64_t generation,
                                                   std::uint64_t non_coherent_atom_size) noexcept {
  if (generation == 0U || non_coherent_atom_size == 0U) {
    return VisibilityStatus::invalid_argument;
  }
  if (!records_.empty() &&
      std::any_of(records_.begin(), records_.end(), [this](const Record& record) {
        return record.in_flight_timeline > completed_timeline_;
      })) {
    return VisibilityStatus::busy;
  }
  generation_ = generation;
  non_coherent_atom_size_ = non_coherent_atom_size;
  highest_submitted_timeline_ = 0U;
  completed_timeline_ = 0U;
  records_.clear();
  return VisibilityStatus::success;
}

VisibilityStatus MemoryVisibilityLedger::register_allocation(const StagingAllocation& allocation,
                                                             bool host_coherent) noexcept {
  if (generation_ == 0U || allocation.id == 0U || allocation.generation == 0U ||
      allocation.generation != generation_ || allocation.size == 0U || allocation.alignment == 0U) {
    return allocation.generation != 0U && allocation.generation != generation_
               ? VisibilityStatus::stale_generation
               : VisibilityStatus::invalid_argument;
  }
  if (find(allocation) != nullptr) {
    return VisibilityStatus::invalid_argument;
  }
  if (!host_coherent && (allocation.offset % non_coherent_atom_size_ != 0U ||
                         allocation.size % non_coherent_atom_size_ != 0U)) {
    return VisibilityStatus::invalid_argument;
  }
  records_.push_back(Record{.allocation = allocation,
                            .host_coherent = host_coherent,
                            .in_flight_timeline = 0U,
                            .host_dirty = {},
                            .device_dirty = {}});
  return VisibilityStatus::success;
}

VisibilityStatus
MemoryVisibilityLedger::unregister_allocation(const StagingAllocation& allocation) noexcept {
  if (allocation.generation == 0U || allocation.generation != generation_) {
    return VisibilityStatus::stale_generation;
  }
  const auto position =
      std::find_if(records_.begin(), records_.end(), [&allocation](const Record& record) {
        return record.allocation.id == allocation.id &&
               record.allocation.generation == allocation.generation &&
               record.allocation.offset == allocation.offset &&
               record.allocation.size == allocation.size;
      });
  if (position == records_.end()) {
    return VisibilityStatus::not_found;
  }
  if (position->in_flight_timeline > completed_timeline_) {
    return VisibilityStatus::busy;
  }
  records_.erase(position);
  return VisibilityStatus::success;
}

VisibilityStatus MemoryVisibilityLedger::submit(std::uint64_t generation,
                                                std::uint64_t timeline) noexcept {
  if (generation == 0U || generation != generation_) {
    return generation != 0U ? VisibilityStatus::stale_generation
                            : VisibilityStatus::invalid_argument;
  }
  if (timeline == 0U || timeline <= highest_submitted_timeline_ ||
      timeline <= completed_timeline_) {
    return VisibilityStatus::invalid_argument;
  }
  highest_submitted_timeline_ = timeline;
  return VisibilityStatus::success;
}

VisibilityStatus MemoryVisibilityLedger::complete(std::uint64_t generation,
                                                  std::uint64_t timeline) noexcept {
  if (generation == 0U || generation != generation_) {
    return generation != 0U ? VisibilityStatus::stale_generation
                            : VisibilityStatus::invalid_argument;
  }
  if (timeline == 0U || timeline > highest_submitted_timeline_ || timeline < completed_timeline_) {
    return VisibilityStatus::invalid_argument;
  }
  completed_timeline_ = timeline;
  return VisibilityStatus::success;
}

VisibilityStatus MemoryVisibilityLedger::host_write(const StagingAllocation& allocation,
                                                    std::uint64_t offset, std::uint64_t size,
                                                    std::uint64_t generation) noexcept {
  VisibilityRange range{};
  Record* record = nullptr;
  const auto status = check_access(allocation, offset, size, generation, &range, &record);
  if (status != VisibilityStatus::success) {
    return status;
  }
  if (record->in_flight_timeline > completed_timeline_) {
    return VisibilityStatus::busy;
  }
  if (!record->host_coherent &&
      std::any_of(record->device_dirty.begin(), record->device_dirty.end(),
                  [&range](const VisibilityRange& dirty) { return overlaps(dirty, range); })) {
    return VisibilityStatus::invalidate_required;
  }
  if (!record->host_coherent) {
    add_range(&record->host_dirty, range);
  }
  return VisibilityStatus::success;
}

VisibilityStatus MemoryVisibilityLedger::flush(const StagingAllocation& allocation,
                                               std::uint64_t offset, std::uint64_t size,
                                               std::uint64_t generation) noexcept {
  VisibilityRange range{};
  Record* record = nullptr;
  const auto status = check_access(allocation, offset, size, generation, &range, &record);
  if (status != VisibilityStatus::success) {
    return status;
  }
  if (record->in_flight_timeline > completed_timeline_) {
    return VisibilityStatus::busy;
  }
  if (!record->host_coherent) {
    remove_range(&record->host_dirty, range);
  }
  return VisibilityStatus::success;
}

VisibilityStatus MemoryVisibilityLedger::device_read(const StagingAllocation& allocation,
                                                     std::uint64_t offset, std::uint64_t size,
                                                     std::uint64_t generation,
                                                     std::uint64_t timeline) noexcept {
  VisibilityRange range{};
  Record* record = nullptr;
  const auto status = check_access(allocation, offset, size, generation, &range, &record);
  if (status != VisibilityStatus::success) {
    return status;
  }
  const auto submission_status = check_submission(generation, timeline);
  if (submission_status != VisibilityStatus::success) {
    return submission_status;
  }
  if (record->in_flight_timeline > completed_timeline_) {
    return VisibilityStatus::busy;
  }
  if (!record->host_coherent &&
      std::any_of(record->host_dirty.begin(), record->host_dirty.end(),
                  [&range](const VisibilityRange& dirty) { return overlaps(dirty, range); })) {
    return VisibilityStatus::flush_required;
  }
  record->in_flight_timeline = timeline;
  return VisibilityStatus::success;
}

VisibilityStatus MemoryVisibilityLedger::device_write(const StagingAllocation& allocation,
                                                      std::uint64_t offset, std::uint64_t size,
                                                      std::uint64_t generation,
                                                      std::uint64_t timeline) noexcept {
  VisibilityRange range{};
  Record* record = nullptr;
  const auto status = check_access(allocation, offset, size, generation, &range, &record);
  if (status != VisibilityStatus::success) {
    return status;
  }
  const auto submission_status = check_submission(generation, timeline);
  if (submission_status != VisibilityStatus::success) {
    return submission_status;
  }
  if (record->in_flight_timeline > completed_timeline_) {
    return VisibilityStatus::busy;
  }
  if (!record->host_coherent &&
      std::any_of(record->host_dirty.begin(), record->host_dirty.end(),
                  [&range](const VisibilityRange& dirty) { return overlaps(dirty, range); })) {
    return VisibilityStatus::flush_required;
  }
  record->in_flight_timeline = timeline;
  if (!record->host_coherent) {
    add_range(&record->device_dirty, range);
  }
  return VisibilityStatus::success;
}

VisibilityStatus MemoryVisibilityLedger::invalidate(const StagingAllocation& allocation,
                                                    std::uint64_t offset, std::uint64_t size,
                                                    std::uint64_t generation) noexcept {
  VisibilityRange range{};
  Record* record = nullptr;
  const auto status = check_access(allocation, offset, size, generation, &range, &record);
  if (status != VisibilityStatus::success) {
    return status;
  }
  if (record->in_flight_timeline > completed_timeline_) {
    return VisibilityStatus::busy;
  }
  if (!record->host_coherent) {
    remove_range(&record->device_dirty, range);
  }
  return VisibilityStatus::success;
}

VisibilityStatus MemoryVisibilityLedger::host_read(const StagingAllocation& allocation,
                                                   std::uint64_t offset, std::uint64_t size,
                                                   std::uint64_t generation) noexcept {
  VisibilityRange range{};
  Record* record = nullptr;
  const auto status = check_access(allocation, offset, size, generation, &range, &record);
  if (status != VisibilityStatus::success) {
    return status;
  }
  if (record->in_flight_timeline > completed_timeline_) {
    return VisibilityStatus::busy;
  }
  if (!record->host_coherent &&
      std::any_of(record->device_dirty.begin(), record->device_dirty.end(),
                  [&range](const VisibilityRange& dirty) { return overlaps(dirty, range); })) {
    return VisibilityStatus::invalidate_required;
  }
  return VisibilityStatus::success;
}

MemoryVisibilityLedger::Record*
MemoryVisibilityLedger::find(const StagingAllocation& allocation) noexcept {
  const auto position =
      std::find_if(records_.begin(), records_.end(), [&allocation](const Record& record) {
        return record.allocation.id == allocation.id &&
               record.allocation.generation == allocation.generation &&
               record.allocation.offset == allocation.offset &&
               record.allocation.size == allocation.size;
      });
  return position == records_.end() ? nullptr : &*position;
}

const MemoryVisibilityLedger::Record*
MemoryVisibilityLedger::find(const StagingAllocation& allocation) const noexcept {
  const auto position =
      std::find_if(records_.begin(), records_.end(), [&allocation](const Record& record) {
        return record.allocation.id == allocation.id &&
               record.allocation.generation == allocation.generation &&
               record.allocation.offset == allocation.offset &&
               record.allocation.size == allocation.size;
      });
  return position == records_.end() ? nullptr : &*position;
}

VisibilityStatus
MemoryVisibilityLedger::normalize_range(const Record& record, std::uint64_t offset,
                                        std::uint64_t size,
                                        VisibilityRange* out_range) const noexcept {
  if (out_range == nullptr || size == 0U || offset < record.allocation.offset ||
      record.allocation.offset >
          std::numeric_limits<std::uint64_t>::max() - record.allocation.size ||
      offset > std::numeric_limits<std::uint64_t>::max() - size ||
      offset + size > record.allocation.offset + record.allocation.size) {
    return VisibilityStatus::range_out_of_bounds;
  }
  if (record.host_coherent) {
    *out_range = VisibilityRange{.offset = offset, .size = size};
    return VisibilityStatus::success;
  }
  const auto atom = non_coherent_atom_size_;
  if (atom == 0U || offset / atom > std::numeric_limits<std::uint64_t>::max() / atom) {
    return VisibilityStatus::invalid_argument;
  }
  const auto aligned_offset = (offset / atom) * atom;
  const auto end = offset + size;
  if (end > std::numeric_limits<std::uint64_t>::max() - (atom - 1U)) {
    return VisibilityStatus::range_out_of_bounds;
  }
  const auto aligned_end = ((end + atom - 1U) / atom) * atom;
  if (aligned_offset < record.allocation.offset ||
      aligned_end > record.allocation.offset + record.allocation.size) {
    return VisibilityStatus::range_out_of_bounds;
  }
  *out_range = VisibilityRange{.offset = aligned_offset, .size = aligned_end - aligned_offset};
  return VisibilityStatus::success;
}

VisibilityStatus MemoryVisibilityLedger::check_access(const StagingAllocation& allocation,
                                                      std::uint64_t offset, std::uint64_t size,
                                                      std::uint64_t generation,
                                                      VisibilityRange* out_range,
                                                      Record** out_record) noexcept {
  if (generation == 0U || generation != generation_) {
    return generation != 0U ? VisibilityStatus::stale_generation
                            : VisibilityStatus::invalid_argument;
  }
  auto* record = find(allocation);
  if (record == nullptr) {
    return allocation.generation != generation_ ? VisibilityStatus::stale_generation
                                                : VisibilityStatus::not_found;
  }
  const auto status = normalize_range(*record, offset, size, out_range);
  if (status != VisibilityStatus::success) {
    return status;
  }
  *out_record = record;
  return VisibilityStatus::success;
}

VisibilityStatus MemoryVisibilityLedger::check_submission(std::uint64_t generation,
                                                          std::uint64_t timeline) const noexcept {
  if (generation == 0U || generation != generation_) {
    return generation != 0U ? VisibilityStatus::stale_generation
                            : VisibilityStatus::invalid_argument;
  }
  if (timeline == 0U || timeline > highest_submitted_timeline_ || timeline <= completed_timeline_) {
    return VisibilityStatus::invalid_argument;
  }
  return VisibilityStatus::success;
}

bool MemoryVisibilityLedger::overlaps(const VisibilityRange& left,
                                      const VisibilityRange& right) noexcept {
  return left.offset < right.offset + right.size && right.offset < left.offset + left.size;
}

void MemoryVisibilityLedger::add_range(std::vector<VisibilityRange>* ranges,
                                       VisibilityRange range) {
  if (ranges == nullptr || range.size == 0U) {
    return;
  }
  std::vector<VisibilityRange> merged;
  merged.reserve(ranges->size() + 1U);
  bool inserted = false;
  const auto range_end = range.offset + range.size;
  for (const auto current : *ranges) {
    const auto current_end = current.offset + current.size;
    if (current_end < range.offset) {
      merged.push_back(current);
    } else if (range_end < current.offset) {
      if (!inserted) {
        merged.push_back(range);
        inserted = true;
      }
      merged.push_back(current);
    } else {
      range.offset = std::min(range.offset, current.offset);
      const auto end = std::max(range_end, current_end);
      range.size = end - range.offset;
    }
  }
  if (!inserted) {
    merged.push_back(range);
  }
  *ranges = std::move(merged);
}

void MemoryVisibilityLedger::remove_range(std::vector<VisibilityRange>* ranges,
                                          VisibilityRange range) {
  if (ranges == nullptr || range.size == 0U) {
    return;
  }
  const auto range_end = range.offset + range.size;
  std::vector<VisibilityRange> remaining;
  remaining.reserve(ranges->size());
  for (const auto current : *ranges) {
    const auto current_end = current.offset + current.size;
    if (current_end <= range.offset || range_end <= current.offset) {
      remaining.push_back(current);
      continue;
    }
    if (current.offset < range.offset) {
      remaining.push_back(
          VisibilityRange{.offset = current.offset, .size = range.offset - current.offset});
    }
    if (range_end < current_end) {
      remaining.push_back(VisibilityRange{.offset = range_end, .size = current_end - range_end});
    }
  }
  *ranges = std::move(remaining);
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

const char* visibility_status_string(VisibilityStatus status) noexcept {
  switch (status) {
  case VisibilityStatus::success:
    return "success";
  case VisibilityStatus::invalid_argument:
    return "invalid-argument";
  case VisibilityStatus::stale_generation:
    return "stale-generation";
  case VisibilityStatus::not_found:
    return "not-found";
  case VisibilityStatus::busy:
    return "busy";
  case VisibilityStatus::flush_required:
    return "flush-required";
  case VisibilityStatus::invalidate_required:
    return "invalidate-required";
  case VisibilityStatus::range_out_of_bounds:
    return "range-out-of-bounds";
  }
  return "unknown";
}

} // namespace metaflux::backend::vulkan
