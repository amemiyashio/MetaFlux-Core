#include <metaflux/transport/vfio_user_server.hpp>
#include <array>
#include <cstdint>
#include <cstring>
#include <sys/socket.h>
#include <unistd.h>

namespace {
using R = metaflux::transport::vfio_user::ServerResult;
using SRV = metaflux::transport::vfio_user::VfioUserServer;
bool pkt(int fd, const void* d, std::size_t n) {
  iovec iov{const_cast<void*>(d), n};
  msghdr msg{};
  msg.msg_iov = &iov; msg.msg_iovlen = 1;
  return ::sendmsg(fd, &msg, MSG_NOSIGNAL) == static_cast<ssize_t>(n);
}
void drain(int fd) { std::uint8_t b[512]; while (::recv(fd, b, sizeof(b), MSG_DONTWAIT) > 0) {} }
bool ok(R r) { return r == R::Idle || r == R::Replied || r == R::NoReply || r == R::Closed || r == R::Malformed; }
int mk(SRV*& s) {
  int sv[2];
  if (socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0, sv) != 0) return -1;
  s = new SRV(sv[1]);
  if (s->state() == metaflux::transport::vfio_user::ServerState::Lost) { delete s; s = nullptr; close(sv[0]); return -1; }
  return sv[0];
}
int test_random_sizes() {
  static const std::size_t sz[] = {0, 1, 7, 15, 63, 255, 4095};
  for (auto n : sz) {
    SRV* s = nullptr; int fd = mk(s); if (fd < 0) return 1;
    std::array<std::uint8_t, 512> d{};
    for (std::size_t i = 0; i < n && i < d.size(); ++i) d[i] = static_cast<std::uint8_t>(i * 37 + 13);
    std::size_t sz_send = n < d.size() ? n : d.size();
    if (!pkt(fd, d.data(), sz_send)) { delete s; close(fd); return 1; }
    R r = s->process_once(); delete s; close(fd); if (!ok(r)) return 1;
  }
  return 0;
}
int test_malformed_headers() {
  SRV* s = nullptr; int fd = mk(s); if (fd < 0) return 1;
  pkt(fd, "", 0);
  if (!ok(s->process_once())) { delete s; close(fd); return 1; }
  drain(fd);
  std::uint8_t one = 0xff; pkt(fd, &one, 1);
  if (!ok(s->process_once())) { delete s; close(fd); return 1; }
  drain(fd);
  mf_transport_message_header_v0 h{}; h.message_type = MF_VFIO_USER_MESSAGE_GET_INFO_V0;
  pkt(fd, &h, sizeof(h));
  if (!ok(s->process_once())) { delete s; close(fd); return 1; }
  delete s; close(fd); return 0;
}
int test_invalid_flags() {
  SRV* s = nullptr; int fd = mk(s); if (fd < 0) return 1;
  mf_transport_message_header_v0 h{};
  h.message_id = 1; h.message_type = MF_VFIO_USER_MESSAGE_GET_INFO_V0;
  h.flags = UINT16_C(0x8000); pkt(fd, &h, sizeof(h));
  if (!ok(s->process_once())) { delete s; close(fd); return 1; }
  drain(fd);
  h.flags = UINT16_C(0x7fff); h.message_id = 2; pkt(fd, &h, sizeof(h));
  if (!ok(s->process_once())) { delete s; close(fd); return 1; }
  delete s; close(fd); return 0;
}
int test_payload_mismatch() {
  SRV* s = nullptr; int fd = mk(s); if (fd < 0) return 1;
  mf_transport_message_header_v0 h{};
  h.message_id = 1; h.message_type = MF_VFIO_USER_MESSAGE_GET_INFO_V0; h.payload_size = 100;
  pkt(fd, &h, sizeof(h));
  if (!ok(s->process_once())) { delete s; close(fd); return 1; }
  drain(fd);
  std::array<std::uint8_t, sizeof(h) + 32> big{};
  h.payload_size = 0; h.message_id = 2;
  std::memcpy(big.data(), &h, sizeof(h));
  std::memset(big.data() + sizeof(h), 0xaa, 32);
  pkt(fd, big.data(), big.size());
  if (!ok(s->process_once())) { delete s; close(fd); return 1; }
  delete s; close(fd); return 0;
}
int test_unknown_types() {
  SRV* s = nullptr; int fd = mk(s); if (fd < 0) return 1;
  for (std::uint16_t t = 0; t <= 10; ++t) {
    mf_transport_message_header_v0 h{}; h.message_id = t + 1; h.message_type = t;
    pkt(fd, &h, sizeof(h));
    if (!ok(s->process_once())) { delete s; close(fd); return 1; }
    drain(fd);
  }
  delete s; close(fd); return 0;
}
int test_burst() {
  SRV* s = nullptr; int fd = mk(s); if (fd < 0) return 1;
  for (int i = 0; i < 64; ++i) {
    std::array<std::uint8_t, 64> p{};
    for (std::size_t j = 0; j < p.size(); ++j) p[j] = static_cast<std::uint8_t>(static_cast<std::size_t>(i) * 7 + j * 31);
    if (!pkt(fd, p.data(), p.size())) break;
    if (!ok(s->process_once())) { delete s; close(fd); return 1; }
  }
  delete s; close(fd); return 0;
}
int test_close_mid_session() {
  SRV* s = nullptr; int fd = mk(s); if (fd < 0) return 1;
  close(fd);
  R r = s->process_once(); delete s;
  return ok(r) && r == R::Closed ? 0 : 1;
}
} // namespace
int main() {
  if (test_random_sizes()) return 1;
  if (test_malformed_headers()) return 2;
  if (test_invalid_flags()) return 3;
  if (test_payload_mismatch()) return 4;
  if (test_unknown_types()) return 5;
  if (test_burst()) return 6;
  if (test_close_mid_session()) return 7;
  return 0;
}
