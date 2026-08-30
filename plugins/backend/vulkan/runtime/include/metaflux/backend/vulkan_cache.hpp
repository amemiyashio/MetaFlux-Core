#ifndef METAFLUX_BACKEND_VULKAN_CACHE_HPP
#define METAFLUX_BACKEND_VULKAN_CACHE_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <map>
#include <mutex>
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
};

class CacheCatalog final {
public:
  explicit CacheCatalog(std::size_t max_entries = 64U) noexcept : max_entries_(max_entries) {}

  [[nodiscard]] CacheStatus publish(std::string key, std::string payload, bool device_bound);
  [[nodiscard]] CacheStatus lookup(const std::string& key, std::string* out_payload);
  [[nodiscard]] CacheStatus mark_corrupt(const std::string& key) noexcept;
  [[nodiscard]] CacheStatus pin(const std::string& key) noexcept;
  [[nodiscard]] CacheStatus unpin(const std::string& key) noexcept;
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

[[nodiscard]] const char* cache_status_string(CacheStatus status) noexcept;

} // namespace metaflux::backend::vulkan

#endif
