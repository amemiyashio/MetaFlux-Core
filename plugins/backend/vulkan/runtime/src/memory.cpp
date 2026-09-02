#include "metaflux/backend/vulkan_memory.hpp"

#include <algorithm>
#include <cerrno>
#include <fcntl.h>
#include <limits>
#include <utility>
#include <unistd.h>

namespace metaflux::backend::vulkan {

ExternalMemoryLedger::ExternalMemoryLedger(std::uint64_t generation,
                                           std::uint32_t memory_type_bits,
                                           std::uint32_t handle_type_bits,
                                           std::uint32_t sync_type_bits,
                                           std::size_t capacity) noexcept {
  (void)configure(generation, memory_type_bits, handle_type_bits, sync_type_bits, capacity);
}

ExternalMemoryStatus ExternalMemoryLedger::configure(std::uint64_t generation,
                                                     std::uint32_t memory_type_bits,
                                                     std::uint32_t handle_type_bits,
                                                     std::uint32_t sync_type_bits,
                                                     std::size_t capacity) noexcept {
  if (generation == 0U || memory_type_bits == 0U || handle_type_bits == 0U ||
      sync_type_bits == 0U) {
    return ExternalMemoryStatus::invalid_argument;
  }
  if (std::any_of(imports_.begin(), imports_.end(),
                  [](const ExternalMemoryImport& import) { return import.references != 0U; })) {
    return ExternalMemoryStatus::busy;
  }
  generation_ = generation;
  memory_type_bits_ = memory_type_bits;
  handle_type_bits_ = handle_type_bits;
  sync_type_bits_ = sync_type_bits;
  capacity_ = capacity;
  next_id_ = 1U;
  imports_.clear();
  return ExternalMemoryStatus::success;
}

bool ExternalMemoryLedger::range_overflows(std::uint64_t offset, std::uint64_t size) noexcept {
  return size == 0U || offset > std::numeric_limits<std::uint64_t>::max() - size;
}

std::uint32_t ExternalMemoryLedger::handle_bit(std::uint32_t handle_type) noexcept {
  return handle_type == 0U || handle_type > 31U ? 0U : UINT32_C(1) << (handle_type - 1U);
}

std::uint32_t ExternalMemoryLedger::sync_bit(std::uint32_t sync_type) noexcept {
  return sync_type == 0U || sync_type > 31U ? 0U : UINT32_C(1) << (sync_type - 1U);
}

std::vector<ExternalMemoryImport>::iterator
ExternalMemoryLedger::find(const ExternalMemoryImport& import) noexcept {
  return std::find_if(imports_.begin(), imports_.end(), [&import](const auto& current) {
    return current.id == import.id && current.generation == import.generation &&
           current.offset == import.offset && current.size == import.size &&
           current.alignment == import.alignment &&
           current.memory_type_index == import.memory_type_index &&
           current.handle_type == import.handle_type && current.sync_type == import.sync_type &&
           current.flags == import.flags && current.permissions == import.permissions;
  });
}

std::vector<ExternalMemoryImport>::const_iterator
ExternalMemoryLedger::find(const ExternalMemoryImport& import) const noexcept {
  return std::find_if(imports_.begin(), imports_.end(), [&import](const auto& current) {
    return current.id == import.id && current.generation == import.generation &&
           current.offset == import.offset && current.size == import.size &&
           current.alignment == import.alignment &&
           current.memory_type_index == import.memory_type_index &&
           current.handle_type == import.handle_type && current.sync_type == import.sync_type &&
           current.flags == import.flags && current.permissions == import.permissions;
  });
}

ExternalMemoryStatus ExternalMemoryLedger::import(
    const mf_vulkan_external_memory_profile_v0& profile, std::uint64_t offset,
    std::uint32_t memory_type_index, ExternalMemoryImport* out_import) noexcept {
  if (out_import == nullptr || generation_ == 0U ||
      !mf_vulkan_external_memory_profile_valid_v0(&profile) ||
      (profile.flags & MF_VULKAN_MEMORY_FLAG_DIRECT_IMPORT_V0) == 0U) {
    return ExternalMemoryStatus::invalid_argument;
  }
  if (profile.generation != generation_) {
    return ExternalMemoryStatus::stale_generation;
  }
  if (memory_type_index >= 32U ||
      (memory_type_bits_ & (UINT32_C(1) << memory_type_index)) == 0U ||
      (profile.memory_type_bits & (UINT32_C(1) << memory_type_index)) == 0U) {
    return ExternalMemoryStatus::incompatible_memory_type;
  }
  if (handle_bit(profile.handle_type) == 0U ||
      (handle_type_bits_ & handle_bit(profile.handle_type)) == 0U) {
    return ExternalMemoryStatus::incompatible_handle;
  }
  if (sync_bit(profile.sync_type) == 0U ||
      (sync_type_bits_ & sync_bit(profile.sync_type)) == 0U) {
    return ExternalMemoryStatus::incompatible_sync;
  }
  if (offset % profile.alignment != 0U || range_overflows(offset, profile.size) ||
      ((profile.flags & MF_VULKAN_MEMORY_FLAG_DEDICATED_ONLY_V0) != 0U && offset != 0U)) {
    return ExternalMemoryStatus::invalid_argument;
  }
  if (capacity_ != 0U && imports_.size() >= capacity_) {
    return ExternalMemoryStatus::exhausted;
  }
  const auto new_end = offset + profile.size;
  for (const auto& current : imports_) {
    if (current.revoked || current.generation != generation_) {
      continue;
    }
    if (offset < current.offset + current.size && current.offset < new_end) {
      return ExternalMemoryStatus::overlap;
    }
  }
  if (next_id_ == 0U || next_id_ == std::numeric_limits<std::uint64_t>::max()) {
    return ExternalMemoryStatus::exhausted;
  }
  ExternalMemoryImport candidate{
      .id = next_id_++,
      .generation = generation_,
      .offset = offset,
      .size = profile.size,
      .alignment = profile.alignment,
      .references = 1U,
      .memory_type_index = memory_type_index,
      .handle_type = profile.handle_type,
      .sync_type = profile.sync_type,
      .flags = profile.flags,
      .permissions = profile.permissions,
      .revoked = false,
  };
  try {
    imports_.push_back(candidate);
  } catch (...) {
    return ExternalMemoryStatus::exhausted;
  }
  *out_import = candidate;
  return ExternalMemoryStatus::success;
}

ExternalMemoryStatus ExternalMemoryLedger::validate(
    const ExternalMemoryImport& import) const noexcept {
  if (import.generation == 0U || import.generation != generation_) {
    return ExternalMemoryStatus::stale_generation;
  }
  const auto position = find(import);
  if (position == imports_.end()) {
    return ExternalMemoryStatus::not_found;
  }
  return position->revoked ? ExternalMemoryStatus::ownership_conflict
                           : ExternalMemoryStatus::success;
}

ExternalMemoryStatus ExternalMemoryLedger::retain(const ExternalMemoryImport& import) noexcept {
  auto position = find(import);
  if (position == imports_.end()) {
    return import.generation != generation_ ? ExternalMemoryStatus::stale_generation
                                            : ExternalMemoryStatus::not_found;
  }
  if (position->revoked || position->references == std::numeric_limits<std::uint64_t>::max()) {
    return ExternalMemoryStatus::ownership_conflict;
  }
  ++position->references;
  return ExternalMemoryStatus::success;
}

ExternalMemoryStatus ExternalMemoryLedger::release(const ExternalMemoryImport& import) noexcept {
  auto position = find(import);
  if (position == imports_.end()) {
    return import.generation != generation_ ? ExternalMemoryStatus::stale_generation
                                            : ExternalMemoryStatus::not_found;
  }
  if (position->references == 0U) {
    return ExternalMemoryStatus::ownership_conflict;
  }
  --position->references;
  if (position->references == 0U) {
    imports_.erase(position);
  }
  return ExternalMemoryStatus::success;
}

ExternalMemoryStatus ExternalMemoryLedger::revoke(const ExternalMemoryImport& import) noexcept {
  auto position = find(import);
  if (position == imports_.end()) {
    return import.generation != generation_ ? ExternalMemoryStatus::stale_generation
                                            : ExternalMemoryStatus::not_found;
  }
  position->revoked = true;
  return position->references > 1U ? ExternalMemoryStatus::busy
                                   : ExternalMemoryStatus::success;
}

ExternalMemoryHandleLedger::ExternalMemoryHandleLedger(
    std::uint64_t generation, std::uint32_t memory_type_bits,
    std::uint32_t handle_type_bits, std::uint32_t sync_type_bits,
    std::size_t capacity) noexcept {
  (void)configure(generation, memory_type_bits, handle_type_bits, sync_type_bits, capacity);
}

ExternalMemoryHandleLedger::~ExternalMemoryHandleLedger() noexcept {
  for (auto& handle : handles_) {
    close_fd(&handle.import.owned_fd);
  }
}

ExternalMemoryStatus ExternalMemoryHandleLedger::configure(
    std::uint64_t generation, std::uint32_t memory_type_bits,
    std::uint32_t handle_type_bits, std::uint32_t sync_type_bits,
    std::size_t capacity) noexcept {
  const auto status = ledger_.configure(generation, memory_type_bits, handle_type_bits,
                                        sync_type_bits, capacity);
  if (status != ExternalMemoryStatus::success) {
    return status;
  }
  handles_.clear();
  return ExternalMemoryStatus::success;
}

bool ExternalMemoryHandleLedger::same_import(const ExternalMemoryImport& left,
                                             const ExternalMemoryImport& right) noexcept {
  return left.id == right.id && left.generation == right.generation &&
         left.offset == right.offset && left.size == right.size &&
         left.alignment == right.alignment &&
         left.memory_type_index == right.memory_type_index &&
         left.handle_type == right.handle_type && left.sync_type == right.sync_type &&
         left.flags == right.flags && left.permissions == right.permissions;
}

int ExternalMemoryHandleLedger::duplicate_fd(int source_fd) noexcept {
  if (source_fd < 0 || ::fcntl(source_fd, F_GETFD) < 0) {
    return -1;
  }
#ifdef F_DUPFD_CLOEXEC
  return ::fcntl(source_fd, F_DUPFD_CLOEXEC, 0);
#else
  const int duplicate = ::dup(source_fd);
  if (duplicate >= 0 && ::fcntl(duplicate, F_SETFD, FD_CLOEXEC) < 0) {
    const int saved_errno = errno;
    (void)::close(duplicate);
    errno = saved_errno;
    return -1;
  }
  return duplicate;
#endif
}

void ExternalMemoryHandleLedger::close_fd(int* fd) noexcept {
  if (fd == nullptr || *fd < 0) {
    return;
  }
  (void)::close(*fd);
  *fd = -1;
}

std::vector<ExternalMemoryHandleLedger::OwnedHandle>::iterator
ExternalMemoryHandleLedger::find(const ExternalMemoryHandleImport& import) noexcept {
  return std::find_if(handles_.begin(), handles_.end(), [&import](const auto& current) {
    return same_import(current.import.allocation, import.allocation) &&
           current.import.owned_fd == import.owned_fd;
  });
}

std::vector<ExternalMemoryHandleLedger::OwnedHandle>::const_iterator
ExternalMemoryHandleLedger::find(const ExternalMemoryHandleImport& import) const noexcept {
  return std::find_if(handles_.begin(), handles_.end(), [&import](const auto& current) {
    return same_import(current.import.allocation, import.allocation) &&
           current.import.owned_fd == import.owned_fd;
  });
}

ExternalMemoryStatus ExternalMemoryHandleLedger::import_fd(
    const mf_vulkan_external_memory_profile_v0& profile, int source_fd,
    std::uint64_t offset, std::uint32_t memory_type_index,
    ExternalMemoryHandleImport* out_import) noexcept {
  if (out_import == nullptr || source_fd < 0) {
    return ExternalMemoryStatus::invalid_argument;
  }
  const int owned_fd = duplicate_fd(source_fd);
  if (owned_fd < 0) {
    return ExternalMemoryStatus::invalid_argument;
  }
  ExternalMemoryImport allocation{};
  const auto status = ledger_.import(profile, offset, memory_type_index, &allocation);
  if (status != ExternalMemoryStatus::success) {
    int disposable_fd = owned_fd;
    close_fd(&disposable_fd);
    return status;
  }
  try {
    handles_.push_back(OwnedHandle{.import = ExternalMemoryHandleImport{
                                       .allocation = allocation, .owned_fd = owned_fd}});
  } catch (...) {
    (void)ledger_.release(allocation);
    int disposable_fd = owned_fd;
    close_fd(&disposable_fd);
    return ExternalMemoryStatus::exhausted;
  }
  *out_import = handles_.back().import;
  return ExternalMemoryStatus::success;
}

ExternalMemoryStatus ExternalMemoryHandleLedger::retain(
    const ExternalMemoryHandleImport& import) noexcept {
  if (find(import) == handles_.end()) {
    return import.allocation.generation != ledger_.generation()
               ? ExternalMemoryStatus::stale_generation
               : ExternalMemoryStatus::not_found;
  }
  return ledger_.retain(import.allocation);
}

ExternalMemoryStatus ExternalMemoryHandleLedger::release(
    const ExternalMemoryHandleImport& import) noexcept {
  const auto position = find(import);
  if (position == handles_.end()) {
    return import.allocation.generation != ledger_.generation()
               ? ExternalMemoryStatus::stale_generation
               : ExternalMemoryStatus::not_found;
  }
  const std::size_t before = ledger_.active_count();
  const auto status = ledger_.release(import.allocation);
  if (status != ExternalMemoryStatus::success) {
    return status;
  }
  if (ledger_.active_count() < before) {
    close_fd(&position->import.owned_fd);
    handles_.erase(position);
  }
  return ExternalMemoryStatus::success;
}

ExternalMemoryStatus ExternalMemoryHandleLedger::revoke(
    const ExternalMemoryHandleImport& import) noexcept {
  const auto position = find(import);
  if (position == handles_.end()) {
    return import.allocation.generation != ledger_.generation()
               ? ExternalMemoryStatus::stale_generation
               : ExternalMemoryStatus::not_found;
  }
  return ledger_.revoke(import.allocation);
}

ExternalMemoryStatus ExternalMemoryHandleLedger::validate(
    const ExternalMemoryHandleImport& import) const noexcept {
  const auto position = find(import);
  if (position == handles_.end()) {
    return import.allocation.generation != ledger_.generation()
               ? ExternalMemoryStatus::stale_generation
               : ExternalMemoryStatus::not_found;
  }
  return ledger_.validate(import.allocation);
}

StagingLedger::StagingLedger(const mf_vulkan_capability_profile_v1& profile,
                             std::uint64_t generation) noexcept {
  (void)configure(profile, generation);
}

MemoryStatus StagingLedger::configure(const mf_vulkan_capability_profile_v1& profile,
                                      std::uint64_t generation) noexcept {
  if (profile.status != MF_VULKAN_PROBE_SUCCESS || generation == 0U ||
      (profile.memory_tier_flags & MF_VULKAN_MEMORY_TIER_STAGING) == 0U ||
      profile.device_local_heap_bytes == 0U || profile.host_visible_heap_bytes == 0U) {
    return MemoryStatus::unsupported;
  }
  const std::uint64_t capacity =
      std::min(profile.device_local_heap_bytes, profile.host_visible_heap_bytes);
  if (capacity == 0U) {
    return MemoryStatus::unsupported;
  }
  if (!allocations_.empty()) {
    return MemoryStatus::busy;
  }
  capacity_ = capacity;
  generation_ = generation;
  next_id_ = 1U;
  active_bytes_ = 0U;
  return MemoryStatus::success;
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
  if (next_id_ == 0U || next_id_ == std::numeric_limits<std::uint64_t>::max()) {
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
               current.offset == allocation.offset && current.size == allocation.size &&
               current.alignment == allocation.alignment;
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
  if (generation == 0U) {
    return MemoryStatus::invalid_argument;
  }
  if (generation != generation_) {
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
  if (generation == 0U) {
    return MemoryStatus::invalid_argument;
  }
  if (generation != generation_) {
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
               record.allocation.size == allocation.size &&
               record.allocation.alignment == allocation.alignment;
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
               record.allocation.size == allocation.size &&
               record.allocation.alignment == allocation.alignment;
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
               record.allocation.size == allocation.size &&
               record.allocation.alignment == allocation.alignment;
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

const char* external_memory_status_string(ExternalMemoryStatus status) noexcept {
  switch (status) {
  case ExternalMemoryStatus::success:
    return "success";
  case ExternalMemoryStatus::invalid_argument:
    return "invalid-argument";
  case ExternalMemoryStatus::unsupported:
    return "unsupported";
  case ExternalMemoryStatus::stale_generation:
    return "stale-generation";
  case ExternalMemoryStatus::not_found:
    return "not-found";
  case ExternalMemoryStatus::busy:
    return "busy";
  case ExternalMemoryStatus::overlap:
    return "overlap";
  case ExternalMemoryStatus::incompatible_memory_type:
    return "incompatible-memory-type";
  case ExternalMemoryStatus::incompatible_handle:
    return "incompatible-handle";
  case ExternalMemoryStatus::incompatible_sync:
    return "incompatible-sync";
  case ExternalMemoryStatus::ownership_conflict:
    return "ownership-conflict";
  case ExternalMemoryStatus::exhausted:
    return "exhausted";
  }
  return "unknown";
}

} // namespace metaflux::backend::vulkan
