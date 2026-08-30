#ifndef METAFLUX_TRANSPORT_VFIO_USER_GUEST_H
#define METAFLUX_TRANSPORT_VFIO_USER_GUEST_H

#include <stddef.h>
#include <stdint.h>

#include <metaflux/transport/generated.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MF_VFIO_USER_REPLY_FLAG_V0 UINT16_C(0x8000)
#define MF_VFIO_USER_MAX_PACKET_SIZE_V0 UINT32_C(512)

mf_shared_status_v1 mf_vfio_user_guest_encode_get_info_v0(uint64_t message_id,
                                                           uint8_t* buffer,
                                                           uint32_t buffer_capacity,
                                                           uint32_t* out_size);

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

#ifdef __cplusplus
}
#endif

#endif
