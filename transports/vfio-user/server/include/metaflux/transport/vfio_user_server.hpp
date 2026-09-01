#ifndef METAFLUX_TRANSPORT_VFIO_USER_SERVER_HPP
#define METAFLUX_TRANSPORT_VFIO_USER_SERVER_HPP

#include <cstddef>
#include <cstdint>
#include <array>
#include <vector>

#include <metaflux/runtime/lifecycle_dispatch.hpp>
#include <metaflux/transport/generated.h>

namespace metaflux::transport::vfio_user {

enum class ServerState : std::uint32_t {
  Negotiating = 0U,
  Configuring = 1U,
  Running = 2U,
  Lost = 3U,
  Closed = 4U,
};

enum class ServerResult : std::uint32_t {
  Idle = 0U,
  Replied = 1U,
  NoReply = 2U,
  Closed = 3U,
  Malformed = 4U,
};

struct ServerConfig final {
  std::uint64_t device_generation = 1U;
  std::uint64_t mapping_epoch = 1U;
  std::uint32_t max_mappings = 64U;
  std::uint32_t address_width = 48U;
  std::uint16_t transport_major = MF_TRANSPORT_MAJOR_V0;
  std::uint16_t transport_minor = MF_TRANSPORT_MINOR_V0;
  std::uint64_t transport_features = MF_TRANSPORT_FEATURE_VFIO_USER_V0 |
                                      MF_TRANSPORT_FEATURE_IOEVENTFD_V0 |
                                      MF_TRANSPORT_FEATURE_MSIX_V0 |
                                      MF_TRANSPORT_FEATURE_SHARED_MEMORY_V0;
  std::uint64_t daemon_incarnation = 1U;
  std::uint64_t view_serial = 1U;
  std::array<std::uint8_t, 16> logical_device_uuid{1U};
  std::uint32_t descriptor_version = 1U;
  std::uint32_t ring_version = 1U;
  std::uint32_t max_queues = 2U;
  std::uint32_t ring_order = 8U;
  std::uint32_t dma_alignment = 4096U;
  std::uint32_t max_regions = 64U;
  std::uint32_t max_inflight = 256U;
  std::uint64_t max_bytes = UINT64_C(0x10000000);
};

struct DmaMapping final {
  std::uint64_t iova = 0U;
  std::uint64_t size = 0U;
  std::uint64_t file_offset = 0U;
  std::uint64_t mapping_epoch = 0U;
  std::uint64_t device_generation = 0U;
  std::uint32_t permissions = 0U;
  int fd = -1;
  bool revoking = false;
  bool finalized = false;
  std::vector<std::uint64_t> lease_ids{};
};

struct DmaLease final {
  std::uint64_t lease_id = 0U;
  std::uint64_t iova = 0U;
  std::uint64_t size = 0U;
  std::uint64_t mapping_epoch = 0U;
  std::uint64_t device_generation = 0U;
  std::uint32_t permissions = 0U;
};

class VfioUserServer final {
public:
  explicit VfioUserServer(int fd, ServerConfig config = {}) noexcept;
  ~VfioUserServer();

  VfioUserServer(const VfioUserServer&) = delete;
  VfioUserServer& operator=(const VfioUserServer&) = delete;

  ServerResult process_once() noexcept;
  [[nodiscard]] ServerResult
  process_once(metaflux::runtime::lifecycle::Coordinator& coordinator,
               const metaflux::runtime::lifecycle::ExternalEvent& disconnect_event,
               metaflux::runtime::lifecycle::ResultDetails& out) noexcept;
  [[nodiscard]] ServerResult
  process_once(metaflux::runtime::lifecycle::Coordinator& coordinator, std::uint64_t request_id,
               std::uint64_t deadline_tick,
               metaflux::runtime::lifecycle::ResultDetails& out) noexcept;
  ServerState state() const noexcept { return state_; }
  std::size_t mapping_count() const noexcept { return mappings_.size(); }
  std::size_t retired_mapping_count() const noexcept { return retired_mappings_.size(); }
  std::uint64_t mapped_bytes() const noexcept { return mapped_bytes_; }
  bool dma_lookup(std::uint64_t iova, std::uint64_t size, std::uint32_t permission) const noexcept;
  [[nodiscard]] bool dma_acquire(std::uint64_t iova, std::uint64_t size,
                                 std::uint32_t permission, DmaLease& out) noexcept;
  [[nodiscard]] bool dma_release(const DmaLease& lease) noexcept;
  [[nodiscard]] bool
  attach_lifecycle(metaflux::runtime::lifecycle::Coordinator& coordinator) noexcept;
  [[nodiscard]] ServerResult
  mark_lost_and_submit(const metaflux::runtime::lifecycle::ExternalEvent& event,
                       metaflux::runtime::lifecycle::Coordinator& coordinator,
                       metaflux::runtime::lifecycle::ResultDetails& out) noexcept;
  [[nodiscard]] metaflux::runtime::lifecycle::Mirror lifecycle_mirror() noexcept;
  [[nodiscard]] bool lifecycle_online() const noexcept { return lifecycle_online_; }
  [[nodiscard]] std::uint64_t device_generation() const noexcept {
    return config_.device_generation;
  }
  [[nodiscard]] std::uint64_t mapping_epoch() const noexcept { return config_.mapping_epoch; }

private:
  static bool lifecycle_prepare(void* context,
                                const metaflux::runtime::lifecycle::MirrorEvent& event) noexcept;
  static bool lifecycle_quiesce(void* context,
                                const metaflux::runtime::lifecycle::MirrorEvent& event) noexcept;
  static bool lifecycle_drain(void* context,
                              const metaflux::runtime::lifecycle::MirrorEvent& event) noexcept;
  static bool lifecycle_commit(void* context,
                               const metaflux::runtime::lifecycle::MirrorEvent& event) noexcept;
  static bool lifecycle_abort(void* context,
                              const metaflux::runtime::lifecycle::MirrorEvent& event) noexcept;
  static void lifecycle_lost(void* context,
                             const metaflux::runtime::lifecycle::MirrorEvent& event) noexcept;
  void mark_lost() noexcept;
  bool drain_lifecycle() noexcept;
  void finalize_mapping(DmaMapping& mapping) noexcept;
  void clear_finalized_tombstones() noexcept;
  ServerResult reply_payload(std::uint64_t message_id, std::uint16_t request_type,
                             const void* payload, std::size_t payload_size, bool no_reply) noexcept;
  ServerResult reply(std::uint64_t message_id, std::uint16_t request_type, std::int32_t status,
                     const void* result, std::size_t result_size, bool no_reply) noexcept;
  ServerResult handle_message(const mf_transport_message_header_v0& header,
                              const std::uint8_t* payload, std::size_t payload_size,
                              int received_fd, bool no_reply) noexcept;
  ServerResult handle_get_info(const mf_transport_message_header_v0& header,
                               bool no_reply) noexcept;
  ServerResult handle_negotiate(const mf_transport_message_header_v0& header,
                                const std::uint8_t* payload, std::size_t payload_size,
                                int received_fd, bool no_reply) noexcept;
  ServerResult handle_dma_map(const mf_transport_message_header_v0& header,
                              const std::uint8_t* payload, std::size_t payload_size,
                              int received_fd, bool no_reply) noexcept;
  ServerResult handle_dma_unmap(const mf_transport_message_header_v0& header,
                                const std::uint8_t* payload, std::size_t payload_size,
                                bool no_reply) noexcept;

  int fd_ = -1;
  ServerConfig config_{};
  ServerState state_ = ServerState::Negotiating;
  std::vector<DmaMapping> mappings_{};
  std::vector<DmaMapping> retired_mappings_{};
  std::uint64_t mapped_bytes_ = 0U;
  std::uint64_t next_lease_id_ = 1U;
  bool lifecycle_online_ = true;
  bool lifecycle_accepting_ = true;
  bool negotiated_ = false;
};

} // namespace metaflux::transport::vfio_user

#endif
