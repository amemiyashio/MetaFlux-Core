#ifndef METAFLUX_TRANSPORT_VFIO_USER_SERVER_HPP
#define METAFLUX_TRANSPORT_VFIO_USER_SERVER_HPP

#include <cstddef>
#include <cstdint>
#include <vector>

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
};

struct DmaMapping final {
  std::uint64_t iova = 0U;
  std::uint64_t size = 0U;
  std::uint64_t file_offset = 0U;
  std::uint64_t mapping_epoch = 0U;
  std::uint64_t device_generation = 0U;
  std::uint32_t permissions = 0U;
  int fd = -1;
};

class VfioUserServer final {
public:
  explicit VfioUserServer(int fd, ServerConfig config = {}) noexcept;
  ~VfioUserServer();

  VfioUserServer(const VfioUserServer&) = delete;
  VfioUserServer& operator=(const VfioUserServer&) = delete;

  ServerResult process_once() noexcept;
  ServerState state() const noexcept { return state_; }
  std::size_t mapping_count() const noexcept { return mappings_.size(); }
  bool dma_lookup(std::uint64_t iova, std::uint64_t size, std::uint32_t permission) const noexcept;

private:
  ServerResult reply_payload(std::uint64_t message_id, std::uint16_t request_type,
                             const void* payload, std::size_t payload_size,
                             bool no_reply) noexcept;
  ServerResult reply(std::uint64_t message_id, std::uint16_t request_type,
                     std::int32_t status, const void* result, std::size_t result_size,
                     bool no_reply) noexcept;
  ServerResult handle_message(const mf_transport_message_header_v0& header,
                              const std::uint8_t* payload, std::size_t payload_size, int received_fd,
                              bool no_reply) noexcept;
  ServerResult handle_get_info(const mf_transport_message_header_v0& header, bool no_reply) noexcept;
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
};

}  // namespace metaflux::transport::vfio_user

#endif
