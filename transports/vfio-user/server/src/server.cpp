#include <metaflux/transport/vfio_user_server.hpp>

#include <array>
#include <algorithm>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <limits>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

namespace metaflux::transport::vfio_user {
namespace {

constexpr std::uint16_t kReplyFlag = UINT16_C(0x8000);
constexpr std::uint32_t kPageSize = 4096U;
constexpr std::size_t kMaximumPacketSize = 512U;

std::uint32_t status_code(std::int32_t status) noexcept {
  return static_cast<std::uint32_t>(status);
}

bool range_overflows(std::uint64_t start, std::uint64_t size) noexcept {
  return size == 0U || start > std::numeric_limits<std::uint64_t>::max() - size;
}

bool aligned(std::uint64_t value) noexcept { return (value % kPageSize) == 0U; }

bool bytes_zero(const std::uint8_t* bytes, std::size_t count) noexcept {
  if (bytes == nullptr) {
    return false;
  }
  for (std::size_t index = 0; index < count; ++index) {
    if (bytes[index] != 0U) {
      return false;
    }
  }
  return true;
}

} // namespace

VfioUserServer::VfioUserServer(int fd, ServerConfig config) noexcept : fd_(fd), config_(config) {
  if (fd_ < 0 || config_.device_generation == 0U || config_.mapping_epoch == 0U ||
      config_.max_mappings == 0U || config_.address_width == 0U || config_.address_width > 63U ||
      config_.transport_major != MF_TRANSPORT_MAJOR_V0 || config_.transport_minor == 0U ||
      config_.transport_features == 0U || config_.daemon_incarnation == 0U ||
      config_.view_serial == 0U || config_.descriptor_version == 0U ||
      config_.ring_version == 0U || config_.max_queues == 0U || config_.ring_order == 0U ||
      config_.dma_alignment == 0U || config_.max_regions == 0U ||
      config_.max_inflight == 0U || config_.max_bytes == 0U) {
    state_ = ServerState::Lost;
    lifecycle_online_ = false;
    lifecycle_accepting_ = false;
  }
  if (state_ != ServerState::Lost) {
    mappings_.reserve(config_.max_mappings);
    retired_mappings_.reserve(config_.max_mappings);
    if (msix_.configure(config_.device_generation, nullptr, nullptr) != MsixStatus::success) {
      state_ = ServerState::Lost;
      lifecycle_online_ = false;
      lifecycle_accepting_ = false;
    }
  }
}

VfioUserServer::~VfioUserServer() {
  const auto close_mappings = [](const std::vector<DmaMapping>& mappings) noexcept {
    for (const DmaMapping& mapping : mappings) {
      if (mapping.fd >= 0) {
        (void)::close(mapping.fd);
      }
    }
  };
  close_mappings(mappings_);
  close_mappings(retired_mappings_);
}

void VfioUserServer::mark_lost() noexcept {
  state_ = ServerState::Lost;
  lifecycle_online_ = false;
  lifecycle_accepting_ = false;
  (void)msix_.mark_lost(config_.device_generation);
}

MsixStatus VfioUserServer::configure_msix(MsixInjectCallback inject, void* context) noexcept {
  if (state_ == ServerState::Closed || config_.device_generation == 0U) {
    return MsixStatus::device_lost;
  }
  msix_inject_ = inject;
  msix_context_ = context;
  return msix_.configure(config_.device_generation, inject, context);
}

MsixStatus VfioUserServer::msix_set_mask(std::uint64_t generation, std::uint32_t vector,
                                         bool masked) noexcept {
  return msix_.set_mask(generation, vector, masked);
}

MsixStatus VfioUserServer::msix_arm_completion(std::uint64_t generation,
                                               std::uint64_t timeline) noexcept {
  return msix_.arm_completion(generation, timeline);
}

MsixStatus VfioUserServer::msix_notify(std::uint64_t generation, std::uint32_t vector,
                                       std::uint64_t timeline) noexcept {
  return msix_.notify(generation, vector, timeline);
}

MsixStatus VfioUserServer::msix_retry_pending(std::uint64_t generation,
                                              std::uint32_t vector) noexcept {
  return msix_.retry_pending(generation, vector);
}

MsixStatus VfioUserServer::msix_snapshot(std::uint64_t generation, std::uint32_t vector,
                                         MsixVectorState* out_state) const noexcept {
  return msix_.snapshot(generation, vector, out_state);
}

ServerResult
VfioUserServer::mark_lost_and_submit(const metaflux::runtime::lifecycle::ExternalEvent& event,
                                     metaflux::runtime::lifecycle::Coordinator& coordinator,
                                     metaflux::runtime::lifecycle::ResultDetails& out) noexcept {
  mark_lost();
  if (event.kind != metaflux::runtime::lifecycle::ExternalEventKind::Disconnect) {
    out = metaflux::runtime::lifecycle::ResultDetails{};
    out.result = metaflux::runtime::lifecycle::Result::Invalid;
    out.snapshot = coordinator.snapshot();
    return ServerResult::Malformed;
  }
  const auto normalized =
      metaflux::runtime::lifecycle::submit_external_event(coordinator, event, out);
  return normalized == metaflux::runtime::lifecycle::NormalizationResult::Accepted
             ? ServerResult::Closed
             : ServerResult::Malformed;
}

bool VfioUserServer::dma_lookup(std::uint64_t iova, std::uint64_t size,
                                std::uint32_t permission) const noexcept {
  if (!lifecycle_online_ || !lifecycle_accepting_ || permission == 0U ||
      range_overflows(iova, size)) {
    return false;
  }
  for (const DmaMapping& mapping : mappings_) {
    const std::uint64_t end = mapping.iova + mapping.size;
    if (!mapping.revoking && !mapping.finalized && iova >= mapping.iova && iova + size <= end &&
        (mapping.permissions & permission) == permission &&
        mapping.device_generation == config_.device_generation &&
        mapping.mapping_epoch == config_.mapping_epoch) {
      return true;
    }
  }
  return false;
}

bool VfioUserServer::dma_acquire(std::uint64_t iova, std::uint64_t size,
                                 std::uint32_t permission, DmaLease& out) noexcept {
  out = DmaLease{};
  if (!dma_lookup(iova, size, permission) || next_lease_id_ == 0U) {
    return false;
  }
  for (DmaMapping& mapping : mappings_) {
    const std::uint64_t end = mapping.iova + mapping.size;
    if (mapping.revoking || mapping.finalized || iova < mapping.iova || iova + size > end ||
        (mapping.permissions & permission) != permission ||
        mapping.device_generation != config_.device_generation ||
        mapping.mapping_epoch != config_.mapping_epoch) {
      continue;
    }
    const std::uint64_t lease_id = next_lease_id_++;
    try {
      mapping.lease_ids.push_back(lease_id);
    } catch (...) {
      return false;
    }
    out.lease_id = lease_id;
    out.iova = iova;
    out.size = size;
    out.mapping_epoch = mapping.mapping_epoch;
    out.device_generation = mapping.device_generation;
    out.permissions = permission;
    return true;
  }
  return false;
}

void VfioUserServer::finalize_mapping(DmaMapping& mapping) noexcept {
  if (mapping.finalized) {
    return;
  }
  if (mapping.fd >= 0) {
    (void)::close(mapping.fd);
    mapping.fd = -1;
  }
  if (mapped_bytes_ >= mapping.size) {
    mapped_bytes_ -= mapping.size;
  } else {
    mapped_bytes_ = 0U;
  }
  mapping.finalized = true;
}

bool VfioUserServer::dma_release(const DmaLease& lease) noexcept {
  if (lease.lease_id == 0U || range_overflows(lease.iova, lease.size) || lease.permissions == 0U) {
    return false;
  }
  const auto release_from = [&](std::vector<DmaMapping>& mappings) noexcept {
    for (DmaMapping& mapping : mappings) {
      if (mapping.iova > lease.iova || range_overflows(mapping.iova, mapping.size) ||
          lease.iova + lease.size > mapping.iova + mapping.size ||
          mapping.mapping_epoch != lease.mapping_epoch ||
          mapping.device_generation != lease.device_generation ||
          (mapping.permissions & lease.permissions) != lease.permissions) {
        continue;
      }
      const auto iterator =
          std::find(mapping.lease_ids.begin(), mapping.lease_ids.end(), lease.lease_id);
      if (iterator == mapping.lease_ids.end()) {
        continue;
      }
      mapping.lease_ids.erase(iterator);
      if (mapping.revoking && mapping.lease_ids.empty()) {
        finalize_mapping(mapping);
      }
      return true;
    }
    return false;
  };
  return release_from(mappings_) || release_from(retired_mappings_);
}

void VfioUserServer::clear_finalized_tombstones() noexcept {
  retired_mappings_.erase(
      std::remove_if(retired_mappings_.begin(), retired_mappings_.end(),
                    [](const DmaMapping& mapping) {
                      return mapping.finalized && mapping.lease_ids.empty();
                    }),
      retired_mappings_.end());
}

ServerResult VfioUserServer::reply_payload(std::uint64_t message_id, std::uint16_t request_type,
                                           const void* payload, std::size_t payload_size,
                                           bool no_reply) noexcept {
  if (no_reply) {
    return ServerResult::NoReply;
  }
  if (fd_ < 0 || payload_size > kMaximumPacketSize - sizeof(mf_transport_message_header_v0) ||
      (payload_size != 0U && payload == nullptr)) {
    mark_lost();
    return ServerResult::Closed;
  }
  mf_transport_message_header_v0 header{};
  header.message_id = message_id;
  header.message_type = static_cast<std::uint16_t>(request_type | kReplyFlag);
  header.payload_size = static_cast<std::uint32_t>(payload_size);
  std::array<std::uint8_t, kMaximumPacketSize> packet{};
  std::memcpy(packet.data(), &header, sizeof(header));
  if (payload_size != 0U) {
    std::memcpy(packet.data() + sizeof(header), payload, payload_size);
  }
  const ssize_t sent = ::send(fd_, packet.data(), sizeof(header) + payload_size, MSG_NOSIGNAL);
  if (sent != static_cast<ssize_t>(sizeof(header) + payload_size)) {
    mark_lost();
    return ServerResult::Closed;
  }
  return ServerResult::Replied;
}

ServerResult VfioUserServer::reply(std::uint64_t message_id, std::uint16_t request_type,
                                   std::int32_t status, const void* result, std::size_t result_size,
                                   bool no_reply) noexcept {
  mf_transport_completion_v0 completion{};
  if (result_size > sizeof(completion.result) || (result_size != 0U && result == nullptr)) {
    mark_lost();
    return ServerResult::Closed;
  }
  completion.status = status_code(status);
  completion.request_id = message_id;
  completion.device_generation = config_.device_generation;
  if (result_size != 0U) {
    std::memcpy(completion.result, result, result_size);
  }
  return reply_payload(message_id, request_type, &completion, sizeof(completion), no_reply);
}

ServerResult VfioUserServer::handle_negotiate(const mf_transport_message_header_v0& header,
                                              const std::uint8_t* payload,
                                              std::size_t payload_size, int received_fd,
                                              bool no_reply) noexcept {
  mf_transport_negotiate_v0 request{};
  mf_transport_negotiate_v0 response{};
  if (received_fd >= 0) {
    (void)::close(received_fd);
  }
  if (!lifecycle_online_ || !lifecycle_accepting_ || state_ == ServerState::Lost ||
      state_ == ServerState::Closed) {
    return reply(header.message_id, header.message_type, MF_SHARED_DEVICE_LOST, nullptr, 0U,
                 no_reply);
  }
  if (payload == nullptr || payload_size != sizeof(request)) {
    return reply(header.message_id, header.message_type, MF_SHARED_INVALID_ARGUMENT, nullptr, 0U,
                 no_reply);
  }
  std::memcpy(&request, payload, sizeof(request));
  if (request.magic != MF_TRANSPORT_MAGIC_V0 || request.struct_size != sizeof(request) ||
      request.flags != 0U || !bytes_zero(request.logical_device_uuid,
                                           sizeof(request.logical_device_uuid)) ||
      request.daemon_incarnation != 0U || request.view_serial != 0U ||
      request.device_generation != 0U || request.descriptor_version != 0U ||
      request.ring_version != 0U || request.max_queues != 0U || request.ring_order != 0U ||
      request.dma_width != 0U || request.dma_alignment != 0U || request.max_regions != 0U ||
      request.max_inflight != 0U || request.max_bytes != 0U ||
      !bytes_zero(request.reserved, sizeof(request.reserved))) {
    return reply(header.message_id, header.message_type, MF_SHARED_INVALID_ARGUMENT, nullptr, 0U,
                 no_reply);
  }
  if (request.major != config_.transport_major || request.minor == 0U ||
      request.minor > config_.transport_minor ||
      (request.required_features & ~config_.transport_features) != 0U) {
    return reply(header.message_id, header.message_type, MF_SHARED_NOT_SUPPORTED, nullptr, 0U,
                 no_reply);
  }
  response.magic = MF_TRANSPORT_MAGIC_V0;
  response.major = config_.transport_major;
  response.minor = request.minor;
  response.struct_size = sizeof(response);
  response.required_features = request.required_features;
  response.optional_features = request.optional_features & config_.transport_features;
  response.daemon_incarnation = config_.daemon_incarnation;
  response.view_serial = config_.view_serial;
  std::memcpy(response.logical_device_uuid, config_.logical_device_uuid.data(),
              sizeof(response.logical_device_uuid));
  response.device_generation = config_.device_generation;
  response.descriptor_version = config_.descriptor_version;
  response.ring_version = config_.ring_version;
  response.max_queues = config_.max_queues;
  response.ring_order = config_.ring_order;
  response.dma_width = config_.address_width;
  response.dma_alignment = config_.dma_alignment;
  response.max_regions = config_.max_regions;
  response.max_inflight = config_.max_inflight;
  response.max_bytes = config_.max_bytes;
  negotiated_ = true;
  state_ = ServerState::Configuring;
  return reply_payload(header.message_id, header.message_type, &response, sizeof(response), no_reply);
}

ServerResult VfioUserServer::handle_get_info(const mf_transport_message_header_v0& header,
                                             bool no_reply) noexcept {
  if (!lifecycle_online_ || !lifecycle_accepting_ ||
      (state_ != ServerState::Negotiating && state_ != ServerState::Configuring &&
       state_ != ServerState::Running)) {
    return reply(header.message_id, header.message_type, MF_SHARED_DEVICE_LOST, nullptr, 0U,
                 no_reply);
  }
  mf_vfio_user_get_info_reply_v0 info{};
  info.status = status_code(MF_SHARED_SUCCESS);
  info.device_generation = config_.device_generation;
  info.bar0_offset = 0U;
  info.bar0_size = UINT64_C(65536);
  info.bar2_offset = UINT64_C(65536);
  info.bar2_size = UINT64_C(4096);
  info.bar4_offset = UINT64_C(69632);
  info.bar4_size = UINT64_C(4096);
  info.msix_vectors = 2U;
  info.doorbell_width = 4U;
  state_ = ServerState::Configuring;
  return reply_payload(header.message_id, header.message_type, &info, sizeof(info), no_reply);
}

ServerResult VfioUserServer::handle_dma_map(const mf_transport_message_header_v0& header,
                                            const std::uint8_t* payload, std::size_t payload_size,
                                            int received_fd, bool no_reply) noexcept {
  mf_vfio_user_dma_map_v0 request{};
  struct stat file_stat{};
  int duplicate_fd = -1;
  const auto fail = [&](std::int32_t status) noexcept {
    if (received_fd >= 0) {
      (void)::close(received_fd);
    }
    return reply(header.message_id, header.message_type, status, nullptr, 0U, no_reply);
  };

  if (!lifecycle_online_ || !lifecycle_accepting_ || state_ == ServerState::Lost ||
      state_ == ServerState::Closed) {
    return fail(MF_SHARED_DEVICE_LOST);
  }
  if ((state_ != ServerState::Configuring && state_ != ServerState::Running) ||
      payload == nullptr || payload_size != sizeof(request) || received_fd < 0) {
    return fail(MF_SHARED_INVALID_ARGUMENT);
  }
  std::memcpy(&request, payload, sizeof(request));
  if (request.struct_size != sizeof(request) ||
      (request.flags & static_cast<std::uint32_t>(~MF_VFIO_USER_DMA_KNOWN_FLAGS_V0)) != 0U ||
      request.flags == 0U || request.fd_index != 0 ||
      !bytes_zero(request.reserved, sizeof(request.reserved))) {
    return fail(MF_SHARED_INVALID_ARGUMENT);
  }
  if (request.mapping_epoch != config_.mapping_epoch ||
      request.device_generation != config_.device_generation) {
    return fail(MF_SHARED_STALE_HANDLE);
  }
  if (range_overflows(request.iova, request.size) || !aligned(request.iova) ||
      !aligned(request.size) || !aligned(request.file_offset) ||
      request.iova + request.size > (UINT64_C(1) << config_.address_width) ||
      ::fstat(received_fd, &file_stat) != 0 ||
      file_stat.st_size < 0 ||
      request.file_offset > static_cast<std::uint64_t>(file_stat.st_size) ||
      request.size > static_cast<std::uint64_t>(file_stat.st_size) - request.file_offset) {
    return fail(MF_SHARED_INVALID_ARGUMENT);
  }
  if (mappings_.size() + retired_mappings_.size() >= config_.max_mappings ||
      request.size > config_.max_bytes - mapped_bytes_) {
    return fail(MF_SHARED_RESOURCE_EXHAUSTED);
  }
  const auto overlaps = [&](const std::vector<DmaMapping>& mappings) noexcept {
    for (const DmaMapping& mapping : mappings) {
      if (request.iova < mapping.iova + mapping.size && mapping.iova < request.iova + request.size) {
        return true;
      }
    }
    return false;
  };
  if (overlaps(mappings_) || overlaps(retired_mappings_)) {
    return fail(MF_SHARED_INVALID_ARGUMENT);
  }
  duplicate_fd = ::fcntl(received_fd, F_DUPFD_CLOEXEC, 0);
  (void)::close(received_fd);
  if (duplicate_fd < 0) {
    return reply(header.message_id, header.message_type, MF_SHARED_SYSTEM_ERROR, nullptr, 0U,
                 no_reply);
  }
  try {
    mappings_.push_back({request.iova, request.size, request.file_offset, request.mapping_epoch,
                         request.device_generation, request.flags, duplicate_fd});
  } catch (...) {
    (void)::close(duplicate_fd);
    return reply(header.message_id, header.message_type, MF_SHARED_SYSTEM_ERROR, nullptr, 0U,
                 no_reply);
  }
  mapped_bytes_ += request.size;
  state_ = ServerState::Running;
  return reply(header.message_id, header.message_type, MF_SHARED_SUCCESS, nullptr, 0U, no_reply);
}

ServerResult VfioUserServer::handle_dma_unmap(const mf_transport_message_header_v0& header,
                                              const std::uint8_t* payload, std::size_t payload_size,
                                              bool no_reply) noexcept {
  mf_vfio_user_dma_unmap_v0 request{};
  if (!lifecycle_online_ || !lifecycle_accepting_ || state_ == ServerState::Lost ||
      state_ == ServerState::Closed) {
    return reply(header.message_id, header.message_type, MF_SHARED_DEVICE_LOST, nullptr, 0U,
                 no_reply);
  }
  if ((state_ != ServerState::Configuring && state_ != ServerState::Running) ||
      payload == nullptr || payload_size != sizeof(request)) {
    return reply(header.message_id, header.message_type, MF_SHARED_INVALID_ARGUMENT, nullptr, 0U,
                 no_reply);
  }
  std::memcpy(&request, payload, sizeof(request));
  if (request.struct_size != sizeof(request) || request.flags != 0U ||
      !bytes_zero(request.reserved, sizeof(request.reserved))) {
    return reply(header.message_id, header.message_type, MF_SHARED_INVALID_ARGUMENT, nullptr, 0U,
                 no_reply);
  }
  if (request.mapping_epoch != config_.mapping_epoch ||
      request.device_generation != config_.device_generation) {
    return reply(header.message_id, header.message_type, MF_SHARED_STALE_HANDLE, nullptr, 0U,
                 no_reply);
  }
  if (range_overflows(request.iova, request.size)) {
    return reply(header.message_id, header.message_type, MF_SHARED_INVALID_ARGUMENT, nullptr, 0U,
                 no_reply);
  }
  for (auto iterator = mappings_.begin(); iterator != mappings_.end(); ++iterator) {
    if (iterator->iova == request.iova && iterator->size == request.size) {
      DmaMapping retired = std::move(*iterator);
      mappings_.erase(iterator);
      retired.revoking = true;
      retired_mappings_.push_back(std::move(retired));
      DmaMapping& pending = retired_mappings_.back();
      if (!pending.lease_ids.empty()) {
        return reply(header.message_id, header.message_type, MF_SHARED_WOULD_BLOCK, nullptr, 0U,
                     no_reply);
      }
      finalize_mapping(pending);
      retired_mappings_.pop_back();
      if (mappings_.empty() && retired_mappings_.empty()) {
        state_ = ServerState::Configuring;
      }
      return reply(header.message_id, header.message_type, MF_SHARED_SUCCESS, nullptr, 0U,
                   no_reply);
    }
  }
  for (auto iterator = retired_mappings_.begin(); iterator != retired_mappings_.end(); ++iterator) {
    if (iterator->iova != request.iova || iterator->size != request.size) {
      continue;
    }
    if (!iterator->lease_ids.empty()) {
      return reply(header.message_id, header.message_type, MF_SHARED_WOULD_BLOCK, nullptr, 0U,
                   no_reply);
    }
    finalize_mapping(*iterator);
    retired_mappings_.erase(iterator);
    if (mappings_.empty() && retired_mappings_.empty()) {
      state_ = ServerState::Configuring;
    }
    return reply(header.message_id, header.message_type, MF_SHARED_SUCCESS, nullptr, 0U,
                 no_reply);
  }
  return reply(header.message_id, header.message_type, MF_SHARED_STALE_HANDLE, nullptr, 0U,
               no_reply);
}

ServerResult VfioUserServer::handle_message(const mf_transport_message_header_v0& header,
                                            const std::uint8_t* payload, std::size_t payload_size,
                                            int received_fd, bool no_reply) noexcept {
  switch (header.message_type) {
  case MF_VFIO_USER_MESSAGE_NEGOTIATE_V0:
    return handle_negotiate(header, payload, payload_size, received_fd, no_reply);
  case MF_VFIO_USER_MESSAGE_GET_INFO_V0:
    if (received_fd >= 0) {
      (void)::close(received_fd);
    }
    return payload_size == 0U ? handle_get_info(header, no_reply)
                              : reply(header.message_id, header.message_type,
                                      MF_SHARED_INVALID_ARGUMENT, nullptr, 0U, no_reply);
  case MF_VFIO_USER_MESSAGE_DMA_MAP_V0:
    return handle_dma_map(header, payload, payload_size, received_fd, no_reply);
  case MF_VFIO_USER_MESSAGE_DMA_UNMAP_V0:
    if (received_fd >= 0) {
      (void)::close(received_fd);
    }
    return handle_dma_unmap(header, payload, payload_size, no_reply);
  case MF_VFIO_USER_MESSAGE_RESET_V0:
  case MF_VFIO_USER_MESSAGE_DOORBELL_V0:
  default:
    if (received_fd >= 0) {
      (void)::close(received_fd);
    }
    return reply(header.message_id, header.message_type,
                 lifecycle_online_ && lifecycle_accepting_ ? MF_SHARED_NOT_SUPPORTED
                                                           : MF_SHARED_DEVICE_LOST,
                 nullptr, 0U, no_reply);
  }
}

ServerResult VfioUserServer::process_once() noexcept {
  std::array<std::uint8_t, kMaximumPacketSize> packet{};
  std::array<std::uint8_t, CMSG_SPACE(sizeof(int))> control{};
  struct iovec vector{packet.data(), packet.size()};
  struct msghdr message{};
  message.msg_iov = &vector;
  message.msg_iovlen = 1;
  message.msg_control = control.data();
  message.msg_controllen = control.size();
  const ssize_t received = ::recvmsg(fd_, &message, MSG_CMSG_CLOEXEC);
  if (received == 0) {
    mark_lost();
    return ServerResult::Closed;
  }
  if (received < 0) {
    if (errno == EINTR || errno == EAGAIN) {
      return ServerResult::Idle;
    }
    mark_lost();
    return ServerResult::Closed;
  }
  int received_fd = -1;
  for (struct cmsghdr* header = CMSG_FIRSTHDR(&message); header != nullptr;
       header = CMSG_NXTHDR(&message, header)) {
    if (header->cmsg_level != SOL_SOCKET || header->cmsg_type != SCM_RIGHTS ||
        header->cmsg_len != CMSG_LEN(sizeof(int)) || received_fd >= 0) {
      if (received_fd >= 0) {
        (void)::close(received_fd);
      }
      mark_lost();
      return ServerResult::Malformed;
    }
    std::memcpy(&received_fd, CMSG_DATA(header), sizeof(received_fd));
  }
  if ((message.msg_flags & (MSG_TRUNC | MSG_CTRUNC)) != 0 ||
      static_cast<std::size_t>(received) < sizeof(mf_transport_message_header_v0)) {
    if (received_fd >= 0) {
      (void)::close(received_fd);
    }
    return ServerResult::Malformed;
  }
  mf_transport_message_header_v0 header{};
  std::memcpy(&header, packet.data(), sizeof(header));
  const std::size_t available = static_cast<std::size_t>(received) - sizeof(header);
  if (header.message_id == 0U ||
      (header.flags & static_cast<std::uint16_t>(~MF_TRANSPORT_FLAG_NO_REPLY_V0)) != 0U ||
      static_cast<std::size_t>(header.payload_size) != available ||
      static_cast<std::size_t>(header.payload_size) > packet.size() - sizeof(header)) {
    if (received_fd >= 0) {
      (void)::close(received_fd);
    }
    return ServerResult::Malformed;
  }
  return handle_message(header, packet.data() + sizeof(header), available, received_fd,
                        (header.flags & MF_TRANSPORT_FLAG_NO_REPLY_V0) != 0U);
}

ServerResult
VfioUserServer::process_once(metaflux::runtime::lifecycle::Coordinator& coordinator,
                             const metaflux::runtime::lifecycle::ExternalEvent& disconnect_event,
                             metaflux::runtime::lifecycle::ResultDetails& out) noexcept {
  out = metaflux::runtime::lifecycle::ResultDetails{};
  const ServerResult result = process_once();
  if (result != ServerResult::Closed) {
    return result;
  }
  return mark_lost_and_submit(disconnect_event, coordinator, out);
}

ServerResult
VfioUserServer::process_once(metaflux::runtime::lifecycle::Coordinator& coordinator,
                             std::uint64_t request_id, std::uint64_t deadline_tick,
                             metaflux::runtime::lifecycle::ResultDetails& out) noexcept {
  out = metaflux::runtime::lifecycle::ResultDetails{};
  const ServerResult result = process_once();
  if (result != ServerResult::Closed) {
    return result;
  }
  const auto disconnect_event = metaflux::runtime::lifecycle::capture_external_event(
      metaflux::runtime::lifecycle::ExternalEventKind::Disconnect, request_id,
      coordinator.snapshot(), deadline_tick);
  return mark_lost_and_submit(disconnect_event, coordinator, out);
}

bool VfioUserServer::drain_lifecycle() noexcept {
  return mappings_.empty() && std::all_of(retired_mappings_.begin(), retired_mappings_.end(),
                                          [](const DmaMapping& mapping) {
                                            return mapping.finalized && mapping.lease_ids.empty();
                                          });
}

bool VfioUserServer::lifecycle_prepare(
    void* context, const metaflux::runtime::lifecycle::MirrorEvent& event) noexcept {
  auto* server = static_cast<VfioUserServer*>(context);
  if (server == nullptr || server->fd_ < 0 || server->state_ == ServerState::Closed) {
    return false;
  }
  return event.request.operation != metaflux::runtime::lifecycle::Operation::Add ||
         server->state_ != ServerState::Lost;
}

bool VfioUserServer::lifecycle_quiesce(
    void* context, const metaflux::runtime::lifecycle::MirrorEvent& event) noexcept {
  auto* server = static_cast<VfioUserServer*>(context);
  if (server == nullptr ||
      (event.request.operation == metaflux::runtime::lifecycle::Operation::Reset &&
       !server->lifecycle_online_)) {
    return false;
  }
  server->lifecycle_accepting_ = false;
  return true;
}

bool VfioUserServer::lifecycle_drain(void* context,
                                     const metaflux::runtime::lifecycle::MirrorEvent&) noexcept {
  auto* server = static_cast<VfioUserServer*>(context);
  return server != nullptr && server->drain_lifecycle();
}

bool VfioUserServer::lifecycle_commit(
    void* context, const metaflux::runtime::lifecycle::MirrorEvent& event) noexcept {
  auto* server = static_cast<VfioUserServer*>(context);
  if (server == nullptr) {
    return false;
  }
  if (event.state_after == metaflux::runtime::lifecycle::State::Absent) {
    server->clear_finalized_tombstones();
    server->state_ = ServerState::Closed;
    server->lifecycle_online_ = false;
    server->lifecycle_accepting_ = false;
    if (server->fd_ >= 0) {
      (void)::close(server->fd_);
      server->fd_ = -1;
    }
    return true;
  }
  if (event.candidate.generation != 0U) {
    server->config_.device_generation = event.candidate.generation;
  }
  if (event.candidate.epoch != 0U) {
    server->config_.mapping_epoch = event.candidate.epoch;
  }
  server->clear_finalized_tombstones();
  if (server->msix_.configure(server->config_.device_generation, server->msix_inject_,
                              server->msix_context_) != MsixStatus::success) {
    server->mark_lost();
    return false;
  }
  server->state_ = ServerState::Configuring;
  server->lifecycle_online_ = true;
  server->lifecycle_accepting_ = true;
  return true;
}

bool VfioUserServer::lifecycle_abort(
    void* context, const metaflux::runtime::lifecycle::MirrorEvent& event) noexcept {
  auto* server = static_cast<VfioUserServer*>(context);
  if (server == nullptr) {
    return false;
  }
  if (event.state_before == metaflux::runtime::lifecycle::State::Lost) {
    server->mark_lost();
  } else if (event.state_before == metaflux::runtime::lifecycle::State::Online) {
    server->state_ = server->mappings_.empty() && server->retired_mappings_.empty()
                         ? ServerState::Configuring
                         : ServerState::Running;
    server->lifecycle_online_ = true;
    server->lifecycle_accepting_ = true;
  } else {
    server->state_ = ServerState::Negotiating;
    server->lifecycle_online_ = false;
    server->lifecycle_accepting_ = false;
  }
  return true;
}

void VfioUserServer::lifecycle_lost(void* context,
                                    const metaflux::runtime::lifecycle::MirrorEvent&) noexcept {
  auto* server = static_cast<VfioUserServer*>(context);
  if (server != nullptr) {
    server->mark_lost();
  }
}

bool VfioUserServer::attach_lifecycle(
    metaflux::runtime::lifecycle::Coordinator& coordinator) noexcept {
  return coordinator.register_mirror(lifecycle_mirror());
}

metaflux::runtime::lifecycle::Mirror VfioUserServer::lifecycle_mirror() noexcept {
  return metaflux::runtime::lifecycle::Mirror{
      .kind = metaflux::runtime::lifecycle::MirrorKind::VfioUser,
      .name = "vfio-user",
      .context = this,
      .prepare = lifecycle_prepare,
      .quiesce = lifecycle_quiesce,
      .drain = lifecycle_drain,
      .commit = lifecycle_commit,
      .abort = lifecycle_abort,
      .publish_lost = lifecycle_lost,
  };
}

} // namespace metaflux::transport::vfio_user
