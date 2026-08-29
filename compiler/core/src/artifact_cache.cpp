#include "metaflux/compiler/artifact_cache.hpp"

#include "metaflux/compiler/cache.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <ctime>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <limits>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <unistd.h>
#include <unordered_map>
#include <utility>
#include <vector>

namespace metaflux::compiler {
namespace {

constexpr std::uint32_t kMetadataFormatVersion = 1;
constexpr std::size_t kMaximumMetadataBytes = 16U * 1024U;
constexpr std::size_t kMaximumReservationBytes = 1024U;

class UniqueFileDescriptor {
public:
  UniqueFileDescriptor() = default;
  explicit UniqueFileDescriptor(int descriptor) : descriptor_(descriptor) {}
  ~UniqueFileDescriptor() { reset(); }
  UniqueFileDescriptor(const UniqueFileDescriptor&) = delete;
  UniqueFileDescriptor& operator=(const UniqueFileDescriptor&) = delete;
  UniqueFileDescriptor(UniqueFileDescriptor&& other) noexcept
      : descriptor_(std::exchange(other.descriptor_, -1)) {}
  UniqueFileDescriptor& operator=(UniqueFileDescriptor&& other) noexcept {
    if (this != &other) {
      reset();
      descriptor_ = std::exchange(other.descriptor_, -1);
    }
    return *this;
  }

  [[nodiscard]] int get() const noexcept { return descriptor_; }
  [[nodiscard]] explicit operator bool() const noexcept { return descriptor_ >= 0; }
  [[nodiscard]] int release() noexcept { return std::exchange(descriptor_, -1); }
  void reset() noexcept {
    if (descriptor_ >= 0) {
      static_cast<void>(close(descriptor_));
      descriptor_ = -1;
    }
  }

private:
  int descriptor_ = -1;
};

struct ReservationRecord {
  std::uint32_t uid;
  std::uint64_t bytes;
  std::string cache_key;
  std::filesystem::path state_path;
  UniqueFileDescriptor state_lock;
  UniqueFileDescriptor key_lock;
};

struct ReservationSnapshot {
  std::uint32_t uid;
  std::uint64_t bytes;
  std::string cache_key;
};

struct Candidate {
  std::filesystem::path directory;
  std::uint32_t uid;
  PersistentArtifactMetadata metadata;
};

std::uint64_t default_clock() {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  return static_cast<std::uint64_t>(
      std::chrono::duration_cast<std::chrono::nanoseconds>(now).count());
}

bool valid_cache_key(std::string_view key) {
  constexpr std::string_view kPrefix = "mf-cache-v1-";
  if (key.size() != kPrefix.size() + 64U || !key.starts_with(kPrefix)) {
    return false;
  }
  return std::all_of(
      key.begin() + static_cast<std::ptrdiff_t>(kPrefix.size()), key.end(), [](char character) {
        return (character >= '0' && character <= '9') || (character >= 'a' && character <= 'f');
      });
}

bool valid_payload(std::string_view payload) {
  return payload.size() <= 4096U &&
         std::none_of(payload.begin(), payload.end(), [](char character) {
           return character == '\n' || character == '\r' || character == '\0';
         });
}

std::filesystem::path epoch_directory(const std::filesystem::path& root, std::uint32_t epoch) {
  return root / ("epoch-" + std::to_string(epoch));
}

std::filesystem::path user_epoch_directory(const PersistentCacheConfig& config, std::uint32_t uid) {
  return epoch_directory(config.mutable_root / "users" / std::to_string(uid),
                         config.compiler_epoch);
}

std::filesystem::path entry_directory(const std::filesystem::path& epoch_root,
                                      std::string_view cache_key) {
  const auto digest = cache_key.substr(cache_key.size() - 64U);
  return epoch_root / std::string(digest.substr(0U, 2U)) / std::string(digest);
}

std::filesystem::path cache_state_directory(const PersistentCacheConfig& config) {
  return config.mutable_root / ".state";
}

std::filesystem::path reservation_directory(const PersistentCacheConfig& config) {
  return cache_state_directory(config) / "reservations";
}

std::filesystem::path key_lock_directory(const PersistentCacheConfig& config, std::uint32_t uid) {
  return cache_state_directory(config) / "key-locks" / std::to_string(uid);
}

bool ensure_directory(const std::filesystem::path& path, mode_t mode) {
  std::error_code error;
  std::filesystem::create_directories(path, error);
  if (error || chmod(path.c_str(), mode) != 0) {
    return false;
  }
  return true;
}

bool ensure_private_directory(const std::filesystem::path& path) {
  std::error_code error;
  std::filesystem::create_directories(path, error);
  if (error) {
    return false;
  }
  struct stat status{};
  if (lstat(path.c_str(), &status) != 0 || !S_ISDIR(status.st_mode) || status.st_uid != geteuid()) {
    return false;
  }
  return (status.st_mode & 0777U) == 0700U || chmod(path.c_str(), 0700) == 0;
}

bool private_regular_file(int descriptor) {
  struct stat status{};
  return fstat(descriptor, &status) == 0 && S_ISREG(status.st_mode) && status.st_uid == geteuid() &&
         status.st_nlink == 1U;
}

std::optional<UniqueFileDescriptor> open_private_file(const std::filesystem::path& path,
                                                      bool create, bool exclusive_create) {
  int flags = O_RDWR | O_CLOEXEC | O_NOFOLLOW;
  if (create) {
    flags |= O_CREAT;
  }
  if (exclusive_create) {
    flags |= O_EXCL;
  }
  UniqueFileDescriptor descriptor(open(path.c_str(), flags, 0600));
  if (!descriptor || !private_regular_file(descriptor.get()) ||
      fchmod(descriptor.get(), 0600) != 0) {
    return std::nullopt;
  }
  return descriptor;
}

bool lock_file(int descriptor, int operation) {
  while (flock(descriptor, operation) != 0) {
    if (errno != EINTR) {
      return false;
    }
  }
  return true;
}

std::optional<UniqueFileDescriptor> acquire_global_lock(const PersistentCacheConfig& config) {
  const auto state = cache_state_directory(config);
  if (!ensure_private_directory(state)) {
    return std::nullopt;
  }
  auto descriptor = open_private_file(state / "cache.lock", true, false);
  if (!descriptor.has_value() || !lock_file(descriptor->get(), LOCK_EX)) {
    return std::nullopt;
  }
  return descriptor;
}

enum class KeyLockState : std::uint32_t {
  Acquired,
  TimedOut,
  Error,
};

struct KeyLockAttempt {
  KeyLockState state = KeyLockState::Error;
  UniqueFileDescriptor descriptor;
};

bool at_or_after(const timespec& value, const timespec& deadline) {
  return value.tv_sec > deadline.tv_sec ||
         (value.tv_sec == deadline.tv_sec && value.tv_nsec >= deadline.tv_nsec);
}

timespec add_duration(timespec value, std::chrono::nanoseconds duration) {
  constexpr std::int64_t kNanosecondsPerSecond = 1'000'000'000;
  const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration);
  const auto nanoseconds = duration - seconds;
  const auto maximum_seconds = std::numeric_limits<time_t>::max() - value.tv_sec;
  if (seconds.count() > maximum_seconds) {
    return {.tv_sec = std::numeric_limits<time_t>::max(), .tv_nsec = 999'999'999};
  }
  value.tv_sec += static_cast<time_t>(seconds.count());
  value.tv_nsec += static_cast<long>(nanoseconds.count());
  if (value.tv_nsec >= kNanosecondsPerSecond) {
    if (value.tv_sec == std::numeric_limits<time_t>::max()) {
      return {.tv_sec = std::numeric_limits<time_t>::max(), .tv_nsec = 999'999'999};
    }
    ++value.tv_sec;
    value.tv_nsec -= kNanosecondsPerSecond;
  }
  return value;
}

std::optional<timespec>
key_lock_deadline(const PersistentCacheConfig& config,
                  std::optional<std::chrono::steady_clock::time_point> request_deadline) {
  auto wait =
      std::max(std::chrono::nanoseconds::zero(),
               std::chrono::duration_cast<std::chrono::nanoseconds>(config.key_lock_timeout));
  if (request_deadline.has_value()) {
    wait = std::min(wait, std::max(std::chrono::nanoseconds::zero(),
                                   std::chrono::duration_cast<std::chrono::nanoseconds>(
                                       *request_deadline - std::chrono::steady_clock::now())));
  }
  timespec now{};
  if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
    return std::nullopt;
  }
  return add_duration(now, wait);
}

KeyLockAttempt
acquire_key_lock(const PersistentCacheConfig& config, std::uint32_t uid, std::string_view cache_key,
                 std::optional<std::chrono::steady_clock::time_point> request_deadline) {
  const auto state = cache_state_directory(config);
  const auto lock_root = state / "key-locks";
  const auto uid_root = key_lock_directory(config, uid);
  if (!ensure_private_directory(state) || !ensure_private_directory(lock_root) ||
      !ensure_private_directory(uid_root)) {
    return {};
  }
  const auto digest = cache_key.substr(cache_key.size() - 64U);
  auto descriptor = open_private_file(uid_root / (std::string(digest) + ".lock"), true, false);
  const auto deadline = key_lock_deadline(config, request_deadline);
  if (!descriptor.has_value() || !deadline.has_value()) {
    return {};
  }
  constexpr auto kRetryInterval = std::chrono::milliseconds(1);
  bool first_attempt = true;
  for (;;) {
    if (!first_attempt) {
      timespec before_attempt{};
      if (clock_gettime(CLOCK_MONOTONIC, &before_attempt) != 0) {
        return {};
      }
      if (at_or_after(before_attempt, *deadline)) {
        return {.state = KeyLockState::TimedOut, .descriptor = {}};
      }
    }
    first_attempt = false;
    if (flock(descriptor->get(), LOCK_EX | LOCK_NB) == 0) {
      return {.state = KeyLockState::Acquired, .descriptor = std::move(*descriptor)};
    }
    if (errno == EINTR) {
      continue;
    }
    if (errno != EWOULDBLOCK && errno != EAGAIN) {
      return {};
    }
    timespec now{};
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
      return {};
    }
    if (at_or_after(now, *deadline)) {
      return {.state = KeyLockState::TimedOut, .descriptor = {}};
    }
    auto retry = add_duration(now, kRetryInterval);
    if (at_or_after(retry, *deadline)) {
      retry = *deadline;
    }
    int sleep_error = 0;
    do {
      sleep_error = clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &retry, nullptr);
      if (sleep_error == EINTR) {
        timespec interrupted{};
        if (clock_gettime(CLOCK_MONOTONIC, &interrupted) != 0) {
          return {};
        }
        if (at_or_after(interrupted, *deadline)) {
          return {.state = KeyLockState::TimedOut, .descriptor = {}};
        }
      }
    } while (sleep_error == EINTR);
    if (sleep_error != 0) {
      return {};
    }
  }
}

bool fsync_directory(const std::filesystem::path& path) {
  const int descriptor = open(path.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
  if (descriptor < 0) {
    return false;
  }
  const bool success = fsync(descriptor) == 0;
  static_cast<void>(close(descriptor));
  return success;
}

std::uint64_t saturated_add(std::uint64_t left, std::uint64_t right) {
  if (right > std::numeric_limits<std::uint64_t>::max() - left) {
    return std::numeric_limits<std::uint64_t>::max();
  }
  return left + right;
}

std::uint64_t percentage_of(std::uint64_t value, std::uint32_t percent) {
  const auto whole = value / 100U;
  const auto remainder = value % 100U;
  if (percent != 0U && whole > std::numeric_limits<std::uint64_t>::max() / percent) {
    return std::numeric_limits<std::uint64_t>::max();
  }
  return saturated_add(whole * percent, remainder * percent / 100U);
}

std::optional<CacheFilesystemSpace> query_filesystem_space(const PersistentCacheConfig& config) {
  if (config.filesystem_space) {
    return config.filesystem_space();
  }
  struct statvfs filesystem{};
  if (statvfs(config.mutable_root.c_str(), &filesystem) != 0 || filesystem.f_frsize == 0U ||
      static_cast<std::uint64_t>(filesystem.f_blocks) >
          std::numeric_limits<std::uint64_t>::max() /
              static_cast<std::uint64_t>(filesystem.f_frsize) ||
      static_cast<std::uint64_t>(filesystem.f_bavail) >
          std::numeric_limits<std::uint64_t>::max() /
              static_cast<std::uint64_t>(filesystem.f_frsize)) {
    return std::nullopt;
  }
  return CacheFilesystemSpace{
      .total_bytes = static_cast<std::uint64_t>(filesystem.f_blocks) * filesystem.f_frsize,
      .available_bytes = static_cast<std::uint64_t>(filesystem.f_bavail) * filesystem.f_frsize,
  };
}

bool write_all(int descriptor, std::span<const std::byte> bytes) {
  std::size_t offset = 0;
  while (offset < bytes.size()) {
    const auto written = write(descriptor, bytes.data() + offset, bytes.size() - offset);
    if (written < 0) {
      if (errno == EINTR) {
        continue;
      }
      return false;
    }
    if (written == 0) {
      return false;
    }
    offset += static_cast<std::size_t>(written);
  }
  return true;
}

std::optional<std::vector<std::byte>> read_binary_descriptor(int descriptor,
                                                             std::uint64_t maximum_size) {
  struct stat status{};
  if (fstat(descriptor, &status) != 0 || !S_ISREG(status.st_mode) || status.st_size < 0 ||
      static_cast<std::uint64_t>(status.st_size) > maximum_size) {
    return std::nullopt;
  }
  std::vector<std::byte> bytes(static_cast<std::size_t>(status.st_size));
  std::size_t offset = 0;
  while (offset < bytes.size()) {
    const auto count =
        pread(descriptor, bytes.data() + offset, bytes.size() - offset, static_cast<off_t>(offset));
    if (count < 0) {
      if (errno == EINTR) {
        continue;
      }
      return std::nullopt;
    }
    if (count == 0) {
      return std::nullopt;
    }
    offset += static_cast<std::size_t>(count);
  }
  return bytes;
}

bool write_synced_file(const std::filesystem::path& path, std::span<const std::byte> bytes,
                       mode_t mode) {
  const int descriptor =
      open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW, mode);
  if (descriptor < 0) {
    return false;
  }
  const bool success = write_all(descriptor, bytes) && fsync(descriptor) == 0;
  static_cast<void>(close(descriptor));
  return success;
}

bool write_synced_text(const std::filesystem::path& path, std::string_view text, mode_t mode) {
  return write_synced_file(path, std::as_bytes(std::span{text.data(), text.size()}), mode);
}

std::optional<std::vector<std::byte>> read_binary(const std::filesystem::path& path,
                                                  std::uint64_t maximum_size) {
  UniqueFileDescriptor descriptor(open(path.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW));
  return descriptor ? read_binary_descriptor(descriptor.get(), maximum_size) : std::nullopt;
}

std::optional<std::string> read_text(const std::filesystem::path& path) {
  const auto bytes = read_binary(path, kMaximumMetadataBytes);
  if (!bytes.has_value()) {
    return std::nullopt;
  }
  return std::string(reinterpret_cast<const char*>(bytes->data()), bytes->size());
}

std::string serialize_metadata(const PersistentArtifactMetadata& metadata) {
  std::string text;
  text.reserve(512U + metadata.payload.size());
  text += "format=" + std::to_string(metadata.format_version) + '\n';
  text += "compiler_epoch=" + std::to_string(metadata.compiler_epoch) + '\n';
  text += "kernel_ir_schema=" + std::to_string(metadata.kernel_ir_schema) + '\n';
  text += "helper_abi=" + std::to_string(metadata.helper_abi) + '\n';
  text += "artifact_size=" + std::to_string(metadata.artifact_size) + '\n';
  text += "last_used=" + std::to_string(metadata.last_used) + '\n';
  text += "cache_key=" + metadata.cache_key + '\n';
  text += "artifact_sha256=" + metadata.artifact_sha256 + '\n';
  text += "payload=" + metadata.payload + '\n';
  return text;
}

template <typename Integer> std::optional<Integer> parse_integer(std::string_view text) {
  Integer value{};
  const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
  if (error != std::errc{} || end != text.data() + text.size()) {
    return std::nullopt;
  }
  return value;
}

std::optional<PersistentArtifactMetadata> parse_metadata(std::string_view text) {
  std::unordered_map<std::string, std::string> fields;
  std::size_t offset = 0;
  while (offset < text.size()) {
    const auto newline = text.find('\n', offset);
    if (newline == std::string_view::npos || newline == offset) {
      return std::nullopt;
    }
    const auto line = text.substr(offset, newline - offset);
    const auto equals = line.find('=');
    if (equals == std::string_view::npos || equals == 0U) {
      return std::nullopt;
    }
    const auto [iterator, inserted] =
        fields.emplace(std::string(line.substr(0U, equals)), std::string(line.substr(equals + 1U)));
    static_cast<void>(iterator);
    if (!inserted) {
      return std::nullopt;
    }
    offset = newline + 1U;
  }
  constexpr std::array<std::string_view, 9> kRequired{
      "format",    "compiler_epoch", "kernel_ir_schema", "helper_abi", "artifact_size",
      "last_used", "cache_key",      "artifact_sha256",  "payload",
  };
  if (fields.size() != kRequired.size() ||
      std::any_of(kRequired.begin(), kRequired.end(),
                  [&](std::string_view key) { return !fields.contains(std::string(key)); })) {
    return std::nullopt;
  }
  const auto format = parse_integer<std::uint32_t>(fields["format"]);
  const auto epoch = parse_integer<std::uint32_t>(fields["compiler_epoch"]);
  const auto schema = parse_integer<std::uint32_t>(fields["kernel_ir_schema"]);
  const auto helper = parse_integer<std::uint32_t>(fields["helper_abi"]);
  const auto size = parse_integer<std::uint64_t>(fields["artifact_size"]);
  const auto last_used = parse_integer<std::uint64_t>(fields["last_used"]);
  if (!format.has_value() || !epoch.has_value() || !schema.has_value() || !helper.has_value() ||
      !size.has_value() || !last_used.has_value() || !valid_cache_key(fields["cache_key"]) ||
      fields["artifact_sha256"].size() != 64U || !valid_payload(fields["payload"])) {
    return std::nullopt;
  }
  return PersistentArtifactMetadata{
      .format_version = *format,
      .compiler_epoch = *epoch,
      .kernel_ir_schema = *schema,
      .helper_abi = *helper,
      .artifact_size = *size,
      .last_used = *last_used,
      .cache_key = std::move(fields["cache_key"]),
      .artifact_sha256 = std::move(fields["artifact_sha256"]),
      .payload = std::move(fields["payload"]),
  };
}

std::string serialize_reservation(const ReservationSnapshot& reservation) {
  return "uid=" + std::to_string(reservation.uid) + '\n' +
         "bytes=" + std::to_string(reservation.bytes) + '\n' +
         "cache_key=" + reservation.cache_key + '\n';
}

std::optional<ReservationSnapshot> parse_reservation(std::string_view text) {
  std::unordered_map<std::string, std::string> fields;
  std::size_t offset = 0;
  while (offset < text.size()) {
    const auto newline = text.find('\n', offset);
    if (newline == std::string_view::npos || newline == offset) {
      return std::nullopt;
    }
    const auto line = text.substr(offset, newline - offset);
    const auto equals = line.find('=');
    if (equals == std::string_view::npos || equals == 0U) {
      return std::nullopt;
    }
    const auto [iterator, inserted] =
        fields.emplace(std::string(line.substr(0U, equals)), std::string(line.substr(equals + 1U)));
    static_cast<void>(iterator);
    if (!inserted) {
      return std::nullopt;
    }
    offset = newline + 1U;
  }
  if (fields.size() != 3U || !fields.contains("uid") || !fields.contains("bytes") ||
      !fields.contains("cache_key")) {
    return std::nullopt;
  }
  const auto uid = parse_integer<std::uint32_t>(fields["uid"]);
  const auto bytes = parse_integer<std::uint64_t>(fields["bytes"]);
  if (!uid.has_value() || !bytes.has_value() || *bytes == 0U ||
      !valid_cache_key(fields["cache_key"])) {
    return std::nullopt;
  }
  return ReservationSnapshot{
      .uid = *uid, .bytes = *bytes, .cache_key = std::move(fields["cache_key"])};
}

bool metadata_matches(const PersistentArtifactMetadata& metadata, std::string_view cache_key,
                      std::uint32_t epoch, std::span<const std::byte> artifact) {
  return metadata.format_version == kMetadataFormatVersion && metadata.compiler_epoch == epoch &&
         metadata.cache_key == cache_key && metadata.artifact_size == artifact.size() &&
         metadata.artifact_sha256 == sha256_hex(artifact);
}

bool remove_directory(const std::filesystem::path& directory) {
  std::error_code ignored;
  std::filesystem::permissions(directory, std::filesystem::perms::owner_all,
                               std::filesystem::perm_options::add, ignored);
  ignored.clear();
  std::filesystem::remove_all(directory, ignored);
  return !ignored && fsync_directory(directory.parent_path());
}

enum class EntryLockState : std::uint32_t {
  Acquired,
  Busy,
  Missing,
  Error,
};

struct EntryLockAttempt {
  EntryLockState state = EntryLockState::Error;
  UniqueFileDescriptor descriptor;
};

EntryLockAttempt try_lock_entry_exclusive(const std::filesystem::path& directory) {
  UniqueFileDescriptor descriptor(
      open((directory / "kernel.so").c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW));
  if (!descriptor) {
    return {.state = errno == ENOENT ? EntryLockState::Missing : EntryLockState::Error,
            .descriptor = {}};
  }
  if (!private_regular_file(descriptor.get())) {
    return {.state = EntryLockState::Error, .descriptor = {}};
  }
  while (flock(descriptor.get(), LOCK_EX | LOCK_NB) != 0) {
    if (errno == EINTR) {
      continue;
    }
    return {.state = errno == EWOULDBLOCK || errno == EAGAIN ? EntryLockState::Busy
                                                             : EntryLockState::Error,
            .descriptor = {}};
  }
  return {.state = EntryLockState::Acquired, .descriptor = std::move(descriptor)};
}

enum class RemovalState : std::uint32_t {
  Removed,
  Busy,
  Error,
};

RemovalState remove_unpinned_directory(const std::filesystem::path& directory) {
  auto lock = try_lock_entry_exclusive(directory);
  if (lock.state == EntryLockState::Busy) {
    return RemovalState::Busy;
  }
  if (lock.state == EntryLockState::Error) {
    return RemovalState::Error;
  }
  return remove_directory(directory) ? RemovalState::Removed : RemovalState::Error;
}

std::optional<UniqueFileDescriptor>
acquire_shared_entry_pin(const std::filesystem::path& artifact_path) {
  UniqueFileDescriptor descriptor(open(artifact_path.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW));
  if (!descriptor || !private_regular_file(descriptor.get()) ||
      !lock_file(descriptor.get(), LOCK_SH)) {
    return std::nullopt;
  }
  return descriptor;
}

bool fault(const PersistentCacheConfig& config, CacheFaultPoint point) {
  return config.inject_fault && config.inject_fault(point);
}

std::optional<std::uint32_t> parse_uid(std::string_view text) {
  return parse_integer<std::uint32_t>(text);
}

} // namespace

struct PersistentArtifactCache::State {
  explicit State(PersistentCacheConfig initial_config) : config(std::move(initial_config)) {
    if (!config.clock) {
      config.clock = default_clock;
    }
  }

  PersistentCacheConfig config;
  std::mutex mutex;
  std::uint64_t next_token = 1U;
  std::unordered_map<std::uint64_t, ReservationRecord> reservations;
};

namespace {

std::optional<std::vector<Candidate>>
scan_candidates(const std::shared_ptr<PersistentArtifactCache::State>& state, bool remove_invalid) {
  std::vector<Candidate> candidates;
  if (fault(state->config, CacheFaultPoint::BeforeCandidateScan)) {
    return std::nullopt;
  }
  const auto users = state->config.mutable_root / "users";
  std::error_code error;
  if (!std::filesystem::exists(users, error)) {
    if (error) {
      return std::nullopt;
    }
    return candidates;
  }
  std::filesystem::directory_iterator uid_iterator(users, error), end;
  if (error) {
    return std::nullopt;
  }
  for (; uid_iterator != end; uid_iterator.increment(error)) {
    if (!uid_iterator->is_directory(error)) {
      if (error) {
        return std::nullopt;
      }
      continue;
    }
    const auto uid = parse_uid(uid_iterator->path().filename().string());
    if (!uid.has_value()) {
      continue;
    }
    const auto epoch = epoch_directory(uid_iterator->path(), state->config.compiler_epoch);
    if (!std::filesystem::exists(epoch, error)) {
      if (error) {
        return std::nullopt;
      }
      continue;
    }
    std::filesystem::directory_iterator prefix_iterator(epoch, error), prefix_end;
    if (error) {
      return std::nullopt;
    }
    for (; prefix_iterator != prefix_end; prefix_iterator.increment(error)) {
      if (!prefix_iterator->is_directory(error)) {
        if (error) {
          return std::nullopt;
        }
        continue;
      }
      std::filesystem::directory_iterator entry_iterator(prefix_iterator->path(), error), entry_end;
      if (error) {
        return std::nullopt;
      }
      for (; entry_iterator != entry_end; entry_iterator.increment(error)) {
        if (!entry_iterator->is_directory(error)) {
          if (error) {
            return std::nullopt;
          }
          continue;
        }
        const auto filename = entry_iterator->path().filename().string();
        if (filename.starts_with(".tmp-")) {
          if (remove_invalid && !remove_directory(entry_iterator->path())) {
            return std::nullopt;
          }
          continue;
        }
        if (remove_invalid) {
          bool metadata_removed = false;
          std::filesystem::directory_iterator child_iterator(entry_iterator->path(), error),
              child_end;
          if (error) {
            return std::nullopt;
          }
          for (; child_iterator != child_end; child_iterator.increment(error)) {
            if (child_iterator->path().filename().string().starts_with(".metadata.tmp-")) {
              std::filesystem::remove(child_iterator->path(), error);
              if (error) {
                return std::nullopt;
              }
              metadata_removed = true;
            }
          }
          if (error || (metadata_removed && !fsync_directory(entry_iterator->path()))) {
            return std::nullopt;
          }
        }
        const auto text = read_text(entry_iterator->path() / "metadata.v1");
        const auto metadata = text.has_value() ? parse_metadata(*text) : std::nullopt;
        const auto digest = metadata.has_value()
                                ? metadata->cache_key.substr(metadata->cache_key.size() - 64U)
                                : std::string{};
        if (!metadata.has_value() || metadata->compiler_epoch != state->config.compiler_epoch ||
            digest != filename || prefix_iterator->path().filename() != digest.substr(0U, 2U)) {
          if (remove_invalid) {
            const auto removed = remove_unpinned_directory(entry_iterator->path());
            if (removed == RemovalState::Error) {
              return std::nullopt;
            }
          }
          continue;
        }
        candidates.push_back(Candidate{
            .directory = entry_iterator->path(),
            .uid = *uid,
            .metadata = *metadata,
        });
      }
      if (error) {
        return std::nullopt;
      }
    }
    if (error) {
      return std::nullopt;
    }
  }
  if (error) {
    return std::nullopt;
  }
  return candidates;
}

std::optional<std::vector<ReservationSnapshot>>
scan_reservations(const PersistentCacheConfig& config) {
  const auto directory = reservation_directory(config);
  if (!ensure_private_directory(directory)) {
    return std::nullopt;
  }
  std::vector<ReservationSnapshot> reservations;
  bool removed_stale = false;
  std::error_code error;
  std::filesystem::directory_iterator iterator(directory, error), end;
  if (error) {
    return std::nullopt;
  }
  for (; iterator != end; iterator.increment(error)) {
    if (error) {
      return std::nullopt;
    }
    auto descriptor = open_private_file(iterator->path(), false, false);
    if (!descriptor.has_value()) {
      return std::nullopt;
    }
    bool active = false;
    // The owner holds an exclusive flock for the reservation lifetime. Acquiring it here proves
    // that the owner exited before cancel or publish and makes the record reclaimable.
    while (flock(descriptor->get(), LOCK_EX | LOCK_NB) != 0) {
      if (errno == EINTR) {
        continue;
      }
      if (errno != EWOULDBLOCK && errno != EAGAIN) {
        return std::nullopt;
      }
      active = true;
      break;
    }
    if (!active) {
      std::filesystem::remove(iterator->path(), error);
      if (error) {
        return std::nullopt;
      }
      removed_stale = true;
      continue;
    }
    const auto record_bytes = read_binary_descriptor(descriptor->get(), kMaximumReservationBytes);
    const auto record =
        record_bytes.has_value()
            ? parse_reservation(std::string_view(
                  reinterpret_cast<const char*>(record_bytes->data()), record_bytes->size()))
            : std::nullopt;
    if (!record.has_value() || record->bytes > config.limits.maximum_entry_bytes) {
      return std::nullopt;
    }
    reservations.push_back(*record);
  }
  if (error || (removed_stale && !fsync_directory(directory))) {
    return std::nullopt;
  }
  return reservations;
}

std::uint64_t reservation_bytes(const std::vector<ReservationSnapshot>& reservations,
                                std::optional<std::uint32_t> uid = std::nullopt) {
  std::uint64_t total = 0;
  for (const auto& reservation : reservations) {
    if (!uid.has_value() || reservation.uid == *uid) {
      total = saturated_add(total, reservation.bytes);
    }
  }
  return total;
}

bool evict_one(std::vector<Candidate>& candidates, std::optional<std::uint32_t> uid,
               std::uint64_t& reclaimed) {
  std::sort(candidates.begin(), candidates.end(),
            [](const Candidate& left, const Candidate& right) {
              if (left.metadata.last_used != right.metadata.last_used) {
                return left.metadata.last_used < right.metadata.last_used;
              }
              return left.metadata.cache_key < right.metadata.cache_key;
            });
  for (auto found = candidates.begin(); found != candidates.end(); ++found) {
    if (uid.has_value() && found->uid != *uid) {
      continue;
    }
    auto entry_lock = try_lock_entry_exclusive(found->directory);
    if (entry_lock.state == EntryLockState::Busy) {
      continue;
    }
    if (entry_lock.state == EntryLockState::Error || !remove_directory(found->directory)) {
      return false;
    }
    reclaimed = saturated_add(reclaimed, found->metadata.artifact_size);
    candidates.erase(found);
    return true;
  }
  return false;
}

std::uint64_t usage(const std::vector<Candidate>& candidates,
                    std::optional<std::uint32_t> uid = std::nullopt) {
  std::uint64_t total = 0;
  for (const auto& candidate : candidates) {
    if (!uid.has_value() || candidate.uid == *uid) {
      total = saturated_add(total, candidate.metadata.artifact_size);
    }
  }
  return total;
}

enum class CommittedEntryState : std::uint32_t {
  Absent,
  Available,
  PinnedInvalid,
  Error,
};

CommittedEntryState inspect_mutable_entry(const PersistentCacheConfig& config, std::uint32_t uid,
                                          std::string_view cache_key) {
  const auto directory = entry_directory(user_epoch_directory(config, uid), cache_key);
  std::error_code error;
  if (!std::filesystem::exists(directory, error)) {
    return error ? CommittedEntryState::Error : CommittedEntryState::Absent;
  }
  const auto metadata_text = read_text(directory / "metadata.v1");
  const auto metadata = metadata_text.has_value() ? parse_metadata(*metadata_text) : std::nullopt;
  const auto artifact = read_binary(directory / "kernel.so", config.limits.maximum_entry_bytes);
  if (metadata.has_value() && artifact.has_value() &&
      metadata_matches(*metadata, cache_key, config.compiler_epoch, *artifact)) {
    return CommittedEntryState::Available;
  }
  const auto removed = remove_unpinned_directory(directory);
  if (removed == RemovalState::Removed) {
    return CommittedEntryState::Absent;
  }
  return removed == RemovalState::Busy ? CommittedEntryState::PinnedInvalid
                                       : CommittedEntryState::Error;
}

std::optional<ReservationRecord> create_reservation_record(const PersistentCacheConfig& config,
                                                           std::uint64_t token, std::uint32_t uid,
                                                           std::uint64_t bytes,
                                                           std::string_view cache_key,
                                                           UniqueFileDescriptor key_lock) {
  const auto directory = reservation_directory(config);
  if (!ensure_private_directory(directory)) {
    return std::nullopt;
  }
  const auto digest = cache_key.substr(cache_key.size() - 64U);
  const auto path =
      directory / ("reservation-" + std::to_string(getpid()) + "-" + std::to_string(uid) + "-" +
                   std::string(digest) + "-" + std::to_string(token));
  auto state_lock = open_private_file(path, true, true);
  if (!state_lock.has_value() || !lock_file(state_lock->get(), LOCK_EX)) {
    return std::nullopt;
  }
  const ReservationSnapshot snapshot{
      .uid = uid,
      .bytes = bytes,
      .cache_key = std::string(cache_key),
  };
  const auto serialized = serialize_reservation(snapshot);
  if (!write_all(state_lock->get(),
                 std::as_bytes(std::span{serialized.data(), serialized.size()})) ||
      fsync(state_lock->get()) != 0 || !fsync_directory(directory)) {
    std::error_code ignored;
    std::filesystem::remove(path, ignored);
    return std::nullopt;
  }
  return ReservationRecord{
      .uid = uid,
      .bytes = bytes,
      .cache_key = std::string(cache_key),
      .state_path = path,
      .state_lock = std::move(*state_lock),
      .key_lock = std::move(key_lock),
  };
}

bool remove_reservation_record(const ReservationRecord& reservation) {
  std::error_code error;
  const bool removed = std::filesystem::remove(reservation.state_path, error);
  return !error && (!removed || fsync_directory(reservation.state_path.parent_path()));
}

PersistentCacheError publish_entry(PersistentArtifactCache::State& state,
                                   const std::filesystem::path& epoch_root,
                                   std::string_view cache_key, std::span<const std::byte> artifact,
                                   const ArtifactDescriptor& descriptor, std::uint64_t token,
                                   bool read_only) {
  if (!valid_cache_key(cache_key) || !valid_payload(descriptor.payload)) {
    return PersistentCacheError::InvalidKey;
  }
  const auto final_directory = entry_directory(epoch_root, cache_key);
  const auto parent = final_directory.parent_path();
  if (!ensure_directory(parent, read_only ? 0755 : 0700)) {
    return PersistentCacheError::Io;
  }
  const auto temporary = parent / (".tmp-" + std::string(cache_key) + "-" +
                                   std::to_string(getpid()) + "-" + std::to_string(token));
  std::error_code error;
  std::filesystem::remove_all(temporary, error);
  error.clear();
  if (!std::filesystem::create_directory(temporary, error) || error ||
      chmod(temporary.c_str(), 0700) != 0) {
    return PersistentCacheError::Io;
  }

  const PersistentArtifactMetadata metadata{
      .format_version = kMetadataFormatVersion,
      .compiler_epoch = state.config.compiler_epoch,
      .kernel_ir_schema = descriptor.kernel_ir_schema,
      .helper_abi = descriptor.helper_abi,
      .artifact_size = artifact.size(),
      .last_used = state.config.clock(),
      .cache_key = std::string(cache_key),
      .artifact_sha256 = sha256_hex(artifact),
      .payload = descriptor.payload,
  };
  if (!write_synced_file(temporary / "kernel.so", artifact, 0600)) {
    remove_directory(temporary);
    return PersistentCacheError::Io;
  }
  if (fault(state.config, CacheFaultPoint::AfterArtifactFsync)) {
    remove_directory(temporary);
    return PersistentCacheError::Io;
  }
  if (!write_synced_text(temporary / "metadata.v1", serialize_metadata(metadata), 0600)) {
    remove_directory(temporary);
    return PersistentCacheError::Io;
  }
  if (read_only && (chmod((temporary / "kernel.so").c_str(), 0444) != 0 ||
                    chmod((temporary / "metadata.v1").c_str(), 0444) != 0 ||
                    chmod(temporary.c_str(), 0555) != 0)) {
    remove_directory(temporary);
    return PersistentCacheError::Io;
  }
  if (fault(state.config, CacheFaultPoint::AfterMetadataFsync) || !fsync_directory(temporary) ||
      fault(state.config, CacheFaultPoint::BeforeRename)) {
    remove_directory(temporary);
    return PersistentCacheError::Io;
  }

  std::filesystem::rename(temporary, final_directory, error);
  if (error) {
    if (std::filesystem::exists(final_directory)) {
      remove_directory(temporary);
      const auto existing_text = read_text(final_directory / "metadata.v1");
      const auto existing_metadata =
          existing_text.has_value() ? parse_metadata(*existing_text) : std::nullopt;
      const auto existing_artifact =
          read_binary(final_directory / "kernel.so", state.config.limits.maximum_entry_bytes);
      const bool equivalent = existing_metadata.has_value() && existing_artifact.has_value() &&
                              metadata_matches(*existing_metadata, cache_key,
                                               state.config.compiler_epoch, *existing_artifact) &&
                              existing_metadata->kernel_ir_schema == descriptor.kernel_ir_schema &&
                              existing_metadata->helper_abi == descriptor.helper_abi &&
                              existing_metadata->payload == descriptor.payload &&
                              existing_metadata->artifact_sha256 == metadata.artifact_sha256;
      return equivalent ? PersistentCacheError::None : PersistentCacheError::MetadataMismatch;
    }
    remove_directory(temporary);
    return PersistentCacheError::Io;
  }
  if (!fsync_directory(parent)) {
    return PersistentCacheError::Io;
  }
  return fault(state.config, CacheFaultPoint::AfterRename) ? PersistentCacheError::Io
                                                           : PersistentCacheError::None;
}

} // namespace

std::string_view persistent_cache_error_name(PersistentCacheError error) noexcept {
  switch (error) {
  case PersistentCacheError::None:
    return "MF_CACHE_NONE";
  case PersistentCacheError::Miss:
    return "MF_CACHE_MISS";
  case PersistentCacheError::EntryAvailable:
    return "MF_CACHE_ENTRY_AVAILABLE";
  case PersistentCacheError::InvalidKey:
    return "MF_CACHE_INVALID_KEY";
  case PersistentCacheError::ArtifactTooLarge:
    return "MF_CACHE_ARTIFACT_TOO_LARGE";
  case PersistentCacheError::QuotaExceeded:
    return "MF_CACHE_QUOTA_EXCEEDED";
  case PersistentCacheError::ReservationNotFound:
    return "MF_CACHE_RESERVATION_NOT_FOUND";
  case PersistentCacheError::MetadataMismatch:
    return "MF_CACHE_METADATA_MISMATCH";
  case PersistentCacheError::Io:
    return "MF_CACHE_IO";
  }
  return "MF_CACHE_UNKNOWN";
}

PersistentArtifactCache::PersistentArtifactCache(PersistentCacheConfig config)
    : state_(std::make_shared<State>(std::move(config))) {}

PersistentArtifactCache::~PersistentArtifactCache() = default;
PersistentArtifactCache::PersistentArtifactCache(PersistentArtifactCache&&) noexcept = default;
PersistentArtifactCache&
PersistentArtifactCache::operator=(PersistentArtifactCache&&) noexcept = default;

PersistentCacheError PersistentArtifactCache::reconcile() {
  std::scoped_lock lock(state_->mutex);
  auto global_lock = acquire_global_lock(state_->config);
  if (!global_lock.has_value()) {
    return PersistentCacheError::Io;
  }
  if (!ensure_directory(state_->config.mutable_root / "users", 0700)) {
    return PersistentCacheError::Io;
  }
  if (!scan_candidates(state_, true).has_value() ||
      !scan_reservations(state_->config).has_value()) {
    return PersistentCacheError::Io;
  }
  return PersistentCacheError::None;
}

PersistentCacheLookup PersistentArtifactCache::lookup(std::uint32_t uid, std::string_view cache_key,
                                                      const ArtifactValidator& validator) {
  if (!valid_cache_key(cache_key)) {
    return {.error = PersistentCacheError::InvalidKey,
            .entry = std::nullopt,
            .corruption_recovered = false};
  }
  std::scoped_lock lock(state_->mutex);
  bool corruption = false;
  bool io_failure = false;
  const auto lookup_tier =
      [&](PersistentCacheTier tier,
          const std::filesystem::path& directory) -> std::optional<PersistentCacheEntry> {
    const auto metadata_text = read_text(directory / "metadata.v1");
    const auto metadata = metadata_text.has_value() ? parse_metadata(*metadata_text) : std::nullopt;
    std::optional<UniqueFileDescriptor> mutable_pin;
    std::optional<std::vector<std::byte>> artifact;
    if (tier == PersistentCacheTier::MutableUser) {
      mutable_pin = acquire_shared_entry_pin(directory / "kernel.so");
      if (mutable_pin.has_value()) {
        artifact =
            read_binary_descriptor(mutable_pin->get(), state_->config.limits.maximum_entry_bytes);
      }
    } else {
      artifact = read_binary(directory / "kernel.so", state_->config.limits.maximum_entry_bytes);
    }
    if (!metadata.has_value() || !artifact.has_value() ||
        !metadata_matches(*metadata, cache_key, state_->config.compiler_epoch, *artifact) ||
        (validator && !validator(*artifact))) {
      std::error_code error;
      if (std::filesystem::exists(directory, error)) {
        corruption = true;
        if (tier == PersistentCacheTier::MutableUser) {
          mutable_pin.reset();
          if (remove_unpinned_directory(directory) == RemovalState::Error) {
            io_failure = true;
          }
        }
      }
      return std::nullopt;
    }

    auto loaded_metadata = *metadata;
    std::shared_ptr<const void> pin;
    if (tier == PersistentCacheTier::MutableUser) {
      loaded_metadata.last_used = state_->config.clock();
      const auto temporary = directory / (".metadata.tmp-" + std::to_string(getpid()) + "-" +
                                          std::to_string(state_->next_token++));
      if (!write_synced_text(temporary, serialize_metadata(loaded_metadata), 0600)) {
        io_failure = true;
        return std::nullopt;
      }
      std::error_code error;
      std::filesystem::rename(temporary, directory / "metadata.v1", error);
      if (error || !fsync_directory(directory)) {
        std::filesystem::remove(temporary, error);
        io_failure = true;
        return std::nullopt;
      }
      const int pinned_descriptor = mutable_pin->release();
      pin = std::shared_ptr<const void>(new std::uint8_t{0U},
                                        [pinned_descriptor](const void* pointer) {
                                          delete static_cast<const std::uint8_t*>(pointer);
                                          static_cast<void>(close(pinned_descriptor));
                                        });
    }
    return PersistentCacheEntry{
        .tier = tier,
        .artifact_path = directory / "kernel.so",
        .metadata = std::move(loaded_metadata),
        .pin = std::move(pin),
    };
  };

  const auto aot = entry_directory(
      epoch_directory(state_->config.aot_root, state_->config.compiler_epoch), cache_key);
  if (auto entry = lookup_tier(PersistentCacheTier::AdministratorAot, aot); entry.has_value()) {
    return {.error = PersistentCacheError::None,
            .entry = std::move(entry),
            .corruption_recovered = corruption};
  }
  if (io_failure) {
    return {.error = PersistentCacheError::Io,
            .entry = std::nullopt,
            .corruption_recovered = corruption};
  }
  const auto user = entry_directory(user_epoch_directory(state_->config, uid), cache_key);
  std::error_code exists_error;
  if (!std::filesystem::exists(user, exists_error)) {
    if (exists_error) {
      return {.error = PersistentCacheError::Io,
              .entry = std::nullopt,
              .corruption_recovered = corruption};
    }
    return {.error = PersistentCacheError::Miss,
            .entry = std::nullopt,
            .corruption_recovered = corruption};
  }

  auto global_lock = acquire_global_lock(state_->config);
  if (!global_lock.has_value()) {
    return {.error = PersistentCacheError::Io,
            .entry = std::nullopt,
            .corruption_recovered = corruption};
  }
  if (auto entry = lookup_tier(PersistentCacheTier::MutableUser, user); entry.has_value()) {
    return {.error = PersistentCacheError::None,
            .entry = std::move(entry),
            .corruption_recovered = corruption};
  }
  if (io_failure) {
    return {.error = PersistentCacheError::Io,
            .entry = std::nullopt,
            .corruption_recovered = corruption};
  }
  return {.error = PersistentCacheError::Miss,
          .entry = std::nullopt,
          .corruption_recovered = corruption};
}

ReservationResult
PersistentArtifactCache::reserve(std::uint32_t uid, std::string_view cache_key, std::uint64_t bytes,
                                 std::optional<std::chrono::steady_clock::time_point> deadline) {
  if (!valid_cache_key(cache_key)) {
    return {.error = PersistentCacheError::InvalidKey, .reservation = std::nullopt};
  }
  if (bytes == 0U || bytes > state_->config.limits.maximum_entry_bytes) {
    return {.error = PersistentCacheError::ArtifactTooLarge, .reservation = std::nullopt};
  }
  // Wait outside State::mutex: the current publisher needs that mutex to release this key lock.
  auto key_lock = acquire_key_lock(state_->config, uid, cache_key, deadline);
  if (key_lock.state != KeyLockState::Acquired) {
    return {.error = PersistentCacheError::Io, .reservation = std::nullopt};
  }
  std::scoped_lock lock(state_->mutex);
  auto global_lock = acquire_global_lock(state_->config);
  if (!global_lock.has_value()) {
    return {.error = PersistentCacheError::Io, .reservation = std::nullopt};
  }
  const auto user_epoch = user_epoch_directory(state_->config, uid);
  const auto uid_root = state_->config.mutable_root / "users" / std::to_string(uid);
  if (!ensure_directory(uid_root, 0700) || !ensure_directory(user_epoch, 0700)) {
    return {.error = PersistentCacheError::Io, .reservation = std::nullopt};
  }
  auto active_reservations = scan_reservations(state_->config);
  if (!active_reservations.has_value()) {
    return {.error = PersistentCacheError::Io, .reservation = std::nullopt};
  }
  const auto committed = inspect_mutable_entry(state_->config, uid, cache_key);
  if (committed == CommittedEntryState::Available) {
    return {.error = PersistentCacheError::EntryAvailable, .reservation = std::nullopt};
  }
  if (committed == CommittedEntryState::PinnedInvalid) {
    return {.error = PersistentCacheError::MetadataMismatch, .reservation = std::nullopt};
  }
  if (committed == CommittedEntryState::Error) {
    return {.error = PersistentCacheError::Io, .reservation = std::nullopt};
  }
  auto scanned = scan_candidates(state_, true);
  if (!scanned.has_value()) {
    return {.error = PersistentCacheError::Io, .reservation = std::nullopt};
  }
  auto candidates = std::move(*scanned);
  std::uint64_t reclaimed = 0;
  while (saturated_add(usage(candidates, uid),
                       saturated_add(reservation_bytes(*active_reservations, uid), bytes)) >
         state_->config.limits.per_uid_bytes) {
    if (!evict_one(candidates, uid, reclaimed)) {
      return {.error = PersistentCacheError::QuotaExceeded, .reservation = std::nullopt};
    }
  }
  while (saturated_add(usage(candidates),
                       saturated_add(reservation_bytes(*active_reservations), bytes)) >
         state_->config.limits.global_bytes) {
    if (!evict_one(candidates, std::nullopt, reclaimed)) {
      return {.error = PersistentCacheError::QuotaExceeded, .reservation = std::nullopt};
    }
  }

  const auto filesystem = query_filesystem_space(state_->config);
  if (!filesystem.has_value()) {
    return {.error = PersistentCacheError::Io, .reservation = std::nullopt};
  }
  // statvfs is sampled after quota eviction, so those reclaimed bytes are already visible.
  reclaimed = 0;
  const auto percent_reserved =
      percentage_of(filesystem->total_bytes, state_->config.limits.reserved_free_percent);
  const auto reserved_free = std::max(state_->config.limits.reserved_free_bytes, percent_reserved);
  const auto required_free =
      saturated_add(reserved_free, saturated_add(reservation_bytes(*active_reservations), bytes));
  while (saturated_add(filesystem->available_bytes, reclaimed) < required_free) {
    if (!evict_one(candidates, std::nullopt, reclaimed)) {
      return {.error = PersistentCacheError::QuotaExceeded, .reservation = std::nullopt};
    }
  }

  auto token = state_->next_token++;
  if (token == 0U) {
    token = state_->next_token++;
  }
  auto record = create_reservation_record(state_->config, token, uid, bytes, cache_key,
                                          std::move(key_lock.descriptor));
  if (!record.has_value()) {
    return {.error = PersistentCacheError::Io, .reservation = std::nullopt};
  }
  state_->reservations.emplace(token, std::move(*record));
  return {.error = PersistentCacheError::None,
          .reservation = CacheReservation{
              .token = token,
              .uid = uid,
              .bytes = bytes,
              .cache_key = std::string(cache_key),
          }};
}

PersistentCacheError PersistentArtifactCache::publish(const CacheReservation& reservation,
                                                      std::span<const std::byte> artifact,
                                                      const ArtifactDescriptor& descriptor) {
  std::scoped_lock lock(state_->mutex);
  const auto found = state_->reservations.find(reservation.token);
  if (found == state_->reservations.end() || found->second.uid != reservation.uid ||
      found->second.cache_key != reservation.cache_key) {
    return PersistentCacheError::ReservationNotFound;
  }
  auto global_lock = acquire_global_lock(state_->config);
  if (!global_lock.has_value()) {
    state_->reservations.erase(found);
    return PersistentCacheError::Io;
  }
  if (artifact.empty() || artifact.size() > found->second.bytes ||
      artifact.size() > state_->config.limits.maximum_entry_bytes) {
    static_cast<void>(remove_reservation_record(found->second));
    state_->reservations.erase(found);
    return PersistentCacheError::ArtifactTooLarge;
  }
  const auto result =
      publish_entry(*state_, user_epoch_directory(state_->config, reservation.uid),
                    reservation.cache_key, artifact, descriptor, reservation.token, false);
  const bool reservation_removed = remove_reservation_record(found->second);
  state_->reservations.erase(found);
  return reservation_removed ? result : PersistentCacheError::Io;
}

void PersistentArtifactCache::cancel(const CacheReservation& reservation) noexcept {
  std::scoped_lock lock(state_->mutex);
  const auto found = state_->reservations.find(reservation.token);
  if (found != state_->reservations.end() && found->second.uid == reservation.uid &&
      found->second.cache_key == reservation.cache_key) {
    if (auto global_lock = acquire_global_lock(state_->config); global_lock.has_value()) {
      static_cast<void>(remove_reservation_record(found->second));
    }
    state_->reservations.erase(found);
  }
}

PersistentCacheError PersistentArtifactCache::install_aot(std::string_view cache_key,
                                                          std::span<const std::byte> artifact,
                                                          const ArtifactDescriptor& descriptor) {
  if (artifact.empty() || artifact.size() > state_->config.limits.maximum_entry_bytes) {
    return PersistentCacheError::ArtifactTooLarge;
  }
  std::scoped_lock lock(state_->mutex);
  auto global_lock = acquire_global_lock(state_->config);
  if (!global_lock.has_value()) {
    return PersistentCacheError::Io;
  }
  const auto token = state_->next_token++;
  return publish_entry(*state_,
                       epoch_directory(state_->config.aot_root, state_->config.compiler_epoch),
                       cache_key, artifact, descriptor, token, true);
}

PersistentCacheError PersistentArtifactCache::invalidate(const PersistentCacheEntry& entry) {
  if (entry.tier == PersistentCacheTier::AdministratorAot) {
    return PersistentCacheError::MetadataMismatch;
  }
  std::scoped_lock lock(state_->mutex);
  auto global_lock = acquire_global_lock(state_->config);
  if (!global_lock.has_value()) {
    return PersistentCacheError::Io;
  }
  const auto removed = remove_unpinned_directory(entry.artifact_path.parent_path());
  return removed == RemovalState::Removed ? PersistentCacheError::None : PersistentCacheError::Io;
}

} // namespace metaflux::compiler
