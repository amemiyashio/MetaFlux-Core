#ifndef METAFLUX_COMPILER_ARTIFACT_CACHE_HPP
#define METAFLUX_COMPILER_ARTIFACT_CACHE_HPP

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace metaflux::compiler {

enum class PersistentCacheError : std::uint32_t {
  None,
  Miss,
  EntryAvailable,
  InvalidKey,
  ArtifactTooLarge,
  QuotaExceeded,
  ReservationNotFound,
  MetadataMismatch,
  Io,
};

[[nodiscard]] std::string_view persistent_cache_error_name(PersistentCacheError error) noexcept;

enum class PersistentCacheTier : std::uint32_t {
  MutableUser,
  AdministratorAot,
};

enum class CacheFaultPoint : std::uint32_t {
  BeforeCandidateScan,
  AfterArtifactFsync,
  AfterMetadataFsync,
  BeforeRename,
  AfterRename,
};

struct PersistentCacheLimits {
  std::uint64_t per_uid_bytes = 4ULL * 1024ULL * 1024ULL * 1024ULL;
  std::uint64_t global_bytes = 32ULL * 1024ULL * 1024ULL * 1024ULL;
  std::uint64_t maximum_entry_bytes = 256ULL * 1024ULL * 1024ULL;
  std::uint64_t reserved_free_bytes = 2ULL * 1024ULL * 1024ULL * 1024ULL;
  std::uint32_t reserved_free_percent = 5;
};

struct CacheFilesystemSpace {
  std::uint64_t total_bytes = 0;
  std::uint64_t available_bytes = 0;
};

struct PersistentCacheConfig {
  std::filesystem::path mutable_root = "/var/cache/metaflux/compiler";
  std::filesystem::path aot_root = "/var/lib/metaflux/aot";
  std::uint32_t compiler_epoch = 1;
  PersistentCacheLimits limits{};
  std::function<std::uint64_t()> clock;
  std::function<bool(CacheFaultPoint)> inject_fault;
  std::function<std::optional<CacheFilesystemSpace>()> filesystem_space;
  std::chrono::milliseconds key_lock_timeout = std::chrono::seconds(30);
};

struct ArtifactDescriptor {
  std::uint32_t kernel_ir_schema = 0;
  std::uint32_t helper_abi = 0;
  std::string payload;
};

struct PersistentArtifactMetadata {
  std::uint32_t format_version = 1;
  std::uint32_t compiler_epoch = 0;
  std::uint32_t kernel_ir_schema = 0;
  std::uint32_t helper_abi = 0;
  std::uint64_t artifact_size = 0;
  std::uint64_t last_used = 0;
  std::string cache_key;
  std::string artifact_sha256;
  std::string payload;
};

struct PersistentCacheEntry {
  PersistentCacheTier tier = PersistentCacheTier::MutableUser;
  std::filesystem::path artifact_path;
  PersistentArtifactMetadata metadata;
  std::shared_ptr<const void> pin;
};

struct PersistentCacheLookup {
  PersistentCacheError error = PersistentCacheError::Miss;
  std::optional<PersistentCacheEntry> entry;
  bool corruption_recovered = false;

  [[nodiscard]] bool hit() const noexcept { return entry.has_value(); }
};

struct CacheReservation {
  std::uint64_t token = 0;
  std::uint32_t uid = 0;
  std::uint64_t bytes = 0;
  std::string cache_key;

  [[nodiscard]] bool valid() const noexcept { return token != 0U; }
};

struct ReservationResult {
  PersistentCacheError error = PersistentCacheError::Io;
  std::optional<CacheReservation> reservation;

  [[nodiscard]] bool ok() const noexcept { return reservation.has_value(); }
};

using ArtifactValidator = std::function<bool(std::span<const std::byte>)>;

class PersistentArtifactCache {
public:
  struct State;

  explicit PersistentArtifactCache(PersistentCacheConfig config);
  ~PersistentArtifactCache();
  PersistentArtifactCache(PersistentArtifactCache&&) noexcept;
  PersistentArtifactCache& operator=(PersistentArtifactCache&&) noexcept;
  PersistentArtifactCache(const PersistentArtifactCache&) = delete;
  PersistentArtifactCache& operator=(const PersistentArtifactCache&) = delete;

  [[nodiscard]] PersistentCacheError reconcile();
  [[nodiscard]] PersistentCacheLookup lookup(std::uint32_t uid, std::string_view cache_key,
                                             const ArtifactValidator& validator = {});
  [[nodiscard]] ReservationResult
  reserve(std::uint32_t uid, std::string_view cache_key, std::uint64_t bytes,
          std::optional<std::chrono::steady_clock::time_point> deadline = std::nullopt);
  [[nodiscard]] PersistentCacheError publish(const CacheReservation& reservation,
                                             std::span<const std::byte> artifact,
                                             const ArtifactDescriptor& descriptor);
  void cancel(const CacheReservation& reservation) noexcept;
  [[nodiscard]] PersistentCacheError install_aot(std::string_view cache_key,
                                                 std::span<const std::byte> artifact,
                                                 const ArtifactDescriptor& descriptor);
  [[nodiscard]] PersistentCacheError invalidate(const PersistentCacheEntry& entry);

private:
  std::shared_ptr<State> state_;
};

} // namespace metaflux::compiler

#endif
