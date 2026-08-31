#include "metaflux/transport/qmp_lifecycle.hpp"

#include <array>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <string_view>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

using metaflux::transport::vfio_user::QmpReply;
using metaflux::transport::vfio_user::QmpReplyKind;
using metaflux::transport::vfio_user::QmpSocket;
using metaflux::transport::vfio_user::QmpSocketResult;
using metaflux::transport::vfio_user::QmpWireKind;
using metaflux::transport::vfio_user::QmpWireMessage;

#define REQUIRE(condition)                                                                         \
  do {                                                                                             \
    if (!(condition)) {                                                                            \
      std::cerr << __func__ << ':' << __LINE__ << ": " #condition "\n";                        \
      return false;                                                                                \
    }                                                                                              \
  } while (false)

namespace {

bool write_all(int fd, std::string_view value) {
  std::size_t offset = 0U;
  while (offset < value.size()) {
    const ssize_t written = ::send(fd, value.data() + offset, value.size() - offset, MSG_NOSIGNAL);
    if (written > 0) {
      offset += static_cast<std::size_t>(written);
    } else if (written < 0 && errno == EINTR) {
      continue;
    } else {
      return false;
    }
  }
  return true;
}

bool read_command(int fd, std::string& out) {
  out.clear();
  bool started = false;
  bool escaped = false;
  bool in_string = false;
  std::size_t depth = 0U;
  while (out.size() < QmpSocket::kMaximumMessageBytes) {
    char byte = 0;
    const ssize_t received = ::recv(fd, &byte, sizeof(byte), 0);
    if (received <= 0) {
      return false;
    }
    if (!started) {
      if (byte == '{') {
        started = true;
        depth = 1U;
      } else {
        continue;
      }
    } else if (in_string) {
      if (escaped) {
        escaped = false;
      } else if (byte == '\\') {
        escaped = true;
      } else if (byte == '"') {
        in_string = false;
      }
    } else if (byte == '"') {
      in_string = true;
    } else if (byte == '{') {
      ++depth;
    } else if (byte == '}' && --depth == 0U) {
      out.push_back(byte);
      return true;
    }
    out.push_back(byte);
  }
  return false;
}

bool socket_round_trip() {
  int sockets[2] = {-1, -1};
  REQUIRE(::socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sockets) == 0);
  QmpSocket client(sockets[0]);
  sockets[0] = -1;

  REQUIRE(write_all(sockets[1], "{\"QMP\":{\"version\":{},\"capabilities\":[]}}"));
  QmpWireMessage message{};
  REQUIRE(client.receive(message) == QmpSocketResult::Ok && message.kind == QmpWireKind::Greeting);

  REQUIRE(client.send_command(7U, "qmp_capabilities", "{}") == QmpSocketResult::Ok);
  std::string command;
  REQUIRE(read_command(sockets[1], command));
  REQUIRE(command.find("\"execute\":\"qmp_capabilities\"") != std::string::npos &&
          command.find("\"arguments\":{}") != std::string::npos &&
          command.find("\"id\":7") != std::string::npos);
  REQUIRE(write_all(sockets[1], "{\"return\":{},\"id\":7}"));
  REQUIRE(client.receive(message) == QmpSocketResult::Ok && message.kind == QmpWireKind::Reply &&
          message.command_id == 7U);

  REQUIRE(write_all(sockets[1], "{\"event\":\"DEVICE_ADDED\",\"data\":{}}"));
  QmpReply reply{};
  REQUIRE(client.receive_lifecycle_reply(7U, reply) == QmpSocketResult::Ok &&
          reply.command_id == 7U && reply.kind == QmpReplyKind::DeviceAdded);

  REQUIRE(write_all(sockets[1], "{\"event\":\"DEVICE_DELETED\",\"data\":{}}"));
  REQUIRE(client.receive_lifecycle_reply(7U, reply) == QmpSocketResult::Ok &&
          reply.kind == QmpReplyKind::DeviceDeleted);

  REQUIRE(write_all(sockets[1], "{\"error\":{\"class\":\"GenericError\"},\"id\":7}"));
  REQUIRE(client.receive_lifecycle_reply(7U, reply) == QmpSocketResult::Ok &&
          reply.kind == QmpReplyKind::CommandFailed);

  REQUIRE(write_all(sockets[1], "{\"event\":\"RESET\",\"data\":{}}"));
  REQUIRE(client.receive_lifecycle_reply(7U, reply) == QmpSocketResult::Unexpected);

  REQUIRE(write_all(sockets[1], "{\"return\":{}}"));
  REQUIRE(client.receive(message) == QmpSocketResult::Malformed);
  ::close(sockets[1]);
  return true;
}

bool rejects_invalid_commands() {
  int sockets[2] = {-1, -1};
  REQUIRE(::socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sockets) == 0);
  QmpSocket client(sockets[0]);
  sockets[0] = -1;
  REQUIRE(client.send_command(0U, "qmp_capabilities") == QmpSocketResult::Invalid);
  REQUIRE(client.send_command(1U, "bad\"command") == QmpSocketResult::Invalid);
  REQUIRE(client.send_command(1U, "qmp_capabilities", "[]") == QmpSocketResult::Invalid);
  REQUIRE(client.send_command(1U, "qmp_capabilities", "{bad}") == QmpSocketResult::Invalid);
  QmpReply reply{};
  REQUIRE(client.receive_lifecycle_reply(0U, reply) == QmpSocketResult::Invalid);
  ::close(sockets[1]);
  return true;
}

bool connects_to_unix_path() {
  char path[] = "/tmp/metaflux-qmp-XXXXXX";
  const int placeholder = ::mkstemp(path);
  if (placeholder < 0) {
    return false;
  }
  (void)::close(placeholder);
  (void)::unlink(path);

  const int listener = ::socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
  if (listener < 0) {
    return false;
  }
  sockaddr_un address{};
  address.sun_family = AF_UNIX;
  std::memcpy(address.sun_path, path, std::strlen(path) + 1U);
  bool ok = ::bind(listener, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) == 0 &&
            ::listen(listener, 1) == 0;
  QmpSocket client;
  if (ok) {
    ok = QmpSocket::connect(path, client, 100U) == QmpSocketResult::Ok;
  }
  const int peer = ok ? ::accept(listener, nullptr, nullptr) : -1;
  ok = ok && peer >= 0;
  if (peer >= 0) {
    (void)::close(peer);
  }
  (void)::close(listener);
  (void)::unlink(path);
  return ok;
}

} // namespace

int main() {
  return socket_round_trip() && rejects_invalid_commands() && connects_to_unix_path() ? 0 : 1;
}
