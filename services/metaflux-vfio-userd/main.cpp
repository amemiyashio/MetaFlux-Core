#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <metaflux/transport/vfio_user_server.hpp>

#include <cerrno>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <poll.h>
#include <string>
#include <string_view>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#ifndef METAFLUX_PROJECT_VERSION
#error "METAFLUX_PROJECT_VERSION must be supplied by the build"
#endif

namespace {

volatile std::sig_atomic_t stop_requested = 0;

extern "C" void request_stop(int) { stop_requested = 1; }

std::string default_socket_path() {
  if (const char* configured = std::getenv("METAFLUX_VFIO_USER_SOCKET");
      configured != nullptr && configured[0] != '\0') {
    return configured;
  }
  if (const char* runtime_directory = std::getenv("XDG_RUNTIME_DIR");
      runtime_directory != nullptr && runtime_directory[0] != '\0') {
    return std::string(runtime_directory) + "/metaflux-vfio-userd.sock";
  }
  return "/run/user/" + std::to_string(static_cast<unsigned long long>(geteuid())) +
         "/metaflux-vfio-userd.sock";
}

bool same_file(const struct stat& left, const struct stat& right) {
  return left.st_dev == right.st_dev && left.st_ino == right.st_ino;
}

int create_listener(const std::string& path, struct stat* out_identity) {
  if (path.empty() || path.size() >= sizeof(sockaddr_un::sun_path) || out_identity == nullptr) {
    errno = ENAMETOOLONG;
    return -1;
  }
  const int listener = ::socket(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC | SOCK_NONBLOCK, 0);
  if (listener < 0) {
    return -1;
  }
  sockaddr_un address{};
  address.sun_family = AF_UNIX;
  std::memcpy(address.sun_path, path.c_str(), path.size() + 1U);
  if (::bind(listener, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) != 0 ||
      ::listen(listener, 16) != 0 || ::chmod(path.c_str(), 0600) != 0 ||
      ::lstat(path.c_str(), out_identity) != 0) {
    const int error = errno;
    (void)::close(listener);
    errno = error;
    return -1;
  }
  return listener;
}

void remove_owned_socket(const std::string& path, const struct stat& identity) {
  struct stat current{};
  if (::lstat(path.c_str(), &current) == 0 && same_file(current, identity)) {
    (void)::unlink(path.c_str());
  }
}

int serve_peer(int peer) {
  metaflux::transport::vfio_user::VfioUserServer server(peer);
  while (stop_requested == 0) {
    pollfd descriptor{.fd = peer, .events = POLLIN, .revents = 0};
    const int result = ::poll(&descriptor, 1U, 250);
    if (result < 0) {
      if (errno == EINTR) {
        continue;
      }
      return 1;
    }
    if (result == 0) {
      continue;
    }
    const auto outcome = server.process_once();
    if (outcome == metaflux::transport::vfio_user::ServerResult::Closed ||
        (outcome == metaflux::transport::vfio_user::ServerResult::Malformed &&
         server.state() == metaflux::transport::vfio_user::ServerState::Lost)) {
      return 0;
    }
  }
  return 0;
}

int serve(const std::string& path) {
  struct sigaction action{};
  action.sa_handler = request_stop;
  sigemptyset(&action.sa_mask);
  if (::sigaction(SIGINT, &action, nullptr) != 0 ||
      ::sigaction(SIGTERM, &action, nullptr) != 0) {
    std::cerr << "metaflux-vfio-userd: signal setup failed: " << std::strerror(errno) << '\n';
    return 1;
  }

  struct stat identity{};
  const int listener = create_listener(path, &identity);
  if (listener < 0) {
    std::cerr << "metaflux-vfio-userd: cannot bind " << path << ": " << std::strerror(errno)
              << '\n';
    return 1;
  }

  int result = 0;
  while (stop_requested == 0) {
    pollfd descriptor{.fd = listener, .events = POLLIN, .revents = 0};
    const int poll_result = ::poll(&descriptor, 1U, 250);
    if (poll_result < 0) {
      if (errno == EINTR) {
        continue;
      }
      result = 1;
      break;
    }
    if (poll_result == 0) {
      continue;
    }
    const int peer = ::accept4(listener, nullptr, nullptr, SOCK_CLOEXEC | SOCK_NONBLOCK);
    if (peer < 0) {
      if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) {
        continue;
      }
      result = 1;
      break;
    }
    if (serve_peer(peer) != 0) {
      result = 1;
    }
  }
  (void)::close(listener);
  remove_owned_socket(path, identity);
  return result;
}

} // namespace

int main(int argc, char** argv) {
  if (argc == 2 && std::string_view(argv[1]) == "--version") {
    std::cout << "metaflux-vfio-userd " << METAFLUX_PROJECT_VERSION << '\n';
    return 0;
  }
  if (argc == 3 && std::string_view(argv[1]) == "--socket") {
    return serve(argv[2]);
  }
  if (argc == 1) {
    return serve(default_socket_path());
  }
  std::cerr << "usage: metaflux-vfio-userd [--socket PATH | --version]\n";
  return 64;
}
