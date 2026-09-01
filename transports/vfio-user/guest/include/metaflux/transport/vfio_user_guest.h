#ifndef METAFLUX_TRANSPORT_VFIO_USER_GUEST_H
#define METAFLUX_TRANSPORT_VFIO_USER_GUEST_H

#include <stddef.h>
#include <stdint.h>

#include <metaflux/client/fastpath.h>
#include <metaflux/transport/generated.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MF_VFIO_USER_REPLY_FLAG_V0 UINT16_C(0x8000)
#define MF_VFIO_USER_MAX_PACKET_SIZE_V0 UINT32_C(512)
#define MF_VFIO_USER_GUEST_MAX_BATCH_V0 UINT32_C(64)

typedef void (*mf_vfio_user_guest_doorbell_v0)(void* context, uint32_t value);

typedef struct mf_vfio_user_guest_ring_v0 {
  mf_client_ring_v1 submission;
  mf_client_ring_v1 completion;
  void* payload_mapping;
  uint64_t payload_size;
  mf_vfio_user_guest_doorbell_v0 doorbell;
  void* doorbell_context;
  uint32_t doorbell_value;
  uint32_t reserved;
  uint64_t last_completion_timeline;
  uint64_t armed_completion_timeline;
} mf_vfio_user_guest_ring_v0;

/*
 * Attach the two shared-memory queues advertised by the vfio-user device.
 * The caller retains ownership of the source FDs and payload mapping; the
 * returned ring owns duplicated queue FDs until close.
 */
mf_shared_status_v1 mf_vfio_user_guest_ring_attach_v0(
    int32_t submission_fd, int32_t completion_fd, mf_registry_view_id_v1 expected_view_id,
    uint64_t expected_queue_generation, void* payload_mapping, uint64_t payload_size,
    mf_vfio_user_guest_doorbell_v0 doorbell, void* doorbell_context, uint32_t doorbell_value,
    mf_vfio_user_guest_ring_v0* out_ring);

void mf_vfio_user_guest_ring_close_v0(mf_vfio_user_guest_ring_v0* ring);

mf_shared_status_v1 mf_vfio_user_guest_ring_payload_contains_v0(
    const mf_vfio_user_guest_ring_v0* ring, uint64_t offset, uint64_t byte_count);

mf_shared_status_v1 mf_vfio_user_guest_ring_submit_v0(
    mf_vfio_user_guest_ring_v0* ring, const mf_ring_descriptor_v1* descriptor);

/*
 * Publish a bounded SPSC batch in caller order and ring BAR2 once. Capacity
 * is checked before publication so a full batch cannot be partially emitted.
 * Descriptors may target different logical streams; the shared ring remains
 * the sole ordered publication boundary.
 */
mf_shared_status_v1 mf_vfio_user_guest_ring_submit_batch_v0(
    mf_vfio_user_guest_ring_v0* ring, const mf_ring_descriptor_v1* descriptors,
    uint32_t descriptor_count);

mf_shared_status_v1 mf_vfio_user_guest_ring_try_consume_v0(
    mf_vfio_user_guest_ring_v0* ring, mf_ring_descriptor_v1* out_descriptor);

mf_shared_status_v1 mf_vfio_user_guest_ring_wait_submission_v0(
    mf_vfio_user_guest_ring_v0* ring, uint64_t timeout_ns);

mf_shared_status_v1 mf_vfio_user_guest_ring_wait_completion_v0(
    mf_vfio_user_guest_ring_v0* ring, uint64_t timeout_ns);

/* Arm and wait for a completion timeline without consuming the descriptor. */
mf_shared_status_v1 mf_vfio_user_guest_ring_arm_completion_v0(
    mf_vfio_user_guest_ring_v0* ring, uint64_t timeline_value);
mf_shared_status_v1 mf_vfio_user_guest_ring_wait_armed_completion_v0(
    mf_vfio_user_guest_ring_v0* ring, uint64_t timeout_ns);
uint64_t mf_vfio_user_guest_ring_last_completion_timeline_v0(
    const mf_vfio_user_guest_ring_v0* ring);

mf_shared_status_v1 mf_vfio_user_guest_encode_get_info_v0(uint64_t message_id,
                                                           uint8_t* buffer,
                                                           uint32_t buffer_capacity,
                                                           uint32_t* out_size);

mf_shared_status_v1 mf_vfio_user_guest_encode_negotiate_v0(
    uint64_t message_id, const mf_transport_negotiate_v0* request, uint8_t* buffer,
    uint32_t buffer_capacity, uint32_t* out_size);

mf_shared_status_v1 mf_vfio_user_guest_encode_dma_map_v0(
    uint64_t message_id, const mf_vfio_user_dma_map_v0* request, uint8_t* buffer,
    uint32_t buffer_capacity, uint32_t* out_size);

mf_shared_status_v1 mf_vfio_user_guest_encode_dma_unmap_v0(
    uint64_t message_id, const mf_vfio_user_dma_unmap_v0* request, uint16_t message_flags,
    uint8_t* buffer, uint32_t buffer_capacity, uint32_t* out_size);

mf_shared_status_v1 mf_vfio_user_guest_decode_completion_v0(
    const uint8_t* buffer, uint32_t buffer_size, uint64_t expected_message_id,
    uint16_t expected_request_type, mf_transport_completion_v0* out_completion);

mf_shared_status_v1 mf_vfio_user_guest_decode_get_info_v0(
    const uint8_t* buffer, uint32_t buffer_size, uint64_t expected_message_id,
    mf_vfio_user_get_info_reply_v0* out_reply);

mf_shared_status_v1 mf_vfio_user_guest_decode_negotiate_v0(
    const uint8_t* buffer, uint32_t buffer_size, uint64_t expected_message_id,
    mf_transport_negotiate_v0* out_reply);

#ifdef __cplusplus
}
#endif

#endif
