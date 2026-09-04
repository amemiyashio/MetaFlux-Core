#include <metaflux/shared/device.h>
#include <metaflux/transport/vfio_user_guest.h>
#include <metaflux/transport/vfio_user_profile.h>

#include <array>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <linux/memfd.h>
#include <string>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

namespace {

bool connect_to(const char* path, int* out_fd) {
  const int fd = ::socket(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0);
  if (fd < 0) {
    return false;
  }
  sockaddr_un address{};
  address.sun_family = AF_UNIX;
  const std::size_t length = std::strlen(path);
  if (length >= sizeof(address.sun_path)) {
    (void)::close(fd);
    return false;
  }
  std::memcpy(address.sun_path, path, length + 1U);
  if (::connect(fd, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) != 0) {
    (void)::close(fd);
    return false;
  }
  *out_fd = fd;
  return true;
}

bool wait_for_socket(const char* path, int* out_fd) {
  for (unsigned int attempt = 0U; attempt < 100U; ++attempt) {
    if (connect_to(path, out_fd)) {
      return true;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  return false;
}

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
                        std::int32_t expected_status) {
  std::array<std::uint8_t,
             sizeof(mf_transport_message_header_v0) + sizeof(mf_transport_completion_v0)>
      packet{};
  const ssize_t received = ::recv(fd, packet.data(), packet.size(), 0);
  mf_transport_completion_v0 completion{};
  return received == static_cast<ssize_t>(packet.size()) &&
         mf_vfio_user_guest_decode_completion_v0(
             packet.data(), static_cast<std::uint32_t>(received), message_id, request_type,
             &completion) == MF_SHARED_SUCCESS &&
         completion.status == static_cast<std::uint32_t>(expected_status);
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
  *out = info;
  return true;
}

bool receive_negotiate(int fd, std::uint64_t message_id) {
  std::array<std::uint8_t,
             sizeof(mf_transport_message_header_v0) + sizeof(mf_transport_negotiate_v0)>
      packet{};
  const ssize_t received = ::recv(fd, packet.data(), packet.size(), 0);
  mf_transport_negotiate_v0 negotiation{};
  return received == static_cast<ssize_t>(packet.size()) &&
         mf_vfio_user_guest_decode_negotiate_v0(
             packet.data(), static_cast<std::uint32_t>(received), message_id, &negotiation) ==
             MF_SHARED_SUCCESS;
}

int make_memfd(std::size_t size) {
  const int fd = static_cast<int>(::syscall(SYS_memfd_create, "metaflux-vfio-userd-guest", 0));
  if (fd < 0 || ::ftruncate(fd, static_cast<off_t>(size)) != 0) {
    if (fd >= 0) {
      (void)::close(fd);
    }
    return -1;
  }
  return fd;
}

void record_doorbell(void* context, std::uint32_t value) {
  auto* counter = static_cast<std::uint32_t*>(context);
  *counter += 1U;
  (void)value;
}

bool prove_copy_on_mapped_guest_ram(std::uint8_t* guest_ram, std::size_t guest_ram_size) {
  if (guest_ram == nullptr || guest_ram_size < 64U) {
    return false;
  }
  const mf_registry_view_id_v1 view_id{UINT64_C(7), UINT64_C(13)};
  mf_client_ring_v1 submission_owner{};
  mf_client_ring_v1 completion_owner{};
  mf_vfio_user_guest_ring_v0 guest{};
  submission_owner.owned_fd = -1;
  completion_owner.owned_fd = -1;
  guest.submission.owned_fd = -1;
  guest.completion.owned_fd = -1;

  std::uint32_t doorbell_count = 0U;
  if (mf_client_ring_create_v1(UINT32_C(4), view_id, MF_CLIENT_SUBMISSION_QUEUE_ID_V1, UINT64_C(5),
                               &submission_owner) != MF_SHARED_SUCCESS ||
      mf_client_ring_create_v1(UINT32_C(4), view_id, MF_CLIENT_COMPLETION_QUEUE_ID_V1, UINT64_C(5),
                               &completion_owner) != MF_SHARED_SUCCESS ||
      mf_vfio_user_guest_ring_attach_v0(mf_client_ring_borrow_fd_v1(&submission_owner),
                                        mf_client_ring_borrow_fd_v1(&completion_owner), view_id,
                                        UINT64_C(5), guest_ram, 64U, record_doorbell,
                                        &doorbell_count, UINT32_C(0x10), &guest) !=
          MF_SHARED_SUCCESS) {
    mf_vfio_user_guest_ring_close_v0(&guest);
    mf_client_ring_close_v1(&completion_owner);
    mf_client_ring_close_v1(&submission_owner);
    return false;
  }

  // Guest-visible payload lives inside the DMA-mapped guest RAM memfd.
  guest_ram[0] = 0x11U;
  guest_ram[1] = 0x22U;
  guest_ram[2] = 0x33U;
  guest_ram[3] = 0x44U;

  mf_ring_descriptor_v1 desc{};
  desc.opcode = MF_RING_OPCODE_COPY;
  desc.request_id = UINT64_C(10);
  desc.target_id = UINT64_C(20);
  // arguments[0]=src offset, arguments[1]=dst offset, arguments[2]=length in the
  // mapped guest-RAM arena owned by the production DMA map.
  desc.arguments[0] = UINT64_C(0);
  desc.arguments[1] = UINT64_C(16);
  desc.arguments[2] = UINT64_C(4);
  if (mf_vfio_user_guest_ring_submit_v0(&guest, &desc) != MF_SHARED_SUCCESS ||
      doorbell_count != 1U) {
    mf_vfio_user_guest_ring_close_v0(&guest);
    mf_client_ring_close_v1(&completion_owner);
    mf_client_ring_close_v1(&submission_owner);
    return false;
  }

  mf_ring_descriptor_v1 consumed{};
  if (mf_client_ring_try_consume_v1(&submission_owner, &consumed) != MF_SHARED_SUCCESS ||
      consumed.opcode != MF_RING_OPCODE_COPY || consumed.request_id != UINT64_C(10) ||
      consumed.arguments[0] != UINT64_C(0) || consumed.arguments[1] != UINT64_C(16) ||
      consumed.arguments[2] != UINT64_C(4)) {
    mf_vfio_user_guest_ring_close_v0(&guest);
    mf_client_ring_close_v1(&completion_owner);
    mf_client_ring_close_v1(&submission_owner);
    return false;
  }

  // Server-side CPU COPY over the mapped guest RAM payload.
  std::memcpy(guest_ram + static_cast<std::size_t>(consumed.arguments[1]),
              guest_ram + static_cast<std::size_t>(consumed.arguments[0]),
              static_cast<std::size_t>(consumed.arguments[2]));
  if (guest_ram[16] != 0x11U || guest_ram[17] != 0x22U || guest_ram[18] != 0x33U ||
      guest_ram[19] != 0x44U) {
    mf_vfio_user_guest_ring_close_v0(&guest);
    mf_client_ring_close_v1(&completion_owner);
    mf_client_ring_close_v1(&submission_owner);
    return false;
  }

  mf_ring_descriptor_v1 completion{};
  completion.opcode = MF_RING_OPCODE_COMPLETION;
  completion.request_id = UINT64_C(10);
  completion.target_id = UINT64_C(20);
  completion.arguments[1] = UINT64_C(1);
  if (mf_client_ring_try_submit_v1(&completion_owner, &completion) != MF_SHARED_SUCCESS) {
    mf_vfio_user_guest_ring_close_v0(&guest);
    mf_client_ring_close_v1(&completion_owner);
    mf_client_ring_close_v1(&submission_owner);
    return false;
  }

  mf_ring_descriptor_v1 guest_completion{};
  if (mf_vfio_user_guest_ring_try_consume_v0(&guest, &guest_completion) != MF_SHARED_SUCCESS ||
      guest_completion.opcode != MF_RING_OPCODE_COMPLETION ||
      guest_completion.request_id != UINT64_C(10) ||
      mf_vfio_user_guest_ring_last_completion_timeline_v0(&guest) != UINT64_C(1) ||
      guest_ram[16] != 0x11U || guest_ram[17] != 0x22U || guest_ram[18] != 0x33U ||
      guest_ram[19] != 0x44U) {
    mf_vfio_user_guest_ring_close_v0(&guest);
    mf_client_ring_close_v1(&completion_owner);
    mf_client_ring_close_v1(&submission_owner);
    return false;
  }

  mf_vfio_user_guest_ring_close_v0(&guest);
  mf_client_ring_close_v1(&completion_owner);
  mf_client_ring_close_v1(&submission_owner);
  return true;
}

} // namespace

int main(int argc, char** argv) {
  if (argc != 2) {
    return 64;
  }
  char directory_template[] = "/tmp/metaflux-vfio-userd-test-XXXXXX";
  char* directory = ::mkdtemp(directory_template);
  if (directory == nullptr) {
    return 1;
  }
  const std::string socket_path = std::string(directory) + "/control.sock";
  const pid_t child = ::fork();
  if (child < 0) {
    (void)::rmdir(directory);
    return 1;
  }
  if (child == 0) {
    ::execl(argv[1], argv[1], "--socket", socket_path.c_str(), static_cast<char*>(nullptr));
    _exit(127);
  }

  int client = -1;
  int memfd = -1;
  void* guest_ram = MAP_FAILED;
  constexpr std::size_t kGuestRamSize = 4096U;
  mf_vfio_user_get_info_reply_v0 info{};
  std::array<std::uint8_t, MF_VFIO_USER_MAX_PACKET_SIZE_V0> request{};
  std::uint32_t request_size = 0U;
  const char* failed_stage = nullptr;
  bool valid = wait_for_socket(socket_path.c_str(), &client);
  if (!valid) {
    failed_stage = "connect";
  }

  mf_transport_negotiate_v0 negotiation{};
  negotiation.magic = MF_TRANSPORT_MAGIC_V0;
  negotiation.major = MF_TRANSPORT_MAJOR_V0;
  negotiation.minor = MF_TRANSPORT_MINOR_V0;
  negotiation.struct_size = sizeof(negotiation);
  negotiation.required_features =
      MF_TRANSPORT_FEATURE_VFIO_USER_V0 | MF_TRANSPORT_FEATURE_SHARED_MEMORY_V0;
  if (valid &&
      (mf_vfio_user_guest_encode_negotiate_v0(40U, &negotiation, request.data(), request.size(),
                                              &request_size) != MF_SHARED_SUCCESS ||
       !send_packet(client, request.data(), request_size) || !receive_negotiate(client, 40U))) {
    valid = false;
    failed_stage = "negotiate";
  }

  if (valid && mf_vfio_user_guest_encode_get_info_v0(41U, request.data(), request.size(),
                                                     &request_size) != MF_SHARED_SUCCESS) {
    valid = false;
    failed_stage = "encode-info";
  }
  if (valid && !send_packet(client, request.data(), request_size)) {
    valid = false;
    failed_stage = "send-info";
  }
  if (valid && !receive_info(client, 41U, &info)) {
    valid = false;
    failed_stage = "receive-info";
  }
  if (valid &&
      (info.bar0_size != MF_VFIO_USER_PROFILE_BAR0_SIZE ||
       info.bar2_size != MF_VFIO_USER_PROFILE_BAR2_SIZE ||
       info.bar4_size != MF_VFIO_USER_PROFILE_BAR4_SIZE ||
       info.msix_vectors != MF_VFIO_USER_PROFILE_MSIX_VECTORS ||
       info.doorbell_width != MF_VFIO_USER_PROFILE_DOORBELL_WIDTH)) {
    valid = false;
    failed_stage = "profile";
  }

  if (valid) {
    memfd = make_memfd(kGuestRamSize);
    guest_ram = memfd < 0 ? MAP_FAILED
                          : ::mmap(nullptr, kGuestRamSize, PROT_READ | PROT_WRITE, MAP_SHARED,
                                   memfd, 0);
    if (memfd < 0 || guest_ram == MAP_FAILED) {
      valid = false;
      failed_stage = "guest-ram";
    }
  }

  mf_vfio_user_dma_map_v0 map{};
  map.struct_size = sizeof(map);
  map.flags = MF_VFIO_USER_DMA_READ_V0 | MF_VFIO_USER_DMA_WRITE_V0;
  map.iova = UINT64_C(0x1000);
  map.size = kGuestRamSize;
  map.mapping_epoch = 1U;
  map.device_generation = 1U;
  map.fd_index = 0;
  if (valid &&
      (mf_vfio_user_guest_encode_dma_map_v0(42U, &map, request.data(), request.size(),
                                            &request_size) != MF_SHARED_SUCCESS ||
       !send_packet(client, request.data(), request_size, memfd) ||
       !receive_completion(client, 42U, MF_VFIO_USER_MESSAGE_DMA_MAP_V0, MF_SHARED_SUCCESS))) {
    valid = false;
    failed_stage = "dma-map";
  }

  if (valid &&
      !prove_copy_on_mapped_guest_ram(static_cast<std::uint8_t*>(guest_ram), kGuestRamSize)) {
    valid = false;
    failed_stage = "guest-ram-copy";
  }

  // milestone-0.1.1.0 freezes unsupported reset: production path never succeeds.
  mf_transport_message_header_v0 reset{};
  reset.message_id = 43U;
  reset.message_type = MF_VFIO_USER_MESSAGE_RESET_V0;
  reset.payload_size = 0U;
  if (valid &&
      (!send_packet(client, reinterpret_cast<const std::uint8_t*>(&reset), sizeof(reset)) ||
       !receive_completion(client, 43U, MF_VFIO_USER_MESSAGE_RESET_V0, MF_SHARED_NOT_SUPPORTED))) {
    valid = false;
    failed_stage = "reset-frozen";
  }

  if (guest_ram != MAP_FAILED) {
    (void)::munmap(guest_ram, kGuestRamSize);
  }
  if (memfd >= 0) {
    (void)::close(memfd);
  }
  if (client >= 0) {
    (void)::close(client);
  }
  (void)::kill(child, SIGTERM);
  int status = 0;
  if (::waitpid(child, &status, 0) != child || !WIFEXITED(status) || WEXITSTATUS(status) != 0) {
    valid = false;
    if (failed_stage == nullptr) {
      failed_stage = "child-exit";
    }
  }
  struct stat socket_stat{};
  if (::lstat(socket_path.c_str(), &socket_stat) == 0 || errno != ENOENT) {
    valid = false;
    if (failed_stage == nullptr) {
      failed_stage = "socket-cleanup";
    }
  }
  (void)::rmdir(directory);
  if (!valid) {
    std::cerr << "vfio-userd service test failed at "
              << (failed_stage == nullptr ? "unknown" : failed_stage) << '\n';
    return 1;
  }
  std::cout << "vfio-userd service: negotiate/map/guest-ram-copy/reset-frozen ok\n";
  return 0;
}
