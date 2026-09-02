#include <metaflux/transport/vfio_user_guest.h>

#include <stdint.h>
#include <string.h>

typedef struct doorbell_fixture {
  uint32_t count;
  uint32_t last_value;
} doorbell_fixture;

static void record_doorbell(void* context, uint32_t value) {
  doorbell_fixture* fixture = (doorbell_fixture*)context;
  fixture->count += UINT32_C(1);
  fixture->last_value = value;
}

static int run_ring_test(void) {
  const mf_registry_view_id_v1 view_id = {UINT64_C(37), UINT64_C(41)};
  mf_client_ring_v1 submission_owner;
  mf_client_ring_v1 completion_owner;
  mf_client_ring_v1 mismatched_completion;
  mf_vfio_user_guest_ring_v0 guest;
  mf_vfio_user_guest_ring_v0 probe;
  mf_ring_descriptor_v1 descriptor;
  mf_ring_descriptor_v1 batch[2];
  mf_ring_descriptor_v1 completion;
  uint8_t payload[32];
  doorbell_fixture doorbell = {0};
  uint32_t index = 0;

  (void)memset(&submission_owner, 0, sizeof(submission_owner));
  (void)memset(&completion_owner, 0, sizeof(completion_owner));
  (void)memset(&mismatched_completion, 0, sizeof(mismatched_completion));
  (void)memset(&guest, 0, sizeof(guest));
  (void)memset(&probe, 0, sizeof(probe));
  submission_owner.owned_fd = -1;
  completion_owner.owned_fd = -1;
  mismatched_completion.owned_fd = -1;
  guest.submission.owned_fd = -1;
  guest.completion.owned_fd = -1;
  probe.submission.owned_fd = -1;
  probe.completion.owned_fd = -1;
  if (mf_client_ring_create_v1(UINT32_C(2), view_id, MF_CLIENT_SUBMISSION_QUEUE_ID_V1, UINT64_C(9),
                               &submission_owner) != MF_SHARED_SUCCESS ||
      mf_client_ring_create_v1(UINT32_C(2), view_id, MF_CLIENT_COMPLETION_QUEUE_ID_V1, UINT64_C(9),
                               &completion_owner) != MF_SHARED_SUCCESS ||
      mf_client_ring_create_v1(UINT32_C(2), view_id, MF_CLIENT_COMPLETION_QUEUE_ID_V1, UINT64_C(10),
                               &mismatched_completion) != MF_SHARED_SUCCESS ||
      mf_vfio_user_guest_ring_attach_v0(mf_client_ring_borrow_fd_v1(&submission_owner),
                                        mf_client_ring_borrow_fd_v1(&completion_owner), view_id,
                                        UINT64_C(9), payload, sizeof(payload), record_doorbell,
                                        &doorbell, UINT32_C(0x42), &guest) != MF_SHARED_SUCCESS) {
    mf_vfio_user_guest_ring_close_v0(&guest);
    mf_client_ring_close_v1(&mismatched_completion);
    mf_client_ring_close_v1(&completion_owner);
    mf_client_ring_close_v1(&submission_owner);
    return 1;
  }
  if (mf_vfio_user_guest_ring_attach_v0(mf_client_ring_borrow_fd_v1(&submission_owner),
                                        mf_client_ring_borrow_fd_v1(&mismatched_completion),
                                        view_id, UINT64_C(9), payload, sizeof(payload),
                                        record_doorbell, &doorbell, UINT32_C(0x42),
                                        &probe) != MF_SHARED_MALFORMED ||
      mf_vfio_user_guest_ring_attach_v0(mf_client_ring_borrow_fd_v1(&submission_owner),
                                        mf_client_ring_borrow_fd_v1(&completion_owner), view_id,
                                        UINT64_C(9), NULL, UINT64_C(1), record_doorbell, &doorbell,
                                        UINT32_C(0x42), &probe) != MF_SHARED_INVALID_ARGUMENT) {
    mf_vfio_user_guest_ring_close_v0(&probe);
    mf_vfio_user_guest_ring_close_v0(&guest);
    mf_client_ring_close_v1(&mismatched_completion);
    mf_client_ring_close_v1(&completion_owner);
    mf_client_ring_close_v1(&submission_owner);
    return 2;
  }
  mf_vfio_user_guest_ring_close_v0(&probe);
  mf_client_ring_close_v1(&mismatched_completion);
  if (mf_vfio_user_guest_ring_payload_contains_v0(&guest, UINT64_C(4), UINT64_C(8)) !=
          MF_SHARED_SUCCESS ||
      mf_vfio_user_guest_ring_payload_contains_v0(&guest, UINT64_C(28), UINT64_C(5)) !=
          MF_SHARED_INVALID_ARGUMENT ||
      mf_vfio_user_guest_ring_payload_contains_v0(&guest, UINT64_MAX, UINT64_C(2)) !=
          MF_SHARED_OVERFLOW) {
    mf_vfio_user_guest_ring_close_v0(&guest);
    mf_client_ring_close_v1(&completion_owner);
    mf_client_ring_close_v1(&submission_owner);
    return 3;
  }
  (void)memset(&descriptor, 0, sizeof(descriptor));
  descriptor.opcode = MF_RING_OPCODE_COPY;
  descriptor.request_id = UINT64_C(1);
  descriptor.target_id = UINT64_C(2);
  guest.payload_mapping = NULL;
  if (mf_vfio_user_guest_ring_payload_contains_v0(&guest, UINT64_C(0), UINT64_C(0)) !=
          MF_SHARED_INVALID_ARGUMENT ||
      mf_vfio_user_guest_ring_submit_v0(&guest, &descriptor) != MF_SHARED_INVALID_ARGUMENT) {
    mf_vfio_user_guest_ring_close_v0(&guest);
    mf_client_ring_close_v1(&completion_owner);
    mf_client_ring_close_v1(&submission_owner);
    return 16;
  }
  guest.payload_mapping = payload;
  if (mf_vfio_user_guest_ring_submit_v0(&guest, &descriptor) != MF_SHARED_SUCCESS) {
    mf_vfio_user_guest_ring_close_v0(&guest);
    mf_client_ring_close_v1(&completion_owner);
    mf_client_ring_close_v1(&submission_owner);
    return 4;
  }
  descriptor.opcode = MF_RING_OPCODE_COMPLETION;
  if (mf_vfio_user_guest_ring_submit_v0(&guest, &descriptor) != MF_SHARED_INVALID_ARGUMENT ||
      doorbell.count != UINT32_C(1)) {
    mf_vfio_user_guest_ring_close_v0(&guest);
    mf_client_ring_close_v1(&completion_owner);
    mf_client_ring_close_v1(&submission_owner);
    return 5;
  }
  descriptor.opcode = MF_RING_OPCODE_COPY;
  if (mf_vfio_user_guest_ring_submit_v0(&guest, &descriptor) != MF_SHARED_SUCCESS ||
      doorbell.count != UINT32_C(2) || doorbell.last_value != UINT32_C(0x42) ||
      mf_vfio_user_guest_ring_submit_v0(&guest, &descriptor) != MF_SHARED_WOULD_BLOCK ||
      doorbell.count != UINT32_C(2)) {
    mf_vfio_user_guest_ring_close_v0(&guest);
    mf_client_ring_close_v1(&completion_owner);
    mf_client_ring_close_v1(&submission_owner);
    return 6;
  }
  if (mf_client_ring_try_consume_v1(&submission_owner, &completion) != MF_SHARED_SUCCESS ||
      mf_vfio_user_guest_ring_wait_submission_v0(&guest, UINT64_C(0)) != MF_SHARED_SUCCESS) {
    mf_vfio_user_guest_ring_close_v0(&guest);
    mf_client_ring_close_v1(&completion_owner);
    mf_client_ring_close_v1(&submission_owner);
    return 7;
  }
  (void)memset(&completion, 0, sizeof(completion));
  completion.opcode = MF_RING_OPCODE_COMPLETION;
  completion.request_id = UINT64_C(1);
  completion.target_id = UINT64_C(2);
  completion.arguments[1] = UINT64_C(1);
  for (index = 0; index < UINT32_C(1); ++index) {
    if (mf_client_ring_try_submit_v1(&completion_owner, &completion) != MF_SHARED_SUCCESS ||
        mf_vfio_user_guest_ring_wait_completion_v0(&guest, UINT64_C(0)) != MF_SHARED_SUCCESS ||
        mf_vfio_user_guest_ring_try_consume_v0(&guest, &descriptor) != MF_SHARED_SUCCESS ||
        descriptor.opcode != MF_RING_OPCODE_COMPLETION || descriptor.request_id != UINT64_C(1)) {
      mf_vfio_user_guest_ring_close_v0(&guest);
      mf_client_ring_close_v1(&completion_owner);
      mf_client_ring_close_v1(&submission_owner);
      return 8;
    }
  }
  if (mf_vfio_user_guest_ring_last_completion_timeline_v0(&guest) != UINT64_C(1) ||
      mf_vfio_user_guest_ring_arm_completion_v0(&guest, UINT64_C(1)) != MF_SHARED_SUCCESS ||
      mf_vfio_user_guest_ring_wait_armed_completion_v0(&guest, UINT64_C(0)) != MF_SHARED_SUCCESS) {
    mf_vfio_user_guest_ring_close_v0(&guest);
    mf_client_ring_close_v1(&completion_owner);
    mf_client_ring_close_v1(&submission_owner);
    return 9;
  }
  if (mf_vfio_user_guest_ring_arm_completion_v0(&guest, UINT64_C(3)) != MF_SHARED_SUCCESS) {
    mf_vfio_user_guest_ring_close_v0(&guest);
    mf_client_ring_close_v1(&completion_owner);
    mf_client_ring_close_v1(&submission_owner);
    return 13;
  }
  if (mf_vfio_user_guest_ring_arm_completion_v0(&guest, UINT64_C(2)) != MF_SHARED_WOULD_BLOCK ||
      mf_vfio_user_guest_ring_arm_completion_v0(&guest, UINT64_C(3)) != MF_SHARED_SUCCESS) {
    mf_vfio_user_guest_ring_close_v0(&guest);
    mf_client_ring_close_v1(&completion_owner);
    mf_client_ring_close_v1(&submission_owner);
    return 17;
  }
  completion.arguments[1] = UINT64_C(2);
  if (mf_client_ring_try_submit_v1(&completion_owner, &completion) != MF_SHARED_SUCCESS ||
      mf_vfio_user_guest_ring_wait_armed_completion_v0(&guest, UINT64_C(0)) !=
          MF_SHARED_WOULD_BLOCK) {
    mf_vfio_user_guest_ring_close_v0(&guest);
    mf_client_ring_close_v1(&completion_owner);
    mf_client_ring_close_v1(&submission_owner);
    return 14;
  }
  completion.arguments[1] = UINT64_C(3);
  if (mf_client_ring_try_submit_v1(&completion_owner, &completion) != MF_SHARED_SUCCESS ||
      mf_vfio_user_guest_ring_wait_armed_completion_v0(&guest, UINT64_C(0)) !=
          MF_SHARED_SUCCESS ||
      mf_vfio_user_guest_ring_try_consume_v0(&guest, &descriptor) != MF_SHARED_SUCCESS ||
      descriptor.arguments[1] != UINT64_C(2) ||
      mf_vfio_user_guest_ring_try_consume_v0(&guest, &descriptor) != MF_SHARED_SUCCESS ||
      descriptor.arguments[1] != UINT64_C(3)) {
    mf_vfio_user_guest_ring_close_v0(&guest);
    mf_client_ring_close_v1(&completion_owner);
    mf_client_ring_close_v1(&submission_owner);
    return 15;
  }

  if (mf_client_ring_try_consume_v1(&submission_owner, &descriptor) != MF_SHARED_SUCCESS) {
    mf_vfio_user_guest_ring_close_v0(&guest);
    mf_client_ring_close_v1(&completion_owner);
    mf_client_ring_close_v1(&submission_owner);
    return 10;
  }
  (void)memset(batch, 0, sizeof(batch));
  batch[0].opcode = MF_RING_OPCODE_COPY;
  batch[0].request_id = UINT64_C(20);
  batch[0].target_id = UINT64_C(100);
  batch[1].opcode = MF_RING_OPCODE_COPY;
  batch[1].request_id = UINT64_C(21);
  batch[1].target_id = UINT64_C(200);
  if (mf_vfio_user_guest_ring_submit_batch_v0(&guest, batch, 2U) != MF_SHARED_SUCCESS ||
      doorbell.count != UINT32_C(3) ||
      mf_client_ring_try_consume_v1(&submission_owner, &descriptor) != MF_SHARED_SUCCESS ||
      descriptor.request_id != UINT64_C(20) || descriptor.target_id != UINT64_C(100) ||
      mf_client_ring_try_consume_v1(&submission_owner, &descriptor) != MF_SHARED_SUCCESS ||
      descriptor.request_id != UINT64_C(21) || descriptor.target_id != UINT64_C(200)) {
    mf_vfio_user_guest_ring_close_v0(&guest);
    mf_client_ring_close_v1(&completion_owner);
    mf_client_ring_close_v1(&submission_owner);
    return 11;
  }
  (void)memset(&completion, 0, sizeof(completion));
  completion.opcode = MF_RING_OPCODE_COMPLETION;
  completion.request_id = UINT64_C(22);
  completion.target_id = UINT64_C(0);
  completion.arguments[1] = UINT64_C(2);
  if (mf_client_ring_try_submit_v1(&completion_owner, &completion) != MF_SHARED_SUCCESS ||
      mf_vfio_user_guest_ring_try_consume_v0(&guest, &descriptor) != MF_SHARED_MALFORMED ||
      mf_vfio_user_guest_ring_last_completion_timeline_v0(&guest) != UINT64_C(0) ||
      mf_vfio_user_guest_ring_submit_v0(&guest, &batch[0]) != MF_SHARED_INVALID_ARGUMENT) {
    mf_vfio_user_guest_ring_close_v0(&guest);
    mf_client_ring_close_v1(&completion_owner);
    mf_client_ring_close_v1(&submission_owner);
    return 12;
  }
  mf_vfio_user_guest_ring_close_v0(&guest);
  mf_client_ring_close_v1(&completion_owner);
  mf_client_ring_close_v1(&submission_owner);
  return 0;
}

int main(void) {
  uint8_t packet[MF_VFIO_USER_MAX_PACKET_SIZE_V0];
  uint32_t packet_size = 0;
  mf_vfio_user_dma_map_v0 map;
  mf_vfio_user_get_info_reply_v0 info;
  mf_transport_message_header_v0 header;

  if (mf_vfio_user_guest_encode_get_info_v0(UINT64_C(7), packet, sizeof(packet), &packet_size) !=
          MF_SHARED_SUCCESS ||
      packet_size != sizeof(mf_transport_message_header_v0)) {
    return 1;
  }
  (void)memcpy(&header, packet, sizeof(header));
  if (header.message_id != UINT64_C(7) || header.message_type != MF_VFIO_USER_MESSAGE_GET_INFO_V0 ||
      header.payload_size != 0U) {
    return 1;
  }

  (void)memset(&info, 0, sizeof(info));
  info.status = MF_SHARED_SUCCESS;
  info.device_generation = UINT64_C(11);
  info.bar0_offset = MF_VFIO_USER_PROFILE_BAR0_OFFSET;
  info.bar0_size = MF_VFIO_USER_PROFILE_BAR0_SIZE;
  info.bar2_offset = MF_VFIO_USER_PROFILE_BAR2_OFFSET;
  info.bar2_size = MF_VFIO_USER_PROFILE_BAR2_SIZE;
  info.bar4_offset = MF_VFIO_USER_PROFILE_BAR4_OFFSET;
  info.bar4_size = MF_VFIO_USER_PROFILE_BAR4_SIZE;
  info.msix_vectors = MF_VFIO_USER_PROFILE_MSIX_VECTORS;
  info.doorbell_width = MF_VFIO_USER_PROFILE_DOORBELL_WIDTH;
  if (mf_vfio_user_guest_validate_get_info_v0(&info, UINT64_C(11)) != MF_SHARED_SUCCESS ||
      mf_vfio_user_guest_validate_get_info_v0(&info, UINT64_C(12)) != MF_SHARED_MALFORMED) {
    return 1;
  }
  info.bar2_size += UINT64_C(1);
  if (mf_vfio_user_guest_validate_get_info_v0(&info, UINT64_C(11)) != MF_SHARED_MALFORMED) {
    return 1;
  }
  info.bar2_size = MF_VFIO_USER_PROFILE_BAR2_SIZE;
  info.status = UINT32_C(99);
  if (mf_vfio_user_guest_validate_get_info_v0(&info, UINT64_C(11)) != MF_SHARED_MALFORMED) {
    return 1;
  }

  (void)memset(&map, 0, sizeof(map));
  map.struct_size = sizeof(map);
  map.flags = MF_VFIO_USER_DMA_READ_V0 | MF_VFIO_USER_DMA_WRITE_V0;
  map.iova = UINT64_C(0x1000);
  map.size = UINT64_C(0x1000);
  map.mapping_epoch = UINT64_C(1);
  map.device_generation = UINT64_C(1);
  map.fd_index = 0;
  if (mf_vfio_user_guest_encode_dma_map_v0(UINT64_C(7), &map, packet, sizeof(packet),
                                           &packet_size) != MF_SHARED_SUCCESS ||
      packet_size != sizeof(mf_transport_message_header_v0) + sizeof(map)) {
    return 1;
  }
  map.flags = UINT32_C(4);
  if (mf_vfio_user_guest_encode_dma_map_v0(UINT64_C(7), &map, packet, sizeof(packet),
                                           &packet_size) != MF_SHARED_INVALID_ARGUMENT) {
    return 1;
  }
  map.flags = MF_VFIO_USER_DMA_READ_V0 | MF_VFIO_USER_DMA_WRITE_V0;
  map.iova = UINT64_C(0x1001);
  if (mf_vfio_user_guest_encode_dma_map_v0(UINT64_C(7), &map, packet, sizeof(packet),
                                           &packet_size) != MF_SHARED_INVALID_ARGUMENT) {
    return 1;
  }
  map.iova = UINT64_C(0x1000);
  map.size = UINT64_C(0);
  if (mf_vfio_user_guest_encode_dma_map_v0(UINT64_C(7), &map, packet, sizeof(packet),
                                           &packet_size) != MF_SHARED_INVALID_ARGUMENT) {
    return 1;
  }
  map.size = UINT64_C(0x1000);
  map.reserved[0] = UINT8_C(1);
  if (mf_vfio_user_guest_encode_dma_map_v0(UINT64_C(7), &map, packet, sizeof(packet),
                                           &packet_size) != MF_SHARED_INVALID_ARGUMENT) {
    return 1;
  }
  map.reserved[0] = UINT8_C(0);
  map.mapping_epoch = UINT64_C(0);
  if (mf_vfio_user_guest_encode_dma_map_v0(UINT64_C(7), &map, packet, sizeof(packet),
                                           &packet_size) != MF_SHARED_INVALID_ARGUMENT) {
    return 1;
  }
  map.mapping_epoch = UINT64_C(1);
  map.file_offset = UINT64_C(1);
  if (mf_vfio_user_guest_encode_dma_map_v0(UINT64_C(7), &map, packet, sizeof(packet),
                                           &packet_size) != MF_SHARED_INVALID_ARGUMENT) {
    return 1;
  }
  map.file_offset = UINT64_C(0);
  if (mf_vfio_user_guest_encode_dma_unmap_v0(UINT64_C(7), NULL, UINT16_C(0), packet,
                                             sizeof(packet), &packet_size) !=
      MF_SHARED_INVALID_ARGUMENT) {
    return 1;
  }
  mf_vfio_user_dma_unmap_v0 unmap;
  (void)memset(&unmap, 0, sizeof(unmap));
  unmap.struct_size = sizeof(unmap);
  unmap.iova = UINT64_C(0x1000);
  unmap.size = UINT64_C(0x1000);
  unmap.mapping_epoch = UINT64_C(1);
  unmap.device_generation = UINT64_C(1);
  if (mf_vfio_user_guest_encode_dma_unmap_v0(UINT64_C(7), &unmap, UINT16_C(2), packet,
                                             sizeof(packet), &packet_size) !=
          MF_SHARED_INVALID_ARGUMENT ||
      mf_vfio_user_guest_encode_dma_unmap_v0(UINT64_C(7), &unmap,
                                             MF_TRANSPORT_FLAG_NO_REPLY_V0, packet,
                                             sizeof(packet), &packet_size) != MF_SHARED_SUCCESS) {
    return 1;
  }
  return run_ring_test();
}
