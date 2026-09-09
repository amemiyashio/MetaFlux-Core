#include "metaflux/shared/device.h"

#include <stddef.h>
#include <stdint.h>

_Static_assert(_Alignof(mf_shared_registry_header_v1) == 64, "registry alignment");
_Static_assert(_Alignof(mf_virtual_device_identity_v1) == 64, "identity alignment");
_Static_assert(_Alignof(mf_ring_descriptor_v1) == 64, "descriptor alignment");
_Static_assert(_Alignof(mf_argument_block_header_v1) == 64, "argument header alignment");
_Static_assert(_Alignof(mf_admission_attempt_record_v1) == 64, "attempt alignment");
_Static_assert(_Alignof(mf_admission_lease_record_v1) == 64, "lease alignment");
_Static_assert(_Alignof(mf_device_validation_update_record_v1) == 64, "update alignment");
_Static_assert(_Alignof(mf_view_publish_record_v1) == 64, "view publish alignment");
_Static_assert(_Alignof(mf_lifecycle_range_record_v1) == 64, "range alignment");
_Static_assert(_Alignof(mf_telemetry_publish_record_v1) == 64, "telemetry publish alignment");
_Static_assert(_Alignof(mf_shared_registry_extension_header_v1) == 64, "extension alignment");
_Static_assert(sizeof(mf_argument_entry_v1) == 32, "argument entry size");
_Static_assert(MF_ARGUMENT_KIND_F32 == UINT32_C(4), "float32 argument kind");
_Static_assert(MF_ARGUMENT_BLOCK_KNOWN_FLAGS_V1 == UINT32_C(3), "argument block known flags");
_Static_assert(MF_RING_COPY_FLAG_REGION_ARGUMENT_BLOCK_V1 == UINT32_C(1), "copy region ring flag");
_Static_assert(MF_RING_COPY_FLAG_DIRECT_HOST_SOURCE_V1 == UINT32_C(2),
               "direct host source ring flag");
_Static_assert(MF_RING_COPY_FLAG_DIRECT_HOST_DESTINATION_V1 == UINT32_C(4),
               "direct host destination ring flag");
_Static_assert(MF_RING_COPY_KNOWN_FLAGS_V1 == UINT32_C(7), "known copy ring flags");
_Static_assert(MF_DEVICE_POLICY_PERSISTENCE_ENABLED_V1 == UINT64_C(1), "persistence policy bit");
_Static_assert(MF_DEVICE_POLICY_COMPUTE_MODE_MASK_V1 == UINT64_C(6), "compute policy mask");
_Static_assert(MF_DEVICE_POLICY_KNOWN_BITS_V1 == UINT64_C(7), "known policy bits");
_Static_assert(MF_COPY_REGION_ARGUMENT_ENTRY_COUNT_V1 == UINT32_C(3), "copy region entry count");
_Static_assert(MF_ARGUMENT_BLOCK_LAUNCH_GRID_X_INDEX_V1 == UINT32_C(0), "launch grid x index");
_Static_assert(MF_ARGUMENT_BLOCK_LAUNCH_GRID_Y_INDEX_V1 == UINT32_C(1), "launch grid y index");
_Static_assert(MF_ARGUMENT_BLOCK_LAUNCH_BLOCK_X_INDEX_V1 == UINT32_C(2), "launch block x index");
_Static_assert(MF_ARGUMENT_BLOCK_LAUNCH_BLOCK_Y_INDEX_V1 == UINT32_C(3), "launch block y index");
_Static_assert(sizeof(mf_admission_attempt_record_v1) == 128, "attempt size");
_Static_assert(sizeof(mf_admission_lease_record_v1) == 192, "lease size");
_Static_assert(sizeof(mf_device_validation_update_record_v1) == 192, "update size");
_Static_assert(sizeof(mf_view_publish_record_v1) == 256, "view publish size");
_Static_assert(sizeof(mf_lifecycle_range_record_v1) == 192, "range size");
_Static_assert(sizeof(mf_telemetry_publish_record_v1) == 256, "telemetry publish size");
_Static_assert(sizeof(mf_shared_registry_extension_header_v1) == 256, "extension size");
_Static_assert(sizeof(mf_virtual_device_telemetry_v1) == 128, "telemetry row size");
_Static_assert(offsetof(mf_shared_registry_header_v1, registry_view_id) == 24, "view ID offset");
_Static_assert(offsetof(mf_shared_registry_header_v1, identities_offset) == 72,
               "identity offset field");
_Static_assert(offsetof(mf_virtual_device_identity_v1, committed_generation) == 104,
               "identity generation offset");
_Static_assert(offsetof(mf_virtual_device_lifecycle_fence_v1, device_state) == 48,
               "fence state offset");
_Static_assert(offsetof(mf_virtual_device_telemetry_v1, memory_active_time_ns) == 64,
               "telemetry memory activity offset");
_Static_assert(offsetof(mf_argument_entry_v1, value) == 24, "argument value offset");
_Static_assert(offsetof(mf_admission_attempt_record_v1, target_lease_slot) == 64,
               "attempt target offset");
_Static_assert(offsetof(mf_admission_lease_record_v1, publication_marker) == 104,
               "lease publication marker offset");
_Static_assert(offsetof(mf_device_validation_update_record_v1, view_publish_slot) == 88,
               "update publish offset");
_Static_assert(offsetof(mf_view_publish_record_v1, range_slot) == 160, "view publish range offset");
_Static_assert(offsetof(mf_lifecycle_range_record_v1, retirement_disposition) == 120,
               "range disposition offset");
_Static_assert(offsetof(mf_telemetry_publish_record_v1, operation_kind) == 152,
               "telemetry publish kind offset");
_Static_assert(offsetof(mf_shared_registry_extension_header_v1, view_publisher_control_offset) ==
                   64,
               "extension control offset");

int main(void) {
  const uint64_t view = mf_view_admission_pack_v1(UINT64_C(41), MF_VIEW_ADMISSION_OPEN);
  const uint64_t device =
      mf_device_admission_pack_v1(UINT32_C(17), MF_DEVICE_ADMISSION_UPDATING, UINT32_C(91));
  const uint64_t telemetry = mf_telemetry_bank_state_pack_v1(UINT32_C(1), MF_TELEMETRY_STATE_READY);
  const uint64_t tagged =
      mf_admission_lease_state_pack_v1(UINT32_C(23), MF_ADMISSION_LEASE_INITIALIZING, UINT32_C(19));
  const uint64_t publisher =
      mf_publisher_control_pack_v1(UINT32_C(29), MF_PUBLISHER_WRITING, UINT32_C(7));
  uint64_t checked_sequence = UINT64_C(91);
  uint32_t checked_tag = UINT32_C(0);

  if (mf_view_admission_generation_v1(view) != UINT64_C(41) ||
      mf_view_admission_state_v1(view) != MF_VIEW_ADMISSION_OPEN ||
      mf_device_admission_generation_v1(device) != UINT32_C(17) ||
      mf_device_admission_state_v1(device) != MF_DEVICE_ADMISSION_UPDATING ||
      mf_device_admission_update_tag_v1(device) != UINT32_C(91) ||
      mf_telemetry_active_bank_v1(telemetry) != UINT32_C(1) ||
      mf_telemetry_state_v1(telemetry) != MF_TELEMETRY_STATE_READY ||
      mf_tagged_record_tag_v1(tagged) != UINT32_C(23) ||
      mf_tagged_record_state_v1(tagged) != MF_ADMISSION_LEASE_INITIALIZING ||
      mf_tagged_record_auxiliary_v1(tagged) != UINT32_C(19) ||
      mf_tagged_record_tag_v1(publisher) != UINT32_C(29) ||
      mf_tagged_record_state_v1(publisher) != MF_PUBLISHER_WRITING ||
      mf_tagged_record_auxiliary_v1(publisher) != UINT32_C(7) ||
      !mf_shared_checked_add_u64_below_terminal_v1(UINT64_C(4), UINT64_C(2), UINT64_C(7),
                                                   &checked_sequence) ||
      checked_sequence != UINT64_C(6) ||
      mf_shared_checked_add_u64_below_terminal_v1(UINT64_C(4), UINT64_C(3), UINT64_C(7),
                                                  &checked_sequence) ||
      !mf_shared_checked_next_record_tag_v1(MF_SHARED_RECORD_TAG_MAX_NORMAL - UINT32_C(1),
                                            &checked_tag) ||
      checked_tag != MF_SHARED_RECORD_TAG_MAX_NORMAL ||
      mf_shared_checked_next_record_tag_v1(MF_SHARED_RECORD_TAG_MAX_NORMAL, &checked_tag)) {
    return 1;
  }
  return 0;
}
