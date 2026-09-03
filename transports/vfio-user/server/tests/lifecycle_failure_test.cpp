#include <metaflux/transport/vfio_user_guest.h>
#include <metaflux/transport/vfio_user_server.hpp>

#include <array>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <linux/memfd.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <unistd.h>

using metaflux::runtime::lifecycle::Config;
using metaflux::runtime::lifecycle::Coordinator;
using metaflux::runtime::lifecycle::ExternalEventKind;
using metaflux::runtime::lifecycle::Operation;
using metaflux::runtime::lifecycle::ProducerIngress;
using metaflux::runtime::lifecycle::Request;
using metaflux::runtime::lifecycle::Result;
using metaflux::runtime::lifecycle::ResultDetails;
using metaflux::runtime::lifecycle::Source;
using metaflux::runtime::lifecycle::State;
using metaflux::transport::vfio_user::DmaLease;
using metaflux::transport::vfio_user::ServerResult;
using metaflux::transport::vfio_user::ServerState;
using metaflux::transport::vfio_user::VfioUserServer;

#define REQUIRE(cond)                                                                                 \
  do {                                                                                                \
    if (!(cond)) {                                                                                    \
      std::cerr << __func__ << ':' << __LINE__ << ": " #cond "\n";                                   \
      return false;                                                                                   \
    }                                                                                                 \
  } while (false)

namespace {

static bool send_packet(int fd, const std::uint8_t* data, std::uint32_t size, int pfd = -1) {
  struct iovec iov{const_cast<std::uint8_t*>(data), size};
  struct msghdr msg{};
  std::array<std::uint8_t, CMSG_SPACE(sizeof(int))> cbuf{};
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;
  if (pfd >= 0) {
    msg.msg_control = cbuf.data();
    msg.msg_controllen = cbuf.size();
    auto* cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_len = CMSG_LEN(sizeof(int));
    std::memcpy(CMSG_DATA(cmsg), &pfd, sizeof(pfd));
  }
  return ::sendmsg(fd, &msg, MSG_NOSIGNAL) == static_cast<ssize_t>(size);
}

static bool recv_completion(int fd, std::uint64_t mid, std::uint16_t rtype, std::int32_t status) {
  std::array<std::uint8_t,
             sizeof(mf_transport_message_header_v0) + sizeof(mf_transport_completion_v0)>
      buf{};
  const ssize_t r = ::recv(fd, buf.data(), buf.size(), 0);
  mf_transport_completion_v0 comp{};
  return r == static_cast<ssize_t>(buf.size()) &&
         mf_vfio_user_guest_decode_completion_v0(buf.data(), static_cast<std::uint32_t>(r), mid,
                                                  rtype, &comp) == MF_SHARED_SUCCESS &&
         comp.status == static_cast<std::uint32_t>(status);
}

static bool recv_negotiate(int fd, std::uint64_t mid) {
  std::array<std::uint8_t,
             sizeof(mf_transport_message_header_v0) + sizeof(mf_transport_negotiate_v0)>
      buf{};
  const ssize_t r = ::recv(fd, buf.data(), buf.size(), 0);
  mf_transport_negotiate_v0 neg{};
  return r == static_cast<ssize_t>(buf.size()) &&
         mf_vfio_user_guest_decode_negotiate_v0(buf.data(), static_cast<std::uint32_t>(r), mid,
                                                 &neg) == MF_SHARED_SUCCESS;
}

static int make_memfd() {
  const long fd = ::syscall(SYS_memfd_create, "metaflux-lifecycle-failure", MFD_CLOEXEC);
  if (fd < 0 || fd > __INT_MAX__ || ::ftruncate(static_cast<int>(fd), 8192) != 0) {
    if (fd >= 0) {
      ::close(static_cast<int>(fd));
    }
    return -1;
  }
  return static_cast<int>(fd);
}

static Config make_config() {
  Config c{};
  c.logical_device_id = 7U;
  c.daemon_incarnation = 11U;
  c.initial_identity_record_id = 1U;
  c.initial_generation = 1U;
  c.initial_epoch = 1U;
  c.generation_terminal = 32U;
  c.identity_record_terminal = 32U;
  c.epoch_terminal = 32U;
  return c;
}

static bool negotiate(int gfd, VfioUserServer& server) {
  std::array<std::uint8_t, MF_VFIO_USER_MAX_PACKET_SIZE_V0> pkt{};
  std::uint32_t sz = 0U;
  mf_transport_negotiate_v0 neg{};
  neg.magic = MF_TRANSPORT_MAGIC_V0;
  neg.major = MF_TRANSPORT_MAJOR_V0;
  neg.minor = MF_TRANSPORT_MINOR_V0;
  neg.struct_size = sizeof(neg);
  neg.required_features =
      MF_TRANSPORT_FEATURE_VFIO_USER_V0 | MF_TRANSPORT_FEATURE_SHARED_MEMORY_V0;
  return mf_vfio_user_guest_encode_negotiate_v0(1U, &neg, pkt.data(), pkt.size(), &sz) ==
             MF_SHARED_SUCCESS &&
         send_packet(gfd, pkt.data(), sz) &&
         server.process_once() == ServerResult::Replied && recv_negotiate(gfd, 1U);
}

static bool map_dma(int gfd, VfioUserServer& server, int memfd, std::uint64_t msg_id,
                    std::uint64_t iova, std::uint64_t gen, std::uint64_t epoch = 1U) {
  std::array<std::uint8_t, MF_VFIO_USER_MAX_PACKET_SIZE_V0> pkt{};
  std::uint32_t sz = 0U;
  mf_vfio_user_dma_map_v0 m{};
  m.struct_size = sizeof(m);
  m.flags = MF_VFIO_USER_DMA_READ_V0 | MF_VFIO_USER_DMA_WRITE_V0;
  m.iova = iova;
  m.size = 4096U;
  m.fd_index = 0;
  m.mapping_epoch = epoch;
  m.device_generation = gen;
  return mf_vfio_user_guest_encode_dma_map_v0(msg_id, &m, pkt.data(), pkt.size(), &sz) ==
             MF_SHARED_SUCCESS &&
         send_packet(gfd, pkt.data(), sz, memfd) &&
         server.process_once() == ServerResult::Replied &&
         recv_completion(gfd, msg_id, MF_VFIO_USER_MESSAGE_DMA_MAP_V0, MF_SHARED_SUCCESS);
}

// DMA revocation on socket disconnect: map DMA, close guest socket, verify
// server transitions to Lost and all DMA is revoked.
bool dma_revocation_on_disconnect() {
  int sp[2]{-1, -1};
  REQUIRE(::socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0, sp) == 0);
  VfioUserServer server(sp[1]);
  Coordinator coordinator(make_config());
  REQUIRE(server.attach_lifecycle(coordinator));
  REQUIRE(negotiate(sp[0], server));
  int memfd = make_memfd();
  REQUIRE(memfd >= 0);
  REQUIRE(map_dma(sp[0], server, memfd, 10U, 0x1000U, 1U));
  REQUIRE(server.mapping_count() == 1U);
  DmaLease lease{};
  REQUIRE(server.dma_acquire(0x1000U, 4096U, MF_VFIO_USER_DMA_READ_V0, lease));
  // Close guest socket while lease is active.
  ::close(sp[0]);
  sp[0] = -1;
  ResultDetails details{};
  ProducerIngress ingress(coordinator);
  REQUIRE(server.process_once(ingress, 0U, details) == ServerResult::Closed);
  REQUIRE(server.state() == ServerState::Lost);
  REQUIRE(details.result == Result::Accepted);
  REQUIRE(details.snapshot.state == State::Lost);
  // DMA acquire should fail after loss.
  DmaLease post_lease{};
  REQUIRE(!server.dma_acquire(0x1000U, 4096U, MF_VFIO_USER_DMA_READ_V0, post_lease));
  (void)server.dma_release(lease);
  ::close(memfd);
  if (sp[0] >= 0) {
    ::close(sp[0]);
  }
  ::close(sp[1]);
  return true;
}

// Stale generation after lifecycle reset: negotiate, do a lifecycle Reset,
// verify old-gen DMA map is rejected and new-gen succeeds.
bool stale_generation_rejected_after_reset() {
  int sp[2]{-1, -1};
  REQUIRE(::socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0, sp) == 0);
  VfioUserServer server(sp[1]);
  Coordinator coordinator(make_config());
  REQUIRE(server.attach_lifecycle(coordinator));
  REQUIRE(negotiate(sp[0], server));
  int memfd = make_memfd();
  REQUIRE(memfd >= 0);
  // Lifecycle Reset (no DMA mappings, so drain completes).
  Request reset{};
  reset.request_id = 21U;
  reset.logical_device_id = 7U;
  reset.daemon_incarnation = 11U;
  reset.expected_identity_record_id = 1U;
  reset.expected_generation = 1U;
  reset.expected_epoch = 1U;
  reset.source = Source::Admin;
  reset.operation = Operation::Reset;
  ResultDetails details{};
  REQUIRE(coordinator.apply(reset, details) == Result::Accepted);
  REQUIRE(server.device_generation() == 2U);
  // Old-gen DMA map should be rejected.
  std::array<std::uint8_t, MF_VFIO_USER_MAX_PACKET_SIZE_V0> pkt{};
  std::uint32_t sz = 0U;
  mf_vfio_user_dma_map_v0 old_map{};
  old_map.struct_size = sizeof(old_map);
  old_map.flags = MF_VFIO_USER_DMA_READ_V0;
  old_map.iova = 0x2000U;
  old_map.size = 4096U;
  old_map.fd_index = 0;
  old_map.mapping_epoch = 1U;
  old_map.device_generation = 1U;
  REQUIRE(mf_vfio_user_guest_encode_dma_map_v0(22U, &old_map, pkt.data(), pkt.size(), &sz) ==
          MF_SHARED_SUCCESS);
  REQUIRE(send_packet(sp[0], pkt.data(), sz, memfd));
  REQUIRE(server.process_once() == ServerResult::Replied);
  REQUIRE(recv_completion(sp[0], 22U, MF_VFIO_USER_MESSAGE_DMA_MAP_V0, MF_SHARED_STALE_HANDLE));
  // New-gen DMA map should succeed (gen=2, epoch=2).
  REQUIRE(map_dma(sp[0], server, memfd, 23U, 0x3000U, 2U, 2U));
  ::close(memfd);
  ::close(sp[0]);
  ::close(sp[1]);
  return true;
}

// New-gen DMA maps succeed after lifecycle reset.
bool new_gen_maps_succeed_after_reset() {
  int sp[2]{-1, -1};
  REQUIRE(::socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0, sp) == 0);
  VfioUserServer server(sp[1]);
  Coordinator coordinator(make_config());
  REQUIRE(server.attach_lifecycle(coordinator));
  REQUIRE(negotiate(sp[0], server));
  int memfd = make_memfd();
  REQUIRE(memfd >= 0);
  // Reset.
  Request reset{};
  reset.request_id = 33U;
  reset.logical_device_id = 7U;
  reset.daemon_incarnation = 11U;
  reset.expected_identity_record_id = 1U;
  reset.expected_generation = 1U;
  reset.expected_epoch = 1U;
  reset.source = Source::Admin;
  reset.operation = Operation::Reset;
  ResultDetails details{};
  REQUIRE(coordinator.apply(reset, details) == Result::Accepted);
  REQUIRE(server.device_generation() == 2U);
  // New-gen maps should succeed (gen=2, epoch=2).
  REQUIRE(map_dma(sp[0], server, memfd, 30U, 0x1000U, 2U, 2U));
  REQUIRE(map_dma(sp[0], server, memfd, 31U, 0x2000U, 2U, 2U));
  REQUIRE(server.mapping_count() == 2U);
  // Acquire on mapped region works.
  DmaLease lease{};
  REQUIRE(server.dma_acquire(0x1000U, 4096U, MF_VFIO_USER_DMA_READ_V0, lease));
  (void)server.dma_release(lease);
  ::close(memfd);
  ::close(sp[0]);
  ::close(sp[1]);
  return true;
}

} // namespace

int main() {
  return dma_revocation_on_disconnect() && stale_generation_rejected_after_reset() &&
                 new_gen_maps_succeed_after_reset()
             ? 0
             : 1;
}
