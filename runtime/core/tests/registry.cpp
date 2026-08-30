#include "metaflux/runtime/core.hpp"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <thread>
#include <type_traits>

#include <sys/mman.h>

using metaflux::runtime::FenceSnapshot;
using metaflux::runtime::RegistryView;
using metaflux::runtime::TelemetrySnapshot;

static_assert(std::is_standard_layout_v<mf_shared_registry_header_v1>);
static_assert(std::is_standard_layout_v<mf_virtual_device_identity_v1>);
static_assert(std::is_standard_layout_v<mf_ring_descriptor_v1>);
static_assert(alignof(mf_shared_registry_header_v1) == 64);
static_assert(alignof(mf_virtual_device_identity_v1) == 64);
static_assert(offsetof(mf_shared_registry_header_v1, registry_view_id) == 24);
static_assert(offsetof(mf_shared_registry_header_v1, identities_offset) == 72);
static_assert(offsetof(mf_virtual_device_identity_v1, committed_generation) == 104);
static_assert(offsetof(mf_virtual_device_lifecycle_fence_v1, device_state) == 48);
static_assert(sizeof(TelemetrySnapshot) == 80);
static_assert(offsetof(TelemetrySnapshot, memory_active_time_ns) == 40);

namespace {

constexpr std::uint64_t kPolicyPattern = UINT64_C(0x55aa55aa55aa55aa);

[[nodiscard]] void* allocate_mapping(std::uint64_t bytes) {
  return mmap(nullptr, static_cast<std::size_t>(bytes), PROT_READ | PROT_WRITE,
              MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
}

void release_mapping(void* mapping, std::uint64_t bytes) {
  if (mapping != MAP_FAILED) {
    (void)munmap(mapping, static_cast<std::size_t>(bytes));
  }
}

[[nodiscard]] mf_virtual_device_identity_v1 make_identity() {
  mf_virtual_device_identity_v1 identity{};
  identity.identity_record_id = UINT64_C(101);
  identity.logical_device_id[0] = UINT8_C(0x11);
  identity.gpu_uuid[0] = UINT8_C(0x22);
  identity.display_name[0] = static_cast<std::uint8_t>('M');
  identity.display_name[1] = static_cast<std::uint8_t>('F');
  identity.committed_generation = UINT64_C(7);
  identity.capability_bits = UINT64_C(0x13);
  identity.backend_id = UINT32_C(1);
  identity.virtual_compute_capability = UINT32_C(0x0900);
  identity.pci_domain = UINT32_C(0);
  identity.pci_bus = UINT32_C(1);
  identity.pci_device = UINT32_C(2);
  identity.pci_function = UINT32_C(0);
  return identity;
}

[[nodiscard]] bool fence_consistent(const FenceSnapshot& fence) {
  if (fence.lifecycle_sequence < 3U) {
    return true;
  }
  const std::uint64_t value = fence.lifecycle_sequence - 2U;
  return fence.epoch == value && fence.effective_quota_bytes == (value ^ kPolicyPattern) &&
         fence.policy_bits == ~value && fence.device_state == MF_DEVICE_STATE_ONLINE;
}

[[nodiscard]] bool legacy_telemetry_fence_tests() {
  std::uint64_t mapping_size = 0;
  if (RegistryView::required_legacy_mapping_size(UINT32_C(1), mapping_size) != MF_SHARED_SUCCESS) {
    return false;
  }
  void* mapping = allocate_mapping(mapping_size);
  if (mapping == MAP_FAILED) {
    return false;
  }
  std::array<mf_virtual_device_identity_v1, 1> identities{make_identity()};
  std::array<FenceSnapshot, 1> fences{{
      {
          .identity_record_id = UINT64_C(101),
          .lifecycle_sequence = UINT64_C(1),
          .epoch = UINT64_C(0),
          .effective_quota_bytes = UINT64_C(4096),
          .policy_bits = UINT64_C(7),
          .device_state = MF_DEVICE_STATE_ONLINE,
      },
  }};
  RegistryView view;
  bool ok = RegistryView::initialize(mapping, mapping_size, {UINT64_C(0xdef), UINT64_C(1)},
                                     UINT64_C(1), identities, fences, view) == MF_SHARED_SUCCESS;
  std::array<mf_virtual_device_telemetry_v1, 1> rows{};
  rows[0].identity_record_id = UINT64_C(101);
  rows[0].observed_lifecycle_sequence = UINT64_C(1);
  ok = ok && view.publish_telemetry(rows) == MF_SHARED_SUCCESS;

  std::uint32_t validation_generation = UINT32_C(1);
  const FenceSnapshot next{
      .identity_record_id = UINT64_C(101),
      .lifecycle_sequence = UINT64_C(2),
      .epoch = UINT64_C(1),
      .effective_quota_bytes = UINT64_C(8192),
      .policy_bits = UINT64_C(9),
      .device_state = MF_DEVICE_STATE_ONLINE,
  };
  ok = ok && view.publish_fence(UINT32_C(0), validation_generation, next, validation_generation) ==
                 MF_SHARED_SUCCESS;
  ok = ok && view.publish_telemetry(rows) == MF_SHARED_RETRY;
  rows[0].observed_lifecycle_sequence = UINT64_C(3);
  ok = ok && view.publish_telemetry(rows) == MF_SHARED_RETRY;
  ok = ok && view.mark_device_lost(UINT32_C(0), UINT64_C(3), UINT64_C(2)) == MF_SHARED_SUCCESS;
  rows[0].observed_lifecycle_sequence = UINT64_C(2);
  ok = ok && view.publish_telemetry(rows) == MF_SHARED_DEVICE_LOST;
  ok = ok && view.close() == MF_SHARED_SUCCESS;
  release_mapping(mapping, mapping_size);
  return ok;
}

} // namespace

int main() {
  if (!legacy_telemetry_fence_tests()) {
    return 15;
  }
  std::uint64_t mapping_size = 0;
  if (RegistryView::required_recovery_mapping_size(UINT32_C(1), mapping_size) !=
          MF_SHARED_SUCCESS ||
      (mapping_size % MF_SHARED_CACHE_LINE_SIZE) != 0U) {
    return 1;
  }
  void* mapping = allocate_mapping(mapping_size);
  if (mapping == MAP_FAILED) {
    return 2;
  }

  std::array<mf_virtual_device_identity_v1, 1> identities{make_identity()};
  std::array<FenceSnapshot, 1> fences{{
      {
          .identity_record_id = UINT64_C(101),
          .lifecycle_sequence = UINT64_C(1),
          .epoch = UINT64_C(0),
          .effective_quota_bytes = UINT64_C(4096),
          .policy_bits = UINT64_C(7),
          .device_state = MF_DEVICE_STATE_ONLINE,
      },
  }};
  RegistryView view;
  const mf_registry_view_id_v1 first_view_id{UINT64_C(0xabc), UINT64_C(1)};
  if (RegistryView::initialize(mapping, mapping_size, first_view_id, UINT64_C(1), identities,
                               fences, view) != MF_SHARED_SUCCESS ||
      view.device_count() != UINT32_C(1)) {
    release_mapping(mapping, mapping_size);
    return 3;
  }

  mf_generation_handle_v1 handle{};
  FenceSnapshot observed{};
  if (view.make_handle(UINT32_C(0), UINT64_C(55), UINT64_C(1), UINT32_C(9), handle) !=
          MF_SHARED_SUCCESS ||
      view.validate_device(handle, observed) != MF_SHARED_SUCCESS ||
      observed.lifecycle_sequence != UINT64_C(1)) {
    release_mapping(mapping, mapping_size);
    return 4;
  }
  mf_generation_handle_v1 stale = handle;
  stale.device_generation += UINT64_C(1);
  if (view.validate_device(stale, observed) != MF_SHARED_STALE_HANDLE) {
    release_mapping(mapping, mapping_size);
    return 5;
  }

  std::uint32_t validation_generation = UINT32_C(1);
  FenceSnapshot policy{
      .identity_record_id = UINT64_C(101),
      .lifecycle_sequence = UINT64_C(2),
      .epoch = UINT64_C(0),
      .effective_quota_bytes = UINT64_C(8192),
      .policy_bits = UINT64_C(11),
      .device_state = MF_DEVICE_STATE_ONLINE,
  };
  if (view.publish_fence(UINT32_C(0), validation_generation, policy, validation_generation) !=
          MF_SHARED_SUCCESS ||
      validation_generation != UINT32_C(2) ||
      view.validate_device(handle, observed) != MF_SHARED_SUCCESS ||
      observed.policy_bits != UINT64_C(11)) {
    release_mapping(mapping, mapping_size);
    return 6;
  }

  std::array<mf_virtual_device_telemetry_v1, 1> telemetry{};
  telemetry[0].identity_record_id = UINT64_C(101);
  telemetry[0].observed_lifecycle_sequence = UINT64_C(2);
  telemetry[0].committed_work_items = UINT64_C(10);
  telemetry[0].completed_work_items = UINT64_C(10);
  telemetry[0].active_time_ns = UINT64_C(10) ^ kPolicyPattern;
  telemetry[0].memory_active_time_ns = ~UINT64_C(10);
  telemetry[0].memory_capacity_bytes = UINT64_C(16384);
  telemetry[0].sample_time_ns = UINT64_C(110);
  if (view.publish_telemetry(telemetry) != MF_SHARED_SUCCESS) {
    release_mapping(mapping, mapping_size);
    return 7;
  }
  TelemetrySnapshot telemetry_snapshot{};
  if (view.read_telemetry(handle, telemetry_snapshot) != MF_SHARED_SUCCESS ||
      telemetry_snapshot.committed_work_items != UINT64_C(10) ||
      telemetry_snapshot.completed_work_items != UINT64_C(10) ||
      telemetry_snapshot.memory_active_time_ns != ~UINT64_C(10) ||
      telemetry_snapshot.snapshot_sequence != UINT64_C(1)) {
    release_mapping(mapping, mapping_size);
    return 8;
  }

  std::atomic<bool> telemetry_done{false};
  std::atomic<bool> telemetry_failed{false};
  std::thread telemetry_writer([&view, &telemetry, &telemetry_done, &telemetry_failed] {
    for (std::uint64_t value = 1; value <= UINT64_C(10000); ++value) {
      telemetry[0].committed_work_items = value;
      telemetry[0].completed_work_items = value;
      telemetry[0].active_time_ns = value ^ kPolicyPattern;
      telemetry[0].memory_active_time_ns = ~value;
      telemetry[0].sample_time_ns = value + UINT64_C(100);
      if (view.publish_telemetry(telemetry) != MF_SHARED_SUCCESS) {
        telemetry_failed.store(true, std::memory_order_release);
        break;
      }
    }
    telemetry_done.store(true, std::memory_order_release);
  });
  while (!telemetry_done.load(std::memory_order_acquire)) {
    const mf_shared_status_v1 status = view.read_telemetry(handle, telemetry_snapshot);
    if (status == MF_SHARED_SUCCESS &&
        (telemetry_snapshot.committed_work_items != telemetry_snapshot.completed_work_items ||
         telemetry_snapshot.active_time_ns !=
             (telemetry_snapshot.committed_work_items ^ kPolicyPattern) ||
         telemetry_snapshot.memory_active_time_ns != ~telemetry_snapshot.committed_work_items ||
         telemetry_snapshot.sample_time_ns !=
             telemetry_snapshot.committed_work_items + UINT64_C(100))) {
      telemetry_failed.store(true, std::memory_order_release);
      break;
    }
    if (status != MF_SHARED_SUCCESS && status != MF_SHARED_RETRY) {
      telemetry_failed.store(true, std::memory_order_release);
      break;
    }
  }
  telemetry_writer.join();
  if (telemetry_failed.load(std::memory_order_acquire)) {
    release_mapping(mapping, mapping_size);
    return 9;
  }

  std::atomic<bool> fence_done{false};
  std::atomic<bool> fence_failed{false};
  std::thread fence_writer([&view, &validation_generation, &fence_done, &fence_failed] {
    for (std::uint64_t value = 1; value <= UINT64_C(20000); ++value) {
      FenceSnapshot next{
          .identity_record_id = UINT64_C(101),
          .lifecycle_sequence = value + UINT64_C(2),
          .epoch = value,
          .effective_quota_bytes = value ^ kPolicyPattern,
          .policy_bits = ~value,
          .device_state = MF_DEVICE_STATE_ONLINE,
      };
      if (view.publish_fence(UINT32_C(0), validation_generation, next, validation_generation) !=
          MF_SHARED_SUCCESS) {
        fence_failed.store(true, std::memory_order_release);
        break;
      }
    }
    fence_done.store(true, std::memory_order_release);
  });
  while (!fence_done.load(std::memory_order_acquire)) {
    const mf_shared_status_v1 status = view.validate_device(handle, observed);
    if (status == MF_SHARED_SUCCESS && !fence_consistent(observed)) {
      fence_failed.store(true, std::memory_order_release);
      break;
    }
    if (status != MF_SHARED_SUCCESS && status != MF_SHARED_RETRY) {
      fence_failed.store(true, std::memory_order_release);
      break;
    }
  }
  fence_writer.join();
  if (fence_failed.load(std::memory_order_acquire) ||
      view.validate_device(handle, observed) != MF_SHARED_SUCCESS || !fence_consistent(observed)) {
    release_mapping(mapping, mapping_size);
    return 10;
  }

  if (view.mark_device_lost(UINT32_C(0), UINT64_C(30000), UINT64_C(20001)) != MF_SHARED_SUCCESS ||
      view.validate_device(handle, observed) != MF_SHARED_DEVICE_LOST) {
    release_mapping(mapping, mapping_size);
    return 11;
  }
  if (view.close() != MF_SHARED_SUCCESS ||
      view.validate_device(handle, observed) != MF_SHARED_TERMINAL_VIEW) {
    release_mapping(mapping, mapping_size);
    return 12;
  }

  void* second_mapping = allocate_mapping(mapping_size);
  if (second_mapping == MAP_FAILED) {
    release_mapping(mapping, mapping_size);
    return 13;
  }
  RegistryView second_view;
  const mf_registry_view_id_v1 second_view_id{UINT64_C(0xabc), UINT64_C(2)};
  if (RegistryView::initialize(second_mapping, mapping_size, second_view_id, UINT64_C(2),
                               identities, fences, second_view) != MF_SHARED_SUCCESS ||
      second_view.validate_device(handle, observed) != MF_SHARED_STALE_HANDLE) {
    release_mapping(second_mapping, mapping_size);
    release_mapping(mapping, mapping_size);
    return 14;
  }
  (void)second_view.close();
  release_mapping(second_mapping, mapping_size);
  release_mapping(mapping, mapping_size);
  return 0;
}
