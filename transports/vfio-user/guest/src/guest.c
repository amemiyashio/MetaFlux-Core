#define _POSIX_C_SOURCE 200809L

#include <metaflux/transport/vfio_user_guest.h>

#include <string.h>
#include <time.h>

static int guest_ring_bound(const mf_vfio_user_guest_ring_v0* ring) {
  return ring != NULL && ring->reserved == UINT32_C(0) && ring->doorbell != NULL &&
         ring->submission.header != NULL && ring->submission.descriptors != NULL &&
         ring->completion.header != NULL && ring->completion.descriptors != NULL &&
         ring->submission.capacity >= UINT32_C(2) && ring->completion.capacity >= UINT32_C(2) &&
         ring->submission.mapping != NULL && ring->completion.mapping != NULL &&
         ring->submission.mapping_size >= sizeof(mf_ring_header_v1) &&
         ring->completion.mapping_size >= sizeof(mf_ring_header_v1) &&
         (ring->payload_mapping != NULL || ring->payload_size == UINT64_C(0)) &&
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

static int guest_descriptor_valid(const mf_ring_descriptor_v1* descriptor) {
  return descriptor != NULL && descriptor->request_id != UINT64_C(0) &&
         descriptor->target_id != UINT64_C(0) && descriptor->opcode != MF_RING_OPCODE_COMPLETION;
}

static mf_shared_status_v1 guest_ring_has_armed_completion(
    mf_vfio_user_guest_ring_v0* ring, uint64_t armed_timeline, int* out_ready) {
  const uint64_t consumer_position =
      mf_atomic_load_u64_relaxed(&ring->completion.header->consumer.position);
  const uint64_t producer_position =
      mf_atomic_load_u64_acquire(&ring->completion.header->producer.position);
  const uint32_t mask = ring->completion.capacity - UINT32_C(1);
  const uint64_t available = producer_position - consumer_position;
  uint64_t previous_timeline = ring->last_completion_timeline;

  if (out_ready == NULL) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  *out_ready = 0;
  if (available > (uint64_t)ring->completion.capacity) {
    ring->reserved = UINT32_C(1);
    return MF_SHARED_MALFORMED;
  }
  for (uint64_t offset = 0; offset < available; ++offset) {
    const uint64_t position = consumer_position + offset;
    const mf_ring_descriptor_v1* descriptor =
        &ring->completion.descriptors[position & (uint64_t)mask];
    const uint64_t sequence = mf_atomic_load_u64_acquire(&descriptor->sequence);
    if (sequence != position + UINT64_C(1)) {
      return MF_SHARED_WOULD_BLOCK;
    }
    if (descriptor->opcode != MF_RING_OPCODE_COMPLETION ||
        descriptor->request_id == UINT64_C(0) || descriptor->target_id == UINT64_C(0) ||
        descriptor->arguments[1] == UINT64_C(0) || descriptor->arguments[1] <= previous_timeline) {
      ring->reserved = UINT32_C(1);
      return MF_SHARED_MALFORMED;
    }
    previous_timeline = descriptor->arguments[1];
    if (previous_timeline >= armed_timeline) {
      *out_ready = 1;
      return MF_SHARED_SUCCESS;
    }
  }
  return MF_SHARED_SUCCESS;
}

static int guest_monotonic_time_ns(uint64_t* out_time_ns) {
  struct timespec now;
  uint64_t seconds = 0;

  if (out_time_ns == NULL || clock_gettime(CLOCK_MONOTONIC, &now) != 0 || now.tv_sec < 0 ||
      now.tv_nsec < 0 || (uint64_t)now.tv_sec > (UINT64_MAX - (uint64_t)now.tv_nsec) /
                                UINT64_C(1000000000)) {
    return 0;
  }
  seconds = (uint64_t)now.tv_sec;
  *out_time_ns = seconds * UINT64_C(1000000000) + (uint64_t)now.tv_nsec;
  return 1;
}

static mf_shared_status_v1 guest_ring_publish(mf_vfio_user_guest_ring_v0* ring,
                                              const mf_ring_descriptor_v1* descriptor,
                                              int notify) {
  const mf_shared_status_v1 status = mf_client_ring_try_submit_v1(&ring->submission, descriptor);
  if (status == MF_SHARED_SUCCESS && notify) {
    ring->doorbell(ring->doorbell_context, ring->doorbell_value);
  }
  return status;
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
  out_ring->last_completion_timeline = 0U;
  out_ring->armed_completion_timeline = 0U;
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
  if (!guest_ring_bound(ring) || !guest_descriptor_valid(descriptor)) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  return guest_ring_publish(ring, descriptor, 1);
}

mf_shared_status_v1 mf_vfio_user_guest_ring_submit_batch_v0(
    mf_vfio_user_guest_ring_v0* ring, const mf_ring_descriptor_v1* descriptors,
    uint32_t descriptor_count) {
  if (!guest_ring_bound(ring) || descriptors == NULL || descriptor_count == 0U ||
      descriptor_count > MF_VFIO_USER_GUEST_MAX_BATCH_V0) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  for (uint32_t index = 0U; index < descriptor_count; ++index) {
    if (!guest_descriptor_valid(&descriptors[index])) {
      return MF_SHARED_INVALID_ARGUMENT;
    }
  }
  const mf_shared_status_v1 status = mf_client_ring_try_submit_batch_v1(
      &ring->submission, descriptors, descriptor_count);
  if (status != MF_SHARED_SUCCESS) {
    return status;
  }
  ring->doorbell(ring->doorbell_context, ring->doorbell_value);
  return MF_SHARED_SUCCESS;
}

mf_shared_status_v1 mf_vfio_user_guest_ring_try_consume_v0(mf_vfio_user_guest_ring_v0* ring,
                                                           mf_ring_descriptor_v1* out_descriptor) {
  if (!guest_ring_bound(ring) || out_descriptor == NULL) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  const mf_shared_status_v1 status =
      mf_client_ring_try_consume_v1(&ring->completion, out_descriptor);
  if (status != MF_SHARED_SUCCESS) {
    return status;
  }
  if (out_descriptor->opcode != MF_RING_OPCODE_COMPLETION ||
      out_descriptor->request_id == UINT64_C(0) || out_descriptor->target_id == UINT64_C(0) ||
      out_descriptor->arguments[1] == UINT64_C(0) ||
      out_descriptor->arguments[1] <= ring->last_completion_timeline) {
    ring->reserved = UINT32_C(1);
    return MF_SHARED_MALFORMED;
  }
  ring->last_completion_timeline = out_descriptor->arguments[1];
  return MF_SHARED_SUCCESS;
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

mf_shared_status_v1 mf_vfio_user_guest_ring_arm_completion_v0(
    mf_vfio_user_guest_ring_v0* ring, uint64_t timeline_value) {
  if (!guest_ring_bound(ring) || timeline_value == UINT64_C(0)) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  if (ring->armed_completion_timeline != UINT64_C(0) &&
      timeline_value < ring->armed_completion_timeline) {
    return MF_SHARED_WOULD_BLOCK;
  }
  ring->armed_completion_timeline = timeline_value;
  return MF_SHARED_SUCCESS;
}

mf_shared_status_v1 mf_vfio_user_guest_ring_wait_armed_completion_v0(
    mf_vfio_user_guest_ring_v0* ring, uint64_t timeout_ns) {
  uint64_t start_time_ns = 0;

  if (!guest_ring_bound(ring) || ring->armed_completion_timeline == UINT64_C(0)) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  if (ring->last_completion_timeline >= ring->armed_completion_timeline) {
    return MF_SHARED_SUCCESS;
  }
  if (timeout_ns != MF_CLIENT_RING_INFINITE_TIMEOUT_NS &&
      !guest_monotonic_time_ns(&start_time_ns)) {
    return MF_SHARED_SYSTEM_ERROR;
  }
  for (;;) {
    int armed_ready = 0;
    uint64_t wait_timeout_ns = timeout_ns;
    mf_shared_status_v1 status =
        guest_ring_has_armed_completion(ring, ring->armed_completion_timeline, &armed_ready);

    if (status == MF_SHARED_SUCCESS && armed_ready != 0) {
      return MF_SHARED_SUCCESS;
    }
    if (status == MF_SHARED_MALFORMED) {
      return status;
    }
    if (timeout_ns == UINT64_C(0)) {
      return MF_SHARED_WOULD_BLOCK;
    }
    if (timeout_ns != MF_CLIENT_RING_INFINITE_TIMEOUT_NS) {
      uint64_t now_time_ns = 0;
      uint64_t elapsed_ns = 0;
      if (!guest_monotonic_time_ns(&now_time_ns) || now_time_ns < start_time_ns) {
        return MF_SHARED_SYSTEM_ERROR;
      }
      elapsed_ns = now_time_ns - start_time_ns;
      if (elapsed_ns >= timeout_ns) {
        return MF_SHARED_TIMEOUT;
      }
      wait_timeout_ns = timeout_ns - elapsed_ns;
    }
    status = mf_client_ring_wait_readable_update_v1(&ring->completion, wait_timeout_ns);
    if (status == MF_SHARED_SUCCESS || status == MF_SHARED_RETRY) {
      continue;
    }
    return status;
  }
}

uint64_t mf_vfio_user_guest_ring_last_completion_timeline_v0(
    const mf_vfio_user_guest_ring_v0* ring) {
  return guest_ring_bound(ring) ? ring->last_completion_timeline : UINT64_C(0);
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

static int bytes_zero(const uint8_t* bytes, size_t count) {
  if (bytes == NULL) {
    return 0;
  }
  for (size_t index = 0; index < count; ++index) {
    if (bytes[index] != 0U) {
      return 0;
    }
  }
  return 1;
}

static mf_shared_status_v1 get_info_status(uint32_t encoded_status) {
  const int32_t status = (int32_t)encoded_status;
  switch (status) {
  case MF_SHARED_SUCCESS:
  case MF_SHARED_WOULD_BLOCK:
  case MF_SHARED_TIMEOUT:
  case MF_SHARED_INTERRUPTED:
  case MF_SHARED_RETRY:
  case MF_SHARED_STALE_HANDLE:
  case MF_SHARED_DEVICE_LOST:
  case MF_SHARED_TERMINAL_VIEW:
  case MF_SHARED_INVALID_ARGUMENT:
  case MF_SHARED_MALFORMED:
  case MF_SHARED_OVERFLOW:
  case MF_SHARED_RESOURCE_EXHAUSTED:
  case MF_SHARED_SYSTEM_ERROR:
  case MF_SHARED_NOT_SUPPORTED:
  case MF_SHARED_PERMISSION_DENIED:
    return status;
  default:
    return MF_SHARED_MALFORMED;
  }
}

mf_shared_status_v1 mf_vfio_user_guest_validate_get_info_v0(
    const mf_vfio_user_get_info_reply_v0* reply, uint64_t expected_generation) {
  mf_shared_status_v1 status;
  if (reply == NULL) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  status = get_info_status(reply->status);
  if (status != MF_SHARED_SUCCESS) {
    return status;
  }
  if (reply->flags != UINT32_C(0) || reply->device_generation == UINT64_C(0) ||
      (expected_generation != UINT64_C(0) && reply->device_generation != expected_generation) ||
      reply->bar0_offset != MF_VFIO_USER_PROFILE_BAR0_OFFSET ||
      reply->bar0_size != MF_VFIO_USER_PROFILE_BAR0_SIZE ||
      reply->bar2_offset != MF_VFIO_USER_PROFILE_BAR2_OFFSET ||
      reply->bar2_size != MF_VFIO_USER_PROFILE_BAR2_SIZE ||
      reply->bar4_offset != MF_VFIO_USER_PROFILE_BAR4_OFFSET ||
      reply->bar4_size != MF_VFIO_USER_PROFILE_BAR4_SIZE ||
      reply->msix_vectors != MF_VFIO_USER_PROFILE_MSIX_VECTORS ||
      reply->doorbell_width != MF_VFIO_USER_PROFILE_DOORBELL_WIDTH ||
      !bytes_zero(reply->reserved, sizeof(reply->reserved))) {
    return MF_SHARED_MALFORMED;
  }
  return MF_SHARED_SUCCESS;
}

mf_shared_status_v1 mf_vfio_user_guest_encode_get_info_v0(uint64_t message_id, uint8_t* buffer,
                                                           uint32_t buffer_capacity,
                                                           uint32_t* out_size) {
  return encode_packet(message_id, MF_VFIO_USER_MESSAGE_GET_INFO_V0, UINT16_C(0), NULL,
                       UINT32_C(0), buffer, buffer_capacity, out_size);
}

mf_shared_status_v1 mf_vfio_user_guest_encode_negotiate_v0(
    uint64_t message_id, const mf_transport_negotiate_v0* request, uint8_t* buffer,
    uint32_t buffer_capacity, uint32_t* out_size) {
  if (request == NULL || request->magic != MF_TRANSPORT_MAGIC_V0 ||
      request->struct_size != sizeof(*request) || request->flags != 0U ||
      !bytes_zero(request->logical_device_uuid, sizeof(request->logical_device_uuid)) ||
      request->daemon_incarnation != UINT64_C(0) || request->view_serial != UINT64_C(0) ||
      request->device_generation != UINT64_C(0) || request->descriptor_version != 0U ||
      request->ring_version != 0U || request->max_queues != 0U || request->ring_order != 0U ||
      request->dma_width != 0U || request->dma_alignment != 0U || request->max_regions != 0U ||
      request->max_inflight != 0U || request->max_bytes != UINT64_C(0) ||
      !bytes_zero(request->reserved, sizeof(request->reserved))) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  return encode_packet(message_id, MF_VFIO_USER_MESSAGE_NEGOTIATE_V0, UINT16_C(0), request,
                       (uint32_t)sizeof(*request), buffer, buffer_capacity, out_size);
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
  return mf_vfio_user_guest_validate_get_info_v0(out_reply, UINT64_C(0));
}

mf_shared_status_v1 mf_vfio_user_guest_decode_negotiate_v0(
    const uint8_t* buffer, uint32_t buffer_size, uint64_t expected_message_id,
    mf_transport_negotiate_v0* out_reply) {
  const mf_transport_message_header_v0* header;
  const uint64_t expected_size =
      (uint64_t)sizeof(*header) + (uint64_t)sizeof(*out_reply);
  if (buffer == NULL || out_reply == NULL || expected_message_id == UINT64_C(0) ||
      buffer_size != expected_size) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  header = (const mf_transport_message_header_v0*)buffer;
  if (header->message_id != expected_message_id ||
      header->message_type !=
          (uint16_t)(MF_VFIO_USER_MESSAGE_NEGOTIATE_V0 | MF_VFIO_USER_REPLY_FLAG_V0) ||
      header->flags != UINT16_C(0) || header->payload_size != sizeof(*out_reply)) {
    return MF_SHARED_MALFORMED;
  }
  (void)memcpy(out_reply, buffer + sizeof(*header), sizeof(*out_reply));
  if (out_reply->magic != MF_TRANSPORT_MAGIC_V0 ||
      out_reply->struct_size != sizeof(*out_reply) || out_reply->flags != 0U ||
      out_reply->major != MF_TRANSPORT_MAJOR_V0 || out_reply->minor == 0U ||
      out_reply->daemon_incarnation == UINT64_C(0) || out_reply->view_serial == UINT64_C(0) ||
      out_reply->device_generation == UINT64_C(0) || out_reply->descriptor_version == 0U ||
      out_reply->ring_version == 0U || out_reply->max_queues == 0U ||
      out_reply->ring_order == 0U || out_reply->dma_width == 0U ||
      out_reply->dma_alignment == 0U || out_reply->max_regions == 0U ||
      out_reply->max_inflight == 0U || out_reply->max_bytes == UINT64_C(0) ||
      !bytes_zero(out_reply->reserved, sizeof(out_reply->reserved))) {
    return MF_SHARED_MALFORMED;
  }
  return MF_SHARED_SUCCESS;
}
