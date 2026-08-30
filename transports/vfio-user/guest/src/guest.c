#include <metaflux/transport/vfio_user_guest.h>

#include <string.h>

static mf_shared_status_v1 encode_packet(uint64_t message_id, uint16_t message_type,
                                         uint16_t flags, const void* payload, uint32_t payload_size,
                                         uint8_t* buffer, uint32_t buffer_capacity,
                                         uint32_t* out_size) {
  mf_transport_message_header_v0 header;
  uint64_t total_size = (uint64_t)sizeof(header) + (uint64_t)payload_size;
  if (message_id == UINT64_C(0) || buffer == NULL || out_size == NULL ||
      (payload_size != UINT32_C(0) && payload == NULL) || total_size > UINT32_MAX ||
      total_size > (uint64_t)buffer_capacity || total_size > MF_VFIO_USER_MAX_PACKET_SIZE_V0) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  (void)memset(buffer, 0, (size_t)total_size);
  (void)memset(&header, 0, sizeof(header));
  header.message_id = message_id;
  header.message_type = message_type;
  header.flags = flags;
  header.payload_size = payload_size;
  (void)memcpy(buffer, &header, sizeof(header));
  if (payload_size != UINT32_C(0)) {
    (void)memcpy(buffer + sizeof(header), payload, payload_size);
  }
  *out_size = (uint32_t)total_size;
  return MF_SHARED_SUCCESS;
}

mf_shared_status_v1 mf_vfio_user_guest_encode_get_info_v0(uint64_t message_id, uint8_t* buffer,
                                                           uint32_t buffer_capacity,
                                                           uint32_t* out_size) {
  return encode_packet(message_id, MF_VFIO_USER_MESSAGE_GET_INFO_V0, UINT16_C(0), NULL,
                       UINT32_C(0), buffer, buffer_capacity, out_size);
}

mf_shared_status_v1 mf_vfio_user_guest_encode_dma_map_v0(
    uint64_t message_id, const mf_vfio_user_dma_map_v0* request, uint8_t* buffer,
    uint32_t buffer_capacity, uint32_t* out_size) {
  if (request == NULL || request->struct_size != sizeof(*request)) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  return encode_packet(message_id, MF_VFIO_USER_MESSAGE_DMA_MAP_V0, UINT16_C(0), request,
                       (uint32_t)sizeof(*request), buffer, buffer_capacity, out_size);
}

mf_shared_status_v1 mf_vfio_user_guest_encode_dma_unmap_v0(
    uint64_t message_id, const mf_vfio_user_dma_unmap_v0* request, uint16_t message_flags,
    uint8_t* buffer, uint32_t buffer_capacity, uint32_t* out_size) {
  if (request == NULL || request->struct_size != sizeof(*request)) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  return encode_packet(message_id, MF_VFIO_USER_MESSAGE_DMA_UNMAP_V0, message_flags, request,
                       (uint32_t)sizeof(*request), buffer, buffer_capacity, out_size);
}

mf_shared_status_v1 mf_vfio_user_guest_decode_completion_v0(
    const uint8_t* buffer, uint32_t buffer_size, uint64_t expected_message_id,
    uint16_t expected_request_type, mf_transport_completion_v0* out_completion) {
  const mf_transport_message_header_v0* header;
  uint64_t expected_size = (uint64_t)sizeof(*header) + (uint64_t)sizeof(*out_completion);
  if (buffer == NULL || out_completion == NULL || expected_message_id == UINT64_C(0) ||
      buffer_size != expected_size || expected_request_type == UINT16_C(0)) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  header = (const mf_transport_message_header_v0*)buffer;
  if (header->message_id != expected_message_id ||
      header->message_type != (uint16_t)(expected_request_type | MF_VFIO_USER_REPLY_FLAG_V0) ||
      header->flags != UINT16_C(0) || header->payload_size != sizeof(*out_completion)) {
    return MF_SHARED_MALFORMED;
  }
  (void)memcpy(out_completion, buffer + sizeof(*header), sizeof(*out_completion));
  return MF_SHARED_SUCCESS;
}

mf_shared_status_v1 mf_vfio_user_guest_decode_get_info_v0(
    const uint8_t* buffer, uint32_t buffer_size, uint64_t expected_message_id,
    mf_vfio_user_get_info_reply_v0* out_reply) {
  const mf_transport_message_header_v0* header;
  uint64_t expected_size = (uint64_t)sizeof(*header) + (uint64_t)sizeof(*out_reply);
  if (buffer == NULL || out_reply == NULL || expected_message_id == UINT64_C(0) ||
      buffer_size != expected_size) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  header = (const mf_transport_message_header_v0*)buffer;
  if (header->message_id != expected_message_id ||
      header->message_type !=
          (uint16_t)(MF_VFIO_USER_MESSAGE_GET_INFO_V0 | MF_VFIO_USER_REPLY_FLAG_V0) ||
      header->flags != UINT16_C(0) || header->payload_size != sizeof(*out_reply)) {
    return MF_SHARED_MALFORMED;
  }
  (void)memcpy(out_reply, buffer + sizeof(*header), sizeof(*out_reply));
  return MF_SHARED_SUCCESS;
}
