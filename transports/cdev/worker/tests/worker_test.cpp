#include "metaflux/transport/cdev_worker.hpp"

#include "metaflux/client/fastpath.h"

#if defined(METAFLUX_CPU_BACKEND)
#include "metaflux/backend/cpu.h"
#endif

#if defined(METAFLUX_CPU_CDEV_LAUNCH)
#include "metaflux/compiler/kernel_ir.hpp"
#include "metaflux/compiler/ptx_frontend.hpp"
#endif

#include <array>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <span>
#include <string>
#include <vector>

namespace {

struct BackendFixture final {
  std::uint32_t calls = 0U;
  mf_backend_copy_v1 last{};
  std::uint32_t launch_calls = 0U;
  mf_backend_launch_v1 last_launch{};
  mf_backend_status_v1 result = MF_BACKEND_SUCCESS;
  std::uint32_t lease_acquires = 0U;
  std::uint32_t lease_releases = 0U;
  bool lease_active = false;
  mf_shared_status_v1 lease_result = MF_SHARED_SUCCESS;
};

mf_shared_status_v1 fixture_lease_acquire(void* context) noexcept {
  auto* fixture = static_cast<BackendFixture*>(context);
  if (fixture == nullptr || fixture->lease_active) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  ++fixture->lease_acquires;
  if (fixture->lease_result != MF_SHARED_SUCCESS) {
    return fixture->lease_result;
  }
  fixture->lease_active = true;
  return MF_SHARED_SUCCESS;
}

void fixture_lease_release(void* context) noexcept {
  auto* fixture = static_cast<BackendFixture*>(context);
  if (fixture != nullptr && fixture->lease_active) {
    fixture->lease_active = false;
    ++fixture->lease_releases;
  }
}

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

mf_backend_status_v1 fixture_submit(mf_backend_instance_v1 instance, mf_backend_queue_v1 queue,
                                    const mf_backend_launch_v1* launch,
                                    mf_backend_event_v1 completion_event) {
  auto* fixture = reinterpret_cast<BackendFixture*>(static_cast<std::uintptr_t>(instance));
  if (fixture == nullptr || queue != 17U || completion_event != 0U || launch == nullptr) {
    return MF_BACKEND_INVALID_ARGUMENT;
  }
  ++fixture->launch_calls;
  fixture->last_launch = *launch;
  return fixture->result;
}

struct LaunchResolutionFixture final {
  metaflux::transport::cdev::CdevLaunchResolution resolution{};
  std::uint32_t calls = 0U;
  mf_shared_status_v1 result = MF_SHARED_SUCCESS;
};

mf_shared_status_v1 resolve_launch(void* context, const mf_ring_descriptor_v1* request,
                                   metaflux::transport::cdev::CdevLaunchResolution* out) noexcept {
  auto* fixture = static_cast<LaunchResolutionFixture*>(context);
  if (fixture == nullptr || request == nullptr || out == nullptr || request->target_id != 4U ||
      request->arguments[0] != 11U || request->arguments[1] != 13U ||
      request->arguments[2] != 17U || request->arguments[3] != 19U) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  ++fixture->calls;
  if (fixture->result != MF_SHARED_SUCCESS) {
    return fixture->result;
  }
  *out = fixture->resolution;
  return MF_SHARED_SUCCESS;
}

mf_backend_api_v1 make_fixture_api() {
  mf_backend_api_v1 api{};
  api.header.abi_version = MF_BACKEND_ABI_VERSION_1;
  api.header.struct_size = sizeof(api);
  api.header.capabilities = MF_BACKEND_CAP_COPY | MF_BACKEND_CAP_LAUNCH;
  api.submit = fixture_submit;
  api.copy = fixture_copy;
  return api;
}

#if defined(METAFLUX_CPU_CDEV_LAUNCH)
std::string read_file(const char* path) {
  std::ifstream input(path, std::ios::binary | std::ios::ate);
  if (!input) {
    return {};
  }
  const auto end = input.tellg();
  if (end <= 0) {
    return {};
  }
  std::string bytes(static_cast<std::size_t>(end), '\0');
  input.seekg(0, std::ios::beg);
  input.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
  return input ? bytes : std::string{};
}

std::vector<std::uint8_t>
make_cpu_argument_block(std::span<const mf_cpu_backend_argument_v1> entries) {
  const auto total_size = sizeof(mf_cpu_backend_argument_block_header_v1) +
                          entries.size() * sizeof(mf_cpu_backend_argument_v1);
  std::vector<std::uint8_t> bytes(total_size, 0U);
  mf_cpu_backend_argument_block_header_v1 header{};
  header.magic = MF_CPU_BACKEND_ARGUMENT_BLOCK_MAGIC_V1;
  header.version = MF_CPU_BACKEND_ARGUMENT_BLOCK_VERSION_V1;
  header.header_size = sizeof(header);
  header.entry_size = sizeof(mf_cpu_backend_argument_v1);
  header.entry_count = static_cast<std::uint32_t>(entries.size());
  header.total_size = total_size;
  std::memcpy(bytes.data(), &header, sizeof(header));
  std::memcpy(bytes.data() + sizeof(header), entries.data(),
              entries.size() * sizeof(mf_cpu_backend_argument_v1));
  return bytes;
}
#endif

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
  alignas(std::uint32_t) std::array<std::uint8_t, 1024> payload{};
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
       .completion_event = 0U,
       .lease_acquire = fixture_lease_acquire,
       .lease_release = fixture_lease_release,
       .lease_context = &backend_fixture});
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
      backend_fixture.lease_acquires != 1U || backend_fixture.lease_releases != 1U ||
      backend_fixture.lease_active ||
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
      backend_fixture.lease_acquires != 2U || backend_fixture.lease_releases != 2U ||
      backend_fixture.lease_active ||
      mf_client_ring_try_consume_v1(&completion, &result) != MF_SHARED_SUCCESS ||
      result.arguments[0] != static_cast<std::uint64_t>(MF_SHARED_TIMEOUT)) {
    mf_client_ring_close_v1(&submission);
    mf_client_ring_close_v1(&completion);
    return 1;
  }

  backend_fixture.lease_result = MF_SHARED_WOULD_BLOCK;
  request.request_id = 430U;
  if (mf_client_ring_try_submit_v1(&submission, &request) != MF_SHARED_SUCCESS ||
      backend_worker.consume_once() != metaflux::transport::cdev::WorkerResult::Completed ||
      backend_fixture.calls != 2U || backend_fixture.lease_acquires != 3U ||
      backend_fixture.lease_releases != 2U || backend_fixture.lease_active ||
      mf_client_ring_try_consume_v1(&completion, &result) != MF_SHARED_SUCCESS ||
      result.arguments[0] != static_cast<std::uint64_t>(MF_SHARED_WOULD_BLOCK)) {
    mf_client_ring_close_v1(&submission);
    mf_client_ring_close_v1(&completion);
    return 1;
  }
  backend_fixture.lease_result = MF_SHARED_SUCCESS;

  backend_fixture.result = MF_BACKEND_SUCCESS;
  LaunchResolutionFixture launch_resolution{};
  launch_resolution.resolution.module = 99U;
  launch_resolution.resolution.kernel_id = MF_KERNEL_PRIMARY_ENTRY_ID;
  launch_resolution.resolution.argument_offset = 64U;
  launch_resolution.resolution.argument_size = 32U;
  launch_resolution.resolution.grid[0] = 1U;
  launch_resolution.resolution.grid[1] = 1U;
  launch_resolution.resolution.grid[2] = 1U;
  launch_resolution.resolution.block[0] = 8U;
  launch_resolution.resolution.block[1] = 1U;
  launch_resolution.resolution.block[2] = 1U;
  metaflux::transport::cdev::CdevWorker launch_worker(
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
       .completion_event = 0U,
       .launch_resolver = resolve_launch,
       .launch_context = &launch_resolution,
       .lease_acquire = fixture_lease_acquire,
       .lease_release = fixture_lease_release,
       .lease_context = &backend_fixture});
  request.opcode = MF_RING_OPCODE_LAUNCH;
  request.flags = 0U;
  request.request_id = 51U;
  request.target_id = 4U;
  request.arguments[0] = 11U;
  request.arguments[1] = 13U;
  request.arguments[2] = 17U;
  request.arguments[3] = 19U;
  if (!launch_worker.backend_bound() ||
      mf_client_ring_try_submit_v1(&submission, &request) != MF_SHARED_SUCCESS ||
      launch_worker.consume_once() != metaflux::transport::cdev::WorkerResult::Completed ||
      launch_resolution.calls != 1U || backend_fixture.launch_calls != 1U ||
      backend_fixture.last_launch.module != 99U ||
      backend_fixture.last_launch.kernel_id != MF_KERNEL_PRIMARY_ENTRY_ID ||
      backend_fixture.last_launch.argument_bytes != payload.data() + 64U ||
      backend_fixture.last_launch.argument_size != 32U ||
      backend_fixture.last_launch.grid[0] != 1U || backend_fixture.last_launch.block[0] != 8U ||
      backend_fixture.lease_acquires != 4U || backend_fixture.lease_releases != 3U ||
      backend_fixture.lease_active ||
      mf_client_ring_try_consume_v1(&completion, &result) != MF_SHARED_SUCCESS ||
      result.arguments[0] != static_cast<std::uint64_t>(MF_SHARED_SUCCESS)) {
    mf_client_ring_close_v1(&submission);
    mf_client_ring_close_v1(&completion);
    return 1;
  }
  launch_resolution.result = MF_SHARED_STALE_HANDLE;
  request.request_id = 52U;
  if (mf_client_ring_try_submit_v1(&submission, &request) != MF_SHARED_SUCCESS ||
      launch_worker.consume_once() != metaflux::transport::cdev::WorkerResult::Completed ||
      backend_fixture.launch_calls != 1U || backend_fixture.lease_acquires != 5U ||
      backend_fixture.lease_releases != 4U || backend_fixture.lease_active ||
      mf_client_ring_try_consume_v1(&completion, &result) != MF_SHARED_SUCCESS ||
      result.arguments[0] != static_cast<std::uint64_t>(MF_SHARED_STALE_HANDLE)) {
    mf_client_ring_close_v1(&submission);
    mf_client_ring_close_v1(&completion);
    return 1;
  }
  launch_resolution.result = MF_SHARED_SUCCESS;
  launch_resolution.resolution.argument_offset = payload.size() + 1U;
  request.request_id = 53U;
  if (mf_client_ring_try_submit_v1(&submission, &request) != MF_SHARED_SUCCESS ||
      launch_worker.consume_once() != metaflux::transport::cdev::WorkerResult::Completed ||
      backend_fixture.launch_calls != 1U || backend_fixture.lease_acquires != 6U ||
      backend_fixture.lease_releases != 5U || backend_fixture.lease_active ||
      mf_client_ring_try_consume_v1(&completion, &result) != MF_SHARED_SUCCESS ||
      result.arguments[0] != static_cast<std::uint64_t>(MF_SHARED_INVALID_ARGUMENT)) {
    mf_client_ring_close_v1(&submission);
    mf_client_ring_close_v1(&completion);
    return 1;
  }
  launch_resolution.resolution.argument_offset = 64U;
  request.request_id = 54U;
  request.flags = UINT32_C(1);
  if (mf_client_ring_try_submit_v1(&submission, &request) != MF_SHARED_SUCCESS ||
      launch_worker.consume_once() != metaflux::transport::cdev::WorkerResult::Completed ||
      backend_fixture.launch_calls != 1U ||
      mf_client_ring_try_consume_v1(&completion, &result) != MF_SHARED_SUCCESS ||
      result.arguments[0] != static_cast<std::uint64_t>(MF_SHARED_MALFORMED)) {
    mf_client_ring_close_v1(&submission);
    mf_client_ring_close_v1(&completion);
    return 1;
  }
  request.opcode = MF_RING_OPCODE_COPY;
  request.flags = 0U;
  request.arguments[2] = 32U;
  request.arguments[3] = 0U;

  metaflux::transport::cdev::CdevWorker unleased_worker(
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
  request.request_id = 440U;
  if (mf_client_ring_try_submit_v1(&submission, &request) != MF_SHARED_SUCCESS ||
      unleased_worker.consume_once() != metaflux::transport::cdev::WorkerResult::Completed ||
      backend_fixture.calls != 2U ||
      mf_client_ring_try_consume_v1(&completion, &result) != MF_SHARED_SUCCESS ||
      result.arguments[0] != static_cast<std::uint64_t>(MF_SHARED_NOT_SUPPORTED)) {
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
       .completion_event = 0U,
       .lease_acquire = fixture_lease_acquire,
       .lease_release = fixture_lease_release,
       .lease_context = &backend_fixture});
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
                                                           .completion_event = 0U,
                                                           .lease_acquire = fixture_lease_acquire,
                                                           .lease_release = fixture_lease_release,
                                                           .lease_context = &backend_fixture});
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
#if defined(METAFLUX_CPU_CDEV_LAUNCH)
  mf_backend_instance_v1 cpu_launch_instance = 0U;
  mf_backend_context_v1 cpu_launch_context = 0U;
  mf_backend_queue_v1 cpu_launch_queue = 0U;
  mf_backend_memory_v1 cpu_launch_memory = 0U;
  mf_backend_module_v1 cpu_module = 0U;
  if (cpu_api->create_instance(nullptr, &cpu_launch_instance) != MF_BACKEND_SUCCESS ||
      cpu_api->create_context(cpu_launch_instance, 0U, &cpu_launch_context) != MF_BACKEND_SUCCESS ||
      cpu_api->create_queue(cpu_launch_instance, cpu_launch_context, &cpu_launch_queue) !=
          MF_BACKEND_SUCCESS ||
      mf_cpu_backend_import_host_memory_v1(cpu_launch_instance, cpu_launch_context, payload.data(),
                                           payload.size(),
                                           &cpu_launch_memory) != MF_BACKEND_SUCCESS) {
    if (cpu_api->destroy_instance != nullptr) {
      cpu_api->destroy_instance(cpu_launch_instance);
    }
    mf_client_ring_close_v1(&submission);
    mf_client_ring_close_v1(&completion);
    return 1;
  }
  const auto ptx = read_file(METAFLUX_CPU_ADD_PTX);
  const auto parsed = metaflux::compiler::ptx::parse(ptx);
  const auto serialized = parsed.ok() ? metaflux::compiler::serialize_kernel(*parsed.kernel)
                                      : metaflux::compiler::SerializationResult{};
  alignas(std::uint32_t) std::array<std::uint32_t, 8> destination{};
  alignas(std::uint32_t) std::array<std::uint32_t, 8> left{};
  alignas(std::uint32_t) std::array<std::uint32_t, 8> right{};
  for (std::size_t index = 0; index < left.size(); ++index) {
    left[index] = static_cast<std::uint32_t>(index + 1U);
    right[index] = static_cast<std::uint32_t>(100U + index);
  }
  std::memcpy(payload.data() + 256U, destination.data(), sizeof(destination));
  std::memcpy(payload.data() + 288U, left.data(), sizeof(left));
  std::memcpy(payload.data() + 320U, right.data(), sizeof(right));
  const std::array<mf_cpu_backend_argument_v1, 4> cpu_entries{
      mf_cpu_backend_argument_v1{.kind = MF_CPU_BACKEND_ARGUMENT_KIND_BUFFER_V1,
                                 .flags = MF_CPU_BACKEND_ARGUMENT_BUFFER_WRITE_V1,
                                 .memory = cpu_launch_memory,
                                 .offset = 256U,
                                 .byte_count = sizeof(destination),
                                 .value = 0U},
      mf_cpu_backend_argument_v1{.kind = MF_CPU_BACKEND_ARGUMENT_KIND_BUFFER_V1,
                                 .flags = MF_CPU_BACKEND_ARGUMENT_BUFFER_READ_V1,
                                 .memory = cpu_launch_memory,
                                 .offset = 288U,
                                 .byte_count = sizeof(left),
                                 .value = 0U},
      mf_cpu_backend_argument_v1{.kind = MF_CPU_BACKEND_ARGUMENT_KIND_BUFFER_V1,
                                 .flags = MF_CPU_BACKEND_ARGUMENT_BUFFER_READ_V1,
                                 .memory = cpu_launch_memory,
                                 .offset = 320U,
                                 .byte_count = sizeof(right),
                                 .value = 0U},
      mf_cpu_backend_argument_v1{.kind = MF_CPU_BACKEND_ARGUMENT_KIND_U32_V1,
                                 .flags = 0U,
                                 .memory = 0U,
                                 .offset = 0U,
                                 .byte_count = 0U,
                                 .value = left.size()},
  };
  const auto cpu_argument_block = make_cpu_argument_block(cpu_entries);
  if (!parsed.ok() || !serialized.ok() ||
      cpu_api->load_module(cpu_launch_instance, 0U,
                           reinterpret_cast<const std::uint8_t*>(serialized.text.data()),
                           serialized.text.size(), &cpu_module) != MF_BACKEND_SUCCESS ||
      cpu_module == 0U || cpu_argument_block.size() > payload.size()) {
    if (cpu_api->destroy_instance != nullptr) {
      cpu_api->destroy_instance(cpu_launch_instance);
    }
    mf_client_ring_close_v1(&submission);
    mf_client_ring_close_v1(&completion);
    return 1;
  }
  std::memcpy(payload.data(), cpu_argument_block.data(), cpu_argument_block.size());
  LaunchResolutionFixture cpu_launch_resolution{};
  cpu_launch_resolution.resolution.module = cpu_module;
  cpu_launch_resolution.resolution.kernel_id = MF_KERNEL_PRIMARY_ENTRY_ID;
  cpu_launch_resolution.resolution.argument_offset = 0U;
  cpu_launch_resolution.resolution.argument_size = cpu_argument_block.size();
  cpu_launch_resolution.resolution.grid[0] = 1U;
  cpu_launch_resolution.resolution.grid[1] = 1U;
  cpu_launch_resolution.resolution.grid[2] = 1U;
  cpu_launch_resolution.resolution.block[0] = left.size();
  cpu_launch_resolution.resolution.block[1] = 1U;
  cpu_launch_resolution.resolution.block[2] = 1U;
  metaflux::transport::cdev::CdevWorker cpu_launch_worker({.submission = submission.header,
                                                           .completion = completion.header,
                                                           .payload = payload.data(),
                                                           .payload_size = payload.size(),
                                                           .generation = 4U},
                                                          {.api = cpu_api,
                                                           .instance = cpu_launch_instance,
                                                           .queue = cpu_launch_queue,
                                                           .memory = cpu_launch_memory,
                                                           .completion_event = 0U,
                                                           .launch_resolver = resolve_launch,
                                                           .launch_context = &cpu_launch_resolution,
                                                           .lease_acquire = fixture_lease_acquire,
                                                           .lease_release = fixture_lease_release,
                                                           .lease_context = &backend_fixture});
  request.opcode = MF_RING_OPCODE_LAUNCH;
  request.flags = 0U;
  request.request_id = 55U;
  request.target_id = 4U;
  request.arguments[0] = 11U;
  request.arguments[1] = 13U;
  request.arguments[2] = 17U;
  request.arguments[3] = 19U;
  if (!cpu_launch_worker.backend_bound() ||
      mf_client_ring_try_submit_v1(&submission, &request) != MF_SHARED_SUCCESS ||
      cpu_launch_worker.consume_once() != metaflux::transport::cdev::WorkerResult::Completed ||
      cpu_launch_resolution.calls != 1U ||
      mf_client_ring_try_consume_v1(&completion, &result) != MF_SHARED_SUCCESS ||
      result.arguments[0] != static_cast<std::uint64_t>(MF_SHARED_SUCCESS)) {
    if (cpu_api->destroy_instance != nullptr) {
      cpu_api->destroy_instance(cpu_launch_instance);
    }
    mf_client_ring_close_v1(&submission);
    mf_client_ring_close_v1(&completion);
    return 1;
  }
  std::memcpy(destination.data(), payload.data() + 256U, sizeof(destination));
  if (destination[0] != 101U || destination[7] != 115U) {
    if (cpu_api->destroy_instance != nullptr) {
      cpu_api->destroy_instance(cpu_launch_instance);
    }
    mf_client_ring_close_v1(&submission);
    mf_client_ring_close_v1(&completion);
    return 1;
  }
  if (cpu_api->unload_module != nullptr) {
    cpu_api->unload_module(cpu_launch_instance, cpu_module);
  }
  if (cpu_api->free_memory != nullptr) {
    cpu_api->free_memory(cpu_launch_instance, cpu_launch_memory);
  }
  if (cpu_api->destroy_queue != nullptr) {
    cpu_api->destroy_queue(cpu_launch_instance, cpu_launch_queue);
  }
  if (cpu_api->destroy_context != nullptr) {
    cpu_api->destroy_context(cpu_launch_instance, cpu_launch_context);
  }
  if (cpu_api->destroy_instance != nullptr) {
    cpu_api->destroy_instance(cpu_launch_instance);
  }
#endif
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
