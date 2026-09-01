#include "metaflux/compiler/artifact_cache.hpp"
#include "metaflux/compiler/cache.hpp"
#include "metaflux/compiler/kernel_ir.hpp"

#include <algorithm>
#include <array>
#include <barrier>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <sys/stat.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#include <utility>
#include <vector>

namespace {

class TemporaryDirectory {
public:
  TemporaryDirectory() {
    std::string pattern = "/tmp/metaflux-artifact-cache-test-XXXXXX";
    const char* created = mkdtemp(pattern.data());
    if (created != nullptr) {
      path_ = created;
    }
  }

  ~TemporaryDirectory() {
    if (!path_.empty()) {
      std::error_code ignored;
      std::filesystem::remove_all(path_, ignored);
    }
  }

  [[nodiscard]] const std::filesystem::path& path() const noexcept { return path_; }
  [[nodiscard]] bool valid() const noexcept { return !path_.empty(); }

private:
  std::filesystem::path path_;
};

bool expect(bool condition, std::string_view message) {
  if (!condition) {
    std::cerr << "persistent artifact cache failure: " << message << '\n';
  }
  return condition;
}

std::string key_for(std::string_view text) {
  metaflux::compiler::CacheIdentity identity{
      .toolchain_fingerprint = "llvm-22.1.8-cache-test",
      .compiler_epoch = 1,
      .kernel_ir_schema = metaflux::compiler::kKernelIrSchemaVersion,
      .pass_pipeline = "cache-test",
      .target_triple = "x86_64-unknown-linux-gnu",
      .cpu_name = "x86-64",
      .canonical_features = {},
      .optimization_level = "O2",
      .fp_semantics = "rn",
      .backend_abi = 1,
      .helper_abi = 1,
      .pgo_id = "none",
  };
  return metaflux::compiler::make_cache_key(identity, text);
}

metaflux::compiler::PersistentCacheConfig config_for(const TemporaryDirectory& temporary,
                                                     std::uint64_t& clock) {
  return metaflux::compiler::PersistentCacheConfig{
      .mutable_root = temporary.path() / "mutable",
      .aot_root = temporary.path() / "aot",
      .compiler_epoch = 1,
      .limits =
          {
              .per_uid_bytes = 1024U,
              .global_bytes = 4096U,
              .maximum_entry_bytes = 1024U,
              .reserved_free_bytes = 0U,
              .reserved_free_percent = 0U,
          },
      .clock = [&clock] { return ++clock; },
      .inject_fault = {},
      .filesystem_space = {},
  };
}

metaflux::compiler::ArtifactDescriptor descriptor() {
  return {
      .kernel_ir_schema = metaflux::compiler::kKernelIrSchemaVersion,
      .helper_abi = 1,
      .payload = "cpu-v1;fp=0;params=buffer_u32,scalar_u32",
  };
}

std::span<const std::byte> bytes(const std::array<std::byte, 4>& artifact) { return artifact; }

bool publish(metaflux::compiler::PersistentArtifactCache& cache, std::uint32_t uid,
             std::string_view key, const std::array<std::byte, 4>& artifact) {
  const auto reservation = cache.reserve(uid, key, artifact.size());
  return reservation.ok() &&
         cache.publish(*reservation.reservation, bytes(artifact), descriptor()) ==
             metaflux::compiler::PersistentCacheError::None;
}

bool write_byte(int descriptor, char value) {
  while (write(descriptor, &value, 1U) < 0) {
    if (errno != EINTR) {
      return false;
    }
  }
  return true;
}

std::optional<char> read_byte(int descriptor) {
  char value = 0;
  ssize_t result = 0;
  do {
    result = read(descriptor, &value, 1U);
  } while (result < 0 && errno == EINTR);
  return result == 1 ? std::optional<char>{value} : std::nullopt;
}

bool test_isolation_corruption_and_epoch() {
  TemporaryDirectory temporary;
  std::uint64_t clock = 0;
  auto config = config_for(temporary, clock);
  metaflux::compiler::PersistentArtifactCache cache(config);
  const std::array artifact{std::byte{0x7f}, std::byte{'E'}, std::byte{'L'}, std::byte{'F'}};
  const auto key = key_for("isolation");
  const auto validator = [](std::span<const std::byte> value) {
    return value.size() == 4U && value[0] == std::byte{0x7f};
  };
  if (!expect(temporary.valid(), "temporary root must exist") ||
      !expect(cache.reconcile() == metaflux::compiler::PersistentCacheError::None,
              "startup reconciliation must succeed") ||
      !expect(publish(cache, 1000U, key, artifact), "user artifact must publish")) {
    return false;
  }

  auto owner = cache.lookup(1000U, key, validator);
  if (!expect(owner.hit(), "owner must hit its mutable artifact") ||
      !expect(owner.entry->tier == metaflux::compiler::PersistentCacheTier::MutableUser,
              "owner hit must report mutable tier") ||
      !expect(owner.entry->artifact_path.string().find("users/1000/epoch-1") != std::string::npos,
              "mutable path must include peer UID and epoch") ||
      !expect(!cache.lookup(1001U, key, validator).hit(),
              "another UID must not read mutable content")) {
    return false;
  }
  struct stat status{};
  const auto uid_root = temporary.path() / "mutable/users/1000";
  if (!expect(stat(uid_root.c_str(), &status) == 0 && (status.st_mode & 0777U) == 0700U,
              "UID cache directory must be mode 0700")) {
    return false;
  }

  owner.entry->pin.reset();
  std::ofstream corrupt(owner.entry->artifact_path, std::ios::binary | std::ios::trunc);
  corrupt.put('X');
  corrupt.close();
  const auto recovered = cache.lookup(1000U, key, validator);
  if (!expect(!recovered.hit() && recovered.corruption_recovered,
              "digest corruption must be removed and become a miss") ||
      !expect(!std::filesystem::exists(owner.entry->artifact_path.parent_path()),
              "corrupt mutable entry must be removed")) {
    return false;
  }

  auto epoch_two_config = config;
  epoch_two_config.compiler_epoch = 2;
  metaflux::compiler::PersistentArtifactCache epoch_two(epoch_two_config);
  return expect(!epoch_two.lookup(1000U, key, validator).hit(),
                "compiler epochs must use isolated namespaces");
}

bool test_fault_publication_and_reconciliation() {
  TemporaryDirectory temporary;
  std::uint64_t clock = 0;
  auto config = config_for(temporary, clock);
  bool inject = true;
  config.inject_fault = [&inject](metaflux::compiler::CacheFaultPoint point) {
    if (inject && point == metaflux::compiler::CacheFaultPoint::AfterArtifactFsync) {
      inject = false;
      return true;
    }
    return false;
  };
  metaflux::compiler::PersistentArtifactCache cache(config);
  const std::array artifact{std::byte{1}, std::byte{2}, std::byte{3}, std::byte{4}};
  const auto key = key_for("fault-before-rename");
  const auto reservation = cache.reserve(42U, key, artifact.size());
  if (!expect(reservation.ok(), "fault test reservation must succeed") ||
      !expect(cache.publish(*reservation.reservation, bytes(artifact), descriptor()) ==
                  metaflux::compiler::PersistentCacheError::Io,
              "fault before rename must fail publication") ||
      !expect(!cache.lookup(42U, key).hit(), "failed publication must not become visible") ||
      !expect(cache.reconcile() == metaflux::compiler::PersistentCacheError::None,
              "reconciliation after publication fault must succeed")) {
    return false;
  }
  for (const auto& entry : std::filesystem::recursive_directory_iterator(config.mutable_root)) {
    if (!expect(!entry.path().filename().string().starts_with(".tmp-"),
                "reconciliation must remove stale temporary directories")) {
      return false;
    }
  }

  bool after_rename = true;
  config.inject_fault = [&after_rename](metaflux::compiler::CacheFaultPoint point) {
    if (after_rename && point == metaflux::compiler::CacheFaultPoint::AfterRename) {
      after_rename = false;
      return true;
    }
    return false;
  };
  metaflux::compiler::PersistentArtifactCache renamed_cache(config);
  const auto renamed_key = key_for("fault-after-rename");
  const auto renamed_reservation = renamed_cache.reserve(42U, renamed_key, artifact.size());
  return expect(renamed_reservation.ok(), "after-rename reservation must succeed") &&
         expect(renamed_cache.publish(*renamed_reservation.reservation, bytes(artifact),
                                      descriptor()) == metaflux::compiler::PersistentCacheError::Io,
                "injected post-rename failure must be reported") &&
         expect(renamed_cache.lookup(42U, renamed_key).hit(),
                "post-rename artifact must be wholly visible and valid");
}

bool test_candidate_scan_io_is_not_treated_as_empty_usage() {
  TemporaryDirectory temporary;
  std::uint64_t clock = 0;
  auto config = config_for(temporary, clock);
  config.inject_fault = [](metaflux::compiler::CacheFaultPoint point) {
    return point == metaflux::compiler::CacheFaultPoint::BeforeCandidateScan;
  };
  metaflux::compiler::PersistentArtifactCache cache(config);
  return expect(cache.reconcile() == metaflux::compiler::PersistentCacheError::Io,
                "startup scan failure must propagate as cache I/O") &&
         expect(cache.reserve(42U, key_for("scan-io-reservation"), 4U).error ==
                    metaflux::compiler::PersistentCacheError::Io,
                "quota reservation must not treat a failed usage scan as an empty cache");
}

bool test_remaining_faults_and_stale_temporary_cleanup() {
  const std::array pre_rename_faults{metaflux::compiler::CacheFaultPoint::AfterMetadataFsync,
                                     metaflux::compiler::CacheFaultPoint::BeforeRename};
  for (const auto fault_point : pre_rename_faults) {
    TemporaryDirectory temporary;
    std::uint64_t clock = 0;
    auto config = config_for(temporary, clock);
    bool inject = true;
    config.inject_fault = [&inject, fault_point](metaflux::compiler::CacheFaultPoint point) {
      if (inject && point == fault_point) {
        inject = false;
        return true;
      }
      return false;
    };
    metaflux::compiler::PersistentArtifactCache cache(config);
    const std::array artifact{std::byte{1}, std::byte{2}, std::byte{3}, std::byte{4}};
    const auto key = key_for(fault_point == metaflux::compiler::CacheFaultPoint::AfterMetadataFsync
                                 ? "fault-after-metadata"
                                 : "fault-before-rename");
    const auto reservation = cache.reserve(42U, key, artifact.size());
    if (!expect(reservation.ok(), "pre-rename fault reservation must succeed") ||
        !expect(cache.publish(*reservation.reservation, bytes(artifact), descriptor()) ==
                    metaflux::compiler::PersistentCacheError::Io,
                "every pre-rename fault must fail publication") ||
        !expect(!cache.lookup(42U, key).hit(),
                "pre-rename fault must not expose a partial entry")) {
      return false;
    }
  }

  TemporaryDirectory temporary;
  std::uint64_t clock = 0;
  const auto config = config_for(temporary, clock);
  const auto stale = config.mutable_root / "users/42/epoch-1/ab/.tmp-crashed-publication";
  std::error_code error;
  std::filesystem::create_directories(stale, error);
  if (!expect(!error && std::filesystem::exists(stale),
              "stale temporary fixture must be created")) {
    return false;
  }
  metaflux::compiler::PersistentArtifactCache reconciled(config);
  if (!expect(reconciled.reconcile() == metaflux::compiler::PersistentCacheError::None,
              "crash reconciliation must succeed") ||
      !expect(!std::filesystem::exists(stale),
              "crash reconciliation must remove stale temporary directories")) {
    return false;
  }
  const std::array artifact{std::byte{1}, std::byte{2}, std::byte{3}, std::byte{4}};
  const auto key = key_for("stale-metadata-update");
  if (!expect(publish(reconciled, 42U, key, artifact), "metadata cleanup fixture must publish")) {
    return false;
  }
  auto entry = reconciled.lookup(42U, key);
  if (!expect(entry.hit(), "metadata cleanup fixture must be readable")) {
    return false;
  }
  const auto stale_metadata = entry.entry->artifact_path.parent_path() / ".metadata.tmp-crash";
  entry.entry->pin.reset();
  std::ofstream stale_output(stale_metadata);
  stale_output << "partial";
  stale_output.close();
  return expect(reconciled.reconcile() == metaflux::compiler::PersistentCacheError::None,
                "metadata crash reconciliation must succeed") &&
         expect(!std::filesystem::exists(stale_metadata),
                "reconciliation must remove stale metadata update files") &&
         expect(reconciled.lookup(42U, key).hit(),
                "metadata cleanup must preserve the committed artifact");
}

bool test_quota_eviction_and_pinning() {
  TemporaryDirectory temporary;
  std::uint64_t clock = 0;
  auto config = config_for(temporary, clock);
  config.limits.per_uid_bytes = 8U;
  config.limits.global_bytes = 12U;
  metaflux::compiler::PersistentArtifactCache cache(config);
  const std::array artifact{std::byte{1}, std::byte{2}, std::byte{3}, std::byte{4}};
  const auto first_key = key_for("evict-first");
  const auto second_key = key_for("evict-second");
  const auto third_key = key_for("evict-third");
  if (!expect(publish(cache, 7U, first_key, artifact), "first quota artifact must publish") ||
      !expect(publish(cache, 7U, second_key, artifact), "second quota artifact must publish")) {
    return false;
  }
  auto second = cache.lookup(7U, second_key);
  if (!expect(second.hit(), "second artifact must be pinnable")) {
    return false;
  }
  const auto third_reservation = cache.reserve(7U, third_key, artifact.size());
  if (!expect(third_reservation.ok(), "oldest unpinned entry must be evicted for quota") ||
      !expect(!cache.lookup(7U, first_key).hit(),
              "oldest entry must be deterministically evicted") ||
      !expect(cache.lookup(7U, second_key).hit(), "pinned newer entry must be retained") ||
      !expect(cache.publish(*third_reservation.reservation, bytes(artifact), descriptor()) ==
                  metaflux::compiler::PersistentCacheError::None,
              "replacement artifact must publish")) {
    return false;
  }

  auto third = cache.lookup(7U, third_key);
  second = {};
  const auto reserved_one = cache.reserve(8U, key_for("reserved-one"), 8U);
  const auto reserved_two = cache.reserve(8U, key_for("reserved-two"), 8U);
  if (reserved_one.ok()) {
    cache.cancel(*reserved_one.reservation);
  }
  return expect(third.hit(), "third entry must be pinnable") &&
         expect(reserved_one.ok(), "first in-flight reservation must count within quota") &&
         expect(!reserved_two.ok() &&
                    reserved_two.error == metaflux::compiler::PersistentCacheError::QuotaExceeded,
                "second reservation must fail when in-flight bytes exhaust UID quota");
}

bool test_default_free_space_and_atomic_reservations() {
  const metaflux::compiler::PersistentCacheLimits defaults;
  const metaflux::compiler::PersistentCacheConfig default_config;
  if (!expect(defaults.per_uid_bytes == 4ULL * 1024ULL * 1024ULL * 1024ULL &&
                  defaults.global_bytes == 32ULL * 1024ULL * 1024ULL * 1024ULL &&
                  defaults.maximum_entry_bytes == 256ULL * 1024ULL * 1024ULL &&
                  defaults.reserved_free_bytes == 2ULL * 1024ULL * 1024ULL * 1024ULL &&
                  defaults.reserved_free_percent == 5U &&
                  default_config.key_lock_timeout == std::chrono::seconds(30),
              "decision-0014 quotas and the compiler resource-aligned 30-second lock bound must remain "
              "stable")) {
    return false;
  }

  TemporaryDirectory free_space_temporary;
  std::uint64_t free_space_clock = 0;
  auto free_space_config = config_for(free_space_temporary, free_space_clock);
  free_space_config.limits.reserved_free_bytes = 200U;
  free_space_config.limits.reserved_free_percent = 5U;
  free_space_config.filesystem_space = [] {
    return metaflux::compiler::CacheFilesystemSpace{
        .total_bytes = 10'000U,
        .available_bytes = 503U,
    };
  };
  metaflux::compiler::PersistentArtifactCache free_space_cache(free_space_config);
  const auto exact_fit = free_space_cache.reserve(11U, key_for("free-space-fit"), 3U);
  const auto exhausted = free_space_cache.reserve(11U, key_for("free-space-exhausted"), 1U);
  if (!expect(exact_fit.ok(), "a reservation may consume space above the five-percent floor") ||
      !expect(!exhausted.ok() &&
                  exhausted.error == metaflux::compiler::PersistentCacheError::QuotaExceeded,
              "in-flight bytes must preserve max(absolute, percent) filesystem free space")) {
    return false;
  }
  free_space_cache.cancel(*exact_fit.reservation);

  TemporaryDirectory concurrent_temporary;
  std::uint64_t concurrent_clock = 0;
  auto concurrent_config = config_for(concurrent_temporary, concurrent_clock);
  concurrent_config.limits.per_uid_bytes = 10U;
  concurrent_config.limits.global_bytes = 10U;
  metaflux::compiler::PersistentArtifactCache concurrent_cache(concurrent_config);
  metaflux::compiler::ReservationResult first;
  metaflux::compiler::ReservationResult second;
  std::barrier start(3);
  std::thread first_thread([&] {
    start.arrive_and_wait();
    first = concurrent_cache.reserve(77U, key_for("concurrent-first"), 6U);
  });
  std::thread second_thread([&] {
    start.arrive_and_wait();
    second = concurrent_cache.reserve(77U, key_for("concurrent-second"), 6U);
  });
  start.arrive_and_wait();
  first_thread.join();
  second_thread.join();
  if (!expect(first.ok() != second.ok(),
              "concurrent reservations must atomically admit exactly one quota winner") ||
      !expect((first.ok() ? second.error : first.error) ==
                  metaflux::compiler::PersistentCacheError::QuotaExceeded,
              "the concurrent quota loser must receive the stable quota error")) {
    return false;
  }
  concurrent_cache.cancel(first.ok() ? *first.reservation : *second.reservation);
  const auto released = concurrent_cache.reserve(77U, key_for("concurrent-released"), 10U);
  if (!expect(released.ok(), "cancellation must release an atomic reservation completely")) {
    return false;
  }
  concurrent_cache.cancel(*released.reservation);

  TemporaryDirectory oversized_temporary;
  std::uint64_t oversized_clock = 0;
  auto oversized_config = config_for(oversized_temporary, oversized_clock);
  oversized_config.limits.per_uid_bytes = 8U;
  oversized_config.limits.global_bytes = 8U;
  oversized_config.limits.maximum_entry_bytes = 8U;
  metaflux::compiler::PersistentArtifactCache oversized_cache(oversized_config);
  const auto undersized = oversized_cache.reserve(88U, key_for("undersized-reservation"), 4U);
  const std::array oversized_artifact{std::byte{1}, std::byte{2}, std::byte{3}, std::byte{4},
                                      std::byte{5}};
  if (!expect(undersized.ok(), "undersized publication fixture must reserve") ||
      !expect(oversized_cache.publish(*undersized.reservation, oversized_artifact, descriptor()) ==
                  metaflux::compiler::PersistentCacheError::ArtifactTooLarge,
              "publication larger than its reservation must fail deterministically")) {
    return false;
  }
  const auto after_oversized =
      oversized_cache.reserve(88U, key_for("released-oversized-reservation"), 8U);
  if (!expect(after_oversized.ok(), "failed oversized publication must release its reservation")) {
    return false;
  }
  oversized_cache.cancel(*after_oversized.reservation);
  return true;
}

bool test_all_pinned_entries_return_stable_quota_error() {
  TemporaryDirectory temporary;
  std::uint64_t clock = 0;
  auto config = config_for(temporary, clock);
  config.limits.per_uid_bytes = 8U;
  config.limits.global_bytes = 8U;
  metaflux::compiler::PersistentArtifactCache cache(config);
  const std::array artifact{std::byte{1}, std::byte{2}, std::byte{3}, std::byte{4}};
  const auto first_key = key_for("all-pinned-first");
  const auto second_key = key_for("all-pinned-second");
  if (!expect(publish(cache, 9U, first_key, artifact), "first pinned fixture must publish") ||
      !expect(publish(cache, 9U, second_key, artifact), "second pinned fixture must publish")) {
    return false;
  }
  const auto first = cache.lookup(9U, first_key);
  const auto second = cache.lookup(9U, second_key);
  const auto blocked = cache.reserve(9U, key_for("all-pinned-blocked"), artifact.size());
  return expect(first.hit() && second.hit(), "both quota entries must remain open and pinned") &&
         expect(!blocked.ok() &&
                    blocked.error == metaflux::compiler::PersistentCacheError::QuotaExceeded,
                "no eligible open entry must return the stable quota error") &&
         expect(cache.lookup(9U, first_key).hit() && cache.lookup(9U, second_key).hit(),
                "quota failure must preserve every pinned entry");
}

bool test_global_quota_lexical_tie_break() {
  TemporaryDirectory temporary;
  std::uint64_t clock = 0;
  auto config = config_for(temporary, clock);
  config.limits.per_uid_bytes = 12U;
  config.limits.global_bytes = 8U;
  config.clock = [] { return 7U; };
  metaflux::compiler::PersistentArtifactCache cache(config);
  const std::array artifact{std::byte{1}, std::byte{2}, std::byte{3}, std::byte{4}};
  const auto first_key = key_for("global-lexical-first");
  const auto second_key = key_for("global-lexical-second");
  const auto replacement_key = key_for("global-replacement");
  if (!expect(publish(cache, 101U, first_key, artifact), "first global artifact must publish") ||
      !expect(publish(cache, 202U, second_key, artifact), "second global artifact must publish")) {
    return false;
  }
  const auto replacement = cache.reserve(303U, replacement_key, artifact.size());
  const auto& evicted_key = std::min(first_key, second_key);
  const auto& retained_key = std::max(first_key, second_key);
  const auto evicted_uid = evicted_key == first_key ? 101U : 202U;
  const auto retained_uid = retained_key == first_key ? 101U : 202U;
  return expect(replacement.ok(), "global quota must evict one eligible cross-UID entry") &&
         expect(!cache.lookup(evicted_uid, evicted_key).hit(),
                "equal-age global eviction must choose the lexical cache digest") &&
         expect(cache.lookup(retained_uid, retained_key).hit(),
                "equal-age global eviction must retain the other digest") &&
         expect(cache.publish(*replacement.reservation, bytes(artifact), descriptor()) ==
                    metaflux::compiler::PersistentCacheError::None,
                "global replacement artifact must publish");
}

bool test_lock_path_hardening() {
  TemporaryDirectory state_temporary;
  std::uint64_t state_clock = 0;
  auto state_config = config_for(state_temporary, state_clock);
  const auto redirected = state_temporary.path() / "redirected-state";
  std::error_code error;
  std::filesystem::create_directories(state_config.mutable_root, error);
  std::filesystem::create_directories(redirected, error);
  std::filesystem::create_directory_symlink(redirected, state_config.mutable_root / ".state",
                                            error);
  metaflux::compiler::PersistentArtifactCache redirected_cache(state_config);
  const auto redirected_reservation =
      redirected_cache.reserve(699U, key_for("redirected-state-lock"), 4U);
  if (!expect(!error && !redirected_reservation.ok() &&
                  redirected_reservation.error == metaflux::compiler::PersistentCacheError::Io,
              "cache state directory symlinks must be rejected") ||
      !expect(std::filesystem::is_empty(redirected),
              "rejected state symlink must not create lock files at its target")) {
    return false;
  }

  TemporaryDirectory lock_temporary;
  std::uint64_t lock_clock = 0;
  auto lock_config = config_for(lock_temporary, lock_clock);
  const auto key = key_for("redirected-key-lock");
  const auto digest = key.substr(key.size() - 64U);
  const auto lock_directory = lock_config.mutable_root / ".state/key-locks/699";
  std::filesystem::create_directories(lock_directory, error);
  const auto redirect_target = lock_temporary.path() / "lock-target";
  std::ofstream target_output(redirect_target);
  target_output << "unchanged";
  target_output.close();
  std::filesystem::create_symlink(redirect_target, lock_directory / (std::string(digest) + ".lock"),
                                  error);
  metaflux::compiler::PersistentArtifactCache lock_cache(lock_config);
  const auto lock_reservation = lock_cache.reserve(699U, key, 4U);
  std::ifstream target_input(redirect_target);
  std::string target_contents;
  target_input >> target_contents;
  return expect(!error && !lock_reservation.ok() &&
                    lock_reservation.error == metaflux::compiler::PersistentCacheError::Io,
                "per-key lock symlinks must be rejected by O_NOFOLLOW") &&
         expect(target_contents == "unchanged",
                "rejected per-key lock symlink must not modify its target");
}

bool test_cross_instance_reservations_and_pins() {
  TemporaryDirectory reservation_temporary;
  std::uint64_t reservation_clock = 0;
  auto reservation_config = config_for(reservation_temporary, reservation_clock);
  reservation_config.limits.per_uid_bytes = 10U;
  reservation_config.limits.global_bytes = 10U;
  metaflux::compiler::PersistentArtifactCache first_cache(reservation_config);
  metaflux::compiler::PersistentArtifactCache second_cache(reservation_config);
  metaflux::compiler::ReservationResult first;
  metaflux::compiler::ReservationResult second;
  std::barrier start(3);
  std::thread first_thread([&] {
    start.arrive_and_wait();
    first = first_cache.reserve(700U, key_for("cross-instance-first"), 6U);
  });
  std::thread second_thread([&] {
    start.arrive_and_wait();
    second = second_cache.reserve(700U, key_for("cross-instance-second"), 6U);
  });
  start.arrive_and_wait();
  first_thread.join();
  second_thread.join();
  if (!expect(first.ok() != second.ok(),
              "separate cache instances must atomically admit one quota winner") ||
      !expect((first.ok() ? second.error : first.error) ==
                  metaflux::compiler::PersistentCacheError::QuotaExceeded,
              "cross-instance quota loser must receive the stable quota error")) {
    return false;
  }
  (first.ok() ? first_cache : second_cache)
      .cancel(first.ok() ? *first.reservation : *second.reservation);

  const std::array artifact{std::byte{1}, std::byte{2}, std::byte{3}, std::byte{4}};
  const auto shared_key = key_for("cross-instance-single-compiler");
  const auto publisher = first_cache.reserve(700U, shared_key, artifact.size());
  if (!expect(publisher.ok(), "same-key publisher must acquire the first reservation")) {
    return false;
  }
  metaflux::compiler::ReservationResult follower;
  std::thread follower_thread(
      [&] { follower = second_cache.reserve(700U, shared_key, artifact.size()); });
  const auto published = first_cache.publish(*publisher.reservation, bytes(artifact), descriptor());
  follower_thread.join();
  if (!expect(published == metaflux::compiler::PersistentCacheError::None,
              "same-key publisher must commit") ||
      !expect(!follower.ok() &&
                  follower.error == metaflux::compiler::PersistentCacheError::EntryAvailable,
              "same-key follower must recheck the newly published entry instead of compiling")) {
    return false;
  }

  TemporaryDirectory pin_temporary;
  std::uint64_t pin_clock = 0;
  auto pin_config = config_for(pin_temporary, pin_clock);
  pin_config.limits.per_uid_bytes = 8U;
  pin_config.limits.global_bytes = 8U;
  pin_config.clock = [] { return 7U; };
  metaflux::compiler::PersistentArtifactCache pin_owner(pin_config);
  metaflux::compiler::PersistentArtifactCache evictor(pin_config);
  const auto one_key = key_for("cross-instance-pin-one");
  const auto two_key = key_for("cross-instance-pin-two");
  const auto& pinned_key = std::min(one_key, two_key);
  const auto& unpinned_key = std::max(one_key, two_key);
  if (!expect(publish(pin_owner, 701U, pinned_key, artifact), "pinned fixture must publish") ||
      !expect(publish(pin_owner, 701U, unpinned_key, artifact), "unpinned fixture must publish")) {
    return false;
  }
  const auto pinned = pin_owner.lookup(701U, pinned_key);
  const auto replacement = evictor.reserve(701U, key_for("cross-instance-pin-replacement"), 4U);
  const bool pinned_path_exists =
      pinned.hit() && std::filesystem::exists(pinned.entry->artifact_path);
  const bool unpinned_was_evicted = !evictor.lookup(701U, unpinned_key).hit();
  if (replacement.ok()) {
    evictor.cancel(*replacement.reservation);
  }
  return expect(pinned.hit(), "cross-instance fixture must hold an artifact pin") &&
         expect(replacement.ok(), "eviction must find an unpinned cross-instance candidate") &&
         expect(pinned_path_exists, "another cache instance must not remove a pinned artifact") &&
         expect(unpinned_was_evicted,
                "cross-instance eviction must choose the unpinned lexical peer");
}

bool test_forked_quota_reservations() {
  TemporaryDirectory temporary;
  std::uint64_t clock = 0;
  auto config = config_for(temporary, clock);
  config.limits.per_uid_bytes = 10U;
  config.limits.global_bytes = 10U;
  int start[2][2]{};
  int result[2][2]{};
  int release[2][2]{};
  for (std::size_t index = 0; index < 2U; ++index) {
    if (pipe(start[index]) != 0 || pipe(result[index]) != 0 || pipe(release[index]) != 0) {
      return expect(false, "forked quota fixture pipes must be created");
    }
  }

  std::array<pid_t, 2> children{};
  for (std::size_t index = 0; index < children.size(); ++index) {
    children[index] = fork();
    if (children[index] == 0) {
      for (std::size_t pipe_index = 0; pipe_index < children.size(); ++pipe_index) {
        close(start[pipe_index][1]);
        close(result[pipe_index][0]);
        close(release[pipe_index][1]);
        if (pipe_index != index) {
          close(start[pipe_index][0]);
          close(result[pipe_index][1]);
          close(release[pipe_index][0]);
        }
      }
      const auto command = read_byte(start[index][0]);
      metaflux::compiler::PersistentArtifactCache cache(config);
      const auto reservation =
          cache.reserve(702U, key_for(index == 0U ? "fork-first" : "fork-second"), 6U);
      const char outcome =
          reservation.ok()
              ? '1'
              : (reservation.error == metaflux::compiler::PersistentCacheError::QuotaExceeded
                     ? 'q'
                     : 'e');
      const bool reported = command.has_value() && write_byte(result[index][1], outcome);
      const auto released = read_byte(release[index][0]);
      if (reservation.ok()) {
        cache.cancel(*reservation.reservation);
      }
      _exit(reported && released.has_value() ? 0 : 2);
    }
    if (children[index] < 0) {
      return expect(false, "forked quota child must start");
    }
  }

  for (std::size_t index = 0; index < children.size(); ++index) {
    close(start[index][0]);
    close(result[index][1]);
    close(release[index][0]);
  }
  const bool started = write_byte(start[0][1], 's') && write_byte(start[1][1], 's');
  const auto first = read_byte(result[0][0]);
  const auto second = read_byte(result[1][0]);
  const bool released = write_byte(release[0][1], 'r') && write_byte(release[1][1], 'r');
  bool exited = true;
  for (const auto child : children) {
    int status = 0;
    exited = waitpid(child, &status, 0) == child && WIFEXITED(status) && WEXITSTATUS(status) == 0 &&
             exited;
  }
  for (std::size_t index = 0; index < children.size(); ++index) {
    close(start[index][1]);
    close(result[index][0]);
    close(release[index][1]);
  }
  if (!expect(started && released && exited && first.has_value() && second.has_value(),
              "forked quota workers must complete their handshake") ||
      !expect((*first == '1' && *second == 'q') || (*first == 'q' && *second == '1'),
              "independent processes must atomically admit exactly one quota reservation")) {
    return false;
  }

  const pid_t crashed = fork();
  if (crashed == 0) {
    metaflux::compiler::PersistentArtifactCache cache(config);
    const auto reservation = cache.reserve(702U, key_for("fork-crashed-owner"), 10U);
    _exit(reservation.ok() ? 0 : 3);
  }
  int crashed_status = 0;
  if (!expect(crashed > 0 && waitpid(crashed, &crashed_status, 0) == crashed &&
                  WIFEXITED(crashed_status) && WEXITSTATUS(crashed_status) == 0,
              "crash fixture must exit while owning its reservation")) {
    return false;
  }
  metaflux::compiler::PersistentArtifactCache recovered(config);
  const auto recovery = recovered.reconcile();
  const auto after_crash = recovered.reserve(702U, key_for("fork-after-crash"), 10U);
  if (after_crash.ok()) {
    recovered.cancel(*after_crash.reservation);
  }
  return expect(recovery == metaflux::compiler::PersistentCacheError::None,
                "reconcile must remove an unlocked crashed-owner reservation") &&
         expect(after_crash.ok(), "crashed reservation bytes must become fully reusable");
}

bool test_forked_pin_protection() {
  TemporaryDirectory temporary;
  std::uint64_t clock = 0;
  auto config = config_for(temporary, clock);
  config.limits.per_uid_bytes = 8U;
  config.limits.global_bytes = 8U;
  config.clock = [] { return 11U; };
  metaflux::compiler::PersistentArtifactCache owner(config);
  const std::array artifact{std::byte{1}, std::byte{2}, std::byte{3}, std::byte{4}};
  const auto one_key = key_for("fork-pin-one");
  const auto two_key = key_for("fork-pin-two");
  const auto& pinned_key = std::min(one_key, two_key);
  const auto& unpinned_key = std::max(one_key, two_key);
  if (!expect(publish(owner, 703U, pinned_key, artifact), "forked pin fixture must publish") ||
      !expect(publish(owner, 703U, unpinned_key, artifact),
              "forked unpinned fixture must publish")) {
    return false;
  }
  auto pinned_path_lookup = owner.lookup(703U, pinned_key);
  auto unpinned_path_lookup = owner.lookup(703U, unpinned_key);
  if (!expect(pinned_path_lookup.hit() && unpinned_path_lookup.hit(),
              "forked pin paths must be discoverable")) {
    return false;
  }
  const auto pinned_path = pinned_path_lookup.entry->artifact_path;
  const auto unpinned_path = unpinned_path_lookup.entry->artifact_path;
  pinned_path_lookup.entry->pin.reset();
  unpinned_path_lookup.entry->pin.reset();

  int start[2]{};
  int result[2]{};
  if (pipe(start) != 0 || pipe(result) != 0) {
    return expect(false, "forked pin fixture pipes must be created");
  }
  const pid_t child = fork();
  if (child == 0) {
    close(start[1]);
    close(result[0]);
    const auto command = read_byte(start[0]);
    metaflux::compiler::PersistentArtifactCache evictor(config);
    const auto replacement = evictor.reserve(703U, key_for("fork-pin-replacement"), 4U);
    const bool reported =
        command.has_value() && write_byte(result[1], replacement.ok() ? '1' : '0');
    if (replacement.ok()) {
      evictor.cancel(*replacement.reservation);
    }
    _exit(reported ? 0 : 2);
  }
  close(start[0]);
  close(result[1]);
  if (child < 0) {
    return expect(false, "forked pin child must start");
  }
  const auto pinned = owner.lookup(703U, pinned_key);
  const bool started = pinned.hit() && write_byte(start[1], 's');
  const auto outcome = read_byte(result[0]);
  int status = 0;
  const bool exited =
      waitpid(child, &status, 0) == child && WIFEXITED(status) && WEXITSTATUS(status) == 0;
  close(start[1]);
  close(result[0]);
  return expect(started && outcome == std::optional<char>{'1'} && exited,
                "forked evictor must complete while the parent holds its pin") &&
         expect(std::filesystem::exists(pinned_path),
                "forked evictor must preserve the parent-pinned artifact") &&
         expect(!std::filesystem::exists(unpinned_path),
                "forked evictor must remove the unpinned lexical peer");
}

bool test_forked_live_holder_timeout_and_recovery() {
  TemporaryDirectory temporary;
  std::uint64_t clock = 0;
  auto config = config_for(temporary, clock);
  config.key_lock_timeout = std::chrono::seconds(2);
  const auto key = key_for("fork-live-holder-timeout");
  int ready[2]{};
  int release[2]{};
  if (pipe(ready) != 0 || pipe(release) != 0) {
    return expect(false, "live-holder timeout fixture pipes must be created");
  }
  const pid_t child = fork();
  if (child == 0) {
    close(ready[0]);
    close(release[1]);
    metaflux::compiler::PersistentArtifactCache holder(config);
    const auto reservation = holder.reserve(704U, key, 4U);
    const bool reported = write_byte(ready[1], reservation.ok() ? '1' : '0');
    const auto command = read_byte(release[0]);
    if (reservation.ok()) {
      holder.cancel(*reservation.reservation);
    }
    _exit(reported && command.has_value() ? 0 : 2);
  }
  close(ready[1]);
  close(release[0]);
  if (child < 0) {
    return expect(false, "live-holder timeout child must start");
  }
  const auto holder_ready = read_byte(ready[0]);
  metaflux::compiler::PersistentArtifactCache follower(config);
  const auto started = std::chrono::steady_clock::now();
  const auto timed_out = follower.reserve(704U, key, 4U, started + std::chrono::milliseconds(50));
  const auto elapsed = std::chrono::steady_clock::now() - started;
  if (timed_out.ok()) {
    follower.cancel(*timed_out.reservation);
  }
  const bool released = write_byte(release[1], 'r');
  int status = 0;
  const bool exited =
      waitpid(child, &status, 0) == child && WIFEXITED(status) && WEXITSTATUS(status) == 0;
  close(ready[0]);
  close(release[1]);
  const auto recovered = follower.reserve(704U, key, 4U);
  if (recovered.ok()) {
    follower.cancel(*recovered.reservation);
  }
  return expect(holder_ready == std::optional<char>{'1'},
                "live holder must acquire the key before its follower") &&
         expect(
             !timed_out.ok() && timed_out.error == metaflux::compiler::PersistentCacheError::Io &&
                 metaflux::compiler::persistent_cache_error_name(timed_out.error) == "MF_CACHE_IO",
             "live-holder deadline must return the stable cache I/O timeout mapping") &&
         expect(elapsed < std::chrono::seconds(1),
                "live-holder follower must return within a broad bounded interval") &&
         expect(released && exited, "live holder must release and exit cleanly") &&
         expect(recovered.ok(), "same-key reservation must recover after the live holder releases");
}

bool test_aot_miss_does_not_initialize_mutable_state() {
  TemporaryDirectory temporary;
  std::uint64_t clock = 0;
  auto config = config_for(temporary, clock);
  config.mutable_root = config.aot_root / ".mutable-tier-disabled";
  const auto mutable_root = config.mutable_root;
  metaflux::compiler::PersistentArtifactCache cache(std::move(config));

  const auto lookup = cache.lookup(23U, key_for("aot-only-miss"));
  return expect(lookup.error == metaflux::compiler::PersistentCacheError::Miss && !lookup.hit(),
                "an AOT-only lookup miss must retain the stable cache miss result") &&
         expect(!std::filesystem::exists(mutable_root),
                "an AOT-only lookup miss must not initialize mutable cache state");
}

bool test_aot_publication_is_independent_of_mutable_state() {
  TemporaryDirectory temporary;
  std::uint64_t clock = 0;
  auto config = config_for(temporary, clock);
  const auto mutable_root = config.mutable_root;
  std::ofstream blocker(mutable_root, std::ios::binary);
  blocker.put('X');
  blocker.close();

  metaflux::compiler::PersistentArtifactCache cache(std::move(config));
  const std::array artifact{std::byte{0x7f}, std::byte{'E'}, std::byte{'L'}, std::byte{'F'}};
  const auto key = key_for("independent-administrator-aot");
  const auto installed = cache.install_aot(key, bytes(artifact), descriptor());
  const auto lookup = cache.lookup(23U, key);
  return expect(installed == metaflux::compiler::PersistentCacheError::None,
                "administrator AOT publication must not use mutable cache state") &&
         expect(std::filesystem::is_regular_file(mutable_root),
                "administrator AOT publication must leave the mutable root untouched") &&
         expect(lookup.hit() &&
                    lookup.entry->tier ==
                        metaflux::compiler::PersistentCacheTier::AdministratorAot,
                "independently published administrator AOT must be readable");
}

bool test_aot_precedence_and_read_only_mode() {
  TemporaryDirectory temporary;
  std::uint64_t clock = 0;
  auto config = config_for(temporary, clock);
  metaflux::compiler::PersistentArtifactCache cache(config);
  const std::array artifact{std::byte{0x7f}, std::byte{'E'}, std::byte{'L'}, std::byte{'F'}};
  const std::array mutable_artifact{std::byte{1}, std::byte{2}, std::byte{3}, std::byte{4}};
  const auto key = key_for("administrator-aot");
  if (!expect(cache.reconcile() == metaflux::compiler::PersistentCacheError::None,
              "runtime reconciliation must initialize the mutable tier") ||
      !expect(!std::filesystem::exists(config.aot_root),
              "runtime reconciliation must not create or mutate the administrator AOT root") ||
      !expect(publish(cache, 9876U, key, mutable_artifact),
              "mutable precedence fixture must publish") ||
      !expect(cache.install_aot(key, bytes(artifact), descriptor()) ==
                  metaflux::compiler::PersistentCacheError::None,
              "administrator AOT install must publish") ||
      !expect(cache.install_aot(key, bytes(mutable_artifact), descriptor()) ==
                  metaflux::compiler::PersistentCacheError::MetadataMismatch,
              "same-key AOT publication with different bytes must be rejected")) {
    return false;
  }
  const auto lookup = cache.lookup(9876U, key);
  if (!expect(lookup.hit(), "AOT precedence fixture must be readable")) {
    return false;
  }
  struct stat artifact_status{};
  struct stat metadata_status{};
  struct stat directory_status{};
  const auto entry_directory = lookup.entry->artifact_path.parent_path();
  const auto epoch_directory = config.aot_root / "epoch-1";
  if (!expect(lookup.entry->tier == metaflux::compiler::PersistentCacheTier::AdministratorAot,
              "AOT must be consulted before a same-key UID mutable tier") ||
      !expect(lookup.entry->metadata.artifact_sha256 ==
                  metaflux::compiler::sha256_hex(bytes(artifact)),
              "AOT precedence must return administrator content, not the mutable artifact") ||
      !expect(lookup.entry->artifact_path.string().find("aot/epoch-1") != std::string::npos,
              "AOT path must be in its separate epoch root") ||
      !expect(stat(lookup.entry->artifact_path.c_str(), &artifact_status) == 0 &&
                  (artifact_status.st_mode & 0777U) == 0444U &&
                  stat((entry_directory / "metadata.v1").c_str(), &metadata_status) == 0 &&
                  (metadata_status.st_mode & 0777U) == 0444U &&
                  stat(entry_directory.c_str(), &directory_status) == 0 &&
                  (directory_status.st_mode & 0777U) == 0555U,
              "AOT files and digest directory must be read-only before publication")) {
    static_cast<void>(chmod(entry_directory.c_str(), 0755));
    return false;
  }
  if (!expect(chmod(epoch_directory.c_str(), 0555) == 0,
              "AOT epoch fixture must become runtime-read-only")) {
    return false;
  }
  const auto reconciled = cache.reconcile();
  const bool unchanged = stat(epoch_directory.c_str(), &directory_status) == 0 &&
                         (directory_status.st_mode & 0777U) == 0555U;
  static_cast<void>(chmod(epoch_directory.c_str(), 0755));
  static_cast<void>(chmod(entry_directory.c_str(), 0755));
  return expect(reconciled == metaflux::compiler::PersistentCacheError::None,
                "mutable reconciliation must succeed with a read-only AOT epoch") &&
         expect(unchanged, "runtime reconciliation must preserve read-only AOT permissions");
}

} // namespace

int main() {
  return test_isolation_corruption_and_epoch() && test_fault_publication_and_reconciliation() &&
                 test_candidate_scan_io_is_not_treated_as_empty_usage() &&
                 test_remaining_faults_and_stale_temporary_cleanup() &&
                 test_quota_eviction_and_pinning() && test_global_quota_lexical_tie_break() &&
                 test_lock_path_hardening() && test_default_free_space_and_atomic_reservations() &&
                 test_all_pinned_entries_return_stable_quota_error() &&
                 test_cross_instance_reservations_and_pins() && test_forked_quota_reservations() &&
                 test_forked_pin_protection() && test_forked_live_holder_timeout_and_recovery() &&
                 test_aot_miss_does_not_initialize_mutable_state() &&
                 test_aot_publication_is_independent_of_mutable_state() &&
                 test_aot_precedence_and_read_only_mode()
             ? 0
             : 1;
}
