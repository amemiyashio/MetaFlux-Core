#include "metaflux/backend/vulkan_cache.hpp"

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdio>
#include <fcntl.h>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string_view>
#include <sys/file.h>
#include <system_error>
#include <thread>
#include <unistd.h>
#include <utility>

namespace metaflux::backend::vulkan {

namespace {

constexpr std::string_view kFileMagic = "metaflux-vulkan-cache-v1";
constexpr std::size_t kMaxKeyBytes = 1024U;
constexpr std::size_t kEnvelopeOverhead = 2048U;
constexpr std::uint64_t kFnvOffset = UINT64_C(1469598103934665603);
constexpr std::uint64_t kFnvPrime = UINT64_C(1099511628211);

class ScopedDescriptor final {
public:
  explicit ScopedDescriptor(int descriptor) noexcept : descriptor_(descriptor) {}
  ~ScopedDescriptor() {
    if (descriptor_ >= 0) {
      static_cast<void>(::close(descriptor_));
    }
  }
  ScopedDescriptor(const ScopedDescriptor&) = delete;
  ScopedDescriptor& operator=(const ScopedDescriptor&) = delete;
  ScopedDescriptor(ScopedDescriptor&& other) noexcept
      : descriptor_(std::exchange(other.descriptor_, -1)) {}
  ScopedDescriptor& operator=(ScopedDescriptor&& other) noexcept {
    if (this != &other) {
      if (descriptor_ >= 0) {
        static_cast<void>(::close(descriptor_));
      }
      descriptor_ = std::exchange(other.descriptor_, -1);
    }
    return *this;
  }

  [[nodiscard]] int get() const noexcept { return descriptor_; }

private:
  int descriptor_ = -1;
};

std::optional<ScopedDescriptor> acquire_cache_key_lock(const std::filesystem::path& path,
                                                       std::chrono::milliseconds timeout) {
  std::error_code error;
  std::filesystem::create_directories(path.parent_path(), error);
  if (error) {
    return std::nullopt;
  }
  ScopedDescriptor descriptor(
      ::open(path.c_str(), O_RDWR | O_CREAT | O_CLOEXEC | O_NOFOLLOW, 0600));
  if (descriptor.get() < 0) {
    return std::nullopt;
  }
  const auto wait = std::max(timeout, std::chrono::milliseconds::zero());
  const auto deadline = std::chrono::steady_clock::now() + wait;
  for (;;) {
    if (::flock(descriptor.get(), LOCK_EX | LOCK_NB) == 0) {
      return std::optional<ScopedDescriptor>(std::move(descriptor));
    }
    if (errno == EINTR) {
      continue;
    }
    if (errno != EWOULDBLOCK && errno != EAGAIN) {
      return std::nullopt;
    }
    const auto now = std::chrono::steady_clock::now();
    if (now >= deadline) {
      return std::nullopt;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
}

std::uint64_t fnv1a(std::string_view value, std::uint64_t seed) noexcept {
  std::uint64_t result = seed;
  for (const char raw_byte : value) {
    const auto byte = static_cast<unsigned char>(raw_byte);
    result ^= byte;
    result *= kFnvPrime;
  }
  return result;
}

std::string hex_u64(std::uint64_t value) {
  std::ostringstream stream;
  stream << std::hex << std::setfill('0') << std::setw(16) << value;
  return stream.str();
}

bool valid_key(std::string_view key) noexcept {
  if (key.empty() || key.size() > kMaxKeyBytes) {
    return false;
  }
  for (const char raw_byte : key) {
    const auto byte = static_cast<unsigned char>(raw_byte);
    if (byte < 0x20U || byte == 0x7fU) {
      return false;
    }
  }
  return true;
}

bool write_all(int fd, std::string_view bytes) noexcept {
  const char* cursor = bytes.data();
  std::size_t remaining = bytes.size();
  while (remaining != 0U) {
    const auto written = ::write(fd, cursor, remaining);
    if (written < 0) {
      if (errno == EINTR) {
        continue;
      }
      return false;
    }
    if (written == 0) {
      return false;
    }
    cursor += written;
    remaining -= static_cast<std::size_t>(written);
  }
  return true;
}

bool sync_directory(const std::filesystem::path& directory) noexcept {
  const int fd = ::open(directory.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
  if (fd < 0) {
    return false;
  }
  const bool synced = ::fsync(fd) == 0;
  (void)::close(fd);
  return synced;
}

template <typename Range> void append_hex(std::ostringstream& stream, const Range& bytes) {
  stream << std::hex << std::setfill('0');
  for (const auto byte : bytes) {
    stream << std::setw(2) << static_cast<unsigned int>(byte);
  }
  stream << std::dec;
}

std::string canonical_key(const CacheIdentity& identity, bool device_bound) {
  std::ostringstream stream;
  stream << "metaflux.vulkan.cache.v1|portable|kernel=";
  append_hex(stream, identity.kernel_ir_digest);
  stream << "|target=";
  append_hex(stream, identity.target_digest);
  stream << "|compiler=" << identity.compiler_epoch << "|lowering=" << identity.lowering_epoch
         << "|spirv-tools=" << identity.spirv_tools_epoch << "|fp=" << identity.fp_mode
         << "|argument-abi=" << identity.argument_abi << "|backend-abi=" << identity.backend_abi
         << "|specialization=";
  append_hex(stream, identity.specialization_digest);
  if (device_bound) {
    stream << "|device|vendor=" << identity.vendor_id << "|device=" << identity.device_id
           << "|driver=" << identity.driver_version << "|device-uuid=";
    append_hex(stream, identity.device_uuid);
    stream << "|driver-uuid=";
    append_hex(stream, identity.driver_uuid);
    stream << "|pipeline-cache-uuid=";
    append_hex(stream, identity.pipeline_cache_uuid);
  }
  return stream.str();
}

} // namespace

std::string CacheIdentity::portable_key() const { return canonical_key(*this, false); }

std::string CacheIdentity::device_key() const { return canonical_key(*this, true); }

WarmLaunchStatus validate_warm_launch_trace(std::span<const WarmLaunchEvent> events) noexcept {
  constexpr std::array<WarmLaunchEvent, 4> expected{
      WarmLaunchEvent::cache_lookup,
      WarmLaunchEvent::pipeline_binding,
      WarmLaunchEvent::argument_binding,
      WarmLaunchEvent::submit,
  };
  if (events.empty()) {
    return WarmLaunchStatus::missing_event;
  }
  for (std::size_t index = 0U; index < events.size(); ++index) {
    const auto raw = static_cast<std::uint32_t>(events[index]);
    if (raw > static_cast<std::uint32_t>(WarmLaunchEvent::metaflux_allocation)) {
      return WarmLaunchStatus::invalid_argument;
    }
    if (raw >= static_cast<std::uint32_t>(WarmLaunchEvent::compiler)) {
      return WarmLaunchStatus::forbidden_event;
    }
    if (index >= expected.size() || events[index] != expected[index]) {
      return WarmLaunchStatus::invalid_order;
    }
  }
  return events.size() == expected.size() ? WarmLaunchStatus::success
                                          : WarmLaunchStatus::missing_event;
}

const char* warm_launch_status_string(WarmLaunchStatus status) noexcept {
  switch (status) {
  case WarmLaunchStatus::success:
    return "success";
  case WarmLaunchStatus::invalid_argument:
    return "invalid-argument";
  case WarmLaunchStatus::invalid_order:
    return "invalid-order";
  case WarmLaunchStatus::forbidden_event:
    return "forbidden-event";
  case WarmLaunchStatus::missing_event:
    return "missing-event";
  }
  return "unknown";
}

WarmLaunchSession::~WarmLaunchSession() noexcept { release(); }

WarmLaunchStatus WarmLaunchSession::append(WarmLaunchEvent event) noexcept {
  if (event_count_ >= events_.size()) {
    return WarmLaunchStatus::invalid_order;
  }
  events_[event_count_++] = event;
  return WarmLaunchStatus::success;
}

void WarmLaunchSession::release() noexcept {
  if (repository_ != nullptr) {
    static_cast<void>(repository_->release_pipeline(key_, generation_));
  }
  repository_ = nullptr;
  key_ = {};
  device_bound_ = false;
  generation_ = 0U;
  event_count_ = 0U;
  arguments_bound_ = false;
  submitted_ = false;
}

CacheStatus WarmLaunchSession::start(PersistentCacheRepository& repository, std::string_view key,
                                     bool device_bound, std::uint64_t generation,
                                     std::string* out_payload, WarmLaunchSession& out) noexcept {
  if (key.empty() || generation == 0U || out_payload == nullptr || out.active()) {
    return CacheStatus::invalid_argument;
  }
  std::string payload;
  const auto lookup = repository.lookup(key, device_bound, &payload);
  if (lookup != CacheStatus::hit) {
    return lookup;
  }
  const auto acquired = repository.acquire_pipeline(key, generation);
  if (acquired != CacheStatus::success) {
    return acquired;
  }
  out.repository_ = &repository;
  out.key_ = key;
  out.device_bound_ = device_bound;
  out.generation_ = generation;
  out.event_count_ = 0U;
  out.arguments_bound_ = false;
  out.submitted_ = false;
  *out_payload = std::move(payload);
  if (out.append(WarmLaunchEvent::cache_lookup) != WarmLaunchStatus::success ||
      out.append(WarmLaunchEvent::pipeline_binding) != WarmLaunchStatus::success) {
    out.release();
    return CacheStatus::invalid_argument;
  }
  return CacheStatus::success;
}

WarmLaunchStatus WarmLaunchSession::bind_arguments(std::uint64_t argument_block_size) noexcept {
  if (repository_ == nullptr || event_count_ != 2U || argument_block_size == 0U ||
      arguments_bound_ || submitted_) {
    return WarmLaunchStatus::invalid_argument;
  }
  const auto status = append(WarmLaunchEvent::argument_binding);
  if (status == WarmLaunchStatus::success) {
    arguments_bound_ = true;
  }
  return status;
}

WarmLaunchStatus WarmLaunchSession::submit() noexcept {
  if (repository_ == nullptr || event_count_ != 3U || !arguments_bound_ || submitted_) {
    return WarmLaunchStatus::invalid_order;
  }
  const auto status = append(WarmLaunchEvent::submit);
  if (status == WarmLaunchStatus::success) {
    submitted_ = true;
  }
  return status;
}

CacheStatus WarmLaunchSession::finish() noexcept {
  if (repository_ == nullptr || !submitted_) {
    return CacheStatus::invalid_argument;
  }
  return cancel();
}

CacheStatus WarmLaunchSession::cancel() noexcept {
  if (repository_ == nullptr) {
    return CacheStatus::invalid_argument;
  }
  PersistentCacheRepository* repository = repository_;
  const std::string_view key = key_;
  const std::uint64_t generation = generation_;
  repository_ = nullptr;
  key_ = {};
  device_bound_ = false;
  generation_ = 0U;
  event_count_ = 0U;
  arguments_bound_ = false;
  submitted_ = false;
  return repository->release_pipeline(key, generation);
}

CacheStatus CacheCatalog::publish(std::string key, std::string payload, bool device_bound) {
  if (key.empty() || payload.empty() || max_entries_ == 0U) {
    return CacheStatus::invalid_argument;
  }
  auto existing = entries_.find(key);
  if (existing != entries_.end()) {
    if (existing->second.live_references != 0U) {
      return CacheStatus::pinned;
    }
    existing->second.payload = std::move(payload);
    existing->second.last_use = ++clock_;
    existing->second.device_bound = device_bound;
    existing->second.corrupt = false;
    return CacheStatus::success;
  }
  if (entries_.size() >= max_entries_ && evict_one() != CacheStatus::success) {
    return CacheStatus::quota_exceeded;
  }
  entries_.emplace(std::move(key), Entry{.payload = std::move(payload),
                                         .last_use = ++clock_,
                                         .live_references = 0U,
                                         .device_bound = device_bound,
                                         .corrupt = false});
  return CacheStatus::success;
}

CacheStatus CacheCatalog::admit_publish(const std::string& key) const noexcept {
  if (key.empty() || max_entries_ == 0U) {
    return CacheStatus::invalid_argument;
  }
  const auto existing = entries_.find(key);
  if (existing != entries_.end()) {
    return existing->second.live_references == 0U ? CacheStatus::success : CacheStatus::pinned;
  }
  if (entries_.size() < max_entries_) {
    return CacheStatus::success;
  }
  const auto evictable = std::find_if(entries_.begin(), entries_.end(), [](const auto& entry) {
    return entry.second.live_references == 0U;
  });
  return evictable == entries_.end() ? CacheStatus::quota_exceeded : CacheStatus::success;
}

CacheStatus CacheCatalog::lookup(const std::string& key, std::string* out_payload) {
  if (key.empty() || out_payload == nullptr) {
    return CacheStatus::invalid_argument;
  }
  const auto position = entries_.find(key);
  if (position == entries_.end()) {
    return CacheStatus::miss;
  }
  if (position->second.corrupt) {
    entries_.erase(position);
    return CacheStatus::corrupt;
  }
  position->second.last_use = ++clock_;
  *out_payload = position->second.payload;
  return CacheStatus::hit;
}

CacheStatus CacheCatalog::lookup(const std::string& key, bool device_bound,
                                 std::string* out_payload) {
  if (key.empty() || out_payload == nullptr) {
    return CacheStatus::invalid_argument;
  }
  const auto position = entries_.find(key);
  if (position == entries_.end() || position->second.device_bound != device_bound) {
    return CacheStatus::miss;
  }
  if (position->second.corrupt) {
    entries_.erase(position);
    return CacheStatus::corrupt;
  }
  position->second.last_use = ++clock_;
  *out_payload = position->second.payload;
  return CacheStatus::hit;
}

CacheStatus CacheCatalog::mark_corrupt(const std::string& key) noexcept {
  const auto position = entries_.find(key);
  if (position == entries_.end()) {
    return CacheStatus::not_found;
  }
  if (position->second.live_references != 0U) {
    return CacheStatus::pinned;
  }
  position->second.corrupt = true;
  return CacheStatus::success;
}

CacheStatus CacheCatalog::pin(const std::string& key) noexcept {
  const auto position = entries_.find(key);
  if (position == entries_.end()) {
    return CacheStatus::not_found;
  }
  if (position->second.corrupt) {
    return CacheStatus::corrupt;
  }
  ++position->second.live_references;
  position->second.last_use = ++clock_;
  return CacheStatus::success;
}

CacheStatus CacheCatalog::unpin(std::string_view key) noexcept {
  const auto position = entries_.find(key);
  if (position == entries_.end()) {
    return CacheStatus::not_found;
  }
  if (position->second.live_references == 0U) {
    return CacheStatus::invalid_argument;
  }
  --position->second.live_references;
  position->second.last_use = ++clock_;
  return CacheStatus::success;
}

CacheStatus CacheCatalog::admit_remove(const std::string& key, bool device_bound) const noexcept {
  if (key.empty()) {
    return CacheStatus::invalid_argument;
  }
  const auto position = entries_.find(key);
  if (position == entries_.end()) {
    return CacheStatus::not_found;
  }
  if (position->second.device_bound != device_bound) {
    return CacheStatus::invalid_argument;
  }
  return position->second.live_references == 0U ? CacheStatus::success : CacheStatus::pinned;
}

CacheStatus CacheCatalog::remove(const std::string& key, bool device_bound) noexcept {
  const auto admission = admit_remove(key, device_bound);
  if (admission != CacheStatus::success) {
    return admission;
  }
  entries_.erase(key);
  return CacheStatus::success;
}

CacheStatus CacheCatalog::evict_one() noexcept {
  auto candidate = entries_.end();
  for (auto position = entries_.begin(); position != entries_.end(); ++position) {
    if (position->second.live_references != 0U) {
      continue;
    }
    if (candidate == entries_.end() || position->second.last_use < candidate->second.last_use) {
      candidate = position;
    }
  }
  if (candidate == entries_.end()) {
    return CacheStatus::quota_exceeded;
  }
  entries_.erase(candidate);
  return CacheStatus::success;
}

const char* cache_status_string(CacheStatus status) noexcept {
  switch (status) {
  case CacheStatus::success:
    return "success";
  case CacheStatus::invalid_argument:
    return "invalid-argument";
  case CacheStatus::hit:
    return "hit";
  case CacheStatus::miss:
    return "miss";
  case CacheStatus::corrupt:
    return "corrupt";
  case CacheStatus::pinned:
    return "pinned";
  case CacheStatus::quota_exceeded:
    return "quota-exceeded";
  case CacheStatus::not_found:
    return "not-found";
  case CacheStatus::io_error:
    return "io-error";
  case CacheStatus::stale_generation:
    return "stale-generation";
  }
  return "unknown";
}

std::string CacheFileStore::key_token(std::string_view key, bool device_bound) {
  const auto first = fnv1a(key, kFnvOffset);
  const auto second = fnv1a(key, kFnvOffset ^ UINT64_C(0x9e3779b97f4a7c15));
  return hex_u64(first) + hex_u64(second) + (device_bound ? ".device" : ".portable");
}

std::string CacheFileStore::payload_digest(std::string_view payload) {
  return hex_u64(fnv1a(payload, kFnvOffset));
}

std::filesystem::path CacheFileStore::entry_path(std::string_view key, bool device_bound) const {
  return root_ / (key_token(key, device_bound) + ".cache");
}

CacheStatus CacheFileStore::write_atomic_locked(std::string_view key, std::string_view payload,
                                                bool device_bound) {
  if (!valid_key(key) || payload.empty() || max_payload_bytes_ == 0U ||
      payload.size() > max_payload_bytes_ || root_.empty()) {
    return CacheStatus::invalid_argument;
  }
  std::error_code error;
  std::filesystem::create_directories(root_, error);
  if (error || !std::filesystem::is_directory(root_, error) || error) {
    return CacheStatus::io_error;
  }

  std::string envelope;
  envelope.reserve(kEnvelopeOverhead + key.size() + payload.size());
  envelope.append(kFileMagic);
  envelope.append("\nkey=");
  envelope.append(key);
  envelope.append("\nmode=");
  envelope.append(device_bound ? "device" : "portable");
  envelope.append("\npayload-size=");
  envelope.append(std::to_string(payload.size()));
  envelope.append("\npayload-digest=");
  envelope.append(payload_digest(payload));
  envelope.append("\n\n");
  envelope.append(payload);

  const auto target = entry_path(key, device_bound);
  static std::atomic<std::uint64_t> temp_counter{0};
  const auto token = std::to_string(static_cast<unsigned long long>(::getpid())) + "." +
                     std::to_string(static_cast<unsigned long long>(
                         temp_counter.fetch_add(1U, std::memory_order_relaxed)));
  const auto temporary = target.string() + ".tmp." + token;
  const int fd = ::open(temporary.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
  if (fd < 0) {
    return CacheStatus::io_error;
  }
  bool written = write_all(fd, envelope);
  if (written) {
    written = ::fsync(fd) == 0;
  }
  const int close_result = ::close(fd);
  written = written && close_result == 0;
  if (!written) {
    std::filesystem::remove(temporary, error);
    return CacheStatus::io_error;
  }
  if (::rename(temporary.c_str(), target.c_str()) != 0) {
    std::filesystem::remove(temporary, error);
    return CacheStatus::io_error;
  }
  return sync_directory(root_) ? CacheStatus::success : CacheStatus::io_error;
}

CacheStatus CacheFileStore::read_locked(std::string_view key, bool device_bound,
                                        std::string* out_payload) {
  if (out_payload == nullptr || !valid_key(key) || max_payload_bytes_ == 0U || root_.empty()) {
    return CacheStatus::invalid_argument;
  }
  out_payload->clear();
  const auto path = entry_path(key, device_bound);
  std::error_code error;
  if (!std::filesystem::exists(path, error)) {
    return error ? CacheStatus::io_error : CacheStatus::miss;
  }
  std::ifstream input(path, std::ios::binary | std::ios::ate);
  if (!input) {
    return CacheStatus::io_error;
  }
  const auto end = input.tellg();
  if (end <= 0) {
    std::filesystem::remove(path, error);
    return CacheStatus::corrupt;
  }
  if (max_payload_bytes_ > std::numeric_limits<std::size_t>::max() - kEnvelopeOverhead ||
      static_cast<std::uintmax_t>(end) >
          static_cast<std::uintmax_t>(max_payload_bytes_ + kEnvelopeOverhead)) {
    std::filesystem::remove(path, error);
    return CacheStatus::corrupt;
  }
  const auto byte_count = static_cast<std::size_t>(end);
  std::string encoded(byte_count, '\0');
  input.seekg(0);
  input.read(encoded.data(), static_cast<std::streamsize>(encoded.size()));
  if (!input) {
    std::filesystem::remove(path, error);
    return CacheStatus::corrupt;
  }

  const auto separator = encoded.find("\n\n");
  if (separator == std::string::npos) {
    std::filesystem::remove(path, error);
    return CacheStatus::corrupt;
  }
  const std::string_view header(encoded.data(), separator);
  std::size_t cursor = 0U;
  auto next_line = [&header, &cursor](std::string_view& line) {
    if (cursor > header.size()) {
      return false;
    }
    const auto end_line = header.find('\n', cursor);
    if (end_line == std::string_view::npos) {
      line = header.substr(cursor);
      cursor = header.size();
    } else {
      line = header.substr(cursor, end_line - cursor);
      cursor = end_line + 1U;
    }
    return true;
  };
  std::string_view magic;
  std::string_view key_line;
  std::string_view mode_line;
  std::string_view size_line;
  std::string_view digest_line;
  const bool parsed = next_line(magic) && next_line(key_line) && next_line(mode_line) &&
                      next_line(size_line) && next_line(digest_line) && cursor == header.size();
  const std::string expected_key = "key=" + std::string(key);
  const std::string_view expected_mode = device_bound ? "mode=device" : "mode=portable";
  if (!parsed || magic != kFileMagic || key_line != expected_key || mode_line != expected_mode ||
      size_line.substr(0U, 13U) != "payload-size=" ||
      digest_line.substr(0U, 15U) != "payload-digest=") {
    std::filesystem::remove(path, error);
    return CacheStatus::corrupt;
  }

  std::uint64_t declared_size = 0U;
  const auto size_value = size_line.substr(13U);
  const auto size_result =
      std::from_chars(size_value.data(), size_value.data() + size_value.size(), declared_size);
  const auto payload =
      std::string_view(encoded.data() + separator + 2U, encoded.size() - separator - 2U);
  if (size_result.ec != std::errc{} || size_result.ptr != size_value.data() + size_value.size() ||
      declared_size == 0U || declared_size > max_payload_bytes_ ||
      declared_size != payload.size() || digest_line.substr(15U) != payload_digest(payload)) {
    std::filesystem::remove(path, error);
    return CacheStatus::corrupt;
  }
  *out_payload = std::string(payload);
  return CacheStatus::hit;
}

CacheStatus CacheFileStore::publish(std::string_view key, std::string_view payload,
                                    bool device_bound) {
  std::lock_guard lock(mutex_);
  return write_atomic_locked(key, payload, device_bound);
}

CacheStatus CacheFileStore::lookup(std::string_view key, bool device_bound,
                                   std::string* out_payload) {
  std::lock_guard lock(mutex_);
  return read_locked(key, device_bound, out_payload);
}

CacheStatus CacheFileStore::invalidate_device(std::string_view key) {
  std::lock_guard lock(mutex_);
  std::string payload;
  const auto status = read_locked(key, true, &payload);
  if (status != CacheStatus::hit) {
    return status;
  }
  std::error_code error;
  return std::filesystem::remove(entry_path(key, true), error) && !error ? CacheStatus::success
                                                                         : CacheStatus::io_error;
}

CacheStatus PersistentCacheRepository::publish(std::string_view key, std::string_view payload,
                                               bool device_bound) {
  const std::string owned_key(key);
  const std::string owned_payload(payload);
  std::lock_guard lock(mutex_);
  const auto admission = catalog_.admit_publish(owned_key);
  if (admission != CacheStatus::success) {
    return admission;
  }
  const auto persisted = files_.publish(owned_key, owned_payload, device_bound);
  if (persisted != CacheStatus::success) {
    return persisted;
  }
  return catalog_.publish(owned_key, owned_payload, device_bound);
}

CacheStatus PersistentCacheRepository::lookup(std::string_view key, bool device_bound,
                                              std::string* out_payload) {
  if (out_payload == nullptr) {
    return CacheStatus::invalid_argument;
  }
  const std::string owned_key(key);
  std::lock_guard lock(mutex_);
  const auto resident = catalog_.lookup(owned_key, device_bound, out_payload);
  if (resident == CacheStatus::hit) {
    return resident;
  }
  if (resident != CacheStatus::miss && resident != CacheStatus::corrupt) {
    return resident;
  }

  std::string persisted_payload;
  const auto persisted = files_.lookup(owned_key, device_bound, &persisted_payload);
  if (persisted != CacheStatus::hit) {
    out_payload->clear();
    return persisted;
  }
  const auto admission = catalog_.admit_publish(owned_key);
  if (admission != CacheStatus::success) {
    out_payload->clear();
    return admission;
  }
  const auto hydrated = catalog_.publish(owned_key, persisted_payload, device_bound);
  if (hydrated != CacheStatus::success) {
    out_payload->clear();
    return hydrated;
  }
  *out_payload = std::move(persisted_payload);
  return CacheStatus::hit;
}

CacheStatus PersistentCacheRepository::lookup_or_publish(std::string_view key, bool device_bound,
                                                         const Producer& producer,
                                                         std::string* out_payload) {
  if (out_payload == nullptr || !producer) {
    return CacheStatus::invalid_argument;
  }
  out_payload->clear();
  auto status = lookup(key, device_bound, out_payload);
  if (status == CacheStatus::hit) {
    return status;
  }
  if (status != CacheStatus::miss && status != CacheStatus::corrupt) {
    return status;
  }

  const std::string owned_key(key);
  const auto lock_path = files_.entry_path(owned_key, device_bound).string() + ".lock";
  auto key_lock = acquire_cache_key_lock(lock_path, key_lock_timeout_);
  if (!key_lock.has_value()) {
    return CacheStatus::io_error;
  }

  status = lookup(key, device_bound, out_payload);
  if (status == CacheStatus::hit) {
    return status;
  }
  if (status == CacheStatus::corrupt) {
    status = CacheStatus::miss;
  }
  if (status != CacheStatus::miss) {
    return status;
  }

  const auto produced = producer();
  if (!produced.has_value()) {
    return CacheStatus::miss;
  }
  if (produced->empty()) {
    return CacheStatus::invalid_argument;
  }
  status = publish(key, *produced, device_bound);
  if (status != CacheStatus::success) {
    return status;
  }
  *out_payload = *produced;
  return CacheStatus::hit;
}

CacheStatus PersistentCacheRepository::acquire_pipeline(std::string_view key,
                                                        std::uint64_t generation) {
  if (!valid_key(key) || generation == 0U) {
    return CacheStatus::invalid_argument;
  }
  const std::string owned_key(key);
  std::lock_guard lock(mutex_);

  std::string payload;
  const auto resident = catalog_.lookup(owned_key, true, &payload);
  if (resident != CacheStatus::hit) {
    return resident;
  }

  const auto active = active_pipeline_bindings_.find(owned_key);
  if (active != active_pipeline_bindings_.end()) {
    return active->second == generation ? CacheStatus::pinned : CacheStatus::stale_generation;
  }
  const auto pinned = catalog_.pin(owned_key);
  if (pinned != CacheStatus::success) {
    return pinned;
  }
  active_pipeline_bindings_.emplace(owned_key, generation);
  return CacheStatus::success;
}

CacheStatus PersistentCacheRepository::release_pipeline(std::string_view key,
                                                        std::uint64_t generation) {
  if (!valid_key(key) || generation == 0U) {
    return CacheStatus::invalid_argument;
  }
  std::lock_guard lock(mutex_);
  const auto active = active_pipeline_bindings_.find(key);
  if (active == active_pipeline_bindings_.end()) {
    return CacheStatus::not_found;
  }
  if (active->second != generation) {
    return CacheStatus::stale_generation;
  }
  const auto unpinned = catalog_.unpin(key);
  if (unpinned != CacheStatus::success) {
    return unpinned;
  }
  active_pipeline_bindings_.erase(active);
  return CacheStatus::success;
}

CacheStatus PersistentCacheRepository::pin(std::string_view key) noexcept {
  const std::string owned_key(key);
  std::lock_guard lock(mutex_);
  return catalog_.pin(owned_key);
}

CacheStatus PersistentCacheRepository::unpin(std::string_view key) noexcept {
  const std::string owned_key(key);
  std::lock_guard lock(mutex_);
  if (active_pipeline_bindings_.find(owned_key) != active_pipeline_bindings_.end()) {
    return CacheStatus::pinned;
  }
  return catalog_.unpin(owned_key);
}

CacheStatus PersistentCacheRepository::invalidate_device(std::string_view key) {
  const std::string owned_key(key);
  std::lock_guard lock(mutex_);
  const auto admission = catalog_.admit_remove(owned_key, true);
  if (admission == CacheStatus::pinned || admission == CacheStatus::invalid_argument) {
    return admission;
  }
  if (admission == CacheStatus::not_found) {
    return files_.invalidate_device(owned_key);
  }
  const auto persisted = files_.invalidate_device(owned_key);
  if (persisted != CacheStatus::success && persisted != CacheStatus::miss) {
    return persisted;
  }
  return catalog_.remove(owned_key, true);
}

std::size_t PersistentCacheRepository::size() const noexcept {
  std::lock_guard lock(mutex_);
  return catalog_.size();
}

std::size_t PersistentCacheRepository::max_entries() const noexcept {
  std::lock_guard lock(mutex_);
  return catalog_.max_entries();
}

} // namespace metaflux::backend::vulkan
