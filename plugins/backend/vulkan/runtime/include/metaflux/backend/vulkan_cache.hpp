#ifndef METAFLUX_BACKEND_VULKAN_CACHE_HPP
#define METAFLUX_BACKEND_VULKAN_CACHE_HPP

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>

namespace metaflux::backend::vulkan {

struct CacheIdentity {
  std::array<std::uint8_t, 32> kernel_ir_digest{};
  std::array<std::uint8_t, 32> target_digest{};
  std::uint32_t compiler_epoch = 0;
  std::uint32_t lowering_epoch = 0;
  std::uint32_t spirv_tools_epoch = 0;
  std::uint32_t fp_mode = 0;
  std::uint32_t argument_abi = 0;
  std::uint32_t backend_abi = 0;
  std::array<std::uint8_t, 16> specialization_digest{};

  std::uint32_t vendor_id = 0;
  std::uint32_t device_id = 0;
  std::uint32_t driver_version = 0;
  std::array<std::uint8_t, 16> device_uuid{};
  std::array<std::uint8_t, 16> driver_uuid{};
  std::array<std::uint8_t, 16> pipeline_cache_uuid{};

  [[nodiscard]] std::string portable_key() const;
  [[nodiscard]] std::string device_key() const;
};

enum class WarmLaunchEvent : std::uint32_t {
  cache_lookup = 0,
  pipeline_binding = 1,
  argument_binding = 2,
  submit = 3,
  compiler = 4,
  validator = 5,
  shader_module_creation = 6,
  pipeline_creation = 7,
  vulkan_allocation = 8,
  metaflux_allocation = 9,
};

enum class WarmLaunchStatus : std::uint32_t {
  success = 0,
  invalid_argument = 1,
  invalid_order = 2,
  forbidden_event = 3,
  missing_event = 4,
};

enum class CacheStatus : std::uint32_t {
  success = 0,
  invalid_argument = 1,
  hit = 2,
  miss = 3,
  corrupt = 4,
  pinned = 5,
  quota_exceeded = 6,
  not_found = 7,
  io_error = 8,
  stale_generation = 9,
};

class CacheCatalog final {
public:
  explicit CacheCatalog(std::size_t max_entries = 64U) noexcept : max_entries_(max_entries) {}

  [[nodiscard]] CacheStatus publish(std::string key, std::string payload, bool device_bound);
  [[nodiscard]] CacheStatus lookup(const std::string& key, std::string* out_payload);
  [[nodiscard]] CacheStatus lookup(const std::string& key, bool device_bound,
                                   std::string* out_payload);
  [[nodiscard]] CacheStatus admit_publish(const std::string& key) const noexcept;
  [[nodiscard]] CacheStatus mark_corrupt(const std::string& key) noexcept;
  [[nodiscard]] CacheStatus pin(const std::string& key) noexcept;
  [[nodiscard]] CacheStatus unpin(const std::string& key) noexcept;
  [[nodiscard]] CacheStatus admit_remove(const std::string& key, bool device_bound) const noexcept;
  [[nodiscard]] CacheStatus remove(const std::string& key, bool device_bound) noexcept;
  [[nodiscard]] CacheStatus evict_one() noexcept;
  [[nodiscard]] std::size_t size() const noexcept { return entries_.size(); }
  [[nodiscard]] std::size_t max_entries() const noexcept { return max_entries_; }

private:
  struct Entry {
    std::string payload;
    std::uint64_t last_use = 0;
    std::uint64_t live_references = 0;
    bool device_bound = false;
    bool corrupt = false;
  };

  std::size_t max_entries_ = 0;
  std::uint64_t clock_ = 0;
  std::map<std::string, Entry> entries_;
};

// Filesystem persistence for portable metadata and device-bound cache blobs.
// The catalog above remains the in-process residency/quota authority.
class CacheFileStore final {
public:
  explicit CacheFileStore(std::filesystem::path root,
                          std::size_t max_payload_bytes = 64U * 1024U * 1024U)
      : root_(std::move(root)), max_payload_bytes_(max_payload_bytes) {}

  [[nodiscard]] CacheStatus publish(std::string_view key, std::string_view payload,
                                    bool device_bound);
  [[nodiscard]] CacheStatus lookup(std::string_view key, bool device_bound,
                                   std::string* out_payload);
  [[nodiscard]] CacheStatus invalidate_device(std::string_view key);
  [[nodiscard]] std::filesystem::path entry_path(std::string_view key, bool device_bound) const;
  [[nodiscard]] const std::filesystem::path& root() const noexcept { return root_; }
  [[nodiscard]] std::size_t max_payload_bytes() const noexcept { return max_payload_bytes_; }

private:
  [[nodiscard]] CacheStatus read_locked(std::string_view key, bool device_bound,
                                        std::string* out_payload);
  [[nodiscard]] CacheStatus write_atomic_locked(std::string_view key, std::string_view payload,
                                                bool device_bound);
  [[nodiscard]] static std::string key_token(std::string_view key, bool device_bound);
  [[nodiscard]] static std::string payload_digest(std::string_view payload);

  std::filesystem::path root_;
  std::size_t max_payload_bytes_ = 0;
  mutable std::mutex mutex_;
};

// One in-process authority for durable cache files, bounded resident entries,
// and generation-scoped pipeline bindings. Cross-process stampede coordination
// and actual Vulkan pipeline objects remain above this host-independent
// repository boundary.
class PersistentCacheRepository final {
public:
  explicit PersistentCacheRepository(
      std::filesystem::path root, std::size_t max_entries = 64U,
      std::size_t max_payload_bytes = 64U * 1024U * 1024U,
      std::chrono::milliseconds key_lock_timeout = std::chrono::milliseconds(30000))
      : catalog_(max_entries), files_(std::move(root), max_payload_bytes),
        key_lock_timeout_(key_lock_timeout) {}

  using Producer = std::function<std::optional<std::string>()>;

  [[nodiscard]] CacheStatus publish(std::string_view key, std::string_view payload,
                                    bool device_bound);
  [[nodiscard]] CacheStatus lookup(std::string_view key, bool device_bound,
                                   std::string* out_payload);
  [[nodiscard]] CacheStatus lookup_or_publish(std::string_view key, bool device_bound,
                                              const Producer& producer, std::string* out_payload);
  [[nodiscard]] CacheStatus acquire_pipeline(std::string_view key, std::uint64_t generation);
  [[nodiscard]] CacheStatus release_pipeline(std::string_view key, std::uint64_t generation);
  [[nodiscard]] CacheStatus pin(std::string_view key) noexcept;
  [[nodiscard]] CacheStatus unpin(std::string_view key) noexcept;
  [[nodiscard]] CacheStatus invalidate_device(std::string_view key);
  [[nodiscard]] std::size_t size() const noexcept;
  [[nodiscard]] std::size_t max_entries() const noexcept;
  [[nodiscard]] const std::filesystem::path& root() const noexcept { return files_.root(); }

private:
  mutable std::mutex mutex_;
  CacheCatalog catalog_;
  CacheFileStore files_;
  std::chrono::milliseconds key_lock_timeout_;
  std::map<std::string, std::uint64_t> active_pipeline_bindings_;
};

// Host-independent warm-launch admission. A successful session owns one
// generation-scoped pipeline binding and exposes only the four allowed steps;
// compiler, validator, object creation, and allocation are outside this path.
class WarmLaunchSession final {
public:
  WarmLaunchSession() noexcept = default;
  ~WarmLaunchSession() noexcept;

  WarmLaunchSession(const WarmLaunchSession&) = delete;
  WarmLaunchSession& operator=(const WarmLaunchSession&) = delete;

  [[nodiscard]] static CacheStatus start(PersistentCacheRepository& repository,
                                          std::string_view key, bool device_bound,
                                          std::uint64_t generation,
                                          std::string* out_payload,
                                          WarmLaunchSession& out) noexcept;
  [[nodiscard]] WarmLaunchStatus bind_arguments(std::uint64_t argument_block_size) noexcept;
  [[nodiscard]] WarmLaunchStatus submit() noexcept;
  [[nodiscard]] CacheStatus cancel() noexcept;
  [[nodiscard]] CacheStatus finish() noexcept;
  [[nodiscard]] std::span<const WarmLaunchEvent> trace() const noexcept {
    return std::span<const WarmLaunchEvent>(events_.data(), event_count_);
  }
  [[nodiscard]] bool active() const noexcept { return repository_ != nullptr; }
  [[nodiscard]] std::uint64_t generation() const noexcept { return generation_; }
  [[nodiscard]] bool submitted() const noexcept { return submitted_; }

private:
  [[nodiscard]] WarmLaunchStatus append(WarmLaunchEvent event) noexcept;
  void release() noexcept;

  PersistentCacheRepository* repository_ = nullptr;
  std::string_view key_{};
  bool device_bound_ = false;
  std::uint64_t generation_ = 0U;
  std::array<WarmLaunchEvent, 4> events_{};
  std::size_t event_count_ = 0U;
  bool arguments_bound_ = false;
  bool submitted_ = false;
};

[[nodiscard]] const char* cache_status_string(CacheStatus status) noexcept;
[[nodiscard]] WarmLaunchStatus
validate_warm_launch_trace(std::span<const WarmLaunchEvent> events) noexcept;
[[nodiscard]] const char* warm_launch_status_string(WarmLaunchStatus status) noexcept;

} // namespace metaflux::backend::vulkan

#endif
