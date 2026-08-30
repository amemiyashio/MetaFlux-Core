#include "metaflux/runtime/core.hpp"

#include <array>
#include <csignal>
#include <cstddef>
#include <cstdint>
#include <span>

#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

using metaflux::runtime::AdmissionLeaseToken;
using metaflux::runtime::FenceSnapshot;
using metaflux::runtime::LifecycleRangeToken;
using metaflux::runtime::RecoveryFaultPoint;
using metaflux::runtime::RegistryView;

namespace {

[[nodiscard]] mf_virtual_device_identity_v1 identity() {
  mf_virtual_device_identity_v1 value{};
  value.identity_record_id = UINT64_C(77);
  value.logical_device_id[0] = UINT8_C(0x17);
  value.gpu_uuid[0] = UINT8_C(0x27);
  value.display_name[0] = static_cast<std::uint8_t>('R');
  value.committed_generation = UINT64_C(9);
  value.backend_id = UINT32_C(1);
  return value;
}

[[nodiscard]] FenceSnapshot initial_fence() {
  return {
      .identity_record_id = UINT64_C(77),
      .lifecycle_sequence = UINT64_C(1),
      .epoch = UINT64_C(1),
      .effective_quota_bytes = UINT64_C(4096),
      .policy_bits = UINT64_C(3),
      .device_state = MF_DEVICE_STATE_ONLINE,
  };
}

[[nodiscard]] void* mapping(std::uint64_t size) {
  return mmap(nullptr, static_cast<std::size_t>(size), PROT_READ | PROT_WRITE,
              MAP_SHARED | MAP_ANONYMOUS, -1, 0);
}

void unmap(void* address, std::uint64_t size) {
  if (address != MAP_FAILED) {
    (void)munmap(address, static_cast<std::size_t>(size));
  }
}

[[nodiscard]] bool initialize_one(void* address, std::uint64_t size, std::uint64_t serial,
                                  RegistryView& view) {
  const std::array<mf_virtual_device_identity_v1, 1> identities{identity()};
  const std::array<FenceSnapshot, 1> fences{initial_fence()};
  return RegistryView::initialize(address, size, {UINT64_C(0xa11ce), serial}, serial, identities,
                                  fences, view) == MF_SHARED_SUCCESS;
}

template <typename Function> [[nodiscard]] bool child_succeeds(Function function) {
  const pid_t pid = fork();
  if (pid < 0) {
    return false;
  }
  if (pid == 0) {
    _exit(function() ? 0 : 1);
  }
  int status = 0;
  return waitpid(pid, &status, 0) == pid && WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

[[nodiscard]] bool wait_for_child_stop(pid_t pid) {
  for (std::uint32_t attempt = 0; attempt < 5000U; ++attempt) {
    int status = 0;
    const pid_t waited = waitpid(pid, &status, WUNTRACED | WNOHANG);
    if (waited == pid) {
      return WIFSTOPPED(status) && WSTOPSIG(status) == SIGSTOP;
    }
    if (waited < 0) {
      return false;
    }
    (void)usleep(1000U);
  }
  (void)kill(pid, SIGKILL);
  (void)waitpid(pid, nullptr, 0);
  return false;
}

[[nodiscard]] bool resume_child_and_succeed(pid_t pid) {
  if (kill(pid, SIGCONT) != 0) {
    (void)kill(pid, SIGKILL);
    (void)waitpid(pid, nullptr, 0);
    return false;
  }
  for (std::uint32_t attempt = 0; attempt < 5000U; ++attempt) {
    int status = 0;
    const pid_t waited = waitpid(pid, &status, WNOHANG);
    if (waited == pid) {
      return WIFEXITED(status) && WEXITSTATUS(status) == 0;
    }
    if (waited < 0) {
      return false;
    }
    (void)usleep(1000U);
  }
  (void)kill(pid, SIGKILL);
  (void)waitpid(pid, nullptr, 0);
  return false;
}

[[nodiscard]] mf_shared_registry_extension_header_v1* extension(void* address,
                                                                std::uint32_t device_count) {
  std::uint64_t legacy_size = 0;
  if (RegistryView::required_legacy_mapping_size(device_count, legacy_size) != MF_SHARED_SUCCESS) {
    return nullptr;
  }
  return reinterpret_cast<mf_shared_registry_extension_header_v1*>(
      static_cast<std::uint8_t*>(address) + legacy_size);
}

[[nodiscard]] FenceSnapshot fence_at(std::uint64_t lifecycle_sequence) {
  return {
      .identity_record_id = UINT64_C(77),
      .lifecycle_sequence = lifecycle_sequence,
      .epoch = lifecycle_sequence,
      .effective_quota_bytes = UINT64_C(4096) * lifecycle_sequence,
      .policy_bits = lifecycle_sequence,
      .device_state = MF_DEVICE_STATE_ONLINE,
  };
}

[[nodiscard]] bool publish_interrupted_fence(void* address, std::uint64_t size,
                                             std::uint32_t expected_generation,
                                             std::uint64_t lifecycle_sequence,
                                             RecoveryFaultPoint fault_point) {
  return child_succeeds([&] {
    RegistryView child;
    std::uint32_t generation = expected_generation;
    return RegistryView::attach(address, size, child) == MF_SHARED_SUCCESS &&
           (child.set_fault_point_for_testing(fault_point),
            child.publish_fence(0U, generation, fence_at(lifecycle_sequence), generation)) ==
               MF_SHARED_INTERRUPTED;
  });
}

[[nodiscard]] bool layout_and_zero_tests() {
  std::uint64_t legacy_size = 0;
  std::uint64_t full_size = 0;
  if (RegistryView::required_legacy_mapping_size(1U, legacy_size) != MF_SHARED_SUCCESS ||
      RegistryView::required_recovery_mapping_size(1U, full_size) != MF_SHARED_SUCCESS ||
      full_size <= legacy_size) {
    return false;
  }
  void* legacy = mapping(legacy_size);
  void* full = mapping(full_size);
  if (legacy == MAP_FAILED || full == MAP_FAILED) {
    unmap(legacy, legacy_size);
    unmap(full, full_size);
    return false;
  }
  RegistryView legacy_view;
  RegistryView full_view;
  RegistryView attached;
  bool ok = initialize_one(legacy, legacy_size, 1U, legacy_view) &&
            !legacy_view.has_recovery_extension() &&
            RegistryView::attach(legacy, legacy_size, attached) == MF_SHARED_SUCCESS &&
            initialize_one(full, full_size, 2U, full_view) && full_view.has_recovery_extension();
  auto* header = static_cast<mf_shared_registry_header_v1*>(full);
  auto* ext = extension(full, 1U);
  if (ext == nullptr) {
    ok = false;
  } else {
    const mf_shared_registry_header_v1 saved_header = *header;
    const mf_shared_registry_extension_header_v1 saved_extension = *ext;
    header->flags |= UINT32_C(0x80000000);
    ok = ok && RegistryView::attach(full, full_size, attached) == MF_SHARED_MALFORMED;
    *header = saved_header;
    ext->admission_lease_capacity = UINT32_MAX;
    ok = ok && RegistryView::attach(full, full_size, attached) == MF_SHARED_MALFORMED;
    *ext = saved_extension;
    ext->device_updates_offset += 1U;
    ok = ok && RegistryView::attach(full, full_size, attached) == MF_SHARED_MALFORMED;
    *ext = saved_extension;
    ext->telemetry_publish_records_offset = UINT64_MAX - UINT64_C(63);
    ok = ok && RegistryView::attach(full, full_size, attached) == MF_SHARED_MALFORMED;
    *ext = saved_extension;
    header->device_count = UINT32_MAX;
    ok = ok && RegistryView::attach(full, full_size, attached) == MF_SHARED_MALFORMED;
    *header = saved_header;
  }
  unmap(legacy, legacy_size);
  unmap(full, full_size);

  std::uint64_t multi_size = 0;
  if (RegistryView::required_recovery_mapping_size(2U, multi_size) != MF_SHARED_SUCCESS) {
    return false;
  }
  void* multi = mapping(multi_size);
  if (multi == MAP_FAILED) {
    return false;
  }
  std::array<mf_virtual_device_identity_v1, 2> multi_identities{identity(), identity()};
  multi_identities[1].identity_record_id = UINT64_C(78);
  multi_identities[1].logical_device_id[0] = UINT8_C(0x18);
  std::array<FenceSnapshot, 2> multi_fences{initial_fence(), initial_fence()};
  multi_fences[1].identity_record_id = UINT64_C(78);
  RegistryView multi_view;
  ok = ok &&
       RegistryView::initialize(multi, multi_size, {UINT64_C(4), UINT64_C(1)}, UINT64_C(1),
                                multi_identities, multi_fences, multi_view) == MF_SHARED_SUCCESS &&
       multi_view.device_count() == 2U;
  std::array<mf_virtual_device_telemetry_v1, 2> multi_rows{};
  multi_rows[0].identity_record_id = UINT64_C(77);
  multi_rows[0].observed_lifecycle_sequence = UINT64_C(1);
  multi_rows[1].identity_record_id = UINT64_C(78);
  multi_rows[1].observed_lifecycle_sequence = UINT64_C(1);
  ok = ok && multi_view.publish_telemetry(multi_rows) == MF_SHARED_SUCCESS &&
       multi_view.close() == MF_SHARED_SUCCESS;
  unmap(multi, multi_size);

  std::uint64_t zero_size = 0;
  if (RegistryView::required_recovery_mapping_size(0U, zero_size) != MF_SHARED_SUCCESS) {
    return false;
  }
  void* zero = mapping(zero_size);
  if (zero == MAP_FAILED) {
    return false;
  }
  RegistryView zero_view;
  const std::span<const mf_virtual_device_identity_v1> no_identities;
  const std::span<const FenceSnapshot> no_fences;
  const std::span<const mf_virtual_device_telemetry_v1> no_rows;
  ok = ok &&
       RegistryView::initialize(zero, zero_size, {UINT64_C(3), UINT64_C(1)}, UINT64_C(1),
                                no_identities, no_fences, zero_view) == MF_SHARED_SUCCESS &&
       zero_view.device_count() == 0U &&
       zero_view.publish_telemetry(no_rows) == MF_SHARED_SUCCESS &&
       zero_view.close() == MF_SHARED_SUCCESS;
  auto* zero_header = static_cast<mf_shared_registry_header_v1*>(zero);
  auto* telemetry = reinterpret_cast<mf_telemetry_control_v1*>(
      static_cast<std::uint8_t*>(zero) + zero_header->telemetry_control_offset);
  ok = ok && (mf_atomic_load_u64_acquire(&telemetry->telemetry_latch_sequence) & 1U) == 0U &&
       mf_telemetry_state_v1(mf_atomic_load_u64_acquire(&telemetry->active_bank_state)) ==
           MF_TELEMETRY_STATE_TERMINAL;
  unmap(zero, zero_size);
  return ok;
}

[[nodiscard]] bool normal_protocol_tests() {
  std::uint64_t size = 0;
  if (RegistryView::required_recovery_mapping_size(1U, size) != MF_SHARED_SUCCESS) {
    return false;
  }
  void* address = mapping(size);
  if (address == MAP_FAILED) {
    return false;
  }
  RegistryView view;
  if (!initialize_one(address, size, 10U, view)) {
    unmap(address, size);
    return false;
  }
  AdmissionLeaseToken first{};
  AdmissionLeaseToken second{};
  bool ok = view.begin_admission(0U, 0U, 1U, 2U, first) == MF_SHARED_SUCCESS &&
            view.release_admission(first) == MF_SHARED_SUCCESS &&
            view.begin_admission(0U, 0U, 3U, 4U, second) == MF_SHARED_SUCCESS &&
            second.lease_tag != first.lease_tag &&
            view.commit_admission(first) == MF_SHARED_STALE_HANDLE &&
            view.commit_admission(second) == MF_SHARED_SUCCESS &&
            view.release_admission(second) == MF_SHARED_SUCCESS;

  LifecycleRangeToken left{};
  LifecycleRangeToken right{};
  ok = ok && view.reserve_lifecycle_range(2U, 0U, left) == MF_SHARED_SUCCESS &&
       view.reserve_lifecycle_range(3U, 0U, right) == MF_SHARED_SUCCESS &&
       view.retire_lifecycle_range(right, right.range_end) == MF_SHARED_WOULD_BLOCK &&
       view.retire_lifecycle_range(left, left.range_begin) == MF_SHARED_SUCCESS &&
       view.retire_lifecycle_range(right, right.range_end) == MF_SHARED_SUCCESS;

  mf_generation_handle_v1 handle{};
  FenceSnapshot observed{};
  std::uint32_t generation = 1U;
  const FenceSnapshot next{
      .identity_record_id = UINT64_C(77),
      .lifecycle_sequence = right.range_end + 1U,
      .epoch = UINT64_C(2),
      .effective_quota_bytes = UINT64_C(8192),
      .policy_bits = UINT64_C(5),
      .device_state = MF_DEVICE_STATE_ONLINE,
  };
  ok = ok && view.make_handle(0U, 1U, 1U, 1U, handle) == MF_SHARED_SUCCESS &&
       view.publish_fence(0U, generation, next, generation) == MF_SHARED_SUCCESS &&
       generation == 2U && view.validate_device(handle, observed) == MF_SHARED_SUCCESS &&
       observed.lifecycle_sequence == next.lifecycle_sequence;
  std::uint32_t stale_generation = 1U;
  const FenceSnapshot stale_next{
      .identity_record_id = UINT64_C(77),
      .lifecycle_sequence = next.lifecycle_sequence + 1U,
      .epoch = UINT64_C(3),
      .effective_quota_bytes = UINT64_C(8192),
      .policy_bits = UINT64_C(7),
      .device_state = MF_DEVICE_STATE_ONLINE,
  };
  ok = ok &&
       view.publish_fence(0U, stale_generation, stale_next, stale_generation) == MF_SHARED_RETRY;

  std::array<mf_virtual_device_telemetry_v1, 1> rows{};
  rows[0].identity_record_id = UINT64_C(77);
  rows[0].observed_lifecycle_sequence = UINT64_C(1);
  ok = ok && view.publish_telemetry(rows) == MF_SHARED_RETRY;
  rows[0].observed_lifecycle_sequence = next.lifecycle_sequence;
  rows[0].committed_work_items = UINT64_C(11);
  rows[0].completed_work_items = UINT64_C(11);
  ok = ok && view.publish_telemetry(rows) == MF_SHARED_SUCCESS &&
       view.mark_device_lost(UINT32_C(0), next.lifecycle_sequence + 1U, UINT64_C(4)) ==
           MF_SHARED_SUCCESS &&
       view.publish_telemetry(rows) == MF_SHARED_DEVICE_LOST && view.close() == MF_SHARED_SUCCESS &&
       view.validate_device(handle, observed) == MF_SHARED_TERMINAL_VIEW;
  unmap(address, size);
  return ok;
}

[[nodiscard]] bool close_and_expiry_tests() {
  std::uint64_t size = 0;
  if (RegistryView::required_recovery_mapping_size(1U, size) != MF_SHARED_SUCCESS) {
    return false;
  }
  void* address = mapping(size);
  RegistryView view;
  if (address == MAP_FAILED || !initialize_one(address, size, 20U, view)) {
    unmap(address, size);
    return false;
  }
  AdmissionLeaseToken token{};
  bool ok = view.begin_admission(0U, 0U, 1U, 1U, token) == MF_SHARED_SUCCESS &&
            view.close() == MF_SHARED_SUCCESS &&
            view.commit_admission(token) == MF_SHARED_STALE_HANDLE;
  unmap(address, size);

  address = mapping(size);
  if (address == MAP_FAILED || !initialize_one(address, size, 21U, view)) {
    unmap(address, size);
    return false;
  }
  ok = ok && view.begin_admission(0U, 1U, 1U, 1U, token) == MF_SHARED_SUCCESS &&
       view.recover() == MF_SHARED_SUCCESS;
  auto* header = static_cast<mf_shared_registry_header_v1*>(address);
  auto* admission = reinterpret_cast<mf_view_admission_control_v1*>(
      static_cast<std::uint8_t*>(address) + header->view_admission_offset);
  ok = ok && mf_view_admission_state_v1(mf_atomic_load_u64_seq_cst(&admission->state_generation)) ==
                 MF_VIEW_ADMISSION_QUARANTINED;
  unmap(address, size);
  return ok;
}

[[nodiscard]] bool fork_attempt_and_lease_tests() {
  std::uint64_t size = 0;
  if (RegistryView::required_recovery_mapping_size(1U, size) != MF_SHARED_SUCCESS) {
    return false;
  }
  void* address = mapping(size);
  RegistryView parent;
  if (address == MAP_FAILED || !initialize_one(address, size, 30U, parent)) {
    unmap(address, size);
    return false;
  }
  bool ok = child_succeeds([&] {
    RegistryView child;
    AdmissionLeaseToken token{};
    if (RegistryView::attach(address, size, child) != MF_SHARED_SUCCESS) {
      return false;
    }
    child.set_fault_point_for_testing(RecoveryFaultPoint::AttemptInitializing);
    return child.begin_admission(0U, 0U, 1U, 1U, token) == MF_SHARED_INTERRUPTED;
  });
  RegistryView helper;
  ok = ok && RegistryView::attach(address, size, helper) == MF_SHARED_SUCCESS &&
       helper.recover() == MF_SHARED_SUCCESS && helper.recover() == MF_SHARED_SUCCESS;
  AdmissionLeaseToken fresh{};
  ok = ok && helper.begin_admission(0U, 0U, 2U, 2U, fresh) == MF_SHARED_SUCCESS &&
       helper.release_admission(fresh) == MF_SHARED_SUCCESS;
  unmap(address, size);

  address = mapping(size);
  if (address == MAP_FAILED || !initialize_one(address, size, 31U, parent)) {
    unmap(address, size);
    return false;
  }
  ok = ok && child_succeeds([&] {
         RegistryView child;
         AdmissionLeaseToken token{};
         if (RegistryView::attach(address, size, child) != MF_SHARED_SUCCESS ||
             child.begin_admission(0U, 0U, 1U, 1U, token) != MF_SHARED_SUCCESS) {
           return false;
         }
         child.set_fault_point_for_testing(RecoveryFaultPoint::LeaseCommitting);
         return child.commit_admission(token) == MF_SHARED_INTERRUPTED;
       });
  ok = ok && RegistryView::attach(address, size, helper) == MF_SHARED_SUCCESS &&
       helper.recover() == MF_SHARED_SUCCESS && helper.recover() == MF_SHARED_SUCCESS;
  auto* ext = extension(address, 1U);
  auto* leases = reinterpret_cast<mf_admission_lease_record_v1*>(
      static_cast<std::uint8_t*>(address) + ext->admission_leases_offset);
  for (std::uint32_t index = 0; index < ext->admission_lease_capacity; ++index) {
    const std::uint32_t state =
        mf_tagged_record_state_v1(mf_atomic_load_u64_seq_cst(&leases[index].tagged_state));
    ok = ok && state != MF_ADMISSION_LEASE_COMMITTED && state != MF_ADMISSION_LEASE_PUBLISHED;
  }
  unmap(address, size);
  return ok;
}

[[nodiscard]] bool tagged_claim_publication_tests() {
  std::uint64_t size = 0;
  if (RegistryView::required_recovery_mapping_size(1U, size) != MF_SHARED_SUCCESS) {
    return false;
  }
  void* address = mapping(size);
  RegistryView parent;
  if (address == MAP_FAILED || !initialize_one(address, size, 35U, parent)) {
    unmap(address, size);
    return false;
  }

  bool ok = child_succeeds([&] {
    RegistryView child;
    AdmissionLeaseToken token{};
    return RegistryView::attach(address, size, child) == MF_SHARED_SUCCESS &&
           child.begin_admission(0U, 0U, 1U, 1U, token) == MF_SHARED_SUCCESS &&
           child.release_admission(token) == MF_SHARED_SUCCESS;
  });
  auto* ext = extension(address, 1U);
  if (ext == nullptr) {
    unmap(address, size);
    return false;
  }
  auto* attempts = reinterpret_cast<mf_admission_attempt_record_v1*>(
      static_cast<std::uint8_t*>(address) + ext->admission_attempts_offset);
  auto* leases = reinterpret_cast<mf_admission_lease_record_v1*>(
      static_cast<std::uint8_t*>(address) + ext->admission_leases_offset);
  const mf_owner_identity_v1 old_owner = attempts[0].owner;
  ok = ok && old_owner.pid != 0U && old_owner.start_time_ticks != 0U &&
       mf_tagged_record_state_v1(mf_atomic_load_u64_seq_cst(&attempts[0].tagged_phase)) ==
           MF_ADMISSION_ATTEMPT_EXITED &&
       mf_tagged_record_state_v1(mf_atomic_load_u64_seq_cst(&leases[0].tagged_state)) ==
           MF_ADMISSION_LEASE_REVOKED;

  RegistryView helper;
  ok = ok && RegistryView::attach(address, size, helper) == MF_SHARED_SUCCESS;
  pid_t pid = fork();
  if (pid == 0) {
    RegistryView child;
    AdmissionLeaseToken token{};
    if (RegistryView::attach(address, size, child) != MF_SHARED_SUCCESS) {
      _exit(1);
    }
    child.set_fault_point_for_testing(RecoveryFaultPoint::AttemptClaimed);
    _exit(child.begin_admission(0U, 0U, 2U, 2U, token) == MF_SHARED_RETRY ? 0 : 1);
  }
  if (pid < 0) {
    unmap(address, size);
    return false;
  }
  const bool attempt_stopped = wait_for_child_stop(pid);
  ok = ok && attempt_stopped;
  const std::uint64_t attempt_claimed = mf_atomic_load_u64_seq_cst(&attempts[0].tagged_phase);
  ok = ok && mf_tagged_record_state_v1(attempt_claimed) == MF_ADMISSION_ATTEMPT_ENTERING &&
       helper.recover_owner(old_owner, false) == MF_SHARED_SUCCESS &&
       mf_atomic_load_u64_seq_cst(&attempts[0].tagged_phase) == attempt_claimed;
  std::uint64_t attempt_expected = attempt_claimed;
  const std::uint64_t attempt_terminal = mf_admission_attempt_phase_pack_v1(
      mf_tagged_record_tag_v1(attempt_claimed), MF_ADMISSION_ATTEMPT_EXITED);
  ok = ok && mf_atomic_compare_exchange_u64_seq_cst(&attempts[0].tagged_phase, &attempt_expected,
                                                    attempt_terminal) != 0;
  ok = resume_child_and_succeed(pid) && ok;
  ok = ok && mf_atomic_load_u64_seq_cst(&attempts[0].tagged_phase) == attempt_terminal;

  pid = fork();
  if (pid == 0) {
    RegistryView child;
    AdmissionLeaseToken token{};
    if (RegistryView::attach(address, size, child) != MF_SHARED_SUCCESS) {
      _exit(1);
    }
    child.set_fault_point_for_testing(RecoveryFaultPoint::LeaseClaimed);
    _exit(child.begin_admission(0U, 0U, 3U, 3U, token) == MF_SHARED_RETRY ? 0 : 1);
  }
  if (pid < 0) {
    unmap(address, size);
    return false;
  }
  const bool lease_stopped = wait_for_child_stop(pid);
  ok = ok && lease_stopped;
  const std::uint64_t lease_claimed = mf_atomic_load_u64_seq_cst(&leases[0].tagged_state);
  ok = ok && mf_tagged_record_state_v1(lease_claimed) == MF_ADMISSION_LEASE_INITIALIZING &&
       helper.recover_owner(old_owner, false) == MF_SHARED_SUCCESS &&
       mf_atomic_load_u64_seq_cst(&leases[0].tagged_state) == lease_claimed;
  std::uint64_t lease_expected = lease_claimed;
  const std::uint64_t lease_terminal = mf_admission_lease_state_pack_v1(
      mf_tagged_record_tag_v1(lease_claimed), MF_ADMISSION_LEASE_TOMBSTONED,
      mf_tagged_record_auxiliary_v1(lease_claimed));
  ok = ok && mf_atomic_compare_exchange_u64_seq_cst(&leases[0].tagged_state, &lease_expected,
                                                    lease_terminal) != 0;
  ok = resume_child_and_succeed(pid) && ok;
  ok = ok && mf_atomic_load_u64_seq_cst(&leases[0].tagged_state) == lease_terminal;

  unmap(address, size);
  return ok;
}

[[nodiscard]] bool fence_recovery_cut_test(std::uint64_t serial, RecoveryFaultPoint fault_point,
                                           std::uint64_t expected_latch_before_recovery) {
  std::uint64_t size = 0;
  if (RegistryView::required_recovery_mapping_size(1U, size) != MF_SHARED_SUCCESS) {
    return false;
  }
  void* address = mapping(size);
  RegistryView parent;
  if (address == MAP_FAILED || !initialize_one(address, size, serial, parent)) {
    unmap(address, size);
    return false;
  }
  mf_generation_handle_v1 handle{};
  bool ok = parent.make_handle(0U, 1U, 1U, 1U, handle) == MF_SHARED_SUCCESS && child_succeeds([&] {
              RegistryView child;
              std::uint32_t generation = 1U;
              const FenceSnapshot next{
                  .identity_record_id = UINT64_C(77),
                  .lifecycle_sequence = UINT64_C(2),
                  .epoch = UINT64_C(2),
                  .effective_quota_bytes = UINT64_C(8192),
                  .policy_bits = UINT64_C(5),
                  .device_state = MF_DEVICE_STATE_ONLINE,
              };
              if (RegistryView::attach(address, size, child) != MF_SHARED_SUCCESS) {
                return false;
              }
              child.set_fault_point_for_testing(fault_point);
              return child.publish_fence(0U, generation, next, generation) == MF_SHARED_INTERRUPTED;
            });
  auto* header = static_cast<mf_shared_registry_header_v1*>(address);
  auto* fence = reinterpret_cast<mf_virtual_device_lifecycle_fence_v1*>(
      static_cast<std::uint8_t*>(address) + header->lifecycle_fences_offset);
  const std::uint64_t latch_before = mf_atomic_load_u64_seq_cst(&fence->fence_latch_sequence);
  RegistryView helper;
  FenceSnapshot observed{};
  ok = ok && latch_before == expected_latch_before_recovery &&
       RegistryView::attach(address, size, helper) == MF_SHARED_SUCCESS &&
       helper.recover() == MF_SHARED_SUCCESS && helper.recover() == MF_SHARED_SUCCESS &&
       mf_atomic_load_u64_seq_cst(&fence->fence_latch_sequence) == UINT64_C(2) &&
       helper.validate_device(handle, observed) == MF_SHARED_SUCCESS &&
       observed.lifecycle_sequence == UINT64_C(2) && observed.epoch == UINT64_C(2);
  if (fault_point == RecoveryFaultPoint::FenceCommitted) {
    ok = ok && mf_atomic_load_u64_seq_cst(&fence->fence_latch_sequence) == latch_before;
  }
  unmap(address, size);
  return ok;
}

[[nodiscard]] bool fork_publication_tests() {
  std::uint64_t size = 0;
  if (RegistryView::required_recovery_mapping_size(1U, size) != MF_SHARED_SUCCESS) {
    return false;
  }
  void* address = mapping(size);
  RegistryView parent;
  if (address == MAP_FAILED || !initialize_one(address, size, 40U, parent)) {
    unmap(address, size);
    return false;
  }
  mf_generation_handle_v1 handle{};
  bool ok = parent.make_handle(0U, 1U, 1U, 1U, handle) == MF_SHARED_SUCCESS && child_succeeds([&] {
              RegistryView child;
              std::uint32_t generation = 1U;
              const FenceSnapshot next{
                  .identity_record_id = UINT64_C(77),
                  .lifecycle_sequence = UINT64_C(2),
                  .epoch = UINT64_C(2),
                  .effective_quota_bytes = UINT64_C(8192),
                  .policy_bits = UINT64_C(5),
                  .device_state = MF_DEVICE_STATE_ONLINE,
              };
              if (RegistryView::attach(address, size, child) != MF_SHARED_SUCCESS) {
                return false;
              }
              child.set_fault_point_for_testing(RecoveryFaultPoint::FenceActive);
              return child.publish_fence(0U, generation, next, generation) == MF_SHARED_INTERRUPTED;
            });
  RegistryView helper;
  FenceSnapshot observed{};
  ok = ok && RegistryView::attach(address, size, helper) == MF_SHARED_SUCCESS &&
       helper.recover() == MF_SHARED_SUCCESS && helper.recover() == MF_SHARED_SUCCESS &&
       helper.validate_device(handle, observed) == MF_SHARED_SUCCESS &&
       observed.lifecycle_sequence == 2U;
  void* replay_address = mapping(size);
  RegistryView replay_view;
  if (replay_address == MAP_FAILED || !initialize_one(replay_address, size, 42U, replay_view)) {
    unmap(replay_address, size);
    unmap(address, size);
    return false;
  }
  ok = ok && replay_view.validate_device(handle, observed) == MF_SHARED_STALE_HANDLE;
  unmap(replay_address, size);
  unmap(address, size);

  ok = ok && fence_recovery_cut_test(43U, RecoveryFaultPoint::FenceOdd, UINT64_C(1)) &&
       fence_recovery_cut_test(44U, RecoveryFaultPoint::FenceCommitted, UINT64_C(2));

  address = mapping(size);
  if (address == MAP_FAILED || !initialize_one(address, size, 41U, parent)) {
    unmap(address, size);
    return false;
  }
  ok = ok && child_succeeds([&] {
         RegistryView child;
         std::array<mf_virtual_device_telemetry_v1, 1> rows{};
         rows[0].identity_record_id = UINT64_C(77);
         rows[0].observed_lifecycle_sequence = UINT64_C(1);
         if (RegistryView::attach(address, size, child) != MF_SHARED_SUCCESS) {
           return false;
         }
         child.set_fault_point_for_testing(RecoveryFaultPoint::TelemetryActiveOdd);
         return child.publish_telemetry(rows) == MF_SHARED_INTERRUPTED;
       });
  ok = ok && RegistryView::attach(address, size, helper) == MF_SHARED_SUCCESS &&
       helper.recover() == MF_SHARED_SUCCESS && helper.recover() == MF_SHARED_SUCCESS;
  auto* header = static_cast<mf_shared_registry_header_v1*>(address);
  auto* telemetry = reinterpret_cast<mf_telemetry_control_v1*>(static_cast<std::uint8_t*>(address) +
                                                               header->telemetry_control_offset);
  ok = ok && (mf_atomic_load_u64_seq_cst(&telemetry->telemetry_latch_sequence) & 1U) == 0U;
  std::array<mf_virtual_device_telemetry_v1, 1> rows{};
  rows[0].identity_record_id = UINT64_C(77);
  rows[0].observed_lifecycle_sequence = UINT64_C(1);
  rows[0].committed_work_items = UINT64_C(9);
  ok = ok && helper.publish_telemetry(rows) == MF_SHARED_SUCCESS;
  unmap(address, size);
  return ok;
}

[[nodiscard]] bool telemetry_marker_loss_recovery_test() {
  std::uint64_t size = 0;
  if (RegistryView::required_recovery_mapping_size(1U, size) != MF_SHARED_SUCCESS) {
    return false;
  }
  void* address = mapping(size);
  RegistryView parent;
  if (address == MAP_FAILED || !initialize_one(address, size, 45U, parent)) {
    unmap(address, size);
    return false;
  }
  const bool published_marker = child_succeeds([&] {
    RegistryView child;
    std::array<mf_virtual_device_telemetry_v1, 1> rows{};
    rows[0].identity_record_id = UINT64_C(77);
    rows[0].observed_lifecycle_sequence = UINT64_C(1);
    if (RegistryView::attach(address, size, child) != MF_SHARED_SUCCESS) {
      return false;
    }
    child.set_fault_point_for_testing(RecoveryFaultPoint::TelemetryMarker);
    return child.publish_telemetry(rows) == MF_SHARED_INTERRUPTED;
  });
  bool ok = published_marker &&
            parent.mark_device_lost(UINT32_C(0), UINT64_C(2), UINT64_C(2)) == MF_SHARED_SUCCESS;
  RegistryView helper;
  ok = ok && RegistryView::attach(address, size, helper) == MF_SHARED_SUCCESS &&
       helper.recover() == MF_SHARED_SUCCESS && helper.recover() == MF_SHARED_SUCCESS;
  auto* header = static_cast<mf_shared_registry_header_v1*>(address);
  auto* telemetry = reinterpret_cast<mf_telemetry_control_v1*>(static_cast<std::uint8_t*>(address) +
                                                               header->telemetry_control_offset);
  auto* ext = extension(address, 1U);
  auto* publications = reinterpret_cast<mf_telemetry_publish_record_v1*>(
      static_cast<std::uint8_t*>(address) + ext->telemetry_publish_records_offset);
  ok = ok && mf_atomic_load_u64_seq_cst(&telemetry->telemetry_latch_sequence) == UINT64_C(0) &&
       mf_atomic_load_u64_relaxed(&telemetry->snapshot_sequence) == UINT64_C(0) &&
       mf_telemetry_state_v1(mf_atomic_load_u64_relaxed(&telemetry->active_bank_state)) ==
           MF_TELEMETRY_STATE_UNAVAILABLE &&
       mf_tagged_record_state_v1(mf_atomic_load_u64_seq_cst(&publications[0].tagged_state)) ==
           MF_TELEMETRY_PUBLISH_TERMINAL;
  std::array<mf_virtual_device_telemetry_v1, 1> rows{};
  rows[0].identity_record_id = UINT64_C(77);
  rows[0].observed_lifecycle_sequence = UINT64_C(1);
  ok = ok && helper.publish_telemetry(rows) == MF_SHARED_DEVICE_LOST &&
       helper.close() == MF_SHARED_SUCCESS;
  unmap(address, size);
  return ok;
}

[[nodiscard]] bool concurrent_exact_fence_recovery_test() {
  std::uint64_t size = 0;
  if (RegistryView::required_recovery_mapping_size(1U, size) != MF_SHARED_SUCCESS) {
    return false;
  }
  void* address = mapping(size);
  RegistryView parent;
  if (address == MAP_FAILED || !initialize_one(address, size, 50U, parent) ||
      !publish_interrupted_fence(address, size, 1U, 2U, RecoveryFaultPoint::FenceActive)) {
    unmap(address, size);
    return false;
  }

  auto* header = static_cast<mf_shared_registry_header_v1*>(address);
  auto* fence = reinterpret_cast<mf_virtual_device_lifecycle_fence_v1*>(
      static_cast<std::uint8_t*>(address) + header->lifecycle_fences_offset);
  const pid_t first_helper = fork();
  if (first_helper == 0) {
    RegistryView helper;
    if (RegistryView::attach(address, size, helper) != MF_SHARED_SUCCESS) {
      _exit(1);
    }
    helper.set_fault_point_for_testing(RecoveryFaultPoint::RecoveryFenceBeforeExactCas);
    const mf_shared_status_v1 status = helper.recover();
    _exit(status == MF_SHARED_SUCCESS || status == MF_SHARED_RETRY ||
                  status == MF_SHARED_DEVICE_LOST
              ? 0
              : 1);
  }
  if (first_helper < 0 || !wait_for_child_stop(first_helper)) {
    unmap(address, size);
    return false;
  }

  RegistryView second_helper;
  bool ok = mf_atomic_load_u64_seq_cst(&fence->fence_latch_sequence) == 0U &&
            RegistryView::attach(address, size, second_helper) == MF_SHARED_SUCCESS &&
            second_helper.recover() == MF_SHARED_SUCCESS &&
            mf_atomic_load_u64_seq_cst(&fence->fence_latch_sequence) == 2U;
  ok = resume_child_and_succeed(first_helper) && ok;
  auto* ext = extension(address, 1U);
  auto* updates = reinterpret_cast<mf_device_validation_update_record_v1*>(
      static_cast<std::uint8_t*>(address) + ext->device_updates_offset);
  ok = ok && mf_atomic_load_u64_seq_cst(&fence->fence_latch_sequence) == 2U &&
       mf_atomic_load_u64_seq_cst(&updates[0].publication_marker) == 1U &&
       mf_tagged_record_state_v1(mf_atomic_load_u64_seq_cst(&updates[0].tagged_state)) ==
           MF_DEVICE_UPDATE_TERMINAL;
  unmap(address, size);
  return ok;
}

[[nodiscard]] bool update_cleanup_cut_test(std::uint64_t serial, RecoveryFaultPoint fault_point) {
  std::uint64_t size = 0;
  if (RegistryView::required_recovery_mapping_size(1U, size) != MF_SHARED_SUCCESS) {
    return false;
  }
  void* address = mapping(size);
  RegistryView parent;
  if (address == MAP_FAILED || !initialize_one(address, size, serial, parent) ||
      !publish_interrupted_fence(address, size, 1U, 2U, fault_point)) {
    unmap(address, size);
    return false;
  }
  RegistryView helper;
  bool ok = RegistryView::attach(address, size, helper) == MF_SHARED_SUCCESS &&
            helper.recover() == MF_SHARED_SUCCESS && helper.recover() == MF_SHARED_SUCCESS;
  auto* ext = extension(address, 1U);
  auto* updates = reinterpret_cast<mf_device_validation_update_record_v1*>(
      static_cast<std::uint8_t*>(address) + ext->device_updates_offset);
  auto* publications = reinterpret_cast<mf_view_publish_record_v1*>(
      static_cast<std::uint8_t*>(address) + ext->view_publish_records_offset);
  auto* ranges = reinterpret_cast<mf_lifecycle_range_record_v1*>(
      static_cast<std::uint8_t*>(address) + ext->lifecycle_ranges_offset);
  auto* publisher = reinterpret_cast<mf_view_publisher_control_v1*>(
      static_cast<std::uint8_t*>(address) + ext->view_publisher_control_offset);
  const std::uint64_t update_word = mf_atomic_load_u64_seq_cst(&updates[0].tagged_state);
  const std::uint32_t publish_slot = updates[0].view_publish_slot;
  const std::uint32_t range_slot = publications[publish_slot].range_slot;
  ok =
      ok && mf_tagged_record_state_v1(update_word) == MF_DEVICE_UPDATE_TERMINAL &&
      mf_tagged_record_state_v1(mf_atomic_load_u64_seq_cst(
          &publications[publish_slot].tagged_state)) == MF_VIEW_PUBLISH_TERMINAL &&
      mf_tagged_record_state_v1(mf_atomic_load_u64_seq_cst(&ranges[range_slot].tagged_state)) ==
          MF_LIFECYCLE_RANGE_RETIRED &&
      mf_tagged_record_auxiliary_v1(mf_atomic_load_u64_seq_cst(&ranges[range_slot].tagged_state)) ==
          MF_LIFECYCLE_RANGE_DISPOSITION_COMPLETE &&
      mf_tagged_record_state_v1(mf_atomic_load_u64_seq_cst(&publisher->tagged_owner)) ==
          MF_PUBLISHER_IDLE;
  std::uint32_t generation = 2U;
  ok = ok && helper.publish_fence(0U, generation, fence_at(3U), generation) == MF_SHARED_SUCCESS &&
       generation == 3U;
  unmap(address, size);
  return ok;
}

[[nodiscard]] bool update_cleanup_cut_tests() {
  constexpr std::array<RecoveryFaultPoint, 5> fault_points{
      RecoveryFaultPoint::UpdateReopened,          RecoveryFaultPoint::UpdatePublicationPublished,
      RecoveryFaultPoint::UpdatePublisherReleased, RecoveryFaultPoint::UpdateRangeRetired,
      RecoveryFaultPoint::UpdateTerminal,
  };
  for (std::uint32_t index = 0; index < fault_points.size(); ++index) {
    if (!update_cleanup_cut_test(60U + index, fault_points[index])) {
      return false;
    }
  }
  return true;
}

[[nodiscard]] bool recovery_payload_hazard_test() {
  std::uint64_t size = 0;
  if (RegistryView::required_recovery_mapping_size(1U, size) != MF_SHARED_SUCCESS) {
    return false;
  }
  void* address = mapping(size);
  RegistryView parent;
  if (address == MAP_FAILED || !initialize_one(address, size, 70U, parent) ||
      !publish_interrupted_fence(address, size, 1U, 2U, RecoveryFaultPoint::FenceActive)) {
    unmap(address, size);
    return false;
  }
  auto* ext = extension(address, 1U);
  auto* updates = reinterpret_cast<mf_device_validation_update_record_v1*>(
      static_cast<std::uint8_t*>(address) + ext->device_updates_offset);
  const std::uint64_t old_word = mf_atomic_load_u64_seq_cst(&updates[0].tagged_state);
  const pid_t stale_helper = fork();
  if (stale_helper == 0) {
    RegistryView helper;
    if (RegistryView::attach(address, size, helper) != MF_SHARED_SUCCESS) {
      _exit(1);
    }
    helper.set_fault_point_for_testing(RecoveryFaultPoint::RecoveryPayloadHazardHeld);
    const mf_shared_status_v1 status = helper.recover();
    _exit(status == MF_SHARED_SUCCESS || status == MF_SHARED_RETRY ||
                  status == MF_SHARED_DEVICE_LOST
              ? 0
              : 1);
  }
  if (stale_helper < 0 || !wait_for_child_stop(stale_helper)) {
    unmap(address, size);
    return false;
  }

  RegistryView helper;
  bool ok = RegistryView::attach(address, size, helper) == MF_SHARED_SUCCESS &&
            helper.recover() == MF_SHARED_SUCCESS &&
            mf_tagged_record_state_v1(mf_atomic_load_u64_seq_cst(&updates[0].tagged_state)) ==
                MF_DEVICE_UPDATE_TERMINAL &&
            mf_atomic_load_u32_acquire(&updates[0].hazard_references) != 0U;
  const std::uint64_t old_sequence = updates[0].intended_lifecycle_sequence;
  std::uint32_t generation = 2U;
  ok = ok && helper.publish_fence(0U, generation, fence_at(3U), generation) == MF_SHARED_SUCCESS &&
       generation == 3U && updates[0].intended_lifecycle_sequence == old_sequence &&
       mf_atomic_load_u64_seq_cst(&updates[0].tagged_state) ==
           mf_device_update_state_pack_v1(mf_tagged_record_tag_v1(old_word),
                                          MF_DEVICE_UPDATE_TERMINAL,
                                          mf_tagged_record_auxiliary_v1(old_word));
  std::uint32_t new_slot = ext->device_update_capacity;
  for (std::uint32_t slot = 0; slot < ext->device_update_capacity; ++slot) {
    if (updates[slot].intended_lifecycle_sequence == 3U) {
      new_slot = slot;
      break;
    }
  }
  ok = ok && new_slot != ext->device_update_capacity && new_slot != 0U;
  ok = resume_child_and_succeed(stale_helper) && ok;
  auto* header = static_cast<mf_shared_registry_header_v1*>(address);
  auto* fence = reinterpret_cast<mf_virtual_device_lifecycle_fence_v1*>(
      static_cast<std::uint8_t*>(address) + header->lifecycle_fences_offset);
  ok = ok && mf_atomic_load_u64_seq_cst(&fence->fence_latch_sequence) == 4U &&
       mf_atomic_load_u64_relaxed(&fence->lifecycle_sequence) == 3U &&
       updates[0].intended_lifecycle_sequence == old_sequence &&
       mf_tagged_record_tag_v1(mf_atomic_load_u64_seq_cst(&updates[0].tagged_state)) ==
           mf_tagged_record_tag_v1(old_word);
  unmap(address, size);
  return ok;
}

[[nodiscard]] bool close_unpublished_claim_test(std::uint64_t serial,
                                                RecoveryFaultPoint fault_point) {
  std::uint64_t size = 0;
  if (RegistryView::required_recovery_mapping_size(1U, size) != MF_SHARED_SUCCESS) {
    return false;
  }
  void* address = mapping(size);
  RegistryView parent;
  if (address == MAP_FAILED || !initialize_one(address, size, serial, parent)) {
    unmap(address, size);
    return false;
  }
  auto* header = static_cast<mf_shared_registry_header_v1*>(address);
  auto* ext = extension(address, 1U);
  auto* attempts = reinterpret_cast<mf_admission_attempt_record_v1*>(
      static_cast<std::uint8_t*>(address) + ext->admission_attempts_offset);
  auto* leases = reinterpret_cast<mf_admission_lease_record_v1*>(
      static_cast<std::uint8_t*>(address) + ext->admission_leases_offset);
  auto* admission = reinterpret_cast<mf_view_admission_control_v1*>(
      static_cast<std::uint8_t*>(address) + header->view_admission_offset);
  const pid_t claimant = fork();
  if (claimant == 0) {
    RegistryView child;
    AdmissionLeaseToken token{};
    if (RegistryView::attach(address, size, child) != MF_SHARED_SUCCESS) {
      _exit(1);
    }
    child.set_fault_point_for_testing(fault_point);
    const mf_shared_status_v1 status = child.begin_admission(0U, 0U, 101U, 202U, token);
    _exit(status == MF_SHARED_TERMINAL_VIEW || status == MF_SHARED_RETRY ||
                  status == MF_SHARED_RESOURCE_EXHAUSTED
              ? 0
              : 1);
  }
  if (claimant < 0 || !wait_for_child_stop(claimant)) {
    unmap(address, size);
    return false;
  }

  const bool attempt_cut = fault_point == RecoveryFaultPoint::AttemptClaimed;
  const std::uint64_t claimed_word = attempt_cut
                                         ? mf_atomic_load_u64_seq_cst(&attempts[0].tagged_phase)
                                         : mf_atomic_load_u64_seq_cst(&leases[0].tagged_state);
  const std::uint64_t claimed_cookie = leases[0].operation_cookie;
  bool ok = parent.close() == MF_SHARED_SUCCESS &&
            mf_view_admission_state_v1(mf_atomic_load_u64_seq_cst(&admission->state_generation)) ==
                MF_VIEW_ADMISSION_QUARANTINED;
  AdmissionLeaseToken third{};
  ok = ok && parent.begin_admission(0U, 0U, 303U, 404U, third) == MF_SHARED_TERMINAL_VIEW &&
       (attempt_cut ? mf_atomic_load_u64_seq_cst(&attempts[0].tagged_phase)
                    : mf_atomic_load_u64_seq_cst(&leases[0].tagged_state)) == claimed_word &&
       leases[0].operation_cookie == claimed_cookie;
  ok = resume_child_and_succeed(claimant) && ok;
  const std::uint64_t final_word = attempt_cut
                                       ? mf_atomic_load_u64_seq_cst(&attempts[0].tagged_phase)
                                       : mf_atomic_load_u64_seq_cst(&leases[0].tagged_state);
  ok = ok && mf_tagged_record_tag_v1(final_word) == mf_tagged_record_tag_v1(claimed_word);
  if (attempt_cut) {
    ok = ok && mf_tagged_record_state_v1(final_word) == MF_ADMISSION_ATTEMPT_EXITED &&
         attempts[0].owner.pid == static_cast<std::uint32_t>(claimant);
  } else {
    ok = ok && mf_tagged_record_state_v1(final_word) == MF_ADMISSION_LEASE_REVOKED &&
         leases[0].owner.pid == static_cast<std::uint32_t>(claimant) &&
         leases[0].operation_cookie == 101U && leases[0].target_cookie == 202U;
  }
  unmap(address, size);
  return ok;
}

[[nodiscard]] bool close_unpublished_claim_tests() {
  return close_unpublished_claim_test(80U, RecoveryFaultPoint::AttemptClaimed) &&
         close_unpublished_claim_test(81U, RecoveryFaultPoint::LeaseClaimed);
}

} // namespace

int main() {
  if (!layout_and_zero_tests()) {
    return 1;
  }
  if (!normal_protocol_tests()) {
    return 2;
  }
  if (!close_and_expiry_tests()) {
    return 3;
  }
  if (!fork_attempt_and_lease_tests()) {
    return 4;
  }
  if (!tagged_claim_publication_tests()) {
    return 5;
  }
  if (!fork_publication_tests()) {
    return 6;
  }
  if (!telemetry_marker_loss_recovery_test()) {
    return 7;
  }
  if (!concurrent_exact_fence_recovery_test()) {
    return 8;
  }
  if (!update_cleanup_cut_tests()) {
    return 9;
  }
  if (!recovery_payload_hazard_test()) {
    return 10;
  }
  if (!close_unpublished_claim_tests()) {
    return 11;
  }
  return 0;
}
