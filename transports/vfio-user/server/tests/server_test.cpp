#include <metaflux/transport/vfio_user_guest.h>
#include <metaflux/transport/vfio_user_server.hpp>

#include <array>
#include <cerrno>
#include <climits>
#include <cstdint>
#include <cstring>
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

int make_memfd() {
  const long fd = syscall(SYS_memfd_create, "metaflux-vfio-test", MFD_CLOEXEC);
  if (fd < 0 || fd > INT_MAX || ftruncate(static_cast<int>(fd), 4096) != 0) {
    if (fd >= 0 && fd <= INT_MAX) {
      (void)close(static_cast<int>(fd));
    }
    return -1;
  }
  return static_cast<int>(fd);
}

} // namespace

int main() {
  int sockets[2] = {-1, -1};
  if (socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0, sockets) != 0) {
    return 1;
  }
  metaflux::transport::vfio_user::VfioUserServer server(sockets[1]);
  std::array<std::uint8_t, MF_VFIO_USER_MAX_PACKET_SIZE_V0> packet{};
  std::uint32_t packet_size = 0;

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
      server.mapping_count() != 1U) {
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
      server.mapping_count() != 0U) {
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
  if (mf_vfio_user_guest_encode_dma_map_v0(11U, &overflow_map, packet.data(), packet.size(),
                                           &packet_size) != MF_SHARED_SUCCESS ||
      !send_packet(sockets[0], packet.data(), packet_size, memfd) ||
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
