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

  metaflux::runtime::lifecycle::Config lifecycle_config{};
  lifecycle_config.logical_device_id = 7U;
  lifecycle_config.daemon_incarnation = 7U;
  lifecycle_config.initial_identity_record_id = 4U;
  lifecycle_config.initial_generation = 4U;
  lifecycle_config.initial_epoch = 1U;
  lifecycle_config.generation_terminal = 32U;
  lifecycle_config.identity_record_terminal = 32U;
  lifecycle_config.epoch_terminal = 32U;
  metaflux::runtime::lifecycle::Coordinator coordinator(lifecycle_config);
  if (!worker.attach_lifecycle(coordinator)) {
    mf_client_ring_close_v1(&submission);
    mf_client_ring_close_v1(&completion);
    return 1;
  }
  const metaflux::runtime::lifecycle::Request reset{
      .request_id = 42U,
      .logical_device_id = 7U,
      .daemon_incarnation = 7U,
      .expected_identity_record_id = 4U,
      .expected_generation = 4U,
      .expected_epoch = 1U,
      .source = metaflux::runtime::lifecycle::Source::Cdev,
      .operation = metaflux::runtime::lifecycle::Operation::Reset,
  };
  if (coordinator.apply(reset) != metaflux::runtime::lifecycle::Result::Accepted ||
      worker.generation() != 5U || !worker.lifecycle_online()) {
    mf_client_ring_close_v1(&submission);
    mf_client_ring_close_v1(&completion);
    return 1;
  }
  request.request_id = 42U;
  request.target_id = 4U;
  if (mf_client_ring_try_submit_v1(&submission, &request) != MF_SHARED_SUCCESS ||
      worker.consume_once() != metaflux::transport::cdev::WorkerResult::Completed ||
      mf_client_ring_try_consume_v1(&completion, &result) != MF_SHARED_SUCCESS ||
      result.arguments[0] != static_cast<std::uint64_t>(MF_SHARED_STALE_HANDLE)) {
    mf_client_ring_close_v1(&submission);
    mf_client_ring_close_v1(&completion);
    return 1;
  }
  const metaflux::runtime::lifecycle::Request loss{
      .request_id = 43U,
      .logical_device_id = 7U,
      .daemon_incarnation = 7U,
      .expected_identity_record_id = 5U,
      .expected_generation = 5U,
      .expected_epoch = 2U,
      .source = metaflux::runtime::lifecycle::Source::Disconnect,
      .operation = metaflux::runtime::lifecycle::Operation::TransportLoss,
  };
  if (coordinator.apply(loss) != metaflux::runtime::lifecycle::Result::Accepted ||
      worker.lifecycle_online()) {
    mf_client_ring_close_v1(&submission);
    mf_client_ring_close_v1(&completion);
    return 1;
  }
  request.request_id = 44U;
  request.target_id = 5U;
  if (mf_client_ring_try_submit_v1(&submission, &request) != MF_SHARED_SUCCESS ||
      worker.consume_once() != metaflux::transport::cdev::WorkerResult::Completed ||
      mf_client_ring_try_consume_v1(&completion, &result) != MF_SHARED_SUCCESS ||
      result.arguments[0] != static_cast<std::uint64_t>(MF_SHARED_DEVICE_LOST)) {
    mf_client_ring_close_v1(&submission);
    mf_client_ring_close_v1(&completion);
    return 1;
  }
  mf_client_ring_close_v1(&submission);
  mf_client_ring_close_v1(&completion);
  return 0;
}
