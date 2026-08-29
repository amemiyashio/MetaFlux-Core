#ifndef METAFLUX_CLIENT_FASTPATH_H
#define METAFLUX_CLIENT_FASTPATH_H

#include <stdint.h>

#include "metaflux/client/protocol.h"
#include "metaflux/shared/device.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MF_CLIENT_RING_INFINITE_TIMEOUT_NS UINT64_MAX
#define MF_CLIENT_SUBMISSION_QUEUE_ID_V1 UINT64_C(1)
#define MF_CLIENT_COMPLETION_QUEUE_ID_V1 UINT64_C(2)
#define MF_CLIENT_QUEUE_GENERATION_V1 UINT64_C(1)
#define MF_CLIENT_RUNTIME_CONTEXT_ID_V1 UINT64_C(1)
#define MF_CLIENT_RUNTIME_EVENT_ID_V1 UINT64_C(1)
#define MF_CLIENT_RUNTIME_EVENT_GENERATION_V1 UINT64_C(1)
#define MF_CLIENT_RUNTIME_ADD_KERNEL_ID_V1 MF_KERNEL_PRIMARY_ENTRY_ID

#define MF_CLIENT_PAYLOAD_WRITABLE_V1 UINT32_C(1)

typedef struct mf_client_ring_v1 {
  void* mapping;
  uint64_t mapping_size;
  mf_ring_header_v1* header;
  mf_ring_descriptor_v1* descriptors;
  mf_registry_view_id_v1 registry_view_id;
  uint64_t queue_id;
  uint64_t queue_generation;
  int32_t owned_fd;
  uint32_t capacity;
} mf_client_ring_v1;

typedef struct mf_client_registry_v1 {
  void* mapping;
  uint64_t mapping_size;
  const mf_shared_registry_header_v1* header;
  const mf_view_admission_control_v1* view_admission;
  const mf_registry_view_control_v1* view_control;
  const mf_virtual_device_identity_v1* identities;
  const mf_device_admission_control_v1* device_admission;
  const mf_virtual_device_lifecycle_fence_v1* lifecycle_fences;
  const mf_telemetry_control_v1* telemetry_control;
  const mf_virtual_device_telemetry_v1* telemetry_banks[2];
  mf_registry_view_id_v1 registry_view_id;
  int32_t owned_fd;
  uint32_t device_count;
} mf_client_registry_v1;

typedef struct mf_client_fence_snapshot_v1 {
  uint64_t identity_record_id;
  uint64_t lifecycle_sequence;
  uint64_t epoch;
  uint64_t effective_quota_bytes;
  uint64_t policy_bits;
  uint32_t device_state;
  uint32_t reserved;
} mf_client_fence_snapshot_v1;

typedef struct mf_client_telemetry_snapshot_v1 {
  uint64_t identity_record_id;
  uint64_t observed_lifecycle_sequence;
  uint64_t committed_work_items;
  uint64_t completed_work_items;
  uint64_t active_time_ns;
  uint64_t memory_active_time_ns;
  uint64_t memory_used_bytes;
  uint64_t memory_capacity_bytes;
  uint64_t sample_time_ns;
  uint64_t snapshot_sequence;
} mf_client_telemetry_snapshot_v1;

typedef struct mf_client_completion_v1 {
  uint64_t request_id;
  int32_t status;
  uint32_t flags;
  uint64_t result_id;
  uint64_t result_generation;
  uint64_t timeline_value;
  uint64_t detail;
} mf_client_completion_v1;

typedef struct mf_client_payload_v1 {
  void* mapping;
  uint64_t mapping_size;
  int32_t owned_fd;
  uint32_t flags;
} mf_client_payload_v1;

typedef struct mf_client_process_snapshot_row_v1 {
  uint32_t pid;
  uint32_t kinds;
  uint64_t process_start_time_ticks;
  uint64_t identity_record_id;
  uint64_t device_generation;
  uint64_t used_memory_bytes;
  char process_name[MF_CLIENT_PROCESS_NAME_SIZE_V1];
} mf_client_process_snapshot_row_v1;

typedef struct mf_client_process_snapshot_v1 {
  void* mapping;
  uint64_t mapping_size;
  uint64_t revision;
  int32_t owned_fd;
  uint32_t row_count;
} mf_client_process_snapshot_v1;

/*
 * A connected session owns the socket plus duplicated registry/ring FDs and
 * mappings. Queue/context/event identities are the public constants above.
 * Control request/response pairs are serialized: one caller may be outstanding.
 */
typedef struct mf_client_session_v1 {
  mf_client_registry_v1 registry;
  mf_client_ring_v1 submission;
  mf_client_ring_v1 completion;
  mf_registry_view_id_v1 registry_view_id;
  uint64_t negotiated_capabilities;
  uint64_t next_control_request_id;
  int32_t socket_fd;
  uint32_t negotiated_version;
} mf_client_session_v1;

uint32_t mf_client_fastpath_bootstrap_abi_version(void);

mf_shared_status_v1 mf_client_argument_block_size_v1(uint32_t entry_count,
                                                     uint64_t* out_byte_count);

mf_shared_status_v1 mf_client_argument_block_validate_v1(const uint8_t* bytes, uint64_t byte_count);

mf_shared_status_v1 mf_client_copy_region_argument_block_validate_v1(const uint8_t* bytes,
                                                                     uint64_t byte_count);

mf_shared_status_v1 mf_client_payload_create_v1(const uint8_t* initial_bytes, uint64_t byte_count,
                                                uint32_t flags, mf_client_payload_v1* out_payload);

void mf_client_payload_close_v1(mf_client_payload_v1* payload);

int32_t mf_client_payload_borrow_fd_v1(const mf_client_payload_v1* payload);

mf_shared_status_v1 mf_client_session_connect_v1(const char* socket_path,
                                                 mf_client_session_v1* out_session);

mf_shared_status_v1 mf_client_session_connect_default_v1(mf_client_session_v1* out_session);

mf_shared_status_v1 mf_client_observer_connect_v1(const char* socket_path,
                                                  mf_client_session_v1* out_session);

mf_shared_status_v1 mf_client_observer_connect_default_v1(mf_client_session_v1* out_session);

/*
 * Socket EOF is the disconnect signal. Close releases the socket first, then
 * completion/submission/registry mappings and owned FDs. All borrowed pointers
 * and descriptors from the session become invalid; payload objects are separate.
 */
void mf_client_session_close_v1(mf_client_session_v1* session);

/*
 * MF_SHARED_SUCCESS means a complete, validated response was received. The
 * operation result is the MF_CLIENT_CONTROL_* status at response byte 12.
 * Any returned payload FD is newly owned by the caller and must be closed.
 */
mf_shared_status_v1 mf_client_session_control_v1(mf_client_session_v1* session, uint16_t opcode,
                                                 uint16_t flags, uint64_t object_id,
                                                 uint64_t argument, int32_t payload_fd,
                                                 mf_client_control_response_v1* out_response,
                                                 int32_t* out_received_payload_fd);

mf_shared_status_v1
mf_client_process_snapshot_fetch_v1(mf_client_session_v1* observer,
                                    mf_client_process_snapshot_v1* out_snapshot);

void mf_client_process_snapshot_close_v1(mf_client_process_snapshot_v1* snapshot);

uint64_t
mf_client_process_snapshot_revision_value_v1(const mf_client_process_snapshot_v1* snapshot);

uint32_t mf_client_process_snapshot_row_count_v1(const mf_client_process_snapshot_v1* snapshot);

/*
 * A null rows pointer queries the immutable snapshot count. A short array is
 * left untouched, count is updated to the required value, and
 * MF_SHARED_RESOURCE_EXHAUSTED is returned.
 */
mf_shared_status_v1
mf_client_process_snapshot_fill_v1(const mf_client_process_snapshot_v1* snapshot, uint32_t* count,
                                   mf_client_process_snapshot_row_v1* rows);

mf_shared_status_v1 mf_client_registry_attach_v1(int32_t fd,
                                                 mf_registry_view_id_v1 expected_view_id,
                                                 mf_client_registry_v1* out_registry);

void mf_client_registry_close_v1(mf_client_registry_v1* registry);

int32_t mf_client_registry_borrow_fd_v1(const mf_client_registry_v1* registry);

uint32_t mf_client_registry_device_count_v1(const mf_client_registry_v1* registry);

mf_shared_status_v1 mf_client_registry_identity_v1(const mf_client_registry_v1* registry,
                                                   uint32_t device_index,
                                                   mf_virtual_device_identity_v1* out_identity);

mf_shared_status_v1 mf_client_registry_make_handle_v1(const mf_client_registry_v1* registry,
                                                      uint32_t device_index, uint64_t object_id,
                                                      uint64_t object_generation,
                                                      uint32_t object_type,
                                                      mf_generation_handle_v1* out_handle);

mf_shared_status_v1 mf_client_registry_validate_device_v1(const mf_client_registry_v1* registry,
                                                          const mf_generation_handle_v1* handle,
                                                          mf_client_fence_snapshot_v1* out_fence);

mf_shared_status_v1
mf_client_registry_read_telemetry_v1(const mf_client_registry_v1* registry,
                                     const mf_generation_handle_v1* handle,
                                     mf_client_telemetry_snapshot_v1* out_snapshot);

mf_shared_status_v1 mf_client_ring_mapping_size_v1(uint32_t capacity, uint64_t* out_mapping_size);

mf_shared_status_v1 mf_client_ring_create_v1(uint32_t capacity,
                                             mf_registry_view_id_v1 registry_view_id,
                                             uint64_t queue_id, uint64_t queue_generation,
                                             mf_client_ring_v1* out_ring);

mf_shared_status_v1 mf_client_ring_attach_v1(int32_t fd, mf_registry_view_id_v1 expected_view_id,
                                             uint64_t expected_queue_id,
                                             uint64_t expected_queue_generation,
                                             mf_client_ring_v1* out_ring);

void mf_client_ring_close_v1(mf_client_ring_v1* ring);

int32_t mf_client_ring_borrow_fd_v1(const mf_client_ring_v1* ring);

mf_shared_status_v1 mf_client_ring_try_submit_v1(mf_client_ring_v1* ring,
                                                 const mf_ring_descriptor_v1* descriptor);

mf_shared_status_v1 mf_client_ring_try_consume_v1(mf_client_ring_v1* ring,
                                                  mf_ring_descriptor_v1* out_descriptor);

mf_shared_status_v1 mf_client_ring_wait_readable_v1(mf_client_ring_v1* ring, uint64_t timeout_ns);

mf_shared_status_v1 mf_client_ring_wait_writable_v1(mf_client_ring_v1* ring, uint64_t timeout_ns);

uint64_t mf_client_ring_consumer_doorbells_v1(const mf_client_ring_v1* ring);
uint64_t mf_client_ring_producer_doorbells_v1(const mf_client_ring_v1* ring);

mf_shared_status_v1 mf_client_submit_memory_alloc_v1(mf_client_ring_v1* ring, uint64_t request_id,
                                                     uint64_t context_id, uint64_t byte_count,
                                                     uint64_t alignment, uint32_t flags);

mf_shared_status_v1 mf_client_submit_memory_free_v1(mf_client_ring_v1* ring, uint64_t request_id,
                                                    uint64_t memory_id, uint64_t memory_generation);

mf_shared_status_v1 mf_client_submit_module_load_v1(mf_client_ring_v1* ring, uint64_t request_id,
                                                    uint64_t artifact_id,
                                                    uint64_t artifact_generation, uint32_t flags);

mf_shared_status_v1 mf_client_submit_module_unload_v1(mf_client_ring_v1* ring, uint64_t request_id,
                                                      uint64_t module_id,
                                                      uint64_t module_generation);

mf_shared_status_v1 mf_client_submit_copy_v1(mf_client_ring_v1* ring, uint64_t request_id,
                                             uint64_t destination_memory_id,
                                             uint64_t destination_generation,
                                             uint64_t source_memory_id, uint64_t source_generation,
                                             uint64_t byte_count, uint32_t flags);

mf_shared_status_v1 mf_client_submit_copy_region_v1(mf_client_ring_v1* ring, uint64_t request_id,
                                                    uint64_t argument_block_id,
                                                    uint64_t argument_block_generation);

mf_shared_status_v1
mf_client_submit_direct_host_copy_v1(mf_client_ring_v1* ring, uint64_t request_id,
                                     uint64_t device_memory_id, uint64_t device_generation,
                                     uint64_t host_address, uint64_t device_offset,
                                     uint64_t byte_count, uint32_t flags);

mf_shared_status_v1 mf_client_submit_launch_v1(mf_client_ring_v1* ring, uint64_t request_id,
                                               uint64_t module_id, uint64_t module_generation,
                                               uint64_t kernel_id, uint64_t argument_block_id,
                                               uint64_t argument_block_generation, uint32_t flags);

mf_shared_status_v1 mf_client_submit_event_record_v1(mf_client_ring_v1* ring, uint64_t request_id,
                                                     uint64_t event_id, uint64_t event_generation,
                                                     uint64_t timeline_value, uint32_t flags);

mf_shared_status_v1 mf_client_submit_event_wait_v1(mf_client_ring_v1* ring, uint64_t request_id,
                                                   uint64_t event_id, uint64_t event_generation,
                                                   uint64_t timeline_value, uint32_t flags);

mf_shared_status_v1 mf_client_submit_queue_control_v1(mf_client_ring_v1* ring, uint32_t opcode,
                                                      uint64_t request_id, uint64_t timeout_ns,
                                                      uint32_t flags);

mf_shared_status_v1 mf_client_try_consume_completion_v1(mf_client_ring_v1* ring,
                                                        mf_client_completion_v1* out_completion);

#ifdef __cplusplus
}
#endif

#endif
