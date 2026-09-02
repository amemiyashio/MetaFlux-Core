#include "metaflux/backend/vulkan_memory.hpp"

#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <fcntl.h>
#include <unistd.h>

namespace {

bool expect_visibility(const char* label, metaflux::backend::vulkan::VisibilityStatus actual,
                       metaflux::backend::vulkan::VisibilityStatus expected) {
  if (actual == expected) {
    return true;
  }
  std::printf("visibility %s: got %s, expected %s\n", label,
              metaflux::backend::vulkan::visibility_status_string(actual),
              metaflux::backend::vulkan::visibility_status_string(expected));
  return false;
}

mf_vulkan_capability_profile_v1 profile() {
  mf_vulkan_capability_profile_v1 result{};
  result.status = MF_VULKAN_PROBE_SUCCESS;
  result.memory_tier_flags = MF_VULKAN_MEMORY_TIER_STAGING;
  result.device_local_heap_bytes = 4096U;
  result.host_visible_heap_bytes = 8192U;
  return result;
}

mf_vulkan_external_memory_profile_v0 direct_profile(std::uint64_t generation = 7U) {
  mf_vulkan_external_memory_profile_v0 result{};
  result.struct_size = sizeof(result);
  result.abi_version = MF_VULKAN_EXTERNAL_MEMORY_ABI_VERSION_0;
  result.tier = MF_VULKAN_MEMORY_TIER_OPAQUE_FD_V0;
  result.handle_type = MF_VULKAN_MEMORY_HANDLE_OPAQUE_FD_V0;
  result.sync_type = MF_VULKAN_MEMORY_SYNC_OPAQUE_FD_V0;
  result.flags = MF_VULKAN_MEMORY_FLAG_DEVICE_LOCAL_V0 |
                 MF_VULKAN_MEMORY_FLAG_DIRECT_IMPORT_V0;
  result.memory_type_bits = UINT32_C(0x3);
  result.size = 4096U;
  result.alignment = 256U;
  result.generation = generation;
  result.permissions = 3U;
  return result;
}

bool external_memory_admission_and_drain() {
  using metaflux::backend::vulkan::ExternalMemoryImport;
  using metaflux::backend::vulkan::ExternalMemoryLedger;
  using metaflux::backend::vulkan::ExternalMemoryStatus;
  ExternalMemoryLedger ledger(7U, UINT32_C(0x3), UINT32_C(0x1), UINT32_C(0x1), 2U);
  ExternalMemoryImport first{};
  const auto profile = direct_profile();
  if (ledger.import(profile, 0U, 0U, &first) != ExternalMemoryStatus::success ||
      first.references != 1U || first.handle_type != MF_VULKAN_MEMORY_HANDLE_OPAQUE_FD_V0 ||
      ledger.retain(first) != ExternalMemoryStatus::success || first.references != 1U ||
      ledger.revoke(first) != ExternalMemoryStatus::busy ||
      ledger.validate(first) != ExternalMemoryStatus::ownership_conflict ||
      ledger.retain(first) != ExternalMemoryStatus::ownership_conflict ||
      ledger.release(first) != ExternalMemoryStatus::success ||
      ledger.release(first) != ExternalMemoryStatus::success ||
      ledger.validate(first) != ExternalMemoryStatus::not_found) {
    return false;
  }

  ExternalMemoryImport second{};
  if (ledger.import(profile, 4096U, 1U, &second) != ExternalMemoryStatus::success ||
      ledger.import(profile, 6144U, 1U, &first) != ExternalMemoryStatus::overlap ||
      ledger.import(profile, 8192U, 2U, &first) !=
          ExternalMemoryStatus::incompatible_memory_type) {
    return false;
  }
  auto stale = profile;
  stale.generation = 8U;
  ExternalMemoryImport third{};
  if (ledger.import(stale, 8192U, 0U, &third) != ExternalMemoryStatus::stale_generation ||
      ledger.import(profile, 8192U, 0U, &third) != ExternalMemoryStatus::success ||
      ledger.import(profile, 12288U, 0U, &first) != ExternalMemoryStatus::exhausted ||
      ledger.release(second) != ExternalMemoryStatus::success ||
      ledger.release(third) != ExternalMemoryStatus::success ||
      ledger.active_count() != 0U) {
    return false;
  }

  auto staging = profile;
  staging.tier = MF_VULKAN_MEMORY_TIER_STAGING_V0;
  staging.handle_type = MF_VULKAN_MEMORY_HANDLE_NONE_V0;
  staging.sync_type = MF_VULKAN_MEMORY_SYNC_NONE_V0;
  staging.flags = MF_VULKAN_MEMORY_FLAG_HOST_VISIBLE_V0;
  if (ledger.import(staging, 0U, 0U, &first) != ExternalMemoryStatus::invalid_argument) {
    return false;
  }
  auto dedicated = profile;
  dedicated.flags |= MF_VULKAN_MEMORY_FLAG_DEDICATED_ONLY_V0;
  return ledger.import(dedicated, 256U, 0U, &first) == ExternalMemoryStatus::invalid_argument &&
         ledger.configure(8U, UINT32_C(0x3), UINT32_C(0x1), UINT32_C(0x1), 2U) ==
             ExternalMemoryStatus::success;
}

bool external_memory_fd_ownership() {
  using metaflux::backend::vulkan::ExternalMemoryHandleImport;
  using metaflux::backend::vulkan::ExternalMemoryHandleLedger;
  using metaflux::backend::vulkan::ExternalMemoryStatus;
  int pipe_fds[2] = {-1, -1};
  if (::pipe(pipe_fds) != 0) {
    return false;
  }
  ExternalMemoryHandleImport imported{};
  bool result = false;
  {
    ExternalMemoryHandleLedger ledger(7U, UINT32_C(0x3), UINT32_C(0x1), UINT32_C(0x1), 1U);
    const auto profile = direct_profile();
    auto rejected = profile;
    rejected.generation = 8U;
    if (ledger.import_fd(rejected, pipe_fds[0], 0U, 0U, &imported) !=
            ExternalMemoryStatus::stale_generation ||
        ::fcntl(pipe_fds[0], F_GETFD) < 0 || ledger.active_count() != 0U ||
        ledger.import_fd(profile, -1, 0U, 0U, &imported) !=
            ExternalMemoryStatus::invalid_argument ||
        ledger.import_fd(profile, pipe_fds[0], 0U, 0U, &imported) !=
            ExternalMemoryStatus::success ||
        imported.owned_fd < 0 || imported.owned_fd == pipe_fds[0] ||
        ::fcntl(imported.owned_fd, F_GETFD) < 0 ||
        ledger.retain(imported) != ExternalMemoryStatus::success ||
        ledger.revoke(imported) != ExternalMemoryStatus::busy ||
        ledger.release(imported) != ExternalMemoryStatus::success ||
        ::fcntl(imported.owned_fd, F_GETFD) < 0 ||
        ledger.release(imported) != ExternalMemoryStatus::success ||
        (errno = 0, ::fcntl(imported.owned_fd, F_GETFD) != -1) || errno != EBADF ||
        ledger.active_count() != 0U) {
      ::close(pipe_fds[0]);
      ::close(pipe_fds[1]);
      return false;
    }
    result = true;
  }
  ::close(pipe_fds[0]);
  ::close(pipe_fds[1]);
  return result;
}

bool staging_lifetime_and_reuse() {
  metaflux::backend::vulkan::StagingLedger ledger(profile(), 7U);
  if (ledger.capacity() != 4096U || ledger.generation() != 7U) {
    return false;
  }
  metaflux::backend::vulkan::StagingAllocation first{};
  metaflux::backend::vulkan::StagingAllocation second{};
  if (ledger.allocate(256U, 256U, 7U, &first) != metaflux::backend::vulkan::MemoryStatus::success ||
      ledger.allocate(513U, 512U, 7U, &second) !=
          metaflux::backend::vulkan::MemoryStatus::success ||
      first.offset != 0U || second.offset != 512U || first.offset + first.size > second.offset ||
      ledger.active_bytes() != 769U ||
      ledger.validate(first) != metaflux::backend::vulkan::MemoryStatus::success) {
    return false;
  }
  auto stale = first;
  stale.generation = 8U;
  if (ledger.validate(stale) != metaflux::backend::vulkan::MemoryStatus::stale_generation ||
      ledger.release(first) != metaflux::backend::vulkan::MemoryStatus::success ||
      ledger.allocate(128U, 128U, 7U, &first) != metaflux::backend::vulkan::MemoryStatus::success ||
      first.offset != 0U) {
    return false;
  }
  return ledger.allocate(4096U, 1U, 7U, &stale) ==
             metaflux::backend::vulkan::MemoryStatus::out_of_memory &&
         ledger.release(second) == metaflux::backend::vulkan::MemoryStatus::success &&
         ledger.release(second) == metaflux::backend::vulkan::MemoryStatus::not_found;
}

bool profile_and_generation_guards() {
  auto unsupported = profile();
  unsupported.memory_tier_flags = 0U;
  metaflux::backend::vulkan::StagingLedger ledger;
  if (ledger.configure(unsupported, 1U) != metaflux::backend::vulkan::MemoryStatus::unsupported) {
    return false;
  }
  metaflux::backend::vulkan::StagingLedger active(profile(), 7U);
  metaflux::backend::vulkan::StagingAllocation active_allocation{};
  if (active.allocate(64U, 64U, 7U, &active_allocation) !=
          metaflux::backend::vulkan::MemoryStatus::success ||
      active.configure(profile(), 8U) != metaflux::backend::vulkan::MemoryStatus::busy ||
      active.configure(unsupported, 8U) != metaflux::backend::vulkan::MemoryStatus::unsupported ||
      active.generation() != 7U || active.active_bytes() != 64U ||
      active.validate(active_allocation) != metaflux::backend::vulkan::MemoryStatus::success) {
    return false;
  }
  metaflux::backend::vulkan::StagingAllocation allocation{};
  const auto status = ledger.configure(profile(), 3U);
  return status == metaflux::backend::vulkan::MemoryStatus::success &&
         ledger.allocate(64U, 3U, 3U, &allocation) ==
             metaflux::backend::vulkan::MemoryStatus::invalid_argument &&
         ledger.allocate(64U, 64U, 4U, &allocation) ==
             metaflux::backend::vulkan::MemoryStatus::stale_generation;
}

bool timeline_guards() {
  metaflux::backend::vulkan::TimelineGate timeline(11U);
  metaflux::backend::vulkan::TimelineSubmission first{};
  metaflux::backend::vulkan::TimelineSubmission second{};
  if (timeline.submit(11U, &first) != metaflux::backend::vulkan::MemoryStatus::success ||
      timeline.submit(11U, &second) != metaflux::backend::vulkan::MemoryStatus::success ||
      first.value != 1U || second.value != 2U ||
      timeline.wait(11U, first.value) != metaflux::backend::vulkan::MemoryStatus::busy ||
      timeline.complete(11U, first.value) != metaflux::backend::vulkan::MemoryStatus::success ||
      timeline.wait(11U, first.value) != metaflux::backend::vulkan::MemoryStatus::success ||
      timeline.complete(11U, 3U) != metaflux::backend::vulkan::MemoryStatus::invalid_argument ||
      timeline.wait(0U, first.value) !=
          metaflux::backend::vulkan::MemoryStatus::invalid_argument ||
      timeline.complete(0U, first.value) !=
          metaflux::backend::vulkan::MemoryStatus::invalid_argument ||
      timeline.complete(10U, second.value) !=
          metaflux::backend::vulkan::MemoryStatus::stale_generation) {
    return false;
  }
  return timeline.complete(11U, second.value) == metaflux::backend::vulkan::MemoryStatus::success &&
         timeline.wait(11U, second.value) == metaflux::backend::vulkan::MemoryStatus::success;
}

bool non_coherent_visibility() {
  using metaflux::backend::vulkan::MemoryVisibilityLedger;
  using metaflux::backend::vulkan::VisibilityStatus;
  metaflux::backend::vulkan::StagingAllocation allocation{
      .id = 9U, .generation = 7U, .offset = 0U, .size = 256U, .alignment = 64U};
  MemoryVisibilityLedger visibility(7U, 64U);
  if (!expect_visibility("register", visibility.register_allocation(allocation, false),
                         VisibilityStatus::success) ||
      !expect_visibility("host-write", visibility.host_write(allocation, 3U, 1U, 7U),
                         VisibilityStatus::success) ||
      !expect_visibility("submit", visibility.submit(7U, 1U), VisibilityStatus::success) ||
      !expect_visibility("read-before-flush", visibility.device_read(allocation, 3U, 1U, 7U, 1U),
                         VisibilityStatus::flush_required) ||
      !expect_visibility("flush", visibility.flush(allocation, 3U, 1U, 7U),
                         VisibilityStatus::success) ||
      !expect_visibility("read-after-flush", visibility.device_read(allocation, 3U, 1U, 7U, 1U),
                         VisibilityStatus::success) ||
      !expect_visibility("host-read-in-flight", visibility.host_read(allocation, 3U, 1U, 7U),
                         VisibilityStatus::busy) ||
      !expect_visibility("complete", visibility.complete(7U, 1U), VisibilityStatus::success) ||
      !expect_visibility("host-read-after-complete", visibility.host_read(allocation, 3U, 1U, 7U),
                         VisibilityStatus::success)) {
    return false;
  }
  if (!expect_visibility("old-timeline", visibility.device_write(allocation, 64U, 64U, 7U, 1U),
                         VisibilityStatus::invalid_argument)) {
    return false;
  }
  MemoryVisibilityLedger second(7U, 64U);
  if (!expect_visibility("second-register", second.register_allocation(allocation, false),
                         VisibilityStatus::success) ||
      !expect_visibility("second-submit", second.submit(7U, 4U), VisibilityStatus::success) ||
      !expect_visibility("device-write", second.device_write(allocation, 64U, 64U, 7U, 4U),
                         VisibilityStatus::success) ||
      !expect_visibility("second-host-read-in-flight", second.host_read(allocation, 64U, 64U, 7U),
                         VisibilityStatus::busy) ||
      !expect_visibility("second-complete", second.complete(7U, 4U), VisibilityStatus::success) ||
      !expect_visibility("host-read-before-invalidate", second.host_read(allocation, 64U, 64U, 7U),
                         VisibilityStatus::invalidate_required) ||
      !expect_visibility("invalidate", second.invalidate(allocation, 64U, 64U, 7U),
                         VisibilityStatus::success) ||
      !expect_visibility("host-read-after-invalidate", second.host_read(allocation, 64U, 64U, 7U),
                         VisibilityStatus::success) ||
      !expect_visibility("unregister", second.unregister_allocation(allocation),
                         VisibilityStatus::success)) {
    return false;
  }
  return true;
}

bool visibility_range_and_coherent_guards() {
  using metaflux::backend::vulkan::MemoryVisibilityLedger;
  using metaflux::backend::vulkan::VisibilityStatus;
  metaflux::backend::vulkan::StagingAllocation unaligned{
      .id = 1U, .generation = 2U, .offset = 0U, .size = 63U, .alignment = 1U};
  MemoryVisibilityLedger visibility(2U, 64U);
  if (!expect_visibility("unaligned-register", visibility.register_allocation(unaligned, false),
                         VisibilityStatus::invalid_argument) ||
      !expect_visibility("coherent-register", visibility.register_allocation(unaligned, true),
                         VisibilityStatus::success) ||
      !expect_visibility("coherent-host-write", visibility.host_write(unaligned, 62U, 1U, 2U),
                         VisibilityStatus::success) ||
      !expect_visibility("coherent-submit", visibility.submit(2U, 1U), VisibilityStatus::success) ||
      !expect_visibility("coherent-device-read", visibility.device_read(unaligned, 62U, 1U, 2U, 1U),
                         VisibilityStatus::success) ||
      !expect_visibility("coherent-complete", visibility.complete(2U, 1U),
                         VisibilityStatus::success) ||
      !expect_visibility("coherent-host-read", visibility.host_read(unaligned, 62U, 1U, 2U),
                         VisibilityStatus::success) ||
      !expect_visibility("zero-range", visibility.host_read(unaligned, 0U, 0U, 2U),
                         VisibilityStatus::range_out_of_bounds) ||
      !expect_visibility("overflow-range", visibility.host_read(unaligned, 62U, 3U, 2U),
                         VisibilityStatus::range_out_of_bounds) ||
      !expect_visibility("stale-access", visibility.host_read(unaligned, 0U, 1U, 3U),
                         VisibilityStatus::stale_generation) ||
      !expect_visibility("future-complete", visibility.complete(2U, 2U),
                         VisibilityStatus::invalid_argument)) {
    return false;
  }
  return visibility.configure(0U, 64U) == VisibilityStatus::invalid_argument;
}

bool visibility_partial_and_lifetime_guards() {
  using metaflux::backend::vulkan::MemoryVisibilityLedger;
  using metaflux::backend::vulkan::VisibilityStatus;
  metaflux::backend::vulkan::StagingAllocation allocation{
      .id = 11U, .generation = 5U, .offset = 0U, .size = 256U, .alignment = 64U};
  MemoryVisibilityLedger visibility(5U, 64U);
  if (!expect_visibility("partial-register", visibility.register_allocation(allocation, false),
                         VisibilityStatus::success) ||
      !expect_visibility("duplicate-register", visibility.register_allocation(allocation, false),
                         VisibilityStatus::invalid_argument) ||
      !expect_visibility("partial-host-write-a", visibility.host_write(allocation, 0U, 32U, 5U),
                         VisibilityStatus::success) ||
      !expect_visibility("partial-host-write-b", visibility.host_write(allocation, 128U, 32U, 5U),
                         VisibilityStatus::success) ||
      !expect_visibility("partial-flush-a", visibility.flush(allocation, 0U, 32U, 5U),
                         VisibilityStatus::success) ||
      !expect_visibility("partial-submit-a", visibility.submit(5U, 1U),
                         VisibilityStatus::success) ||
      !expect_visibility("partial-device-read-a",
                         visibility.device_read(allocation, 0U, 32U, 5U, 1U),
                         VisibilityStatus::success) ||
      !expect_visibility("partial-unregister-in-flight",
                         visibility.unregister_allocation(allocation), VisibilityStatus::busy) ||
      !expect_visibility("partial-reconfigure-in-flight", visibility.configure(6U, 64U),
                         VisibilityStatus::busy) ||
      !expect_visibility("partial-flush-in-flight", visibility.flush(allocation, 128U, 32U, 5U),
                         VisibilityStatus::busy) ||
      !expect_visibility("partial-complete-a", visibility.complete(5U, 1U),
                         VisibilityStatus::success) ||
      !expect_visibility("partial-submit-b", visibility.submit(5U, 2U),
                         VisibilityStatus::success) ||
      !expect_visibility("partial-device-read-b-before-flush",
                         visibility.device_read(allocation, 128U, 32U, 5U, 2U),
                         VisibilityStatus::flush_required) ||
      !expect_visibility("partial-flush-b", visibility.flush(allocation, 128U, 32U, 5U),
                         VisibilityStatus::success) ||
      !expect_visibility("partial-device-write-b",
                         visibility.device_write(allocation, 128U, 32U, 5U, 2U),
                         VisibilityStatus::success) ||
      !expect_visibility("partial-complete-b", visibility.complete(5U, 2U),
                         VisibilityStatus::success) ||
      !expect_visibility("partial-host-write-before-invalidate",
                         visibility.host_write(allocation, 128U, 32U, 5U),
                         VisibilityStatus::invalidate_required) ||
      !expect_visibility("partial-invalidate-b", visibility.invalidate(allocation, 128U, 32U, 5U),
                         VisibilityStatus::success) ||
      !expect_visibility("partial-host-write-after-invalidate",
                         visibility.host_write(allocation, 128U, 32U, 5U),
                         VisibilityStatus::success) ||
      !expect_visibility("partial-flush-after-write", visibility.flush(allocation, 128U, 32U, 5U),
                         VisibilityStatus::success) ||
      !expect_visibility("partial-unregister", visibility.unregister_allocation(allocation),
                         VisibilityStatus::success)) {
    return false;
  }
  return visibility.complete(5U, 3U) == VisibilityStatus::invalid_argument;
}

} // namespace

int main() {
  const bool ok = external_memory_admission_and_drain() && external_memory_fd_ownership() &&
                  staging_lifetime_and_reuse() &&
                  profile_and_generation_guards() &&
                  timeline_guards() && non_coherent_visibility() &&
                  visibility_range_and_coherent_guards() &&
                  visibility_partial_and_lifetime_guards();
  std::printf("vulkan memory model: %s\n", ok ? "pass" : "fail");
  return ok ? 0 : 1;
}
