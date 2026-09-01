#ifndef METAFLUX_BACKEND_VULKAN_MEMORY_HPP
#define METAFLUX_BACKEND_VULKAN_MEMORY_HPP

#include "metaflux/backend/vulkan.h"
#include "metaflux/backend/vulkan_memory.h"

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

enum class ExternalMemoryStatus : std::uint32_t {
  success = 0,
  invalid_argument = 1,
  unsupported = 2,
  stale_generation = 3,
  not_found = 4,
  busy = 5,
  overlap = 6,
  incompatible_memory_type = 7,
  incompatible_handle = 8,
  incompatible_sync = 9,
  ownership_conflict = 10,
  exhausted = 11,
};

struct StagingAllocation {
  std::uint64_t id = 0;
  std::uint64_t generation = 0;
  std::uint64_t offset = 0;
  std::uint64_t size = 0;
  std::uint64_t alignment = 0;
};

struct ExternalMemoryImport final {
  std::uint64_t id = 0;
  std::uint64_t generation = 0;
  std::uint64_t offset = 0;
  std::uint64_t size = 0;
  std::uint64_t alignment = 0;
  std::uint64_t references = 0;
  std::uint32_t memory_type_index = 0;
  std::uint32_t handle_type = MF_VULKAN_MEMORY_HANDLE_NONE_V0;
  std::uint32_t sync_type = MF_VULKAN_MEMORY_SYNC_NONE_V0;
  std::uint32_t flags = 0;
  std::uint64_t permissions = 0;
  bool revoked = false;
};

struct ExternalMemoryHandleImport final {
  ExternalMemoryImport allocation{};
  int owned_fd = -1;
};

// Models direct external-memory admission and lifetime without importing an OS
// fd or owning a Vulkan allocation. The physical adapter must repeat these
// checks before taking ownership of its native handles.
class ExternalMemoryLedger final {
public:
  ExternalMemoryLedger() = default;
  explicit ExternalMemoryLedger(std::uint64_t generation,
                                std::uint32_t memory_type_bits,
                                std::uint32_t handle_type_bits,
                                std::uint32_t sync_type_bits,
                                std::size_t capacity = 0U) noexcept;

  [[nodiscard]] ExternalMemoryStatus configure(std::uint64_t generation,
                                               std::uint32_t memory_type_bits,
                                               std::uint32_t handle_type_bits,
                                               std::uint32_t sync_type_bits,
                                               std::size_t capacity = 0U) noexcept;
  [[nodiscard]] ExternalMemoryStatus import(
      const mf_vulkan_external_memory_profile_v0& profile, std::uint64_t offset,
      std::uint32_t memory_type_index, ExternalMemoryImport* out_import) noexcept;
  [[nodiscard]] ExternalMemoryStatus retain(const ExternalMemoryImport& import) noexcept;
  [[nodiscard]] ExternalMemoryStatus release(const ExternalMemoryImport& import) noexcept;
  [[nodiscard]] ExternalMemoryStatus revoke(const ExternalMemoryImport& import) noexcept;
  [[nodiscard]] ExternalMemoryStatus validate(const ExternalMemoryImport& import) const noexcept;
  [[nodiscard]] std::size_t active_count() const noexcept { return imports_.size(); }
  [[nodiscard]] std::uint64_t generation() const noexcept { return generation_; }

private:
  [[nodiscard]] static bool range_overflows(std::uint64_t offset,
                                            std::uint64_t size) noexcept;
  [[nodiscard]] static std::uint32_t handle_bit(std::uint32_t handle_type) noexcept;
  [[nodiscard]] static std::uint32_t sync_bit(std::uint32_t sync_type) noexcept;
  [[nodiscard]] std::vector<ExternalMemoryImport>::iterator
  find(const ExternalMemoryImport& import) noexcept;
  [[nodiscard]] std::vector<ExternalMemoryImport>::const_iterator
  find(const ExternalMemoryImport& import) const noexcept;

  std::uint64_t generation_ = 0;
  std::uint32_t memory_type_bits_ = 0;
  std::uint32_t handle_type_bits_ = 0;
  std::uint32_t sync_type_bits_ = 0;
  std::size_t capacity_ = 0;
  std::uint64_t next_id_ = 1;
  std::vector<ExternalMemoryImport> imports_;
};

// Owns the duplicated native handle only after the metadata ledger accepts an
// import. The caller retains the source fd; the owned duplicate is closed when
// the final ledger reference is released or the object is destroyed.
class ExternalMemoryHandleLedger final {
public:
  ExternalMemoryHandleLedger() = default;
  explicit ExternalMemoryHandleLedger(std::uint64_t generation,
                                      std::uint32_t memory_type_bits,
                                      std::uint32_t handle_type_bits,
                                      std::uint32_t sync_type_bits,
                                      std::size_t capacity = 0U) noexcept;
  ~ExternalMemoryHandleLedger() noexcept;

  ExternalMemoryHandleLedger(const ExternalMemoryHandleLedger&) = delete;
  ExternalMemoryHandleLedger& operator=(const ExternalMemoryHandleLedger&) = delete;

  [[nodiscard]] ExternalMemoryStatus configure(std::uint64_t generation,
                                               std::uint32_t memory_type_bits,
                                               std::uint32_t handle_type_bits,
                                               std::uint32_t sync_type_bits,
                                               std::size_t capacity = 0U) noexcept;
  [[nodiscard]] ExternalMemoryStatus import_fd(
      const mf_vulkan_external_memory_profile_v0& profile, int source_fd,
      std::uint64_t offset, std::uint32_t memory_type_index,
      ExternalMemoryHandleImport* out_import) noexcept;
  [[nodiscard]] ExternalMemoryStatus retain(const ExternalMemoryHandleImport& import) noexcept;
  [[nodiscard]] ExternalMemoryStatus release(const ExternalMemoryHandleImport& import) noexcept;
  [[nodiscard]] ExternalMemoryStatus revoke(const ExternalMemoryHandleImport& import) noexcept;
  [[nodiscard]] ExternalMemoryStatus validate(
      const ExternalMemoryHandleImport& import) const noexcept;
  [[nodiscard]] std::size_t active_count() const noexcept { return handles_.size(); }
  [[nodiscard]] std::uint64_t generation() const noexcept { return ledger_.generation(); }

private:
  struct OwnedHandle final {
    ExternalMemoryHandleImport import{};
  };

  static bool same_import(const ExternalMemoryImport& left,
                          const ExternalMemoryImport& right) noexcept;
  static int duplicate_fd(int source_fd) noexcept;
  static void close_fd(int* fd) noexcept;
  [[nodiscard]] std::vector<OwnedHandle>::iterator
  find(const ExternalMemoryHandleImport& import) noexcept;
  [[nodiscard]] std::vector<OwnedHandle>::const_iterator
  find(const ExternalMemoryHandleImport& import) const noexcept;

  ExternalMemoryLedger ledger_{};
  std::vector<OwnedHandle> handles_{};
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
[[nodiscard]] const char* external_memory_status_string(ExternalMemoryStatus status) noexcept;

} // namespace metaflux::backend::vulkan

#endif
