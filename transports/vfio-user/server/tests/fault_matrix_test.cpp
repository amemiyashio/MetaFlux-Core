// work-item-0.1.1.4: DMA boundary, in-flight unmap, dirty-unpin, ownership-death,
// and MSI-X storm soak matrix for the vfio-user server control fixture.
#include <metaflux/runtime/lifecycle.hpp>
#include <metaflux/transport/msix.hpp>
#include <metaflux/transport/vfio_user_guest.h>
#include <metaflux/transport/vfio_user_server.hpp>

#include <array>
#include <climits>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <linux/memfd.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <unistd.h>

namespace {

using metaflux::transport::vfio_user::DmaLease;
using metaflux::transport::vfio_user::ServerResult;
using metaflux::transport::vfio_user::ServerState;
using metaflux::transport::vfio_user::VfioUserServer;

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

int make_memfd(std::size_t size = 8192) {
  const long fd = ::syscall(SYS_memfd_create, "metaflux-fault-matrix", MFD_CLOEXEC);
  if (fd < 0 || fd > INT_MAX || ::ftruncate(static_cast<int>(fd), static_cast<off_t>(size)) != 0) {
    if (fd >= 0 && fd <= INT_MAX) {
      (void)::close(static_cast<int>(fd));
    }
    return -1;
  }
  return static_cast<int>(fd);
}

bool negotiate(int guest_fd, VfioUserServer& server, std::uint64_t message_id) {
  mf_transport_negotiate_v0 negotiation{};
  negotiation.magic = MF_TRANSPORT_MAGIC_V0;
  negotiation.major = MF_TRANSPORT_MAJOR_V0;
  negotiation.minor = MF_TRANSPORT_MINOR_V0;
  negotiation.struct_size = sizeof(negotiation);
  negotiation.required_features =
      MF_TRANSPORT_FEATURE_VFIO_USER_V0 | MF_TRANSPORT_FEATURE_SHARED_MEMORY_V0;
  std::array<std::uint8_t, MF_VFIO_USER_MAX_PACKET_SIZE_V0> packet{};
  std::uint32_t packet_size = 0U;
  return mf_vfio_user_guest_encode_negotiate_v0(message_id, &negotiation, packet.data(),
                                                packet.size(), &packet_size) == MF_SHARED_SUCCESS &&
         send_packet(guest_fd, packet.data(), packet_size) &&
         server.process_once() == ServerResult::Replied &&
         receive_negotiate(guest_fd, message_id);
}

bool map_region(int guest_fd, VfioUserServer& server, int memfd, std::uint64_t message_id,
                std::uint64_t iova, std::uint64_t size, std::uint32_t flags,
                std::uint64_t epoch = 1U, std::uint64_t generation = 1U,
                std::int32_t expected = MF_SHARED_SUCCESS) {
  mf_vfio_user_dma_map_v0 map{};
  map.struct_size = sizeof(map);
  map.flags = flags;
  map.iova = iova;
  map.size = size;
  map.mapping_epoch = epoch;
  map.device_generation = generation;
  map.fd_index = 0;
  std::array<std::uint8_t, MF_VFIO_USER_MAX_PACKET_SIZE_V0> packet{};
  std::uint32_t packet_size = 0U;
  return mf_vfio_user_guest_encode_dma_map_v0(message_id, &map, packet.data(), packet.size(),
                                              &packet_size) == MF_SHARED_SUCCESS &&
         send_packet(guest_fd, packet.data(), packet_size, memfd) &&
         server.process_once() == ServerResult::Replied &&
         receive_completion(guest_fd, message_id, MF_VFIO_USER_MESSAGE_DMA_MAP_V0, expected);
}

bool unmap_region(int guest_fd, VfioUserServer& server, std::uint64_t message_id, std::uint64_t iova,
                  std::uint64_t size, std::uint64_t epoch = 1U, std::uint64_t generation = 1U,
                  std::int32_t expected = MF_SHARED_SUCCESS) {
  mf_vfio_user_dma_unmap_v0 unmap{};
  unmap.struct_size = sizeof(unmap);
  unmap.iova = iova;
  unmap.size = size;
  unmap.mapping_epoch = epoch;
  unmap.device_generation = generation;
  std::array<std::uint8_t, MF_VFIO_USER_MAX_PACKET_SIZE_V0> packet{};
  std::uint32_t packet_size = 0U;
  return mf_vfio_user_guest_encode_dma_unmap_v0(message_id, &unmap, UINT16_C(0), packet.data(),
                                                packet.size(), &packet_size) == MF_SHARED_SUCCESS &&
         send_packet(guest_fd, packet.data(), packet_size) &&
         server.process_once() == ServerResult::Replied &&
         receive_completion(guest_fd, message_id, MF_VFIO_USER_MESSAGE_DMA_UNMAP_V0, expected);
}

bool dma_overlap_rejected() {
  int sockets[2] = {-1, -1};
  if (::socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0, sockets) != 0) {
    return false;
  }
  VfioUserServer server(sockets[1]);
  const int memfd = make_memfd();
  // Page-aligned ranges that still overlap: [0x1000,0x3000) vs [0x2000,0x3000).
  if (memfd < 0 || !negotiate(sockets[0], server, 1U) ||
      !map_region(sockets[0], server, memfd, 2U, 0x1000U, 0x2000U,
                  MF_VFIO_USER_DMA_READ_V0 | MF_VFIO_USER_DMA_WRITE_V0) ||
      !map_region(sockets[0], server, memfd, 3U, 0x2000U, 0x1000U,
                  MF_VFIO_USER_DMA_READ_V0 | MF_VFIO_USER_DMA_WRITE_V0,
                  1U, 1U, MF_SHARED_INVALID_ARGUMENT) ||
      server.mapping_count() != 1U) {
    if (memfd >= 0) {
      (void)::close(memfd);
    }
    (void)::close(sockets[0]);
    return false;
  }
  (void)::close(memfd);
  (void)::close(sockets[0]);
  return true;
}

bool dma_hole_exact_unmap_required() {
  // Partial/hole unmap of a live exact range must not succeed as a silent hole punch.
  int sockets[2] = {-1, -1};
  if (::socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0, sockets) != 0) {
    return false;
  }
  VfioUserServer server(sockets[1]);
  const int memfd = make_memfd();
  // Exact-range unmap only: page-aligned subranges that are not the full mapping
  // must remain STALE_HANDLE (no silent hole punch).
  if (memfd < 0 || !negotiate(sockets[0], server, 10U) ||
      !map_region(sockets[0], server, memfd, 11U, 0x2000U, 0x2000U,
                  MF_VFIO_USER_DMA_READ_V0 | MF_VFIO_USER_DMA_WRITE_V0) ||
      !unmap_region(sockets[0], server, 12U, 0x2000U, 0x1000U, 1U, 1U, MF_SHARED_STALE_HANDLE) ||
      !unmap_region(sockets[0], server, 13U, 0x3000U, 0x1000U, 1U, 1U, MF_SHARED_STALE_HANDLE) ||
      server.mapping_count() != 1U ||
      !unmap_region(sockets[0], server, 14U, 0x2000U, 0x2000U) ||
      server.mapping_count() != 0U) {
    if (memfd >= 0) {
      (void)::close(memfd);
    }
    (void)::close(sockets[0]);
    return false;
  }
  (void)::close(memfd);
  (void)::close(sockets[0]);
  return true;
}

bool dma_stale_epoch_and_generation_rejected() {
  int sockets[2] = {-1, -1};
  if (::socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0, sockets) != 0) {
    return false;
  }
  VfioUserServer server(sockets[1]);
  const int memfd = make_memfd();
  if (memfd < 0 || !negotiate(sockets[0], server, 20U) ||
      !map_region(sockets[0], server, memfd, 21U, 0x3000U, 0x1000U, MF_VFIO_USER_DMA_READ_V0, 2U,
                  1U, MF_SHARED_STALE_HANDLE) ||
      !map_region(sockets[0], server, memfd, 22U, 0x3000U, 0x1000U, MF_VFIO_USER_DMA_READ_V0, 1U,
                  9U, MF_SHARED_STALE_HANDLE) ||
      server.mapping_count() != 0U) {
    if (memfd >= 0) {
      (void)::close(memfd);
    }
    (void)::close(sockets[0]);
    return false;
  }
  (void)::close(memfd);
  (void)::close(sockets[0]);
  return true;
}

bool in_flight_unmap_blocks_until_lease_and_pin_drain() {
  int sockets[2] = {-1, -1};
  if (::socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0, sockets) != 0) {
    return false;
  }
  VfioUserServer server(sockets[1]);
  const int memfd = make_memfd();
  if (memfd < 0 || !negotiate(sockets[0], server, 30U) ||
      !map_region(sockets[0], server, memfd, 31U, 0x4000U, 0x1000U,
                  MF_VFIO_USER_DMA_READ_V0 | MF_VFIO_USER_DMA_WRITE_V0)) {
    if (memfd >= 0) {
      (void)::close(memfd);
    }
    (void)::close(sockets[0]);
    return false;
  }
  DmaLease lease{};
  if (!server.dma_acquire(0x4000U, 0x100U, MF_VFIO_USER_DMA_READ_V0, lease) ||
      !server.pin_longterm(0x4100U, 0x100U) || !server.mark_dirty(0x4200U, 0x40U) ||
      server.dirty_bytes() != 0x40U || server.pin_references() != 1U ||
      !unmap_region(sockets[0], server, 32U, 0x4000U, 0x1000U, 1U, 1U, MF_SHARED_WOULD_BLOCK) ||
      server.mapping_count() != 0U || server.retired_mapping_count() != 1U) {
    (void)::close(memfd);
    (void)::close(sockets[0]);
    return false;
  }
  if (!server.dma_release(lease) || server.retired_mapping_count() != 1U ||
      server.mapped_bytes() != 0x1000U || // pin still holds finalization
      !server.unpin_longterm(0x4100U, 0x100U) || server.pin_references() != 0U ||
      server.dirty_bytes() != 0U || server.mapped_bytes() != 0U ||
      !unmap_region(sockets[0], server, 33U, 0x4000U, 0x1000U) ||
      server.retired_mapping_count() != 0U) {
    (void)::close(memfd);
    (void)::close(sockets[0]);
    return false;
  }
  (void)::close(memfd);
  (void)::close(sockets[0]);
  return true;
}

bool client_death_marks_lost_and_blocks_stale_reuse() {
  int sockets[2] = {-1, -1};
  if (::socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0, sockets) != 0) {
    return false;
  }
  VfioUserServer server(sockets[1]);
  const int memfd = make_memfd();
  if (memfd < 0 || !negotiate(sockets[0], server, 40U) ||
      !map_region(sockets[0], server, memfd, 41U, 0x5000U, 0x1000U,
                  MF_VFIO_USER_DMA_READ_V0 | MF_VFIO_USER_DMA_WRITE_V0)) {
    if (memfd >= 0) {
      (void)::close(memfd);
    }
    (void)::close(sockets[0]);
    return false;
  }
  DmaLease lease{};
  if (!server.dma_acquire(0x5000U, 0x100U, MF_VFIO_USER_DMA_READ_V0, lease)) {
    (void)::close(memfd);
    (void)::close(sockets[0]);
    return false;
  }

  metaflux::runtime::lifecycle::Config config{};
  config.logical_device_id = 7U;
  config.daemon_incarnation = 11U;
  config.initial_identity_record_id = 1U;
  config.initial_generation = 1U;
  config.initial_epoch = 1U;
  config.generation_terminal = 32U;
  config.identity_record_terminal = 32U;
  config.epoch_terminal = 32U;
  metaflux::runtime::lifecycle::Coordinator coordinator(config);
  if (!server.attach_lifecycle(coordinator)) {
    (void)::close(memfd);
    (void)::close(sockets[0]);
    return false;
  }

  // Guest/client death: close the peer and drive the disconnect through the server.
  (void)::close(sockets[0]);
  sockets[0] = -1;
  metaflux::runtime::lifecycle::ResultDetails details{};
  metaflux::runtime::lifecycle::ProducerIngress ingress(coordinator);
  const auto result = server.process_once(ingress, 0U, details);
  if (result != ServerResult::Closed || server.state() != ServerState::Lost ||
      details.result != metaflux::runtime::lifecycle::Result::Accepted ||
      details.snapshot.state != metaflux::runtime::lifecycle::State::Lost) {
    if (memfd >= 0) {
      (void)::close(memfd);
    }
    return false;
  }

  DmaLease post{};
  if (server.dma_acquire(0x5000U, 0x100U, MF_VFIO_USER_DMA_READ_V0, post) ||
      server.dma_lookup(0x5000U, 0x100U, MF_VFIO_USER_DMA_READ_V0) ||
      server.mark_dirty(0x5000U, 0x10U) || server.pin_longterm(0x5000U, 0x10U)) {
    (void)server.dma_release(lease);
    (void)::close(memfd);
    return false;
  }
  (void)server.dma_release(lease);
  (void)::close(memfd);
  return true;
}

bool structured_dma_map_unmap_fuzz_is_defined() {
  // Structured fuzz: random-ish map/unmap payloads must never crash and must
  // return only defined ServerResult values.
  static constexpr auto defined = [](ServerResult result) noexcept {
    return result == ServerResult::Idle || result == ServerResult::Replied ||
           result == ServerResult::NoReply || result == ServerResult::Closed ||
           result == ServerResult::Malformed;
  };
  for (unsigned seed = 0U; seed < 48U; ++seed) {
    int sockets[2] = {-1, -1};
    if (::socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0, sockets) != 0) {
      return false;
    }
    VfioUserServer server(sockets[1]);
    if (!negotiate(sockets[0], server, 100U + seed)) {
      (void)::close(sockets[0]);
      return false;
    }
    std::array<std::uint8_t, sizeof(mf_transport_message_header_v0) + 160> packet{};
    mf_transport_message_header_v0 header{};
    header.message_id = 200U + seed;
    header.message_type =
        (seed % 2U == 0U) ? MF_VFIO_USER_MESSAGE_DMA_MAP_V0 : MF_VFIO_USER_MESSAGE_DMA_UNMAP_V0;
    header.payload_size = static_cast<std::uint32_t>(32U + (seed % 96U));
    std::memcpy(packet.data(), &header, sizeof(header));
    for (std::size_t index = 0; index < header.payload_size; ++index) {
      packet[sizeof(header) + index] =
          static_cast<std::uint8_t>((seed * 37U + index * 91U + 13U) & 0xffU);
    }
    if (!send_packet(sockets[0], packet.data(),
                     static_cast<std::uint32_t>(sizeof(header) + header.payload_size))) {
      (void)::close(sockets[0]);
      return false;
    }
    const ServerResult result = server.process_once();
    (void)::close(sockets[0]);
    if (!defined(result)) {
      return false;
    }
  }
  return true;
}

struct InjectionCounter final {
  std::uint32_t calls = 0U;
  std::uint32_t failures = 0U;
  bool fail_next = false;
};

bool inject(void* context, std::uint32_t vector) {
  auto* counter = static_cast<InjectionCounter*>(context);
  if (counter == nullptr || vector >= 2U) {
    return false;
  }
  if (counter->fail_next) {
    counter->fail_next = false;
    ++counter->failures;
    return false;
  }
  ++counter->calls;
  return true;
}

bool msix_interrupt_storm_and_injection_failure_soak() {
  using metaflux::transport::vfio_user::MsixNotificationLedger;
  using metaflux::transport::vfio_user::MsixStatus;
  InjectionCounter counter{};
  MsixNotificationLedger ledger(1U, inject, &counter);
  // Arm once, then flood notifications under mask to force coalescing.
  if (ledger.set_mask(1U, 0U, true) != MsixStatus::success ||
      ledger.arm_completion(1U, 1U) != MsixStatus::success) {
    return false;
  }
  for (std::uint64_t timeline = 1U; timeline <= 4096U; ++timeline) {
    if (ledger.notify(1U, 0U, timeline) != MsixStatus::masked) {
      return false;
    }
  }
  // One unmask delivers a single coalesced injection for the storm.
  if (ledger.set_mask(1U, 0U, false) != MsixStatus::success || counter.calls != 1U) {
    return false;
  }
  // Re-arm and prove injection failure + bounded retry under storm pressure.
  if (ledger.arm_completion(1U, 5000U) != MsixStatus::success) {
    return false;
  }
  counter.fail_next = true;
  if (ledger.notify(1U, 0U, 5000U) != MsixStatus::injection_failed ||
      ledger.retry_pending(1U, 0U) != MsixStatus::success || counter.failures != 1U ||
      counter.calls != 2U) {
    return false;
  }
  // Vector-1 storm while disarmed must stay quiet.
  for (std::uint64_t timeline = 1U; timeline <= 256U; ++timeline) {
    if (ledger.notify(1U, 1U, timeline) != MsixStatus::disarmed) {
      return false;
    }
  }
  return counter.calls == 2U;
}

} // namespace

int main() {
  struct Case {
    const char* name;
    bool (*fn)();
  };
  const Case cases[] = {
      {"dma-overlap-rejected", dma_overlap_rejected},
      {"dma-hole-exact-unmap", dma_hole_exact_unmap_required},
      {"dma-stale-epoch-generation", dma_stale_epoch_and_generation_rejected},
      {"in-flight-unmap-drain", in_flight_unmap_blocks_until_lease_and_pin_drain},
      {"client-death-lost-no-reuse", client_death_marks_lost_and_blocks_stale_reuse},
      {"structured-dma-fuzz", structured_dma_map_unmap_fuzz_is_defined},
      {"msix-storm-injection", msix_interrupt_storm_and_injection_failure_soak},
  };
  for (std::size_t index = 0; index < sizeof(cases) / sizeof(cases[0]); ++index) {
    if (!cases[index].fn()) {
      std::cerr << "vfio-user fault matrix failed: " << cases[index].name << '\n';
      return static_cast<int>(index + 1);
    }
  }
  std::cout << "vfio-user fault matrix: " << (sizeof(cases) / sizeof(cases[0])) << '/'
            << (sizeof(cases) / sizeof(cases[0])) << " passed\n";
  return 0;
}
