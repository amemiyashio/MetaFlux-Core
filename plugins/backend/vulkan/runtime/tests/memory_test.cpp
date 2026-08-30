#include "metaflux/backend/vulkan_memory.hpp"

#include <cstdint>
#include <cstdio>

namespace {

mf_vulkan_capability_profile_v1 profile() {
  mf_vulkan_capability_profile_v1 result{};
  result.status = MF_VULKAN_PROBE_SUCCESS;
  result.memory_tier_flags = MF_VULKAN_MEMORY_TIER_STAGING;
  result.device_local_heap_bytes = 4096U;
  result.host_visible_heap_bytes = 8192U;
  return result;
}

bool staging_lifetime_and_reuse() {
  metaflux::backend::vulkan::StagingLedger ledger(profile(), 7U);
  if (ledger.capacity() != 4096U || ledger.generation() != 7U) {
    return false;
  }
  metaflux::backend::vulkan::StagingAllocation first{};
  metaflux::backend::vulkan::StagingAllocation second{};
  if (ledger.allocate(256U, 256U, 7U, &first) !=
          metaflux::backend::vulkan::MemoryStatus::success ||
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
      ledger.allocate(128U, 128U, 7U, &first) !=
          metaflux::backend::vulkan::MemoryStatus::success ||
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
      timeline.complete(10U, second.value) != metaflux::backend::vulkan::MemoryStatus::stale_generation) {
    return false;
  }
  return timeline.complete(11U, second.value) == metaflux::backend::vulkan::MemoryStatus::success &&
         timeline.wait(11U, second.value) == metaflux::backend::vulkan::MemoryStatus::success;
}

} // namespace

int main() {
  const bool ok = staging_lifetime_and_reuse() && profile_and_generation_guards() && timeline_guards();
  std::printf("vulkan memory model: %s\n", ok ? "pass" : "fail");
  return ok ? 0 : 1;
}
