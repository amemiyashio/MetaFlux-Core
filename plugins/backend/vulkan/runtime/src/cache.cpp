#include "metaflux/backend/vulkan_cache.hpp"

#include <algorithm>
#include <cstddef>
#include <iomanip>
#include <limits>
#include <sstream>

namespace metaflux::backend::vulkan {

namespace {

template <typename Range>
void append_hex(std::ostringstream& stream, const Range& bytes) {
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

CacheStatus CacheCatalog::unpin(const std::string& key) noexcept {
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
  }
  return "unknown";
}

} // namespace metaflux::backend::vulkan
