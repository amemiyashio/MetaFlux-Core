#include "metaflux/transport/cdev_worker.hpp"

#include "metaflux/client/fastpath.h"

#if defined(METAFLUX_CPU_BACKEND)
#include "metaflux/backend/cpu.h"
#endif

#include <array>
#include <cstdint>
#include <cstring>

namespace {

struct BackendFixture final {
  std::uint32_t calls = 0U;
  mf_backend_copy_v1 last{};
  mf_backend_status_v1 result = MF_BACKEND_SUCCESS;
};

mf_backend_status_v1 fixture_copy(mf_backend_instance_v1 instance, mf_backend_queue_v1 queue,
                                  const mf_backend_copy_v1* copy,
                                  mf_backend_event_v1 completion_event) {
  auto* fixture = reinterpret_cast<BackendFixture*>(static_cast<std::uintptr_t>(instance));
  if (fixture == nullptr || queue != 17U || completion_event != 0U || copy == nullptr) {
    return MF_BACKEND_INVALID_ARGUMENT;
  }
  ++fixture->calls;
  fixture->last = *copy;
  return fixture->result;
}

mf_backend_api_v1 make_fixture_api() {
  mf_backend_api_v1 api{};
  api.header.abi_version = MF_BACKEND_ABI_VERSION_1;
  api.header.struct_size = sizeof(api);
  api.header.capabilities = MF_BACKEND_CAP_COPY;
  api.copy = fixture_copy;
  return api;
}

} // namespace

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

  BackendFixture backend_fixture{};
  const auto backend_api = make_fixture_api();
  metaflux::transport::cdev::CdevWorker backend_worker(
      {.submission = submission.header,
       .completion = completion.header,
       .payload = payload.data(),
       .payload_size = payload.size(),
       .generation = 4U},
      {.api = &backend_api,
       .instance =
           static_cast<mf_backend_instance_v1>(reinterpret_cast<std::uintptr_t>(&backend_fixture)),
       .queue = 17U,
       .memory = 23U,
       .completion_event = 0U});
  request.request_id = 42U;
  request.arguments[0] = 320U;
  request.arguments[1] = 128U;
  request.arguments[2] = 32U;
  request.arguments[3] = 16U;
  if (!backend_worker.backend_bound() ||
      mf_client_ring_try_submit_v1(&submission, &request) != MF_SHARED_SUCCESS ||
      backend_worker.consume_once() != metaflux::transport::cdev::WorkerResult::Completed ||
      backend_fixture.calls != 1U ||
      backend_fixture.last.struct_size != sizeof(mf_backend_copy_v1) ||
      backend_fixture.last.destination != 23U || backend_fixture.last.source != 23U ||
      backend_fixture.last.destination_offset != 336U ||
      backend_fixture.last.source_offset != 144U || backend_fixture.last.byte_count != 32U ||
      mf_client_ring_try_consume_v1(&completion, &result) != MF_SHARED_SUCCESS ||
      result.arguments[0] != static_cast<std::uint64_t>(MF_SHARED_SUCCESS)) {
    mf_client_ring_close_v1(&submission);
    mf_client_ring_close_v1(&completion);
    return 1;
  }
  backend_fixture.result = MF_BACKEND_TIMEOUT;
  request.request_id = 43U;
  if (mf_client_ring_try_submit_v1(&submission, &request) != MF_SHARED_SUCCESS ||
      backend_worker.consume_once() != metaflux::transport::cdev::WorkerResult::Completed ||
      mf_client_ring_try_consume_v1(&completion, &result) != MF_SHARED_SUCCESS ||
      result.arguments[0] != static_cast<std::uint64_t>(MF_SHARED_TIMEOUT)) {
    mf_client_ring_close_v1(&submission);
    mf_client_ring_close_v1(&completion);
    return 1;
  }

  auto unsupported_api = backend_api;
  unsupported_api.header.capabilities = 0U;
  metaflux::transport::cdev::CdevWorker unsupported_worker(
      {.submission = submission.header,
       .completion = completion.header,
       .payload = payload.data(),
       .payload_size = payload.size(),
       .generation = 4U},
      {.api = &unsupported_api,
       .instance =
           static_cast<mf_backend_instance_v1>(reinterpret_cast<std::uintptr_t>(&backend_fixture)),
       .queue = 17U,
       .memory = 23U,
       .completion_event = 0U});
  request.request_id = 44U;
  if (mf_client_ring_try_submit_v1(&submission, &request) != MF_SHARED_SUCCESS ||
      unsupported_worker.consume_once() != metaflux::transport::cdev::WorkerResult::Completed ||
      mf_client_ring_try_consume_v1(&completion, &result) != MF_SHARED_SUCCESS ||
      result.arguments[0] != static_cast<std::uint64_t>(MF_SHARED_NOT_SUPPORTED)) {
    mf_client_ring_close_v1(&submission);
    mf_client_ring_close_v1(&completion);
    return 1;
  }

  request.request_id = 46U;
  request.flags = UINT32_C(0x80000000);
  request.arguments[2] = 32U;
  request.arguments[3] = 0U;
  if (mf_client_ring_try_submit_v1(&submission, &request) != MF_SHARED_SUCCESS ||
      worker.consume_once() != metaflux::transport::cdev::WorkerResult::Completed ||
      mf_client_ring_try_consume_v1(&completion, &result) != MF_SHARED_SUCCESS ||
      result.arguments[0] != static_cast<std::uint64_t>(MF_SHARED_MALFORMED)) {
    mf_client_ring_close_v1(&submission);
    mf_client_ring_close_v1(&completion);
    return 1;
  }

  request.request_id = 47U;
  request.flags = MF_RING_COPY_FLAG_DIRECT_HOST_SOURCE_V1;
  if (mf_client_ring_try_submit_v1(&submission, &request) != MF_SHARED_SUCCESS ||
      worker.consume_once() != metaflux::transport::cdev::WorkerResult::Completed ||
      mf_client_ring_try_consume_v1(&completion, &result) != MF_SHARED_SUCCESS ||
      result.arguments[0] != static_cast<std::uint64_t>(MF_SHARED_NOT_SUPPORTED)) {
    mf_client_ring_close_v1(&submission);
    mf_client_ring_close_v1(&completion);
    return 1;
  }

  request.request_id = 48U;
  request.flags = 0U;
  request.arguments[2] = 0U;
  if (mf_client_ring_try_submit_v1(&submission, &request) != MF_SHARED_SUCCESS ||
      worker.consume_once() != metaflux::transport::cdev::WorkerResult::Completed ||
      mf_client_ring_try_consume_v1(&completion, &result) != MF_SHARED_SUCCESS ||
      result.arguments[0] != static_cast<std::uint64_t>(MF_SHARED_INVALID_ARGUMENT)) {
    mf_client_ring_close_v1(&submission);
    mf_client_ring_close_v1(&completion);
    return 1;
  }

  request.request_id = 49U;
  request.arguments[2] = 32U;
  request.arguments[3] = payload.size() + 1U;
  if (mf_client_ring_try_submit_v1(&submission, &request) != MF_SHARED_SUCCESS ||
      worker.consume_once() != metaflux::transport::cdev::WorkerResult::Completed ||
      mf_client_ring_try_consume_v1(&completion, &result) != MF_SHARED_SUCCESS ||
      result.arguments[0] != static_cast<std::uint64_t>(MF_SHARED_NOT_SUPPORTED)) {
    mf_client_ring_close_v1(&submission);
    mf_client_ring_close_v1(&completion);
    return 1;
  }

  metaflux::transport::cdev::CdevWorker malformed_worker({
      .submission = nullptr,
      .completion = completion.header,
      .payload = payload.data(),
      .payload_size = payload.size(),
      .generation = 4U,
  });
  if (malformed_worker.consume_once() != metaflux::transport::cdev::WorkerResult::Malformed) {
    mf_client_ring_close_v1(&submission);
    mf_client_ring_close_v1(&completion);
    return 1;
  }

  mf_client_ring_v1 pressure_submission{};
  mf_client_ring_v1 pressure_completion{};
  if (mf_client_ring_create_v1(2U, view, 3U, 4U, &pressure_submission) != MF_SHARED_SUCCESS ||
      mf_client_ring_create_v1(2U, view, 4U, 4U, &pressure_completion) != MF_SHARED_SUCCESS) {
    mf_client_ring_close_v1(&pressure_submission);
    mf_client_ring_close_v1(&pressure_completion);
    mf_client_ring_close_v1(&submission);
    mf_client_ring_close_v1(&completion);
    return 1;
  }
  mf_ring_descriptor_v1 filler{};
  filler.opcode = MF_RING_OPCODE_NOOP;
  filler.target_id = 4U;
  filler.request_id = 100U;
  if (mf_client_ring_try_submit_v1(&pressure_completion, &filler) != MF_SHARED_SUCCESS) {
    mf_client_ring_close_v1(&pressure_submission);
    mf_client_ring_close_v1(&pressure_completion);
    mf_client_ring_close_v1(&submission);
    mf_client_ring_close_v1(&completion);
    return 1;
  }
  filler.request_id = 101U;
  if (mf_client_ring_try_submit_v1(&pressure_completion, &filler) != MF_SHARED_SUCCESS) {
    mf_client_ring_close_v1(&pressure_submission);
    mf_client_ring_close_v1(&pressure_completion);
    mf_client_ring_close_v1(&submission);
    mf_client_ring_close_v1(&completion);
    return 1;
  }
  metaflux::transport::cdev::CdevWorker pressure_worker({
      .submission = pressure_submission.header,
      .completion = pressure_completion.header,
      .payload = payload.data(),
      .payload_size = payload.size(),
      .generation = 4U,
  });
  request.request_id = 50U;
  request.flags = 0U;
  request.opcode = MF_RING_OPCODE_NOOP;
  request.target_id = 4U;
  if (mf_client_ring_try_submit_v1(&pressure_submission, &request) != MF_SHARED_SUCCESS) {
    mf_client_ring_close_v1(&pressure_submission);
    mf_client_ring_close_v1(&pressure_completion);
    mf_client_ring_close_v1(&submission);
    mf_client_ring_close_v1(&completion);
    return 1;
  }
  const auto pressure_result = pressure_worker.consume_once();
  if (pressure_result != metaflux::transport::cdev::WorkerResult::Backpressure) {
    mf_client_ring_close_v1(&pressure_submission);
    mf_client_ring_close_v1(&pressure_completion);
    mf_client_ring_close_v1(&submission);
    mf_client_ring_close_v1(&completion);
    return 1;
  }
  if (mf_client_ring_try_consume_v1(&pressure_completion, &result) != MF_SHARED_SUCCESS) {
    mf_client_ring_close_v1(&pressure_submission);
    mf_client_ring_close_v1(&pressure_completion);
    mf_client_ring_close_v1(&submission);
    mf_client_ring_close_v1(&completion);
    return 1;
  }
  if (pressure_worker.consume_once() != metaflux::transport::cdev::WorkerResult::Completed) {
    mf_client_ring_close_v1(&pressure_submission);
    mf_client_ring_close_v1(&pressure_completion);
    mf_client_ring_close_v1(&submission);
    mf_client_ring_close_v1(&completion);
    return 1;
  }
  if (mf_client_ring_try_consume_v1(&pressure_completion, &result) != MF_SHARED_SUCCESS ||
      result.request_id != 101U ||
      mf_client_ring_try_consume_v1(&pressure_completion, &result) != MF_SHARED_SUCCESS ||
      result.request_id != 50U ||
      result.arguments[0] != static_cast<std::uint64_t>(MF_SHARED_SUCCESS)) {
    mf_client_ring_close_v1(&pressure_submission);
    mf_client_ring_close_v1(&pressure_completion);
    mf_client_ring_close_v1(&submission);
    mf_client_ring_close_v1(&completion);
    return 1;
  }
  mf_client_ring_close_v1(&pressure_submission);
  mf_client_ring_close_v1(&pressure_completion);
  request.opcode = MF_RING_OPCODE_COPY;

#if defined(METAFLUX_CPU_BACKEND)
  const auto* cpu_api = mf_cpu_backend_get_api_v1();
  mf_backend_instance_v1 cpu_instance = 0U;
  mf_backend_context_v1 cpu_context = 0U;
  mf_backend_queue_v1 cpu_queue = 0U;
  mf_backend_memory_v1 cpu_memory = 0U;
  if (cpu_api == nullptr || cpu_api->create_instance == nullptr ||
      cpu_api->create_context == nullptr || cpu_api->create_queue == nullptr ||
      cpu_api->copy == nullptr ||
      cpu_api->create_instance(nullptr, &cpu_instance) != MF_BACKEND_SUCCESS ||
      cpu_api->create_context(cpu_instance, 0U, &cpu_context) != MF_BACKEND_SUCCESS ||
      cpu_api->create_queue(cpu_instance, cpu_context, &cpu_queue) != MF_BACKEND_SUCCESS ||
      mf_cpu_backend_import_host_memory_v1(cpu_instance, cpu_context, payload.data(),
                                           payload.size(), &cpu_memory) != MF_BACKEND_SUCCESS) {
    if (cpu_api != nullptr && cpu_api->destroy_instance != nullptr) {
      cpu_api->destroy_instance(cpu_instance);
    }
    mf_client_ring_close_v1(&submission);
    mf_client_ring_close_v1(&completion);
    return 1;
  }
  metaflux::transport::cdev::CdevWorker production_worker({.submission = submission.header,
                                                           .completion = completion.header,
                                                           .payload = payload.data(),
                                                           .payload_size = payload.size(),
                                                           .generation = 4U},
                                                          {.api = cpu_api,
                                                           .instance = cpu_instance,
                                                           .queue = cpu_queue,
                                                           .memory = cpu_memory,
                                                           .completion_event = 0U});
  request.request_id = 45U;
  request.target_id = 4U;
  request.arguments[0] = 384U;
  request.arguments[1] = 256U;
  request.arguments[2] = 64U;
  request.arguments[3] = 0U;
  if (!production_worker.backend_bound() ||
      mf_client_ring_try_submit_v1(&submission, &request) != MF_SHARED_SUCCESS ||
      production_worker.consume_once() != metaflux::transport::cdev::WorkerResult::Completed ||
      std::memcmp(payload.data() + 384U, payload.data() + 256U, 64U) != 0 ||
      mf_client_ring_try_consume_v1(&completion, &result) != MF_SHARED_SUCCESS ||
      result.arguments[0] != static_cast<std::uint64_t>(MF_SHARED_SUCCESS)) {
    if (cpu_api->destroy_instance != nullptr) {
      cpu_api->destroy_instance(cpu_instance);
    }
    mf_client_ring_close_v1(&submission);
    mf_client_ring_close_v1(&completion);
    return 1;
  }
  if (cpu_api->free_memory != nullptr) {
    cpu_api->free_memory(cpu_instance, cpu_memory);
  }
  if (cpu_api->destroy_queue != nullptr) {
    cpu_api->destroy_queue(cpu_instance, cpu_queue);
  }
  if (cpu_api->destroy_context != nullptr) {
    cpu_api->destroy_context(cpu_instance, cpu_context);
  }
  if (cpu_api->destroy_instance != nullptr) {
    cpu_api->destroy_instance(cpu_instance);
  }
#endif

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
