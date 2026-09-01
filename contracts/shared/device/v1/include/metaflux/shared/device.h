#ifndef METAFLUX_SHARED_DEVICE_H
#define METAFLUX_SHARED_DEVICE_H

#ifdef __KERNEL__
#include <linux/stddef.h>
#include <linux/types.h>
#ifndef UINT32_C
#define UINT32_C(value) value##U
#endif
#ifndef UINT64_C
#define UINT64_C(value) value##ULL
#endif
#ifndef INT32_C
#define INT32_C(value) value
#endif
#else
#include <stddef.h>
#include <stdint.h>
#endif

#include "metaflux/shared/atomic.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MF_SHARED_DEVICE_ABI_VERSION_1 UINT32_C(1)
#define MF_SHARED_CACHE_LINE_SIZE UINT32_C(64)

#define MF_SHARED_REGISTRY_MAGIC UINT32_C(0x3152464d)
#define MF_SHARED_REGISTRY_EXTENSION_MAGIC UINT32_C(0x3158464d)
#define MF_SHARED_RING_MAGIC UINT32_C(0x3151464d)
#define MF_SHARED_ARGUMENT_BLOCK_MAGIC UINT32_C(0x3141464d)

#define MF_SHARED_REGISTRY_FLAG_RECOVERY_TABLES_V1 UINT32_C(0x00000001)
#define MF_SHARED_REGISTRY_KNOWN_FLAGS_V1 MF_SHARED_REGISTRY_FLAG_RECOVERY_TABLES_V1

#define MF_SHARED_ADMISSION_ATTEMPT_CAPACITY_V1 UINT32_C(16)
#define MF_SHARED_ADMISSION_LEASE_CAPACITY_V1 UINT32_C(32)
#define MF_SHARED_DEVICE_UPDATE_CAPACITY_V1 UINT32_C(16)
#define MF_SHARED_LIFECYCLE_RANGE_CAPACITY_V1 UINT32_C(16)
#define MF_SHARED_VIEW_PUBLISH_CAPACITY_V1 UINT32_C(18)
#define MF_SHARED_VIEW_PUBLISH_ORDINARY_CAPACITY_V1 UINT32_C(16)
#define MF_SHARED_TELEMETRY_PUBLISH_CAPACITY_V1 UINT32_C(9)
#define MF_SHARED_TELEMETRY_PUBLISH_ORDINARY_CAPACITY_V1 UINT32_C(8)

#define MF_SHARED_SUCCESS INT32_C(0)
#define MF_SHARED_WOULD_BLOCK INT32_C(1)
#define MF_SHARED_TIMEOUT INT32_C(2)
#define MF_SHARED_INTERRUPTED INT32_C(3)
#define MF_SHARED_RETRY INT32_C(4)
#define MF_SHARED_STALE_HANDLE INT32_C(5)
#define MF_SHARED_DEVICE_LOST INT32_C(6)
#define MF_SHARED_TERMINAL_VIEW INT32_C(7)
#define MF_SHARED_INVALID_ARGUMENT INT32_C(8)
#define MF_SHARED_MALFORMED INT32_C(9)
#define MF_SHARED_OVERFLOW INT32_C(10)
#define MF_SHARED_RESOURCE_EXHAUSTED INT32_C(11)
#define MF_SHARED_SYSTEM_ERROR INT32_C(12)
#define MF_SHARED_NOT_SUPPORTED INT32_C(13)
#define MF_SHARED_PERMISSION_DENIED INT32_C(14)

typedef int32_t mf_shared_status_v1;

#define MF_VIEW_ADMISSION_OPEN UINT32_C(1)
#define MF_VIEW_ADMISSION_CLOSING UINT32_C(2)
#define MF_VIEW_ADMISSION_TERMINAL UINT32_C(3)
#define MF_VIEW_ADMISSION_QUARANTINED UINT32_C(4)

#define MF_DEVICE_ADMISSION_OPEN UINT32_C(1)
#define MF_DEVICE_ADMISSION_UPDATING UINT32_C(2)
#define MF_DEVICE_ADMISSION_CLOSED UINT32_C(3)

#define MF_DEVICE_STATE_OFFLINE UINT32_C(0)
#define MF_DEVICE_STATE_ONLINE UINT32_C(1)
#define MF_DEVICE_STATE_LOST UINT32_C(2)
#define MF_DEVICE_STATE_REMOVED UINT32_C(3)

#define MF_DEVICE_POLICY_PERSISTENCE_ENABLED_V1 (UINT64_C(1) << 0U)
#define MF_DEVICE_POLICY_COMPUTE_MODE_SHIFT_V1 UINT32_C(1)
#define MF_DEVICE_POLICY_COMPUTE_MODE_MASK_V1 (UINT64_C(3) << 1U)
#define MF_DEVICE_POLICY_COMPUTE_MODE_DEFAULT_V1 UINT64_C(0)
#define MF_DEVICE_POLICY_COMPUTE_MODE_EXCLUSIVE_THREAD_V1 UINT64_C(1)
#define MF_DEVICE_POLICY_COMPUTE_MODE_PROHIBITED_V1 UINT64_C(2)
#define MF_DEVICE_POLICY_COMPUTE_MODE_EXCLUSIVE_PROCESS_V1 UINT64_C(3)
#define MF_DEVICE_POLICY_KNOWN_BITS_V1                                                             \
  (MF_DEVICE_POLICY_PERSISTENCE_ENABLED_V1 | MF_DEVICE_POLICY_COMPUTE_MODE_MASK_V1)

#define MF_VIEW_GATE_OPEN UINT32_C(1)
#define MF_VIEW_GATE_CLOSING UINT32_C(2)
#define MF_VIEW_GATE_TERMINAL UINT32_C(3)

#define MF_TELEMETRY_STATE_UNAVAILABLE UINT32_C(0)
#define MF_TELEMETRY_STATE_READY UINT32_C(1)
#define MF_TELEMETRY_STATE_TERMINAL UINT32_C(2)

#define MF_SHARED_RECORD_TAG_MAX_NORMAL UINT32_C(0x00fffffe)
#define MF_SHARED_RECORD_TAG_TERMINAL UINT32_C(0x00ffffff)
#define MF_SHARED_RECORD_SLOT_NONE UINT32_C(0x00ffffff)

#define MF_PUBLISHER_IDLE UINT32_C(0)
#define MF_PUBLISHER_WRITING UINT32_C(1)
#define MF_PUBLISHER_QUARANTINED UINT32_C(2)

#define MF_ADMISSION_ATTEMPT_IDLE UINT32_C(0)
#define MF_ADMISSION_ATTEMPT_ENTERING UINT32_C(1)
#define MF_ADMISSION_ATTEMPT_CLAIMING UINT32_C(2)
#define MF_ADMISSION_ATTEMPT_INITIALIZING UINT32_C(3)
#define MF_ADMISSION_ATTEMPT_READY UINT32_C(4)
#define MF_ADMISSION_ATTEMPT_EXITED UINT32_C(5)

#define MF_ADMISSION_LEASE_FREE UINT32_C(0)
#define MF_ADMISSION_LEASE_INITIALIZING UINT32_C(1)
#define MF_ADMISSION_LEASE_RESERVED UINT32_C(2)
#define MF_ADMISSION_LEASE_COMMITTING UINT32_C(3)
#define MF_ADMISSION_LEASE_COMMITTED UINT32_C(4)
#define MF_ADMISSION_LEASE_PUBLISHED UINT32_C(5)
#define MF_ADMISSION_LEASE_RELEASED UINT32_C(6)
#define MF_ADMISSION_LEASE_REVOKED UINT32_C(7)
#define MF_ADMISSION_LEASE_TOMBSTONED UINT32_C(8)
#define MF_ADMISSION_LEASE_QUARANTINED UINT32_C(9)

#define MF_DEVICE_UPDATE_FREE UINT32_C(0)
#define MF_DEVICE_UPDATE_INITIALIZING UINT32_C(1)
#define MF_DEVICE_UPDATE_PREPARED UINT32_C(2)
#define MF_DEVICE_UPDATE_LINKING UINT32_C(3)
#define MF_DEVICE_UPDATE_ACTIVE UINT32_C(4)
#define MF_DEVICE_UPDATE_FENCE_PUBLISHED UINT32_C(5)
#define MF_DEVICE_UPDATE_REOPENED UINT32_C(6)
#define MF_DEVICE_UPDATE_ABORTED UINT32_C(7)
#define MF_DEVICE_UPDATE_CLOSED UINT32_C(8)
#define MF_DEVICE_UPDATE_TERMINAL UINT32_C(9)
#define MF_DEVICE_UPDATE_QUARANTINED UINT32_C(10)

#define MF_VIEW_PUBLISH_FREE UINT32_C(0)
#define MF_VIEW_PUBLISH_INITIALIZING UINT32_C(1)
#define MF_VIEW_PUBLISH_PREPARED UINT32_C(2)
#define MF_VIEW_PUBLISH_LINKING UINT32_C(3)
#define MF_VIEW_PUBLISH_ACTIVE UINT32_C(4)
#define MF_VIEW_PUBLISH_PUBLISHED UINT32_C(5)
#define MF_VIEW_PUBLISH_ABORTED UINT32_C(6)
#define MF_VIEW_PUBLISH_TERMINAL UINT32_C(7)
#define MF_VIEW_PUBLISH_QUARANTINED UINT32_C(8)

#define MF_VIEW_PUBLISH_KIND_NORMAL UINT32_C(1)
#define MF_VIEW_PUBLISH_KIND_RANGE_RESERVE UINT32_C(2)
#define MF_VIEW_PUBLISH_KIND_RANGE_RETIRE UINT32_C(3)
#define MF_VIEW_PUBLISH_KIND_CLOSE_CLOSING UINT32_C(4)
#define MF_VIEW_PUBLISH_KIND_CLOSE_TERMINAL UINT32_C(5)

#define MF_LIFECYCLE_RANGE_FREE UINT32_C(0)
#define MF_LIFECYCLE_RANGE_INITIALIZING UINT32_C(1)
#define MF_LIFECYCLE_RANGE_PREPARED UINT32_C(2)
#define MF_LIFECYCLE_RANGE_OPEN UINT32_C(3)
#define MF_LIFECYCLE_RANGE_RETIRED UINT32_C(4)
#define MF_LIFECYCLE_RANGE_TERMINAL UINT32_C(5)
#define MF_LIFECYCLE_RANGE_QUARANTINED UINT32_C(6)

#define MF_LIFECYCLE_RANGE_DISPOSITION_NONE UINT32_C(0)
#define MF_LIFECYCLE_RANGE_DISPOSITION_WHOLE UINT32_C(1)
#define MF_LIFECYCLE_RANGE_DISPOSITION_SUFFIX UINT32_C(2)
#define MF_LIFECYCLE_RANGE_DISPOSITION_COMPLETE UINT32_C(3)

#define MF_TELEMETRY_PUBLISH_FREE UINT32_C(0)
#define MF_TELEMETRY_PUBLISH_INITIALIZING UINT32_C(1)
#define MF_TELEMETRY_PUBLISH_PREPARED UINT32_C(2)
#define MF_TELEMETRY_PUBLISH_LINKING UINT32_C(3)
#define MF_TELEMETRY_PUBLISH_ACTIVE UINT32_C(4)
#define MF_TELEMETRY_PUBLISH_PUBLISHED UINT32_C(5)
#define MF_TELEMETRY_PUBLISH_ABORTED UINT32_C(6)
#define MF_TELEMETRY_PUBLISH_TERMINAL UINT32_C(7)
#define MF_TELEMETRY_PUBLISH_QUARANTINED UINT32_C(8)

#define MF_TELEMETRY_PUBLISH_KIND_NORMAL UINT32_C(1)
#define MF_TELEMETRY_PUBLISH_KIND_TERMINAL UINT32_C(2)

#define MF_RECOVERY_PLAN_NONE UINT32_C(0)
#define MF_RECOVERY_PLAN_CANCEL UINT32_C(1)
#define MF_RECOVERY_PLAN_COMPLETE UINT32_C(2)
#define MF_RECOVERY_PLAN_TOMBSTONE UINT32_C(3)
#define MF_RECOVERY_PLAN_RECONCILE UINT32_C(4)

#define MF_RING_WAIT_ACTIVE UINT32_C(0)
#define MF_RING_WAIT_ARMED UINT32_C(1)
#define MF_RING_WAIT_SLEEPING UINT32_C(2)

#define MF_RING_OPCODE_NOOP UINT32_C(1)
#define MF_RING_OPCODE_MEMORY_ALLOC UINT32_C(2)
#define MF_RING_OPCODE_MEMORY_FREE UINT32_C(3)
#define MF_RING_OPCODE_MODULE_LOAD UINT32_C(4)
#define MF_RING_OPCODE_MODULE_UNLOAD UINT32_C(5)
#define MF_RING_OPCODE_COPY UINT32_C(6)
#define MF_RING_OPCODE_LAUNCH UINT32_C(7)
#define MF_RING_OPCODE_EVENT_RECORD UINT32_C(8)
#define MF_RING_OPCODE_EVENT_WAIT UINT32_C(9)
#define MF_RING_OPCODE_QUEUE_SYNCHRONIZE UINT32_C(10)
#define MF_RING_OPCODE_QUEUE_CANCEL UINT32_C(11)
#define MF_RING_OPCODE_COMPLETION UINT32_C(0x80000001)

#define MF_RING_COPY_FLAG_REGION_ARGUMENT_BLOCK_V1 UINT32_C(1)
#define MF_RING_COPY_FLAG_DIRECT_HOST_SOURCE_V1 UINT32_C(2)
#define MF_RING_COPY_FLAG_DIRECT_HOST_DESTINATION_V1 UINT32_C(4)
#define MF_RING_COPY_KNOWN_FLAGS_V1                                                               \
  (MF_RING_COPY_FLAG_REGION_ARGUMENT_BLOCK_V1 | MF_RING_COPY_FLAG_DIRECT_HOST_SOURCE_V1 |        \
   MF_RING_COPY_FLAG_DIRECT_HOST_DESTINATION_V1)

#define MF_OBJECT_TYPE_CONTEXT UINT32_C(1)
#define MF_OBJECT_TYPE_QUEUE UINT32_C(2)
#define MF_OBJECT_TYPE_DEVICE_MEMORY UINT32_C(3)
#define MF_OBJECT_TYPE_HOST_MEMORY UINT32_C(4)
#define MF_OBJECT_TYPE_ARTIFACT UINT32_C(5)
#define MF_OBJECT_TYPE_ARGUMENT_BLOCK UINT32_C(6)
#define MF_OBJECT_TYPE_MODULE UINT32_C(7)
#define MF_OBJECT_TYPE_EVENT UINT32_C(8)

#define MF_ARGUMENT_KIND_U32 UINT32_C(1)
#define MF_ARGUMENT_KIND_BUFFER UINT32_C(2)
#define MF_ARGUMENT_KIND_U64 UINT32_C(3)

#define MF_ARGUMENT_BUFFER_READ UINT32_C(1)
#define MF_ARGUMENT_BUFFER_WRITE UINT32_C(2)
#define MF_ARGUMENT_BUFFER_KNOWN_FLAGS (MF_ARGUMENT_BUFFER_READ | MF_ARGUMENT_BUFFER_WRITE)

#define MF_ARGUMENT_BLOCK_FLAG_LAUNCH_DIMENSIONS_XY_V1 UINT32_C(1)
#define MF_ARGUMENT_BLOCK_FLAG_COPY_REGION_V1 UINT32_C(2)
#define MF_ARGUMENT_BLOCK_KNOWN_FLAGS_V1                                                           \
  (MF_ARGUMENT_BLOCK_FLAG_LAUNCH_DIMENSIONS_XY_V1 | MF_ARGUMENT_BLOCK_FLAG_COPY_REGION_V1)
#define MF_ARGUMENT_BLOCK_LAUNCH_GRID_X_INDEX_V1 UINT32_C(0)
#define MF_ARGUMENT_BLOCK_LAUNCH_GRID_Y_INDEX_V1 UINT32_C(1)
#define MF_ARGUMENT_BLOCK_LAUNCH_BLOCK_X_INDEX_V1 UINT32_C(2)
#define MF_ARGUMENT_BLOCK_LAUNCH_BLOCK_Y_INDEX_V1 UINT32_C(3)

#define MF_COPY_REGION_ARGUMENT_ENTRY_COUNT_V1 UINT32_C(3)
#define MF_COPY_REGION_DESTINATION_INDEX_V1 UINT32_C(0)
#define MF_COPY_REGION_SOURCE_INDEX_V1 UINT32_C(1)
#define MF_COPY_REGION_BYTE_COUNT_INDEX_V1 UINT32_C(2)

#define MF_KERNEL_PRIMARY_ENTRY_ID UINT64_C(1)

#define MF_VIEW_GENERATION_MAX_NORMAL UINT64_C(0x00fffffffffffffe)
#define MF_VIEW_GENERATION_TERMINAL UINT64_C(0x00ffffffffffffff)
#define MF_DEVICE_GENERATION_MAX_NORMAL UINT32_C(0x00fffffe)
#define MF_DEVICE_GENERATION_TERMINAL UINT32_C(0x00ffffff)

typedef struct mf_registry_view_id_v1 {
  uint64_t daemon_incarnation;
  uint64_t view_serial;
} mf_registry_view_id_v1;

typedef struct mf_owner_identity_v1 {
  uint64_t start_time_ticks;
  uint32_t pid;
  uint32_t reserved;
} mf_owner_identity_v1;

typedef struct MF_SHARED_ALIGNED(64) mf_generation_handle_v1 {
  mf_registry_view_id_v1 registry_view_id;
  uint64_t identity_record_id;
  uint64_t device_generation;
  uint64_t object_id;
  uint64_t object_generation;
  uint32_t object_type;
  uint32_t reserved;
} mf_generation_handle_v1;

typedef struct MF_SHARED_ALIGNED(64) mf_argument_block_header_v1 {
  uint32_t magic;
  uint32_t abi_version;
  uint32_t header_size;
  uint32_t entry_size;
  uint32_t entry_count;
  uint32_t flags;
  uint64_t total_size;
  uint64_t reserved[4];
} mf_argument_block_header_v1;

typedef struct mf_argument_entry_v1 {
  uint32_t kind;
  uint32_t flags;
  uint64_t object_id;
  uint64_t object_generation;
  uint64_t value;
} mf_argument_entry_v1;

typedef struct MF_SHARED_ALIGNED(64) mf_virtual_device_identity_v1 {
  uint64_t identity_record_id;
  uint8_t logical_device_id[16];
  uint8_t gpu_uuid[16];
  uint8_t display_name[64];
  uint64_t committed_generation;
  uint64_t capability_bits;
  uint32_t backend_id;
  uint32_t virtual_compute_capability;
  uint32_t pci_domain;
  uint32_t pci_bus;
  uint32_t pci_device;
  uint32_t pci_function;
  uint64_t reserved[14];
} mf_virtual_device_identity_v1;

typedef struct MF_SHARED_ALIGNED(64) mf_view_admission_control_v1 {
  mf_registry_view_id_v1 registry_view_id;
  uint64_t state_generation;
  uint64_t reserved[5];
} mf_view_admission_control_v1;

typedef struct MF_SHARED_ALIGNED(64) mf_registry_view_control_v1 {
  uint64_t control_latch_sequence;
  mf_registry_view_id_v1 registry_view_id;
  uint32_t selection_policy;
  uint32_t default_order;
  uint64_t process_view_revision;
  uint64_t allocation_high_water;
  uint64_t publication_cursor;
  uint64_t reservation_head_slot;
  uint64_t reservation_tail_slot;
  uint64_t next_committable_slot;
  uint64_t gate_generation;
  uint32_t gate_state;
  uint32_t mapping_terminal;
  uint64_t wake_sequence;
  uint64_t reserved[3];
} mf_registry_view_control_v1;

typedef struct MF_SHARED_ALIGNED(64) mf_device_admission_control_v1 {
  uint64_t identity_record_id;
  uint64_t state_generation_tag;
  uint64_t reserved[6];
} mf_device_admission_control_v1;

typedef struct MF_SHARED_ALIGNED(64) mf_virtual_device_lifecycle_fence_v1 {
  uint64_t fence_latch_sequence;
  uint64_t identity_record_id;
  uint64_t lifecycle_sequence;
  uint64_t epoch;
  uint64_t effective_quota_bytes;
  uint64_t policy_bits;
  uint32_t device_state;
  uint32_t reserved_word;
  uint64_t reserved;
} mf_virtual_device_lifecycle_fence_v1;

typedef struct MF_SHARED_ALIGNED(64) mf_telemetry_control_v1 {
  uint64_t telemetry_latch_sequence;
  uint64_t snapshot_sequence;
  uint64_t active_bank_state;
  uint32_t row_count;
  uint32_t reserved_word;
  uint64_t publish_generation;
  uint64_t reserved[3];
} mf_telemetry_control_v1;

typedef struct MF_SHARED_ALIGNED(64) mf_virtual_device_telemetry_v1 {
  uint64_t identity_record_id;
  uint64_t observed_lifecycle_sequence;
  uint64_t committed_work_items;
  uint64_t completed_work_items;
  uint64_t active_time_ns;
  uint64_t memory_used_bytes;
  uint64_t memory_capacity_bytes;
  uint64_t sample_time_ns;
  uint64_t memory_active_time_ns;
  uint64_t reserved[7];
} mf_virtual_device_telemetry_v1;

typedef struct MF_SHARED_ALIGNED(64) mf_view_publisher_control_v1 {
  uint64_t tagged_owner;
  uint64_t reserved[7];
} mf_view_publisher_control_v1;

typedef struct MF_SHARED_ALIGNED(64) mf_telemetry_publisher_control_v1 {
  uint64_t tagged_owner;
  uint64_t reserved[7];
} mf_telemetry_publisher_control_v1;

typedef struct MF_SHARED_ALIGNED(64) mf_admission_attempt_record_v1 {
  uint64_t tagged_phase;
  mf_registry_view_id_v1 registry_view_id;
  uint64_t identity_record_id;
  uint64_t view_validation_generation;
  mf_owner_identity_v1 owner;
  uint64_t deadline_ns;
  uint32_t target_lease_slot;
  uint32_t target_lease_tag;
  uint32_t device_validation_generation;
  uint32_t holder_references;
  uint32_t hazard_references;
  uint32_t reserved_word;
  uint64_t reserved[5];
} mf_admission_attempt_record_v1;

typedef struct MF_SHARED_ALIGNED(64) mf_admission_lease_record_v1 {
  uint64_t tagged_state;
  mf_registry_view_id_v1 registry_view_id;
  uint64_t identity_record_id;
  uint64_t lease_id;
  uint64_t view_validation_generation;
  mf_owner_identity_v1 owner;
  uint64_t deadline_ns;
  uint64_t operation_cookie;
  uint64_t target_cookie;
  uint64_t target_offset;
  uint64_t target_size;
  uint64_t publication_marker;
  uint32_t device_validation_generation;
  uint32_t recovery_plan;
  uint32_t attempt_slot;
  uint32_t attempt_tag;
  uint32_t holder_references;
  uint32_t hazard_references;
  uint64_t reserved[7];
} mf_admission_lease_record_v1;

typedef struct MF_SHARED_ALIGNED(64) mf_device_validation_update_record_v1 {
  uint64_t tagged_state;
  mf_registry_view_id_v1 registry_view_id;
  uint64_t identity_record_id;
  mf_owner_identity_v1 owner;
  uint64_t deadline_ns;
  uint64_t expected_device_control;
  uint64_t target_device_control;
  uint64_t intended_fence_hash;
  uint64_t publication_marker;
  uint32_t view_publish_slot;
  uint32_t publish_tag;
  uint32_t old_validation_generation;
  uint32_t new_validation_generation;
  uint32_t link_recovery_plan;
  uint32_t reopen_recovery_plan;
  uint32_t holder_references;
  uint32_t hazard_references;
  uint64_t intended_lifecycle_sequence;
  uint64_t intended_epoch;
  uint64_t intended_effective_quota_bytes;
  uint64_t intended_policy_bits;
  uint64_t expected_fence_latch_sequence;
  uint64_t target_fence_latch_sequence;
  uint32_t intended_device_state;
  uint32_t device_index;
  uint64_t reserved[2];
} mf_device_validation_update_record_v1;

typedef struct MF_SHARED_ALIGNED(64) mf_view_publish_record_v1 {
  uint64_t tagged_state;
  mf_registry_view_id_v1 registry_view_id;
  mf_owner_identity_v1 owner;
  uint64_t deadline_ns;
  uint64_t transaction_id;
  uint64_t range_begin;
  uint64_t range_end;
  uint64_t expected_cursor;
  uint64_t target_cursor;
  uint64_t gate_generation;
  uint64_t token_generation;
  uint64_t expected_fence_hash;
  uint64_t target_fence_hash;
  uint64_t expected_control_hash;
  uint64_t target_control_hash;
  uint64_t expected_latch_sequence;
  uint64_t target_latch_sequence;
  uint64_t publication_marker;
  uint32_t range_slot;
  uint32_t operation_kind;
  uint32_t recovery_plan;
  uint32_t holder_references;
  uint32_t hazard_references;
  uint32_t flags;
  uint64_t reserved[9];
} mf_view_publish_record_v1;

typedef struct MF_SHARED_ALIGNED(64) mf_lifecycle_range_record_v1 {
  uint64_t tagged_state;
  mf_registry_view_id_v1 registry_view_id;
  uint64_t transaction_id;
  mf_owner_identity_v1 owner;
  uint64_t deadline_ns;
  uint64_t range_begin;
  uint64_t range_end;
  uint64_t unused_suffix_begin;
  uint64_t unused_suffix_end;
  uint64_t token_generation;
  uint64_t predecessor_high_water;
  uint64_t predecessor_tail_slot;
  uint64_t next_tail_slot;
  uint32_t retirement_disposition;
  uint32_t holder_references;
  uint32_t hazard_references;
  uint32_t flags;
  uint64_t reserved[7];
} mf_lifecycle_range_record_v1;

typedef struct MF_SHARED_ALIGNED(64) mf_telemetry_publish_record_v1 {
  uint64_t tagged_state;
  mf_registry_view_id_v1 registry_view_id;
  mf_owner_identity_v1 owner;
  uint64_t deadline_ns;
  uint64_t expected_latch_sequence;
  uint64_t target_latch_sequence;
  uint64_t expected_snapshot_sequence;
  uint64_t target_snapshot_sequence;
  uint64_t old_bank_state;
  uint64_t target_bank_state;
  uint64_t staging_offset;
  uint64_t staging_size;
  uint64_t staging_hash;
  uint64_t expected_control_hash;
  uint64_t target_control_hash;
  uint64_t publication_marker;
  uint64_t view_validation_generation;
  uint32_t operation_kind;
  uint32_t recovery_plan;
  uint32_t holder_references;
  uint32_t hazard_references;
  uint32_t target_bank;
  uint32_t flags;
  uint64_t reserved[10];
} mf_telemetry_publish_record_v1;

typedef struct MF_SHARED_ALIGNED(64) mf_shared_registry_extension_header_v1 {
  uint32_t magic;
  uint32_t abi_version;
  uint32_t header_size;
  uint32_t flags;
  uint64_t total_size;
  mf_registry_view_id_v1 registry_view_id;
  uint32_t admission_attempt_capacity;
  uint32_t admission_lease_capacity;
  uint32_t device_update_capacity;
  uint32_t lifecycle_range_capacity;
  uint32_t view_publish_capacity;
  uint32_t telemetry_publish_capacity;
  uint64_t view_publisher_control_offset;
  uint64_t telemetry_publisher_control_offset;
  uint64_t admission_attempts_offset;
  uint64_t admission_leases_offset;
  uint64_t device_updates_offset;
  uint64_t lifecycle_ranges_offset;
  uint64_t view_publish_records_offset;
  uint64_t telemetry_publish_records_offset;
  uint32_t ordinary_view_publish_capacity;
  uint32_t ordinary_telemetry_publish_capacity;
  uint32_t close_closing_slot;
  uint32_t close_terminal_slot;
  uint32_t telemetry_terminal_slot;
  uint32_t reserved_word;
  uint64_t owner_clock_boot_id_hash;
  uint64_t reserved[12];
} mf_shared_registry_extension_header_v1;

typedef struct MF_SHARED_ALIGNED(64) mf_shared_registry_header_v1 {
  uint32_t magic;
  uint32_t abi_version;
  uint32_t header_size;
  uint32_t flags;
  uint64_t total_size;
  mf_registry_view_id_v1 registry_view_id;
  uint64_t process_view_revision;
  uint32_t device_count;
  uint32_t telemetry_row_count;
  uint64_t view_admission_offset;
  uint64_t view_control_offset;
  uint64_t identities_offset;
  uint64_t device_admission_offset;
  uint64_t lifecycle_fences_offset;
  uint64_t telemetry_control_offset;
  uint64_t telemetry_bank0_offset;
  uint64_t telemetry_bank1_offset;
} mf_shared_registry_header_v1;

typedef struct MF_SHARED_ALIGNED(64) mf_ring_metadata_v1 {
  uint32_t magic;
  uint32_t abi_version;
  uint32_t header_size;
  uint32_t descriptor_size;
  uint32_t capacity;
  uint32_t flags;
  uint64_t mapping_size;
  mf_registry_view_id_v1 registry_view_id;
  uint64_t queue_id;
  uint64_t queue_generation;
} mf_ring_metadata_v1;

typedef struct MF_SHARED_ALIGNED(64) mf_ring_cursor_v1 {
  uint64_t position;
  uint64_t reserved[7];
} mf_ring_cursor_v1;

typedef struct MF_SHARED_ALIGNED(64) mf_ring_wait_control_v1 {
  uint32_t consumer_wait_state;
  uint32_t consumer_wake_sequence;
  uint32_t producer_wait_state;
  uint32_t producer_wake_sequence;
  uint64_t consumer_doorbell_count;
  uint64_t producer_doorbell_count;
  uint64_t reserved[4];
} mf_ring_wait_control_v1;

typedef struct MF_SHARED_ALIGNED(64) mf_ring_header_v1 {
  mf_ring_metadata_v1 metadata;
  mf_ring_cursor_v1 producer;
  mf_ring_cursor_v1 consumer;
  mf_ring_wait_control_v1 wait;
} mf_ring_header_v1;

typedef struct MF_SHARED_ALIGNED(64) mf_ring_descriptor_v1 {
  uint64_t sequence;
  uint32_t opcode;
  uint32_t flags;
  uint64_t request_id;
  uint64_t target_id;
  uint64_t arguments[4];
} mf_ring_descriptor_v1;

static inline int mf_registry_view_id_equal_v1(mf_registry_view_id_v1 left,
                                               mf_registry_view_id_v1 right) {
  return left.daemon_incarnation == right.daemon_incarnation &&
         left.view_serial == right.view_serial;
}

static inline uint64_t mf_view_admission_pack_v1(uint64_t generation, uint32_t state) {
  return (generation << 8U) | (uint64_t)(state & UINT32_C(0xff));
}

static inline uint64_t mf_view_admission_generation_v1(uint64_t value) { return value >> 8U; }

static inline uint32_t mf_view_admission_state_v1(uint64_t value) {
  return (uint32_t)(value & UINT64_C(0xff));
}

static inline uint64_t mf_device_admission_pack_v1(uint32_t generation, uint32_t state,
                                                   uint32_t update_tag) {
  return ((uint64_t)update_tag << 32U) | ((uint64_t)(generation & UINT32_C(0x00ffffff)) << 8U) |
         (uint64_t)(state & UINT32_C(0xff));
}

static inline uint32_t mf_device_admission_generation_v1(uint64_t value) {
  return (uint32_t)((value >> 8U) & UINT64_C(0x00ffffff));
}

static inline uint32_t mf_device_admission_state_v1(uint64_t value) {
  return (uint32_t)(value & UINT64_C(0xff));
}

static inline uint32_t mf_device_admission_update_tag_v1(uint64_t value) {
  return (uint32_t)(value >> 32U);
}

static inline uint64_t mf_telemetry_bank_state_pack_v1(uint32_t bank, uint32_t state) {
  return ((uint64_t)state << 32U) | (uint64_t)bank;
}

static inline uint32_t mf_telemetry_active_bank_v1(uint64_t value) {
  return (uint32_t)(value & UINT64_C(0xffffffff));
}

static inline uint32_t mf_telemetry_state_v1(uint64_t value) { return (uint32_t)(value >> 32U); }

static inline uint64_t mf_tagged_record_pack_v1(uint32_t tag, uint32_t state, uint32_t auxiliary) {
  return ((uint64_t)(tag & UINT32_C(0x00ffffff)) << 32U) |
         ((uint64_t)(auxiliary & UINT32_C(0x00ffffff)) << 8U) | (uint64_t)(state & UINT32_C(0xff));
}

static inline uint32_t mf_tagged_record_tag_v1(uint64_t value) {
  return (uint32_t)((value >> 32U) & UINT64_C(0x00ffffff));
}

static inline uint32_t mf_tagged_record_auxiliary_v1(uint64_t value) {
  return (uint32_t)((value >> 8U) & UINT64_C(0x00ffffff));
}

static inline uint32_t mf_tagged_record_state_v1(uint64_t value) {
  return (uint32_t)(value & UINT64_C(0xff));
}

static inline uint64_t mf_publisher_control_pack_v1(uint32_t tag, uint32_t state,
                                                    uint32_t record_slot) {
  return mf_tagged_record_pack_v1(tag, state, record_slot);
}

static inline uint64_t mf_admission_attempt_phase_pack_v1(uint32_t attempt_tag, uint32_t phase) {
  return mf_tagged_record_pack_v1(attempt_tag, phase, 0U);
}

static inline uint64_t mf_admission_lease_state_pack_v1(uint32_t lease_tag, uint32_t state,
                                                        uint32_t attempt_tag) {
  return mf_tagged_record_pack_v1(lease_tag, state, attempt_tag);
}

static inline uint64_t mf_device_update_state_pack_v1(uint32_t update_tag, uint32_t state,
                                                      uint32_t view_publish_slot) {
  return mf_tagged_record_pack_v1(update_tag, state, view_publish_slot);
}

static inline uint64_t mf_view_publish_state_pack_v1(uint32_t publish_tag, uint32_t state,
                                                     uint32_t range_slot) {
  return mf_tagged_record_pack_v1(publish_tag, state, range_slot);
}

static inline uint64_t mf_lifecycle_range_state_pack_v1(uint32_t range_tag, uint32_t state,
                                                        uint32_t disposition) {
  return mf_tagged_record_pack_v1(range_tag, state, disposition);
}

static inline uint64_t mf_telemetry_publish_state_pack_v1(uint32_t publish_tag, uint32_t state,
                                                          uint32_t target_bank) {
  return mf_tagged_record_pack_v1(publish_tag, state, target_bank);
}

static inline int mf_shared_checked_add_u64_below_terminal_v1(uint64_t cursor, uint64_t increment,
                                                              uint64_t terminal,
                                                              uint64_t* out_value) {
  if (out_value == NULL || cursor >= terminal || increment >= terminal - cursor) {
    return 0;
  }
  *out_value = cursor + increment;
  return 1;
}

static inline int mf_shared_checked_next_record_tag_v1(uint32_t tag, uint32_t* out_tag) {
  if (out_tag == NULL || tag >= MF_SHARED_RECORD_TAG_MAX_NORMAL) {
    return 0;
  }
  *out_tag = tag + 1U;
  return 1;
}

MF_SHARED_STATIC_ASSERT(sizeof(mf_registry_view_id_v1) == 16, "registry view ID size");
MF_SHARED_STATIC_ASSERT(sizeof(mf_owner_identity_v1) == 16, "owner identity size");
MF_SHARED_STATIC_ASSERT(sizeof(mf_generation_handle_v1) == 64, "generation handle size");
MF_SHARED_STATIC_ASSERT(sizeof(mf_argument_block_header_v1) == 64, "argument block header size");
MF_SHARED_STATIC_ASSERT(sizeof(mf_argument_entry_v1) == 32, "argument entry size");
MF_SHARED_STATIC_ASSERT(offsetof(mf_argument_block_header_v1, total_size) == 24,
                        "argument block total size offset");
MF_SHARED_STATIC_ASSERT(offsetof(mf_argument_entry_v1, object_id) == 8,
                        "argument entry object offset");
MF_SHARED_STATIC_ASSERT(sizeof(mf_virtual_device_identity_v1) == 256, "identity record size");
MF_SHARED_STATIC_ASSERT(sizeof(mf_view_admission_control_v1) == 64, "view admission size");
MF_SHARED_STATIC_ASSERT(sizeof(mf_registry_view_control_v1) == 128, "view control size");
MF_SHARED_STATIC_ASSERT(sizeof(mf_device_admission_control_v1) == 64, "device admission size");
MF_SHARED_STATIC_ASSERT(sizeof(mf_virtual_device_lifecycle_fence_v1) == 64, "lifecycle fence size");
MF_SHARED_STATIC_ASSERT(sizeof(mf_telemetry_control_v1) == 64, "telemetry control size");
MF_SHARED_STATIC_ASSERT(sizeof(mf_virtual_device_telemetry_v1) == 128, "telemetry row size");
MF_SHARED_STATIC_ASSERT(offsetof(mf_virtual_device_telemetry_v1, memory_active_time_ns) == 64,
                        "telemetry memory activity offset");
MF_SHARED_STATIC_ASSERT(sizeof(mf_view_publisher_control_v1) == 64, "view publisher control size");
MF_SHARED_STATIC_ASSERT(sizeof(mf_telemetry_publisher_control_v1) == 64,
                        "telemetry publisher control size");
MF_SHARED_STATIC_ASSERT(sizeof(mf_admission_attempt_record_v1) == 128,
                        "admission attempt record size");
MF_SHARED_STATIC_ASSERT(sizeof(mf_admission_lease_record_v1) == 192, "admission lease record size");
MF_SHARED_STATIC_ASSERT(sizeof(mf_device_validation_update_record_v1) == 192,
                        "device update record size");
MF_SHARED_STATIC_ASSERT(sizeof(mf_view_publish_record_v1) == 256, "view publish record size");
MF_SHARED_STATIC_ASSERT(sizeof(mf_lifecycle_range_record_v1) == 192, "lifecycle range record size");
MF_SHARED_STATIC_ASSERT(sizeof(mf_telemetry_publish_record_v1) == 256,
                        "telemetry publish record size");
MF_SHARED_STATIC_ASSERT(sizeof(mf_shared_registry_extension_header_v1) == 256,
                        "registry extension header size");
MF_SHARED_STATIC_ASSERT(sizeof(mf_shared_registry_header_v1) == 128, "registry header size");
MF_SHARED_STATIC_ASSERT(sizeof(mf_ring_metadata_v1) == 64, "ring metadata size");
MF_SHARED_STATIC_ASSERT(sizeof(mf_ring_cursor_v1) == 64, "ring cursor size");
MF_SHARED_STATIC_ASSERT(sizeof(mf_ring_wait_control_v1) == 64, "ring wait size");
MF_SHARED_STATIC_ASSERT(sizeof(mf_ring_header_v1) == 256, "ring header size");
MF_SHARED_STATIC_ASSERT(sizeof(mf_ring_descriptor_v1) == 64, "ring descriptor size");
MF_SHARED_STATIC_ASSERT(offsetof(mf_ring_header_v1, producer) == 64, "producer cursor cache line");
MF_SHARED_STATIC_ASSERT(offsetof(mf_ring_header_v1, consumer) == 128, "consumer cursor cache line");
MF_SHARED_STATIC_ASSERT(offsetof(mf_ring_header_v1, wait) == 192, "wait control cache line");
MF_SHARED_STATIC_ASSERT(offsetof(mf_ring_descriptor_v1, sequence) == 0,
                        "descriptor sequence offset");
MF_SHARED_STATIC_ASSERT(offsetof(mf_ring_descriptor_v1, arguments) == 32,
                        "descriptor argument offset");
MF_SHARED_STATIC_ASSERT(offsetof(mf_admission_attempt_record_v1, target_lease_slot) == 64,
                        "attempt target offset");
MF_SHARED_STATIC_ASSERT(offsetof(mf_admission_lease_record_v1, publication_marker) == 104,
                        "lease publication marker offset");
MF_SHARED_STATIC_ASSERT(offsetof(mf_device_validation_update_record_v1, view_publish_slot) == 88,
                        "device update publish foreign key offset");
MF_SHARED_STATIC_ASSERT(offsetof(mf_view_publish_record_v1, range_slot) == 160,
                        "view publish range foreign key offset");
MF_SHARED_STATIC_ASSERT(offsetof(mf_lifecycle_range_record_v1, retirement_disposition) == 120,
                        "range disposition offset");
MF_SHARED_STATIC_ASSERT(offsetof(mf_telemetry_publish_record_v1, operation_kind) == 152,
                        "telemetry publish kind offset");
MF_SHARED_STATIC_ASSERT(offsetof(mf_shared_registry_extension_header_v1,
                                 view_publisher_control_offset) == 64,
                        "extension control offset field");
MF_SHARED_STATIC_ASSERT(offsetof(mf_shared_registry_extension_header_v1,
                                 ordinary_view_publish_capacity) == 128,
                        "extension ordinary capacities offset");

#ifdef __cplusplus
}
#endif

#endif
