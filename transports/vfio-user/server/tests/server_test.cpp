#include <metaflux/transport/vfio_user_guest.h>
#include <metaflux/transport/vfio_user_server.hpp>

#include <array>
#include <cerrno>
#include <climits>
#include <cstdint>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <linux/memfd.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <unistd.h>

namespace {

bool send_packet(int fd, const std::uint8_t* bytes, std::uint32_t size, int passed_fd = -1) {
  struct iovec vector{const_cast<std::uint8_t*>(bytes), size};
  struct msghdr message{};
  std::array<std::uint8_t, CMSG_SPACE(sizeof(int))> control{};
  message.msg_iov = &vector;
  message.msg_iovlen = 1;
  if (passed_fd >= 0) {
    message.msg_control = control.data();
    message.msg_controllen = control.size();
    auto* header = CMSG_FIRSTHDR(&message);
    header->cmsg_level = SOL_SOCKET;
    header->cmsg_type = SCM_RIGHTS;
    header->cmsg_len = CMSG_LEN(sizeof(int));
    std::memcpy(CMSG_DATA(header), &passed_fd, sizeof(passed_fd));
  }
  return ::sendmsg(fd, &message, MSG_NOSIGNAL) == static_cast<ssize_t>(size);
}

bool send_multiple_rights(int fd, const std::uint8_t* bytes, std::uint32_t size,
                          int passed_fd) {
  struct iovec vector{const_cast<std::uint8_t*>(bytes), size};
  struct msghdr message{};
  std::array<std::uint8_t, CMSG_SPACE(sizeof(int) * 2U)> control{};
  message.msg_iov = &vector;
  message.msg_iovlen = 1;
  message.msg_control = control.data();
  message.msg_controllen = control.size();
  auto* header = CMSG_FIRSTHDR(&message);
  if (header == nullptr) {
    return false;
  }
  header->cmsg_level = SOL_SOCKET;
  header->cmsg_type = SCM_RIGHTS;
  header->cmsg_len = CMSG_LEN(sizeof(int) * 2U);
  const int descriptors[2] = {passed_fd, passed_fd};
  std::memcpy(CMSG_DATA(header), descriptors, sizeof(descriptors));
  return ::sendmsg(fd, &message, MSG_NOSIGNAL) == static_cast<ssize_t>(size);
}

std::size_t open_fd_count() {
  DIR* directory = ::opendir("/proc/self/fd");
  if (directory == nullptr) {
    return 0U;
  }
  std::size_t count = 0U;
  for (const dirent* entry = ::readdir(directory); entry != nullptr;
       entry = ::readdir(directory)) {
    if (entry->d_name[0] != '.') {
      ++count;
    }
  }
  (void)::closedir(directory);
  return count;
}

bool receive_completion(int fd, std::uint64_t message_id, std::uint16_t request_type,
                        std::int32_t expected_status, mf_transport_completion_v0* out = nullptr) {
  std::array<std::uint8_t,
             sizeof(mf_transport_message_header_v0) + sizeof(mf_transport_completion_v0)>
      packet{};
  const ssize_t received = ::recv(fd, packet.data(), packet.size(), 0);
  mf_transport_completion_v0 completion{};
  if (received != static_cast<ssize_t>(packet.size()) ||
      mf_vfio_user_guest_decode_completion_v0(packet.data(), static_cast<std::uint32_t>(received),
                                              message_id, request_type,
                                              &completion) != MF_SHARED_SUCCESS ||
      completion.status != static_cast<std::uint32_t>(expected_status)) {
    return false;
  }
  if (out != nullptr) {
    *out = completion;
  }
  return true;
}

bool receive_info(int fd, std::uint64_t message_id, mf_vfio_user_get_info_reply_v0* out) {
  std::array<std::uint8_t,
             sizeof(mf_transport_message_header_v0) + sizeof(mf_vfio_user_get_info_reply_v0)>
      packet{};
  const ssize_t received = ::recv(fd, packet.data(), packet.size(), 0);
  mf_vfio_user_get_info_reply_v0 info{};
  if (received != static_cast<ssize_t>(packet.size()) ||
      mf_vfio_user_guest_decode_get_info_v0(packet.data(), static_cast<std::uint32_t>(received),
                                            message_id, &info) != MF_SHARED_SUCCESS ||
      info.status != static_cast<std::uint32_t>(MF_SHARED_SUCCESS)) {
    return false;
  }
  if (out != nullptr) {
    *out = info;
  }
  return true;
}

bool receive_negotiate(int fd, std::uint64_t message_id, mf_transport_negotiate_v0* out) {
  std::array<std::uint8_t,
             sizeof(mf_transport_message_header_v0) + sizeof(mf_transport_negotiate_v0)>
      packet{};
  const ssize_t received = ::recv(fd, packet.data(), packet.size(), 0);
  mf_transport_negotiate_v0 negotiation{};
  if (received != static_cast<ssize_t>(packet.size()) ||
      mf_vfio_user_guest_decode_negotiate_v0(
          packet.data(), static_cast<std::uint32_t>(received), message_id, &negotiation) !=
          MF_SHARED_SUCCESS) {
    return false;
  }
  if (out != nullptr) {
    *out = negotiation;
  }
  return true;
}

int make_memfd() {
  const long fd = syscall(SYS_memfd_create, "metaflux-vfio-test", MFD_CLOEXEC);
  if (fd < 0 || fd > INT_MAX || ftruncate(static_cast<int>(fd), 8192) != 0) {
    if (fd >= 0 && fd <= INT_MAX) {
      (void)close(static_cast<int>(fd));
    }
    return -1;
  }
  return static_cast<int>(fd);
}

bool test_server_releases_transport_fd() {
  int sockets[2] = {-1, -1};
  if (::socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0, sockets) != 0) {
    return false;
  }
  const int owned_fd = sockets[1];
  {
    metaflux::transport::vfio_user::VfioUserServer server(owned_fd);
    if (server.state() == metaflux::transport::vfio_user::ServerState::Lost) {
      close(sockets[0]);
      close(sockets[1]);
      return false;
    }
  }
  const bool released = ::fcntl(owned_fd, F_GETFD) < 0 && errno == EBADF;
  close(sockets[0]);
  return released;
}

bool test_dma_requires_shared_memory_negotiation() {
  int sockets[2] = {-1, -1};
  if (::socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0, sockets) != 0) {
    return false;
  }
  metaflux::transport::vfio_user::VfioUserServer server(sockets[1]);
  const int memfd = make_memfd();
  if (memfd < 0) {
    close(sockets[0]);
    close(sockets[1]);
    return false;
  }
  std::array<std::uint8_t, MF_VFIO_USER_MAX_PACKET_SIZE_V0> packet{};
  std::uint32_t packet_size = 0U;
  mf_vfio_user_dma_map_v0 map{};
  map.struct_size = sizeof(map);
  map.flags = MF_VFIO_USER_DMA_READ_V0;
  map.iova = 0x1000U;
  map.size = 0x1000U;
  map.mapping_epoch = 1U;
  map.device_generation = 1U;
  if (mf_vfio_user_guest_encode_dma_map_v0(1U, &map, packet.data(), packet.size(), &packet_size) !=
          MF_SHARED_SUCCESS ||
      !send_packet(sockets[0], packet.data(), packet_size, memfd) ||
      server.process_once() != metaflux::transport::vfio_user::ServerResult::Replied ||
      !receive_completion(sockets[0], 1U, MF_VFIO_USER_MESSAGE_DMA_MAP_V0,
                          MF_SHARED_NOT_SUPPORTED) ||
      server.mapping_count() != 0U) {
    close(memfd);
    close(sockets[0]);
    close(sockets[1]);
    return false;
  }
  close(memfd);
  close(sockets[0]);
  close(sockets[1]);
  return true;
}

bool test_invalid_loss_event_does_not_mutate_server() {
  int sockets[2] = {-1, -1};
  if (::socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0, sockets) != 0) {
    return false;
  }
  metaflux::transport::vfio_user::VfioUserServer server(sockets[1]);
  metaflux::runtime::lifecycle::Coordinator coordinator;
  metaflux::runtime::lifecycle::ResultDetails details{};
  const metaflux::runtime::lifecycle::ExternalEvent invalid_event{
      .request_id = 1U,
      .logical_device_id = 1U,
      .daemon_incarnation = 1U,
      .expected_identity_record_id = 1U,
      .expected_generation = 1U,
      .expected_epoch = 1U,
      .kind = metaflux::runtime::lifecycle::ExternalEventKind::AdminReset,
  };
  const auto result = server.mark_lost_and_submit(invalid_event, coordinator, details);
  const bool valid = result == metaflux::transport::vfio_user::ServerResult::Malformed &&
                     server.state() == metaflux::transport::vfio_user::ServerState::Negotiating &&
                     details.result == metaflux::runtime::lifecycle::Result::Invalid;
  close(sockets[0]);
  close(sockets[1]);
  return valid;
}

bool test_fatal_control_error_reports_transport_loss() {
  int sockets[2] = {-1, -1};
  if (::socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0, sockets) != 0) {
    return false;
  }
  metaflux::transport::vfio_user::VfioUserServer server(sockets[1]);
  metaflux::runtime::lifecycle::Config lifecycle_config{};
  lifecycle_config.logical_device_id = 7U;
  lifecycle_config.daemon_incarnation = 11U;
  lifecycle_config.initial_identity_record_id = 1U;
  lifecycle_config.initial_generation = 1U;
  lifecycle_config.initial_epoch = 1U;
  lifecycle_config.generation_terminal = 32U;
  lifecycle_config.identity_record_terminal = 32U;
  lifecycle_config.epoch_terminal = 32U;
  metaflux::runtime::lifecycle::Coordinator coordinator(lifecycle_config);
  const mf_transport_message_header_v0 header{
      .message_id = 1U,
      .message_type = MF_VFIO_USER_MESSAGE_GET_INFO_V0,
      .flags = 0U,
      .payload_size = 0U,
  };
  const std::size_t descriptors_before = open_fd_count();
  metaflux::runtime::lifecycle::ResultDetails details{};
  const bool sent = send_multiple_rights(
      sockets[0], reinterpret_cast<const std::uint8_t*>(&header), sizeof(header), sockets[0]);
  const auto result = server.process_once(coordinator, 2U, 0U, details);
  const std::size_t descriptors_after = open_fd_count();
  const bool valid = sent && result == metaflux::transport::vfio_user::ServerResult::Closed &&
                     server.state() == metaflux::transport::vfio_user::ServerState::Lost &&
                     details.result == metaflux::runtime::lifecycle::Result::Accepted &&
                     details.snapshot.state == metaflux::runtime::lifecycle::State::Lost &&
                     descriptors_before == descriptors_after;
  close(sockets[0]);
  close(sockets[1]);
  return valid;
}

bool test_transport_loss_drains_dma_before_recovery() {
  int sockets[2] = {-1, -1};
  if (::socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0, sockets) != 0) {
    return false;
  }
  metaflux::transport::vfio_user::VfioUserServer server(sockets[1]);
  std::array<std::uint8_t, MF_VFIO_USER_MAX_PACKET_SIZE_V0> packet{};
  std::uint32_t packet_size = 0U;
  mf_transport_negotiate_v0 negotiation{};
  negotiation.magic = MF_TRANSPORT_MAGIC_V0;
  negotiation.major = MF_TRANSPORT_MAJOR_V0;
  negotiation.minor = MF_TRANSPORT_MINOR_V0;
  negotiation.struct_size = sizeof(negotiation);
  negotiation.required_features = MF_TRANSPORT_FEATURE_VFIO_USER_V0 |
                                  MF_TRANSPORT_FEATURE_SHARED_MEMORY_V0;
  if (mf_vfio_user_guest_encode_negotiate_v0(1U, &negotiation, packet.data(), packet.size(),
                                             &packet_size) != MF_SHARED_SUCCESS ||
      !send_packet(sockets[0], packet.data(), packet_size) ||
      server.process_once() != metaflux::transport::vfio_user::ServerResult::Replied) {
    close(sockets[0]);
    close(sockets[1]);
    return false;
  }
  mf_transport_negotiate_v0 negotiated{};
  if (!receive_negotiate(sockets[0], 1U, &negotiated)) {
    close(sockets[0]);
    close(sockets[1]);
    return false;
  }

  const int memfd = make_memfd();
  if (memfd < 0) {
    close(sockets[0]);
    close(sockets[1]);
    return false;
  }
  mf_vfio_user_dma_map_v0 map{};
  map.struct_size = sizeof(map);
  map.flags = MF_VFIO_USER_DMA_READ_V0 | MF_VFIO_USER_DMA_WRITE_V0;
  map.iova = 0x1000U;
  map.size = 0x1000U;
  map.mapping_epoch = 1U;
  map.device_generation = 1U;
  if (mf_vfio_user_guest_encode_dma_map_v0(2U, &map, packet.data(), packet.size(), &packet_size) !=
          MF_SHARED_SUCCESS ||
      !send_packet(sockets[0], packet.data(), packet_size, memfd) ||
      server.process_once() != metaflux::transport::vfio_user::ServerResult::Replied ||
      !receive_completion(sockets[0], 2U, MF_VFIO_USER_MESSAGE_DMA_MAP_V0, MF_SHARED_SUCCESS)) {
    close(memfd);
    close(sockets[0]);
    close(sockets[1]);
    return false;
  }

  metaflux::runtime::lifecycle::Config lifecycle_config{};
  lifecycle_config.generation_terminal = 32U;
  lifecycle_config.identity_record_terminal = 32U;
  lifecycle_config.epoch_terminal = 32U;
  metaflux::runtime::lifecycle::Coordinator coordinator(lifecycle_config);
  if (!server.attach_lifecycle(coordinator)) {
    close(memfd);
    close(sockets[0]);
    close(sockets[1]);
    return false;
  }
  metaflux::transport::vfio_user::DmaLease lease{};
  if (!server.dma_acquire(0x1200U, 0x100U, MF_VFIO_USER_DMA_READ_V0, lease)) {
    close(memfd);
    close(sockets[0]);
    close(sockets[1]);
    return false;
  }
  auto forged_lease = lease;
  forged_lease.size = 0x80U;
  if (server.dma_release(forged_lease) ||
      !server.dma_lookup(0x1200U, 0x100U, MF_VFIO_USER_DMA_READ_V0)) {
    close(memfd);
    close(sockets[0]);
    close(sockets[1]);
    return false;
  }
  const auto disconnect = metaflux::runtime::lifecycle::capture_external_event(
      metaflux::runtime::lifecycle::ExternalEventKind::Disconnect, 3U, coordinator.snapshot());
  metaflux::runtime::lifecycle::ResultDetails details{};
  const auto loss_result = server.mark_lost_and_submit(disconnect, coordinator, details);
  const bool loss_valid =
      loss_result == metaflux::transport::vfio_user::ServerResult::Closed &&
      server.state() == metaflux::transport::vfio_user::ServerState::Lost &&
      server.mapping_count() == 0U && server.retired_mapping_count() == 1U &&
      server.mapped_bytes() == 0x1000U && !server.dma_lookup(0x1200U, 0x100U,
                                                              MF_VFIO_USER_DMA_READ_V0) &&
      details.result == metaflux::runtime::lifecycle::Result::Accepted &&
      details.snapshot.state == metaflux::runtime::lifecycle::State::Lost;
  if (!loss_valid || !server.dma_release(lease) || server.mapped_bytes() != 0U ||
      server.retired_mapping_count() != 1U || server.dma_release(lease)) {
    close(memfd);
    close(sockets[0]);
    close(sockets[1]);
    return false;
  }

  const metaflux::runtime::lifecycle::Request recover{
      .request_id = 4U,
      .logical_device_id = 1U,
      .daemon_incarnation = 1U,
      .expected_identity_record_id = 1U,
      .expected_generation = 1U,
      .expected_epoch = 1U,
      .source = metaflux::runtime::lifecycle::Source::Restart,
      .operation = metaflux::runtime::lifecycle::Operation::Recover,
  };
  const bool recovered =
      coordinator.apply(recover, details) == metaflux::runtime::lifecycle::Result::Accepted &&
      server.device_generation() == 2U && server.mapping_epoch() == 2U &&
      server.lifecycle_online() &&
      server.state() == metaflux::transport::vfio_user::ServerState::Configuring &&
      server.retired_mapping_count() == 0U;
  close(memfd);
  close(sockets[0]);
  close(sockets[1]);
  return recovered;
}

} // namespace

int main() {
  if (!test_server_releases_transport_fd() ||
      !test_dma_requires_shared_memory_negotiation() ||
      !test_invalid_loss_event_does_not_mutate_server() ||
      !test_fatal_control_error_reports_transport_loss() ||
      !test_transport_loss_drains_dma_before_recovery()) {
    return 1;
  }
  int sockets[2] = {-1, -1};
  if (socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0, sockets) != 0) {
    return 1;
  }
  metaflux::transport::vfio_user::ServerConfig server_config{};
  server_config.max_bytes = 0x2000U;
  metaflux::transport::vfio_user::VfioUserServer server(sockets[1], server_config);
  std::array<std::uint8_t, MF_VFIO_USER_MAX_PACKET_SIZE_V0> packet{};
  std::array<std::uint8_t, MF_VFIO_USER_MAX_PACKET_SIZE_V0> no_reply_packet{};
  std::uint32_t packet_size = 0;

  mf_transport_negotiate_v0 negotiation{};
  negotiation.magic = MF_TRANSPORT_MAGIC_V0;
  negotiation.major = MF_TRANSPORT_MAJOR_V0;
  negotiation.minor = MF_TRANSPORT_MINOR_V0 + 1U;
  negotiation.struct_size = sizeof(negotiation);
  negotiation.required_features = MF_TRANSPORT_FEATURE_VFIO_USER_V0;
  if (mf_vfio_user_guest_encode_negotiate_v0(6U, &negotiation, packet.data(), packet.size(),
                                             &packet_size) != MF_SHARED_SUCCESS ||
      !send_packet(sockets[0], packet.data(), packet_size) ||
      server.process_once() != metaflux::transport::vfio_user::ServerResult::Replied ||
      !receive_completion(sockets[0], 6U, MF_VFIO_USER_MESSAGE_NEGOTIATE_V0,
                          MF_SHARED_NOT_SUPPORTED)) {
    return 1;
  }

  negotiation.minor = MF_TRANSPORT_MINOR_V0;
  negotiation.required_features = MF_TRANSPORT_FEATURE_VFIO_USER_V0 |
                                  MF_TRANSPORT_FEATURE_SHARED_MEMORY_V0;
  negotiation.optional_features = MF_TRANSPORT_FEATURE_IOEVENTFD_V0 |
                                  MF_TRANSPORT_FEATURE_MSIX_V0 |
                                  MF_TRANSPORT_FEATURE_CDEV_V0;
  if (mf_vfio_user_guest_encode_negotiate_v0(7U, &negotiation, packet.data(), packet.size(),
                                             &packet_size) != MF_SHARED_SUCCESS ||
      !send_packet(sockets[0], packet.data(), packet_size) ||
      server.process_once() != metaflux::transport::vfio_user::ServerResult::Replied) {
    return 1;
  }
  mf_transport_negotiate_v0 negotiated{};
  if (!receive_negotiate(sockets[0], 7U, &negotiated) ||
      negotiated.required_features != negotiation.required_features ||
      negotiated.optional_features !=
          (MF_TRANSPORT_FEATURE_IOEVENTFD_V0 | MF_TRANSPORT_FEATURE_MSIX_V0) ||
      negotiated.daemon_incarnation == 0U || negotiated.view_serial == 0U ||
      negotiated.device_generation != 1U || negotiated.descriptor_version != 1U ||
      negotiated.ring_version != 1U || negotiated.max_queues != 2U ||
      negotiated.ring_order != 8U || negotiated.dma_width != 48U ||
      negotiated.dma_alignment != 4096U || negotiated.max_regions != 64U ||
      negotiated.max_inflight != 256U || negotiated.max_bytes != 0x2000U ||
      server.state() != metaflux::transport::vfio_user::ServerState::Configuring) {
    return 1;
  }

  if (mf_vfio_user_guest_encode_get_info_v0(7U, no_reply_packet.data(), no_reply_packet.size(),
                                             &packet_size) != MF_SHARED_SUCCESS) {
    return 1;
  }
  auto* no_reply_header =
      reinterpret_cast<mf_transport_message_header_v0*>(no_reply_packet.data());
  no_reply_header->flags = MF_TRANSPORT_FLAG_NO_REPLY_V0;
  if (!send_packet(sockets[0], no_reply_packet.data(), packet_size) ||
      mf_vfio_user_guest_encode_get_info_v0(7U, packet.data(), packet.size(), &packet_size) !=
          MF_SHARED_SUCCESS ||
      !send_packet(sockets[0], packet.data(), packet_size) ||
      server.process_once() != metaflux::transport::vfio_user::ServerResult::NoReply ||
      server.process_once() != metaflux::transport::vfio_user::ServerResult::Replied) {
    return 1;
  }
  if (!receive_info(sockets[0], 7U, nullptr)) {
    return 1;
  }

  if (mf_vfio_user_guest_encode_get_info_v0(7U, packet.data(), packet.size(), &packet_size) !=
          MF_SHARED_SUCCESS ||
      !send_packet(sockets[0], packet.data(), packet_size) ||
      server.process_once() != metaflux::transport::vfio_user::ServerResult::Replied) {
    return 1;
  }
  mf_vfio_user_get_info_reply_v0 info{};
  if (!receive_info(sockets[0], 7U, &info)) {
    return 1;
  }
  if (info.bar0_size != 65536U || info.bar2_size != 4096U || info.bar4_size != 4096U ||
      info.msix_vectors != 2U || info.doorbell_width != 4U ||
      server.state() != metaflux::transport::vfio_user::ServerState::Configuring) {
    return 1;
  }

  const int memfd = make_memfd();
  if (memfd < 0) {
    return 1;
  }
  mf_vfio_user_dma_map_v0 map{};
  map.struct_size = sizeof(map);
  map.flags = MF_VFIO_USER_DMA_READ_V0 | MF_VFIO_USER_DMA_WRITE_V0;
  map.iova = 0x1000U;
  map.size = 0x1000U;
  map.mapping_epoch = 1U;
  map.device_generation = 1U;
  map.fd_index = 0;
  if (mf_vfio_user_guest_encode_dma_map_v0(7U, &map, packet.data(), packet.size(), &packet_size) !=
          MF_SHARED_SUCCESS ||
      !send_packet(sockets[0], packet.data(), packet_size, memfd) ||
      server.process_once() != metaflux::transport::vfio_user::ServerResult::Replied ||
      !receive_completion(sockets[0], 7U, MF_VFIO_USER_MESSAGE_DMA_MAP_V0, MF_SHARED_SUCCESS) ||
      !server.dma_lookup(0x1200U, 0x100U, MF_VFIO_USER_DMA_READ_V0) ||
      server.mapping_count() != 1U || server.mapped_bytes() != 0x1000U) {
    close(memfd);
    return 1;
  }
  auto* mapped = static_cast<std::uint8_t*>(
      server.dma_host_address(0x1200U, 0x100U, MF_VFIO_USER_DMA_WRITE_V0));
  std::uint8_t observed = 0U;
  const std::uint8_t expected = 0xa5U;
  if (mapped == nullptr) {
    close(memfd);
    return 1;
  }
  mapped[0x20U] = expected;
  if (::pread(memfd, &observed, sizeof(observed), 0x220) != 1 || observed != expected) {
    close(memfd);
    return 1;
  }

  mf_vfio_user_dma_map_v0 quota_map = map;
  quota_map.iova = 0x3000U;
  quota_map.size = 0x2000U;
  if (mf_vfio_user_guest_encode_dma_map_v0(71U, &quota_map, packet.data(), packet.size(),
                                           &packet_size) != MF_SHARED_SUCCESS ||
      !send_packet(sockets[0], packet.data(), packet_size, memfd) ||
      server.process_once() != metaflux::transport::vfio_user::ServerResult::Replied ||
      !receive_completion(sockets[0], 71U, MF_VFIO_USER_MESSAGE_DMA_MAP_V0,
                          MF_SHARED_RESOURCE_EXHAUSTED) ||
      server.mapping_count() != 1U || server.mapped_bytes() != 0x1000U) {
    close(memfd);
    return 1;
  }

  if (mf_vfio_user_guest_encode_dma_map_v0(7U, &map, packet.data(), packet.size(), &packet_size) !=
      MF_SHARED_SUCCESS) {
    close(memfd);
    return 1;
  }

  metaflux::transport::vfio_user::DmaLease lease{};
  metaflux::transport::vfio_user::DmaLease invalid_lease{};
  const bool acquired = server.dma_acquire(0x1200U, 0x100U, MF_VFIO_USER_DMA_READ_V0, lease);
  const bool invalid_acquired =
      server.dma_acquire(0x1200U, 0U, MF_VFIO_USER_DMA_READ_V0, invalid_lease);
  if (!acquired || lease.lease_id == 0U || invalid_acquired) {
    close(memfd);
    return 1;
  }

  const int duplicate_fd = fcntl(memfd, F_DUPFD_CLOEXEC, 0);
  if (duplicate_fd < 0 || !send_packet(sockets[0], packet.data(), packet_size, duplicate_fd) ||
      server.process_once() != metaflux::transport::vfio_user::ServerResult::Replied ||
      !receive_completion(sockets[0], 7U, MF_VFIO_USER_MESSAGE_DMA_MAP_V0,
                          MF_SHARED_INVALID_ARGUMENT)) {
    close(memfd);
    if (duplicate_fd >= 0) {
      close(duplicate_fd);
    }
    return 1;
  }
  close(duplicate_fd);

  mf_transport_message_header_v0 reset{};
  reset.message_id = 7U;
  reset.message_type = MF_VFIO_USER_MESSAGE_RESET_V0;
  reset.payload_size = 0U;
  if (!send_packet(sockets[0], reinterpret_cast<const std::uint8_t*>(&reset), sizeof(reset)) ||
      server.process_once() != metaflux::transport::vfio_user::ServerResult::Replied ||
      !receive_completion(sockets[0], 7U, MF_VFIO_USER_MESSAGE_RESET_V0, MF_SHARED_NOT_SUPPORTED)) {
    close(memfd);
    return 1;
  }

  mf_vfio_user_dma_unmap_v0 unmap{};
  unmap.struct_size = sizeof(unmap);
  unmap.iova = map.iova;
  unmap.size = map.size;
  unmap.mapping_epoch = map.mapping_epoch;
  unmap.device_generation = map.device_generation;
  if (mf_vfio_user_guest_encode_dma_unmap_v0(7U, &unmap, MF_TRANSPORT_FLAG_NO_REPLY_V0,
                                             packet.data(), packet.size(),
                                             &packet_size) != MF_SHARED_SUCCESS ||
      !send_packet(sockets[0], packet.data(), packet_size) ||
      server.process_once() != metaflux::transport::vfio_user::ServerResult::NoReply ||
      server.mapping_count() != 0U || server.retired_mapping_count() != 1U ||
      server.dma_lookup(0x1200U, 0x100U, MF_VFIO_USER_DMA_READ_V0) ||
      server.mapped_bytes() != 0x1000U) {
    close(memfd);
    return 1;
  }

  if (!server.dma_release(lease) || server.retired_mapping_count() != 1U ||
      server.mapped_bytes() != 0U || server.dma_release(lease)) {
    close(memfd);
    return 1;
  }

  if (mf_vfio_user_guest_encode_dma_unmap_v0(12U, &unmap, UINT16_C(0), packet.data(), packet.size(),
                                             &packet_size) != MF_SHARED_SUCCESS ||
      !send_packet(sockets[0], packet.data(), packet_size) ||
      server.process_once() != metaflux::transport::vfio_user::ServerResult::Replied ||
      !receive_completion(sockets[0], 12U, MF_VFIO_USER_MESSAGE_DMA_UNMAP_V0, MF_SHARED_SUCCESS) ||
      server.retired_mapping_count() != 0U || server.mapping_count() != 0U ||
      server.mapped_bytes() != 0U) {
    close(memfd);
    return 1;
  }

  if (mf_vfio_user_guest_encode_dma_unmap_v0(8U, &unmap, UINT16_C(0), packet.data(), packet.size(),
                                             &packet_size) != MF_SHARED_SUCCESS ||
      !send_packet(sockets[0], packet.data(), packet_size) ||
      server.process_once() != metaflux::transport::vfio_user::ServerResult::Replied ||
      !receive_completion(sockets[0], 8U, MF_VFIO_USER_MESSAGE_DMA_UNMAP_V0,
                          MF_SHARED_STALE_HANDLE)) {
    close(memfd);
    return 1;
  }

  const std::uint8_t short_packet = 0U;
  if (!send_packet(sockets[0], &short_packet, sizeof(short_packet)) ||
      server.process_once() != metaflux::transport::vfio_user::ServerResult::Malformed ||
      server.state() != metaflux::transport::vfio_user::ServerState::Configuring) {
    close(memfd);
    return 1;
  }

  mf_transport_message_header_v0 malformed_header{};
  malformed_header.message_id = 9U;
  malformed_header.message_type = MF_VFIO_USER_MESSAGE_GET_INFO_V0;
  malformed_header.flags = UINT16_C(0x8000);
  if (!send_packet(sockets[0], reinterpret_cast<const std::uint8_t*>(&malformed_header),
                   sizeof(malformed_header)) ||
      server.process_once() != metaflux::transport::vfio_user::ServerResult::Malformed) {
    close(memfd);
    return 1;
  }
  malformed_header.flags = 0U;
  malformed_header.message_id = 10U;
  malformed_header.payload_size = 1U;
  if (!send_packet(sockets[0], reinterpret_cast<const std::uint8_t*>(&malformed_header),
                   sizeof(malformed_header)) ||
      server.process_once() != metaflux::transport::vfio_user::ServerResult::Malformed) {
    close(memfd);
    return 1;
  }

  malformed_header.payload_size = 0U;
  if (!send_packet(sockets[0], reinterpret_cast<const std::uint8_t*>(&malformed_header),
                   sizeof(malformed_header)) ||
      server.process_once() != metaflux::transport::vfio_user::ServerResult::Replied ||
      !receive_info(sockets[0], 10U, &info)) {
    close(memfd);
    return 1;
  }

  mf_vfio_user_dma_map_v0 overflow_map = map;
  overflow_map.iova = UINT64_MAX - UINT64_C(0x0fff);
  overflow_map.size = UINT64_C(0x1000);
  overflow_map.mapping_epoch = 1U;
  overflow_map.device_generation = 1U;
  if (mf_vfio_user_guest_encode_dma_map_v0(11U, &map, packet.data(), packet.size(),
                                           &packet_size) != MF_SHARED_SUCCESS) {
    close(memfd);
    return 1;
  }
  (void)std::memcpy(packet.data() + sizeof(mf_transport_message_header_v0), &overflow_map,
                    sizeof(overflow_map));
  if (!send_packet(sockets[0], packet.data(), packet_size, memfd) ||
      server.process_once() != metaflux::transport::vfio_user::ServerResult::Replied ||
      !receive_completion(sockets[0], 11U, MF_VFIO_USER_MESSAGE_DMA_MAP_V0,
                          MF_SHARED_INVALID_ARGUMENT) ||
      server.mapping_count() != 0U) {
    close(memfd);
    return 1;
  }

  metaflux::runtime::lifecycle::Config lifecycle_config{};
  lifecycle_config.logical_device_id = 7U;
  lifecycle_config.daemon_incarnation = 11U;
  lifecycle_config.initial_identity_record_id = 1U;
  lifecycle_config.initial_generation = 1U;
  lifecycle_config.initial_epoch = 1U;
  lifecycle_config.generation_terminal = 32U;
  lifecycle_config.identity_record_terminal = 32U;
  lifecycle_config.epoch_terminal = 32U;
  metaflux::runtime::lifecycle::Coordinator coordinator(lifecycle_config);
  const metaflux::runtime::lifecycle::Request reset_request{
      .request_id = 8U,
      .logical_device_id = 7U,
      .daemon_incarnation = 11U,
      .expected_identity_record_id = 1U,
      .expected_generation = 1U,
      .expected_epoch = 1U,
      .source = metaflux::runtime::lifecycle::Source::VfioUser,
      .operation = metaflux::runtime::lifecycle::Operation::Reset,
  };
  if (!server.attach_lifecycle(coordinator) ||
      coordinator.apply(reset_request) != metaflux::runtime::lifecycle::Result::Accepted ||
      server.device_generation() != 2U || server.mapping_epoch() != 2U ||
      !server.lifecycle_online() ||
      server.state() != metaflux::transport::vfio_user::ServerState::Configuring) {
    close(memfd);
    return 1;
  }

  map.device_generation = 1U;
  map.mapping_epoch = 1U;
  if (mf_vfio_user_guest_encode_dma_map_v0(8U, &map, packet.data(), packet.size(), &packet_size) !=
          MF_SHARED_SUCCESS ||
      !send_packet(sockets[0], packet.data(), packet_size, memfd) ||
      server.process_once() != metaflux::transport::vfio_user::ServerResult::Replied ||
      !receive_completion(sockets[0], 8U, MF_VFIO_USER_MESSAGE_DMA_MAP_V0,
                          MF_SHARED_STALE_HANDLE)) {
    close(memfd);
    return 1;
  }
  metaflux::runtime::lifecycle::ResultDetails disconnect_details{};
  close(sockets[0]);
  sockets[0] = -1;
  if (server.process_once(coordinator, 9U, 0U, disconnect_details) !=
          metaflux::transport::vfio_user::ServerResult::Closed ||
      server.state() != metaflux::transport::vfio_user::ServerState::Lost ||
      disconnect_details.result != metaflux::runtime::lifecycle::Result::Accepted ||
      disconnect_details.snapshot.state != metaflux::runtime::lifecycle::State::Lost) {
    close(memfd);
    close(sockets[1]);
    return 1;
  }
  close(memfd);
  if (sockets[0] >= 0) {
    close(sockets[0]);
  }
  close(sockets[1]);
  return 0;
}
