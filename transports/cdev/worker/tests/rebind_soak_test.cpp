#include "metaflux/transport/cdev_worker.hpp"
#include "metaflux/uapi/transport.h"
#include "metaflux/client/fastpath.h"

#include <array>
#include <cstdint>
#include <iostream>
#include <memory>
#include <vector>

namespace {

#define REQUIRE(cond)                                                                              \
  do {                                                                                             \
    if (!(cond)) {                                                                                 \
      std::cerr << __func__ << ':' << __LINE__ << ": " #cond "\n";                                \
      return false;                                                                                \
    }                                                                                              \
  } while (false)

namespace cdev = metaflux::transport::cdev;
namespace lc = metaflux::runtime::lifecycle;

constexpr std::uint32_t kSoakCycles = 256U;
constexpr std::uint32_t kRingCapacity = 8U;

struct BackendFixture final {
  std::uint32_t calls = 0U;
  std::uint32_t lease_acquires = 0U;
  std::uint32_t lease_releases = 0U;
  std::uint32_t retire_calls = 0U;
  bool lease_active = false;
};

struct OwnedRings final {
  mf_client_ring_v1 submission{};
  mf_client_ring_v1 completion{};
  bool open = false;

  OwnedRings() = default;
  OwnedRings(const OwnedRings&) = delete;
  OwnedRings& operator=(const OwnedRings&) = delete;
  OwnedRings(OwnedRings&& other) noexcept
      : submission(other.submission), completion(other.completion), open(other.open) {
    other.open = false;
    other.submission = {};
    other.completion = {};
  }
  OwnedRings& operator=(OwnedRings&& other) noexcept {
    if (this != &other) {
      close();
      submission = other.submission;
      completion = other.completion;
      open = other.open;
      other.open = false;
      other.submission = {};
      other.completion = {};
    }
    return *this;
  }
  ~OwnedRings() { close(); }

  void close() noexcept {
    if (open) {
      mf_client_ring_close_v1(&submission);
      mf_client_ring_close_v1(&completion);
      open = false;
    }
  }
};

struct RebindFixture final {
  BackendFixture* backend_fixture = nullptr;
  mf_backend_api_v1* api = nullptr;
  std::array<std::uint8_t, 512>* payload = nullptr;
  // Keep every generation's rings alive until the soak ends. Closing the previous
  // pair during prepare would invalidate the worker's still-active view pointers.
  std::vector<OwnedRings> generations{};
  std::uint32_t calls = 0U;

  OwnedRings* current() noexcept {
    return generations.empty() ? nullptr : &generations.back();
  }

  void close_all() noexcept {
    for (auto& rings : generations) {
      rings.close();
    }
    generations.clear();
  }
};

mf_shared_status_v1 fixture_lease_acquire(void* context) noexcept {
  auto* fixture = static_cast<BackendFixture*>(context);
  if (fixture == nullptr || fixture->lease_active) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  ++fixture->lease_acquires;
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

void fixture_retire(void* context) noexcept {
  auto* fixture = static_cast<BackendFixture*>(context);
  if (fixture != nullptr) {
    ++fixture->retire_calls;
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
  return MF_BACKEND_SUCCESS;
}

bool fixture_rebind(void* context, std::uint64_t generation, cdev::WorkerQueueView* out_view,
                    cdev::CdevBackendBinding* out_backend) noexcept {
  auto* fixture = static_cast<RebindFixture*>(context);
  if (fixture == nullptr || fixture->backend_fixture == nullptr || fixture->api == nullptr ||
      fixture->payload == nullptr || out_view == nullptr || out_backend == nullptr ||
      generation == 0U) {
    return false;
  }
  mf_registry_view_id_v1 view{.daemon_incarnation = 7U, .view_serial = 9U};
  OwnedRings next{};
  if (mf_client_ring_create_v1(kRingCapacity, view, 1U, generation, &next.submission) !=
          MF_SHARED_SUCCESS ||
      mf_client_ring_create_v1(kRingCapacity, view, 2U, generation, &next.completion) !=
          MF_SHARED_SUCCESS) {
    next.close();
    return false;
  }
  next.open = true;
  try {
    fixture->generations.push_back(std::move(next));
  } catch (...) {
    next.close();
    return false;
  }
  auto* current = fixture->current();
  if (current == nullptr) {
    return false;
  }
  ++fixture->calls;
  *out_view = {
      .submission = current->submission.header,
      .completion = current->completion.header,
      .payload = fixture->payload->data(),
      .payload_size = fixture->payload->size(),
      .generation = generation,
  };
  *out_backend = {
      .api = fixture->api,
      .instance = static_cast<mf_backend_instance_v1>(
          reinterpret_cast<std::uintptr_t>(fixture->backend_fixture)),
      .queue = 17U,
      .memory = 23U,
      .lease_acquire = fixture_lease_acquire,
      .lease_release = fixture_lease_release,
      .lease_context = fixture->backend_fixture,
      .retire = fixture_retire,
      .retire_context = fixture->backend_fixture,
      .generation = generation,
      .rebind = fixture_rebind,
      .rebind_context = fixture,
  };
  return true;
}

struct SoakContext final {
  mf_client_ring_v1 initial_submission{};
  mf_client_ring_v1 initial_completion{};
  BackendFixture backend{};
  RebindFixture rebind{};
  mf_backend_api_v1 api{};
  std::array<std::uint8_t, 512> payload{};
  std::unique_ptr<lc::Coordinator> coordinator;
  bool rings_open = false;

  bool init() {
    mf_registry_view_id_v1 view{.daemon_incarnation = 7U, .view_serial = 9U};
    if (mf_client_ring_create_v1(kRingCapacity, view, 1U, 1U, &initial_submission) !=
            MF_SHARED_SUCCESS ||
        mf_client_ring_create_v1(kRingCapacity, view, 2U, 1U, &initial_completion) !=
            MF_SHARED_SUCCESS) {
      return false;
    }
    rings_open = true;
    api.header.abi_version = MF_BACKEND_ABI_VERSION_1;
    api.header.struct_size = sizeof(api);
    api.header.capabilities = MF_BACKEND_CAP_COPY;
    api.copy = fixture_copy;
    rebind.backend_fixture = &backend;
    rebind.api = &api;
    rebind.payload = &payload;
    lc::Config config{};
    config.logical_device_id = 7U;
    config.daemon_incarnation = 7U;
    config.initial_identity_record_id = 1U;
    config.initial_generation = 1U;
    config.initial_epoch = 1U;
    config.generation_terminal = 4096U;
    config.identity_record_terminal = 4096U;
    config.epoch_terminal = 4096U;
    coordinator = std::make_unique<lc::Coordinator>(config);
    return coordinator != nullptr && coordinator->valid();
  }

  cdev::CdevWorker make_worker() {
    return {{.submission = initial_submission.header,
             .completion = initial_completion.header,
             .payload = payload.data(),
             .payload_size = payload.size(),
             .generation = 1U},
            {.api = &api,
             .instance =
                 static_cast<mf_backend_instance_v1>(reinterpret_cast<std::uintptr_t>(&backend)),
             .queue = 17U,
             .memory = 23U,
             .lease_acquire = fixture_lease_acquire,
             .lease_release = fixture_lease_release,
             .lease_context = &backend,
             .retire = fixture_retire,
             .retire_context = &backend,
             .generation = 1U,
             .rebind = fixture_rebind,
             .rebind_context = &rebind}};
  }

  void close() {
    rebind.close_all();
    if (rings_open) {
      mf_client_ring_close_v1(&initial_submission);
      mf_client_ring_close_v1(&initial_completion);
      rings_open = false;
    }
  }
};

lc::Request make_reset(std::uint64_t request_id, const lc::Snapshot& snapshot) {
  return {.request_id = request_id,
          .logical_device_id = snapshot.logical_device_id,
          .daemon_incarnation = snapshot.daemon_incarnation,
          .expected_identity_record_id = snapshot.identity_record_id,
          .expected_generation = snapshot.generation,
          .expected_epoch = snapshot.epoch,
          .source = lc::Source::Admin,
          .operation = lc::Operation::Reset};
}

bool submit_and_complete_copy(cdev::CdevWorker& worker, mf_client_ring_v1* submission,
                              mf_client_ring_v1* completion, std::uint64_t generation,
                              std::uint64_t request_id) {
  mf_ring_descriptor_v1 request{};
  mf_ring_descriptor_v1 result{};
  request.opcode = MF_RING_OPCODE_COPY;
  request.request_id = request_id;
  request.target_id = generation;
  request.arguments[0] = 0U;
  request.arguments[1] = 64U;
  request.arguments[2] = 32U;
  if (mf_client_ring_try_submit_v1(submission, &request) != MF_SHARED_SUCCESS) {
    return false;
  }
  if (worker.consume_once() != cdev::WorkerResult::Completed) {
    return false;
  }
  if (mf_client_ring_try_consume_v1(completion, &result) != MF_SHARED_SUCCESS) {
    return false;
  }
  return result.request_id == request_id &&
         result.arguments[0] == static_cast<std::uint64_t>(MF_SHARED_SUCCESS);
}

bool generation_replacement_soak_under_interleaved_work() {
  SoakContext context{};
  REQUIRE(context.init());
  auto worker = context.make_worker();
  REQUIRE(worker.attach_lifecycle(*context.coordinator));
  REQUIRE(worker.lifecycle_online());
  REQUIRE(worker.generation() == 1U);

  std::uint64_t request_id = 1U;
  mf_client_ring_v1* active_submission = &context.initial_submission;
  mf_client_ring_v1* active_completion = &context.initial_completion;

  for (std::uint32_t cycle = 0U; cycle < kSoakCycles; ++cycle) {
    const lc::Snapshot before = context.coordinator->snapshot();
    REQUIRE(before.state == lc::State::Online);
    REQUIRE(before.generation == worker.generation());
    REQUIRE(submit_and_complete_copy(worker, active_submission, active_completion, before.generation,
                                     request_id++));

    const lc::Request reset = make_reset(request_id++, before);
    REQUIRE(context.coordinator->apply(reset) == lc::Result::Accepted);
    const lc::Snapshot after = context.coordinator->snapshot();
    REQUIRE(after.state == lc::State::Online);
    REQUIRE(after.generation == before.generation + 1U);
    REQUIRE(worker.generation() == after.generation);
    REQUIRE(worker.lifecycle_online());
    REQUIRE(worker.backend_bound());
    REQUIRE(context.rebind.calls == cycle + 1U);
    REQUIRE(context.backend.retire_calls == cycle + 1U);
    REQUIRE(!context.backend.lease_active);
    REQUIRE(context.backend.lease_acquires == context.backend.lease_releases);
    auto* current = context.rebind.current();
    REQUIRE(current != nullptr);
    REQUIRE(current->open);

    active_submission = &current->submission;
    active_completion = &current->completion;
    REQUIRE(submit_and_complete_copy(worker, active_submission, active_completion, after.generation,
                                     request_id++));
  }

  REQUIRE(context.rebind.calls == kSoakCycles);
  REQUIRE(context.backend.retire_calls == kSoakCycles);
  REQUIRE(context.backend.calls == kSoakCycles * 2U);
  REQUIRE(context.coordinator->snapshot().generation == 1U + kSoakCycles);
  context.close();
  return true;
}

bool stale_generation_after_many_replacements_is_rejected() {
  SoakContext context{};
  REQUIRE(context.init());
  auto worker = context.make_worker();
  REQUIRE(worker.attach_lifecycle(*context.coordinator));

  std::uint64_t request_id = 1000U;
  for (std::uint32_t cycle = 0U; cycle < 8U; ++cycle) {
    const lc::Snapshot before = context.coordinator->snapshot();
    REQUIRE(context.coordinator->apply(make_reset(request_id++, before)) == lc::Result::Accepted);
  }
  REQUIRE(worker.generation() == 9U);
  auto* current = context.rebind.current();
  REQUIRE(current != nullptr);
  REQUIRE(current->open);

  mf_ring_descriptor_v1 request{};
  mf_ring_descriptor_v1 result{};
  request.opcode = MF_RING_OPCODE_COPY;
  request.request_id = request_id++;
  request.target_id = 1U; // original generation, now stale
  request.arguments[0] = 0U;
  request.arguments[1] = 16U;
  request.arguments[2] = 8U;
  REQUIRE(mf_client_ring_try_submit_v1(&current->submission, &request) == MF_SHARED_SUCCESS);
  REQUIRE(worker.consume_once() == cdev::WorkerResult::Completed);
  REQUIRE(mf_client_ring_try_consume_v1(&current->completion, &result) == MF_SHARED_SUCCESS);
  REQUIRE(result.arguments[0] == static_cast<std::uint64_t>(MF_SHARED_STALE_HANDLE));

  request.request_id = request_id++;
  request.target_id = worker.generation();
  REQUIRE(mf_client_ring_try_submit_v1(&current->submission, &request) == MF_SHARED_SUCCESS);
  REQUIRE(worker.consume_once() == cdev::WorkerResult::Completed);
  REQUIRE(mf_client_ring_try_consume_v1(&current->completion, &result) == MF_SHARED_SUCCESS);
  REQUIRE(result.arguments[0] == static_cast<std::uint64_t>(MF_SHARED_SUCCESS));
  context.close();
  return true;
}

bool counter_wrap_adjacent_generation_request_is_stale() {
  // Prove the worker rejects a generation that is numerically adjacent across a
  // large replacement window rather than treating request IDs as the identity.
  SoakContext context{};
  REQUIRE(context.init());
  auto worker = context.make_worker();
  REQUIRE(worker.attach_lifecycle(*context.coordinator));

  std::uint64_t request_id = 2000U;
  for (std::uint32_t cycle = 0U; cycle < 16U; ++cycle) {
    const lc::Snapshot before = context.coordinator->snapshot();
    REQUIRE(context.coordinator->apply(make_reset(request_id++, before)) == lc::Result::Accepted);
  }
  const std::uint64_t live = worker.generation();
  REQUIRE(live == 17U);
  auto* current = context.rebind.current();
  REQUIRE(current != nullptr);

  mf_ring_descriptor_v1 request{};
  mf_ring_descriptor_v1 result{};
  request.opcode = MF_RING_OPCODE_COPY;
  request.request_id = UINT64_MAX - 3U;
  request.target_id = live - 1U;
  request.arguments[0] = 0U;
  request.arguments[1] = 8U;
  request.arguments[2] = 4U;
  REQUIRE(mf_client_ring_try_submit_v1(&current->submission, &request) == MF_SHARED_SUCCESS);
  REQUIRE(worker.consume_once() == cdev::WorkerResult::Completed);
  REQUIRE(mf_client_ring_try_consume_v1(&current->completion, &result) == MF_SHARED_SUCCESS);
  REQUIRE(result.request_id == UINT64_MAX - 3U);
  REQUIRE(result.arguments[0] == static_cast<std::uint64_t>(MF_SHARED_STALE_HANDLE));
  context.close();
  return true;
}

} // namespace

int main() {
  return generation_replacement_soak_under_interleaved_work() &&
                 stale_generation_after_many_replacements_is_rejected() &&
                 counter_wrap_adjacent_generation_request_is_stale()
             ? 0
             : 1;
}
