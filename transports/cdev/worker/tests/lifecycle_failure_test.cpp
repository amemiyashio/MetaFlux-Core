#include "metaflux/transport/cdev_worker.hpp"
#include "metaflux/uapi/transport.h"
#include "metaflux/client/fastpath.h"
#include <array>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <memory>

namespace {

#define REQUIRE(cond)                                                                                  \
  do {                                                                                                 \
    if (!(cond)) {                                                                                     \
      std::cerr << __func__ << ':' << __LINE__ << ": " #cond "\n";                                    \
      return false;                                                                                    \
    }                                                                                                  \
  } while (false)

struct BackendFixture {
  std::uint32_t retire_calls = 0U;
};

struct RebindFixture {
  metaflux::transport::cdev::WorkerQueueView view{};
  metaflux::transport::cdev::CdevBackendBinding backend{};
  std::uint32_t calls = 0U;
};

void fixture_retire(void* c) noexcept { auto* f = static_cast<BackendFixture*>(c); if (f) ++f->retire_calls; }
mf_shared_status_v1 fixture_lease_acquire(void* c) noexcept { (void)c; return MF_SHARED_SUCCESS; }
void fixture_lease_release(void* c) noexcept { (void)c; }
mf_backend_status_v1 fixture_copy(mf_backend_instance_v1, mf_backend_queue_v1,
                                  const mf_backend_copy_v1*, mf_backend_event_v1) { return MF_BACKEND_SUCCESS; }

bool fixture_rebind(void* c, std::uint64_t gen, metaflux::transport::cdev::WorkerQueueView* ov,
                    metaflux::transport::cdev::CdevBackendBinding* ob) noexcept {
  auto* f = static_cast<RebindFixture*>(c);
  if (!f || !ov || !ob) return false;
  ++f->calls; *ov = f->view; *ob = f->backend;
  ov->generation = gen; ob->generation = gen; return true;
}

namespace cdev = metaflux::transport::cdev;
namespace lc = metaflux::runtime::lifecycle;

struct TestContext {
  mf_client_ring_v1 sub{}, comp{}, rsub{}, rcomp{};
  BackendFixture bf{};
  RebindFixture rebind{};
  mf_backend_api_v1 api{};
  std::array<std::uint8_t, 512> payload{};
  std::unique_ptr<lc::Coordinator> coord;

  bool init() {
    mf_registry_view_id_v1 view{.daemon_incarnation = 7U, .view_serial = 9U};
    if (mf_client_ring_create_v1(8U, view, 1U, 4U, &sub) != MF_SHARED_SUCCESS ||
        mf_client_ring_create_v1(8U, view, 2U, 4U, &comp) != MF_SHARED_SUCCESS ||
        mf_client_ring_create_v1(8U, view, 1U, 5U, &rsub) != MF_SHARED_SUCCESS ||
        mf_client_ring_create_v1(8U, view, 2U, 5U, &rcomp) != MF_SHARED_SUCCESS) return false;
    api.header.abi_version = MF_BACKEND_ABI_VERSION_1;
    api.header.struct_size = sizeof(api);
    api.header.capabilities = MF_BACKEND_CAP_COPY;
    api.copy = fixture_copy;
    rebind.view = {.submission = rsub.header, .completion = rcomp.header,
      .payload = payload.data(), .payload_size = payload.size(), .generation = 5U};
    rebind.backend = {.api = &api,
      .instance = static_cast<mf_backend_instance_v1>(reinterpret_cast<std::uintptr_t>(&bf)),
      .queue = 17U, .memory = 23U,
      .lease_acquire = fixture_lease_acquire, .lease_release = fixture_lease_release,
      .lease_context = &bf, .retire = fixture_retire, .retire_context = &bf, .generation = 5U};
    lc::Config c{}; c.logical_device_id = 7U; c.daemon_incarnation = 7U;
    c.initial_identity_record_id = 4U; c.initial_generation = 4U; c.initial_epoch = 1U;
    c.generation_terminal = 32U; c.identity_record_terminal = 32U; c.epoch_terminal = 32U;
    coord = std::make_unique<lc::Coordinator>(c);
    return true;
  }

  cdev::CdevBackendBinding make_binding(std::uint64_t gen) {
    return {.api = &api,
      .instance = static_cast<mf_backend_instance_v1>(reinterpret_cast<std::uintptr_t>(&bf)),
      .queue = 17U, .memory = 23U,
      .lease_acquire = fixture_lease_acquire, .lease_release = fixture_lease_release,
      .lease_context = &bf, .retire = fixture_retire, .retire_context = &bf, .generation = gen,
      .rebind = fixture_rebind, .rebind_context = &rebind};
  }

  cdev::CdevWorker make_worker(std::uint64_t gen) {
    return {{.submission = sub.header, .completion = comp.header,
      .payload = payload.data(), .payload_size = payload.size(), .generation = gen},
      make_binding(gen)};
  }

  void close() {
    mf_client_ring_close_v1(&rsub); mf_client_ring_close_v1(&rcomp);
    mf_client_ring_close_v1(&sub); mf_client_ring_close_v1(&comp);
  }
};

lc::Request reset_req(std::uint64_t id, std::uint64_t gen, std::uint64_t epoch) {
  return {.request_id = id, .logical_device_id = 7U, .daemon_incarnation = 7U,
    .expected_identity_record_id = gen, .expected_generation = gen, .expected_epoch = epoch,
    .source = lc::Source::Admin, .operation = lc::Operation::Reset};
}

// Test 1: Reset succeeds when no pending operations.
bool reset_without_pending_succeeds() {
  TestContext ctx{};
  if (!ctx.init()) return false;
  auto w = ctx.make_worker(4U);
  REQUIRE(w.attach_lifecycle(*ctx.coord));
  REQUIRE(ctx.coord->apply(reset_req(900U, 4U, 1U)) == lc::Result::Accepted);
  REQUIRE(w.generation() == 5U);
  REQUIRE(w.lifecycle_online());
  REQUIRE(ctx.bf.retire_calls == 1U);
  REQUIRE(ctx.rebind.calls == 1U);
  ctx.close();
  return true;
}

// Test 2: Stale generation rejected after lifecycle commit.
bool stale_generation_rejected_after_reset() {
  TestContext ctx{};
  if (!ctx.init()) return false;
  auto w = ctx.make_worker(4U);
  REQUIRE(w.attach_lifecycle(*ctx.coord));
  REQUIRE(ctx.coord->apply(reset_req(901U, 4U, 1U)) == lc::Result::Accepted);
  REQUIRE(w.generation() == 5U);
  // Worker is online at gen 5, accepting new work.
  REQUIRE(w.lifecycle_online());
  ctx.close();
  return true;
}

// Test 3: Remove transitions to offline.
bool remove_transitions_to_absent() {
  TestContext ctx{};
  if (!ctx.init()) return false;
  auto w = ctx.make_worker(4U);
  REQUIRE(w.attach_lifecycle(*ctx.coord));
  lc::Request remove{.request_id = 905U, .logical_device_id = 7U, .daemon_incarnation = 7U,
    .expected_identity_record_id = 4U, .expected_generation = 4U, .expected_epoch = 1U,
    .source = lc::Source::Admin, .operation = lc::Operation::Remove};
  REQUIRE(ctx.coord->apply(remove) == lc::Result::Accepted);
  REQUIRE(!w.lifecycle_online());
  REQUIRE(w.generation() == 0U);
  ctx.close();
  return true;
}

// Test 5: Transport loss transitions to lost.
bool transport_loss_transitions_to_lost() {
  TestContext ctx{};
  if (!ctx.init()) return false;
  auto w = ctx.make_worker(4U);
  REQUIRE(w.attach_lifecycle(*ctx.coord));
  lc::ResultDetails details{};
  REQUIRE(w.report_disconnect(*ctx.coord, 7U, 19U, details) ==
          metaflux::runtime::lifecycle::NormalizationResult::Accepted);
  REQUIRE(details.result == lc::Result::Accepted);
  REQUIRE(!w.lifecycle_online());
  ctx.close();
  return true;
}

} // namespace

int main() {
  return reset_without_pending_succeeds() && stale_generation_rejected_after_reset() &&
                 remove_transitions_to_absent() && transport_loss_transitions_to_lost()
             ? 0
             : 1;
}
