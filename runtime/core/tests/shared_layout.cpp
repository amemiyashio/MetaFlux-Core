#include "metaflux/shared/device.h"

#include <cstddef>
#include <cstdint>
#include <type_traits>

static_assert(std::is_standard_layout_v<mf_admission_attempt_record_v1>);
static_assert(std::is_standard_layout_v<mf_admission_lease_record_v1>);
static_assert(std::is_standard_layout_v<mf_device_validation_update_record_v1>);
static_assert(std::is_standard_layout_v<mf_view_publish_record_v1>);
static_assert(std::is_standard_layout_v<mf_lifecycle_range_record_v1>);
static_assert(std::is_standard_layout_v<mf_telemetry_publish_record_v1>);
static_assert(std::is_standard_layout_v<mf_shared_registry_extension_header_v1>);

static_assert(alignof(mf_admission_attempt_record_v1) == 64);
static_assert(alignof(mf_admission_lease_record_v1) == 64);
static_assert(alignof(mf_device_validation_update_record_v1) == 64);
static_assert(alignof(mf_view_publish_record_v1) == 64);
static_assert(alignof(mf_lifecycle_range_record_v1) == 64);
static_assert(alignof(mf_telemetry_publish_record_v1) == 64);
static_assert(alignof(mf_shared_registry_extension_header_v1) == 64);

static_assert(sizeof(mf_admission_attempt_record_v1) == 128);
static_assert(sizeof(mf_admission_lease_record_v1) == 192);
static_assert(sizeof(mf_device_validation_update_record_v1) == 192);
static_assert(sizeof(mf_view_publish_record_v1) == 256);
static_assert(sizeof(mf_lifecycle_range_record_v1) == 192);
static_assert(sizeof(mf_telemetry_publish_record_v1) == 256);
static_assert(sizeof(mf_shared_registry_extension_header_v1) == 256);
static_assert(sizeof(mf_virtual_device_telemetry_v1) == 128);

static_assert(offsetof(mf_admission_attempt_record_v1, target_lease_slot) == 64);
static_assert(offsetof(mf_admission_lease_record_v1, publication_marker) == 104);
static_assert(offsetof(mf_device_validation_update_record_v1, view_publish_slot) == 88);
static_assert(offsetof(mf_view_publish_record_v1, range_slot) == 160);
static_assert(offsetof(mf_lifecycle_range_record_v1, retirement_disposition) == 120);
static_assert(offsetof(mf_telemetry_publish_record_v1, operation_kind) == 152);
static_assert(offsetof(mf_shared_registry_extension_header_v1, view_publisher_control_offset) ==
              64);
static_assert(offsetof(mf_virtual_device_telemetry_v1, memory_active_time_ns) == 64);

int main() {
  const std::uint64_t value =
      mf_view_publish_state_pack_v1(UINT32_C(17), MF_VIEW_PUBLISH_ACTIVE, UINT32_C(5));
  return mf_tagged_record_tag_v1(value) == UINT32_C(17) &&
                 mf_tagged_record_state_v1(value) == MF_VIEW_PUBLISH_ACTIVE &&
                 mf_tagged_record_auxiliary_v1(value) == UINT32_C(5)
             ? 0
             : 1;
}
