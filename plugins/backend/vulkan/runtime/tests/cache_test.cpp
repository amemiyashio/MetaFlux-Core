#include "metaflux/backend/vulkan_cache.hpp"

#include <cstdint>
#include <cstdio>

namespace {

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
  if (catalog.publish(key_a, "spirv-a", false) !=
          metaflux::backend::vulkan::CacheStatus::success ||
      catalog.lookup(key_a, &payload) != metaflux::backend::vulkan::CacheStatus::hit ||
      payload != "spirv-a" ||
      catalog.publish(key_b, "spirv-b", false) !=
          metaflux::backend::vulkan::CacheStatus::success ||
      catalog.pin(key_a) != metaflux::backend::vulkan::CacheStatus::success) {
    return false;
  }
  auto changed_again = identity();
  changed_again.fp_mode = 10U;
  const auto key_c = changed_again.portable_key();
  if (catalog.publish(key_c, "spirv-c", false) !=
      metaflux::backend::vulkan::CacheStatus::success) {
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
  if (catalog.publish(key, "pipeline", true) !=
          metaflux::backend::vulkan::CacheStatus::success ||
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

} // namespace

int main() {
  const bool ok = key_partitioning() && catalog_lifecycle() && pinned_quota();
  std::printf("vulkan cache model: %s\n", ok ? "pass" : "fail");
  return ok ? 0 : 1;
}
