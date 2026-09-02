#include "metaflux/transport/qmp_lifecycle.hpp"

#include <cctype>
#include <cerrno>
#include <cstring>
#include <limits>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>
#include <unistd.h>

namespace metaflux::transport::vfio_user {
namespace {

constexpr std::size_t kMaximumJsonDepth = 128U;

QmpSocketResult map_errno(int error) noexcept {
  switch (error) {
  case EAGAIN:
#if EWOULDBLOCK != EAGAIN
  case EWOULDBLOCK:
#endif
    return QmpSocketResult::WouldBlock;
  case EINTR:
    return QmpSocketResult::WouldBlock;
  case ETIMEDOUT:
    return QmpSocketResult::Timeout;
  case ECONNRESET:
  case EPIPE:
    return QmpSocketResult::Closed;
  default:
    return QmpSocketResult::IoError;
  }
}

bool is_space(char value) noexcept { return std::isspace(static_cast<unsigned char>(value)) != 0; }

void skip_space(const char* bytes, std::size_t size, std::size_t& cursor) noexcept {
  while (cursor < size && is_space(bytes[cursor])) {
    ++cursor;
  }
}

bool parse_string(const char* bytes, std::size_t size, std::size_t& cursor,
                  std::size_t* out_begin = nullptr, std::size_t* out_end = nullptr) noexcept {
  if (cursor >= size || bytes[cursor] != '"') {
    return false;
  }
  ++cursor;
  const std::size_t begin = cursor;
  bool escaped = false;
  while (cursor < size) {
    const char value = bytes[cursor];
    if (value == '\0' || static_cast<unsigned char>(value) < 0x20U) {
      return false;
    }
    if (escaped) {
      if (value != '"' && value != '\\' && value != '/' && value != 'b' && value != 'f' &&
          value != 'n' && value != 'r' && value != 't' && value != 'u') {
        return false;
      }
      if (value == 'u') {
        if (size - cursor < 5U) {
          return false;
        }
        for (std::size_t index = 1U; index <= 4U; ++index) {
          const unsigned char digit = static_cast<unsigned char>(bytes[cursor + index]);
          if (!((digit >= '0' && digit <= '9') || (digit >= 'a' && digit <= 'f') ||
                (digit >= 'A' && digit <= 'F'))) {
            return false;
          }
        }
        cursor += 4U;
      }
      escaped = false;
      ++cursor;
      continue;
    }
    if (value == '\\') {
      escaped = true;
      ++cursor;
      continue;
    }
    if (value == '"') {
      if (out_begin != nullptr) {
        *out_begin = begin;
      }
      if (out_end != nullptr) {
        *out_end = cursor;
      }
      ++cursor;
      return true;
    }
    ++cursor;
  }
  return false;
}

bool consume_literal(const char* bytes, std::size_t size, std::size_t& cursor,
                     std::string_view literal) noexcept {
  if (size - cursor < literal.size() ||
      std::memcmp(bytes + cursor, literal.data(), literal.size()) != 0) {
    return false;
  }
  cursor += literal.size();
  return true;
}

bool parse_number(const char* bytes, std::size_t size, std::size_t& cursor) noexcept {
  const std::size_t begin = cursor;
  if (cursor < size && bytes[cursor] == '-') {
    ++cursor;
  }
  if (cursor >= size) {
    return false;
  }
  if (bytes[cursor] == '0') {
    ++cursor;
    if (cursor < size && bytes[cursor] >= '0' && bytes[cursor] <= '9') {
      return false;
    }
  } else {
    if (bytes[cursor] < '1' || bytes[cursor] > '9') {
      return false;
    }
    do {
      ++cursor;
    } while (cursor < size && bytes[cursor] >= '0' && bytes[cursor] <= '9');
  }
  if (cursor < size && bytes[cursor] == '.') {
    ++cursor;
    const std::size_t fraction_begin = cursor;
    while (cursor < size && bytes[cursor] >= '0' && bytes[cursor] <= '9') {
      ++cursor;
    }
    if (cursor == fraction_begin) {
      return false;
    }
  }
  if (cursor < size && (bytes[cursor] == 'e' || bytes[cursor] == 'E')) {
    ++cursor;
    if (cursor < size && (bytes[cursor] == '+' || bytes[cursor] == '-')) {
      ++cursor;
    }
    const std::size_t exponent_begin = cursor;
    while (cursor < size && bytes[cursor] >= '0' && bytes[cursor] <= '9') {
      ++cursor;
    }
    if (cursor == exponent_begin) {
      return false;
    }
  }
  return cursor != begin;
}

bool skip_value(const char* bytes, std::size_t size, std::size_t& cursor,
                std::size_t depth = 0U) noexcept {
  if (depth > kMaximumJsonDepth) {
    return false;
  }
  skip_space(bytes, size, cursor);
  if (cursor >= size) {
    return false;
  }
  if (bytes[cursor] == '"') {
    return parse_string(bytes, size, cursor);
  }
  if (bytes[cursor] == '{' || bytes[cursor] == '[') {
    const char open = bytes[cursor++];
    const char close = open == '{' ? '}' : ']';
    skip_space(bytes, size, cursor);
    if (cursor < size && bytes[cursor] == close) {
      ++cursor;
      return true;
    }
    while (cursor < size) {
      if (open == '{' && !parse_string(bytes, size, cursor)) {
        return false;
      }
      skip_space(bytes, size, cursor);
      if (open == '{') {
        if (cursor >= size || bytes[cursor++] != ':') {
          return false;
        }
      }
      if (!skip_value(bytes, size, cursor, depth + 1U)) {
        return false;
      }
      skip_space(bytes, size, cursor);
      if (cursor >= size) {
        return false;
      }
      if (bytes[cursor] == close) {
        ++cursor;
        return true;
      }
      if (bytes[cursor++] != ',') {
        return false;
      }
      skip_space(bytes, size, cursor);
    }
    return false;
  }
  if (consume_literal(bytes, size, cursor, "true") ||
      consume_literal(bytes, size, cursor, "false") ||
      consume_literal(bytes, size, cursor, "null")) {
    return true;
  }
  return parse_number(bytes, size, cursor);
}

bool is_json_object(std::string_view value) noexcept {
  if (value.size() < 2U || value.front() != '{' || value.back() != '}') {
    return false;
  }
  std::size_t cursor = 0U;
  if (!skip_value(value.data(), value.size(), cursor) || cursor != value.size()) {
    return false;
  }
  return true;
}

bool key_is(const char* bytes, std::size_t begin, std::size_t end,
            std::string_view expected) noexcept {
  return end >= begin && end - begin == expected.size() &&
         std::memcmp(bytes + begin, expected.data(), expected.size()) == 0;
}

bool parse_id(const char* bytes, std::size_t size, std::size_t& cursor,
              std::uint64_t& out) noexcept {
  skip_space(bytes, size, cursor);
  if (cursor >= size || bytes[cursor] == '-' || bytes[cursor] == '+') {
    return false;
  }
  const std::size_t begin = cursor;
  std::uint64_t value = 0U;
  while (cursor < size && bytes[cursor] >= '0' && bytes[cursor] <= '9') {
    const std::uint64_t digit = static_cast<std::uint64_t>(bytes[cursor] - '0');
    if (value > (std::numeric_limits<std::uint64_t>::max() - digit) / 10U) {
      return false;
    }
    value = value * 10U + digit;
    ++cursor;
  }
  if (cursor == begin) {
    return false;
  }
  out = value;
  return true;
}

} // namespace

QmpSocket::~QmpSocket() { close(); }

QmpSocket::QmpSocket(QmpSocket&& other) noexcept : fd_(other.fd_) { other.fd_ = -1; }

QmpSocket& QmpSocket::operator=(QmpSocket&& other) noexcept {
  if (this != &other) {
    close();
    fd_ = other.fd_;
    other.fd_ = -1;
  }
  return *this;
}

QmpSocketResult QmpSocket::set_timeout(std::uint32_t timeout_ms) noexcept {
  if (fd_ < 0) {
    return QmpSocketResult::Invalid;
  }
  timeval timeout{};
  timeout.tv_sec = static_cast<time_t>(timeout_ms / 1000U);
  timeout.tv_usec = static_cast<suseconds_t>((timeout_ms % 1000U) * 1000U);
  if (::setsockopt(fd_, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) != 0 ||
      ::setsockopt(fd_, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) != 0) {
    return map_errno(errno);
  }
  return QmpSocketResult::Ok;
}

QmpSocketResult QmpSocket::connect(const char* path, QmpSocket& out,
                                   std::uint32_t timeout_ms) noexcept {
  sockaddr_un address{};
  if (path == nullptr || path[0] == '\0' || std::strlen(path) >= sizeof(address.sun_path)) {
    return QmpSocketResult::Invalid;
  }
  out.close();
  const int fd = ::socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
  if (fd < 0) {
    return map_errno(errno);
  }
  out.fd_ = fd;
  const auto timeout_result = out.set_timeout(timeout_ms);
  if (timeout_result != QmpSocketResult::Ok) {
    out.close();
    return timeout_result;
  }
  address.sun_family = AF_UNIX;
  std::memcpy(address.sun_path, path, std::strlen(path) + 1U);
  if (::connect(out.fd_, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) != 0) {
    const auto result = map_errno(errno);
    out.close();
    return result;
  }
  return QmpSocketResult::Ok;
}

QmpSocketResult QmpSocket::send_bytes(const char* bytes, std::size_t size) noexcept {
  if (fd_ < 0 || bytes == nullptr || size == 0U) {
    return QmpSocketResult::Invalid;
  }
  std::size_t sent_total = 0U;
  while (sent_total < size) {
    const ssize_t sent = ::send(fd_, bytes + sent_total, size - sent_total, MSG_NOSIGNAL);
    if (sent > 0) {
      sent_total += static_cast<std::size_t>(sent);
      continue;
    }
    if (sent < 0 && errno == EINTR) {
      continue;
    }
    return sent < 0 ? map_errno(errno) : QmpSocketResult::Closed;
  }
  return QmpSocketResult::Ok;
}

QmpSocketResult QmpSocket::send_command(std::uint64_t command_id, std::string_view execute,
                                        std::string_view arguments) noexcept {
  if (fd_ < 0 || command_id == 0U || execute.empty() || arguments.size() < 2U ||
      arguments.front() != '{' || arguments.back() != '}' ||
      execute.find_first_of("\\\"\n\r\t") != std::string_view::npos ||
      arguments.find('\0') != std::string_view::npos || !is_json_object(arguments)) {
    return QmpSocketResult::Invalid;
  }
  std::array<char, kMaximumMessageBytes> packet{};
  std::size_t size = 0U;
  const auto append = [&](std::string_view value) noexcept {
    if (value.size() > packet.size() - size) {
      return false;
    }
    std::memcpy(packet.data() + size, value.data(), value.size());
    size += value.size();
    return true;
  };
  const auto append_id = [&]() noexcept {
    char digits[32]{};
    std::size_t count = sizeof(digits);
    std::uint64_t value = command_id;
    do {
      digits[--count] = static_cast<char>('0' + (value % 10U));
      value /= 10U;
    } while (value != 0U);
    return append(std::string_view(digits + count, sizeof(digits) - count));
  };
  if (!append("{\"execute\":\"") || !append(execute) || !append("\",\"arguments\":") ||
      !append(arguments) || !append(",\"id\":") || !append_id() || !append("}")) {
    return QmpSocketResult::Invalid;
  }
  return send_bytes(packet.data(), size);
}

QmpSocketResult QmpSocket::read_json_object(std::array<char, kMaximumMessageBytes>& object,
                                            std::size_t& size) noexcept {
  size = 0U;
  if (fd_ < 0) {
    return QmpSocketResult::Invalid;
  }
  bool started = false;
  bool escaped = false;
  bool in_string = false;
  std::size_t depth = 0U;
  while (size < object.size()) {
    char byte = 0;
    const ssize_t received = ::recv(fd_, &byte, sizeof(byte), 0);
    if (received == 0) {
      return QmpSocketResult::Closed;
    }
    if (received < 0) {
      if (errno == EINTR) {
        continue;
      }
      return map_errno(errno);
    }
    if (!started) {
      if (is_space(byte)) {
        continue;
      }
      if (byte != '{') {
        return QmpSocketResult::Malformed;
      }
      started = true;
      depth = 1U;
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
      if (++depth > kMaximumJsonDepth) {
        return QmpSocketResult::Malformed;
      }
    } else if (byte == '}') {
      if (depth == 0U || --depth == 0U) {
        object[size++] = byte;
        return QmpSocketResult::Ok;
      }
    } else if (byte == '\0') {
      return QmpSocketResult::Malformed;
    }
    object[size++] = byte;
  }
  return QmpSocketResult::Malformed;
}

QmpSocketResult QmpSocket::classify_json(const char* bytes, std::size_t size,
                                         QmpWireMessage& out) noexcept {
  out = {};
  if (bytes == nullptr || size < 2U || bytes[0] != '{' || bytes[size - 1U] != '}') {
    return QmpSocketResult::Malformed;
  }
  bool has_return = false;
  bool has_error = false;
  bool has_qmp = false;
  bool has_event = false;
  bool id_present = false;
  std::uint64_t id = 0U;
  QmpWireKind event_kind = QmpWireKind::OtherEvent;
  std::size_t cursor = 1U;
  while (true) {
    skip_space(bytes, size, cursor);
    if (cursor >= size) {
      return QmpSocketResult::Malformed;
    }
    if (bytes[cursor] == '}') {
      ++cursor;
      break;
    }
    std::size_t key_begin = 0U;
    std::size_t key_end = 0U;
    if (!parse_string(bytes, size, cursor, &key_begin, &key_end)) {
      return QmpSocketResult::Malformed;
    }
    skip_space(bytes, size, cursor);
    if (cursor >= size || bytes[cursor++] != ':') {
      return QmpSocketResult::Malformed;
    }
    skip_space(bytes, size, cursor);
    if (key_is(bytes, key_begin, key_end, "id")) {
      if (id_present || !parse_id(bytes, size, cursor, id) || id == 0U) {
        return QmpSocketResult::Malformed;
      }
      id_present = true;
    } else if (key_is(bytes, key_begin, key_end, "event")) {
      std::size_t event_begin = 0U;
      std::size_t event_end = 0U;
      if (has_event || !parse_string(bytes, size, cursor, &event_begin, &event_end)) {
        return QmpSocketResult::Malformed;
      }
      has_event = true;
      if (key_is(bytes, event_begin, event_end, "DEVICE_ADDED")) {
        event_kind = QmpWireKind::DeviceAdded;
      } else if (key_is(bytes, event_begin, event_end, "DEVICE_DELETED")) {
        event_kind = QmpWireKind::DeviceDeleted;
      } else {
        event_kind = QmpWireKind::OtherEvent;
      }
    } else {
      if (key_is(bytes, key_begin, key_end, "return")) {
        if (has_return) {
          return QmpSocketResult::Malformed;
        }
        has_return = true;
      } else if (key_is(bytes, key_begin, key_end, "error")) {
        if (has_error) {
          return QmpSocketResult::Malformed;
        }
        has_error = true;
      } else if (key_is(bytes, key_begin, key_end, "QMP")) {
        if (has_qmp) {
          return QmpSocketResult::Malformed;
        }
        has_qmp = true;
      }
      if (!skip_value(bytes, size, cursor)) {
        return QmpSocketResult::Malformed;
      }
    }
    skip_space(bytes, size, cursor);
    if (cursor >= size) {
      return QmpSocketResult::Malformed;
    }
    if (bytes[cursor] == ',') {
      ++cursor;
      continue;
    }
    if (bytes[cursor] == '}') {
      ++cursor;
      break;
    }
    return QmpSocketResult::Malformed;
  }
  skip_space(bytes, size, cursor);
  if (cursor != size || (has_return && has_error) || (has_event && (has_return || has_error))) {
    return QmpSocketResult::Malformed;
  }
  if (has_event) {
    out.kind = event_kind;
    return QmpSocketResult::Ok;
  }
  if (has_error) {
    out.kind = QmpWireKind::CommandFailed;
    out.command_id = id_present ? id : 0U;
    return QmpSocketResult::Ok;
  }
  if (has_return) {
    if (!id_present) {
      return QmpSocketResult::Malformed;
    }
    out.kind = QmpWireKind::Reply;
    out.command_id = id;
    return QmpSocketResult::Ok;
  }
  if (has_qmp && !id_present) {
    out.kind = QmpWireKind::Greeting;
    return QmpSocketResult::Ok;
  }
  return QmpSocketResult::Malformed;
}

QmpSocketResult QmpSocket::receive(QmpWireMessage& out) noexcept {
  std::array<char, kMaximumMessageBytes> object{};
  std::size_t size = 0U;
  const auto result = read_json_object(object, size);
  if (result != QmpSocketResult::Ok) {
    out = {};
    return result;
  }
  return classify_json(object.data(), size, out);
}

QmpSocketResult QmpSocket::receive_lifecycle_reply(std::uint64_t pending_command_id,
                                                   QmpReply& out) noexcept {
  out = {};
  if (pending_command_id == 0U) {
    return QmpSocketResult::Invalid;
  }
  QmpWireMessage message{};
  const auto result = receive(message);
  if (result != QmpSocketResult::Ok) {
    return result;
  }
  if (message.kind == QmpWireKind::DeviceAdded || message.kind == QmpWireKind::DeviceDeleted) {
    out.command_id = pending_command_id;
    out.kind = message.kind == QmpWireKind::DeviceAdded ? QmpReplyKind::DeviceAdded
                                                        : QmpReplyKind::DeviceDeleted;
    return QmpSocketResult::Ok;
  }
  if (message.kind == QmpWireKind::CommandFailed) {
    if (message.command_id != 0U && message.command_id != pending_command_id) {
      return QmpSocketResult::Unexpected;
    }
    out.command_id = pending_command_id;
    out.kind = QmpReplyKind::CommandFailed;
    return QmpSocketResult::Ok;
  }
  if (message.kind == QmpWireKind::Reply && message.command_id == pending_command_id) {
    return QmpSocketResult::Unexpected;
  }
  return QmpSocketResult::Unexpected;
}

void QmpSocket::close() noexcept {
  if (fd_ >= 0) {
    (void)::close(fd_);
    fd_ = -1;
  }
}

} // namespace metaflux::transport::vfio_user
