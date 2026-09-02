#include <metaflux/transport/vfio_user_guest.h>

#include <array>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <string>
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

bool receive_info(int fd, mf_vfio_user_get_info_reply_v0* out) {
  std::array<std::uint8_t,
             sizeof(mf_transport_message_header_v0) + sizeof(mf_vfio_user_get_info_reply_v0)>
      response{};
  const ssize_t received = ::recv(fd, response.data(), response.size(), 0);
  return received == static_cast<ssize_t>(response.size()) &&
         mf_vfio_user_guest_decode_get_info_v0(
             response.data(), static_cast<std::uint32_t>(received), 41U, out) == MF_SHARED_SUCCESS;
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
  mf_vfio_user_get_info_reply_v0 info{};
  std::array<std::uint8_t, MF_VFIO_USER_MAX_PACKET_SIZE_V0> request{};
  std::uint32_t request_size = 0U;
  const char* failed_stage = nullptr;
  bool valid = wait_for_socket(socket_path.c_str(), &client);
  if (!valid) {
    failed_stage = "connect";
  }
  if (valid && mf_vfio_user_guest_encode_get_info_v0(41U, request.data(), request.size(),
                                                     &request_size) != MF_SHARED_SUCCESS) {
    valid = false;
    failed_stage = "encode";
  }
  if (valid && ::send(client, request.data(), request_size, MSG_NOSIGNAL) !=
                   static_cast<ssize_t>(request_size)) {
    valid = false;
    failed_stage = "send";
  }
  if (valid && !receive_info(client, &info)) {
    valid = false;
    failed_stage = "receive";
  }
  if (valid && (info.bar0_size != UINT64_C(65536) || info.bar2_size != UINT64_C(4096) ||
                info.bar4_size != UINT64_C(4096) || info.msix_vectors != 2U ||
                info.doorbell_width != 4U)) {
    valid = false;
    failed_stage = "profile";
  }
  if (client >= 0) {
    (void)::close(client);
  }
  (void)::kill(child, SIGTERM);
  int status = 0;
  if (::waitpid(child, &status, 0) != child || !WIFEXITED(status) || WEXITSTATUS(status) != 0) {
    valid = false;
    failed_stage = "child-exit";
  }
  struct stat socket_stat{};
  if (::lstat(socket_path.c_str(), &socket_stat) == 0 || errno != ENOENT) {
    valid = false;
    failed_stage = "socket-cleanup";
  }
  (void)::rmdir(directory);
  if (!valid) {
    std::cerr << "vfio-userd service test failed at "
              << (failed_stage == nullptr ? "unknown" : failed_stage) << '\n';
    return 1;
  }
  std::cout << "vfio-userd service: ok\n";
  return 0;
}
