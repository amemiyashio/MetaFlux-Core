#include <metaflux/transport/msix.hpp>

#include <array>
#include <cstdint>

namespace {

struct InjectionFixture final {
  std::array<std::uint32_t, metaflux::transport::vfio_user::MsixNotificationLedger::kVectorCount>
      calls{};
  bool fail = false;
};

bool inject(void* context, std::uint32_t vector) {
  auto* fixture = static_cast<InjectionFixture*>(context);
  if (fixture == nullptr || vector >= fixture->calls.size() || fixture->fail) {
    return false;
  }
  ++fixture->calls[vector];
  return true;
}

bool notification_contract() {
  using metaflux::transport::vfio_user::MsixNotificationLedger;
  using metaflux::transport::vfio_user::MsixStatus;
  InjectionFixture fixture{};
  MsixNotificationLedger ledger(9U, inject, &fixture);
  MsixStatus status = ledger.arm_completion(9U, 10U);
  if (status != MsixStatus::success || ledger.notify(9U, 1U, 9U) != MsixStatus::disarmed ||
      ledger.notify(9U, 1U, 10U) != MsixStatus::success || fixture.calls[1] != 1U ||
      ledger.notify(9U, 1U, 11U) != MsixStatus::disarmed ||
      ledger.arm_completion(9U, 11U) != MsixStatus::success) {
    return false;
  }

  if (ledger.set_mask(9U, 0U, true) != MsixStatus::success ||
      ledger.notify(9U, 0U, 1U) != MsixStatus::masked || fixture.calls[0] != 0U ||
      ledger.set_mask(9U, 0U, false) != MsixStatus::success || fixture.calls[0] != 1U) {
    return false;
  }

  fixture.fail = true;
  if (ledger.notify(9U, 0U, 2U) != MsixStatus::injection_failed ||
      ledger.retry_pending(9U, 0U) != MsixStatus::injection_failed) {
    return false;
  }
  fixture.fail = false;
  if (ledger.retry_pending(9U, 0U) != MsixStatus::success || fixture.calls[0] != 2U) {
    return false;
  }

  metaflux::transport::vfio_user::MsixVectorState snapshot{};
  return ledger.snapshot(9U, 0U, &snapshot) == MsixStatus::success &&
         snapshot.delivered_count == 2U && !snapshot.pending &&
         ledger.mark_lost(9U) == MsixStatus::success &&
         ledger.notify(9U, 0U, 3U) == MsixStatus::device_lost &&
         ledger.notify(8U, 0U, 3U) == MsixStatus::stale_generation &&
         ledger.set_mask(9U, 2U, true) == MsixStatus::invalid_argument;
}

bool overflow_and_coalescing() {
  using metaflux::transport::vfio_user::MsixNotificationLedger;
  using metaflux::transport::vfio_user::MsixStatus;
  InjectionFixture fixture{};
  MsixNotificationLedger ledger(3U, inject, &fixture);
  if (ledger.set_mask(3U, 0U, true) != MsixStatus::success ||
      ledger.notify(3U, 0U, UINT64_MAX) != MsixStatus::masked ||
      ledger.notify(3U, 0U, 4U) != MsixStatus::masked ||
      ledger.set_mask(3U, 0U, false) != MsixStatus::success) {
    return false;
  }
  metaflux::transport::vfio_user::MsixVectorState state{};
  return ledger.snapshot(3U, 0U, &state) == MsixStatus::success &&
         state.last_delivered_timeline == UINT64_MAX && state.delivered_count == 1U;
}

} // namespace

int main() { return notification_contract() && overflow_and_coalescing() ? 0 : 1; }
