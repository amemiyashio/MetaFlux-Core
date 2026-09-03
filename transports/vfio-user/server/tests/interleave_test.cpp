#include <metaflux/transport/vfio_user_guest.h>
#include <metaflux/transport/vfio_user_server.hpp>
#include <array>
#include <cstring>
#include <linux/memfd.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <unistd.h>

static bool send_packet(int fd, const std::uint8_t* bytes, std::uint32_t size, int pfd = -1) {
  struct iovec v{const_cast<std::uint8_t*>(bytes), size};
  struct msghdr m{};
  std::array<std::uint8_t, CMSG_SPACE(sizeof(int))> c{};
  m.msg_iov = &v; m.msg_iovlen = 1;
  if (pfd >= 0) {
    m.msg_control = c.data(); m.msg_controllen = c.size();
    auto* h = CMSG_FIRSTHDR(&m);
    h->cmsg_level = SOL_SOCKET; h->cmsg_type = SCM_RIGHTS;
    h->cmsg_len = CMSG_LEN(sizeof(int));
    std::memcpy(CMSG_DATA(h), &pfd, sizeof(pfd));
  }
  return ::sendmsg(fd, &m, MSG_NOSIGNAL) == static_cast<ssize_t>(size);
}

static bool recv_completion(int fd, uint64_t mid, uint16_t rtype, int32_t status) {
  std::array<uint8_t, sizeof(mf_transport_message_header_v0) + sizeof(mf_transport_completion_v0)> p{};
  ssize_t r = ::recv(fd, p.data(), p.size(), 0);
  mf_transport_completion_v0 c{};
  return r == static_cast<ssize_t>(p.size()) &&
         mf_vfio_user_guest_decode_completion_v0(p.data(), static_cast<uint32_t>(r),
                                                 mid, rtype, &c) == MF_SHARED_SUCCESS &&
         c.status == static_cast<uint32_t>(status);
}

static bool recv_info(int fd, uint64_t mid) {
  std::array<uint8_t, sizeof(mf_transport_message_header_v0) + sizeof(mf_vfio_user_get_info_reply_v0)> p{};
  ssize_t r = ::recv(fd, p.data(), p.size(), 0);
  mf_vfio_user_get_info_reply_v0 info{};
  return r == static_cast<ssize_t>(p.size()) &&
         mf_vfio_user_guest_decode_get_info_v0(p.data(), static_cast<uint32_t>(r), mid, &info) == MF_SHARED_SUCCESS;
}

static int make_memfd() {
  long fd = syscall(SYS_memfd_create, "metaflux-interleave", MFD_CLOEXEC);
  if (fd < 0 || fd > __INT_MAX__ || ftruncate(static_cast<int>(fd), 8192) != 0) {
    if (fd >= 0) close(static_cast<int>(fd));
    return -1;
  }
  return static_cast<int>(fd);
}

static bool recv_negotiate(int fd, uint64_t mid) {
  std::array<uint8_t, sizeof(mf_transport_message_header_v0) + sizeof(mf_transport_negotiate_v0)> p{};
  ssize_t r = ::recv(fd, p.data(), p.size(), 0);
  mf_transport_negotiate_v0 neg{};
  return r == static_cast<ssize_t>(p.size()) &&
         mf_vfio_user_guest_decode_negotiate_v0(p.data(), static_cast<uint32_t>(r), mid, &neg) == MF_SHARED_SUCCESS;
}

static bool negotiate(int gfd, metaflux::transport::vfio_user::VfioUserServer& server) {
  std::array<uint8_t, MF_VFIO_USER_MAX_PACKET_SIZE_V0> pkt{};
  uint32_t sz = 0U;
  mf_transport_negotiate_v0 neg{};
  neg.magic = MF_TRANSPORT_MAGIC_V0; neg.major = MF_TRANSPORT_MAJOR_V0;
  neg.minor = MF_TRANSPORT_MINOR_V0; neg.struct_size = sizeof(neg);
  neg.required_features = MF_TRANSPORT_FEATURE_VFIO_USER_V0 | MF_TRANSPORT_FEATURE_SHARED_MEMORY_V0;
  if (mf_vfio_user_guest_encode_negotiate_v0(1U, &neg, pkt.data(), pkt.size(), &sz) != MF_SHARED_SUCCESS ||
      !send_packet(gfd, pkt.data(), sz) ||
      server.process_once() != metaflux::transport::vfio_user::ServerResult::Replied ||
      !recv_negotiate(gfd, 1U) ||
      mf_vfio_user_guest_encode_get_info_v0(2U, pkt.data(), pkt.size(), &sz) != MF_SHARED_SUCCESS ||
      !send_packet(gfd, pkt.data(), sz) ||
      server.process_once() != metaflux::transport::vfio_user::ServerResult::Replied ||
      !recv_info(gfd, 2U) ||
      server.state() != metaflux::transport::vfio_user::ServerState::Configuring)
    return false;
  return true;
}

static bool test_interleaved_dma_ordering() {
  constexpr std::size_t N = 4U;
  int sp[2]; if (::socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0, sp) != 0) return false;
  metaflux::transport::vfio_user::ServerConfig cfg{}; cfg.max_bytes = 0x20000U;
  metaflux::transport::vfio_user::VfioUserServer server(sp[1], cfg);
  if (!negotiate(sp[0], server)) { close(sp[0]); close(sp[1]); return false; }
  int memfd = make_memfd();
  if (memfd < 0) { close(sp[0]); close(sp[1]); return false; }
  std::array<uint8_t, MF_VFIO_USER_MAX_PACKET_SIZE_V0> pkt{}; uint32_t sz = 0U;
  for (std::size_t i = 0U; i < N; ++i) {
    mf_vfio_user_dma_map_v0 m{}; m.struct_size = sizeof(m);
    m.flags = MF_VFIO_USER_DMA_READ_V0 | MF_VFIO_USER_DMA_WRITE_V0;
    m.iova = 0x10000U + i * 0x1000U; m.size = 0x1000U;
    m.mapping_epoch = 1U; m.device_generation = 1U; m.fd_index = 0;
    if (mf_vfio_user_guest_encode_dma_map_v0(100U + i, &m, pkt.data(), pkt.size(), &sz) != MF_SHARED_SUCCESS ||
        !send_packet(sp[0], pkt.data(), sz, memfd) ||
        server.process_once() != metaflux::transport::vfio_user::ServerResult::Replied ||
        !recv_completion(sp[0], 100U + i, MF_VFIO_USER_MESSAGE_DMA_MAP_V0, MF_SHARED_SUCCESS))
      { close(memfd); close(sp[0]); close(sp[1]); return false; }
  }
  bool ok = server.mapping_count() == N;
  close(memfd); close(sp[0]); close(sp[1]);
  return ok;
}

static bool test_no_reply_then_replied() {
  int sp[2]; if (::socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0, sp) != 0) return false;
  metaflux::transport::vfio_user::VfioUserServer server(sp[1]);
  if (!negotiate(sp[0], server)) { close(sp[0]); close(sp[1]); return false; }
  std::array<uint8_t, MF_VFIO_USER_MAX_PACKET_SIZE_V0> nr{}, r{}; uint32_t sz = 0U;
  if (mf_vfio_user_guest_encode_get_info_v0(50U, nr.data(), nr.size(), &sz) != MF_SHARED_SUCCESS)
    { close(sp[0]); close(sp[1]); return false; }
  reinterpret_cast<mf_transport_message_header_v0*>(nr.data())->flags = MF_TRANSPORT_FLAG_NO_REPLY_V0;
  if (!send_packet(sp[0], nr.data(), sz) ||
      mf_vfio_user_guest_encode_get_info_v0(51U, r.data(), r.size(), &sz) != MF_SHARED_SUCCESS ||
      !send_packet(sp[0], r.data(), sz))
    { close(sp[0]); close(sp[1]); return false; }
  bool ok = server.process_once() == metaflux::transport::vfio_user::ServerResult::NoReply &&
            server.process_once() == metaflux::transport::vfio_user::ServerResult::Replied &&
            recv_info(sp[0], 51U);
  close(sp[0]); close(sp[1]);
  return ok;
}

int main() {
  return (!test_interleaved_dma_ordering() || !test_no_reply_then_replied()) ? 1 : 0;
}
