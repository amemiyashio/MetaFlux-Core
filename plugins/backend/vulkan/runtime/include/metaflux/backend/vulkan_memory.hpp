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
                                       std::uint64_t generation,
                                       StagingAllocation* out_allocation);
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
  [[nodiscard]] MemoryStatus complete(std::uint64_t generation,
                                      std::uint64_t value) noexcept;
  [[nodiscard]] MemoryStatus wait(std::uint64_t generation, std::uint64_t value) const noexcept;
  [[nodiscard]] std::uint64_t next_value() const noexcept { return next_value_; }
  [[nodiscard]] std::uint64_t completed_value() const noexcept { return completed_value_; }

 private:
  std::uint64_t generation_ = 0;
  std::uint64_t next_value_ = 1;
  std::uint64_t completed_value_ = 0;
};

[[nodiscard]] const char* memory_status_string(MemoryStatus status) noexcept;

} // namespace metaflux::backend::vulkan

#endif
