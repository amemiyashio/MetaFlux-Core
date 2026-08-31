#include "metaflux/backend/vulkan_cache.hpp"

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

namespace {

class TemporaryDirectory final {
public:
  TemporaryDirectory() {
    char pattern[] = "/tmp/metaflux-vulkan-cache-XXXXXX";
    const char* created = ::mkdtemp(pattern);
    if (created != nullptr) {
      path_ = created;
    }
  }

  ~TemporaryDirectory() {
    std::error_code error;
    std::filesystem::remove_all(path_, error);
  }

  [[nodiscard]] bool valid() const noexcept { return !path_.empty(); }
  [[nodiscard]] const std::filesystem::path& path() const noexcept { return path_; }

private:
  std::filesystem::path path_;
};

metaflux::backend::vulkan::CacheIdentity identity() {
  metaflux::backend::vulkan::CacheIdentity result{};
  result.compiler_epoch = 1U;
  result.lowering_epoch = 2U;
  result.spirv_tools_epoch = 3U;
  result.fp_mode = 4U;
  result.argument_abi = 5U;
  result.backend_abi = 6U;
  result.vendor_id = 0x1002U;
  result.device_id = 0x744cU;
  result.driver_version = 42U;
  for (std::uint32_t index = 0U; index < result.kernel_ir_digest.size(); ++index) {
    result.kernel_ir_digest[index] = static_cast<std::uint8_t>(index + 1U);
    result.target_digest[index] = static_cast<std::uint8_t>(0xa0U + index);
  }
  for (std::uint32_t index = 0U; index < result.specialization_digest.size(); ++index) {
    result.specialization_digest[index] = static_cast<std::uint8_t>(0x40U + index);
  }
  for (std::uint32_t index = 0U; index < result.device_uuid.size(); ++index) {
    result.device_uuid[index] = static_cast<std::uint8_t>(0x10U + index);
    result.driver_uuid[index] = static_cast<std::uint8_t>(0x20U + index);
    result.pipeline_cache_uuid[index] = static_cast<std::uint8_t>(0x30U + index);
  }
  return result;
}

bool key_partitioning() {
  auto base = identity();
  const auto portable = base.portable_key();
  const auto device = base.device_key();
  if (portable.empty() || device.empty() || portable == device ||
      portable.find("kernel=") == std::string::npos ||
      device.find("pipeline-cache-uuid=") == std::string::npos) {
    return false;
  }
  auto changed = base;
  changed.fp_mode += 1U;
  if (changed.portable_key() == portable) {
    return false;
  }
  changed = base;
  changed.device_uuid[0] ^= 0xffU;
  return changed.portable_key() == portable && changed.device_key() != device;
}

bool catalog_lifecycle() {
  const auto key_a = identity().portable_key();
  auto changed = identity();
  changed.fp_mode = 9U;
  const auto key_b = changed.portable_key();
  metaflux::backend::vulkan::CacheCatalog catalog(2U);
  std::string payload;
  if (catalog.publish(key_a, "spirv-a", false) != metaflux::backend::vulkan::CacheStatus::success ||
      catalog.lookup(key_a, &payload) != metaflux::backend::vulkan::CacheStatus::hit ||
      payload != "spirv-a" ||
      catalog.publish(key_b, "spirv-b", false) != metaflux::backend::vulkan::CacheStatus::success ||
      catalog.pin(key_a) != metaflux::backend::vulkan::CacheStatus::success) {
    return false;
  }
  auto changed_again = identity();
  changed_again.fp_mode = 10U;
  const auto key_c = changed_again.portable_key();
  if (catalog.publish(key_c, "spirv-c", false) != metaflux::backend::vulkan::CacheStatus::success) {
    return false;
  }
  if (catalog.lookup(key_a, &payload) != metaflux::backend::vulkan::CacheStatus::hit ||
      catalog.lookup(key_b, &payload) != metaflux::backend::vulkan::CacheStatus::miss ||
      catalog.mark_corrupt(key_a) != metaflux::backend::vulkan::CacheStatus::pinned ||
      catalog.unpin(key_a) != metaflux::backend::vulkan::CacheStatus::success ||
      catalog.mark_corrupt(key_a) != metaflux::backend::vulkan::CacheStatus::success ||
      catalog.lookup(key_a, &payload) != metaflux::backend::vulkan::CacheStatus::corrupt ||
      catalog.publish(key_a, "spirv-a-rebuilt", false) !=
          metaflux::backend::vulkan::CacheStatus::success ||
      catalog.unpin(key_a) != metaflux::backend::vulkan::CacheStatus::invalid_argument) {
    return false;
  }
  return catalog.lookup(key_a, &payload) == metaflux::backend::vulkan::CacheStatus::hit &&
         payload == "spirv-a-rebuilt";
}

bool pinned_quota() {
  const auto base = identity();
  const auto key = base.device_key();
  metaflux::backend::vulkan::CacheCatalog catalog(1U);
  if (catalog.publish(key, "pipeline", true) != metaflux::backend::vulkan::CacheStatus::success ||
      catalog.pin(key) != metaflux::backend::vulkan::CacheStatus::success) {
    return false;
  }
  auto changed = base;
  changed.device_id += 1U;
  const auto changed_key = changed.device_key();
  if (catalog.publish(changed_key, "pipeline-new", true) !=
      metaflux::backend::vulkan::CacheStatus::quota_exceeded) {
    return false;
  }
  return catalog.unpin(key) == metaflux::backend::vulkan::CacheStatus::success &&
         catalog.publish(changed_key, "pipeline-new", true) ==
             metaflux::backend::vulkan::CacheStatus::success;
}

bool filesystem_round_trip_and_atomic_replace() {
  TemporaryDirectory temporary;
  if (!temporary.valid()) {
    return false;
  }
  metaflux::backend::vulkan::CacheFileStore store(temporary.path() / "cache", 1024U);
  const auto key = identity().portable_key();
  if (store.publish(key, "spirv-old", false) != metaflux::backend::vulkan::CacheStatus::success ||
      store.publish(key, "spirv-new", false) != metaflux::backend::vulkan::CacheStatus::success) {
    return false;
  }
  std::string payload;
  if (store.lookup(key, false, &payload) != metaflux::backend::vulkan::CacheStatus::hit ||
      payload != "spirv-new") {
    return false;
  }
  metaflux::backend::vulkan::CacheFileStore reopened(temporary.path() / "cache", 1024U);
  if (reopened.lookup(key, false, &payload) != metaflux::backend::vulkan::CacheStatus::hit ||
      payload != "spirv-new") {
    return false;
  }
  for (const auto& entry : std::filesystem::directory_iterator(store.root())) {
    if (entry.path().filename().string().find(".tmp.") != std::string::npos) {
      return false;
    }
  }
  return true;
}

bool filesystem_corruption_is_removed() {
  TemporaryDirectory temporary;
  if (!temporary.valid()) {
    return false;
  }
  metaflux::backend::vulkan::CacheFileStore store(temporary.path() / "cache", 1024U);
  const auto key = identity().portable_key();
  if (store.publish(key, "0123456789", false) != metaflux::backend::vulkan::CacheStatus::success) {
    return false;
  }
  {
    std::ofstream output(store.entry_path(key, false), std::ios::binary | std::ios::trunc);
    output << "truncated";
  }
  std::string payload;
  if (store.lookup(key, false, &payload) != metaflux::backend::vulkan::CacheStatus::corrupt ||
      std::filesystem::exists(store.entry_path(key, false))) {
    return false;
  }
  if (store.publish(key, "0123456789", false) != metaflux::backend::vulkan::CacheStatus::success) {
    return false;
  }
  const auto path = store.entry_path(key, false);
  std::ifstream input(path, std::ios::binary);
  const std::string original((std::istreambuf_iterator<char>(input)),
                             std::istreambuf_iterator<char>());
  if (original.empty()) {
    return false;
  }
  std::string mutated = original;
  mutated.back() = mutated.back() == 'x' ? 'y' : 'x';
  {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output.write(mutated.data(), static_cast<std::streamsize>(mutated.size()));
  }
  return store.lookup(key, false, &payload) == metaflux::backend::vulkan::CacheStatus::corrupt &&
         !std::filesystem::exists(path);
}

bool filesystem_device_invalidation_and_inputs() {
  TemporaryDirectory temporary;
  if (!temporary.valid()) {
    return false;
  }
  metaflux::backend::vulkan::CacheFileStore store(temporary.path() / "cache", 8U);
  const auto portable_key = identity().portable_key();
  const auto device_key = identity().device_key();
  if (store.publish(portable_key, "portable", false) !=
          metaflux::backend::vulkan::CacheStatus::success ||
      store.publish(device_key, "device", true) !=
          metaflux::backend::vulkan::CacheStatus::success ||
      store.invalidate_device(device_key) != metaflux::backend::vulkan::CacheStatus::success) {
    return false;
  }
  std::string payload;
  if (store.lookup(device_key, true, &payload) != metaflux::backend::vulkan::CacheStatus::miss ||
      store.lookup(portable_key, false, &payload) != metaflux::backend::vulkan::CacheStatus::hit ||
      payload != "portable") {
    return false;
  }
  return store.publish("bad\nkey", "x", false) ==
             metaflux::backend::vulkan::CacheStatus::invalid_argument &&
         store.publish(portable_key, "012345678", false) ==
             metaflux::backend::vulkan::CacheStatus::invalid_argument &&
         store.lookup(portable_key, false, nullptr) ==
             metaflux::backend::vulkan::CacheStatus::invalid_argument;
}

bool persistent_repository_hydrates_and_preserves_pins() {
  TemporaryDirectory temporary;
  if (!temporary.valid()) {
    return false;
  }
  const auto base = identity();
  const auto key_a = base.portable_key();
  auto changed = base;
  changed.fp_mode = 17U;
  const auto key_b = changed.portable_key();
  auto device_identity = base;
  device_identity.device_id += 1U;
  const auto device_key = device_identity.device_key();

  metaflux::backend::vulkan::PersistentCacheRepository repository(temporary.path() / "cache", 1U,
                                                                  1024U);
  std::string payload;
  if (repository.publish(key_a, "portable-a", false) !=
          metaflux::backend::vulkan::CacheStatus::success ||
      repository.pin(key_a) != metaflux::backend::vulkan::CacheStatus::success ||
      repository.publish(key_a, "portable-a-replaced", false) !=
          metaflux::backend::vulkan::CacheStatus::pinned ||
      repository.lookup(key_a, false, &payload) != metaflux::backend::vulkan::CacheStatus::hit ||
      payload != "portable-a" ||
      repository.publish(key_b, "portable-b", false) !=
          metaflux::backend::vulkan::CacheStatus::quota_exceeded ||
      repository.unpin(key_a) != metaflux::backend::vulkan::CacheStatus::success ||
      repository.publish(key_b, "portable-b", false) !=
          metaflux::backend::vulkan::CacheStatus::success ||
      repository.size() != 1U) {
    return false;
  }

  metaflux::backend::vulkan::PersistentCacheRepository reopened(temporary.path() / "cache", 1U,
                                                                1024U);
  if (reopened.lookup(key_a, false, &payload) != metaflux::backend::vulkan::CacheStatus::hit ||
      payload != "portable-a" || reopened.size() != 1U ||
      reopened.lookup(key_b, false, &payload) != metaflux::backend::vulkan::CacheStatus::hit ||
      payload != "portable-b") {
    return false;
  }

  if (reopened.pin(key_b) != metaflux::backend::vulkan::CacheStatus::success ||
      reopened.publish(device_key, "device", true) !=
          metaflux::backend::vulkan::CacheStatus::quota_exceeded ||
      reopened.unpin(key_b) != metaflux::backend::vulkan::CacheStatus::success ||
      reopened.publish(device_key, "device", true) !=
          metaflux::backend::vulkan::CacheStatus::success ||
      reopened.pin(device_key) != metaflux::backend::vulkan::CacheStatus::success ||
      reopened.invalidate_device(device_key) != metaflux::backend::vulkan::CacheStatus::pinned ||
      reopened.unpin(device_key) != metaflux::backend::vulkan::CacheStatus::success ||
      reopened.invalidate_device(device_key) != metaflux::backend::vulkan::CacheStatus::success) {
    return false;
  }
  return reopened.lookup(device_key, true, &payload) ==
         metaflux::backend::vulkan::CacheStatus::miss;
}

bool persistent_repository_binds_pipeline_generations() {
  TemporaryDirectory temporary;
  if (!temporary.valid()) {
    return false;
  }
  const auto key = identity().device_key();
  metaflux::backend::vulkan::PersistentCacheRepository repository(temporary.path() / "cache", 2U,
                                                                  1024U);
  std::string payload;
  if (repository.publish(key, "pipeline", true) !=
          metaflux::backend::vulkan::CacheStatus::success ||
      repository.acquire_pipeline(key, 0U) !=
          metaflux::backend::vulkan::CacheStatus::invalid_argument ||
      repository.acquire_pipeline(key, 7U) != metaflux::backend::vulkan::CacheStatus::success ||
      repository.acquire_pipeline(key, 7U) != metaflux::backend::vulkan::CacheStatus::pinned ||
      repository.acquire_pipeline(key, 8U) !=
          metaflux::backend::vulkan::CacheStatus::stale_generation ||
      repository.unpin(key) != metaflux::backend::vulkan::CacheStatus::pinned ||
      repository.invalidate_device(key) != metaflux::backend::vulkan::CacheStatus::pinned ||
      repository.release_pipeline(key, 8U) !=
          metaflux::backend::vulkan::CacheStatus::stale_generation ||
      repository.release_pipeline(key, 7U) != metaflux::backend::vulkan::CacheStatus::success ||
      repository.invalidate_device(key) != metaflux::backend::vulkan::CacheStatus::success ||
      repository.lookup(key, true, &payload) != metaflux::backend::vulkan::CacheStatus::miss ||
      repository.release_pipeline(key, 7U) != metaflux::backend::vulkan::CacheStatus::not_found ||
      repository.acquire_pipeline(key, 7U) != metaflux::backend::vulkan::CacheStatus::miss) {
    return false;
  }
  return std::string(metaflux::backend::vulkan::cache_status_string(
             metaflux::backend::vulkan::CacheStatus::stale_generation)) == "stale-generation";
}

bool persistent_repository_coalesces_process_misses() {
  TemporaryDirectory temporary;
  if (!temporary.valid()) {
    return false;
  }
  int start_pipe[2] = {-1, -1};
  int result_pipe[2] = {-1, -1};
  if (::pipe(start_pipe) != 0 || ::pipe(result_pipe) != 0) {
    if (start_pipe[0] >= 0) {
      static_cast<void>(::close(start_pipe[0]));
      static_cast<void>(::close(start_pipe[1]));
    }
    return false;
  }
  const auto key = identity().device_key();
  const auto marker = temporary.path() / "producer-once";
  const auto producer = [&marker]() -> std::optional<std::string> {
    const int descriptor = ::open(marker.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
    if (descriptor < 0) {
      return std::nullopt;
    }
    static_cast<void>(::close(descriptor));
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    return std::string("pipeline");
  };

  const pid_t child = ::fork();
  if (child < 0) {
    static_cast<void>(::close(start_pipe[0]));
    static_cast<void>(::close(start_pipe[1]));
    static_cast<void>(::close(result_pipe[0]));
    static_cast<void>(::close(result_pipe[1]));
    return false;
  }
  if (child == 0) {
    static_cast<void>(::close(start_pipe[1]));
    static_cast<void>(::close(result_pipe[0]));
    char start = 0;
    const auto started = ::read(start_pipe[0], &start, sizeof(start));
    static_cast<void>(::close(start_pipe[0]));
    if (started != 1) {
      _exit(2);
    }
    metaflux::backend::vulkan::PersistentCacheRepository repository(temporary.path() / "cache", 1U,
                                                                    1024U, std::chrono::seconds(5));
    std::string payload;
    const auto status = repository.lookup_or_publish(key, true, producer, &payload);
    const char result =
        status == metaflux::backend::vulkan::CacheStatus::hit && payload == "pipeline" ? '0' : '1';
    static_cast<void>(::write(result_pipe[1], &result, sizeof(result)));
    static_cast<void>(::close(result_pipe[1]));
    _exit(result == '0' ? 0 : 1);
  }

  static_cast<void>(::close(start_pipe[0]));
  static_cast<void>(::close(result_pipe[1]));
  const char start = '1';
  const bool start_sent = ::write(start_pipe[1], &start, sizeof(start)) == 1;
  static_cast<void>(::close(start_pipe[1]));
  metaflux::backend::vulkan::PersistentCacheRepository repository(temporary.path() / "cache", 1U,
                                                                  1024U, std::chrono::seconds(5));
  std::string payload;
  const auto status = start_sent ? repository.lookup_or_publish(key, true, producer, &payload)
                                 : metaflux::backend::vulkan::CacheStatus::io_error;
  char child_result = 0;
  const bool result_received = ::read(result_pipe[0], &child_result, sizeof(child_result)) == 1;
  static_cast<void>(::close(result_pipe[0]));
  int child_status = 0;
  const bool child_reaped = ::waitpid(child, &child_status, 0) == child;
  return status == metaflux::backend::vulkan::CacheStatus::hit && payload == "pipeline" &&
         result_received && child_result == '0' && child_reaped && WIFEXITED(child_status) &&
         WEXITSTATUS(child_status) == 0 && std::filesystem::exists(marker);
}

} // namespace

int main() {
  const bool ok = key_partitioning() && catalog_lifecycle() && pinned_quota() &&
                  filesystem_round_trip_and_atomic_replace() &&
                  filesystem_corruption_is_removed() &&
                  filesystem_device_invalidation_and_inputs() &&
                  persistent_repository_hydrates_and_preserves_pins() &&
                  persistent_repository_binds_pipeline_generations() &&
                  persistent_repository_coalesces_process_misses();
  std::printf("vulkan cache model: %s\n", ok ? "pass" : "fail");
  return ok ? 0 : 1;
}
