#include "metaflux/transport/cdev_worker.hpp"

#include "metaflux/client/fastpath.h"

#include <array>
#include <cstdint>
#include <cstring>

int main() {
  mf_registry_view_id_v1 view{.daemon_incarnation = 7U, .view_serial = 9U};
  mf_client_ring_v1 submission{};
  mf_client_ring_v1 completion{};
  if (mf_client_ring_create_v1(8U, view, 1U, 4U, &submission) != MF_SHARED_SUCCESS ||
      mf_client_ring_create_v1(8U, view, 2U, 4U, &completion) != MF_SHARED_SUCCESS) {
    mf_client_ring_close_v1(&submission);
    mf_client_ring_close_v1(&completion);
    return 1;
  }
  std::array<std::uint8_t, 512> payload{};
  for (std::size_t index = 0; index < 64U; ++index) {
    payload[128U + index] = static_cast<std::uint8_t>(index);
  }
  mf_ring_descriptor_v1 request{};
  request.opcode = MF_RING_OPCODE_COPY;
  request.request_id = 41U;
  request.target_id = 4U;
  request.arguments[0] = 256U;
  request.arguments[1] = 128U;
  request.arguments[2] = 64U;
  request.arguments[3] = 0U;
  if (mf_client_ring_try_submit_v1(&submission, &request) != MF_SHARED_SUCCESS) {
    mf_client_ring_close_v1(&submission);
    mf_client_ring_close_v1(&completion);
    return 1;
  }
  metaflux::transport::cdev::CdevWorker worker({
      .submission = submission.header,
      .completion = completion.header,
      .payload = payload.data(),
      .payload_size = payload.size(),
      .generation = 4U,
  });
  if (worker.consume_once() != metaflux::transport::cdev::WorkerResult::Completed ||
      std::memcmp(payload.data() + 256U, payload.data() + 128U, 64U) != 0) {
    mf_client_ring_close_v1(&submission);
    mf_client_ring_close_v1(&completion);
    return 1;
  }
  mf_ring_descriptor_v1 result{};
  if (mf_client_ring_try_consume_v1(&completion, &result) != MF_SHARED_SUCCESS ||
      result.opcode != MF_RING_OPCODE_COMPLETION || result.request_id != 41U ||
      result.arguments[0] != static_cast<std::uint64_t>(MF_SHARED_SUCCESS) ||
      result.arguments[1] != 1U) {
    mf_client_ring_close_v1(&submission);
    mf_client_ring_close_v1(&completion);
    return 1;
  }
  mf_client_ring_close_v1(&submission);
  mf_client_ring_close_v1(&completion);
  return 0;
}
