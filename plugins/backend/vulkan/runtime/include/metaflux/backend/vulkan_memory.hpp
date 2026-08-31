#ifndef METAFLUX_BACKEND_VULKAN_MEMORY_HPP
#define METAFLUX_BACKEND_VULKAN_MEMORY_HPP

#include "metaflux/backend/vulkan.h"

#include <cstdint>
#include <vector>

namespace metaflux::backend::vulkan {

enum class MemoryStatus : std::uint32_t {
  success = 0,
  invalid_argument = 1,
  unsupported = 2,
  out_of_memory = 3,
  stale_generation = 4,
  not_found = 5,
  busy = 6,
};

enum class VisibilityStatus : std::uint32_t {
  success = 0,
  invalid_argument = 1,
  stale_generation = 2,
  not_found = 3,
  busy = 4,
  flush_required = 5,
  invalidate_required = 6,
  range_out_of_bounds = 7,
};

struct StagingAllocation {
  std::uint64_t id = 0;
  std::uint64_t generation = 0;
  std::uint64_t offset = 0;
  std::uint64_t size = 0;
  std::uint64_t alignment = 0;
};

class StagingLedger final {
public:
  StagingLedger() = default;
  explicit StagingLedger(const mf_vulkan_capability_profile_v1& profile,
                         std::uint64_t generation) noexcept;

  [[nodiscard]] MemoryStatus configure(const mf_vulkan_capability_profile_v1& profile,
                                       std::uint64_t generation) noexcept;
  [[nodiscard]] MemoryStatus allocate(std::uint64_t size, std::uint64_t alignment,
                                      std::uint64_t generation, StagingAllocation* out_allocation);
  [[nodiscard]] MemoryStatus release(const StagingAllocation& allocation) noexcept;
  [[nodiscard]] MemoryStatus validate(const StagingAllocation& allocation) const noexcept;
  [[nodiscard]] std::uint64_t capacity() const noexcept { return capacity_; }
  [[nodiscard]] std::uint64_t active_bytes() const noexcept { return active_bytes_; }
  [[nodiscard]] std::uint64_t generation() const noexcept { return generation_; }

private:
  [[nodiscard]] static bool is_power_of_two(std::uint64_t value) noexcept;
  [[nodiscard]] static bool round_up(std::uint64_t value, std::uint64_t alignment,
                                     std::uint64_t* out) noexcept;

  std::uint64_t capacity_ = 0;
  std::uint64_t generation_ = 0;
  std::uint64_t next_id_ = 1;
  std::uint64_t active_bytes_ = 0;
  std::vector<StagingAllocation> allocations_;
};

struct TimelineSubmission {
  std::uint64_t value = 0;
  std::uint64_t generation = 0;
};

class TimelineGate final {
public:
  explicit TimelineGate(std::uint64_t generation = 0) noexcept : generation_(generation) {}

  [[nodiscard]] MemoryStatus submit(std::uint64_t generation,
                                    TimelineSubmission* out_submission) noexcept;
  [[nodiscard]] MemoryStatus complete(std::uint64_t generation, std::uint64_t value) noexcept;
  [[nodiscard]] MemoryStatus wait(std::uint64_t generation, std::uint64_t value) const noexcept;
  [[nodiscard]] std::uint64_t next_value() const noexcept { return next_value_; }
  [[nodiscard]] std::uint64_t completed_value() const noexcept { return completed_value_; }

private:
  std::uint64_t generation_ = 0;
  std::uint64_t next_value_ = 1;
  std::uint64_t completed_value_ = 0;
};

struct VisibilityRange {
  std::uint64_t offset = 0;
  std::uint64_t size = 0;
};

// Models host/device visibility obligations without owning Vulkan handles. A
// future adapter maps these operations to vkFlushMappedMemoryRanges and
// vkInvalidateMappedMemoryRanges after the same admission checks pass.
class MemoryVisibilityLedger final {
public:
  MemoryVisibilityLedger() = default;
  explicit MemoryVisibilityLedger(std::uint64_t generation,
                                  std::uint64_t non_coherent_atom_size) noexcept;

  [[nodiscard]] VisibilityStatus configure(std::uint64_t generation,
                                           std::uint64_t non_coherent_atom_size) noexcept;
  [[nodiscard]] VisibilityStatus register_allocation(const StagingAllocation& allocation,
                                                     bool host_coherent) noexcept;
  [[nodiscard]] VisibilityStatus
  unregister_allocation(const StagingAllocation& allocation) noexcept;

  [[nodiscard]] VisibilityStatus submit(std::uint64_t generation, std::uint64_t timeline) noexcept;
  [[nodiscard]] VisibilityStatus complete(std::uint64_t generation,
                                          std::uint64_t timeline) noexcept;

  [[nodiscard]] VisibilityStatus host_write(const StagingAllocation& allocation,
                                            std::uint64_t offset, std::uint64_t size,
                                            std::uint64_t generation) noexcept;
  [[nodiscard]] VisibilityStatus flush(const StagingAllocation& allocation, std::uint64_t offset,
                                       std::uint64_t size, std::uint64_t generation) noexcept;
  [[nodiscard]] VisibilityStatus device_read(const StagingAllocation& allocation,
                                             std::uint64_t offset, std::uint64_t size,
                                             std::uint64_t generation,
                                             std::uint64_t timeline) noexcept;
  [[nodiscard]] VisibilityStatus device_write(const StagingAllocation& allocation,
                                              std::uint64_t offset, std::uint64_t size,
                                              std::uint64_t generation,
                                              std::uint64_t timeline) noexcept;
  [[nodiscard]] VisibilityStatus invalidate(const StagingAllocation& allocation,
                                            std::uint64_t offset, std::uint64_t size,
                                            std::uint64_t generation) noexcept;
  [[nodiscard]] VisibilityStatus host_read(const StagingAllocation& allocation,
                                           std::uint64_t offset, std::uint64_t size,
                                           std::uint64_t generation) noexcept;

  [[nodiscard]] std::uint64_t generation() const noexcept { return generation_; }
  [[nodiscard]] std::uint64_t non_coherent_atom_size() const noexcept {
    return non_coherent_atom_size_;
  }
  [[nodiscard]] std::uint64_t completed_timeline() const noexcept { return completed_timeline_; }

private:
  struct Record {
    StagingAllocation allocation{};
    bool host_coherent = false;
    std::uint64_t in_flight_timeline = 0;
    std::vector<VisibilityRange> host_dirty;
    std::vector<VisibilityRange> device_dirty;
  };

  [[nodiscard]] Record* find(const StagingAllocation& allocation) noexcept;
  [[nodiscard]] const Record* find(const StagingAllocation& allocation) const noexcept;
  [[nodiscard]] VisibilityStatus normalize_range(const Record& record, std::uint64_t offset,
                                                 std::uint64_t size,
                                                 VisibilityRange* out_range) const noexcept;
  [[nodiscard]] VisibilityStatus check_access(const StagingAllocation& allocation,
                                              std::uint64_t offset, std::uint64_t size,
                                              std::uint64_t generation, VisibilityRange* out_range,
                                              Record** out_record) noexcept;
  [[nodiscard]] VisibilityStatus check_submission(std::uint64_t generation,
                                                  std::uint64_t timeline) const noexcept;
  static bool overlaps(const VisibilityRange& left, const VisibilityRange& right) noexcept;
  static void add_range(std::vector<VisibilityRange>* ranges, VisibilityRange range);
  static void remove_range(std::vector<VisibilityRange>* ranges, VisibilityRange range);

  std::uint64_t generation_ = 0;
  std::uint64_t non_coherent_atom_size_ = 0;
  std::uint64_t highest_submitted_timeline_ = 0;
  std::uint64_t completed_timeline_ = 0;
  std::vector<Record> records_;
};

[[nodiscard]] const char* memory_status_string(MemoryStatus status) noexcept;
[[nodiscard]] const char* visibility_status_string(VisibilityStatus status) noexcept;

} // namespace metaflux::backend::vulkan

#endif
