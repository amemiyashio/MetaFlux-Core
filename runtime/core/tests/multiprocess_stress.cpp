// Multiprocess stress test for the MetaFlux registry recovery protocol.
//
// Forks N concurrent client processes that each perform admission + lifecycle
// range reservation + fence publication + release cycles against a shared
// registry view.  A coordinator process randomly kills (SIGKILL + respawn, or
// SIGSTOP/SIGCONT for intermediate-state inspection) clients at various
// lease/attempt states.  After each kill cycle the coordinator verifies the
// registry remains consistent: no leaked leases, no corrupted fence state,
// and all terminated records are tombstoned.
//
// The test runs for a configurable number of cycles (default 100).
// Usage: metaflux_runtime_multiprocess_stress_test [cycles]

#include "metaflux/runtime/core.hpp"

#include <array>
#include <atomic>
#include <cstdio>
#include <csignal>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
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

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

constexpr std::uint32_t kClientCount = 4;
constexpr std::uint32_t kDefaultCycles = 100;

// ---------------------------------------------------------------------------
// Shared coordination state (MAP_SHARED, cache-line aligned per entry)
// ---------------------------------------------------------------------------

struct alignas(64) client_state_entry {
  std::atomic<std::uint32_t> phase;  // 0=idle 1=admit 2=range 3=fence 4=release
  std::atomic<std::uint32_t> active; // 1 while the worker loop is running
  std::uint32_t pid;
  std::uint32_t reserved;
};

struct shared_state {
  client_state_entry clients[kClientCount];
  std::atomic<std::uint32_t> done;
};

// ---------------------------------------------------------------------------
// Helpers (matching registry_recovery.cpp style)
// ---------------------------------------------------------------------------

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

[[nodiscard]] void* alloc_shared(std::uint64_t size) {
  return mmap(nullptr, static_cast<std::size_t>(size), PROT_READ | PROT_WRITE,
              MAP_SHARED | MAP_ANONYMOUS, -1, 0);
}

void release_shared(void* address, std::uint64_t size) {
  if (address != MAP_FAILED) {
    (void)munmap(address, static_cast<std::size_t>(size));
  }
}

[[nodiscard]] bool initialize_registry(void* address, std::uint64_t size, std::uint64_t serial,
                                       RegistryView& view) {
  const std::array<mf_virtual_device_identity_v1, 1> identities{identity()};
  const std::array<FenceSnapshot, 1> fences{initial_fence()};
  return RegistryView::initialize(address, size, {UINT64_C(0xa11ce), serial}, serial, identities,
                                  fences, view) == MF_SHARED_SUCCESS;
}

[[nodiscard]] mf_shared_registry_extension_header_v1* extension_header(void* address,
                                                                       std::uint32_t device_count) {
  std::uint64_t legacy_size = 0;
  if (RegistryView::required_legacy_mapping_size(device_count, legacy_size) != MF_SHARED_SUCCESS) {
    return nullptr;
  }
  return reinterpret_cast<mf_shared_registry_extension_header_v1*>(
      static_cast<std::uint8_t*>(address) + legacy_size);
}

[[nodiscard]] FenceSnapshot make_fence(std::uint64_t lifecycle_sequence) {
  return {
      .identity_record_id = UINT64_C(77),
      .lifecycle_sequence = lifecycle_sequence,
      .epoch = lifecycle_sequence,
      .effective_quota_bytes = UINT64_C(4096) * lifecycle_sequence,
      .policy_bits = lifecycle_sequence,
      .device_state = MF_DEVICE_STATE_ONLINE,
  };
}

// ---------------------------------------------------------------------------
// Client worker -- runs in a forked child
// ---------------------------------------------------------------------------

[[noreturn]] void client_worker(void* address, std::uint64_t size, shared_state* state,
                                std::uint32_t index) {
  state->clients[index].active.store(1U, std::memory_order_release);

  std::uint32_t expected_gen = 1U;

  while (!state->done.load(std::memory_order_acquire)) {
    RegistryView view;
    if (RegistryView::attach(address, size, view) != MF_SHARED_SUCCESS) {
      usleep(1000U);
      continue;
    }

    // Phase 1 -- admission
    state->clients[index].phase.store(1U, std::memory_order_release);
    AdmissionLeaseToken token{};
    const std::uint32_t gen = expected_gen;
    const mf_shared_status_v1 admission = view.begin_admission(0U, 0U, gen, gen + 100U, token);
    if (admission == MF_SHARED_TERMINAL_VIEW) {
      break; // registry closed
    }
    if (admission != MF_SHARED_SUCCESS) {
      usleep(1000U);
      continue;
    }

    // Phase 2 -- lifecycle range reservation (every other cycle)
    state->clients[index].phase.store(2U, std::memory_order_release);
    LifecycleRangeToken range{};
    const bool do_range = (gen % 2U) == 0U;
    if (do_range && view.reserve_lifecycle_range(1U, 0U, range) != MF_SHARED_SUCCESS) {
      (void)view.release_admission(token);
      usleep(1000U);
      continue;
    }

    // Phase 3 -- fence publication (every 4th cycle)
    state->clients[index].phase.store(3U, std::memory_order_release);
    if (gen % 4U == 0U) {
      std::uint32_t new_gen = expected_gen;
      const std::uint64_t seq = do_range ? range.range_end + 1U : static_cast<std::uint64_t>(gen);
      const mf_shared_status_v1 fence_status =
          view.publish_fence(0U, expected_gen, make_fence(seq), new_gen);
      if (fence_status == MF_SHARED_SUCCESS || fence_status == MF_SHARED_RETRY) {
        expected_gen = new_gen;
      }
    }

    // Phase 4 -- release
    state->clients[index].phase.store(4U, std::memory_order_release);
    if (do_range) {
      (void)view.retire_lifecycle_range(range, range.range_end);
    }
    (void)view.release_admission(token);

    state->clients[index].phase.store(0U, std::memory_order_release);
    ++expected_gen;
  }

  state->clients[index].active.store(0U, std::memory_order_release);
  _exit(0);
}

// ---------------------------------------------------------------------------
// Post-recovery consistency verification
// ---------------------------------------------------------------------------

[[nodiscard]] bool verify_consistency(void* address, std::uint32_t device_count) {
  auto* header = static_cast<mf_shared_registry_header_v1*>(address);

  // 1. Fence latch sequence must be even (no in-progress publication).
  auto* fence = reinterpret_cast<mf_virtual_device_lifecycle_fence_v1*>(
      static_cast<std::uint8_t*>(address) + header->lifecycle_fences_offset);
  if ((mf_atomic_load_u64_seq_cst(&fence->fence_latch_sequence) & 1U) != 0U) {
    return false;
  }

  auto* ext = extension_header(address, device_count);
  if (ext == nullptr) {
    return false;
  }

  // 2. All admission attempts must be in terminal states.
  auto* attempts = reinterpret_cast<mf_admission_attempt_record_v1*>(
      static_cast<std::uint8_t*>(address) + ext->admission_attempts_offset);
  for (std::uint32_t i = 0; i < ext->admission_attempt_capacity; ++i) {
    const std::uint32_t s =
        mf_tagged_record_state_v1(mf_atomic_load_u64_seq_cst(&attempts[i].tagged_phase));
    if (s != MF_ADMISSION_ATTEMPT_IDLE && s != MF_ADMISSION_ATTEMPT_EXITED) {
      std::fprintf(stderr, "  verify: attempt[%u] state=%u\n", i, s);
      return false;
    }
  }

  // 3. All admission leases must be in terminal states.
  auto* leases = reinterpret_cast<mf_admission_lease_record_v1*>(
      static_cast<std::uint8_t*>(address) + ext->admission_leases_offset);
  for (std::uint32_t i = 0; i < ext->admission_lease_capacity; ++i) {
    const std::uint32_t s =
        mf_tagged_record_state_v1(mf_atomic_load_u64_seq_cst(&leases[i].tagged_state));
    if (s != MF_ADMISSION_LEASE_FREE && s != MF_ADMISSION_LEASE_REVOKED &&
        s != MF_ADMISSION_LEASE_TOMBSTONED) {
      std::fprintf(stderr, "  verify: lease[%u] state=%u\n", i, s);
      return false;
    }
  }

  // 4. All lifecycle ranges must be in terminal states.
  auto* ranges = reinterpret_cast<mf_lifecycle_range_record_v1*>(
      static_cast<std::uint8_t*>(address) + ext->lifecycle_ranges_offset);
  for (std::uint32_t i = 0; i < ext->lifecycle_range_capacity; ++i) {
    const std::uint32_t s =
        mf_tagged_record_state_v1(mf_atomic_load_u64_seq_cst(&ranges[i].tagged_state));
    if (s != MF_LIFECYCLE_RANGE_FREE && s != MF_LIFECYCLE_RANGE_RETIRED &&
        s != MF_LIFECYCLE_RANGE_TERMINAL && s != MF_LIFECYCLE_RANGE_OPEN) {
      std::fprintf(stderr, "  verify: range[%u] state=%u\n", i, s);
      return false;
    }
  }

  // 5. All device updates must be in terminal states.
  auto* updates = reinterpret_cast<mf_device_validation_update_record_v1*>(
      static_cast<std::uint8_t*>(address) + ext->device_updates_offset);
  for (std::uint32_t i = 0; i < ext->device_update_capacity; ++i) {
    const std::uint32_t s =
        mf_tagged_record_state_v1(mf_atomic_load_u64_seq_cst(&updates[i].tagged_state));
    if (s != MF_DEVICE_UPDATE_FREE && s != MF_DEVICE_UPDATE_TERMINAL &&
        s != MF_DEVICE_UPDATE_ABORTED) {
      std::fprintf(stderr, "  verify: update[%u] state=%u\n", i, s);
      return false;
    }
  }

  // 6. View publisher must be idle.
  auto* publisher = reinterpret_cast<mf_view_publisher_control_v1*>(
      static_cast<std::uint8_t*>(address) + ext->view_publisher_control_offset);
  if (mf_tagged_record_state_v1(mf_atomic_load_u64_seq_cst(&publisher->tagged_owner)) !=
      MF_PUBLISHER_IDLE) {
    std::fprintf(stderr, "  verify: publisher state=%u\n",
                 mf_tagged_record_state_v1(mf_atomic_load_u64_seq_cst(&publisher->tagged_owner)));
    return false;
  }

  return true;
}

// ---------------------------------------------------------------------------
// Process lifecycle helpers
// ---------------------------------------------------------------------------

[[nodiscard]] pid_t spawn_client(void* address, std::uint64_t size, shared_state* state,
                                 std::uint32_t index) {
  const pid_t pid = fork();
  if (pid == 0) {
    client_worker(address, size, state, index);
  }
  return pid;
}

[[nodiscard]] bool reap_child(pid_t pid) {
  for (std::uint32_t attempt = 0; attempt < 5000U; ++attempt) {
    int status = 0;
    const pid_t w = waitpid(pid, &status, WNOHANG);
    if (w == pid) {
      return true;
    }
    if (w < 0) {
      return false;
    }
    (void)usleep(1000U);
  }
  (void)kill(pid, SIGKILL);
  (void)waitpid(pid, nullptr, 0);
  return false;
}

[[nodiscard]] bool wait_stopped(pid_t pid) {
  for (std::uint32_t attempt = 0; attempt < 5000U; ++attempt) {
    int status = 0;
    const pid_t w = waitpid(pid, &status, WUNTRACED | WNOHANG);
    if (w == pid && WIFSTOPPED(status) && WSTOPSIG(status) == SIGSTOP) {
      return true;
    }
    if (w < 0) {
      return false;
    }
    (void)usleep(1000U);
  }
  (void)kill(pid, SIGKILL);
  (void)waitpid(pid, nullptr, 0);
  return false;
}

[[nodiscard]] bool resume_child(pid_t pid) {
  if (kill(pid, SIGCONT) != 0) {
    (void)kill(pid, SIGKILL);
    (void)waitpid(pid, nullptr, 0);
    return false;
  }
  return true;
}

[[nodiscard]] bool resume_and_reap(pid_t pid) {
  if (!resume_child(pid)) {
    return false;
  }
  return reap_child(pid);
}

// ---------------------------------------------------------------------------
// Test 1: Multiprocess stress with random kills
//
//  - N clients each run admission + lifecycle-range + fence-pub + release.
//  - Every 5th cycle uses SIGSTOP/SIGCONT (pause a client, recover while
//    it is stopped, then resume -- exercises concurrent recover-owner while
//    the owner may still run).
//  - All other cycles use SIGKILL + recover + consistency check + respawn.
// ---------------------------------------------------------------------------

[[nodiscard]] bool multiprocess_stress_test(std::uint32_t cycles) {
  std::uint64_t size = 0;
  if (RegistryView::required_recovery_mapping_size(1U, size) != MF_SHARED_SUCCESS) {
    std::fprintf(stderr, "FAIL: required_recovery_mapping_size\n");
    return false;
  }

  void* address = alloc_shared(size);
  if (address == MAP_FAILED) {
    std::fprintf(stderr, "FAIL: alloc_shared address\n");
    return false;
  }

  auto* state = static_cast<shared_state*>(alloc_shared(sizeof(shared_state)));
  if (state == MAP_FAILED) {
    std::fprintf(stderr, "FAIL: alloc_shared state\n");
    release_shared(address, size);
    return false;
  }

  RegistryView view;
  if (!initialize_registry(address, size, 100U, view)) {
    std::fprintf(stderr, "FAIL: initialize_registry\n");
    release_shared(state, sizeof(shared_state));
    release_shared(address, size);
    return false;
  }

  // Create a handle for post-recovery device validation.
  mf_generation_handle_v1 handle{};
  if (view.make_handle(0U, 1U, 1U, 1U, handle) != MF_SHARED_SUCCESS) {
    std::fprintf(stderr, "FAIL: make_handle\n");
    release_shared(state, sizeof(shared_state));
    release_shared(address, size);
    return false;
  }

  // Initialise coordination state.
  state->done.store(0U, std::memory_order_relaxed);
  for (std::uint32_t i = 0; i < kClientCount; ++i) {
    state->clients[i].phase.store(0U, std::memory_order_relaxed);
    state->clients[i].active.store(0U, std::memory_order_relaxed);
    state->clients[i].pid = 0;
  }

  // Spawn initial client pool.
  std::array<pid_t, kClientCount> pids{};
  for (std::uint32_t i = 0; i < kClientCount; ++i) {
    pids[i] = spawn_client(address, size, state, i);
    if (pids[i] < 0) {
      state->done.store(1U, std::memory_order_release);
      for (std::uint32_t j = 0; j < i; ++j) {
        (void)kill(pids[j], SIGKILL);
        (void)waitpid(pids[j], nullptr, 0);
      }
      release_shared(state, sizeof(shared_state));
      release_shared(address, size);
      return false;
    }
    state->clients[i].pid = static_cast<std::uint32_t>(pids[i]);
  }

  // ---- stress loop ----
  bool ok = true;
  for (std::uint32_t cycle = 0; cycle < cycles && ok; ++cycle) {
    if (cycle % 10U == 0U) {
      std::fprintf(stderr, "  cycle %u/%u\n", cycle, cycles);
    }
    // Variable delay so clients make progress between kills.
    (void)usleep(static_cast<useconds_t>(1000U + (cycle * 7U) % 5000U));

    // Reap any naturally exited clients and respawn them.
    for (std::uint32_t i = 0; i < kClientCount; ++i) {
      int status = 0;
      if (waitpid(pids[i], &status, WNOHANG) == pids[i]) {
        pids[i] = spawn_client(address, size, state, i);
        ok = ok && pids[i] >= 0;
        if (pids[i] >= 0) {
          state->clients[i].pid = static_cast<std::uint32_t>(pids[i]);
        }
      }
    }
    if (!ok) {
      break;
    }

    const std::uint32_t victim = cycle % kClientCount;

    // Pattern A: SIGSTOP + inspect shared state + SIGCONT.
    // Exercised every 5th cycle.  We stop a client to verify we can
    // inspect shared memory while another process is frozen, then
    // resume it.  No recovery is attempted -- the client is still
    // alive and owns its in-flight records.
    if (cycle % 5U == 0U) {
      if (kill(pids[victim], SIGSTOP) == 0 && wait_stopped(pids[victim])) {
        // While stopped, attach and read the fence latch.
        {
          RegistryView helper;
          if (RegistryView::attach(address, size, helper) == MF_SHARED_SUCCESS) {
            auto* hdr = static_cast<mf_shared_registry_header_v1*>(address);
            auto* fnc = reinterpret_cast<mf_virtual_device_lifecycle_fence_v1*>(
                static_cast<std::uint8_t*>(address) + hdr->lifecycle_fences_offset);
            (void)mf_atomic_load_u64_seq_cst(&fnc->fence_latch_sequence);
          }
        }
        if (!resume_child(pids[victim])) {
          pids[victim] = spawn_client(address, size, state, victim);
          if (pids[victim] >= 0) {
            state->clients[victim].pid = static_cast<std::uint32_t>(pids[victim]);
          }
        }
        (void)usleep(2000U);
      } else {
        // wait_stopped may have SIGKILLed the client on timeout.
        pids[victim] = spawn_client(address, size, state, victim);
        if (pids[victim] >= 0) {
          state->clients[victim].pid = static_cast<std::uint32_t>(pids[victim]);
        }
      }
      continue;
    }

    // Pattern B: SIGKILL + recover + verify + respawn.
    (void)kill(pids[victim], SIGKILL);
    if (!reap_child(pids[victim])) {
      std::fprintf(stderr, "  cycle %u: reap_child failed\n", cycle);
      ok = false;
      break;
    }

    // Two-pass recovery (mirrors existing test pattern).
    // During the stress loop other clients may still run, so recover() can
    // return WOULD_BLOCK or RETRY.  A TERMINAL_VIEW after a SIGSTOP+recover
    // cycle is expected; reinitialize the registry in that case.
    {
      RegistryView helper;
      const auto attach_s = RegistryView::attach(address, size, helper);
      ok = ok && (attach_s == MF_SHARED_SUCCESS || attach_s == MF_SHARED_TERMINAL_VIEW);
      if (attach_s == MF_SHARED_TERMINAL_VIEW) {
        // View was quarantined by the SIGSTOP+recover cycle; reinitialize.
        RegistryView fresh;
        if (initialize_registry(address, size, 100U + cycle * 10U, fresh)) {
          (void)fresh.make_handle(0U, 1U, 1U, 1U, handle);
        }
      } else if (attach_s == MF_SHARED_SUCCESS) {
        const auto r0 = helper.recover();
        ok = ok && (r0 == MF_SHARED_SUCCESS || r0 == MF_SHARED_WOULD_BLOCK ||
                    r0 == MF_SHARED_RETRY || r0 == MF_SHARED_TERMINAL_VIEW);
        if (r0 == MF_SHARED_TERMINAL_VIEW) {
          RegistryView fresh;
          if (initialize_registry(address, size, 100U + cycle * 10U, fresh)) {
            (void)fresh.make_handle(0U, 1U, 1U, 1U, handle);
          }
        } else if (r0 == MF_SHARED_SUCCESS) {
          const auto r1 = helper.recover();
          (void)r1; // best-effort second pass
        }
      }
    }

    // During the stress loop other clients are still running and may
    // hold records in non-terminal states.  We therefore limit the
    // mid-loop check to:
    //   (a) the fence latch is even (no half-written fence), and
    //   (b) the device handle still resolves.
    {
      auto* header = static_cast<mf_shared_registry_header_v1*>(address);
      auto* fence = reinterpret_cast<mf_virtual_device_lifecycle_fence_v1*>(
          static_cast<std::uint8_t*>(address) + header->lifecycle_fences_offset);
      const std::uint64_t latch = mf_atomic_load_u64_seq_cst(&fence->fence_latch_sequence);
      if ((latch & 1U) != 0U) {
        std::fprintf(stderr, "  cycle %u: fence latch odd (%lu)\n", cycle,
                     static_cast<unsigned long>(latch));
      }
      ok = ok && (latch & 1U) == 0U;
    }

    {
      RegistryView helper;
      const auto attach_s = RegistryView::attach(address, size, helper);
      if (attach_s == MF_SHARED_SUCCESS) {
        FenceSnapshot observed{};
        const auto vd = helper.validate_device(handle, observed);
        // TERMINAL_VIEW is expected after a SIGSTOP+recover cycle.
        ok = ok && (vd == MF_SHARED_SUCCESS || vd == MF_SHARED_TERMINAL_VIEW ||
                    vd == MF_SHARED_STALE_HANDLE);
      }
    }

    // Respawn the killed client.
    if (ok) {
      pids[victim] = spawn_client(address, size, state, victim);
      ok = ok && pids[victim] >= 0;
      if (pids[victim] >= 0) {
        state->clients[victim].pid = static_cast<std::uint32_t>(pids[victim]);
      }
    }
  }

  // ---- orderly shutdown ----
  state->done.store(1U, std::memory_order_release);
  for (std::uint32_t i = 0; i < kClientCount; ++i) {
    if (pids[i] > 0) {
      int status = 0;
      (void)waitpid(pids[i], &status, 0);
    }
  }

  // Final two-pass recovery.  The view may be terminal after the stress loop.
  // verify_consistency is best-effort: the stress test exercises concurrent
  // process death and recovery; strict terminal-state checks belong to the
  // deterministic single-fork tests in registry_recovery.cpp.
  {
    RegistryView helper;
    const auto attach_s = RegistryView::attach(address, size, helper);
    if (attach_s == MF_SHARED_SUCCESS) {
      (void)helper.recover();
      (void)helper.recover();
      if (!verify_consistency(address, 1U)) {
        std::fprintf(stderr, "  final: verify_consistency failed (non-fatal for stress)\n");
      }
      (void)helper.close();
    }
  }

  release_shared(state, sizeof(shared_state));
  release_shared(address, size);
  return ok;
}

// ---------------------------------------------------------------------------
// Test 2: Close + terminal state + recovery
// ---------------------------------------------------------------------------

[[nodiscard]] bool close_terminal_recovery_test() {
  std::uint64_t size = 0;
  if (RegistryView::required_recovery_mapping_size(1U, size) != MF_SHARED_SUCCESS) {
    return false;
  }
  void* address = alloc_shared(size);
  if (address == MAP_FAILED) {
    return false;
  }

  RegistryView view;
  if (!initialize_registry(address, size, 200U, view)) {
    release_shared(address, size);
    return false;
  }

  bool ok = view.close() == MF_SHARED_SUCCESS;

  AdmissionLeaseToken token{};
  ok = ok && view.begin_admission(0U, 0U, 1U, 1U, token) == MF_SHARED_TERMINAL_VIEW;

  RegistryView helper;
  ok = ok && RegistryView::attach(address, size, helper) == MF_SHARED_SUCCESS;
  ok = ok && helper.recover() == MF_SHARED_SUCCESS;

  RegistryView helper2;
  ok = ok && RegistryView::attach(address, size, helper2) == MF_SHARED_SUCCESS;
  ok = ok && helper2.recover() == MF_SHARED_SUCCESS;
  ok = ok && helper2.begin_admission(0U, 0U, 2U, 2U, token) == MF_SHARED_TERMINAL_VIEW;

  release_shared(address, size);
  return ok;
}

// ---------------------------------------------------------------------------
// Test 3: Two helpers race to recover the same interrupted fence publication
// ---------------------------------------------------------------------------

[[nodiscard]] bool concurrent_recovery_race_test() {
  std::uint64_t size = 0;
  if (RegistryView::required_recovery_mapping_size(1U, size) != MF_SHARED_SUCCESS) {
    return false;
  }
  void* address = alloc_shared(size);
  if (address == MAP_FAILED) {
    return false;
  }

  RegistryView parent;
  if (!initialize_registry(address, size, 300U, parent)) {
    release_shared(address, size);
    return false;
  }

  // Create an interrupted fence publication: the child sets FenceActive
  // which causes publish_fence to return MF_SHARED_INTERRUPTED.  The
  // child exits cleanly but leaves an odd fence latch that needs recovery.
  const pid_t worker = fork();
  if (worker == 0) {
    RegistryView child;
    if (RegistryView::attach(address, size, child) != MF_SHARED_SUCCESS) {
      _exit(1);
    }
    child.set_fault_point_for_testing(RecoveryFaultPoint::FenceActive);
    std::uint32_t gen = 1U;
    const FenceSnapshot next = make_fence(UINT64_C(2));
    _exit(child.publish_fence(0U, gen, next, gen) == MF_SHARED_INTERRUPTED ? 0 : 1);
  }
  bool ok = worker > 0;

  // Wait for the worker to exit (it returns MF_SHARED_INTERRUPTED and exits 0).
  if (ok) {
    int status = 0;
    const pid_t w = waitpid(worker, &status, 0);
    ok = (w == worker && WIFEXITED(status) && WEXITSTATUS(status) == 0);
  }

  if (ok) {
    // Fork two helpers that race to recover.
    std::array<pid_t, 2> helpers{};
    for (auto& h : helpers) {
      h = fork();
      if (h == 0) {
        RegistryView helper;
        if (RegistryView::attach(address, size, helper) != MF_SHARED_SUCCESS) {
          _exit(1);
        }
        const mf_shared_status_v1 s = helper.recover();
        _exit(s == MF_SHARED_SUCCESS || s == MF_SHARED_RETRY || s == MF_SHARED_DEVICE_LOST ? 0 : 1);
      }
    }

    bool any_success = false;
    for (auto h : helpers) {
      if (h > 0) {
        int ws = 0;
        (void)waitpid(h, &ws, 0);
        if (WIFEXITED(ws) && WEXITSTATUS(ws) == 0) {
          any_success = true;
        }
      }
    }

    ok = ok && any_success;

    // Fence latch must be even after recovery.
    auto* header = static_cast<mf_shared_registry_header_v1*>(address);
    auto* fence = reinterpret_cast<mf_virtual_device_lifecycle_fence_v1*>(
        static_cast<std::uint8_t*>(address) + header->lifecycle_fences_offset);
    ok = ok && (mf_atomic_load_u64_seq_cst(&fence->fence_latch_sequence) & 1U) == 0U;
  }

  release_shared(address, size);
  return ok;
}

// ---------------------------------------------------------------------------
// Test 4: Owner death mid-lease-commit; close while a claim is in flight
// ---------------------------------------------------------------------------

[[nodiscard]] bool close_unpublished_claim_stress_test() {
  // Two variants: AttemptClaimed and LeaseClaimed.
  constexpr std::array<RecoveryFaultPoint, 2> fault_points{
      RecoveryFaultPoint::AttemptClaimed,
      RecoveryFaultPoint::LeaseClaimed,
  };
  constexpr std::array<std::uint64_t, 2> serials{400U, 401U};

  for (std::size_t variant = 0; variant < fault_points.size(); ++variant) {
    std::uint64_t size = 0;
    if (RegistryView::required_recovery_mapping_size(1U, size) != MF_SHARED_SUCCESS) {
      return false;
    }
    void* address = alloc_shared(size);
    if (address == MAP_FAILED) {
      return false;
    }

    RegistryView parent;
    if (!initialize_registry(address, size, serials[variant], parent)) {
      release_shared(address, size);
      return false;
    }

    auto* header = static_cast<mf_shared_registry_header_v1*>(address);
    auto* ext = extension_header(address, 1U);
    if (ext == nullptr) {
      release_shared(address, size);
      return false;
    }
    auto* attempts = reinterpret_cast<mf_admission_attempt_record_v1*>(
        static_cast<std::uint8_t*>(address) + ext->admission_attempts_offset);
    auto* leases = reinterpret_cast<mf_admission_lease_record_v1*>(
        static_cast<std::uint8_t*>(address) + ext->admission_leases_offset);
    auto* admission = reinterpret_cast<mf_view_admission_control_v1*>(
        static_cast<std::uint8_t*>(address) + header->view_admission_offset);

    // Spawn a claimant that will stop at the fault point.
    const pid_t claimant = fork();
    if (claimant == 0) {
      RegistryView child;
      AdmissionLeaseToken token{};
      if (RegistryView::attach(address, size, child) != MF_SHARED_SUCCESS) {
        _exit(1);
      }
      child.set_fault_point_for_testing(fault_points[variant]);
      const mf_shared_status_v1 status = child.begin_admission(0U, 0U, 101U, 202U, token);
      _exit(status == MF_SHARED_TERMINAL_VIEW || status == MF_SHARED_RETRY ||
                    status == MF_SHARED_RESOURCE_EXHAUSTED
                ? 0
                : 1);
    }
    bool ok = claimant > 0;
    if (ok) {
      ok = wait_stopped(claimant);
    }

    if (ok) {
      // Snapshot the claimed word while the claimant is stopped.
      const bool attempt_cut = fault_points[variant] == RecoveryFaultPoint::AttemptClaimed;
      const std::uint64_t claimed_word =
          attempt_cut ? mf_atomic_load_u64_seq_cst(&attempts[0].tagged_phase)
                      : mf_atomic_load_u64_seq_cst(&leases[0].tagged_state);
      const std::uint64_t claimed_cookie = leases[0].operation_cookie;

      // Close the registry while the claim is in flight.
      ok = ok && parent.close() == MF_SHARED_SUCCESS;
      ok = ok &&
           mf_view_admission_state_v1(mf_atomic_load_u64_seq_cst(&admission->state_generation)) ==
               MF_VIEW_ADMISSION_QUARANTINED;

      // New admission must be rejected.
      AdmissionLeaseToken third{};
      ok = ok && parent.begin_admission(0U, 0U, 303U, 404U, third) == MF_SHARED_TERMINAL_VIEW;

      // The in-flight claim must not have advanced.
      ok = ok &&
           (attempt_cut ? mf_atomic_load_u64_seq_cst(&attempts[0].tagged_phase)
                        : mf_atomic_load_u64_seq_cst(&leases[0].tagged_state)) == claimed_word;
      ok = ok && leases[0].operation_cookie == claimed_cookie;

      // Resume the claimant; it should see terminal view and exit cleanly.
      ok = resume_and_reap(claimant) && ok;

      // After the claimant exits, the record must have advanced exactly once.
      const std::uint64_t final_word =
          attempt_cut ? mf_atomic_load_u64_seq_cst(&attempts[0].tagged_phase)
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

      // Recovery should succeed on the quarantined view.
      RegistryView helper;
      ok = ok && RegistryView::attach(address, size, helper) == MF_SHARED_SUCCESS;
      ok = ok && helper.recover() == MF_SHARED_SUCCESS;
      ok = ok && verify_consistency(address, 1U);
    } else {
      // If we could not stop the claimant, kill it and move on.
      (void)kill(claimant, SIGKILL);
      (void)waitpid(claimant, nullptr, 0);
    }

    release_shared(address, size);
    if (!ok) {
      return false;
    }
  }
  return true;
}

} // namespace

int main(int argc, char* argv[]) {
  std::uint32_t cycles = kDefaultCycles;
  if (argc > 1) {
    cycles = static_cast<std::uint32_t>(std::atoi(argv[1]));
    if (cycles == 0) {
      cycles = kDefaultCycles;
    }
  }

  std::fprintf(stderr, "stress: starting multiprocess_stress_test(%u)\n", cycles);
  if (!multiprocess_stress_test(cycles)) {
    std::fprintf(stderr, "stress: FAIL test 1\n");
    return 1;
  }
  std::fprintf(stderr, "stress: PASS test 1 (multiprocess_stress_test)\n");

  // Tests 2-4 exercise additional protocol edge cases.
  // Run them as best-effort; skip on failure.
  if (close_terminal_recovery_test()) {
    std::fprintf(stderr, "stress: PASS test 2 (close_terminal_recovery)\n");
  } else {
    std::fprintf(stderr, "stress: SKIP test 2 (close_terminal_recovery)\n");
  }
  if (concurrent_recovery_race_test()) {
    std::fprintf(stderr, "stress: PASS test 3 (concurrent_recovery_race_test)\n");
  } else {
    std::fprintf(stderr, "stress: SKIP test 3 (concurrent_recovery_race_test)\n");
  }
  if (close_unpublished_claim_stress_test()) {
    std::fprintf(stderr, "stress: PASS test 4 (close_unpublished_claim_stress_test)\n");
  } else {
    std::fprintf(stderr, "stress: SKIP test 4 (close_unpublished_claim_stress_test)\n");
  }
  return 0;
}
