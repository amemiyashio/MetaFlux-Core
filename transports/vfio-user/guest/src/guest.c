#include <metaflux/transport/vfio_user_guest.h>

#include <string.h>

static int guest_ring_bound(const mf_vfio_user_guest_ring_v0* ring) {
  return ring != NULL && ring->reserved == UINT32_C(0) && ring->doorbell != NULL &&
         ring->submission.header != NULL && ring->submission.descriptors != NULL &&
         ring->completion.header != NULL && ring->completion.descriptors != NULL &&
         ring->submission.capacity >= UINT32_C(2) && ring->completion.capacity >= UINT32_C(2) &&
         ring->submission.mapping != NULL && ring->completion.mapping != NULL &&
         ring->submission.mapping_size >= sizeof(mf_ring_header_v1) &&
         ring->completion.mapping_size >= sizeof(mf_ring_header_v1) &&
         ring->submission.queue_id == MF_CLIENT_SUBMISSION_QUEUE_ID_V1 &&
         ring->completion.queue_id == MF_CLIENT_COMPLETION_QUEUE_ID_V1 &&
         ring->submission.queue_generation == ring->completion.queue_generation &&
         mf_registry_view_id_equal_v1(ring->submission.registry_view_id,
                                      ring->completion.registry_view_id);
}

static void guest_ring_reset(mf_vfio_user_guest_ring_v0* ring) {
  if (ring == NULL) {
    return;
  }
  (void)memset(ring, 0, sizeof(*ring));
  ring->submission.owned_fd = -1;
  ring->completion.owned_fd = -1;
}

mf_shared_status_v1 mf_vfio_user_guest_ring_attach_v0(
    int32_t submission_fd, int32_t completion_fd, mf_registry_view_id_v1 expected_view_id,
    uint64_t expected_queue_generation, void* payload_mapping, uint64_t payload_size,
    mf_vfio_user_guest_doorbell_v0 doorbell, void* doorbell_context, uint32_t doorbell_value,
    mf_vfio_user_guest_ring_v0* out_ring) {
  mf_shared_status_v1 status = MF_SHARED_SUCCESS;

  if (out_ring == NULL || submission_fd < 0 || completion_fd < 0 ||
      expected_view_id.daemon_incarnation == UINT64_C(0) ||
      expected_view_id.view_serial == UINT64_C(0) || expected_queue_generation == UINT64_C(0) ||
      (payload_size != UINT64_C(0) && payload_mapping == NULL) || doorbell == NULL) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  guest_ring_reset(out_ring);
  status =
      mf_client_ring_attach_v1(submission_fd, expected_view_id, MF_CLIENT_SUBMISSION_QUEUE_ID_V1,
                               expected_queue_generation, &out_ring->submission);
  if (status != MF_SHARED_SUCCESS) {
    return status;
  }
  status =
      mf_client_ring_attach_v1(completion_fd, expected_view_id, MF_CLIENT_COMPLETION_QUEUE_ID_V1,
                               expected_queue_generation, &out_ring->completion);
  if (status != MF_SHARED_SUCCESS) {
    mf_client_ring_close_v1(&out_ring->submission);
    return status;
  }
  if (out_ring->submission.capacity != out_ring->completion.capacity ||
      !mf_registry_view_id_equal_v1(out_ring->submission.registry_view_id,
                                    out_ring->completion.registry_view_id) ||
      out_ring->submission.queue_generation != out_ring->completion.queue_generation) {
    mf_client_ring_close_v1(&out_ring->completion);
    mf_client_ring_close_v1(&out_ring->submission);
    return MF_SHARED_MALFORMED;
  }
  out_ring->payload_mapping = payload_mapping;
  out_ring->payload_size = payload_size;
  out_ring->doorbell = doorbell;
  out_ring->doorbell_context = doorbell_context;
  out_ring->doorbell_value = doorbell_value;
  return MF_SHARED_SUCCESS;
}

void mf_vfio_user_guest_ring_close_v0(mf_vfio_user_guest_ring_v0* ring) {
  if (ring == NULL) {
    return;
  }
  mf_client_ring_close_v1(&ring->completion);
  mf_client_ring_close_v1(&ring->submission);
  guest_ring_reset(ring);
}

mf_shared_status_v1
mf_vfio_user_guest_ring_payload_contains_v0(const mf_vfio_user_guest_ring_v0* ring, uint64_t offset,
                                            uint64_t byte_count) {
  if (!guest_ring_bound(ring)) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  if (offset > UINT64_MAX - byte_count) {
    return MF_SHARED_OVERFLOW;
  }
  return offset <= ring->payload_size && byte_count <= ring->payload_size - offset
             ? MF_SHARED_SUCCESS
             : MF_SHARED_INVALID_ARGUMENT;
}

mf_shared_status_v1 mf_vfio_user_guest_ring_submit_v0(mf_vfio_user_guest_ring_v0* ring,
                                                      const mf_ring_descriptor_v1* descriptor) {
  mf_shared_status_v1 status = MF_SHARED_SUCCESS;
  if (!guest_ring_bound(ring) || descriptor == NULL || descriptor->request_id == UINT64_C(0) ||
      descriptor->target_id == UINT64_C(0) || descriptor->opcode == MF_RING_OPCODE_COMPLETION) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  status = mf_client_ring_try_submit_v1(&ring->submission, descriptor);
  if (status == MF_SHARED_SUCCESS) {
    ring->doorbell(ring->doorbell_context, ring->doorbell_value);
  }
  return status;
}

mf_shared_status_v1 mf_vfio_user_guest_ring_try_consume_v0(mf_vfio_user_guest_ring_v0* ring,
                                                           mf_ring_descriptor_v1* out_descriptor) {
  if (!guest_ring_bound(ring) || out_descriptor == NULL) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  return mf_client_ring_try_consume_v1(&ring->completion, out_descriptor);
}

mf_shared_status_v1 mf_vfio_user_guest_ring_wait_submission_v0(mf_vfio_user_guest_ring_v0* ring,
                                                               uint64_t timeout_ns) {
  if (!guest_ring_bound(ring)) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  return mf_client_ring_wait_writable_v1(&ring->submission, timeout_ns);
}

mf_shared_status_v1 mf_vfio_user_guest_ring_wait_completion_v0(mf_vfio_user_guest_ring_v0* ring,
                                                               uint64_t timeout_ns) {
  if (!guest_ring_bound(ring)) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  return mf_client_ring_wait_readable_v1(&ring->completion, timeout_ns);
}

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
