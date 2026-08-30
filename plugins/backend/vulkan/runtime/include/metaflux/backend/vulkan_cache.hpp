#ifndef METAFLUX_BACKEND_VULKAN_CACHE_HPP
#define METAFLUX_BACKEND_VULKAN_CACHE_HPP

#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <string_view>

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
};

class CacheCatalog final {
 public:
  explicit CacheCatalog(std::size_t max_entries = 64U) noexcept
      : max_entries_(max_entries) {}

  [[nodiscard]] CacheStatus publish(std::string key, std::string payload,
                                    bool device_bound);
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

[[nodiscard]] const char* cache_status_string(CacheStatus status) noexcept;

} // namespace metaflux::backend::vulkan

#endif
